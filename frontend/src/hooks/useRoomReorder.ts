import { useEffect, useRef, useState, type PointerEvent, type KeyboardEvent } from 'react'

type Drag = { source: string; target: string; after: boolean }
export function useRoomReorder(ids: string[], onMove: (source: string, target: string) => void) {
  const [drag, setDrag] = useState<Drag | null>(null)
  const current = useRef<Drag | null>(null), frame = useRef(0)
  const position = useRef({ x: 0, y: 0 })
  const listRef = useRef<HTMLDivElement>(null)
  const latest = useRef({ ids, onMove }); latest.current = { ids, onMove }
  const end = () => { cancelAnimationFrame(frame.current); current.current = null; setDrag(null) }
  useEffect(() => () => cancelAnimationFrame(frame.current), [])
  const update = () => {
    if (!current.current || !listRef.current) return
    const list = listRef.current, bounds = list.getBoundingClientRect(), { x, y } = position.current
    if (x >= bounds.left && x <= bounds.right) {
      if (y < bounds.top + 36) list.scrollTop -= 7
      else if (y > bounds.bottom - 36) list.scrollTop += 7
    }
    const element = document.elementFromPoint(x, y)?.closest<HTMLElement>('[data-room-id]')
    const target = element && list.contains(element) ? element.dataset.roomId : undefined
    if (target && latest.current.ids.includes(target) && target !== current.current.target) {
      current.current = { source: current.current.source, target, after: latest.current.ids.indexOf(current.current.source) < latest.current.ids.indexOf(target) }
      setDrag(current.current)
    }
    frame.current = requestAnimationFrame(update)
  }
  const handleProps = (id: string) => ({
    onPointerDown: (event: PointerEvent<HTMLElement>) => {
      if (event.button !== 0 || !event.isPrimary || ids.length < 2) return
      event.preventDefault(); event.stopPropagation(); (event.currentTarget.querySelector<HTMLButtonElement>('button') || event.currentTarget).focus(); event.currentTarget.setPointerCapture(event.pointerId)
      position.current = { x: event.clientX, y: event.clientY }
      current.current = { source: id, target: id, after: false }; setDrag(current.current)
      frame.current = requestAnimationFrame(update)
    },
    onPointerMove: (event: PointerEvent<HTMLElement>) => { if (current.current) position.current = { x: event.clientX, y: event.clientY } },
    onPointerUp: (event: PointerEvent<HTMLElement>) => {
      const value = current.current
      const bounds = listRef.current?.getBoundingClientRect()
      if (value && bounds && event.clientX >= bounds.left && event.clientX <= bounds.right && event.clientY >= bounds.top && event.clientY <= bounds.bottom) latest.current.onMove(value.source, value.target)
      end()
      if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId)
    },
    onPointerCancel: end,
    onLostPointerCapture: end,
    onKeyDown: (event: KeyboardEvent<HTMLElement>) => {
      if (event.key === 'Escape') { end(); return }
      const index = ids.indexOf(id)
      const target = event.key === 'ArrowUp' ? ids[index - 1] : event.key === 'ArrowDown' ? ids[index + 1] : event.key === 'Home' ? ids[0] : event.key === 'End' ? ids[ids.length - 1] : null
      if (!['ArrowUp', 'ArrowDown', 'Home', 'End'].includes(event.key)) return
      event.preventDefault(); event.stopPropagation()
      if (target) onMove(id, target)
    },
  })
  return { drag, listRef, handleProps }
}
