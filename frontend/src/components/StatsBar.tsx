import React from 'react'
import { Statistic } from 'antd'
import { MessageOutlined, GiftOutlined, UserOutlined, TeamOutlined } from '@ant-design/icons'

interface Props {
  chatCount: number
  giftCount: number
  enterCount: number
  onlineCount: number
}

const StatsBar: React.FC<Props> = ({ chatCount, giftCount, enterCount, onlineCount }) => {
  return (
    <div style={{
      padding: '12px 24px',
      borderTop: '1px solid rgba(0,0,0,0.06)',
      display: 'flex',
      justifyContent: 'space-around',
      background: 'rgba(0,0,0,0.02)',
    }}>
      <Statistic
        title="弹幕"
        value={chatCount}
        prefix={<MessageOutlined />}
        valueStyle={{ fontSize: 18 }}
      />
      <Statistic
        title="礼物"
        value={giftCount}
        prefix={<GiftOutlined />}
        valueStyle={{ fontSize: 18, color: '#ffd700' }}
      />
      <Statistic
        title="进场"
        value={enterCount}
        prefix={<UserOutlined />}
        valueStyle={{ fontSize: 18 }}
      />
      <Statistic
        title="在线"
        value={onlineCount}
        prefix={<TeamOutlined />}
        valueStyle={{ fontSize: 18, color: '#1677ff' }}
      />
    </div>
  )
}

export default StatsBar
