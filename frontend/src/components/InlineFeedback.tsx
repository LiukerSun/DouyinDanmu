import { Alert } from '@heroui/react'
import type { ReactNode } from 'react'

export default function InlineFeedback({ children, success = false, className = '', action }: {
  children: ReactNode; success?: boolean; className?: string; action?: ReactNode
}) {
  return <Alert status={success ? 'success' : 'danger'} className={'studio-feedback ' + className} role={success ? 'status' : 'alert'}>
    <Alert.Indicator />
    <Alert.Content><Alert.Description>{children}</Alert.Description></Alert.Content>
    {action}
  </Alert>
}
