import { useEffect, useState } from 'react'
import { Button, Chip, EmptyState, Spinner } from '@heroui/react'
import { ArrowLeftOutlined, HistoryOutlined, MessageOutlined, ReloadOutlined, TrophyOutlined } from '@ant-design/icons'
import StudioFilters from './StudioFilters'
import InlineFeedback from './InlineFeedback'
import PipelineMessage from './PipelineMessage'
import MessageDetails from './MessageDetails'
import { request } from '../pipeline/monitor'
import { exactCount, type RankingResult } from '../pipeline/rankings'
import { sessionEventsQuery, sessionRankingQuery, sessionsQuery, type LiveSession, type SessionListResult } from '../pipeline/sessions'
import type { PipelineEvent } from '../pipeline/messages'
import './RoomSessions.css'

const startedAt = (value: number) => new Date(value).toLocaleString('zh-CN', { year: 'numeric', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit', hour12: false })
const statusLabel = (session: LiveSession) => session.status === 'live' ? '进行中' : '已结束'

function SessionEvents({ room, session, type }: { room: string; session: LiveSession; type: 'chat' | 'gift' }) {
  const [cursors, setCursors] = useState<string[]>([''])
  const [result, setResult] = useState<{ events: PipelineEvent[]; total: number; next_before: string | null } | null>(null)
  const [loading, setLoading] = useState(true), [error, setError] = useState(''), [revision, setRevision] = useState(0)
  const [detail, setDetail] = useState<PipelineEvent | null>(null)
  const before = cursors[cursors.length - 1]
  useEffect(() => {
    const controller = new AbortController()
    setLoading(true); setResult(null); setError('')
    void request<NonNullable<typeof result>>('/messages/search?' + sessionEventsQuery(room, session, type, before), { signal: controller.signal }).then(data => {
      if (!controller.signal.aborted) setResult(data)
    }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    return () => controller.abort()
  }, [room, session, type, before, revision])
  return <>
    {error && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => setRevision(n => n + 1)}>重试</Button>}>{error}</InlineFeedback>}
    <div className="message-list session-events-list" aria-label={type === 'chat' ? '该场次的弹幕记录' : '该场次的礼物记录'} aria-busy={loading}>
      {loading && <div className="session-empty" role="status"><Spinner size="md" /><span>正在查询本场{type === 'chat' ? '弹幕' : '礼物'}记录…</span></div>}
      {!loading && !error && !result?.events.length && <EmptyState className="session-empty"><MessageOutlined /><strong>{type === 'chat' ? '该场次没有弹幕记录' : '该场次没有礼物记录'}</strong></EmptyState>}
      {result?.events.map(event => <PipelineMessage key={event.event_id} event={event} showDate onInspectEvent={setDetail} />)}
    </div>
    <div className="session-pagination"><span>{result ? `${result.total.toLocaleString('zh-CN')} 条${type === 'chat' ? '弹幕' : '礼物'}记录` : ''}</span><div><Button size="sm" variant="secondary" isDisabled={loading || cursors.length === 1} onPress={() => setCursors(values => values.slice(0, -1))}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || !result?.next_before} onPress={() => { if (result?.next_before) setCursors(values => [...values, result.next_before!]) }}>下一页</Button></div></div>
    <MessageDetails event={detail} onClose={() => setDetail(null)} />
  </>
}

function SessionGiftRanking({ room, session }: { room: string; session: LiveSession }) {
  const [page, setPage] = useState<{ offset: number; snapshot?: string }>({ offset: 0 })
  const [response, setResponse] = useState<{ key: string; data: RankingResult } | null>(null)
  const [loading, setLoading] = useState(true), [error, setError] = useState(''), [revision, setRevision] = useState(0)
  const query = sessionRankingQuery(room, session, page.offset, page.snapshot)
  const key = query + ':' + revision
  const result = response?.key === key ? response.data : null
  useEffect(() => {
    const controller = new AbortController()
    setLoading(true); setError('')
    void request<RankingResult>('/analytics/gift-ranking?' + query, { signal: controller.signal }).then(data => {
      if (!controller.signal.aborted) setResponse({ key, data })
    }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    return () => controller.abort()
  }, [query, key])
  return <>
    <p className="session-note">本场次送礼贡献按已知礼物价值排序；价值单位为钻石，缺少价格的礼物单独标注。</p>
    {error && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => { setPage({ offset: 0 }); setRevision(n => n + 1) }}>重试</Button>}>{error}</InlineFeedback>}
    <div className="session-table-scroll" aria-busy={loading}>
      {loading && !result && <div className="session-empty" role="status"><Spinner size="md" /><span>正在统计本场送礼记录…</span></div>}
      {!loading && !error && result?.items.length === 0 && <EmptyState className="session-empty"><TrophyOutlined /><strong>该场次没有可排行的送礼记录</strong><p>本场尚未采集到带用户信息的送礼消息。</p></EmptyState>}
      {result && result.items.length > 0 && <table className="session-table" aria-label="该场次的礼物贡献榜">
        <thead><tr><th scope="col">排名</th><th scope="col">用户</th><th scope="col" className="session-numeric">已知礼物价值</th><th scope="col" className="session-numeric">礼物数量</th></tr></thead>
        <tbody>{result.items.map(item => <tr key={item.user_id}><td><span className={'session-place' + (item.rank <= 3 ? ' session-place-leading' : '')}>{item.rank}</span></td><th scope="row"><div className="session-user"><strong>{item.user_name || '未提供昵称'}</strong><span>UID {item.user_id}{item.user_level !== null && <small>财富 Lv.{item.user_level}</small>}</span></div></th>
          <td className="session-numeric"><strong>{item.unknown_price_quantity === item.gift_quantity ? '价格未知' : exactCount(item.known_gift_value)}</strong>{!item.value_complete && <small className="session-incomplete">{exactCount(item.unknown_price_quantity)} 件价格未知</small>}</td><td className="session-numeric">{exactCount(item.gift_quantity)}</td></tr>)}</tbody>
      </table>}
    </div>
    <div className="session-pagination"><span>{result ? `第 ${page.offset / 20 + 1} 页 · 共 ${exactCount(result.total)} 位用户` : ''}</span><div><Button size="sm" variant="secondary" isDisabled={loading || page.offset === 0 || !result} onPress={() => { if (result) setPage({ offset: Math.max(0, page.offset - 20), snapshot: result.meta.as_of_seq }) }}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || result?.next_offset == null} onPress={() => { if (result?.next_offset != null) setPage({ offset: result.next_offset, snapshot: result.meta.as_of_seq }) }}>下一页</Button></div></div>
  </>
}

function SessionDetail({ room, session, onBack }: { room: string; session: LiveSession; onBack: () => void }) {
  const [tab, setTab] = useState('chat')
  const stats = session.stats
  return <div className="room-sessions session-detail">
    <div className="session-detail-heading">
      <Button size="sm" variant="secondary" aria-label="返回场次列表" onPress={onBack}><ArrowLeftOutlined />返回场次列表</Button>
      <div><strong>场次 #{session.id}</strong><Chip size="sm" variant="soft" color={session.status === 'live' ? 'success' : 'default'}>{statusLabel(session)}</Chip>
        <p>{startedAt(session.started_at_ms)} 开播 · {session.ended_at_ms !== null ? startedAt(session.ended_at_ms) + ' 结束' : '进行中'} · 峰值观看 {stats.peak_online.toLocaleString('zh-CN')} 人</p></div>
    </div>
    <dl className="session-metrics" aria-label="本场次数据">
      <div><dt>总收入</dt><dd>{exactCount(stats.known_gift_value)}{!stats.value_complete && '+'}<small>钻石{!stats.value_complete && ` · 含 ${exactCount(stats.unknown_price_quantity)} 件价格未知`}</small></dd></div>
      <div><dt>礼物数</dt><dd>{exactCount(stats.gift_quantity)}<small>件</small></dd></div>
      <div><dt>弹幕数</dt><dd>{exactCount(stats.chat_count)}</dd></div>
      <div><dt>点赞数</dt><dd>{exactCount(stats.like_count)}</dd></div>
    </dl>
    <StudioFilters label="场次记录类型" className="session-detail-tabs" value={tab} onChange={setTab} options={[{ id: 'chat', label: '弹幕记录' }, { id: 'gift', label: '礼物记录' }, { id: 'ranking', label: '贡献榜' }]} />
    {tab === 'ranking' ? <SessionGiftRanking room={room} session={session} /> : <SessionEvents key={tab} room={room} session={session} type={tab as 'chat' | 'gift'} />}
  </div>
}

export default function RoomSessions({ room }: { room: string }) {
  const [offset, setOffset] = useState(0)
  const [response, setResponse] = useState<{ key: string; data: SessionListResult } | null>(null)
  const [loading, setLoading] = useState(true), [error, setError] = useState(''), [revision, setRevision] = useState(0)
  const [inspected, setInspected] = useState<LiveSession | null>(null)
  const query = sessionsQuery(room, offset)
  const key = query + ':' + revision
  const result = response?.key === key ? response.data : null
  useEffect(() => {
    const controller = new AbortController()
    setLoading(true); setError('')
    void request<SessionListResult>(`/rooms/${room}/sessions?${query}`, { signal: controller.signal }).then(data => {
      if (!controller.signal.aborted) setResponse({ key, data })
    }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    return () => controller.abort()
  }, [room, query, key])
  if (inspected) return <SessionDetail room={room} session={inspected} onBack={() => setInspected(null)} />
  return <div className="room-sessions">
    <div className="session-context"><span><strong>直播场次</strong> · 按采集到的开播与结束边界划分</span><div><span>{result ? `共 ${exactCount(result.total)} 场` : loading ? '正在统计…' : ''}</span><Button size="sm" variant="secondary" aria-label="刷新场次" isDisabled={loading} onPress={() => { setOffset(0); setRevision(n => n + 1) }}><ReloadOutlined />刷新</Button></div></div>
    {error && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => setRevision(n => n + 1)}>重试</Button>}>{error}</InlineFeedback>}
    <div className="session-table-scroll" aria-busy={loading}>
      {loading && !result && <div className="session-empty" role="status"><Spinner size="md" /><span>正在统计本房间场次…</span></div>}
      {!loading && !error && result?.items.length === 0 && <EmptyState className="session-empty"><HistoryOutlined /><strong>该房间还没有采集到场次</strong><p>开播后自动记录场次；仅统计监控期间观测到的直播边界。</p></EmptyState>}
      {result && result.items.length > 0 && <table className="session-table" aria-label="直播场次列表">
        <thead><tr><th scope="col">开播时间</th><th scope="col">状态</th><th scope="col" className="session-numeric">收入</th><th scope="col" className="session-numeric">峰值观看</th><th scope="col"><span className="sr-only">查看详情</span></th></tr></thead>
        <tbody>{result.items.map(session => <tr key={session.id}><td className="session-started">{startedAt(session.started_at_ms)}</td><td><Chip size="sm" variant="soft" color={session.status === 'live' ? 'success' : 'default'}>{statusLabel(session)}</Chip></td>
          <td className="session-numeric"><strong>{exactCount(session.stats.known_gift_value)}{!session.stats.value_complete && '+'}</strong><small> 钻石</small>{!session.stats.value_complete && <small className="session-incomplete">含 {exactCount(session.stats.unknown_price_quantity)} 件价格未知</small>}</td>
          <td className="session-numeric">{session.stats.peak_online.toLocaleString('zh-CN')}</td>
          <td><Button size="sm" variant="ghost" onPress={() => setInspected(session)} aria-label={`查看 ${startedAt(session.started_at_ms)} 开播场次的详情`}>详情</Button></td></tr>)}</tbody>
      </table>}
    </div>
    <div className="session-pagination"><span>{result ? `第 ${offset / 20 + 1} 页 · 共 ${exactCount(result.total)} 场` : ''}</span><div><Button size="sm" variant="secondary" isDisabled={loading || offset === 0 || !result} onPress={() => { if (result) setOffset(Math.max(0, offset - 20)) }}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || result?.next_offset == null} onPress={() => { if (result?.next_offset != null) setOffset(result.next_offset) }}>下一页</Button></div></div>
  </div>
}
