import { WsMessage, WsSubscribe } from '../types'

type MessageHandler = (msg: WsMessage) => void
type StatusHandler = (connected: boolean) => void

export class WsService {
  private ws: WebSocket | null = null
  private url: string
  private messageHandlers = new Set<MessageHandler>()
  private statusHandlers = new Set<StatusHandler>()
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private reconnectAttempts = 0
  private maxReconnectAttempts = 10

  constructor(url?: string) {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    this.url = url || `${protocol}//${window.location.host}/ws`
  }

  connect() {
    if (this.ws?.readyState === WebSocket.OPEN) return

    this.ws = new WebSocket(this.url)

    this.ws.onopen = () => {
      console.log('[WS] Connected')
      this.reconnectAttempts = 0
      this.statusHandlers.forEach(h => h(true))
    }

    this.ws.onclose = () => {
      console.log('[WS] Disconnected')
      this.statusHandlers.forEach(h => h(false))
      this.scheduleReconnect()
    }

    this.ws.onerror = (err) => {
      console.error('[WS] Error:', err)
    }

    this.ws.onmessage = (event) => {
      try {
        const msg: WsMessage = JSON.parse(event.data)
        this.messageHandlers.forEach(h => h(msg))
      } catch (e) {
        console.error('[WS] Parse error:', e)
      }
    }
  }

  disconnect() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }
    this.ws?.close()
    this.ws = null
  }

  send(data: WsSubscribe) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data))
    }
  }

  subscribe(roomId: string) {
    this.send({ action: 'subscribe', room_id: roomId })
  }

  unsubscribe(roomId: string) {
    this.send({ action: 'unsubscribe', room_id: roomId })
  }

  subscribeAll() {
    this.send({ action: 'subscribe_all' })
  }

  onMessage(handler: MessageHandler) {
    this.messageHandlers.add(handler)
    return () => this.messageHandlers.delete(handler)
  }

  onStatus(handler: StatusHandler) {
    this.statusHandlers.add(handler)
    return () => this.statusHandlers.delete(handler)
  }

  private scheduleReconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[WS] Max reconnect attempts reached')
      return
    }
    const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts), 30000)
    this.reconnectAttempts++
    console.log(`[WS] Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`)
    this.reconnectTimer = setTimeout(() => this.connect(), delay)
  }

  get connected() {
    return this.ws?.readyState === WebSocket.OPEN
  }
}

export const wsService = new WsService()
