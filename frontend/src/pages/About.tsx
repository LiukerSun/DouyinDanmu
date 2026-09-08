import React from 'react'
import { Card, Typography, Descriptions, Tag } from 'antd'

const { Title, Paragraph, Link } = Typography

const About: React.FC = () => {
  return (
    <div style={{ padding: 24, maxWidth: 600 }}>
      <Card>
        <Title level={3}>抖音弹幕 - 直播间信息获取工具</Title>
        <Paragraph>
          一个用于采集抖音直播间实时信息的桌面工具，支持弹幕、礼物、进场等消息的实时采集和展示。
        </Paragraph>

        <Descriptions column={1} bordered size="small" style={{ marginTop: 24 }}>
          <Descriptions.Item label="版本">0.1.0</Descriptions.Item>
          <Descriptions.Item label="技术栈">
            <Tag>React 18</Tag>
            <Tag>TypeScript</Tag>
            <Tag>Ant Design 5</Tag>
            <Tag>C++ Backend</Tag>
            <Tag>WebSocket</Tag>
          </Descriptions.Item>
          <Descriptions.Item label="功能">
            <ul style={{ margin: 0, paddingLeft: 20 }}>
              <li>多房间实时监控</li>
              <li>弹幕/礼物/进场消息展示</li>
              <li>气泡/列表双视图</li>
              <li>消息筛选与搜索</li>
              <li>通知提醒</li>
              <li>深色/浅色主题</li>
            </ul>
          </Descriptions.Item>
          <Descriptions.Item label="GitHub">
            <Link href="https://github.com/saermart/DouyinLiveWebFetcher" target="_blank">
              DouyinLiveWebFetcher
            </Link>
          </Descriptions.Item>
        </Descriptions>
      </Card>
    </div>
  )
}

export default About
