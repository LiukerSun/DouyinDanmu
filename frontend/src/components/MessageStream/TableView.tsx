import React from 'react'
import { Table, Tag } from 'antd'
import type { ColumnsType } from 'antd/es/table'
import { Message } from '../../types'
import dayjs from 'dayjs'

interface Props {
  messages: Message[]
}

const typeColors: Record<string, string> = {
  chat: 'blue',
  gift: 'gold',
  enter: 'default',
  like: 'magenta',
  social: 'red',
  system: 'cyan',
  online_count: 'geekblue',
  stats: 'green',
}

const columns: ColumnsType<Message> = [
  {
    title: '时间',
    dataIndex: 'timestamp',
    width: 100,
    render: (ts: number) => dayjs(ts).format('HH:mm:ss'),
  },
  {
    title: '类型',
    dataIndex: 'type',
    width: 80,
    render: (type: string) => <Tag color={typeColors[type] || 'default'}>{type}</Tag>,
  },
  {
    title: '用户',
    dataIndex: 'user_name',
    width: 120,
    ellipsis: true,
  },
  {
    title: '内容',
    dataIndex: 'content',
    ellipsis: true,
    render: (content: string, record: Message) => {
      if (record.type === 'gift') {
        return <span style={{ color: '#ffd700' }}>🎁 {content} x{record.gift_count}</span>
      }
      return content
    },
  },
]

const TableView: React.FC<Props> = ({ messages }) => {
  return (
    <Table<Message>
      columns={columns}
      dataSource={messages}
      rowKey="id"
      size="small"
      pagination={false}
      scroll={{ y: 'calc(100vh - 350px)' }}
      style={{ padding: '0 16px' }}
    />
  )
}

export default TableView
