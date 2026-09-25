export type SessionStats = {
  chat_count: string; gift_events: string; gift_quantity: string; known_gift_value: string
  unknown_price_quantity: string; value_complete: boolean; like_count: string; enter_count: string; peak_online: number
}
export type LiveSession = {
  id: number; room_id: string; started_at_ms: number; ended_at_ms: number | null
  status: 'live' | 'ended'; start_source: string; end_source: string | null
  stats: SessionStats
}
export type SessionMeta = { as_of_seq: string; value_unit: string; session_basis: string }
export type SessionListResult = { items: LiveSession[]; total: string; next_offset: number | null; meta: SessionMeta }
export function sessionsQuery(room: string, offset: number, limit = 20) {
  if (!/^\d{1,32}$/.test(room)) throw new Error('请选择一个直播间查看直播场次')
  return new URLSearchParams({ limit: String(limit), offset: String(offset) }).toString()
}
export function sessionEventsQuery(room: string, session: LiveSession, type: 'chat' | 'gift', before: string, limit = 50) {
  const params = new URLSearchParams({ room, type, limit: String(limit) })
  if (type === 'chat') params.set('view', 'events')
  params.set('from_ms', String(session.started_at_ms))
  if (session.ended_at_ms !== null) params.set('to_ms', String(session.ended_at_ms))
  if (before) params.set('before', before)
  return params.toString()
}
export function sessionRankingQuery(room: string, session: LiveSession, offset: number, snapshot?: string, sort = 'value', limit = 20) {
  const params = new URLSearchParams({ room, limit: String(limit), offset: String(offset), sort })
  params.set('from_ms', String(session.started_at_ms))
  if (session.ended_at_ms !== null) params.set('to_ms', String(session.ended_at_ms))
  if (snapshot !== undefined) params.set('as_of_seq', snapshot)
  return params.toString()
}
