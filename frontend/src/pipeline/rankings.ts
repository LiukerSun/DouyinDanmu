export type RankingUser = {
  rank: number; user_id: string; user_name: string; user_level: number | null
  chat_count: string; gift_quantity: string; known_gift_value: string
  unknown_price_quantity: string; value_complete: boolean; last_seen_at_ms: number
}
export type RankingMeta = { as_of_seq: string; room: string; from_ms: number | null; to_ms: number | null; unattributed_event_count: string }
export type RankingResult = { items: RankingUser[]; total: string; next_offset: number | null; meta: RankingMeta }
export type RankingPeriod = 'all' | 'today' | 'week' | 'thisWeek' | 'lastWeek' | 'thisMonth' | 'lastMonth'
export function rankingBounds(period: RankingPeriod, now = new Date()) {
  if (period === 'all') return {}
  const start = new Date(now); start.setHours(0, 0, 0, 0)
  if (period === 'week') start.setDate(start.getDate() - 6)
  const end = new Date(now)
  if (period === 'thisWeek' || period === 'lastWeek') {
    start.setDate(start.getDate() - (start.getDay() + 6) % 7)
    if (period === 'lastWeek') { end.setTime(start.getTime()); start.setDate(start.getDate() - 7) }
  }
  if (period === 'thisMonth' || period === 'lastMonth') {
    start.setDate(1)
    if (period === 'lastMonth') { end.setTime(start.getTime()); start.setMonth(start.getMonth() - 1) }
  }
  return { from_ms: start.getTime(), to_ms: end.getTime() }
}
export function customRankingBounds(from: string, to: string) {
  const parse = (value: string) => {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(value)) return null
    const [year, month, day] = value.split('-').map(Number)
    const date = new Date(year, month - 1, day)
    return date.getFullYear() === year && date.getMonth() === month - 1 && date.getDate() === day ? date : null
  }
  const start = parse(from), end = parse(to)
  if (!start || !end || start > end) return null
  end.setDate(end.getDate() + 1)
  return { from_ms: start.getTime(), to_ms: end.getTime() }
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
