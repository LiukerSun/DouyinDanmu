import { useState } from 'react'
import emojiCatalog from '../data/douyin-emojis.json'

const images: Record<string, string> = emojiCatalog

function EmojiImage({ code, src }: { code: string; src: string }) {
  const [failed, setFailed] = useState(false)
  if (failed) return <span title="表情图片加载失败">{code}</span>
  return <img className="chat-emoji" src={src} alt={code} title={code} decoding="async" onError={() => setFailed(true)} />
}

export default function ChatContent({ content }: { content: string }) {
  // React escapes every text segment; unknown codes and Unicode stay untouched.
  return <>{content.split(/(\[[^\[\]\r\n]{1,40}\])/g).map((part, index) =>
    Object.prototype.hasOwnProperty.call(images, part)
      ? <EmojiImage key={`${index}:${part}`} code={part} src={images[part]} />
      : part
  )}</>
}
