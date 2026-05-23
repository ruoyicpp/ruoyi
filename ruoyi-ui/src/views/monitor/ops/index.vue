<template>
  <div class="app-container ops-panel">

    <!-- 顶部操作栏 -->
    <el-row type="flex" justify="space-between" align="middle" style="margin-bottom:16px">
      <el-col>
        <span class="panel-title"><i class="el-icon-monitor"></i> 系统运维面板</span>
        <span v-if="data.runtime" class="panel-sub">
          生成于 {{ generatedAt }} &nbsp;·&nbsp; 在线 {{ data.runtime.uptime_text }}
        </span>
      </el-col>
      <el-col style="text-align:right">
        <el-button size="small" icon="el-icon-refresh" :loading="loading" @click="load">刷新</el-button>
        <el-button size="small" type="warning" icon="el-icon-refresh-right" @click="confirmAction('reload')">热重载</el-button>
        <el-button size="small" type="danger"  icon="el-icon-switch-button" @click="confirmAction('restart')">重启服务</el-button>
      </el-col>
    </el-row>

    <!-- 汇总卡片 -->
    <el-row :gutter="12" class="summary-row">
      <el-col :span="6" v-for="card in summaryCards" :key="card.key">
        <div class="summary-card" :class="'tone-' + card.tone">
          <div class="sc-label">{{ card.label }}</div>
          <div class="sc-value">{{ card.value }}</div>
        </div>
      </el-col>
    </el-row>

    <el-row :gutter="16" style="margin-top:16px">

      <!-- 左栏：运行状态 + 应用信息 + 资源 -->
      <el-col :span="12">

        <!-- 运行状态 -->
        <el-card class="ops-card" shadow="never">
          <div slot="header"><i class="el-icon-cpu"></i> 运行状态</div>
          <template v-if="data.runtime">
            <info-row label="服务状态">
              <el-tag size="mini" :type="toneToType(data.runtime.tone)">{{ data.runtime.status_text }}</el-tag>
            </info-row>
            <info-row label="进程 PID"  :value="data.runtime.pid" />
            <info-row label="运行平台"  :value="data.runtime.platform" />
            <info-row label="启动时间"  :value="data.runtime.start_time" />
            <info-row label="运行时长"  :value="data.runtime.uptime_text" />
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

        <!-- 应用信息 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <div slot="header"><i class="el-icon-info"></i> 应用信息</div>
          <template v-if="data.application">
            <info-row label="名称"      :value="data.application.name" />
            <info-row label="框架"      :value="data.application.framework + ' ' + data.application.version" />
            <info-row label="操作系统"  :value="data.application.os" />
            <info-row label="IO 线程数" :value="data.application.threads" />
            <info-row label="数据库"    :value="data.application.db_backend" />
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

        <!-- 资源 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <div slot="header"><i class="el-icon-pie-chart"></i> 运行资源</div>
          <template v-if="data.resources && data.resources.length">
            <div v-for="r in data.resources" :key="r.label" class="resource-row">
              <span class="res-label">{{ r.label }}</span>
              <el-tag size="mini" :type="toneToType(r.tone)">{{ r.value }}</el-tag>
              <span class="res-helper">{{ r.helper }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="2" animated />
        </el-card>

      </el-col>

      <!-- 右栏：依赖 + 日志告警 -->
      <el-col :span="12">

        <!-- 依赖检测 -->
        <el-card class="ops-card" shadow="never">
          <div slot="header"><i class="el-icon-connection"></i> 依赖检测</div>
          <template v-if="data.dependencies && data.dependencies.length">
            <div v-for="dep in data.dependencies" :key="dep.name" class="dep-row">
              <el-tag size="mini" :type="toneToType(dep.tone)">{{ dep.status_text }}</el-tag>
              <span class="dep-name">{{ dep.name }}</span>
              <span class="dep-msg">{{ dep.message }}</span>
              <span class="dep-lat">{{ dep.latency_text }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="3" animated />
        </el-card>

        <!-- 日志告警 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <div slot="header">
            <i class="el-icon-warning-outline"></i> 日志告警
            <el-badge v-if="data.logs" :value="data.logs.error_count" :max="99"
                      :type="data.logs.error_count > 0 ? 'danger' : 'primary'" style="margin-left:8px" />
          </div>
          <template v-if="data.logs">
            <!-- 文件列表 -->
            <div v-for="f in data.logs.files" :key="f.name" class="log-file-row">
              <i :class="f.exists ? 'el-icon-document' : 'el-icon-document-delete'"></i>
              <span class="lf-name">{{ f.name }}</span>
              <span class="lf-meta">{{ f.size_text }}</span>
            </div>
            <el-divider />
            <!-- 最近告警 -->
            <div v-if="data.logs.recent_errors && data.logs.recent_errors.length">
              <div v-for="(e, i) in data.logs.recent_errors" :key="i" class="log-err-row">
                <el-tag size="mini" type="danger">{{ e.file }}</el-tag>
                <span class="log-err-msg">{{ e.message }}</span>
              </div>
            </div>
            <div v-else class="log-empty"><i class="el-icon-check"></i> 未发现异常日志</div>
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

      </el-col>
    </el-row>

    <!-- 操作确认对话框 -->
    <el-dialog :title="actionTitle" :visible.sync="actionDialog" width="400px">
      <p>{{ actionDesc }}</p>
      <el-input v-model="actionReason" placeholder="填写操作原因（可选）" size="small" />
      <span slot="footer">
        <el-button @click="actionDialog = false">取消</el-button>
        <el-button type="primary" :loading="actionLoading" @click="doAction">确认执行</el-button>
      </span>
    </el-dialog>

  </div>
</template>

<script>
import { getOpsOverview, opsReload, opsRestart } from '@/api/monitor/ops'

// 内联 InfoRow 组件（避免额外文件）
const InfoRow = {
  functional: true,
  props: ['label', 'value'],
  render (h, ctx) {
    return h('div', { class: 'info-row' }, [
      h('span', { class: 'ir-label' }, ctx.props.label),
      ctx.slots().default
        ? ctx.slots().default
        : h('span', { class: 'ir-value' }, String(ctx.props.value ?? '—'))
    ])
  }
}

export default {
  name: 'OpsPanel',
  components: { InfoRow },
  data () {
    return {
      loading: false,
      data: {},
      generatedAt: '',
      // action dialog
      actionDialog: false,
      actionType: '',
      actionReason: '',
      actionLoading: false
    }
  },
  computed: {
    summaryCards () { return this.data.summary || [] },
    actionTitle () { return this.actionType === 'reload' ? '热重载确认' : '重启服务确认' },
    actionDesc ()  {
      return this.actionType === 'reload'
        ? '热重载会重新加载代码（Linux 支持），请确认操作。'
        : '重启服务会短暂中断所有请求，请确认操作。'
    }
  },
  created () { this.load() },
  methods: {
    load () {
      this.loading = true
      getOpsOverview().then(res => {
        this.data = res.data || {}
        this.generatedAt = res.data?.generated_at || ''
      }).finally(() => { this.loading = false })
    },
    toneToType (tone) {
      const map = { success: 'success', danger: 'danger', warning: 'warning', primary: '', gray: 'info' }
      return map[tone] ?? ''
    },
    confirmAction (type) {
      this.actionType = type
      this.actionReason = ''
      this.actionDialog = true
    },
    doAction () {
      this.actionLoading = true
      const fn = this.actionType === 'reload' ? opsReload : opsRestart
      fn({ reason: this.actionReason }).then(res => {
        this.$message.success(res.msg || '指令已提交')
        this.actionDialog = false
        if (this.actionType === 'reload') setTimeout(() => this.load(), 2000)
      }).catch(err => {
        this.$message.error(err.message || '操作失败')
      }).finally(() => { this.actionLoading = false })
    }
  }
}
</script>

<style scoped>
.ops-panel { padding: 16px; }
.panel-title { font-size: 16px; font-weight: 700; margin-right: 12px; }
.panel-sub   { font-size: 12px; color: #909399; }

.summary-row { margin-bottom: 4px; }
.summary-card { padding: 14px 16px; border-radius: 6px; background: #f5f7fa; border: 1px solid #e4e7ed; }
.summary-card.tone-success { background: #f0f9eb; border-color: #b3e19d; }
.summary-card.tone-danger  { background: #fef0f0; border-color: #fbc4c4; }
.summary-card.tone-warning { background: #fdf6ec; border-color: #f5dab1; }
.sc-label { font-size: 12px; color: #909399; margin-bottom: 6px; }
.sc-value { font-size: 20px; font-weight: 700; }

.ops-card { border-radius: 8px; }
.ops-card >>> .el-card__header { padding: 10px 16px; font-size: 13px; font-weight: 600; }

.info-row  { display: flex; align-items: center; padding: 7px 0; border-bottom: 1px solid #f2f2f2; font-size: 13px; }
.ir-label  { width: 90px; color: #606266; flex-shrink: 0; }
.ir-value  { color: #303133; }

.resource-row { display: flex; align-items: center; padding: 7px 0; border-bottom: 1px solid #f2f2f2; font-size: 13px; gap: 8px; }
.res-label    { width: 90px; color: #606266; flex-shrink: 0; }
.res-helper   { color: #909399; font-size: 12px; }

.dep-row  { display: flex; align-items: center; padding: 7px 0; border-bottom: 1px solid #f2f2f2; font-size: 13px; gap: 8px; }
.dep-name { font-weight: 600; min-width: 120px; }
.dep-msg  { color: #909399; font-size: 12px; flex: 1; }
.dep-lat  { color: #409EFF; font-size: 12px; flex-shrink: 0; }

.log-file-row { display: flex; align-items: center; padding: 5px 0; font-size: 12px; gap: 6px; color: #606266; }
.lf-name { flex: 1; }
.lf-meta { color: #909399; }
.log-err-row  { display: flex; align-items: flex-start; padding: 4px 0; font-size: 12px; gap: 6px; }
.log-err-msg  { color: #606266; word-break: break-all; flex: 1; }
.log-empty    { color: #67c23a; font-size: 12px; padding: 8px 0; }
</style>
