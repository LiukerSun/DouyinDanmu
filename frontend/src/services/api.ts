import { Room, RoomStats, Message, AppConfig } from '../types'

const BASE_URL = '/api'

async function request<T>(url: string, options?: RequestInit): Promise<T> {
  const res = await fetch(BASE_URL + url, {
    headers: { 'Content-Type': 'application/json' },
    ...options,
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: res.statusText }))
    throw new Error(err.error || 'Request failed')
  }
  return res.json()
}

export const api = {
  // 房间管理
  getRooms: () => request<Room[]>('/rooms'),
  addRoom: (live_id: string) => request<Room>('/rooms', {
    method: 'POST',
    body: JSON.stringify({ live_id }),
  }),
  removeRoom: (roomId: string) => request<void>(`/rooms/${roomId}`, { method: 'DELETE' }),
  getRoomStats: (roomId: string) => request<RoomStats>(`/rooms/${roomId}/stats`),
  getRoomMessages: (roomId: string, params?: {
    type?: string; keyword?: string; offset?: number; limit?: number
  }) => {
    const query = new URLSearchParams()
    if (params?.type) query.set('type', params.type)
    if (params?.keyword) query.set('keyword', params.keyword)
    if (params?.offset) query.set('offset', String(params.offset))
    if (params?.limit) query.set('limit', String(params.limit))
    return request<{ room_id: string; messages: Message[]; total: number }>(
      `/rooms/${roomId}/messages?${query}`
    )
  },

  // 配置
  getConfig: () => request<AppConfig>('/config'),
  updateConfig: (config: Partial<AppConfig>) => request<AppConfig>('/config', {
    method: 'PUT',
    body: JSON.stringify(config),
  }),

  // 健康检查
  health: () => request<{ status: string }>('/health'),
}
