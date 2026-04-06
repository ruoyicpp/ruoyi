import request from '@/utils/request'

export function queryWebsiteInfo(url) {
  return request({
    url: '/tool/websiteInfo',
    method: 'get',
    params: { url }
  })
}
