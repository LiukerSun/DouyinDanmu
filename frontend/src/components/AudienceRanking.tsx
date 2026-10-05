import { useEffect, useState, type FormEvent } from 'react'
import { Button, Input } from '@heroui/react'
import { TrophyOutlined } from '@ant-design/icons'
import StudioSelect from './StudioSelect'
import { audienceRankFreshMs, audienceScore, defaultAudienceLimit, parseAudienceLimit, selectAudienceRanking } from '../pipeline/audience-ranking'
import type { RoomStateEvent } from '../pipeline/messages'
import './AudienceRanking.css'

const presets = [10, 20, 50, 100].map(limit => ({ id: String(limit), label: `前 ${limit} 位` }))

export default function AudienceRanking({ states, username, connected }: { states: RoomStateEvent[]; username: string; connected: boolean }) {
  const preferenceKey = 'monitor:audience-limit:' + username
  const [limit, setLimit] = useState(() => {
    try { return parseAudienceLimit(localStorage.getItem(preferenceKey)) ?? defaultAudienceLimit } catch { return defaultAudienceLimit }
  })
  const [custom, setCustom] = useState(false), [draft, setDraft] = useState(String(limit)), [error, setError] = useState('')
  const [collapsed, setCollapsed] = useState(false)
  useEffect(() => { try { localStorage.setItem(preferenceKey, String(limit)) } catch { /* Keep this session's preference. */ } }, [preferenceKey, limit])
  const state = selectAudienceRanking(states)
  const entries = state?.audience_ranks ?? []
  const visible = entries.slice(0, limit)
  const total = state?.audience_ranks_total ?? entries.length
  const stale = !!state && (!connected || Date.now() - state.received_at_ms > audienceRankFreshMs)
  const options = [...presets, ...(!presets.some(option => option.id === String(limit)) ? [{ id: String(limit), label: `前 ${limit} 位` }] : []), { id: 'custom', label: '自定义人数' }]
  const setPreference = (value: number) => { setLimit(value); setDraft(String(value)); setCustom(false); setError('') }
  const submit = (event: FormEvent) => {
    event.preventDefault()
    const value = parseAudienceLimit(draft)
    if (value === null) setError('请输入 1–100 的整数')
    else setPreference(value)
  }
  // Keep the message area available until a ranking has actually arrived.
  if (!state) return null

  return <section className="audience-rank-panel" aria-label="在线观众榜">
    <div className="audience-rank-heading">
      <TrophyOutlined /><strong>在线观众榜</strong>
      <span className="audience-rank-summary">{`${stale ? '上次观测' : '平台已下发'} ${total} 位 · 展示 ${visible.length} 位`}</span>
      <div className="audience-rank-controls">
        <StudioSelect compact label="观众榜展示人数" value={custom ? 'custom' : String(limit)} options={options} onChange={value => {
          if (value === 'custom') { setCustom(true); setDraft(String(limit)); setError('') }
          else { const parsed = parseAudienceLimit(value); if (parsed !== null) setPreference(parsed) }
        }} />
        <Button size="sm" variant="ghost" aria-expanded={!collapsed} aria-controls="audience-rank-entries" onPress={() => setCollapsed(value => !value)}>{collapsed ? '展开观众榜' : '收起观众榜'}</Button>
      </div>
    </div>
    {custom && <form className="audience-rank-custom" onSubmit={submit} noValidate>
      <Input aria-label="自定义观众榜人数" type="number" inputMode="numeric" min={1} max={100} step={1} value={draft} onChange={event => { setDraft(event.target.value); setError('') }} aria-invalid={!!error} aria-describedby="audience-rank-custom-help" />
      <Button type="submit" size="sm" variant="secondary">应用人数</Button>
      <span id="audience-rank-custom-help" role={error ? 'alert' : undefined}>{error || '1–100 位'}</span>
    </form>}
    {!collapsed && <div id="audience-rank-entries">
      {visible.length ? <ol className="audience-rank-list" aria-label="平台观众榜名单">{visible.map((entry, index) => <li key={`${entry.rank}:${entry.user_id}:${index}`}>
        <span className={'audience-rank-place' + (entry.rank <= 3 ? ' audience-rank-place-leading' : '')}>{entry.rank || index + 1}</span>
        <span className="audience-rank-name" title={entry.hidden ? undefined : entry.user_name}>{entry.hidden || !entry.user_name ? '神秘人' : entry.user_name}</span>
        <span className="audience-rank-score" title={audienceScore(entry)}>{audienceScore(entry)}</span>
      </li>)}</ol> : <p className="audience-rank-empty">平台本次未提供观众榜名单</p>}
      <p className="audience-rank-note">{new Date(state.received_at_ms).toLocaleTimeString('zh-CN', { hour12: false })} {stale ? '观测' : '更新'} · 贡献按平台提供的数值或描述显示{total < limit ? ` · 当前仅下发 ${total} 位` : ''}</p>
    </div>}
  </section>
}
