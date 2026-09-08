import { useState, useCallback, useMemo } from 'react'
import { Message, MessageType, WsMessage } from '../types'

export function useMessages(initialMessages: WsMessage[] = []) {
  const [messages, setMessages] = useState<Message[]>([])
  const [filter, setFilter] = useState<Set<MessageType>>(new Set())
  const [keyword, setKeyword] = useState('')

  // 从 WsMessage 转换为 Message
  const addWsMessage = useCallback((wsMsg: WsMessage) => {
    if (wsMsg.type === 'status' || wsMsg.type === 'notification') return
    const msg: Message = {
      id: wsMsg.data.id || Date.now(),
      type: wsMsg.type as MessageType,
      room_id: wsMsg.room_id,
      timestamp: wsMsg.timestamp,
      user_id: wsMsg.data.user_id || '',
      user_name: wsMsg.data.user_name || '',
      content: wsMsg.data.content || wsMsg.data.gift_name || '',
      gift_count: wsMsg.data.gift_count || 0,
      extra: JSON.stringify(wsMsg.data.extra || {}),
      trace_id: wsMsg.data.trace_id || '',
    }
    setMessages(prev => [...prev.slice(-1999), msg])
  }, [])

  // 过滤后的消息
  const filteredMessages = useMemo(() => {
    let result = messages
    if (filter.size > 0) {
      result = result.filter(m => filter.has(m.type))
    }
    if (keyword) {
      const kw = keyword.toLowerCase()
      result = result.filter(m =>
        m.content.toLowerCase().includes(kw) ||
        m.user_name.toLowerCase().includes(kw)
      )
    }
    return result
  }, [messages, filter, keyword])

  // 统计
  const stats = useMemo(() => {
    const today = new Date()
    today.setHours(0, 0, 0, 0)
    const todayStart = today.getTime()

    const todayMsgs = messages.filter(m => m.timestamp >= todayStart)
    return {
      total: todayMsgs.length,
      chat: todayMsgs.filter(m => m.type === 'chat').length,
      gift: todayMsgs.filter(m => m.type === 'gift').length,
      enter: todayMsgs.filter(m => m.type === 'enter').length,
      giftValue: todayMsgs.filter(m => m.type === 'gift').reduce((s, m) => s + m.gift_count, 0),
    }
  }, [messages])

  const toggleFilter = useCallback((type: MessageType) => {
    setFilter(prev => {
      const next = new Set(prev)
      if (next.has(type)) next.delete(type)
      else next.add(type)
      return next
    })
  }, [])

  return {
    messages: filteredMessages,
    allMessages: messages,
    stats,
    filter,
    keyword,
    addWsMessage,
    toggleFilter,
    setKeyword,
    setMessages,
  }
}
