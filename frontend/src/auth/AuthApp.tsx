import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react'
import { Avatar, Button, Form, Input, Label, Link, Modal, Spinner } from '@heroui/react'
import { EyeInvisibleOutlined, EyeOutlined, LockOutlined, UserOutlined, AudioOutlined, CheckCircleOutlined } from '@ant-design/icons'
import Pipeline from '../pages/Pipeline'
import './Auth.css'
import InlineFeedback from '../components/InlineFeedback'

export type StudioUser = { username: string; displayName: string; role: string }
type Session = { user: StudioUser | null; setupRequired: boolean }
async function authRequest<T>(action: string, body?: object): Promise<T> {
  const response = await fetch('/api/auth/' + action, { method: body ? 'POST' : 'GET', credentials: 'same-origin', headers: { 'Content-Type': 'application/json' }, body: body ? JSON.stringify(body) : undefined, signal: AbortSignal.timeout(15000) })
  let data
  try { data = await response.json() } catch { throw new Error('账号服务暂时不可用，请稍后重试') }
  if (!response.ok) throw new Error(data.error || '请求失败')
  return data
}
function PasswordField({ id, label, value, onChange, autoComplete = 'current-password' }: { id: string; label: string; value: string; onChange: (value: string) => void; autoComplete?: string }) {
  const [visible, setVisible] = useState(false)
  return <div className="auth-field"><Label htmlFor={id}>{label}</Label><div className="auth-input-wrap"><LockOutlined /><Input id={id} name={id} type={visible ? 'text' : 'password'} value={value} onChange={event => onChange(event.target.value)} autoComplete={autoComplete} maxLength={128} required /><Button type="button" isIconOnly variant="ghost" aria-label={visible ? '隐藏' + label : '显示' + label} onPress={() => setVisible(!visible)}>{visible ? <EyeInvisibleOutlined /> : <EyeOutlined />}</Button></div></div>
}
export default function AuthApp() {
  const [session, setSession] = useState<Session | null>(null), [initialError, setInitialError] = useState('')
  const [username, setUsername] = useState(''), [displayName, setDisplayName] = useState(''), [password, setPassword] = useState(''), [confirm, setConfirm] = useState('')
  const [busy, setBusy] = useState(false), [error, setError] = useState(''), [accountOpen, setAccountOpen] = useState(false), [newPassword, setNewPassword] = useState(''), [result, setResult] = useState('')
  const sessionVersion = useRef(0)
  const checkSession = useCallback(async () => {
    const version = sessionVersion.current
    try { const next = await authRequest<Session>('session'); if (version !== sessionVersion.current) return; setSession(next); setInitialError('') }
    catch (failure) { setInitialError((failure as Error).message) }
  }, [])
  useEffect(() => { if (!session?.user) { setAccountOpen(false); setPassword(''); setNewPassword(''); setConfirm('') } }, [session?.user?.username])
  useEffect(() => { void checkSession(); const timer = setInterval(() => void checkSession(), 30000); const expired = () => { setSession(previous => previous ? { ...previous, user: null } : previous); setAccountOpen(false); setPassword(''); setError('登录已过期，请重新登录') }; window.addEventListener('studio:unauthorized', expired); return () => { clearInterval(timer); window.removeEventListener('studio:unauthorized', expired) } }, [checkSession])
  const submit = async (event: FormEvent) => {
    event.preventDefault(); if (busy) return
    setError('')
    if (session?.setupRequired && (password.length < 12 || password !== confirm)) { setError(password.length < 12 ? '密码至少需要 12 个字符' : '两次输入的密码不一致'); return }
    setBusy(true); sessionVersion.current++
    try { const data = await authRequest<{ user: StudioUser }>(session?.setupRequired ? 'setup' : 'login', { username, password, displayName }); setSession({ user: data.user, setupRequired: false }); setPassword(''); setConfirm('') }
    catch (failure) { setError((failure as Error).message); setPassword(''); setConfirm(''); if ((failure as Error).message.includes('管理员已创建')) setSession({ user: null, setupRequired: false }) }
    finally { sessionVersion.current++; setBusy(false) }
  }
  const logout = async () => {
    if (busy) return
    setBusy(true); sessionVersion.current++; setError(''); setResult('')
    try { await authRequest('logout', {}); setSession(previous => ({ setupRequired: previous?.setupRequired || false, user: null })); setAccountOpen(false); setPassword(''); setNewPassword('') }
    catch (failure) { setError((failure as Error).message); setAccountOpen(true) }
    finally { sessionVersion.current++; setBusy(false) }
  }
  const changePassword = async (event: FormEvent) => {
    event.preventDefault(); if (busy) return
    setError(''); setResult('')
    if (newPassword.length < 12 || newPassword !== confirm) { setError(newPassword.length < 12 ? '新密码至少需要 12 个字符' : '两次输入的新密码不一致'); return }
    setBusy(true); sessionVersion.current++
    try { await authRequest('password', { password, newPassword }); setPassword(''); setNewPassword(''); setConfirm(''); setResult('密码已更新，其他设备的登录已失效。') }
    catch (failure) { setError((failure as Error).message) }
    finally { sessionVersion.current++; setBusy(false) }
  }
  if (!session) return <div className="auth-loading"><Spinner size="lg" aria-label="正在连接工作台" /><p>{initialError || '正在连接工作台…'}</p>{initialError && <Button variant="secondary" onPress={() => void checkSession()}>重新连接</Button>}</div>
  if (session.user) return <>
    <Pipeline user={session.user} onAccount={() => { setPassword(''); setConfirm(''); setNewPassword(''); setError(''); setResult(''); setAccountOpen(true) }} />
    <Modal.Backdrop isOpen={accountOpen} onOpenChange={value => { if (!busy) { setAccountOpen(value); setPassword(''); setConfirm(''); setNewPassword('') } }}><Modal.Container><Modal.Dialog className="console-dialog account-dialog"><Modal.CloseTrigger aria-label="关闭个人信息" /><Modal.Header><Modal.Heading>个人信息</Modal.Heading></Modal.Header><Modal.Body><div className="account-summary"><Avatar className="account-avatar"><Avatar.Fallback>{Array.from(session.user.displayName)[0]}</Avatar.Fallback></Avatar><div><h3>{session.user.displayName}</h3><p>{session.user.username} · {session.user.role}</p></div></div><Form validationBehavior="native" onSubmit={changePassword}><h4>修改登录密码</h4><PasswordField id="current-password" label="当前密码" value={password} onChange={setPassword} /><PasswordField id="new-password" label="新密码" value={newPassword} onChange={setNewPassword} autoComplete="new-password" /><PasswordField id="confirm-new-password" label="确认新密码" value={confirm} onChange={setConfirm} autoComplete="new-password" /><p className="auth-hint">使用 12–128 个字符，修改后其他登录会自动失效。</p>{error && <InlineFeedback className="auth-feedback">{error}</InlineFeedback>}{result && <InlineFeedback success className="auth-feedback">{result}</InlineFeedback>}<div className="account-actions"><Button type="submit" isDisabled={busy || !password || !newPassword || !confirm}>{busy ? '处理中…' : '保存新密码'}</Button><Button type="button" variant="ghost" isDisabled={busy} onPress={() => void logout()}>退出登录</Button></div></Form></Modal.Body></Modal.Dialog></Modal.Container></Modal.Backdrop>
  </>
  return <main className="login-page">
    <section className="login-story"><Link href="/" className="login-brand"><AudioOutlined />直播台</Link><div className="login-story-content"><span className="story-mark"><i /><i /><i /><i /><i /></span><h1>每个直播间，<br />都在视线之内。</h1><p>把分散的直播互动汇集到一个工作台，<br />及时看见弹幕、礼物和需要关注的房间。</p><div className="login-capabilities"><span><CheckCircleOutlined />多房间同步监控</span><span><CheckCircleOutlined />独立采集身份</span></div></div><span className="login-story-footer">抖音直播监控工作台</span></section>
    <section className="login-main"><div className="login-form-container"><span className="login-form-icon"><UserOutlined /></span><h2>{session.setupRequired ? '创建你的工作台' : '欢迎回来'}</h2><p className="login-description">{session.setupRequired ? '首次使用，请设置管理员账号和登录密码。' : '登录账号，继续关注你的直播间。'}</p><Form validationBehavior="native" onSubmit={submit}>
      {session.setupRequired && <div className="auth-field"><Label htmlFor="display-name">你的姓名</Label><Input id="display-name" name="name" autoComplete="name" placeholder="用于左下角个人信息" value={displayName} onChange={event => setDisplayName(event.target.value)} maxLength={32} /></div>}
      <div className="auth-field"><Label htmlFor="username">账号</Label><div className="auth-input-wrap"><UserOutlined /><Input id="username" name="username" autoComplete="username" placeholder={session.setupRequired ? '3–32 位字母、数字或 _ . -' : '请输入账号'} value={username} onChange={event => setUsername(event.target.value)} pattern="[a-zA-Z0-9_.\-]{3,32}" maxLength={32} required /></div></div>
      <PasswordField id="password" label="密码" value={password} onChange={setPassword} autoComplete={session.setupRequired ? 'new-password' : 'current-password'} />
      {session.setupRequired && <><PasswordField id="confirm-password" label="确认密码" value={confirm} onChange={setConfirm} autoComplete="new-password" /><p className="auth-hint">密码至少 12 个字符，建议使用不易猜测的长密码。</p></>}
      {(error || initialError) && <InlineFeedback className="auth-feedback">{error || initialError}</InlineFeedback>}
      <Button type="submit" fullWidth className="login-submit" isDisabled={busy || !username.trim() || !password || (session.setupRequired && !confirm)}>{busy ? '正在验证…' : session.setupRequired ? '创建账号并进入工作台' : '登录工作台'}</Button>
    </Form><p className="login-note"><LockOutlined />仅授权用户可以访问直播数据和采集配置</p></div><footer>直播台 <span>账号与数据保存在当前部署中</span></footer></section>
  </main>
}
