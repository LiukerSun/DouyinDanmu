import React from 'react'
import { Switch } from 'antd'
import { SunOutlined, MoonOutlined } from '@ant-design/icons'

interface Props {
  isDark: boolean
  onChange: (dark: boolean) => void
}

const ThemeSwitch: React.FC<Props> = ({ isDark, onChange }) => {
  return (
    <Switch
      checked={isDark}
      onChange={onChange}
      checkedChildren={<MoonOutlined />}
      unCheckedChildren={<SunOutlined />}
    />
  )
}

export default ThemeSwitch
