import Cookies from 'js-cookie'

const TokenKey = 'Admin-Token'

export function getToken() {
  return sessionStorage.getItem(TokenKey)
}

export function setToken(token) {
  Cookies.remove(TokenKey)          // 清除旧 cookie（迁移兼容）
  return sessionStorage.setItem(TokenKey, token)
}

export function removeToken() {
  Cookies.remove(TokenKey)          // 确保旧 cookie 也清除
  return sessionStorage.removeItem(TokenKey)
}
