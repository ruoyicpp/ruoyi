<template>
  <div id="app">
    <router-view />
    <theme-picker />
  </div>
</template>

<script>
import ThemePicker from "@/components/ThemePicker"
import { MessageBox } from 'element-ui'
import request from '@/utils/request'

export default {
  name: "App",
  components: { ThemePicker },
  data() { return { _ws: null, _wsTimer: null } },
  watch: {
    // token 变化时重新连接（登录/注销）
    '$store.getters.token': {
      immediate: true,
      handler(token) {
        this._closeWs()
        if (token) this._connectWs(token)
      }
    }
  },
  beforeDestroy() { this._closeWs() },
  methods: {
    async _connectWs(token) {
      const proto = location.protocol === 'https:' ? 'wss' : 'ws'
      let param
      try {
        const res = await request({ url: '/ws/ticket', method: 'get' })
        param = 'ticket=' + encodeURIComponent(res.data.ticket)
      } catch (_) {
        param = 'token=' + encodeURIComponent(token)
      }
      const url = proto + '://' + location.host + '/ws/notify?' + param
      const ws  = new WebSocket(url)
      this._ws  = ws

      ws.onmessage = ({ data }) => {
        try {
          const msg = JSON.parse(data)
          if (msg.type === 'kick') {
            ws.close()
            this._ws = null
            this.$store.dispatch('LogOut').finally(() => {
              MessageBox.alert(msg.msg || '您已被强制下线', '系统通知', {
                type: 'warning',
                callback: () => { this.$router.replace('/login') }
              })
            })
          }
        } catch (_) {}
      }

      // 每 30s 发送 ping 保活
      this._wsTimer = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) ws.send('ping')
      }, 30000)

      ws.onclose = () => { clearInterval(this._wsTimer); this._ws = null }
      ws.onerror = () => { ws.close() }
    },
    _closeWs() {
      clearInterval(this._wsTimer)
      if (this._ws) { this._ws.close(); this._ws = null }
    }
  }
}
</script>
<style scoped>
#app .theme-picker {
  display: none;
}
</style>
