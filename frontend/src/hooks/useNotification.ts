import { useState, useCallback, useEffect } from 'react'
import { Message, MessageType, WsMessage } from '../types'

const DEFAULT_TRIGGER_TYPES: MessageType[] = ['gift', 'social']

export function useNotification() {
  const [notifications, setNotifications] = useState<Message[]>([])
  const [unreadCount, setUnreadCount] = useState(0)
  const [triggerTypes, setTriggerTypes] = useState<MessageType[]>(DEFAULT_TRIGGER_TYPES)

  // 从 localStorage 恢复配置
  useEffect(() => {
    const saved = localStorage.getItem('notification_trigger_types')
    if (saved) {
      try { setTriggerTypes(JSON.parse(saved)) } catch {}
    }
  }, [])

  const handleWsMessage = useCallback((wsMsg: WsMessage) => {
    if (wsMsg.type === 'status' || wsMsg.type === 'notification') return
    if (!triggerTypes.includes(wsMsg.type as MessageType)) return

    const msg: Message = {
      id: wsMsg.data.id || Date.now(),
      type: wsMsg.type as MessageType,
      room_id: wsMsg.room_id,
      timestamp: wsMsg.timestamp,
      user_id: wsMsg.data.user_id || '',
      user_name: wsMsg.data.user_name || '',
      content: wsMsg.data.content || wsMsg.data.gift_name || '',
      gift_count: wsMsg.data.gift_count || 0,
      extra: '',
      trace_id: '',
    }

    setNotifications(prev => [msg, ...prev].slice(0, 100))
    setUnreadCount(prev => prev + 1)

    // 浏览器通知
    if (Notification.permission === 'granted') {
      new Notification(`${msg.user_name} - ${msg.type}`, {
        body: msg.content || '新消息',
        icon: '/vite.svg',
      })
    }
  }, [triggerTypes])

  const clearUnread = useCallback(() => setUnreadCount(0), [])

  const updateTriggerTypes = useCallback((types: MessageType[]) => {
    setTriggerTypes(types)
    localStorage.setItem('notification_trigger_types', JSON.stringify(types))
  }, [])

  return {
    notifications,
    unreadCount,
    triggerTypes,
    handleWsMessage,
    clearUnread,
    updateTriggerTypes,
  }
}
