// Invalidate only the requested room. Old asynchronous callbacks already check
// session.active; the reconcile loop will restart it only if it is still enabled.
function reconnectRoom(sessions, liveId, onReconnect) {
  const session = sessions.get(liveId);
  if (!session?.active || session.source !== 'douyin') return 0;
  onReconnect(session);
  session.active = false;
  try { session.ws?.terminate(); } catch {}
  sessions.delete(liveId);
  return 1;
}

module.exports = { reconnectRoom };
