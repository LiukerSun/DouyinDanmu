import React from 'react';
import { Space, Select, Tag, Input, Switch, Typography, Tooltip } from 'antd';
import {
  FilterOutlined,
  MessageOutlined,
  GiftOutlined,
  LikeOutlined,
  UserAddOutlined,
  ShareAltOutlined,
  LoginOutlined,
  InfoCircleOutlined,
} from '@ant-design/icons';
import type { FilterConfig, MessageType } from '../../types';

const { Text } = Typography;
const { Search } = Input;

interface FilterBarProps {
  filter: FilterConfig;
  onFilterChange: (filter: Partial<FilterConfig>) => void;
  onReset: () => void;
}

const messageTypeOptions: { value: MessageType; label: string; icon: React.ReactNode }[] = [
  { value: 'chat', label: '聊天', icon: <MessageOutlined /> },
  { value: 'gift', label: '礼物', icon: <GiftOutlined /> },
  { value: 'like', label: '点赞', icon: <LikeOutlined /> },
  { value: 'social', label: '关注', icon: <UserAddOutlined /> },
  { value: 'enter', label: '进入', icon: <LoginOutlined /> },
  { value: 'system', label: '系统', icon: <InfoCircleOutlined /> },
];

const selectOptions = messageTypeOptions.map((opt) => ({
  value: opt.value,
  label: (
    <Space>
      {opt.icon}
      {opt.label}
    </Space>
  ),
}));

const FilterBar: React.FC<FilterBarProps> = ({ filter, onFilterChange, onReset }) => {
  const handleTypeChange = (values: MessageType[]) => {
    onFilterChange({ messageTypes: values });
  };

  const handleKeywordChange = (value: string) => {
    const keywords = value
      .split(',')
      .map((k) => k.trim())
      .filter(Boolean);
    onFilterChange({ keywords });
  };

  return (
    <Space wrap size="middle" style={{ width: '100%' }}>
      <Space>
        <FilterOutlined />
        <Text strong>过滤：</Text>
      </Space>

      <Select
        mode="multiple"
        placeholder="消息类型"
        value={filter.messageTypes}
        onChange={handleTypeChange}
        style={{ minWidth: 200 }}
        options={selectOptions}
        maxTagCount="responsive"
      />

      <Search
        placeholder="关键词过滤（逗号分隔）"
        onSearch={handleKeywordChange}
        style={{ width: 220 }}
        allowClear
      />

      <Tooltip title="显示系统消息">
        <Space>
          <Switch
            size="small"
            checked={filter.showSystemMessages}
            onChange={(checked) => onFilterChange({ showSystemMessages: checked })}
          />
          <Text type="secondary">系统消息</Text>
        </Space>
      </Tooltip>

      <Tag
        closable
        onClose={(e) => {
          e.preventDefault();
          onReset();
        }}
        color="processing"
      >
        重置过滤
      </Tag>
    </Space>
  );
};

export default FilterBar;
