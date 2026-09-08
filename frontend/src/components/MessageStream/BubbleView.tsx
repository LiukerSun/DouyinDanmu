import React from 'react'
import { Avatar, Tag } from 'antd'
import { GiftOutlined, UserOutlined, HeartOutlined, SoundOutlined, LikeOutlined } from '@ant-design/icons'
import { Message, MessageType } from '../../types'
import dayjs from 'dayjs'

interface Props {
  messages: Message[]
}

const typeConfig: Record<MessageType | 'unknown', { icon: React.ReactNode; color: string; label: string }> = {
  chat: { icon: <SoundOutlined />, color: '#fff', label: '💬' },
  gift: { icon: <GiftOutlined />, color: '#ffd700', label: '🎁' },
  enter: { icon: <UserOutlined />, color: '#999', label: '🚪' },
  like: { icon: <LikeOutlined />, color: '#ff69b4', label: '❤️' },
  social: { icon: <HeartOutlined />, color: '#ff4d4f', label: '💕' },
  online_count: { icon: null, color: '#1677ff', label: '📊' },
  system: { icon: null, color: '#1677ff', label: '📢' },
  stats: { icon: null, color: '#52c41a', label: '📈' },
  unknown: { icon: null, color: '#999', label: '❓' },
}

const BubbleView: React.FC<Props> = ({ messages }) => {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 8, padding: 16 }}>
      {messages.map((msg) => {
        const config = typeConfig[msg.type] || typeConfig.unknown
        const isSystem = msg.type === 'system' || msg.type === 'online_count'
        const isGift = msg.type === 'gift'

        if (isSystem) {
          return (
            <div key={msg.id} style={{ textAlign: 'center', color: config.color, fontSize: 13 }}>
              {config.label} {msg.content}
            </div>
          )
        }

        return (
          <div
            key={msg.id}
            style={{
              display: 'flex',
              gap: 8,
              alignItems: 'flex-start',
              padding: '8px 12px',
              borderRadius: 8,
              background: isGift ? 'rgba(255, 215, 0, 0.1)' : 'transparent',
              border: isGift ? '1px solid rgba(255, 215, 0, 0.3)' : 'none',
            }}
          >
            <Avatar size={32} icon={<UserOutlined />} style={{ flexShrink: 0 }}>
              {msg.user_name?.charAt(0)}
            </Avatar>
            <div style={{ flex: 1, minWidth: 0 }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span style={{ fontWeight: 500, color: isGift ? '#ffd700' : undefined }}>
                  {msg.user_name}
                </span>
                <span style={{ fontSize: 12, color: '#666' }}>
                  {dayjs(msg.timestamp).format('HH:mm:ss')}
                </span>
              </div>
              <div style={{ marginTop: 4 }}>
                {isGift ? (
                  <span>
                    {config.label} 送出 <Tag color="gold">{msg.content}</Tag>
                    {msg.gift_count > 1 && <span style={{ color: '#ffd700' }}> x{msg.gift_count}</span>}
                  </span>
                ) : msg.type === 'enter' ? (
                  <span style={{ color: '#999' }}>{config.label} 进入了直播间</span>
                ) : msg.type === 'like' ? (
                  <span style={{ color: '#ff69b4' }}>{config.label} 点赞了</span>
                ) : (
                  <span>{msg.content}</span>
                )}
              </div>
            </div>
          </div>
        )
      })}
    </div>
  )
}

export default BubbleView
