import { Description, Label, ListBox, Select } from '@heroui/react'

type Option = { id: string; label: string; description?: string }

export default function StudioSelect({ label, visibleLabel, value, options, onChange, className = '', compact = false }: {
  label: string
  visibleLabel?: string
  value: string
  options: Option[]
  onChange: (value: string) => void
  className?: string
  compact?: boolean
}) {
  return <Select className={'studio-select ' + className} aria-label={label} value={value} onChange={key => { if (key !== null) onChange(String(key)) }}>
    {visibleLabel && <Label>{visibleLabel}</Label>}
    <Select.Trigger><Select.Value /><Select.Indicator /></Select.Trigger>
    <Select.Popover className={'studio-select-popover' + (compact ? ' is-compact' : '')} placement="bottom start">
      <ListBox items={options}>{option => <ListBox.Item id={option.id} textValue={option.label}>
        <div className="studio-select-option"><Label>{option.label}</Label>{option.description && <Description>{option.description}</Description>}</div>
        <ListBox.ItemIndicator />
      </ListBox.Item>}</ListBox>
    </Select.Popover>
  </Select>
}
