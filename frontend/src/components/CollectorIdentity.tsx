export type CollectorAuthStatus = 'configured' | 'anonymous' | 'configuration_error'

export default function CollectorIdentity({ status }: { status?: CollectorAuthStatus }) {
  const identity = status === 'configured'
    ? { label: '已配置登录凭据', detail: '登录凭据是否有效，仍需以实际采集结果确认。' }
    : status === 'anonymous'
      ? { label: '游客采集', detail: '本直播间使用游客身份采集；礼物是否下发以实际采集为准。' }
      : status === 'configuration_error'
        ? { label: '登录凭据配置错误', detail: '请检查登录凭据配置，并重新连接直播间。' }
        : { label: '身份状态待确认', detail: '正在等待采集服务返回当前会话身份。' }
  return <div className={'collector-identity ' + (status || 'unknown')} aria-label="采集身份">
    <span><i />{identity.label}</span><p>{identity.detail}</p>
  </div>
}
