export type RankingUser = {
  rank: number; user_id: string; user_name: string; user_level: number | null
  chat_count: string; gift_quantity: string; known_gift_value: string
  unknown_price_quantity: string; value_complete: boolean; last_seen_at_ms: number
}
export type RankingMeta = { as_of_seq: string; room: string; from_ms: number | null; to_ms: number | null; unattributed_event_count: string }
export type RankingResult = { items: RankingUser[]; total: string; next_offset: number | null; meta: RankingMeta }
export type RankingPeriod = 'all' | 'today' | 'week'
export function rankingBounds(period: RankingPeriod, now = new Date()) {
  if (period === 'all') return {}
  const start = new Date(now); start.setHours(0, 0, 0, 0)
  if (period === 'week') start.setDate(start.getDate() - 6)
  return { from_ms: start.getTime(), to_ms: now.getTime() }
}
export function rankingQuery(room: string, bounds: { from_ms?: number; to_ms?: number }, offset: number, snapshot?: string, sort = 'value') {
  if (!/^\d{1,32}$/.test(room)) throw new Error('请选择一个直播间查看排行榜')
  const params = new URLSearchParams({ room, limit: '20', offset: String(offset), sort })
  if (bounds.from_ms !== undefined) params.set('from_ms', String(bounds.from_ms))
  if (bounds.to_ms !== undefined) params.set('to_ms', String(bounds.to_ms))
  if (snapshot !== undefined) params.set('as_of_seq', snapshot)
  return params.toString()
}
export function exactCount(value: string) {
  try { return BigInt(value).toLocaleString('zh-CN') } catch { return '—' }
}
