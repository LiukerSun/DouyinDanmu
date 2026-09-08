// 消息类型
export type MessageType = 'chat' | 'gift' | 'enter' | 'like' | 'social' | 'online_count' | 'system' | 'stats'

// 消息类型枚举（用于组件）
export const MessageTypeEnum = {
  CHAT: 'chat' as MessageType,
  GIFT: 'gift' as MessageType,
  ENTER: 'enter' as MessageType,
  LIKE: 'like' as MessageType,
  SOCIAL: 'social' as MessageType,
  ONLINE_COUNT: 'online_count' as MessageType,
  SYSTEM: 'system' as MessageType,
  STATS: 'stats' as MessageType,
}

// 聊天消息
export interface ChatMessage extends Message {
  type: 'chat'
  emoji_id?: string
}

// 礼物消息
export interface GiftMessage extends Message {
  type: 'gift'
  gift_name: string
  gift_count: number
  gift_value?: number
}

// 过滤配置
export interface FilterConfig {
  messageTypes: MessageType[]
  keywords: string[]
  showSystemMessages: boolean
}

// 统计数据
export interface StatsData {
  totalMessages: number
  chatCount: number
  giftCount: number
  enterCount: number
  likeCount: number
  socialCount: number
  onlineCount: number
}

// 主题模式
export type ThemeMode = 'light' | 'dark'

// 消息接口
export interface Message {
  id: number
  type: MessageType
  room_id: string
  timestamp: number
  user_id: string
  user_name: string
  content: string
  gift_count: number
  extra: string
  trace_id: string
}

// 房间状态
export type RoomStatus = 'offline' | 'connecting' | 'live'

// 房间接口
export interface Room {
  room_id: string
  live_id: string
  title: string
  anchor_name: string
  status: RoomStatus
  online_count: number
  created_at: number
  updated_at: number
}

// 房间统计
export interface RoomStats {
  room_id: string
  chat_count: number
  gift_count: number
  enter_count: number
  online_count: number
}

// WebSocket 消息格式
export interface WsMessage {
  type: MessageType | 'status' | 'notification'
  room_id: string
  timestamp: number
  data: any
}

// WebSocket 订阅请求
export interface WsSubscribe {
  action: 'subscribe' | 'unsubscribe' | 'subscribe_all'
  room_id?: string
}

// 通知配置
export interface NotificationConfig {
  trigger_types: MessageType[]
}

// 应用配置
export interface AppConfig {
  server: { host: string; port: number }
  capture: {
    rooms: string[]
    heartbeat_interval: number
    reconnect_interval: number
    max_reconnect_attempts: number
  }
  notification: NotificationConfig
  log: { level: string; file: string }
}
