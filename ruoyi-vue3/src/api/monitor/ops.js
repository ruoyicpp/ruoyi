import request from '@/utils/request'

export function getOpsOverview() {
  return request({ url: '/monitor/ops/overview', method: 'get' })
}

export function opsReload(data) {
  return request({ url: '/monitor/ops/reload', method: 'post', data })
}

export function opsRestart(data) {
  return request({ url: '/monitor/ops/restart', method: 'post', data })
}
