import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig(({ command, mode }) => { 
  const env = loadEnv(mode, process.cwd(), '')
  
  return {
    plugins: [react()],
    define: {
      'process.env.VITE_CONTROL_FLOW_GRAPH_DATA': JSON.stringify(env.VITE_CONTROL_FLOW_GRAPH_DATA)
  },

  }
})
