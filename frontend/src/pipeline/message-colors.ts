export const wealthBands = [
  { min: 0, label: '0–9', tone: 'slate' },
  { min: 10, label: '10–19', tone: 'teal' },
  { min: 20, label: '20–29', tone: 'blue' },
  { min: 30, label: '30–39', tone: 'violet' },
  { min: 40, label: '40–49', tone: 'amber' },
  { min: 50, label: '50+', tone: 'rose' },
] as const
export const fansBands = [
  { min: 0, label: '0–4', tone: 'teal' },
  { min: 5, label: '5–9', tone: 'blue' },
  { min: 10, label: '10–14', tone: 'violet' },
  { min: 15, label: '15–19', tone: 'amber' },
  { min: 20, label: '20+', tone: 'rose' },
] as const
export const messageTypes = [
  { type: 'chat', label: '弹幕', tone: 'blue' },
  { type: 'gift', label: '礼物', tone: 'amber' },
  { type: 'enter', label: '进场', tone: 'teal' },
  { type: 'like', label: '点赞', tone: 'rose' },
  { type: 'follow', label: '关注', tone: 'violet' },
  { type: 'share', label: '分享', tone: 'blue' },
  { type: 'social', label: '其他互动', tone: 'violet' },
  { type: 'system', label: '系统', tone: 'slate' },
  { type: 'emoji', label: '表情', tone: 'blue' },
  { type: 'fansclub', label: '粉丝团', tone: 'violet' },
  { type: 'online_count', label: '在线人数', tone: 'teal' },
  { type: 'room_stats', label: '统计更新', tone: 'teal' },
  { type: 'room_notice', label: '房间通知', tone: 'slate' },
  { type: 'room_rank', label: '房间榜单', tone: 'amber' },
  { type: 'hour_rank', label: '小时榜', tone: 'amber' },
  { type: 'banner', label: '横幅', tone: 'violet' },
  { type: 'stream', label: '流适配', tone: 'slate' },
  { type: 'room_sync', label: '房间同步', tone: 'slate' },
  { type: 'notice', label: '直播通知', tone: 'slate' },
  { type: 'episode_chat', label: '节目聊天', tone: 'blue' },
  { type: 'audio_chat', label: '语音聊天', tone: 'blue' },
  { type: 'screen_chat', label: '屏幕聊天', tone: 'blue' },
  { type: 'commerce', label: '电商', tone: 'orange' },
  { type: 'game', label: '游戏/竞猜', tone: 'violet' },
  { type: 'lucky_box', label: '福袋', tone: 'amber' },
  { type: 'decoration', label: '装饰', tone: 'slate' },
  { type: 'linkmic', label: '连麦/KTV', tone: 'teal' },
  { type: 'effect', label: '特效', tone: 'violet' },
  { type: 'gift_notice', label: '礼物关联', tone: 'amber' },
  { type: 'task', label: '任务', tone: 'teal' },
  { type: 'protocol', label: '其他协议', tone: 'slate' },
  { type: 'unknown', label: '待识别消息', tone: 'orange' },
  { type: 'parse_error', label: '解析失败', tone: 'rose' },
] as const

export const feedMessageTypes = ['chat', 'gift', 'follow', 'share', 'enter', 'like', 'fansclub', 'social', 'notice'].map(type => messageTypes.find(item => item.type === type)!)

export function levelBand(level: number | null | undefined, kind: 'wealth' | 'fans') {
  if (level == null || !Number.isInteger(level) || level < 0) return { tone: 'slate', label: '等级未知' }
  const bands = kind === 'wealth' ? wealthBands : fansBands
  return [...bands].reverse().find(band => level >= band.min)!
}

const palette = ['teal', 'blue', 'violet', 'amber', 'rose', 'orange'] as const
export function identityTone(value: string) {
  let hash = 0
  for (const character of value) hash = (Math.imul(hash, 31) + character.codePointAt(0)!) >>> 0
  return palette[hash % palette.length]
}

// Visual identities, independent of gift price or any platform ranking.
const giftTones: Record<string, string> = {
  '粉丝团灯牌': 'violet', '为你闪耀': 'amber', '小心心': 'rose',
  '玫瑰': 'rose', '啤酒': 'orange', '比心': 'teal', '抖音一号': 'blue',
}
export function giftTone(name: string) {
  const key = name.trim().normalize('NFKC')
  return Object.prototype.hasOwnProperty.call(giftTones, key) ? giftTones[key] : identityTone(key)
}
