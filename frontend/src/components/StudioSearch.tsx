import { SearchField } from '@heroui/react'

export default function StudioSearch({ label, placeholder, value, onChange, clearLabel }: {
  label: string; placeholder: string; value: string; onChange: (value: string) => void; clearLabel: string
}) {
  return <SearchField className="studio-search" aria-label={label} value={value} onChange={onChange}>
    <SearchField.Group>
      <SearchField.SearchIcon />
      <SearchField.Input placeholder={placeholder} />
      <SearchField.ClearButton aria-label={clearLabel} />
    </SearchField.Group>
  </SearchField>
}
