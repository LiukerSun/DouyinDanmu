import React, { useState } from 'react'
import { Badge, Popover, List, Typography, Empty } from 'antd'
import { BellOutlined } from '@ant-design/icons'
import { Message } from '../types'
import dayjs from 'dayjs'

interface Props {
  notifications: Message[]
  unreadCount: number
  onClearUnread: () => void
}

const NotificationBell: React.FC<Props> = ({ notifications, unreadCount, onClearUnread }) => {
  const [open, setOpen] = useState(false)

  const handleOpenChange = (visible: boolean) => {
    setOpen(visible)
    if (visible && unreadCount > 0) {
      onClearUnread()
    }
  }

  const content = (
    <div style={{ width: 320, maxHeight: 400, overflow: 'auto' }}>
      {notifications.length === 0 ? (
        <Empty description="暂无通知" image={Empty.PRESENTED_IMAGE_SIMPLE} />
      ) : (
        <List
          size="small"
          dataSource={notifications.slice(0, 20)}
          renderItem={(msg) => (
            <List.Item style={{ padding: '8px 0' }}>
              <List.Item.Meta
                title={
                  <span>
                    {msg.type === 'gift' ? '🎁' : '💕'} {msg.user_name}
                    <Typography.Text type="secondary" style={{ marginLeft: 8, fontSize: 12 }}>
                      {dayjs(msg.timestamp).format('HH:mm')}
                    </Typography.Text>
                  </span>
                }
                description={msg.content || (msg.type === 'social' ? '关注了主播' : '')}
              />
            </List.Item>
          )}
        />
      )}
    </div>
  )

  return (
    <Popover
      content={content}
      title="通知"
      trigger="click"
      open={open}
      onOpenChange={handleOpenChange}
    >
      <Badge count={unreadCount} size="small" offset={[-2, 2]}>
        <BellOutlined style={{ fontSize: 20, cursor: 'pointer' }} />
      </Badge>
    </Popover>
  )
}

export default NotificationBell
