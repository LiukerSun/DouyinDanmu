import { advanceCursor, isFeedMessage, mergeMessages, mergeRoomState, type PipelineEvent, type RoomStateEvent } from './messages'

export type RoomStream = { messages: PipelineEvent[]; roomState: RoomStateEvent[]; cursor: string; connected: boolean; loaded: boolean; error: string; lastSeen: number | null }
type Snapshot = { events: PipelineEvent[]; state_events?: RoomStateEvent[]; through_seq: string }
type Dependencies = { snapshot: (id: string, signal: AbortSignal) => Promise<Snapshot>; socket: () => WebSocket; change: (state: Record<string, RoomStream>) => void }
type Channel = { ids: Set<string>; socket?: WebSocket; retry?: ReturnType<typeof setTimeout>; backoff: number }
const empty = (): RoomStream => ({ messages: [], roomState: [], cursor: '0', connected: false, loaded: false, error: '', lastSeen: null })

// Keep subscriptions independent of selection, filters and layout. Each socket
// respects the server's 32-subscription limit; each room has its own replay cursor.
export class RoomStreams {
  private state: Record<string, RoomStream> = {}
  private channels: Channel[] = []
  private controllers = new Map<string, AbortController>()
  private retries = new Map<string, ReturnType<typeof setTimeout>>()
  private disposed = false
  private publish?: ReturnType<typeof setTimeout>
  constructor(private deps: Dependencies) {}
  private emit() {
    if (this.publish || this.disposed) return
    this.publish = setTimeout(() => { this.publish = undefined; this.deps.change({ ...this.state }) }, 100)
  }
  sync(ids: string[]) {
    const wanted = new Set(ids)
    for (const id of Object.keys(this.state)) if (!wanted.has(id)) {
      this.controllers.get(id)?.abort(); this.controllers.delete(id)
      clearTimeout(this.retries.get(id)); this.retries.delete(id)
      for (const channel of this.channels) if (channel.ids.delete(id) && channel.socket?.readyState === 1) channel.socket.send(JSON.stringify({ action: 'unsubscribe', live_id: id }))
      delete this.state[id]
    }
    for (const id of wanted) if (!this.state[id]) { this.state[id] = empty(); void this.initialize(id) }
    this.emit()
  }
  private async initialize(id: string) {
    const controller = new AbortController(); this.controllers.set(id, controller)
    const timeout = setTimeout(() => controller.abort(), 15000)
    try {
      const snapshot = await this.deps.snapshot(id, controller.signal)
      if (this.disposed || this.controllers.get(id) !== controller || !this.state[id]) return
      this.state[id] = { ...empty(), loaded: true, messages: mergeMessages([], snapshot.events.filter(isFeedMessage), id), roomState: mergeRoomState([], [...snapshot.events, ...(snapshot.state_events || [])], id), cursor: advanceCursor('0', snapshot.through_seq) }
      let channel = this.channels.find(item => item.ids.size < 32)
      if (!channel) { channel = { ids: new Set(), backoff: 500 }; this.channels.push(channel) }
      channel.ids.add(id)
      if (!channel.socket && !channel.retry) this.connect(channel)
      else if (channel.socket?.readyState === 1) this.subscribe(channel, id)
      this.emit()
    } catch {
      if (this.disposed || this.controllers.get(id) !== controller || !this.state[id]) return
      this.state[id] = { ...this.state[id], error: '历史消息加载失败，正在重试' }; this.emit()
      this.retries.set(id, setTimeout(() => { this.retries.delete(id); void this.initialize(id) }, 3000))
    } finally { clearTimeout(timeout) }
  }
  private subscribe(channel: Channel, id: string) { channel.socket?.send(JSON.stringify({ action: 'subscribe', live_id: id, after_seq: this.state[id].cursor })) }
  private connect(channel: Channel) {
    if (this.disposed || !channel.ids.size) return
    const socket = this.deps.socket(); channel.socket = socket
    socket.onopen = () => { if (this.disposed) { socket.close(); return }; channel.backoff = 500; for (const id of channel.ids) this.subscribe(channel, id) }
    socket.onmessage = event => {
      if (this.disposed || channel.socket !== socket) return
      try {
        const data = JSON.parse(event.data), id = data.live_id
        if (!channel.ids.has(id) || !this.state[id]) return
        const previous = this.state[id]
        if (data.type === 'subscribed') this.state[id] = { ...previous, connected: true, error: '' }
        if (data.type === 'event_batch') {
          const messages = mergeMessages(previous.messages, data.events.filter(isFeedMessage), id)
          const roomState = mergeRoomState(previous.roomState, [...data.events, ...(data.state_events || [])], id)
          const cursor = advanceCursor(previous.cursor, data.through_seq)
          this.state[id] = { ...previous, messages, roomState, cursor, connected: true, error: '', lastSeen: Date.now() }
        }
        this.emit()
      } catch { for (const id of channel.ids) this.state[id] = { ...this.state[id], error: '消息格式异常，正在恢复连接' }; socket.close() }
    }
    socket.onerror = () => socket.close()
    socket.onclose = () => {
      if (this.disposed || channel.socket !== socket) return
      channel.socket = undefined
      for (const id of channel.ids) this.state[id] = { ...this.state[id], connected: false }
      this.emit()
      channel.retry = setTimeout(() => { channel.retry = undefined; this.connect(channel) }, channel.backoff)
      channel.backoff = Math.min(10000, channel.backoff * 2)
    }
  }
  dispose() {
    this.disposed = true; clearTimeout(this.publish)
    for (const controller of this.controllers.values()) controller.abort()
    for (const timer of this.retries.values()) clearTimeout(timer)
    for (const channel of this.channels) { clearTimeout(channel.retry); channel.socket?.close() }
  }
}
