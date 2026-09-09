export type FansClub = { member: boolean | null; level: number | null; name: string | null; status: number | null }

export type PipelineEvent = {
  seq: string
  event_id: string
  display_id?: string
  live_id: string
  source: string
  type: string
  action?: number | string
  social_action?: string
  online_count?: number
  method?: string
  parse_status?: string
  payload_bytes?: number
  has_details?: boolean
  timestamp: number
  user_id: string
  user_name: string
  user_level?: number | null
  fans_club?: FansClub | null
  content: string
  gift_count: number
  gift_combo?: boolean
  gift_final?: boolean
  persisted_at_ms: number
  received_at_ms: number
}

export function mergeMessages(previous: PipelineEvent[], incoming: PipelineEvent[], liveId: string): PipelineEvent[] {
  const messages = new Map<string, PipelineEvent>()
  for (const event of [...previous, ...incoming]) {
    if (event.live_id !== liveId) continue
    const key = messageDisplayKey(event)
    const current = messages.get(key)
    if (!current || BigInt(event.seq) > BigInt(current.seq)) messages.set(key, event)
  }
  return [...messages.values()].sort((left, right) => {
    const a = BigInt(left.seq), b = BigInt(right.seq)
    return a < b ? -1 : a > b ? 1 : 0
  }).slice(-500)
}

export function messageDisplayKey(event: PipelineEvent): string {
  return JSON.stringify([event.live_id, event.display_id || event.event_id])
}

export function advanceCursor(previous: string, incoming: string): string {
  return BigInt(incoming) > BigInt(previous) ? incoming : previous
}

export const parseStatusLabels: Record<string, string> = { decoded: '协议已解码', partial: '部分字段待解释', unmapped: '待识别', failed: '解析失败', legacy: '历史记录' }

export const audienceMessageTypes = new Set(['chat', 'gift', 'enter', 'like', 'social', 'follow', 'share', 'emoji', 'fansclub', 'episode_chat', 'audio_chat', 'screen_chat', 'system'])
// The backend applies the same policy before paging or sending messages. This
// guard also prevents an old server response from filling the feed during rollout.
export function isFeedMessage(event: PipelineEvent): boolean {
  if (event.parse_status === 'failed') return false
  if (audienceMessageTypes.has(event.type)) return true
  if (event.type === 'room_notice' && (!event.method || event.method === 'WebcastRoomMessage')) return !!event.content?.trim() && event.content !== '房间通知'
  if (event.type === 'notice' && (!event.method || ['WebcastCommonTextMessage', 'WebcastNotifyMessage'].includes(event.method))) return !!event.content?.trim() && !['公共文本', '通知'].includes(event.content)
  return false
}
export type RoomStateEvent = Pick<PipelineEvent, 'seq' | 'live_id' | 'type' | 'method' | 'online_count' | 'received_at_ms'>
// Only header metrics are delivered; protocol/configuration history stays in storage.
export const isRoomState = (event: RoomStateEvent) => event.type === 'online_count' && event.online_count != null
export function mergeRoomState(previous: RoomStateEvent[], incoming: RoomStateEvent[], liveId: string): RoomStateEvent[] {
  const latest = new Map<string, RoomStateEvent>()
  for (const event of [...previous, ...incoming]) {
    if (event.live_id !== liveId || !isRoomState(event)) continue
    const key = event.type, current = latest.get(key)
    if (!current || BigInt(event.seq) > BigInt(current.seq)) latest.set(key, { seq: event.seq, live_id: event.live_id, type: event.type, method: 'WebcastRoomUserSeqMessage', online_count: event.online_count, received_at_ms: event.received_at_ms })
  }
  return [...latest.values()].sort((a, b) => BigInt(a.seq) < BigInt(b.seq) ? -1 : BigInt(a.seq) > BigInt(b.seq) ? 1 : 0)
}
export function messageKind(event: PipelineEvent): string {
  // User-facing categories describe an activity, independent of its protocol.
  if (['emoji', 'episode_chat', 'audio_chat', 'screen_chat'].includes(event.type)) return 'chat'
  if (['system', 'room_notice'].includes(event.type)) return 'notice'
  if (event.type !== 'social') return event.type
  // Use parsed social actions, with action 1 as the follow fallback.
  return event.social_action === 'follow' || event.social_action === 'share' ? event.social_action
    : event.social_action == null && String(event.action) === '1' ? 'follow' : 'social'
}
export function messageContent(event: PipelineEvent): string {
  if (event.type !== 'social') return event.content
  const kind = messageKind(event)
  return kind === 'follow' ? '关注了主播' : kind === 'share' ? '分享了直播间' : '发生了其他互动'
}
