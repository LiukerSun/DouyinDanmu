import { theme } from 'antd'
import type { ThemeConfig } from 'antd'

export const lightTheme: ThemeConfig = {
  algorithm: theme.defaultAlgorithm,
  token: {
    colorPrimary: '#1677ff',
    borderRadius: 8,
    fontSize: 14,
  },
  components: {
    Layout: {
      headerBg: '#fff',
      siderBg: '#fff',
    },
  },
}
