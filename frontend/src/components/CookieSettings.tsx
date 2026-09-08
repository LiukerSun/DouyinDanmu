import { useEffect, useRef, useState } from 'react'
import { Accordion, Button, Form, Input, Label, Link } from '@heroui/react'
import { KeyOutlined, EyeInvisibleOutlined, EyeOutlined, CopyOutlined, SaveOutlined, DeleteOutlined } from '@ant-design/icons'
import type { CollectorAuthStatus } from './CollectorIdentity'
import './CookieSettings.css'
import InlineFeedback from './InlineFeedback'

type Settings = { live_id: string; auth_status: CollectorAuthStatus; reconnecting_rooms?: number; reconnect_pending?: boolean }

async function settingsRequest(liveId: string, method = 'GET', cookie?: string): Promise<Settings> {
  const response = await fetch(`/api/settings/rooms/${encodeURIComponent(liveId)}/douyin-cookie`, {
    method, cache: 'no-store',
    headers: { 'Content-Type': 'application/json', 'X-Settings-Request': '1' },
    body: method === 'GET' ? undefined : JSON.stringify(method === 'PUT' ? { cookie } : {}),
  })
  let data
  try { data = await response.json() }
  catch { throw new Error('配置服务暂不可用，请稍后重试') }
  if (!response.ok) throw new Error(data.error || 'Cookie 配置失败')
  if (data.live_id !== liveId) throw new Error('配置响应与当前直播间不匹配，请重试')
  return data
}

type Props = { onSaved: () => void; liveId: string }

export default function CookieSettings(props: Props) {
  // Switching rooms discards the entire credential draft and all local UI state.
  return <RoomCookieSettings key={props.liveId} {...props} />
}

function RoomCookieSettings({ onSaved, liveId }: Props) {
  const [expanded, setExpanded] = useState(false)
  const [cookie, setCookie] = useState('')
  const [visible, setVisible] = useState(false)
  const [busy, setBusy] = useState(false)
  const [status, setStatus] = useState<CollectorAuthStatus>()
  const [error, setError] = useState('')
  const [message, setMessage] = useState('')
  const mounted = useRef(false)
  const operation = useRef(0)
  const refresh = async () => {
    const current = ++operation.current
    try {
      const data = await settingsRequest(liveId)
      if (mounted.current && operation.current === current) { setStatus(data.auth_status); setError('') }
    } catch { if (mounted.current && operation.current === current) setError('暂时无法读取本直播间 Cookie 配置，请重新展开后重试') }
  }
  useEffect(() => {
    mounted.current = true
    void refresh()
    return () => { mounted.current = false; operation.current++ }
  }, [])
  const toggle = () => {
    setExpanded(!expanded); setCookie(''); setVisible(false); setMessage('')
    if (!expanded) void refresh()
  }
  const paste = async () => {
    setError(''); setMessage('')
    try {
      const text = await navigator.clipboard.readText()
      if (!mounted.current) return
      if (text.length > 32768) throw new Error('too long')
      setCookie(text); setVisible(false)
      if (!text.trim()) setError('剪贴板为空，请先复制抖音请求里的 Cookie')
    } catch { if (mounted.current) setError('无法读取剪贴板，请点输入框后按 Ctrl+V 粘贴') }
  }
  const save = async (clear = false) => {
    ++operation.current
    setBusy(true); setError(''); setMessage('')
    try {
      const data = await settingsRequest(liveId, clear ? 'DELETE' : 'PUT', clear ? undefined : cookie)
      if (!mounted.current) return
      setStatus(data.auth_status); setCookie(''); setVisible(false)
      setMessage(data.reconnect_pending ? '本直播间配置已保存，请通过停止 / 启动按钮重新连接。' :
        clear ? `本直播间已切换为游客采集${data.reconnecting_rooms ? '，正在重连' : '，下次启动时生效'}。` :
          `本直播间 Cookie 已保存${data.reconnecting_rooms ? '，正在重连' : '，下次启动时生效'}。登录是否有效仍以采集结果为准。`)
      onSaved()
    } catch (err) { if (mounted.current) setError(err instanceof Error ? err.message : '保存失败，请稍后重试') }
    finally { if (mounted.current) setBusy(false) }
  }
  const label = status === 'configured' ? '已配置' : status === 'anonymous' ? '游客模式' : status === 'configuration_error' ? '配置异常' : '读取中'
  return <section className="cookie-settings" aria-label={`直播间 ${liveId} Cookie 配置`}>
    <div className="cookie-settings-heading"><div><KeyOutlined /><h3>采集 Cookie</h3><span>{label}</span></div><Button type="button" variant="ghost" size="sm" className="text-button" aria-expanded={expanded} aria-controls="cookie-settings-form" isDisabled={busy} onPress={toggle}>{expanded ? '收起配置' : '配置 Cookie'}</Button></div>
    <p className="cookie-room-scope">直播间 {liveId} · 独立配置，保存和清除仅对本直播间生效</p>
    {expanded && <Form validationBehavior="native" id="cookie-settings-form" onSubmit={event => { event.preventDefault(); void save() }}>
      <p>粘贴本直播间要使用的账号 Cookie。每个直播间可使用不同账号；未配置时使用独立的游客身份，礼物是否下发以实际采集为准。</p>
      <Label htmlFor="douyin-cookie-input">Cookie 请求头</Label>
      <div className="cookie-input-row"><Input id="douyin-cookie-input" type={visible ? 'text' : 'password'} value={cookie} onChange={event => { setCookie(event.target.value); setMessage('') }} placeholder={status === 'configured' ? '已保存的内容不会回显；粘贴新的 Cookie 可替换' : '粘贴完整 Cookie 值，支持 Cookie: 前缀'} maxLength={32768} autoComplete="off" spellCheck={false} disabled={busy} /><Button type="button" variant="ghost" isIconOnly size="sm" className="cookie-visibility" aria-label={visible ? '隐藏 Cookie' : '显示 Cookie'} onPress={() => setVisible(!visible)} isDisabled={busy}>{visible ? <EyeInvisibleOutlined /> : <EyeOutlined />}</Button></div>
      <div className="cookie-settings-actions"><Button type="button" variant="secondary" className="cookie-paste-button" isDisabled={busy} onPress={paste}><CopyOutlined />从剪贴板粘贴</Button><Button type="submit" className="cookie-save-button" isDisabled={busy || !cookie.trim()}><SaveOutlined />{busy ? '正在应用…' : '保存并重连'}</Button><Button type="button" variant="ghost" className="cookie-clear-button" isDisabled={busy || status === 'anonymous' || !status} onPress={() => void save(true)}><DeleteOutlined />清除并使用游客</Button></div>
      <Accordion className="cookie-help"><Accordion.Item><Accordion.Heading><Accordion.Trigger>如何获取 Cookie？<Accordion.Indicator /></Accordion.Trigger></Accordion.Heading><Accordion.Panel><Accordion.Body><ol><li>在浏览器打开<Link href={`https://live.douyin.com/${liveId}`} target="_blank" rel="noreferrer">抖音直播间</Link>并登录需要用于此房间的账号。</li><li>按 F12 打开开发者工具，选择 Network（网络），刷新页面。</li><li>选择发往 live.douyin.com 的请求，在 Request Headers（请求标头）里复制 Cookie 的完整值。使用不同账号时，可通过不同浏览器个人资料分别登录。</li></ol></Accordion.Body></Accordion.Panel></Accordion.Item></Accordion>
      <p className="cookie-storage-note">仅保存在本机采集服务中，保存后输入框自动清空。无需把 Cookie 发到聊天里。</p>
    </Form>}
    {error && <InlineFeedback>{error}</InlineFeedback>}
    {message && <InlineFeedback success>{message}</InlineFeedback>}
  </section>
}
