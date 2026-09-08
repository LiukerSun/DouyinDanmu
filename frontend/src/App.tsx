import React, { useState, useEffect } from 'react'
import { BrowserRouter, Routes, Route, useNavigate, useLocation } from 'react-router-dom'
import { ConfigProvider, Layout, Menu, theme } from 'antd'
import {
  MessageOutlined,
  SettingOutlined,
  InfoCircleOutlined,
} from '@ant-design/icons'
import zhCN from 'antd/locale/zh_CN'
import Room from './pages/Room'
import Settings from './pages/Settings'
import About from './pages/About'
import ThemeSwitch from './components/ThemeSwitch'
import NotificationBell from './components/NotificationBell'
import { AppProvider, useWs, useNotif } from './context'
import { darkTheme } from './themes/dark'
import { lightTheme } from './themes/light'

const { Header, Content, Sider } = Layout

const AppLayout: React.FC = () => {
  const [isDark, setIsDark] = useState(() => {
    return localStorage.getItem('theme') === 'dark'
  })
  const navigate = useNavigate()
  const location = useLocation()

  const { connected } = useWs()
  const { notifications, unreadCount, clearUnread } = useNotif()

  useEffect(() => {
    localStorage.setItem('theme', isDark ? 'dark' : 'light')
  }, [isDark])

  const menuItems = [
    { key: '/', icon: <MessageOutlined />, label: '直播间' },
    { key: '/settings', icon: <SettingOutlined />, label: '设置' },
    { key: '/about', icon: <InfoCircleOutlined />, label: '关于' },
  ]

  return (
    <ConfigProvider
      theme={isDark ? darkTheme : lightTheme}
      locale={zhCN}
    >
      <Layout style={{ height: '100vh', overflow: 'hidden' }}>
        <Sider
          breakpoint="lg"
          collapsedWidth="0"
          style={{
            background: isDark ? '#16213e' : '#fff',
            height: '100vh',
            overflow: 'auto',
          }}
        >
          <div style={{
            height: 64,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: isDark ? '#fff' : '#1677ff',
            fontSize: 18,
            fontWeight: 'bold',
          }}>
            抖音弹幕
          </div>
          <Menu
            mode="inline"
            selectedKeys={[location.pathname]}
            items={menuItems}
            onClick={({ key }) => navigate(key)}
            style={{ borderRight: 0 }}
          />
        </Sider>
        <Layout style={{ height: '100vh', overflow: 'hidden' }}>
          <Header style={{
            padding: '0 24px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'flex-end',
            gap: 16,
            background: isDark ? '#1a1a2e' : '#fff',
            borderBottom: '1px solid rgba(0,0,0,0.06)',
            height: 64,
            flexShrink: 0,
          }}>
            <div style={{ flex: 1 }}>
              {connected ? (
                <span style={{ color: '#52c41a' }}>&#9679; 已连接</span>
              ) : (
                <span style={{ color: '#ff4d4f' }}>&#9679; 未连接</span>
              )}
            </div>
            <NotificationBell
              notifications={notifications}
              unreadCount={unreadCount}
              onClearUnread={clearUnread}
            />
            <ThemeSwitch isDark={isDark} onChange={setIsDark} />
          </Header>
          <Content style={{ overflow: 'hidden', flex: 1 }}>
            <Routes>
              <Route path="/" element={<Room />} />
              <Route path="/settings" element={<Settings />} />
              <Route path="/about" element={<About />} />
            </Routes>
          </Content>
        </Layout>
      </Layout>
    </ConfigProvider>
  )
}

const App: React.FC = () => {
  return (
    <BrowserRouter>
      <AppProvider>
        <AppLayout />
      </AppProvider>
    </BrowserRouter>
  )
}

export default App
