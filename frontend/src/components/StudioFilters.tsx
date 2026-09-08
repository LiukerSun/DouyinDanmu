import { ToggleButton, ToggleButtonGroup } from '@heroui/react'

export default function StudioFilters({ label, value, options, onChange, className = '' }: {
  label: string; value: string; options: { id: string; label: string }[]; onChange: (value: string) => void; className?: string
}) {
  return <ToggleButtonGroup aria-label={label} selectionMode="single" disallowEmptySelection isDetached size="sm"
    selectedKeys={new Set([value])} onSelectionChange={keys => { const next = [...keys][0]; if (next !== undefined) onChange(String(next)) }} className={className}>
    {options.map(option => <ToggleButton key={option.id} id={option.id} variant="ghost" className={'button button--ghost ' + (value === option.id ? 'is-active' : '')}>{option.label}</ToggleButton>)}
  </ToggleButtonGroup>
}
