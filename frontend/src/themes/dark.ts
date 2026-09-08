import { theme } from 'antd'
import type { ThemeConfig } from 'antd'

export const darkTheme: ThemeConfig = {
  algorithm: theme.darkAlgorithm,
  token: {
    colorPrimary: '#1677ff',
    colorBgContainer: '#141414',
    colorBgElevated: '#1f1f1f',
    borderRadius: 8,
    fontSize: 14,
  },
  components: {
    Layout: {
      headerBg: '#1a1a2e',
      siderBg: '#16213e',
      bodyBg: '#0f0f23',
    },
    Card: {
      colorBgContainer: '#1a1a2e',
    },
  },
}
