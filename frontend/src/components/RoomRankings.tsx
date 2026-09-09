import { useEffect, useMemo, useRef, useState } from 'react'
import { Button, EmptyState, Spinner } from '@heroui/react'
import { ArrowLeftOutlined, MessageOutlined, ReloadOutlined, TrophyOutlined } from '@ant-design/icons'
import StudioFilters from './StudioFilters'
import StudioSelect from './StudioSelect'
import InlineFeedback from './InlineFeedback'
import PipelineMessage from './PipelineMessage'
import MessageDetails from './MessageDetails'
import { request } from '../pipeline/monitor'
import { exactCount, rankingBounds, rankingQuery, type RankingMeta, type RankingPeriod, type RankingResult, type RankingUser } from '../pipeline/rankings'
import type { PipelineEvent } from '../pipeline/messages'
import './RoomRankings.css'

const periods = [{ id: 'all', label: '全部已采集记录' }, { id: 'today', label: '今天' }, { id: 'week', label: '近 7 天' }]
const observedTime = (value: number) => new Date(value).toLocaleString('zh-CN', { month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit', hour12: false })

function RoomUserHistory({ room, roomName, user, meta, onBack }: { room: string; roomName: string; user: RankingUser; meta: RankingMeta; onBack: () => void }) {
  const [cursors, setCursors] = useState<string[]>([''])
  const [result, setResult] = useState<{ events: PipelineEvent[]; total: number; next_before: string | null } | null>(null)
  const [loading, setLoading] = useState(true), [error, setError] = useState(''), [revision, setRevision] = useState(0)
  const [detail, setDetail] = useState<PipelineEvent | null>(null)
  const before = cursors[cursors.length - 1]
  useEffect(() => {
    const controller = new AbortController()
    const params = new URLSearchParams({ room, user_id: user.user_id, view: 'events', as_of_seq: meta.as_of_seq, limit: '50' })
    if (meta.from_ms !== null) params.set('from_ms', String(meta.from_ms))
    if (meta.to_ms !== null) params.set('to_ms', String(meta.to_ms))
    if (before) params.set('before', before)
    setLoading(true); setResult(null); setError('')
    void request<NonNullable<typeof result>>('/messages/search?' + params, { signal: controller.signal }).then(data => {
      if (!controller.signal.aborted) setResult(data)
    }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    return () => controller.abort()
  }, [room, user.user_id, meta, before, revision])
  return <div className="room-rankings ranking-history">
    <div className="ranking-history-heading"><Button size="sm" variant="secondary" aria-label="返回排行榜" onPress={onBack}><ArrowLeftOutlined />返回排行榜</Button><div><strong>{user.user_name || '未提供昵称'}</strong><span>UID {user.user_id}</span><p>{roomName} · 榜单时间范围内的全部行为</p></div></div>
    {error && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => setRevision(n => n + 1)}>重试</Button>}>{error}</InlineFeedback>}
    <div className="message-list ranking-history-list" aria-label="该用户在本房间的记录" aria-busy={loading}>
      {loading && <div className="ranking-empty" role="status"><Spinner size="md" /><span>正在查询本房间记录…</span></div>}
      {!loading && !error && !result?.events.length && <EmptyState className="ranking-empty"><MessageOutlined /><strong>该时间范围内没有记录</strong></EmptyState>}
      {result?.events.map(event => <PipelineMessage key={event.event_id} event={event} showDate onInspectEvent={setDetail} />)}
    </div>
    <div className="ranking-pagination"><span>{result ? `${result.total.toLocaleString('zh-CN')} 条行为记录` : ''}</span><div><Button size="sm" variant="secondary" isDisabled={loading || cursors.length === 1} onPress={() => setCursors(values => values.slice(0, -1))}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || !result?.next_before} onPress={() => { if (result?.next_before) setCursors(values => [...values, result.next_before!]) }}>下一页</Button></div></div>
    <MessageDetails event={detail} onClose={() => setDetail(null)} />
  </div>
}

export default function RoomRankings({ room, roomName }: { room: string; roomName: string }) {
  const [kind, setKind] = useState('chat'), [period, setPeriod] = useState<RankingPeriod>('all'), [sort, setSort] = useState('value')
  const [page, setPage] = useState<{ offset: number; snapshot?: string }>({ offset: 0 })
  const [revision, setRevision] = useState(0)
  const [response, setResponse] = useState<{ key: string; data: RankingResult; updated: number } | null>(null)
  const [loading, setLoading] = useState(true), [error, setError] = useState('')
  const [inspected, setInspected] = useState<RankingUser | null>(null)
  const scroll = useRef<HTMLDivElement>(null)
  const savedScroll = useRef(0)
  const bounds = useMemo(() => rankingBounds(period), [period, revision])
  const query = rankingQuery(room, bounds, page.offset, page.snapshot, sort)
  const key = `${kind}:${query}:${revision}`
  const result = response?.key === key ? response.data : null
  useEffect(() => {
    const controller = new AbortController()
    setLoading(true); setError('')
    void request<RankingResult>(`/analytics/${kind}-ranking?${query}`, { signal: controller.signal }).then(data => {
      if (!controller.signal.aborted) { setResponse({ key, data, updated: Date.now() }); scroll.current?.scrollTo({ top: 0 }) }
    }).catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) }).finally(() => { if (!controller.signal.aborted) setLoading(false) })
    return () => controller.abort()
  }, [kind, query, key])
  const resetPage = () => { setPage({ offset: 0 }); setInspected(null) }
  const inspect = (user: RankingUser) => { savedScroll.current = scroll.current?.scrollTop ?? 0; setInspected(user) }
  if (inspected && result) return <RoomUserHistory room={room} roomName={roomName} user={inspected} meta={result.meta} onBack={() => { setInspected(null); requestAnimationFrame(() => scroll.current?.scrollTo({ top: savedScroll.current })) }} />
  return <div className="room-rankings">
    <div className="ranking-toolbar">
      <StudioFilters label="排行榜类型" className="ranking-kind" value={kind} onChange={value => { setKind(value); resetPage() }} options={[{ id: 'chat', label: '弹幕榜' }, { id: 'gift', label: '礼物榜' }]} />
      <div className="ranking-options"><StudioSelect label="排行榜时间范围" value={period} options={periods} onChange={value => { setPeriod(value as RankingPeriod); resetPage() }} />
        {kind === 'gift' && <StudioSelect label="礼物榜排序" value={sort} options={[{ id: 'value', label: '按已知价值' }, { id: 'quantity', label: '按礼物数量' }]} onChange={value => { setSort(value); resetPage() }} />}
        <Button size="sm" variant="secondary" aria-label="刷新榜单" isDisabled={loading} onPress={() => { resetPage(); setRevision(n => n + 1) }}><ReloadOutlined />刷新榜单</Button></div>
    </div>
    <div className="ranking-context"><span><strong>{roomName}</strong> · 仅统计本房间采集记录</span><span>{result ? `${exactCount(result.total)} 位用户` : loading ? '正在统计…' : ''}</span></div>
    {kind === 'gift' && <p className="ranking-note">礼物连送按新增数量计入；价值单位为钻石，缺少价格的礼物单独标注。</p>}
    {error && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => setRevision(n => n + 1)}>重试</Button>}>{error}</InlineFeedback>}
    <div className="ranking-table-scroll" ref={scroll} aria-busy={loading}>
      {loading && !result && <div className="ranking-empty" role="status"><Spinner size="md" /><span>正在统计本房间{kind === 'chat' ? '发言' : '送礼'}记录…</span></div>}
      {!loading && !error && result?.items.length === 0 && <EmptyState className="ranking-empty"><TrophyOutlined /><strong>{kind === 'chat' ? '还没有可排行的发言记录' : '还没有可排行的送礼记录'}</strong><p>当前范围内尚未采集到{kind === 'chat' ? '带用户信息的文字或表情弹幕' : '带用户信息的送礼消息'}，可调整时间范围或稍后刷新。</p></EmptyState>}
      {result && result.items.length > 0 && <table className="ranking-table" aria-label={`${roomName}的${kind === 'chat' ? '弹幕榜' : '礼物榜'}`}>
        <thead><tr><th scope="col">排名</th><th scope="col">用户</th>{kind === 'chat' ? <th scope="col" className="ranking-numeric">弹幕条数</th> : <><th scope="col" className="ranking-numeric">已知礼物价值</th><th scope="col" className="ranking-numeric">礼物数量</th></>}<th scope="col">最近出现</th><th scope="col"><span className="sr-only">查看记录</span></th></tr></thead>
        <tbody>{result.items.map(item => <tr key={item.user_id}><td><span className={'ranking-place' + (item.rank <= 3 ? ' ranking-place-leading' : '')}>{item.rank}</span></td><th scope="row"><div className="ranking-user"><Button variant="ghost" className="ranking-user-name" onPress={() => inspect(item)}>{item.user_name || '未提供昵称'}</Button><span>UID {item.user_id}{item.user_level !== null && <small>财富 Lv.{item.user_level}</small>}</span></div></th>
          {kind === 'chat' ? <td className="ranking-numeric"><strong>{exactCount(item.chat_count)}</strong></td> : <><td className="ranking-numeric"><strong>{item.unknown_price_quantity === item.gift_quantity ? '价格未知' : exactCount(item.known_gift_value)}</strong>{!item.value_complete && <small className="ranking-incomplete">{exactCount(item.unknown_price_quantity)} 件价格未知</small>}</td><td className="ranking-numeric">{exactCount(item.gift_quantity)}</td></>}
          <td className="ranking-last-seen">{observedTime(item.last_seen_at_ms)}</td><td><Button size="sm" variant="ghost" onPress={() => inspect(item)} aria-label={`查看 ${item.user_name || item.user_id} 在本房间的记录`}>本房间记录</Button></td></tr>)}</tbody>
      </table>}
    </div>
    <div className="ranking-pagination"><span>{result ? `第 ${page.offset / 20 + 1} 页 · ${observedTime(response!.updated)} 更新` : ''}</span><div><Button size="sm" variant="secondary" isDisabled={loading || page.offset === 0 || !result} onPress={() => { if (result) setPage({ offset: Math.max(0, page.offset - 20), snapshot: result.meta.as_of_seq }) }}>上一页</Button><Button size="sm" variant="secondary" isDisabled={loading || result?.next_offset == null} onPress={() => { if (result?.next_offset != null) setPage({ offset: result.next_offset, snapshot: result.meta.as_of_seq }) }}>下一页</Button></div></div>
  </div>
}
