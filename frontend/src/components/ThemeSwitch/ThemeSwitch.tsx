import React from 'react';
import { Switch, Tooltip, Space } from 'antd';
import { SunOutlined, MoonOutlined } from '@ant-design/icons';
import type { ThemeMode } from '../../types';

interface ThemeSwitchProps {
  theme: ThemeMode;
  onThemeChange: (theme: ThemeMode) => void;
}

const ThemeSwitch: React.FC<ThemeSwitchProps> = ({ theme, onThemeChange }) => {
  return (
    <Tooltip title={theme === 'dark' ? '切换到亮色模式' : '切换到暗色模式'}>
      <Space>
        <SunOutlined style={{ color: theme === 'light' ? '#faad14' : '#ffffff73' }} />
        <Switch
          checked={theme === 'dark'}
          onChange={(checked) => onThemeChange(checked ? 'dark' : 'light')}
          checkedChildren={<MoonOutlined />}
          unCheckedChildren={<SunOutlined />}
        />
        <MoonOutlined style={{ color: theme === 'dark' ? '#faad14' : '#ffffff73' }} />
      </Space>
    </Tooltip>
  );
};

export default ThemeSwitch;
