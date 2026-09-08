import React, { useEffect, useState, useCallback, useRef } from 'react'
import { Tabs, Button, Modal, Input, message, Spin, Empty } from 'antd'
import { PlusOutlined, ReloadOutlined, DisconnectOutlined } from '@ant-design/icons'
import MessageStream from '../components/MessageStream'
import FilterBar from '../components/FilterBar'
import StatsBar from '../components/StatsBar'
import { useWs, useMsg, useNotif } from '../context'
import { api } from '../services/api'
import { Room as RoomType, WsMessage } from '../types'

const Room: React.FC = () => {
  const [rooms, setRooms] = useState<RoomType[]>([])
  const [activeRoom, setActiveRoom] = useState<string>('')
  const [loading, setLoading] = useState(false)
  const [addModalOpen, setAddModalOpen] = useState(false)
  const [newRoomId, setNewRoomId] = useState('')
  const messagesEndRef = useRef<HTMLDivElement>(null)

  const { messages: wsMessages, subscribe, unsubscribe } = useWs()
  const { messages, stats, filter, keyword, addWsMessage, toggleFilter, setKeyword } = useMsg()
  const { handleWsMessage } = useNotif()
  const lastProcessedRef = useRef<number>(0)

  // 加载房间列表
  const loadRooms = useCallback(async () => {
    try {
      const data = await api.getRooms()
      setRooms(data)
      if (data.length > 0 && !activeRoom) {
        setActiveRoom(data[0].room_id)
      }
      // 订阅所有房间
      data.forEach(room => subscribe(room.live_id))
    } catch (err) {
      console.error('Failed to load rooms:', err)
    }
  }, [activeRoom])

  useEffect(() => {
    loadRooms()
  }, [])

  // 处理 WebSocket 消息 - 只处理新消息
  useEffect(() => {
    if (wsMessages.length > lastProcessedRef.current) {
      // 只处理新增的消息
      const newMessages = wsMessages.slice(lastProcessedRef.current)
      lastProcessedRef.current = wsMessages.length

      newMessages.forEach(msg => {
        if (msg.room_id === activeRoom) {
          addWsMessage(msg)
        }
        handleWsMessage(msg)
      })
    }
  }, [wsMessages, activeRoom])

  // 自动滚动到底部
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages])

  // 从 URL 或纯数字中提取房间号
  const extractRoomId = (input: string): string => {
    const trimmed = input.trim()
    // Extract the room number from a live-room URL.
    const urlMatch = trimmed.match(/live\.douyin\.com\/(\d+)/)
    if (urlMatch) return urlMatch[1]
    // 匹配纯数字
    const numMatch = trimmed.match(/^(\d+)$/)
    if (numMatch) return numMatch[1]
    return trimmed
  }

  // 添加房间
  const handleAddRoom = async () => {
    if (!newRoomId.trim()) {
      message.error('请输入房间号或直播间链接')
      return
    }
    const liveId = extractRoomId(newRoomId)
    if (!liveId) {
      message.error('无法解析房间号')
      return
    }
    setLoading(true)
    try {
      await api.addRoom(liveId)
      message.success('房间添加成功')
      setAddModalOpen(false)
      setNewRoomId('')
      await loadRooms()
      subscribe(liveId)
    } catch (err: any) {
      message.error(err.message || '添加失败')
    } finally {
      setLoading(false)
    }
  }

  // 移除房间
  const handleRemoveRoom = async (roomId: string) => {
    const room = rooms.find(r => r.room_id === roomId)
    if (!room) return

    Modal.confirm({
      title: '确认移除',
      content: '确定要移除该房间吗？',
      onOk: async () => {
        try {
          await api.removeRoom(room.live_id)
          unsubscribe(room.live_id)
          setRooms(prev => prev.filter(r => r.room_id !== roomId))
          if (activeRoom === roomId) {
            setActiveRoom(rooms.find(r => r.room_id !== roomId)?.room_id || '')
          }
          message.success('房间已移除')
        } catch (err: any) {
          message.error(err.message || '移除失败')
        }
      },
    })
  }

  // 切换房间
  const handleTabChange = (key: string) => {
    setActiveRoom(key)
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      {/* 房间 Tabs - 固定高度 */}
      <div style={{ padding: '8px 16px 0', display: 'flex', alignItems: 'center', gap: 8, flexShrink: 0 }}>
        <Tabs
          activeKey={activeRoom}
          onChange={handleTabChange}
          style={{ flex: 1 }}
          type="editable-card"
          hideAdd
          items={rooms.map(room => ({
            key: room.room_id,
            label: (
              <span>
                {room.status === 'live' && <span style={{ color: '#52c41a' }}>&#9679; </span>}
                {room.status === 'connecting' && <span style={{ color: '#faad14' }}>&#9679; </span>}
                {room.status === 'offline' && <span style={{ color: '#ff4d4f' }}>&#9679; </span>}
                {room.anchor_name || room.room_id}
              </span>
            ),
            closable: true,
          }))}
          onEdit={(targetKey, action) => {
            if (action === 'remove') handleRemoveRoom(targetKey as string)
          }}
        />
        <Button
          type="dashed"
          icon={<PlusOutlined />}
          onClick={() => setAddModalOpen(true)}
          size="small"
        >
          添加房间
        </Button>
        <Button
          icon={<ReloadOutlined />}
          onClick={loadRooms}
          size="small"
        />
      </div>

      {/* 筛选器 - 固定高度 */}
      <div style={{ flexShrink: 0 }}>
        <FilterBar
          filter={filter}
          keyword={keyword}
          onToggleFilter={toggleFilter}
          onKeywordChange={setKeyword}
        />
      </div>

      {/* 消息流 - 可滚动区域 */}
      <div style={{ flex: 1, overflow: 'hidden', minHeight: 0 }}>
        {activeRoom ? (
          <MessageStream messages={messages} />
        ) : (
          <Empty description="请添加直播间" style={{ marginTop: 100 }} />
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* 统计栏 - 固定高度 */}
      <div style={{ flexShrink: 0 }}>
        <StatsBar
          chatCount={stats.chat}
          giftCount={stats.gift}
          enterCount={stats.enter}
          onlineCount={0}
        />
      </div>

      {/* 添加房间弹窗 */}
      <Modal
        title="添加直播间"
        open={addModalOpen}
        onOk={handleAddRoom}
        onCancel={() => setAddModalOpen(false)}
        confirmLoading={loading}
      >
        <Input
          placeholder="输入房间号或直播链接"
          value={newRoomId}
          onChange={e => setNewRoomId(e.target.value)}
          onPressEnter={handleAddRoom}
        />
      </Modal>
    </div>
  )
}

export default Room
