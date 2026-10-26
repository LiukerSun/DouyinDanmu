import { useCallback, useEffect, useRef, useState } from 'react'

export type UpdateStatus = {
  current_version: string
  latest_version: string | null
  update_available: boolean
  can_install: boolean
  release_url: string
  release_notes: string | null
  published_at: string | null
  checked_at: number | null
  checking: boolean
  error: string | null
  install: {
    phase: 'idle' | 'downloading' | 'verifying' | 'staging' | 'restarting' | 'error'
    downloaded_bytes: number
    total_bytes: number
    error: string | null
  }
}

export const isInstalling = (status: UpdateStatus | null) => !!status && !['idle', 'error'].includes(status.install.phase)

// Update requests are independent of room polling. During a restart their
// connection may disappear; retain progress instead of clearing the workspace.
export function useUpdates(enabled: boolean) {
  const [status, setStatus] = useState<UpdateStatus | null>(null)
  const [error, setError] = useState('')
  const [disconnected, setDisconnected] = useState(false)
  const [pending, setPending] = useState<'check' | 'install' | null>(null)
  const current = useRef(status), busy = useRef(false), actionBusy = useRef(false), generation = useRef(0)
  const started = useRef(false), mounted = useRef(true), lastRequest = useRef(0)
  current.current = status
  useEffect(() => { mounted.current = true; return () => { mounted.current = false } }, [])

  const run = useCallback(async (action: 'status' | 'check' | 'install') => {
    if (action === 'status') {
      if (busy.current || actionBusy.current) return
      busy.current = true
    } else {
      if (actionBusy.current || isInstalling(current.current)) return
      actionBusy.current = true
      generation.current++
    }
    const requestGeneration = generation.current
    lastRequest.current = Date.now()
    if (action !== 'status') { setPending(action); setError('') }
    try {
      const response = await fetch('/api/updates/' + action, {
        method: action === 'status' ? 'GET' : 'POST', credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json' }, body: action === 'status' ? undefined : '{}',
        signal: AbortSignal.timeout(12000),
      })
      const next = await response.json()
      if (!response.ok) throw new Error(next.error || (response.status === 401 ? '登录已失效，请重新登录后检查更新。' : '更新服务暂不可用，请稍后重试。'))
      if (!next || typeof next.current_version !== 'string' || typeof next.install?.phase !== 'string') throw new Error('更新状态暂不可用，请稍后重试。')
      if (mounted.current && requestGeneration === generation.current) { current.current = next; setStatus(next); setError(''); setDisconnected(false) }
    } catch (failure) {
      if (mounted.current && requestGeneration === generation.current) {
        if (isInstalling(current.current)) setDisconnected(true)
        else setError(failure instanceof Error ? failure.message : '检查更新失败，请稍后重试。')
      }
    } finally {
      if (action === 'status') busy.current = false
      else { actionBusy.current = false; if (mounted.current) setPending(null) }
    }
  }, [])

  useEffect(() => {
    if (enabled && !started.current) { started.current = true; void run('status') }
  }, [enabled, run])
  const fast = status?.checking || isInstalling(status)
  useEffect(() => {
    if (!enabled && !started.current) return
    const timer = setInterval(() => void run('status'), fast ? 1000 : 5 * 60 * 1000)
    const focus = () => { if (Date.now() - lastRequest.current > 30000) void run('status') }
    window.addEventListener('focus', focus)
    return () => { clearInterval(timer); window.removeEventListener('focus', focus) }
  }, [enabled, fast, run])

  return { status, error, pending, disconnected, check: () => run('check'), install: () => run('install') }
}
