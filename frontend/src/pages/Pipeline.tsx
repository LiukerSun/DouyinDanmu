import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from 'react'
import { Avatar as HeroAvatar, Button, Card, Checkbox, Chip, EmptyState, Label, Link, Modal, TextArea, Toast, Tooltip, Spinner } from '@heroui/react'
import { AppstoreOutlined, ArrowLeftOutlined, DeleteOutlined, CheckCircleOutlined, CloseOutlined, MenuFoldOutlined, DatabaseOutlined, MessageOutlined, PauseOutlined, PlayCircleOutlined, PlusOutlined, PushpinFilled, PushpinOutlined, ReloadOutlined, SearchOutlined, SettingOutlined, ThunderboltFilled, UserOutlined, WarningOutlined, WifiOutlined } from '@ant-design/icons'
import StudioSelect from '../components/StudioSelect'
import StudioSearch from '../components/StudioSearch'
import StudioFilters from '../components/StudioFilters'
import InlineFeedback from '../components/InlineFeedback'
import PipelineMessage from '../components/PipelineMessage'
import MessageDetails from '../components/MessageDetails'
import MessageArchive, { type ArchiveUser } from './MessageArchive'
import CollectorIdentity from '../components/CollectorIdentity'
import CookieSettings from '../components/CookieSettings'
import RoomRankings from '../components/RoomRankings'
import { useRoomStreams } from '../hooks/useRoomStreams'
import { useRoomReorder } from '../hooks/useRoomReorder'
import { wealthBands, fansBands, feedMessageTypes } from '../pipeline/message-colors'
import { completeRoomOrder, reorderVisibleRooms } from '../pipeline/room-order'
import { type PipelineEvent, messageDisplayKey, messageKind, messageContent } from '../pipeline/messages'
import { type Room, type Health, request, roomName, isLive, isStale, needsAttention, parseRoomInput, statusLabels, formatNumber as number } from '../pipeline/monitor'

import './Pipeline.css'
import type { StudioUser } from '../auth/AuthApp'

function RoomDragHandle({ name, handlers }: { name: string; handlers: ReturnType<ReturnType<typeof useRoomReorder>['handleProps']> }) {
  const { onKeyDown, onPointerDown, ...pointerHandlers } = handlers
  // HeroUI handles keyboard/focus; the surface preserves pointer capture for dragging.
  return <span className="room-drag-surface" onPointerDownCapture={onPointerDown} {...pointerHandlers}><Button type="button" variant="ghost" isIconOnly className="room-drag-handle" aria-label={'拖动排序 ' + name} aria-describedby="room-reorder-help" onKeyDown={onKeyDown}><svg width="12" height="16" viewBox="0 0 12 16" fill="currentColor" aria-hidden="true"><circle cx="3" cy="3" r="1.3" /><circle cx="9" cy="3" r="1.3" /><circle cx="3" cy="8" r="1.3" /><circle cx="9" cy="8" r="1.3" /><circle cx="3" cy="13" r="1.3" /><circle cx="9" cy="13" r="1.3" /></svg></Button></span>
}

type View = 'overview' | 'attention' | 'archive' | 'room'

const time = (n: number) => new Date(n).toLocaleTimeString('zh-CN', { hour12: false })
function Check({ selected, onChange, label, mixed = false }: { selected: boolean; onChange: (v: boolean) => void; label: string; mixed?: boolean }) {
  return <Checkbox isSelected={selected} isIndeterminate={mixed} onChange={onChange}><Checkbox.Content><Checkbox.Control><Checkbox.Indicator /></Checkbox.Control><Label className="sr-only">{label}</Label></Checkbox.Content></Checkbox>
}
function Dialog({ open, onChange, title, children, footer }: { open: boolean; onChange: (v: boolean) => void; title: string; children: ReactNode; footer?: ReactNode }) {
  return <Modal.Backdrop isOpen={open} onOpenChange={onChange}><Modal.Container size="lg"><Modal.Dialog className="console-dialog"><Modal.CloseTrigger aria-label="关闭弹窗" /><Modal.Header><Modal.Heading>{title}</Modal.Heading></Modal.Header><Modal.Body>{children}</Modal.Body>{footer && <Modal.Footer>{footer}</Modal.Footer>}</Modal.Dialog></Modal.Container></Modal.Backdrop>
}
function Avatar({ room }: { room: Room }) {
  return <HeroAvatar className="room-avatar"><HeroAvatar.Image src={room.metadata?.anchor.avatar_url || undefined} alt="" referrerPolicy="no-referrer" /><HeroAvatar.Fallback>{Array.from(roomName(room))[0]}</HeroAvatar.Fallback></HeroAvatar>
}
function Status({ room }: { room: Room }) {
  return <Chip size="sm" variant="soft" color={needsAttention(room) ? 'warning' : room.status === 'collecting' ? 'success' : 'default'}><span className="status-dot" />{statusLabels[room.status] || room.status}</Chip>
}

export default function Pipeline({ user, onAccount }: { user: StudioUser; onAccount: () => void }) {
  const [rooms, setRooms] = useState<Room[]>([]), [health, setHealth] = useState<Health | null>(null)
  const [loaded, setLoaded] = useState(false), [serviceError, setServiceError] = useState('')
  const viewKey = 'monitor:current-view:' + user.username
  const [view, setView] = useState<View>(() => {
    try {
      const saved = localStorage.getItem(viewKey)
      if (saved === 'overview' || saved === 'attention' || saved === 'archive' || saved === 'room') return saved
    } catch { /* Open the overview when browser storage is unavailable. */ }
    return 'overview'
  })
  useEffect(() => { try { localStorage.setItem(viewKey, view) } catch { /* Keep navigation available without browser storage. */ } }, [viewKey, view])
  const sidebarKey = 'monitor:sidebar-collapsed:' + user.username
  const [sidebarCollapsed, setSidebarCollapsed] = useState(() => { try { const saved = localStorage.getItem(sidebarKey); if (saved !== null) return saved === 'true' } catch { /* Use the viewport default when storage is unavailable. */ } return window.matchMedia('(max-width: 760px)').matches })
  useEffect(() => { try { localStorage.setItem(sidebarKey, String(sidebarCollapsed)) } catch { /* Keep this session's preference. */ } }, [sidebarKey, sidebarCollapsed])
  useEffect(() => () => Toast.toast.clear(), [])
  const closeMobileSidebar = () => { if (window.matchMedia('(max-width: 760px)').matches) setSidebarCollapsed(true) }
  const orderKey = 'monitor:room-order:' + user.username
  const [roomOrder, setRoomOrder] = useState<string[]>(() => { try { const value = JSON.parse(localStorage.getItem(orderKey) || '[]'); return Array.isArray(value) ? value.filter(id => typeof id === 'string') : [] } catch { return [] } })
  const [search, setSearch] = useState(''), [status, setStatus] = useState('all'), [sort, setSort] = useState(roomOrder.length ? 'manual' : 'default')
  const [reorderNotice, setReorderNotice] = useState('')
  const activeKey = 'monitor:active-room:' + user.username
  const [selected, setSelected] = useState<string[]>([]), [active, setActive] = useState(() => { try { return localStorage.getItem(activeKey) || '' } catch { return '' } })
  useEffect(() => { if (active) { try { localStorage.setItem(activeKey, active) } catch { /* Keep the current room in memory. */ } } }, [activeKey, active])
  const [archiveUser, setArchiveUser] = useState<ArchiveUser | null>(null)
  const [pinned, setPinned] = useState<string[]>(() => { try { const value = JSON.parse(localStorage.getItem('monitor:pinned') || '[]'); return Array.isArray(value) ? value.filter(id => typeof id === 'string') : [] } catch { return [] } })
  const [busy, setBusy] = useState(false)
  const [addOpen, setAddOpen] = useState(false), [settingsOpen, setSettingsOpen] = useState(false), [runtimeOpen, setRuntimeOpen] = useState(false)
  const [colorGuideOpen, setColorGuideOpen] = useState(false)
  const [profileOpen, setProfileOpen] = useState(false), [settingsRoomId, setSettingsRoomId] = useState('')
  const [roomInput, setRoomInput] = useState(''), [addError, setAddError] = useState('')
  const [inspectedEvent, setInspectedEvent] = useState<PipelineEvent | null>(null)
  const [filter, setFilter] = useState('all'), [keyword, setKeyword] = useState('')
  const [roomContent, setRoomContent] = useState('messages')
  const [frozen, setFrozen] = useState<PipelineEvent[] | null>(null)
  const listRef = useRef<HTMLDivElement>(null), follow = useRef(true), loading = useRef(false)
  const streams = useRoomStreams(rooms.filter(room => room.enabled || view === 'room' && room.live_id === active).map(room => room.live_id))
  const load = useCallback(async () => {
    if (loading.current) return
    loading.current = true
    try {
      const [roomResult, healthResult] = await Promise.allSettled([request<Room[]>('/rooms'), request<Health>('/health')])
      if (roomResult.status === 'fulfilled') {
        const next = roomResult.value.filter(room => room.source === 'douyin')
        setRooms(next); setActive(previous => next.some(room => room.live_id === previous) ? previous : next[0]?.live_id || '')
        setSelected(previous => previous.filter(id => next.some(room => room.live_id === id && room.enabled))); setServiceError('')
      } else setServiceError('房间状态更新失败，当前显示上次成功获取的数据。正在自动重试。')
      setHealth(healthResult.status === 'fulfilled' ? healthResult.value : null)
    } finally { setLoaded(true); loading.current = false }
  }, [])
  useEffect(() => { void load(); const timer = setInterval(() => void load(), 2500); return () => clearInterval(timer) }, [load])
  useEffect(() => { try { localStorage.setItem('monitor:pinned', JSON.stringify(pinned)) } catch { /* Keep pins in memory if browser storage is unavailable. */ } }, [pinned])
  useEffect(() => { try { localStorage.setItem(orderKey, JSON.stringify(roomOrder)) } catch { /* Preserve this session's order when storage is unavailable. */ } }, [orderKey, roomOrder])
  const room = rooms.find(item => item.live_id === active)
  const settingsRoom = rooms.find(item => item.live_id === settingsRoomId)
  const orderedIds = useMemo(() => completeRoomOrder(roomOrder, rooms.map(item => item.live_id)), [roomOrder, rooms])
  const manualRank = useMemo(() => new Map(orderedIds.map((id, index) => [id, index])), [orderedIds])
  const filtered = useMemo(() => rooms.filter(item => {
    if (!item.enabled) return false
    if (view === 'attention' && !needsAttention(item)) return false
    if (status === 'live' && !isLive(item)) return false
    return `${roomName(item)} ${item.live_id} ${item.metadata?.title || ''}`.toLowerCase().includes(search.trim().toLowerCase())
  }).sort((a, b) => sort === 'manual' ? (manualRank.get(a.live_id)! - manualRank.get(b.live_id)!) : sort === 'online' ? ((isLive(b) && b.enabled ? b.stats.online : 0) - (isLive(a) && a.enabled ? a.stats.online : 0)) : sort === 'chat' ? b.stats.chat - a.stats.chat : Number(pinned.includes(b.live_id)) - Number(pinned.includes(a.live_id)) || Number(needsAttention(b)) - Number(needsAttention(a))), [rooms, view, pinned, status, search, sort, manualRank])
  const reordering = useRoomReorder(filtered.map(item => item.live_id), (source, target) => {
    if (source === target || !filtered.some(item => item.live_id === source) || !filtered.some(item => item.live_id === target)) return
    setRoomOrder(reorderVisibleRooms(orderedIds, filtered.map(item => item.live_id), source, target)); setSort('manual')
    setReorderNotice(`${roomName(rooms.find(item => item.live_id === source)!)} 已移至当前列表第 ${filtered.findIndex(item => item.live_id === target) + 1} 位`)
  })
  const allMessages = useMemo(() => Object.values(streams).flatMap(stream => stream.messages).sort((a, b) => BigInt(a.seq) < BigInt(b.seq) ? -1 : BigInt(a.seq) > BigInt(b.seq) ? 1 : 0), [streams])
  const messages = useMemo(() => (frozen ?? allMessages).filter(event => event.live_id === active && (filter === 'all' || messageKind(event) === filter) && `${event.user_name} ${event.user_id} ${messageContent(event)} ${event.method || ''}`.toLowerCase().includes(keyword.toLowerCase())).slice(-500), [allMessages, frozen, active, filter, keyword])
  useEffect(() => { if (!frozen && follow.current && listRef.current) listRef.current.scrollTop = listRef.current.scrollHeight }, [messages, frozen])
  const attention = rooms.filter(needsAttention), enabled = rooms.filter(item => item.enabled)
  const connectedCount = rooms.filter(item => item.enabled && streams[item.live_id]?.connected).length
  const selectedVisible = filtered.filter(item => selected.includes(item.live_id)).length
  const changeSelection = (id: string, checked: boolean) => setSelected(previous => checked ? [...new Set([...previous, id])] : previous.filter(value => value !== id))
  const togglePin = (id: string) => setPinned(previous => previous.includes(id) ? previous.filter(value => value !== id) : [...previous, id])
  const operate = async (targets: Room[], enable: boolean) => {
    if (busy) return
    const changed = targets.filter(item => item.enabled !== enable)
    if (!changed.length) return
    setBusy(true)
    const failures: string[] = []
    try {
      for (const item of changed) { try { await request(enable ? '/rooms' : `/rooms/${item.live_id}`, { method: enable ? 'POST' : 'DELETE', body: enable ? JSON.stringify({ live_id: item.live_id }) : undefined }) } catch (error) { failures.push(`${roomName(item)}：${(error as Error).message}`) } }
      const succeeded = changed.length - failures.length
      if (succeeded) Toast.toast.success(`已${enable ? '启动' : '移除'} ${succeeded} 个直播间${enable ? '' : '监控'}`, { timeout: 5000 })
      if (failures.length) Toast.toast.danger(`${failures.length} 个直播间操作失败`, { description: failures.join('；'), timeout: 0 })
      await load()
    } finally { setBusy(false) }
  }
  const addRooms = async () => {
    if (busy) return
    const { ids, invalid } = parseRoomInput(roomInput)
    if (!ids.length || invalid.length) { setAddError(invalid.length ? `以下内容无法识别：${invalid.join('、')}` : '请至少输入一个直播间号或链接'); return }
    if (ids.length > 50) { setAddError('单次最多添加 50 个直播间，请分批添加'); return }
    setBusy(true); setAddError('')
    const failures: string[] = [], errors: string[] = []
    try {
      for (const id of ids) {
        if (rooms.some(item => item.live_id === id && item.enabled)) continue
        try { await request('/rooms', { method: 'POST', body: JSON.stringify({ live_id: id }) }) } catch (error) { failures.push(id); errors.push(`${id}：${(error as Error).message}`) }
      }
      await load(); setRoomInput(failures.join('\n'))
      if (failures.length) setAddError(errors.join('；'))
      else { setAddOpen(false); Toast.toast.success('直播间已加入监控', { description: '已有房间保持原采集状态。', timeout: 5000 }); setView('overview'); setStatus('all'); setSearch('') }
    } finally { setBusy(false) }
  }
  const resetFilters = () => { setSearch(''); setStatus('all'); setView('overview') }

  const focusRoom = (id: string) => {
    setActive(id); setFrozen(null); setFilter('all'); setKeyword(''); setRoomContent('messages'); setView('room'); follow.current = true
  }
  const inspectUser = (event: PipelineEvent) => { setArchiveUser({ id: event.user_id, name: event.user_name }); setView('archive') }
  const openSettings = (id: string) => { setSettingsRoomId(id); setProfileOpen(false); setSettingsOpen(true) }
  const serviceReady = !!health?.collector.online && !!health.database && !!health.rabbitmq
  const activeStream = streams[active]
  const roomStates = [...(activeStream?.roomState || [])].sort((a, b) => (a.method || a.type).localeCompare(b.method || b.type))
  const onlineState = roomStates.find(event => event.type === 'online_count')
  const liveOnline = !!room && !serviceError && !!health?.collector.online && room.enabled && room.status === 'collecting' && isLive(room) && !!activeStream?.connected && !!onlineState && Date.now() - onlineState.received_at_ms < 60000
  const viewTitle = view === 'room' ? (room ? roomName(room) : '直播间详情') : view === 'archive' ? '信息汇总' : view === 'overview' ? '直播监控台' : '采集故障'

  return <div className={'console-app ' + (sidebarCollapsed ? 'sidebar-collapsed' : 'sidebar-expanded')}>
    <Toast.Provider placement="top end" className="studio-toast-region" width={360} aria-label="操作通知">{({ toast }) => <Toast toast={toast} variant={toast.content.variant} placement="top end"><Toast.Indicator variant={toast.content.variant} /><Toast.Content><Toast.Title>{toast.content.title}</Toast.Title>{toast.content.description && <Toast.Description>{toast.content.description}</Toast.Description>}</Toast.Content><Toast.CloseButton aria-label="关闭通知" /></Toast>}</Toast.Provider>
    <header className="global-header">
      <Link className="global-brand" href="/" aria-label="直播台首页"><svg width="27" height="27" viewBox="0 0 32 32" aria-hidden="true"><rect x="3" y="10" width="6" height="17" rx="3" fill="currentColor" /><rect x="13" y="4" width="6" height="23" rx="3" fill="currentColor" /><rect x="23" y="8" width="6" height="19" rx="3" fill="currentColor" /></svg><strong>直播台</strong></Link>
      <nav aria-label="顶部菜单"><Button variant="ghost" className={!runtimeOpen ? 'is-active' : ''} onPress={() => { setRuntimeOpen(false); setView('overview'); setStatus('all'); setSearch('') }}>直播工作台</Button><Button variant="ghost" className={runtimeOpen ? 'is-active' : ''} onPress={() => setRuntimeOpen(true)}>服务管理</Button></nav>
      <div className="global-context"><span className={'status-dot ' + (serviceReady ? 'good' : 'warn')} /><span>{serviceReady ? '服务已连接' : '正在检查服务'}</span></div>
    </header>
    <Button variant="ghost" className="sidebar-scrim" aria-label="收起导航侧栏" aria-hidden={sidebarCollapsed} isDisabled={sidebarCollapsed} onPress={() => setSidebarCollapsed(true)} />
    <aside className="studio-rail" id="workspace-sidebar" onKeyDown={event => { if (event.key === 'Escape') closeMobileSidebar() }}>
      <div className="sidebar-section-heading"><span className="sidebar-label">工作空间</span><Tooltip delay={250}><Button variant="ghost" isIconOnly className="sidebar-toggle" aria-label={sidebarCollapsed ? '展开侧边栏' : '收起侧边栏'} aria-expanded={!sidebarCollapsed} aria-controls="workspace-sidebar" onPress={() => setSidebarCollapsed(value => !value)}><MenuFoldOutlined className="sidebar-toggle-icon" /></Button><Tooltip.Content placement="right">{sidebarCollapsed ? '展开侧边栏' : '收起侧边栏'}</Tooltip.Content></Tooltip></div>
      <nav aria-label="主导航">
        {[
          { key: 'overview', name: '监控台', icon: <AppstoreOutlined /> },
          { key: 'archive', name: '信息汇总', icon: <DatabaseOutlined /> },
          { key: 'attention', name: '采集故障', icon: <WarningOutlined /> },
        ].map(item => <Tooltip key={item.key} delay={250}><Button variant="ghost" className={'rail-link ' + (view === item.key || view === 'room' && item.key === 'overview' ? 'is-active' : '')} aria-label={item.name} aria-pressed={view === item.key || view === 'room' && item.key === 'overview'} onPress={() => { setView(item.key as View); if (item.key === 'archive') setArchiveUser(null); setStatus('all'); setSearch(''); closeMobileSidebar() }}>
          {item.icon}<span>{item.name}</span>{item.key === 'attention' && attention.length > 0 && <b className="rail-count">{attention.length}</b>}
        </Button><Tooltip.Content placement="right">{item.name}</Tooltip.Content></Tooltip>)}
      </nav>
      <div className="rail-bottom">
        <div className="sidebar-service"><span className={'status-dot ' + (serviceReady ? 'good' : 'warn')} /><span>{enabled.length} 间已启用监控</span></div>
        <div className="sidebar-user"><Tooltip delay={250}><Button variant="ghost" className="user-profile-button" aria-label="个人信息" onPress={() => { closeMobileSidebar(); onAccount() }}><HeroAvatar className="workspace-avatar"><HeroAvatar.Fallback>{Array.from(user.displayName)[0]}</HeroAvatar.Fallback></HeroAvatar><span><strong>{user.displayName}</strong><small>{user.role}</small></span></Button><Tooltip.Content placement="right">个人信息</Tooltip.Content></Tooltip></div>
      </div>
    </aside>

    <div className="studio-workspace">
      <header className="studio-header">
        <div className="page-title"><h1>{viewTitle}</h1><span className="platform-label">抖音</span></div>
        <div className="header-actions">
          {view === 'room' && <Button variant="secondary" onPress={() => setView('overview')}><ArrowLeftOutlined />返回监控台</Button>}
          <span className="sync-indicator"><i className={'status-dot ' + (connectedCount === enabled.length && enabled.length ? 'good' : 'warn')} />{connectedCount}/{enabled.length} 间消息已连接</span>
          {view !== 'archive' && <Button variant="secondary" isIconOnly aria-label="刷新监控数据" onPress={() => void load()}><ReloadOutlined /></Button>}
          <Button className="add-button" onPress={() => { setAddError(''); setAddOpen(true) }}><PlusOutlined />添加直播间</Button>
        </div>
      </header>


      <div className="console-notices">
        {serviceError && <InlineFeedback>{serviceError}</InlineFeedback>}
        {loaded && !serviceError && !serviceReady && <InlineFeedback action={<Button size="sm" variant="ghost" onPress={() => setRuntimeOpen(true)}>检查服务</Button>}>{!health ? '暂时无法获取服务状态，正在重试。' : '采集或存储服务异常，实时数据可能延迟。'}</InlineFeedback>}
      </div>

      {view === 'archive' ? <MessageArchive rooms={rooms} initialUser={archiveUser} onRoom={focusRoom} /> : <main className={'console-grid ' + (view === 'room' ? 'room-detail-view' : 'monitor-overview')}>
        {view !== 'room' && <section className="rooms-panel" aria-label="直播间监控">
          <div className="panel-heading">
            <div><h2>直播间 <span className="count-label">{filtered.length}</span></h2><p>{view === 'attention' ? '连接失败、重连中或消息积压' : '管理监控中的直播间，点击卡片查看数据'}</p></div>
          </div>
          <div className="rooms-tools">
            <StudioSearch label="搜索直播间" placeholder="搜索主播、房间号" value={search} onChange={setSearch} clearLabel="清空房间搜索" />
            <div className="room-filter-row">
              <StudioFilters label="房间状态筛选" className="filter-tabs" value={status} onChange={setStatus} options={[{ id: 'all', label: '全部' }, { id: 'live', label: '直播中' }]} />
              <StudioSelect compact label="直播间排序" className="room-sort-select" value={sort} onChange={setSort} options={[{ id: 'default', label: '默认排序' }, { id: 'manual', label: '手动排序' }, { id: 'online', label: '在线优先' }, { id: 'chat', label: '弹幕优先' }]} />
            </div>
            <div className={'selection-bar ' + (selected.length ? 'has-selection' : '')}>
              <div><Check label="选择当前筛选的全部直播间" selected={filtered.length > 0 && selectedVisible === filtered.length} mixed={selectedVisible > 0 && selectedVisible < filtered.length} onChange={checked => setSelected(previous => checked ? [...new Set([...previous, ...filtered.map(item => item.live_id)])] : previous.filter(id => !filtered.some(item => item.live_id === id)))} /><span>{selected.length ? '已选 ' + selected.length + ' 间' : '批量选择'}</span></div>
              <div><Button size="sm" variant="ghost" isDisabled={busy || !selected.length} onPress={() => void operate(rooms.filter(item => selected.includes(item.live_id)), false)} aria-label="批量移除监控"><DeleteOutlined />移除监控</Button>{selected.length > 0 && <Button variant="ghost" size="sm" isIconOnly aria-label="取消选择" onPress={() => setSelected([])}><CloseOutlined /></Button>}</div>
            </div>
            {selected.length > selectedVisible && <p className="selection-note">含 {selected.length - selectedVisible} 间被筛选隐藏的已选房间</p>}
          </div>

          <p id="room-reorder-help" className="sr-only">拖动把手调整房间顺序，也可聚焦把手后使用上下方向键移动，Home 或 End 移到首尾。顺序保存在当前浏览器。</p>
          <span className="sr-only" role="status">{reorderNotice}</span>
          <div className="room-list" ref={reordering.listRef}>
            {!loaded && <div className="empty-state"><Spinner size="md" aria-label="正在加载直播间" /><strong>正在加载直播间</strong></div>}
            {filtered.map(item => <Card key={item.live_id} data-room-id={item.live_id} className={'channel-card ' + (pinned.includes(item.live_id) ? 'is-pinned ' : '') + (needsAttention(item) ? 'needs-attention ' : '') + (reordering.drag?.source === item.live_id ? 'is-dragging ' : '') + (reordering.drag?.target === item.live_id && reordering.drag.source !== item.live_id ? reordering.drag.after ? 'drop-after' : 'drop-before' : '')}>
              <Button variant="ghost" className="channel-select-hitarea" aria-label={'查看 ' + roomName(item) + ' 的消息'} onPress={() => focusRoom(item.live_id)}><span className="sr-only">查看 {roomName(item)} 的数据</span></Button>
              <div className="channel-card-top"><Check label={'选择 ' + roomName(item)} selected={selected.includes(item.live_id)} onChange={checked => changeSelection(item.live_id, checked)} /><Status room={item} /><Button isIconOnly size="sm" variant="ghost" className={'pin-button ' + (pinned.includes(item.live_id) ? 'is-pinned' : '')} aria-label={(pinned.includes(item.live_id) ? '取消重点关注 ' : '重点关注 ') + roomName(item)} aria-pressed={pinned.includes(item.live_id)} onPress={() => togglePin(item.live_id)}>{pinned.includes(item.live_id) ? <PushpinFilled /> : <PushpinOutlined />}</Button><RoomDragHandle name={roomName(item)} handlers={reordering.handleProps(item.live_id)} /></div>
              <div className="channel-profile"><Avatar key={item.metadata?.anchor.avatar_url || item.live_id} room={item} /><span><strong>{roomName(item)}</strong><small>{item.live_id}</small></span></div>
              <div className="channel-metrics">
                <div><span>在线</span><strong>{!serviceError && health?.collector.online && item.enabled && item.status === 'collecting' && isLive(item) ? number(item.stats.online) : '—'}</strong></div>
                <div><span>弹幕</span><strong>{number(item.stats.chat)}</strong></div>
                <div><span>{item.stats.gift_quantity == null ? '礼物组' : '礼物'}</span><strong>{number(item.stats.gift_quantity ?? item.stats.gift)}</strong></div>
              </div>
              {needsAttention(item) && <div className="channel-last-message"><WarningOutlined /><span>{item.detail || '连接异常，请检查采集状态'}</span></div>}
              <div className="channel-footer"><span><i className={'status-dot ' + (streams[item.live_id]?.connected ? 'good' : 'warn')} />{streams[item.live_id]?.connected ? '已同步' : '正在同步'}</span><Button variant="ghost" size="sm" onPress={() => openSettings(item.live_id)} aria-label={'配置 ' + roomName(item)}><SettingOutlined />配置</Button><Button variant="ghost" size="sm" className="remove-monitor" aria-label={'移除监控 ' + roomName(item)} isDisabled={busy} onPress={() => void operate([item], false)}><DeleteOutlined />移除监控</Button></div>
            </Card>)}
            {loaded && !filtered.length && <EmptyState className="empty-state"><SearchOutlined /><strong>{!enabled.length ? '开始监控第一个直播间' : view === 'attention' ? '暂无采集故障' : '没有匹配的直播间'}</strong><p>{!enabled.length ? '添加抖音直播间号或链接，接收实时互动。' : view === 'attention' ? '此处显示连接失败、正在重连或消息积压的房间；等待开播属于正常状态。' : '调整筛选条件，或返回全部直播间。'}</p><Button size="sm" variant="secondary" onPress={() => enabled.length ? resetFilters() : setAddOpen(true)}>{enabled.length ? '查看全部直播间' : '添加直播间'}</Button></EmptyState>}
            {loaded && enabled.length > 0 && <Button variant="ghost" className="add-channel" onPress={() => setAddOpen(true)}><PlusOutlined />添加更多直播间</Button>}
          </div>
        </section>}

        {view === 'room' && <section className={'conversation-panel' + (roomContent === 'rankings' ? ' has-room-rankings' : '')} aria-label="直播间实时互动">
          <div className="panel-heading">
            <div><h2><span className={'feed-indicator ' + (frozen ? 'is-paused' : '')} />{roomContent === 'messages' ? <>实时行为 <span className="count-label">{messages.length}</span></> : '本房间排行榜'}</h2><p>{room ? '正在查看 ' + roomName(room) : '选择直播间查看互动'}</p></div>
            {roomContent === 'messages' && <div className="feed-heading-actions"><Button size="sm" variant="ghost" className="color-guide-button" onPress={() => setColorGuideOpen(true)}>配色说明</Button><Button size="sm" variant={frozen ? 'secondary' : 'ghost'} className="pause-button" onPress={() => { setFrozen(previous => previous ? null : allMessages); follow.current = true }}>{frozen ? <PlayCircleOutlined /> : <PauseOutlined />}{frozen ? '继续展示' : '暂停展示'}</Button></div>}
          </div>
          {room && <div className="room-observation">
            <div className="active-room-summary"><Avatar key={room.live_id} room={room} /><div><span>当前选中</span><strong>{roomName(room)}</strong></div><Status room={room} />
              <dl className="room-live-metrics" aria-label="直播间观测数据">
                <div title={onlineState ? '最近观测 ' + new Date(onlineState.received_at_ms).toLocaleString('zh-CN') : '尚未收到在线人数消息'}><dt>{liveOnline ? '在线人数' : '上次在线'}</dt><dd>{onlineState ? number(onlineState.online_count ?? room.stats.online) : '—'}<small>人</small></dd></div>
                <div><dt>累计弹幕</dt><dd>{number(room.stats.chat)}</dd></div>
                <div><dt>累计礼物</dt><dd>{number(room.stats.gift_quantity ?? room.stats.gift)}</dd></div>
                <div><dt>累计进场</dt><dd>{number(room.stats.enter)}</dd></div>
                <div><dt>累计点赞</dt><dd>{number(room.stats.like)}</dd></div>
              </dl>
              <Button size="sm" variant="secondary" onPress={() => setProfileOpen(true)}><UserOutlined />主播信息</Button>
            </div>
          </div>}
          {room && <StudioFilters label="直播间内容" className="room-content-tabs" value={roomContent} onChange={setRoomContent} options={[{ id: 'messages', label: '实时消息' }, { id: 'rankings', label: '本房间排行榜' }]} />}
          {roomContent === 'rankings' && room ? <RoomRankings key={room.live_id} room={room.live_id} roomName={roomName(room)} /> : <>
          <div className="conversation-tools">
            <StudioSearch label="搜索消息" placeholder="搜索内容、昵称或 UID" value={keyword} onChange={setKeyword} clearLabel="清空消息搜索" />
          </div>
          <StudioFilters label="实时消息类型" className="message-filters expanded-message-filters" value={filter} onChange={value => { setFilter(value); follow.current = true }} options={[{ type: 'all', label: '全部行为' }, ...feedMessageTypes].map(item => ({ id: item.type, label: item.label }))} />
          <div className="message-list" ref={listRef} aria-label="实时消息列表" onScroll={event => { const node = event.currentTarget; follow.current = node.scrollHeight - node.scrollTop - node.clientHeight < 60 }}>
            {messages.length === 0 && <EmptyState className="empty-state message-empty"><div className="empty-conversation"><MessageOutlined /></div><strong>{keyword || filter !== 'all' ? '没有匹配的互动' : '等待直播间的下一条消息'}</strong><p>{keyword || filter !== 'all' ? '尝试其他关键词，或切换消息类型。' : '房间连接成功后，弹幕、礼物与观众互动将在这里同步。'}</p>{(keyword || filter !== 'all') && <Button variant="secondary" size="sm" onPress={() => { setKeyword(''); setFilter('all') }}>清除消息筛选</Button>}</EmptyState>}
            {messages.map(event => <div className="sourced-message" data-message-type={messageKind(event)} key={messageDisplayKey(event)}><PipelineMessage event={event} onInspectUser={inspectUser} onInspectEvent={setInspectedEvent} /></div>)}
          </div>
          <div className="conversation-footer"><span>{frozen ? '后台仍在接收所有房间消息' : '最多展示最近 500 条行为 · 房间状态在上方更新'}</span><Button size="sm" variant="ghost" onPress={() => { setFrozen(null); follow.current = true; listRef.current?.scrollTo({ top: listRef.current.scrollHeight }) }}>回到最新 <span aria-hidden="true">↓</span></Button></div>
          </>}
        </section>}


      </main>}
    </div>

    <MessageDetails event={inspectedEvent} onClose={() => setInspectedEvent(null)} />
    <Dialog open={colorGuideOpen} onChange={setColorGuideOpen} title="配色说明"><div className="color-guide">
      <section><h3>消息类型</h3><div className="color-guide-swatches">{feedMessageTypes.map(item => <Chip key={item.type} className="message-chip" data-tone={item.tone}>{item.label}</Chip>)}</div><p>左侧色条和类型标签使用相同配色。房间来源另按房间号保持固定颜色。</p></section>
      <section><h3>财富等级</h3><div className="color-guide-swatches">{wealthBands.map(band => <Chip key={band.min} className="message-chip" data-tone={band.tone}>Lv.{band.label}</Chip>)}</div></section>
      <section><h3>粉丝团等级</h3><div className="color-guide-swatches">{fansBands.map(band => <Chip key={band.min} className="message-chip" data-tone={band.tone}>Lv.{band.label}</Chip>)}</div><p>有灯牌资料时显示“名字-等级”；未提供名称时显示“粉丝团-等级”。星守护的灯牌也按此展示。等级颜色表示粉丝团等级，不表示会员状态。</p></section>
      <section><h3>礼物</h3><p>按礼物名称保持固定配色，相同礼物在不同房间中颜色一致。颜色仅用于辨认礼物，不表示礼物价值。</p></section>
    </div></Dialog>
    <Dialog open={profileOpen && !!room} onChange={setProfileOpen} title="主播信息"><div className="anchor-info">
          {room ? <div className="inspector-content">
            <div className="inspector-profile"><Avatar key={room.metadata?.anchor.avatar_url || room.live_id} room={room} /><h3>{roomName(room)}</h3><Link href={'https://live.douyin.com/' + room.live_id} target="_blank" rel="noreferrer">打开抖音直播间 <span aria-hidden="true">↗</span></Link></div>
            <p className="inspector-title">{room.metadata?.title || '等待获取直播标题'}</p>
            <div className="inspector-status"><Status room={room} /><span>{isStale(room) ? '开播状态待确认' : isLive(room) ? '正在直播' : '未开播'}</span></div>
            <div className="inspector-section"><h4>房间数据</h4><dl className="detail-stats"><div><dt>直播间号</dt><dd>{room.live_id}</dd></div><div><dt>累计进场</dt><dd>{number(room.stats.enter)}</dd></div><div><dt>累计点赞</dt><dd>{number(room.stats.like)}</dd></div><div><dt>上次观测在线</dt><dd>{number(room.stats.online)} 人</dd></div>{room.metadata?.anchor.follower_count != null && <div><dt>主播粉丝</dt><dd>{number(room.metadata.anchor.follower_count)}</dd></div>}</dl></div>
            <div className="inspector-section"><h4>采集状态</h4><div className="connection-detail"><span className={'status-dot ' + (activeStream?.connected ? 'good' : 'warn')} /><span>{activeStream?.connected ? '消息连接正常' : '消息连接正在恢复'}</span></div><dl className="detail-stats"><div><dt>最近资料检查</dt><dd>{room.metadata?.checked_at_ms ? time(room.metadata.checked_at_ms) : '等待检查'}</dd></div>{room.metadata?.live_started_at_ms ? <div><dt>最近开播</dt><dd>{new Date(room.metadata.live_started_at_ms).toLocaleString('zh-CN')}</dd></div> : null}</dl>{room.detail && <p className={'detail-note ' + (needsAttention(room) ? 'warning' : '')}>{room.detail}</p>}{activeStream?.error && <p className="detail-note warning" role="alert">{activeStream.error}</p>}</div>
            <div className="inspector-section"><h4>采集身份</h4><CollectorIdentity status={health?.collector.room_auth?.[active]?.auth_status} /><Button fullWidth variant="secondary" className="identity-config-button" onPress={() => openSettings(room.live_id)}><SettingOutlined />配置此房间 Cookie</Button></div>
            <Button fullWidth className="room-control-button" variant="secondary" isDisabled={busy} onPress={() => void operate([room], !room.enabled)}>{room.enabled ? <PauseOutlined /> : <PlayCircleOutlined />}{room.enabled ? '移除此房间监控' : '启动此房间采集'}</Button>
          </div> : <EmptyState className="empty-state"><UserOutlined /><strong>还未选择直播间</strong><p>添加房间后，可在这里查看主播资料和采集状态。</p></EmptyState>}
    </div></Dialog>
    <Dialog open={addOpen} onChange={value => { if (!busy) setAddOpen(value) }} title="添加直播间" footer={<><Button variant="secondary" isDisabled={busy} onPress={() => setAddOpen(false)}>取消</Button><Button isDisabled={busy || !roomInput.trim()} onPress={() => void addRooms()}><PlusOutlined />{busy ? '正在添加…' : '添加并开始监控'}</Button></>}>
      <div className="dialog-intro"><div className="dialog-symbol"><PlusOutlined /></div><p>将需要关注的直播间加入工作台。<br />支持批量添加，每行输入一个房间号或链接。</p></div>
      <Label className="field-label" htmlFor="room-addresses">直播间地址</Label><TextArea id="room-addresses" rows={6} value={roomInput} onChange={event => setRoomInput(event.target.value)} placeholder={'输入房间号，例如 123456789\n或 https://live.douyin.com/123456789'} disabled={busy} />
      <div className="dialog-hint"><CheckCircleOutlined /><span>每次最多 50 间，已监控房间自动跳过，已移除房间重新启用。新房间使用独立游客身份，可在配置中添加 Cookie。</span></div>{addError && <InlineFeedback>{addError}</InlineFeedback>}
    </Dialog>
    <Dialog open={settingsOpen && !!settingsRoom} onChange={setSettingsOpen} title={settingsRoom ? roomName(settingsRoom) + ' · 采集配置' : '采集配置'}>{settingsRoom && <CookieSettings liveId={settingsRoom.live_id} onSaved={() => void load()} />}</Dialog>
    <Dialog open={runtimeOpen} onChange={setRuntimeOpen} title="服务运行状态">
      <p className="dialog-description">查看采集、存储及推送服务的当前状态。</p>
      <div className="health-grid">{[['采集服务', health?.collector.online], [health?.transport === 'local' ? '本地投递' : '消息队列', health?.rabbitmq], ['数据存储', health?.database], [health?.cache_backend === 'sqlite' ? '本地统计' : '统计缓存', health?.redis]].map(([name, ok]) => <div key={String(name)}><DatabaseOutlined /><span>{name}</span><Chip size="sm" color={ok ? 'success' : 'warning'}>{ok ? '在线' : health ? name === '统计缓存' ? '回源模式' : '不可用' : '未知'}</Chip></div>)}</div>
      <dl className="detail-stats"><div><dt>已处理原始帧</dt><dd>{health ? number(health.frames) : '—'}</dd></div><div><dt>已持久化事件</dt><dd>{health ? number(health.events) : '—'}</dd></div><div><dt>待处理隔离</dt><dd>{health ? number(health.quarantine) : '—'}</dd></div><div><dt>本地待确认日志</dt><dd>{health ? ((health.collector.spool_bytes || 0) / 1024).toFixed(1) + ' KB' : '—'}</dd></div><div><dt>推送连接</dt><dd>{connectedCount} / {enabled.length} 间</dd></div></dl>
      <Button variant="secondary" onPress={() => void load()}><ReloadOutlined />刷新状态</Button>
    </Dialog>
  </div>
}
