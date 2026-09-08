import React from 'react';
import { Card, Statistic, Row, Col, Tooltip, Space, Typography } from 'antd';
import {
  MessageOutlined,
  GiftOutlined,
  LikeOutlined,
  UserOutlined,
  TeamOutlined,
} from '@ant-design/icons';
import type { StatsData } from '../../types';

const { Text } = Typography;

interface StatsBarProps {
  stats: StatsData;
  compact?: boolean;
}

const StatsBar: React.FC<StatsBarProps> = ({ stats, compact = false }) => {
  if (compact) {
    return (
      <Space size="large" wrap>
        <Tooltip title="消息总数">
          <Space>
            <MessageOutlined />
            <Text strong>{stats.totalMessages}</Text>
          </Space>
        </Tooltip>
        <Tooltip title="礼物总数">
          <Space>
            <GiftOutlined style={{ color: '#fe2c55' }} />
            <Text strong>{stats.giftCount}</Text>
          </Space>
        </Tooltip>
        <Tooltip title="点赞总数">
          <Space>
            <LikeOutlined style={{ color: '#eb2f96' }} />
            <Text strong>{stats.likeCount}</Text>
          </Space>
        </Tooltip>
        <Tooltip title="在线人数">
          <Space>
            <TeamOutlined style={{ color: '#1677ff' }} />
            <Text strong>{stats.onlineCount}</Text>
          </Space>
        </Tooltip>
      </Space>
    );
  }

  return (
    <Card size="small">
      <Row gutter={[16, 16]}>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="消息"
            value={stats.totalMessages}
            prefix={<MessageOutlined />}
          />
        </Col>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="弹幕"
            value={stats.chatCount}
            prefix={<MessageOutlined />}
          />
        </Col>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="礼物"
            value={stats.giftCount}
            prefix={<GiftOutlined />}
            valueStyle={{ color: '#fe2c55' }}
          />
        </Col>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="点赞"
            value={stats.likeCount}
            prefix={<LikeOutlined />}
            valueStyle={{ color: '#eb2f96' }}
          />
        </Col>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="进场"
            value={stats.enterCount}
            prefix={<UserOutlined />}
            valueStyle={{ color: '#52c41a' }}
          />
        </Col>
        <Col xs={12} sm={8} md={4}>
          <Statistic
            title="在线"
            value={stats.onlineCount}
            prefix={<TeamOutlined />}
            valueStyle={{ color: '#1677ff' }}
          />
        </Col>
      </Row>
    </Card>
  );
};

export default StatsBar;
