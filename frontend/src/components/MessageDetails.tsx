import { useEffect, useState } from 'react'
import { Button, Disclosure, Modal, Spinner } from '@heroui/react'
import { request } from '../pipeline/monitor'
import { messageContent, messageKind, parseStatusLabels, type PipelineEvent } from '../pipeline/messages'
import { feedMessageTypes } from '../pipeline/message-colors'
import ChatContent from './ChatContent'

type Detail = { event: PipelineEvent; debug_mode?: boolean; details: { decoded?: unknown; wire?: unknown; error?: string; payload_base64?: string; schema_evidence?: string; field_analysis?: { entries: unknown[]; truncated: boolean; note: string }; [key: string]: unknown } | null }
export default function MessageDetails({ event, onClose }: { event: PipelineEvent | null; onClose: () => void }) {
  const [result, setResult] = useState<Detail | null>(null)
  const [error, setError] = useState('')
  const [revision, setRevision] = useState(0)
  useEffect(() => {
    setResult(null); setError('')
    if (!event) return
    const controller = new AbortController()
    void request<Detail>('/messages/detail?' + new URLSearchParams({ event_id: event.event_id }), { signal: controller.signal })
      .then(value => { if (!controller.signal.aborted) setResult(value) })
      .catch(reason => { if (!controller.signal.aborted) setError((reason as Error).message) })
    return () => controller.abort()
  }, [event?.event_id, revision])
  const data = result?.details
  const debugMode = result?.debug_mode === true && result.event.event_id === event?.event_id
  const preview = JSON.stringify(data?.decoded ?? data?.wire ?? {}, null, 2)
  const analysis = data?.field_analysis
  const analysisPreview = JSON.stringify(analysis?.entries || [], null, 2)
  return <Modal.Backdrop isOpen={!!event} onOpenChange={open => { if (!open) onClose() }}><Modal.Container size="lg"><Modal.Dialog className="console-dialog message-detail-dialog">
    <Modal.CloseTrigger aria-label="关闭消息详情" /><Modal.Header><Modal.Heading>消息详情</Modal.Heading></Modal.Header>
    <Modal.Body>
      {event && <>
        <dl className="message-detail-meta"><dt>行为</dt><dd>{feedMessageTypes.find(item => item.type === messageKind(event))?.label || '互动'}</dd><dt>用户</dt><dd>{event.user_name || '直播间'}</dd>{event.user_id && <><dt>UID</dt><dd>{event.user_id}</dd></>}<dt>直播间</dt><dd>{event.live_id}</dd><dt>时间</dt><dd>{new Date(event.timestamp).toLocaleString('zh-CN')}</dd></dl>
        <p><ChatContent content={messageContent(event)} />{event.type === 'gift' && ` × ${event.gift_count}`}</p>
      </>}
      {!result && !error && <Spinner aria-label="正在加载消息详情" />}
      {error && <p role="alert">{error} <Button size="sm" variant="secondary" onPress={() => setRevision(n => n + 1)}>重试</Button></p>}
      {debugMode && <Disclosure key={event?.event_id} className="protocol-disclosure"><Disclosure.Heading><Disclosure.Trigger>协议诊断<Disclosure.Indicator /></Disclosure.Trigger></Disclosure.Heading><Disclosure.Content><Disclosure.Body>
      <dl className="message-detail-meta"><dt>原始类型</dt><dd>{event?.method || event?.type}</dd><dt>解析状态</dt><dd>{parseStatusLabels[result?.event.parse_status || event?.parse_status || 'legacy']}</dd><dt>接收时间</dt><dd>{event && new Date(event.received_at_ms).toLocaleString('zh-CN')}</dd><dt>消息 ID</dt><dd>{event?.event_id}</dd></dl>
      {result && !data && <p>这条历史记录没有保留原始载荷，无法补全详情。</p>}
      {data && <>
        <p>协议解码不代表所有字段的业务含义均已确认。未知字段和未展开的嵌套载荷保留在原始数据中。</p>
        {data.schema_evidence && <p>{data.schema_evidence}</p>}
        {analysis && <p>已展开 {analysis.entries.length} 处{data.decoded ? '未知或二进制字段' : '原始字段'}{analysis.truncated ? '（部分预览达到上限）' : ''}。下方可查看字段路径、可读文本及候选嵌套结构。</p>}
        {data.error && <p role="alert">解析错误：{data.error}</p>}
        <h3>{data.decoded ? '已识别字段' : '原始字段编号与值'}</h3>
        <pre className="message-detail-json">{preview.slice(0, 50000)}</pre>
        {preview.length > 50000 && <p>详情较长，当前预览前 50,000 字符。</p>}
        {analysis && analysis.entries.length > 0 && <>
          <h3>未知字段结构分析</h3><p>{analysis.note}</p>
          <pre className="message-detail-json">{analysisPreview.slice(0, 50000)}</pre>
          {(analysis.truncated || analysisPreview.length > 50000) && <p>结构预览已截断。</p>}
        </>}
        <Disclosure className="protocol-disclosure"><Disclosure.Heading><Disclosure.Trigger>原始载荷（Base64，{event?.payload_bytes ?? 0} 字节）<Disclosure.Indicator /></Disclosure.Trigger></Disclosure.Heading><Disclosure.Content><Disclosure.Body><pre className="message-detail-json">{data.payload_base64?.slice(0, 4096)}</pre>{(data.payload_base64?.length || 0) > 4096 && <p>此处显示开头片段。</p>}</Disclosure.Body></Disclosure.Content></Disclosure>
      </>}
      </Disclosure.Body></Disclosure.Content></Disclosure>}
    </Modal.Body><Modal.Footer><Button variant="ghost" onPress={onClose}>关闭</Button></Modal.Footer>
  </Modal.Dialog></Modal.Container></Modal.Backdrop>
}
