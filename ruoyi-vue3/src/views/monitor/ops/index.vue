<template>
  <div class="app-container ops-panel">

    <!-- 顶部操作栏 -->
    <el-row justify="space-between" align="middle" style="margin-bottom:16px">
      <el-col>
        <span class="panel-title"><el-icon><Monitor /></el-icon> 系统运维面板</span>
        <span v-if="data.runtime" class="panel-sub">
          生成于 {{ data.generated_at }} &nbsp;·&nbsp; 在线 {{ data.runtime?.uptime_text }}
        </span>
      </el-col>
      <el-col style="text-align:right">
        <el-button size="small" :icon="Refresh" :loading="loading" @click="load">刷新</el-button>
        <el-button size="small" type="warning" :icon="RefreshRight" @click="confirmAction('reload')">热重载</el-button>
        <el-button size="small" type="danger"  :icon="SwitchButton" @click="confirmAction('restart')">重启服务</el-button>
      </el-col>
    </el-row>

    <!-- 汇总卡片 -->
    <el-row :gutter="12" style="margin-bottom:16px">
      <el-col :span="6" v-for="card in data.summary || []" :key="card.key">
        <div class="summary-card" :class="'tone-' + card.tone">
          <div class="sc-label">{{ card.label }}</div>
          <div class="sc-value">{{ card.value }}</div>
        </div>
      </el-col>
    </el-row>

    <el-row :gutter="16">

      <!-- 左栏 -->
      <el-col :span="12">

        <!-- 运行状态 -->
        <el-card class="ops-card" shadow="never">
          <template #header><el-icon><Cpu /></el-icon> 运行状态</template>
          <template v-if="data.runtime">
            <div v-for="(val, key) in runtimeRows" :key="key" class="info-row">
              <span class="ir-label">{{ val.label }}</span>
              <el-tag v-if="val.tag" size="small" :type="toneToType(data.runtime.tone)">{{ val.value }}</el-tag>
              <span v-else class="ir-value">{{ val.value }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

        <!-- 应用信息 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <template #header><el-icon><InfoFilled /></el-icon> 应用信息</template>
          <template v-if="data.application">
            <div v-for="row in appRows" :key="row.label" class="info-row">
              <span class="ir-label">{{ row.label }}</span>
              <span class="ir-value">{{ row.value }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

        <!-- 资源 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <template #header><el-icon><PieChart /></el-icon> 运行资源</template>
          <template v-if="data.resources?.length">
            <div v-for="r in data.resources" :key="r.label" class="resource-row">
              <span class="res-label">{{ r.label }}</span>
              <el-tag size="small" :type="toneToType(r.tone)">{{ r.value }}</el-tag>
              <span class="res-helper">{{ r.helper }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="2" animated />
        </el-card>

      </el-col>

      <!-- 右栏 -->
      <el-col :span="12">

        <!-- 依赖检测 -->
        <el-card class="ops-card" shadow="never">
          <template #header><el-icon><Connection /></el-icon> 依赖检测</template>
          <template v-if="data.dependencies?.length">
            <div v-for="dep in data.dependencies" :key="dep.name" class="dep-row">
              <el-tag size="small" :type="toneToType(dep.tone)">{{ dep.status_text }}</el-tag>
              <span class="dep-name">{{ dep.name }}</span>
              <span class="dep-msg">{{ dep.message }}</span>
              <span class="dep-lat">{{ dep.latency_text }}</span>
            </div>
          </template>
          <el-skeleton v-else :rows="3" animated />
        </el-card>

        <!-- 日志告警 -->
        <el-card class="ops-card" shadow="never" style="margin-top:12px">
          <template #header>
            <el-icon><WarningFilled /></el-icon> 日志告警
            <el-badge v-if="data.logs" :value="data.logs.error_count" :max="99"
                      :type="data.logs.error_count > 0 ? 'danger' : 'primary'" style="margin-left:8px" />
          </template>
          <template v-if="data.logs">
            <div v-for="f in data.logs.files" :key="f.name" class="log-file-row">
              <el-icon><Document /></el-icon>
              <span class="lf-name">{{ f.name }}</span>
              <span class="lf-meta">{{ f.size_text }}</span>
            </div>
            <el-divider />
            <div v-if="data.logs.recent_errors?.length">
              <div v-for="(e, i) in data.logs.recent_errors" :key="i" class="log-err-row">
                <el-tag size="small" type="danger">{{ e.file }}</el-tag>
                <span class="log-err-msg">{{ e.message }}</span>
              </div>
            </div>
            <div v-else class="log-empty"><el-icon><CircleCheck /></el-icon> 未发现异常日志</div>
          </template>
          <el-skeleton v-else :rows="4" animated />
        </el-card>

      </el-col>
    </el-row>

    <!-- 操作确认对话框 -->
    <el-dialog :title="actionTitle" v-model="actionDialog" width="400px">
      <p>{{ actionDesc }}</p>
      <el-input v-model="actionReason" placeholder="填写操作原因（可选）" size="small" />
      <template #footer>
        <el-button @click="actionDialog = false">取消</el-button>
        <el-button type="primary" :loading="actionLoading" @click="doAction">确认执行</el-button>
      </template>
    </el-dialog>

  </div>
</template>

<script setup>
import { Refresh, RefreshRight, SwitchButton, Monitor, Cpu, InfoFilled, PieChart, Connection, WarningFilled, Document, CircleCheck } from '@element-plus/icons-vue'
import { getOpsOverview, opsReload, opsRestart } from '@/api/monitor/ops'

const { proxy } = getCurrentInstance()

const loading = ref(false)
const data = ref({})

const runtimeRows = computed(() => {
  const r = data.value.runtime || {}
  return [
    { label: '服务状态', value: r.status_text, tag: true },
    { label: '进程 PID',  value: r.pid },
    { label: '运行平台',  value: r.platform },
    { label: '启动时间',  value: r.start_time },
    { label: '运行时长',  value: r.uptime_text },
  ]
})

const appRows = computed(() => {
  const a = data.value.application || {}
  return [
    { label: '名称',      value: a.name },
    { label: '框架',      value: `${a.framework} ${a.version}` },
    { label: '操作系统',  value: a.os },
    { label: 'IO 线程数', value: a.threads },
    { label: '数据库',    value: a.db_backend },
  ]
})

function toneToType(tone) {
  const map = { success: 'success', danger: 'danger', warning: 'warning', primary: '', gray: 'info' }
  return map[tone] ?? ''
}

function load() {
  loading.value = true
  getOpsOverview().then(res => {
    data.value = res.data || {}
  }).finally(() => { loading.value = false })
}

// 操作对话框
const actionDialog  = ref(false)
const actionType    = ref('')
const actionReason  = ref('')
const actionLoading = ref(false)

const actionTitle = computed(() => actionType.value === 'reload' ? '热重载确认' : '重启服务确认')
const actionDesc  = computed(() =>
  actionType.value === 'reload'
    ? '热重载会重新加载代码（Linux 支持），请确认操作。'
    : '重启服务会短暂中断所有请求，请确认操作。'
)

function confirmAction(type) {
  actionType.value = type
  actionReason.value = ''
  actionDialog.value = true
}

function doAction() {
  actionLoading.value = true
  const fn = actionType.value === 'reload' ? opsReload : opsRestart
  fn({ reason: actionReason.value }).then(res => {
    proxy.$modal.msgSuccess(res.msg || '指令已提交')
    actionDialog.value = false
    if (actionType.value === 'reload') setTimeout(load, 2000)
  }).catch(err => {
    proxy.$modal.msgError(err.message || '操作失败')
  }).finally(() => { actionLoading.value = false })
}

load()
</script>

<style scoped>
.ops-panel { padding: 16px; }
.panel-title { font-size: 16px; font-weight: 700; margin-right: 12px; }
.panel-sub   { font-size: 12px; color: #909399; }

.summary-card { padding: 14px 16px; border-radius: 6px; background: #f5f7fa; border: 1px solid #e4e7ed; }
.summary-card.tone-success { background: #f0f9eb; border-color: #b3e19d; }
.summary-card.tone-danger  { background: #fef0f0; border-color: #fbc4c4; }
.summary-card.tone-warning { background: #fdf6ec; border-color: #f5dab1; }
.sc-label { font-size: 12px; color: #909399; margin-bottom: 6px; }
.sc-value { font-size: 20px; font-weight: 700; }

.ops-card { border-radius: 8px; }

.info-row  { display: flex; align-items: center; padding: 7px 0; border-bottom: 1px solid #f2f2f2; font-size: 13px; gap: 8px; }
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
.log-empty    { color: #67c23a; font-size: 12px; padding: 8px 0; display: flex; align-items: center; gap: 4px; }
</style>
