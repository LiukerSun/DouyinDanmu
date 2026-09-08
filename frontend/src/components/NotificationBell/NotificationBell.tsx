import React, { useState } from 'react';
import { Badge, Switch, List, Typography, Popover, Button, Space, Tag } from 'antd';
import { BellOutlined, SettingOutlined } from '@ant-design/icons';
import type { NotificationConfig, MessageType } from '../../types';

const { Text, Title } = Typography;

const NOTIFICATION_OPTIONS: { type: MessageType; label: string }[] = [
  { type: 'gift', label: '礼物通知' },
  { type: 'social', label: '关注通知' },
  { type: 'system', label: '系统通知' },
];

interface NotificationBellProps {
  config: NotificationConfig;
  onConfigChange: (config: Partial<NotificationConfig>) => void;
  onRequestPermission: () => Promise<boolean>;
  hasPermission: boolean;
  unreadCount?: number;
}

const NotificationBell: React.FC<NotificationBellProps> = ({
  config,
  onConfigChange,
  onRequestPermission,
  hasPermission,
  unreadCount = 0,
}) => {
  const [popoverOpen, setPopoverOpen] = useState(false);

  const handleToggleType = (type: MessageType) => {
    const current = config.trigger_types || [];
    const next = current.includes(type)
      ? current.filter((t) => t !== type)
      : [...current, type];
    onConfigChange({ trigger_types: next });
  };

  const content = (
    <div style={{ width: 280 }}>
      <Title level={5} style={{ marginTop: 0 }}>
        通知设置
      </Title>
      <List size="small">
        {NOTIFICATION_OPTIONS.map((opt) => (
          <List.Item
            key={opt.type}
            actions={[
              <Switch
                checked={(config.trigger_types || []).includes(opt.type)}
                onChange={() => handleToggleType(opt.type)}
                size="small"
              />,
            ]}
          >
            <Space>
              <BellOutlined />
              <Text>{opt.label}</Text>
            </Space>
          </List.Item>
        ))}
      </List>
      {!hasPermission && (
        <div style={{ marginTop: 12 }}>
          <Tag color="warning">浏览器通知权限未开启</Tag>
          <Button
            size="small"
            type="link"
            onClick={async () => {
              await onRequestPermission();
            }}
          >
            申请权限
          </Button>
        </div>
      )}
    </div>
  );

  return (
    <Popover
      content={content}
      trigger="click"
      open={popoverOpen}
      onOpenChange={setPopoverOpen}
      placement="bottomRight"
    >
      <Badge count={unreadCount} size="small">
        <Button
          type="text"
          icon={<SettingOutlined />}
          style={{ fontSize: 18 }}
        />
      </Badge>
    </Popover>
  );
};

export default NotificationBell;
