import React, { useState } from 'react'
import { Segmented, Empty } from 'antd'
import { UnorderedList, AppstoreOutlined } from '@ant-design/icons'
import BubbleView from './BubbleView'
import TableView from './TableView'
import { Message } from '../../types'

interface Props {
  messages: Message[]
  loading?: boolean
}

const MessageStream: React.FC<Props> = ({ messages, loading }) => {
  const [viewMode, setViewMode] = useState<'bubble' | 'table'>('bubble')

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{ padding: '8px 16px', display: 'flex', justifyContent: 'flex-end' }}>
        <Segmented
          value={viewMode}
          onChange={(v) => setViewMode(v as 'bubble' | 'table')}
          options={[
            { value: 'bubble', icon: <AppstoreOutlined /> },
            { value: 'table', icon: <UnorderedList /> },
          ]}
          size="small"
        />
      </div>
      <div style={{ flex: 1, overflow: 'auto' }}>
        {messages.length === 0 ? (
          <Empty
            description="暂无消息"
            style={{ marginTop: 100 }}
          />
        ) : viewMode === 'bubble' ? (
          <BubbleView messages={messages} />
        ) : (
          <TableView messages={messages} />
        )}
      </div>
    </div>
  )
}

export default MessageStream
