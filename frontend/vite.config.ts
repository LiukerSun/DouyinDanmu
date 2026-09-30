import { defineConfig, type Plugin } from 'vite'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'

// Bundle the repository changelog (outside the frontend root) as plain text.
function changelogRaw(): Plugin {
  const id = 'virtual:changelog-raw', resolved = '\0' + id
  return {
    name: 'changelog-raw',
    resolveId(source) { return source === id ? resolved : null },
    load(source) { return source === resolved ? `export default ${JSON.stringify(readFileSync(fileURLToPath(new URL('../CHANGELOG.md', import.meta.url)), 'utf8'))}` : null },
  }
}

export default defineConfig({
  plugins: [react(), tailwindcss(), changelogRaw()],
  server: {
    port: 5173,
    proxy: {
      '/api/settings/': {
        target: 'http://127.0.0.1:3000',
        changeOrigin: false,
      },
      '/api': {
        target: 'http://localhost:3000',
        changeOrigin: true,
      },
      '/ws': {
        target: 'ws://127.0.0.1:3000',
        ws: true,
      },
    },
  },
  build: {
    outDir: 'dist',
    sourcemap: false,
  },
})
