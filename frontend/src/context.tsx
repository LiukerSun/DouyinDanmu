import React, { createContext, useContext } from 'react'
import { useWebSocket as useWebSocketHook } from './hooks/useWebSocket'
import { useMessages as useMessagesHook } from './hooks/useMessages'
import { useNotification as useNotificationHook } from './hooks/useNotification'
import { WsMessage, Message, MessageType } from './types'

interface WsContextType {
  connected: boolean
  messages: WsMessage[]
  subscribe: (roomId: string) => void
  unsubscribe: (roomId: string) => void
  clearMessages: () => void
}

interface MsgContextType {
  messages: Message[]
  stats: {
    total: number
    chat: number
    gift: number
    enter: number
    giftValue: number
  }
  filter: Set<MessageType>
  keyword: string
  addWsMessage: (msg: WsMessage) => void
  toggleFilter: (type: MessageType) => void
  setKeyword: (kw: string) => void
}

interface NotifContextType {
  notifications: Message[]
  unreadCount: number
  handleWsMessage: (msg: WsMessage) => void
  clearUnread: () => void
}

const WsContext = createContext<WsContextType | null>(null)
const MsgContext = createContext<MsgContextType | null>(null)
const NotifContext = createContext<NotifContextType | null>(null)

export function AppProvider({ children }: { children: React.ReactNode }) {
  const ws = useWebSocketHook()
  const msg = useMessagesHook()
  const notif = useNotificationHook()

  return (
    <WsContext.Provider value={ws}>
      <MsgContext.Provider value={msg}>
        <NotifContext.Provider value={notif}>
          {children}
        </NotifContext.Provider>
      </MsgContext.Provider>
    </WsContext.Provider>
  )
}

export function useWs() {
  const ctx = useContext(WsContext)
  if (!ctx) throw new Error('useWs must be used within AppProvider')
  return ctx
}

export function useMsg() {
  const ctx = useContext(MsgContext)
  if (!ctx) throw new Error('useMsg must be used within AppProvider')
  return ctx
}

export function useNotif() {
  const ctx = useContext(NotifContext)
  if (!ctx) throw new Error('useNotif must be used within AppProvider')
  return ctx
}
