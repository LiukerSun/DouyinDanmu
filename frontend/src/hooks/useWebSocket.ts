import { useState, useEffect, useCallback, useRef } from 'react'
import { wsService } from '../services/ws'
import { WsMessage } from '../types'

export function useWebSocket() {
  const [connected, setConnected] = useState(false)
  const [messages, setMessages] = useState<WsMessage[]>([])
  const initializedRef = useRef(false)

  useEffect(() => {
    console.log('[useWebSocket] Initializing')
    const unsubStatus = wsService.onStatus(setConnected)
    const unsubMsg = wsService.onMessage((msg) => {
      setMessages(prev => [...prev.slice(-999), msg])
    })

    wsService.connect()

    return () => {
      console.log('[useWebSocket] Cleanup')
      unsubStatus()
      unsubMsg()
      // 不断开连接，让 wsService 单例管理生命周期
    }
  }, [])

  const subscribe = useCallback((roomId: string) => {
    wsService.subscribe(roomId)
  }, [])

  const unsubscribe = useCallback((roomId: string) => {
    wsService.unsubscribe(roomId)
  }, [])

  const clearMessages = useCallback(() => {
    setMessages([])
  }, [])

  return { connected, messages, subscribe, unsubscribe, clearMessages }
}
