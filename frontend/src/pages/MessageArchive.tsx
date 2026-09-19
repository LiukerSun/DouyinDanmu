import { useEffect, useMemo, useRef, useState } from 'react'
import { Button, Chip, EmptyState, Spinner } from '@heroui/react'
import { DatabaseOutlined, ReloadOutlined, SearchOutlined, UserOutlined } from '@ant-design/icons'
import StudioSelect from '../components/StudioSelect'
import StudioSearch from '../components/StudioSearch'
import PipelineMessage from '../components/PipelineMessage'
import MessageDetails from '../components/MessageDetails'
import InlineFeedback from '../components/InlineFeedback'
import { request, roomName, type Room } from '../pipeline/monitor'
import { messageDisplayKey, messageKind, type PipelineEvent } from '../pipeline/messages'
import { fansBands, wealthBands, feedMessageTypes, identityTone } from '../pipeline/message-colors'
import './MessageArchive.css'

export type ArchiveUser = { id: string; name: string }
type Result = { events: PipelineEvent[]; total: number; next_before: string | null }
type Filters = { room: string; type: string; q: string; wealth: string; fans: string; view: string }
const defaults: Filters = { room: '', type: '', q: '', wealth: '', fans: '', view: 'events' }
const levelOptions = (bands: readonly { min: number; label: string }[]) => [{ id: '', label: '全部等级' }, ...bands.map(b => ({ id: String(b.min), label: 'Lv.' + b.label })), { id: 'unknown', label: '等级未提供' }]

export default function MessageArchive({ rooms, user, onUserChange, onRoom }: { rooms: Room[]; user: ArchiveUser | null; onUserChange: (user: ArchiveUser | null) => void; onRoom: (id: string) => void }) {
  const [inspectedEvent, setInspectedEvent] = useState<PipelineEvent | null>(null)
  const [filters, setFilters] = useState<Filters>(defaults)
  const [cursors, setCursors] = useState<string[]>([''])
  const [result, setResult] = useState<Result | null>(null), [loading, setLoading] = useState(true), [error, setError] = useState('')
  const [revision, setRevision] = useState(0)
  const list = useRef<HTMLDivElement>(null)
  // Navigation and message actions share the same user scope. A new scope
  // starts at the first page, including when the parent clears it from the rail.
  useEffect(() => { setCursors(['']) }, [user?.id])
  const query = useMemo(() => {
    const params = new URLSearchParams({ limit: '50', view: filters.view })
    if (filters.room) params.set('room', filters.room)
    if (filters.type) params.set('type', filters.type)
    if (filters.q.trim()) params.set('q', filters.q.trim())
    if (user) params.set('user_id', user.id)
    const before = cursors[cursors.length - 1]; if (before) params.set('before', before)
    for (const [key, bands, value] of [['wealth', wealthBands, filters.wealth], ['fans', fansBands, filters.fans]] as const) {
      if (value === 'unknown') params.set(key + '_unknown', '1')
      else if (value !== '') {
        const index = bands.findIndex(b => String(b.min) === value)
        if (index >= 0) { params.set(key + '_min', value); if (bands[index + 1]) params.set(key + '_max', String(bands[index + 1].min - 1)) }
      }
    }
    return params.toString()
  }, [filters, user, cursors])
  useEffect(() => {
    const controller = new AbortController()
    setLoading(true); setError(''); setResult(null)
    const timer = setTimeout(() => {
      void request<Result>('/messages/search?' + query, { signal: controller.signal }).then(data => {
        if (controller.signal.aborted) return
        setResult(data); list.current?.scrollTo({ top: 0 })
      }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    }, 250)
    return () => { clearTimeout(timer); controller.abort() }
  }, [query, revision])
  const change = (key: keyof Filters, value: string) => { setFilters(previous => ({ ...previous, [key]: value })); setCursors(['']) }
  const clear = () => { setFilters(defaults); onUserChange(null); setCursors(['']) }
  const inspect = (event: PipelineEvent) => { onUserChange({ id: event.user_id, name: event.user_name }); setFilters(defaults); setCursors(['']) }
  const hasFilters = user || Object.entries(filters).some(([key, value]) => key !== 'view' && !!value)
  return <main className="archive-panel" aria-label="所有直播间信息汇总">
    <div className="panel-heading"><div><h2><DatabaseOutlined />全部行为记录</h2><p>查询观众互动与直播间通知，按入库时间倒序排列。</p></div><Button size="sm" variant="secondary" isDisabled={loading} onPress={() => { setCursors(['']); setRevision(n => n + 1) }}><ReloadOutlined />刷新记录</Button></div>
    <div className="archive-tools">
      <StudioSearch label="搜索全部行为记录" placeholder="搜索内容、昵称或 UID" value={filters.q} onChange={value => change('q', value)} clearLabel="清空汇总搜索" />
      <div className="archive-filter-grid">
        <StudioSelect label="汇总直播间筛选" visibleLabel="直播间" value={filters.room} onChange={v => change('room', v)} options={[{ id: '', label: '全部直播间' }, ...rooms.map(room => ({ id: room.live_id, label: roomName(room), description: room.live_id }))]} />
        <StudioSelect label="汇总消息类型" visibleLabel="消息类型" value={filters.type} onChange={v => change('type', v)} options={[{ id: '', label: '全部类型' }, ...feedMessageTypes.map(t => ({ id: t.type, label: t.label }))]} />
        <StudioSelect label="汇总财富等级" visibleLabel="财富等级" value={filters.wealth} onChange={v => change('wealth', v)} options={levelOptions(wealthBands)} />
        <StudioSelect label="汇总粉丝等级" visibleLabel="粉丝等级" value={filters.fans} onChange={v => change('fans', v)} options={levelOptions(fansBands)} />
        <StudioSelect label="记录口径" visibleLabel="记录口径" value={filters.view} onChange={v => change('view', v)} options={[{ id: 'events', label: '全部事件（含连送过程）' }, { id: 'merged', label: '合并礼物连送' }]} />
        <Button size="sm" variant="ghost" isDisabled={!hasFilters} onPress={clear}>重置筛选</Button>
      </div>
      {user && <div className="archive-user-scope"><UserOutlined /><span>正在查看 <strong>{user.name || '该用户'}</strong> 的互动记录</span><Chip size="sm">UID {user.id}</Chip><Button size="sm" variant="ghost" onPress={() => { onUserChange(null); setCursors(['']) }}>取消用户筛选</Button></div>}
    </div>
    <div className="archive-result-heading" role="status">{loading ? '正在查询历史记录…' : error ? '查询未完成' : `共 ${result?.total ?? 0} 条记录`}</div>
    {error && <InlineFeedback className="archive-error" action={<Button size="sm" variant="ghost" onPress={() => setRevision(n => n + 1)}>重试</Button>}>{error}</InlineFeedback>}
    <div className="message-list archive-results" ref={list} aria-label="汇总查询结果" aria-busy={loading}>
      {loading && <div className="archive-empty"><Spinner size="md" /><span>正在查询</span></div>}
      {!loading && !error && result?.events.length === 0 && <EmptyState className="archive-empty"><SearchOutlined /><strong>没有符合条件的记录</strong><span>调整筛选条件，或等待直播间产生新的互动。</span>{hasFilters && <Button size="sm" variant="secondary" onPress={clear}>清除全部筛选</Button>}</EmptyState>}
      {result?.events.map(event => <div className="sourced-message" data-message-type={messageKind(event)} key={filters.view === 'events' ? event.event_id : messageDisplayKey(event)}><Button variant="ghost" size="sm" className="message-room-label" data-tone={identityTone(event.live_id)} onPress={() => onRoom(event.live_id)} aria-label={'切换到直播间 ' + (rooms.find(r => r.live_id === event.live_id)?.metadata?.anchor.nickname || event.live_id)}><span className="source-channel-dot" />{rooms.find(r => r.live_id === event.live_id)?.metadata?.anchor.nickname || event.live_id}</Button><PipelineMessage event={event} showDate onInspectUser={inspect} onInspectEvent={setInspectedEvent} /></div>)}
    </div>
    <div className="conversation-footer archive-pagination"><span>第 {cursors.length} 页 · 每页 50 条</span><div><Button size="sm" variant="secondary" isDisabled={loading || cursors.length === 1} onPress={() => setCursors(previous => previous.slice(0, -1))}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || !result?.next_before} onPress={() => { if (result?.next_before) setCursors(previous => [...previous, result.next_before!]) }}>下一页</Button></div></div>
    <MessageDetails event={inspectedEvent} onClose={() => setInspectedEvent(null)} />
  </main>
}
