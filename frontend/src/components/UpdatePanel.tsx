import { Button, Link } from '@heroui/react'
import { DownloadOutlined, ExportOutlined, ReloadOutlined } from '@ant-design/icons'
import InlineFeedback from './InlineFeedback'
import { isInstalling, type useUpdates } from '../hooks/useUpdates'
import { formatBytes } from '../pipeline/monitor'

const releases = 'https://github.com/LiukerSun/DouyinDanmu/releases/latest'
function releaseLink(value?: string) {
  try {
    const url = new URL(value || releases)
    if (url.origin === 'https://github.com' && /^\/LiukerSun\/DouyinDanmu\/releases(?:\/(?:latest|tag\/[^/]+))?$/.test(url.pathname)) return url.href
  } catch { /* Use the fixed release page if a response has an unexpected URL. */ }
  return releases
}

const phaseLabels = { idle: '', downloading: '正在下载更新包', verifying: '正在校验更新包', staging: '正在准备新版本', restarting: '正在重启程序', error: '更新未完成' }

export default function UpdatePanel({ updates, currentVersion }: { updates: ReturnType<typeof useUpdates>; currentVersion?: string }) {
  const { status, error, pending, disconnected } = updates
  const installing = isInstalling(status) || pending === 'install'
  const checking = !!status?.checking || pending === 'check'
  const problem = error || status?.install.error || status?.error
  const current = status?.current_version || currentVersion
  const latest = status?.latest_version
  const phase = status?.install.phase || 'idle'
  const downloaded = status?.install.downloaded_bytes || 0, total = status?.install.total_bytes || 0
  const outcome = status?.update_available ? `发现新版本 v${latest}` : !status?.checked_at || !latest ? '尚未取得最新版本信息' : latest === current ? '当前已是最新稳定版' : '未发现比当前版本更新的稳定版'
  return <section className="update-panel" aria-label="软件更新">
    <div className="update-heading"><h3>{checking ? '正在检查更新…' : outcome}</h3><Button variant="secondary" size="sm" aria-label={checking ? '检查中…' : '检查更新'} isDisabled={checking || installing} onPress={() => void updates.check()}><ReloadOutlined />{checking ? '检查中…' : '检查更新'}</Button></div>
    <dl className="update-versions"><div><dt>当前版本</dt><dd>{current ? 'v' + current : '未知'}</dd></div><div><dt>GitHub 稳定版</dt><dd>{latest ? 'v' + latest : '待检查'}</dd></div></dl>
    {status?.checked_at && <p className="update-checked">上次检查：{new Date(status.checked_at).toLocaleString('zh-CN')}</p>}
    {problem && !disconnected && <InlineFeedback>{problem}</InlineFeedback>}
    {installing && <div className="update-progress" role="status" aria-live="polite">
      <strong>{disconnected ? '更新连接暂时中断' : phaseLabels[phase] || '正在提交更新请求'}</strong>
      {phase === 'downloading' && <><progress aria-label="更新包下载进度" max={total || undefined} value={total ? Math.min(downloaded, total) : undefined} /><span>{formatBytes(downloaded)}{total > 0 ? ' / ' + formatBytes(total) : ''}</span></>}
      <p>{disconnected ? '程序可能正在重启。稍候可重新连接；重启后需要重新登录。' : phase === 'restarting' ? '请稍候，重启后需要重新登录。账号和历史数据会保留。' : '准备完成后将自动重启，请保持程序运行。'}</p>
      {(disconnected || phase === 'restarting') && <Button variant="secondary" aria-label="重新连接" onPress={() => window.location.reload()}><ReloadOutlined />重新连接</Button>}
    </div>}
    {status?.update_available && <>
      {status.release_notes && <div className="update-release-notes"><h4>新版本说明</h4><p>{status.release_notes}</p></div>}
      <p className="update-install-note">更新会暂时停止采集并重启程序，账号、Cookie、配置和历史数据会保留。</p>
      {!status.can_install && !installing && <p className="update-install-note">当前部署或发布包暂不支持一键更新，请前往 GitHub 按发布说明更新。</p>}
    </>}
    <div className="update-actions">
      {status?.update_available && (status.can_install || installing) && <Button aria-label={installing ? '更新进行中…' : '下载并重启更新'} isDisabled={installing || checking} onPress={() => void updates.install()}><DownloadOutlined />{installing ? '更新进行中…' : '下载并重启更新'}</Button>}
      <Link className="update-github" href={releaseLink(status?.release_url)} target="_blank" rel="noopener noreferrer">前往 GitHub<ExportOutlined /></Link>
    </div>
  </section>
}
