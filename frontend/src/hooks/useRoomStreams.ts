import { useEffect, useRef, useState } from 'react'
import { RoomStreams, type RoomStream } from '../pipeline/room-streams'
import { request } from '../pipeline/monitor'

export function useRoomStreams(ids: string[]) {
  const [streams, setStreams] = useState<Record<string, RoomStream>>({})
  const manager = useRef<RoomStreams | null>(null)
  const key = JSON.stringify([...ids].sort())
  useEffect(() => {
    const instance = new RoomStreams({ snapshot: (id, signal) => request(`/rooms/${id}/snapshot`, { signal }), socket: () => new WebSocket(`${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws`), change: setStreams })
    manager.current = instance
    return () => { instance.dispose(); manager.current = null }
  }, [])
  useEffect(() => { manager.current?.sync(JSON.parse(key)) }, [key])
  return streams
}
