import type { AudienceRank, RoomStateEvent } from './messages'

export const defaultAudienceLimit = 10
export const maximumAudienceLimit = 100
export const audienceRankFreshMs = 60_000

export function parseAudienceLimit(value: string | null | undefined): number | null {
  if (!value || !/^[1-9]\d{0,2}$/.test(value.trim())) return null
  const limit = Number(value.trim())
  return limit <= maximumAudienceLimit ? limit : null
}

export function selectAudienceRanking(states: RoomStateEvent[]): RoomStateEvent | undefined {
  const extended = states.find(state => state.type === 'audience_rank')
  const online = states.find(state => state.type === 'online_count' && Array.isArray(state.audience_ranks))
  // A fresh expanded list survives frequent top-three online updates. Once
  // older by a minute, the more recent online observation takes over.
  if (extended && (!online || online.received_at_ms - extended.received_at_ms <= audienceRankFreshMs)) return extended
  return online
}

export function audienceScore(entry: AudienceRank): string {
  if (entry.hidden) return '未公开'
  const description = entry.score_description?.trim() || entry.exactly_score?.trim()
  if (description) return description
  if (entry.score == null || entry.score === '') return '未提供'
  if (typeof entry.score === 'number' && !Number.isSafeInteger(entry.score)) return '未提供'
  try { return BigInt(entry.score).toLocaleString('zh-CN') } catch { return '未提供' }
}
