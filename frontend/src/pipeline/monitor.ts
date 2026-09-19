import type { CollectorAuthStatus } from '../components/CollectorIdentity'

export type Stats = { chat: number; gift: number; gift_quantity?: number; enter: number; like: number; online: number; total: number }
export type Room = {
  live_id: string; source: string; title: string; enabled: boolean; status: string; detail: string; stats: Stats; metadata_stale: boolean
  metadata: null | {
    room_id: string; title: string; live_status: 'live' | 'offline' | 'unknown'; checked_at_ms: number; live_started_at_ms: number | null
    anchor: { nickname: string; avatar_url: string; display_id: string; signature: string; follower_count?: number | null }
  }
}
export type Health = { frames: number; events: number; quarantine: number; rabbitmq: boolean; redis: boolean; database: boolean; transport?: 'local'; cache_backend?: 'sqlite'; collector: { online: boolean; spool_bytes?: number; spool_quarantine?: { files: number; bytes: number }; room_auth?: Record<string, { auth_status: CollectorAuthStatus }> } }
export const statusLabels: Record<string, string> = { connecting: '连接中', collecting: '采集中', reconnecting: '重连中', waiting_live: '等待开播', stopped: '已停止', failed: '连接失败', backpressured: '缓冲已满' }
export const formatNumber = (value: number) => value.toLocaleString('zh-CN')
export const roomName = (room: Room) => room.metadata?.anchor.nickname || room.live_id
export const isStale = (room: Room) => !room.metadata || room.metadata_stale || Date.now() - room.metadata.checked_at_ms > 180000
export const isLive = (room: Room) => !isStale(room) && room.metadata?.live_status === 'live'
export const needsAttention = (room: Room) => room.enabled && ['failed', 'backpressured', 'reconnecting'].includes(room.status)
export async function request<T>(path: string, options?: RequestInit): Promise<T> {
  const response = await fetch('/api' + path, { ...options, signal: options?.signal ?? AbortSignal.timeout(15000), headers: { 'Content-Type': 'application/json', ...options?.headers } })
  if (response.status === 401) window.dispatchEvent(new Event('studio:unauthorized'))
  let data
  try { data = await response.json() } catch { throw new Error('服务暂不可用，请稍后重试') }
  if (!response.ok) throw new Error(data.error || '请求失败')
  return data
}
export function parseRoomInput(input: string) {
  const tokens = input.trim().split(/[\s,，;；]+/).filter(Boolean)
  const ids: string[] = [], invalid: string[] = []
  for (const token of tokens) {
    let id = /^\d{1,32}$/.test(token) ? token : ''
    if (!id) {
      try { const url = new URL(token.startsWith('live.douyin.com/') ? 'https://' + token : token); if (['http:', 'https:'].includes(url.protocol) && url.hostname === 'live.douyin.com') id = url.pathname.match(/^\/(\d{1,32})\/?$/)?.[1] || '' } catch { /* Invalid input is reported below. */ }
    }
    if (id) { if (!ids.includes(id)) ids.push(id) } else invalid.push(token)
  }
  return { ids, invalid }
}
