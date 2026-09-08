import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './auth/AuthApp'
import './hero.css'
import './components/StudioComponents.css'
import './components/MessageColors.css'
import '@fontsource-variable/noto-sans-sc'

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
