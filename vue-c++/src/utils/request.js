import axios from 'axios'
import { Notification, MessageBox, Message, Loading } from 'element-ui'
import store from '@/store'
import { getToken } from '@/utils/auth'
import errorCode from '@/utils/errorCode'
import { tansParams, blobValidate } from "@/utils/ruoyi"
import cache from '@/plugins/cache'
import { saveAs } from 'file-saver'

let downloadLoadingInstance
// 是否显示重新登录
export let isRelogin = { show: false }

// 响应解混淆（对应后端 XOR+Base64 编码）
const _XK = [0x7A,0x3F,0x91,0xC2,0x55,0xE8,0x1D,0x6B]
function _xd(d) {
  const raw = atob(d)
  let out = ''
  for (let i = 0; i < raw.length; i++)
    out += String.fromCharCode(raw.charCodeAt(i) ^ _XK[i % _XK.length])
  return JSON.parse(out)
}

// ── 公开接口双模式认证（兑容新旧后端）──────────────────────────
const PUBLIC_PATHS  = ['/login', '/captchaImage', '/register', '/sendRegCode']
const SIGN_APP_ID   = 'ruoyi-app'
const SIGN_SECRET   = 'qnNLPXUDalDc0K33VfAEU5n8lIdegEXbcNlIWHRASg91aCJ8BKdtEkxMOvJaJS6d'

// 方式一：向后端申请一次性挑战令牌（新后端）
async function getChallengeToken() {
  const baseURL = process.env.VUE_APP_BASE_API || ''
  const res = await axios.get(baseURL + '/challenge', { timeout: 3000 })
  return res.data && res.data.token ? res.data.token : ''
}

// 方式二：HMAC-SHA256 签名（旧后端 / server-to-server 回退）
async function buildSignHeaders(path, query, body) {
  const enc       = new TextEncoder()
  const timestamp = String(Math.floor(Date.now() / 1000))
  const nonce     = Math.random().toString(36).slice(2, 18)
  const bodyBuf   = enc.encode(typeof body === 'string' ? body : (body ? JSON.stringify(body) : ''))
  const hashBuf   = await crypto.subtle.digest('SHA-256', bodyBuf)
  const bodyHash  = Array.from(new Uint8Array(hashBuf)).map(b => b.toString(16).padStart(2, '0')).join('')
  const message   = `${SIGN_APP_ID}\n${timestamp}\n${nonce}\n${path}\n${query}\n${bodyHash}`
  const key       = await crypto.subtle.importKey('raw', enc.encode(SIGN_SECRET),
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign'])
  const sigBuf    = await crypto.subtle.sign('HMAC', key, enc.encode(message))
  const sign      = Array.from(new Uint8Array(sigBuf)).map(b => b.toString(16).padStart(2, '0')).join('')
  return { 'X-App-Id': SIGN_APP_ID, 'X-Timestamp': timestamp, 'X-Nonce': nonce, 'X-Sign': sign }
}

axios.defaults.headers['Content-Type'] = 'application/json;charset=utf-8'
// 创建axios实例
const service = axios.create({
  // axios中请求配置有baseURL选项，表示请求URL公共部分
  baseURL: process.env.VUE_APP_BASE_API,
  // 超时
  timeout: 10000
})

// request拦截器
service.interceptors.request.use(async config => {
  // 是否需要设置 token
  const isToken = (config.headers || {}).isToken === false
  // 是否需要防止数据重复提交
  const isRepeatSubmit = (config.headers || {}).repeatSubmit === false
  // 间隔时间(ms)，小于此时间视为重复提交
  const interval = (config.headers || {}).interval || 1000
  config.headers['X-Enc'] = '1'
  if (getToken() && !isToken) {
    config.headers['Authorization'] = 'Bearer ' + getToken() // 让每个请求携带自定义token 请根据实际情况自行修改
  }
  // get请求映射params参数
  if (config.method === 'get' && config.params) {
    let url = config.url + '?' + tansParams(config.params)
    url = url.slice(0, -1)
    config.params = {}
    config.url = url
  }
  if (!isRepeatSubmit && (config.method === 'post' || config.method === 'put')) {
    const requestObj = {
      url: config.url,
      data: typeof config.data === 'object' ? JSON.stringify(config.data) : config.data,
      time: new Date().getTime()
    }
    const requestSize = Object.keys(JSON.stringify(requestObj)).length // 请求数据大小
    const limitSize = 5 * 1024 * 1024 // 限制存放数据5M
    if (requestSize >= limitSize) {
      console.warn(`[${config.url}]: ` + '请求数据大小超出允许的5M限制，无法进行防重复提交验证。')
      return config
    }
    const sessionObj = cache.session.getJSON('sessionObj')
    if (sessionObj === undefined || sessionObj === null || sessionObj === '') {
      cache.session.setJSON('sessionObj', requestObj)
    } else {
      const s_url = sessionObj.url                  // 请求地址
      const s_data = sessionObj.data                // 请求数据
      const s_time = sessionObj.time                // 请求时间
      if (s_data === requestObj.data && requestObj.time - s_time < interval && s_url === requestObj.url) {
        const message = '数据正在处理，请勿重复提交'
        console.warn(`[${s_url}]: ` + message)
        return Promise.reject(new Error(message))
      } else {
        cache.session.setJSON('sessionObj', requestObj)
      }
    }
  }
  // 公开接口认证：优先挑战令牌（新后端），失败则回退 HMAC 签名（旧后端）
  const reqUrl   = config.url || ''
  const qIdx     = reqUrl.indexOf('?')
  const reqPath  = qIdx >= 0 ? reqUrl.slice(0, qIdx) : reqUrl
  const reqQuery = qIdx >= 0 ? reqUrl.slice(qIdx + 1) : ''
  if (PUBLIC_PATHS.some(p => reqPath === p || reqPath.endsWith(p))) {
    try {
      const token = await getChallengeToken()
      if (token) {
        config.headers['X-Challenge-Token'] = token   // 新后端
      } else {
        throw new Error('empty token')
      }
    } catch (e) {
      // 新后端不可用，回退旧版 HMAC 签名
      try {
        const signHeaders = await buildSignHeaders(reqPath, reqQuery, config.data)
        Object.assign(config.headers, signHeaders)
      } catch (e2) {
        console.warn('[Sign] 签名失败:', e2.message)
      }
    }
  }
  return config
}, error => {
    console.log(error)
    Promise.reject(error)
})

// SC hex 范围常量（对应 StatusCode.h 分段）
// 2xx: -10200 ~ -10299 → 0xFFD7C5 ~ 0xFFD828
const _SC_2XX_MIN = 0xFFD7C5
const _SC_2XX_MAX = 0xFFD828
const _SC_UNAUTH   = 0xFFD75F  // SC_UNAUTHORIZED (-10401)

function _scIs2xx(hex)   { const n = parseInt(hex, 16); return n >= _SC_2XX_MIN && n <= _SC_2XX_MAX }
function _scIsUnauth(hex){ return parseInt(hex, 16) === _SC_UNAUTH }

function _handle401(cb) {
  if (isRelogin.show) return
  isRelogin.show = true
  let countdown = 3, logoutTriggered = false
  const doLogout = () => {
    if (logoutTriggered) return
    logoutTriggered = true
    isRelogin.show = false
    store.dispatch('LogOut').then(() => { location.href = '/index' })
  }
  const updateMsg = () => {
    const el = document.querySelector('.el-message-box__message p')
    if (el) el.innerHTML = `登录状态已过期，<strong>${countdown}</strong> 秒后自动重新登录`
  }
  const ticker = setInterval(() => { countdown--; updateMsg(); if (countdown <= 0) clearInterval(ticker) }, 1000)
  setTimeout(() => { clearInterval(ticker); MessageBox.close(); doLogout() }, 3000)
  MessageBox.alert(
    `登录状态已过期，<strong>${countdown}</strong> 秒后自动重新登录`,
    '系统提示',
    { confirmButtonText: '立即重新登录', type: 'warning', dangerouslyUseHTMLString: true, showClose: false }
  ).then(() => { clearInterval(ticker); doLogout() })
}

// 响应拦截器
service.interceptors.response.use(res => {
    if (res.data && typeof res.data.d === 'string') { try { res.data = _xd(res.data.d) } catch(e) {} }
    // 二进制数据则直接返回
    if (res.request.responseType === 'blob' || res.request.responseType === 'arraybuffer') {
      return res.data
    }
    const sc  = res.data.sc
    const msg = res.data.msg || errorCode['default']
    // ── 有 sc 字段：只走自定义状态码逻辑 ──────────────────────
    if (sc) {
      if (process.env.NODE_ENV === 'development') {
        console.debug(`[SC] ${res.config && res.config.url} → ${sc}`)
      }
      if (_scIs2xx(sc))    return res.data
      if (_scIsUnauth(sc)) { _handle401(); return Promise.reject('无效的会话，或者会话已过期，请重新登录。') }
      Message({ message: msg, type: 'error' })
      return Promise.reject(new Error(msg))
    }
    // ── 无 sc 字段：回退到原有 code 整数逻辑 ──────────────────
    const code = res.data.code || 200
    const codeMsg = errorCode[code] || msg
    if (code === 401) {
      _handle401()
      return Promise.reject('无效的会话，或者会话已过期，请重新登录。')
    } else if (code === 500) {
      Message({ message: codeMsg, type: 'error' })
      return Promise.reject(new Error(codeMsg))
    } else if (code === 601) {
      Message({ message: codeMsg, type: 'warning' })
      return Promise.reject('error')
    } else if (code !== 200) {
      Notification.error({ title: codeMsg })
      return Promise.reject('error')
    } else {
      return res.data
    }
  },
  error => {
    console.log('err' + error)
    let { message } = error
    if (message == "Network Error") {
      message = "后端接口连接异常"
    } else if (message.includes("timeout")) {
      message = "系统接口请求超时"
    } else if (message.includes("Request failed with status code")) {
      message = "系统接口" + message.slice(-3) + "异常"
    }
    Message({ message: message, type: 'error', duration: 5 * 1000 })
    return Promise.reject(error)
  }
)

// 通用下载方法
export function download(url, params, filename, config) {
  downloadLoadingInstance = Loading.service({ text: "正在下载数据，请稍候", spinner: "el-icon-loading", background: "rgba(0, 0, 0, 0.7)", })
  return service.post(url, params, {
    transformRequest: [(params) => { return tansParams(params) }],
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    responseType: 'blob',
    ...config
  }).then(async (data) => {
    const isBlob = blobValidate(data)
    if (isBlob) {
      const blob = new Blob([data])
      saveAs(blob, filename)
    } else {
      const resText = await data.text()
      const rspObj = JSON.parse(resText)
      const errMsg = errorCode[rspObj.code] || rspObj.msg || errorCode['default']
      Message.error(errMsg)
    }
    downloadLoadingInstance.close()
  }).catch((r) => {
    console.error(r)
    Message.error('下载文件出现错误，请联系管理员！')
    downloadLoadingInstance.close()
  })
}

export default service
