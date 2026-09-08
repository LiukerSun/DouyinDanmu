import React, { useRef, useEffect } from 'react';
import { List, Typography, Avatar, Tag, Space, Empty } from 'antd';
import {
  MessageOutlined,
  GiftOutlined,
  LikeOutlined,
  UserAddOutlined,
  ShareAltOutlined,
  LoginOutlined,
  InfoCircleOutlined,
} from '@ant-design/icons';
import type { Message, ChatMessage, GiftMessage, MessageType } from '../../types';
import dayjs from 'dayjs';
import './MessageStream.css';

const { Text } = Typography;

interface MessageStreamProps {
  messages: Message[];
  autoScroll?: boolean;
  fontSize?: number;
}

const getMessageIcon = (type: MessageType) => {
  switch (type) {
    case 'chat':
      return <MessageOutlined style={{ color: '#1677ff' }} />;
    case 'gift':
      return <GiftOutlined style={{ color: '#fe2c55' }} />;
    case 'like':
      return <LikeOutlined style={{ color: '#eb2f96' }} />;
    case 'social':
      return <UserAddOutlined style={{ color: '#52c41a' }} />;
    case 'enter':
      return <LoginOutlined style={{ color: '#13c2c2' }} />;
    case 'system':
      return <InfoCircleOutlined style={{ color: '#faad14' }} />;
    default:
      return <MessageOutlined />;
  }
};

const getMessageColor = (type: MessageType) => {
  switch (type) {
    case 'gift':
      return '#fff1f0';
    case 'social':
      return '#f6ffed';
    case 'system':
      return '#fffbe6';
    default:
      return 'transparent';
  }
};

const renderMessageContent = (message: Message) => {
  switch (message.type) {
    case 'chat':
      return <Text>{(message as ChatMessage).content}</Text>;
    case 'gift': {
      const giftMsg = message as GiftMessage;
      return (
        <Space>
          <Text>送出</Text>
          <Tag color="red">{giftMsg.gift_name}</Tag>
          <Text strong>x{giftMsg.gift_count}</Text>
        </Space>
      );
    }
    case 'like':
      return <Text type="secondary">点了赞</Text>;
    case 'social':
      return <Text type="success">关注了主播</Text>;
    case 'enter':
      return <Text type="secondary">进入直播间</Text>;
    case 'system':
      return <Text type="warning">{message.content}</Text>;
    default:
      return null;
  }
};

const MessageStream: React.FC<MessageStreamProps> = ({
  messages,
  autoScroll = true,
  fontSize = 14,
}) => {
  const listRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (autoScroll && listRef.current) {
      listRef.current.scrollTop = listRef.current.scrollHeight;
    }
  }, [messages, autoScroll]);

  if (messages.length === 0) {
    return (
      <div className="message-stream-empty">
        <Empty description="暂无消息" />
      </div>
    );
  }

  return (
    <div className="message-stream" ref={listRef} style={{ fontSize }}>
      <List
        dataSource={messages}
        renderItem={(message) => (
          <List.Item
            className="message-item"
            style={{
              backgroundColor: getMessageColor(message.type),
              padding: '8px 16px',
            }}
          >
            <List.Item.Meta
              avatar={
                <Avatar
                  size="small"
                  src={undefined}
                  icon={getMessageIcon(message.type)}
                />
              }
              title={
                <Space>
                  <Text strong style={{ fontSize: fontSize - 2 }}>
                    {message.user_name}
                  </Text>
                  <Text type="secondary" style={{ fontSize: fontSize - 4 }}>
                    {dayjs(message.timestamp).format('HH:mm:ss')}
                  </Text>
                </Space>
              }
              description={
                <div style={{ fontSize, lineHeight: 1.6 }}>
                  {renderMessageContent(message)}
                </div>
              }
            />
          </List.Item>
        )}
      />
    </div>
  );
};

export default MessageStream;
