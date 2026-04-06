import request from '@/utils/request'

export function getDashboardStats() {
  return request({ url: '/dashboard/stats', method: 'get' }).then(res => {
    const d = res.data || {}
    // 将后端 JSON 对象转换为首页卡片数组
    const cards = [
      { label: '系统用户',   value: d.totalUsers      ?? '-' },
      { label: '系统角色',   value: d.totalRoles      ?? '-' },
      { label: '在线用户',   value: d.onlineUsers      ?? '-', color: '#67c23a' },
      { label: '今日登录',   value: d.todayLogins      ?? '-', color: '#409eff' },
      { label: '今日操作',   value: d.todayOperLogs    ?? '-', color: '#e6a23c' },
      { label: 'IP 封禁',    value: d.bannedIps        ?? '-', color: d.bannedIps > 0 ? '#f56c6c' : '#909399' },
    ]
    return { ...res, data: cards }
  })
}
