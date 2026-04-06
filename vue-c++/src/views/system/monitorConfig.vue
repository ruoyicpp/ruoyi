<template>
  <div class="app-container">
    <el-card class="box-card">
      <div slot="header"><b>服务器监控告警配置</b>
        <span style="font-size:12px;color:#999;margin-left:12px">内存/磁盘过高或服务崩溃时自动发送邮件告警</span>
      </div>

      <el-form :model="cfg" label-width="120px" size="small" style="max-width:560px">
        <el-form-item label="启用监控">
          <el-switch v-model="cfg.enabled" active-value="1" inactive-value="0" />
        </el-form-item>

        <el-form-item label="告警收件人">
          <el-input v-model="cfg.alertEmail" placeholder="多个邮箱用逗号分隔，如 a@qq.com,b@163.com" />
          <div style="font-size:11px;color:#999;margin-top:4px">邮件发送使用「邮件发件箱配置」里的发件人</div>
        </el-form-item>

        <el-form-item label="内存告警阈值">
          <el-input-number v-model.number="cfg.memThreshold" :min="50" :max="99" :step="5" />
          <span style="margin-left:8px;color:#666">%（超过此值发送告警）</span>
        </el-form-item>

        <el-form-item label="磁盘告警阈值">
          <el-input-number v-model.number="cfg.diskThreshold" :min="50" :max="99" :step="5" />
          <span style="margin-left:8px;color:#666">%（超过此值发送告警）</span>
        </el-form-item>

        <el-form-item label="崩溃重启告警">
          <el-switch v-model="cfg.crashAlert" active-value="1" inactive-value="0" />
          <span style="margin-left:8px;font-size:12px;color:#999">服务器异常重启时发送邮件通知</span>
        </el-form-item>
      </el-form>

      <el-divider />
      <el-row :gutter="12">
        <el-col :span="24">
          <el-button type="primary" size="small" icon="el-icon-check"
            :loading="saving" @click="doSave">保存配置</el-button>
          <el-button size="small" icon="el-icon-search"
            :loading="testing" @click="doTest">立即检测一次</el-button>
          <el-button size="small" icon="el-icon-refresh" @click="loadConfig">刷新</el-button>
        </el-col>
      </el-row>

      <el-alert style="margin-top:16px" type="info" :closable="false" show-icon>
        <div slot="title">说明</div>
        <div>每 <b>5分钟</b> 自动检测一次；每种告警 <b>1小时</b> 内只发一封，不会刷屏。<br>
          崩溃告警：服务重启时若上次不是正常关闭，则立即发送告警邮件。</div>
      </el-alert>
    </el-card>
  </div>
</template>

<script>
import request from '@/utils/request'

const API = '/system/monitorConfig'

export default {
  name: 'MonitorConfig',
  data() {
    return {
      cfg: { enabled: '1', alertEmail: '', memThreshold: 90, diskThreshold: 90, crashAlert: '1' },
      saving: false,
      testing: false
    }
  },
  created() { this.loadConfig() },
  methods: {
    loadConfig() {
      request({ url: API, method: 'get' }).then(res => {
        this.cfg = {
          ...res.data,
          memThreshold:  Number(res.data.memThreshold  || 90),
          diskThreshold: Number(res.data.diskThreshold || 90)
        }
      })
    },
    doSave() {
      this.saving = true
      request({ url: API, method: 'post', data: {
        ...this.cfg,
        memThreshold:  String(this.cfg.memThreshold),
        diskThreshold: String(this.cfg.diskThreshold)
      }})
        .then(() => this.$message.success('保存成功'))
        .catch(e => this.$message.error(e.msg || '保存失败'))
        .finally(() => { this.saving = false })
    },
    doTest() {
      this.testing = true
      request({ url: API + '/test', method: 'post' })
        .then(res => this.$message.success(res.msg || '检测已触发'))
        .catch(e => this.$message.error(e.msg || '检测失败'))
        .finally(() => { this.testing = false })
    }
  }
}
</script>
