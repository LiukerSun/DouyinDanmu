import React from 'react'
import { Checkbox, Input, Space } from 'antd'
import { SearchOutlined } from '@ant-design/icons'
import { MessageType } from '../types'

interface Props {
  filter: Set<MessageType>
  keyword: string
  onToggleFilter: (type: MessageType) => void
  onKeywordChange: (keyword: string) => void
}

const filterOptions: { type: MessageType; label: string }[] = [
  { type: 'chat', label: '💬 弹幕' },
  { type: 'gift', label: '🎁 礼物' },
  { type: 'enter', label: '🚪 进场' },
  { type: 'like', label: '❤️ 点赞' },
  { type: 'social', label: '💕 关注' },
  { type: 'system', label: '📢 系统' },
]

const FilterBar: React.FC<Props> = ({ filter, keyword, onToggleFilter, onKeywordChange }) => {
  return (
    <div style={{ padding: '8px 16px', display: 'flex', alignItems: 'center', gap: 16, flexWrap: 'wrap' }}>
      <Space size="middle" wrap>
        {filterOptions.map(opt => (
          <Checkbox
            key={opt.type}
            checked={filter.size === 0 || filter.has(opt.type)}
            onChange={() => onToggleFilter(opt.type)}
          >
            {opt.label}
          </Checkbox>
        ))}
      </Space>
      <Input
        placeholder="搜索消息..."
        prefix={<SearchOutlined />}
        value={keyword}
        onChange={e => onKeywordChange(e.target.value)}
        style={{ width: 200 }}
        allowClear
        size="small"
      />
    </div>
  )
}

export default FilterBar
