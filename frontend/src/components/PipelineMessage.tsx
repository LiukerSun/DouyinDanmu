import { useState } from 'react'
import { Button, Chip, Tooltip } from '@heroui/react'
import { CheckOutlined, CopyOutlined, GiftOutlined } from '@ant-design/icons'
import ChatContent from './ChatContent'
import { giftTone, levelBand, messageTypes } from '../pipeline/message-colors'
import { messageKind, messageContent, type PipelineEvent } from '../pipeline/messages'

const labels: Record<string, string> = Object.fromEntries(messageTypes.map(item => [item.type, item.label]))
const time = (timestamp: number) => new Date(timestamp).toLocaleTimeString('zh-CN', { hour12: false })
const exactGiftNumber = (value: string) => { try { return BigInt(value).toLocaleString('zh-CN') } catch { return '—' } }

export default function PipelineMessage({ event, onInspectUser, onInspectEvent, showDate = false }: { event: PipelineEvent; onInspectUser?: (event: PipelineEvent) => void; onInspectEvent?: (event: PipelineEvent) => void; showDate?: boolean }) {
  const kind = messageKind(event)
  const giftValue = event.gift_unit_price != null && Number.isFinite(event.gift_unit_price) && event.gift_unit_price >= 0 ? event.gift_unit_price * event.gift_count : null
  // Raw history includes per-delivery accounting. Live and merged messages keep
  // their cumulative presentation and must not be substituted for these facts.
  const giftStatistics = event.gift_statistics
  const noNewGifts = giftStatistics?.quantity_delta === '0'
  const inlineContent = event.type === 'enter' || event.type === 'system'
  const hasUser = !!event.user_id || ['chat', 'gift', 'enter', 'like', 'social', 'emoji', 'fansclub'].includes(event.type)
  const [copyStatus, setCopyStatus] = useState<'idle' | 'copied' | 'failed'>('idle')
  const uid = event.user_id && event.user_id !== '0' ? event.user_id : ''
  const knownLevel = event.user_level != null && Number.isInteger(event.user_level) && event.user_level >= 0
  const fans = event.fans_club
  const knownFansLevel = fans?.level != null && Number.isInteger(fans.level) && fans.level >= 0
  const wealth = levelBand(event.user_level, 'wealth')
  const fanBand = levelBand(fans?.member === false ? null : fans?.level, 'fans')
  const fanName = fans?.name?.trim()
  const fanLabel = knownFansLevel ? `${fanName || '粉丝团'}-${fans?.level}级` : fanName ? `${fanName}-等级待补全` : fans?.member === true ? '粉丝团-等级待补全' : fans?.member === false ? '未加入粉丝团' : '未显示灯牌'
  const fanDescription = [
    fans?.name ? `灯牌名称：${fans.name}` : '',
    fans?.member !== false && knownFansLevel ? `粉丝团等级区间：${fanBand.label}` : '',
    fans?.member == null ? '本条消息未确认灯牌点亮状态；未显示不等于未加入。' : '',
    fans?.member === true && !knownFansLevel ? '本条消息未提供粉丝团等级。' : '',
  ].filter(Boolean).join(' ')
  const messageTone = messageTypes.find(item => item.type === kind)?.tone || 'slate'
  const copyUid = async () => {
    try { await navigator.clipboard.writeText(uid); setCopyStatus('copied') }
    catch { setCopyStatus('failed') }
  }
  return <div className={'message-row type-' + kind}>
    <div className="message-body">
      <div className="message-meta">
      <strong>{event.user_name || '系统'}</strong>
      {hasUser && <div className="message-profile">
        <span className="message-uid">UID <code>{uid || '未提供'}</code>{uid && <Tooltip delay={350}><Button className="copy-uid" type="button" variant="ghost" isIconOnly size="sm" onPress={copyUid} aria-label={copyStatus === 'copied' ? '已复制 UID' : `复制 UID ${uid}`}>{copyStatus === 'copied' ? <CheckOutlined /> : <CopyOutlined />}</Button><Tooltip.Content>{copyStatus === 'copied' ? '已复制 UID' : '复制 UID'}</Tooltip.Content></Tooltip>}</span>
        <Chip size="sm" className={'message-chip user-level-badge ' + (knownLevel ? 'known' : 'unknown')} data-tone={wealth.tone} title={knownLevel ? '财富等级区间：' + wealth.label : '未提供财富等级'}>{knownLevel ? `财富 Lv.${event.user_level}` : '财富等级未知'}</Chip>
        <Chip size="sm" className={'message-chip fans-badge ' + (fans?.member === true ? 'member' : 'unknown')} data-tone={fanBand.tone} title={fanDescription || undefined} aria-label={fanDescription ? `${fanLabel}。${fanDescription}` : fanLabel}>{fanLabel}</Chip>
        {copyStatus === 'failed' && <span className="copy-feedback" role="status">复制失败，可选中 UID 复制</span>}
      </div>}
      {inlineContent ? <span className="message-content message-content--inline">{event.content || (event.type === 'enter' ? '进入直播间' : '')}</span> : <Chip size="sm" className={'message-chip message-type ' + kind} data-tone={messageTone}>{labels[kind] || kind}</Chip>}
      {hasUser && uid && onInspectUser && <Button size="sm" variant="ghost" className="message-user-history" aria-label={`查看 ${event.user_name || uid} 在所有直播间的记录`} onPress={() => onInspectUser(event)}>用户记录</Button>}
      {onInspectEvent && <Button size="sm" variant="ghost" aria-label={'查看消息详情 · ' + (labels[kind] || '消息')} onPress={() => onInspectEvent(event)}>详情</Button>}
      <time>{showDate ? new Date(event.timestamp).toLocaleString('zh-CN', { hour12: false }) : time(event.timestamp)}</time>
      </div>
      {!inlineContent && <div className="message-content">{event.type === 'gift' ? <>
        <span className="gift-increment-label">{giftStatistics ? noNewGifts ? '本次未新增' : '本次新增' : '送出'}</span>{' '}
        <Chip className="message-chip gift-token" data-tone={giftTone(event.content)}><GiftOutlined /><span className="gift-name">{event.content}</span><strong className="gift-number">× {giftStatistics ? exactGiftNumber(giftStatistics.quantity_delta) : event.gift_count}</strong></Chip>
        {giftStatistics ? <span className="gift-price">{giftStatistics.value_delta === null ? '价格未知' : `${exactGiftNumber(giftStatistics.value_delta)} 钻石`}</span> : giftValue != null && <span className="gift-price">{giftValue.toLocaleString('zh-CN')} 钻石</span>}
        {giftStatistics && <><span className="gift-reported-count">上报数量 × {event.gift_count.toLocaleString('zh-CN')}</span>{noNewGifts && <span className="gift-accounting-note">不重复计入</span>}</>}
        {event.gift_combo !== false && event.gift_final != null && <span className={'gift-progress ' + (event.gift_final ? 'complete' : '')}>{event.gift_final ? '连送完成' : '连送中'}</span>}
      </> : <ChatContent content={messageContent(event)} />}</div>}
    </div>
    <span className="message-seq">#{event.seq}</span>
  </div>
}
