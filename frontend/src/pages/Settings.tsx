import React, { useState, useEffect } from 'react'
import { Form, Select, Switch, InputNumber, Card, Button, message, Divider } from 'antd'
import { api } from '../services/api'
import { AppConfig, MessageType } from '../types'

const Settings: React.FC = () => {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [loading, setLoading] = useState(false)
  const [form] = Form.useForm()

  useEffect(() => {
    api.getConfig().then(cfg => {
      setConfig(cfg)
      form.setFieldsValue(cfg)
    }).catch(err => {
      message.error('加载配置失败')
    })
  }, [])

  const handleSave = async () => {
    setLoading(true)
    try {
      const values = form.getFieldsValue()
      await api.updateConfig(values)
      message.success('配置已保存')
    } catch (err: any) {
      message.error(err.message || '保存失败')
    } finally {
      setLoading(false)
    }
  }

  if (!config) return null

  return (
    <div style={{ padding: 24, maxWidth: 600 }}>
      <Card title="连接设置">
        <Form form={form} layout="vertical">
          <Form.Item label="心跳间隔（秒）" name={['capture', 'heartbeat_interval']}>
            <InputNumber min={1} max={60} />
          </Form.Item>
          <Form.Item label="重连间隔（秒）" name={['capture', 'reconnect_interval']}>
            <InputNumber min={1} max={60} />
          </Form.Item>
          <Form.Item label="最大重连次数" name={['capture', 'max_reconnect_attempts']}>
            <InputNumber min={1} max={100} />
          </Form.Item>
        </Form>
      </Card>

      <Divider />

      <Card title="通知设置">
        <Form form={form} layout="vertical">
          <Form.Item label="触发通知的消息类型" name={['notification', 'trigger_types']}>
            <Select
              mode="multiple"
              placeholder="选择触发通知的消息类型"
              options={[
                { value: 'chat', label: '弹幕' },
                { value: 'gift', label: '礼物' },
                { value: 'enter', label: '进场' },
                { value: 'like', label: '点赞' },
                { value: 'social', label: '关注' },
              ]}
            />
          </Form.Item>
        </Form>
      </Card>

      <Divider />

      <Card title="日志设置">
        <Form form={form} layout="vertical">
          <Form.Item label="日志级别" name={['log', 'level']}>
            <Select
              options={[
                { value: 'trace', label: 'Trace' },
                { value: 'debug', label: 'Debug' },
                { value: 'info', label: 'Info' },
                { value: 'warn', label: 'Warn' },
                { value: 'error', label: 'Error' },
              ]}
            />
          </Form.Item>
        </Form>
      </Card>

      <div style={{ marginTop: 24 }}>
        <Button type="primary" onClick={handleSave} loading={loading}>
          保存配置
        </Button>
      </div>
    </div>
  )
}

export default Settings
