export function completeRoomOrder(saved: string[], roomIds: string[]): string[] {
  const known = new Set(roomIds)
  return [...new Set([...saved.filter(id => known.has(id)), ...roomIds])]
}

// Reordering a filtered list leaves hidden rooms in their existing slots.
export function reorderVisibleRooms(order: string[], visible: string[], source: string, target: string): string[] {
  const from = visible.indexOf(source), to = visible.indexOf(target)
  if (from < 0 || to < 0 || from === to) return order
  const moved = [...visible]
  moved.splice(from, 1); moved.splice(to, 0, source)
  const visibleSet = new Set(visible)
  let index = 0
  return order.map(id => visibleSet.has(id) ? moved[index++] : id)
}
