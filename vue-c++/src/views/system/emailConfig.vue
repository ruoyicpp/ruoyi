<template>
  <div class="app-container">
    <el-card class="box-card">
      <div slot="header"><b>邮件发件箱配置</b>
        <span style="font-size:12px;color:#999;margin-left:12px">系统将轮询使用下方发件人列表发送邮件</span>
      </div>

      <!-- SMTP 基础设置 -->
      <el-form :model="cfg" label-width="100px" size="small" style="max-width:600px">
        <el-form-item label="SMTP 主机">
          <el-input v-model="cfg.host" placeholder="smtp.qq.com" />
        </el-form-item>
        <el-form-item label="端口">
          <el-input v-model="cfg.port" placeholder="465（SSL）" style="width:160px" />
          <span style="color:#999;margin-left:8px">QQ邮箱: 465 | 163邮箱: 465</span>
        </el-form-item>
        <el-form-item label="发件人名称">
          <el-input v-model="cfg.fromName" placeholder="系统通知" style="width:240px" />
        </el-form-item>
      </el-form>

      <!-- 发件人列表 -->
      <el-divider content-position="left">发件人列表（轮询）</el-divider>

      <el-table :data="cfg.senders" border size="small" style="margin-bottom:12px">
        <el-table-column label="邮箱" prop="email" min-width="200">
          <template slot-scope="scope">
            <el-input v-model="scope.row.email" placeholder="xxx@qq.com" size="mini" />
          </template>
        </el-table-column>
        <el-table-column label="授权码 / 密码" prop="authCode" min-width="200">
          <template slot-scope="scope">
            <el-input v-model="scope.row.authCode" type="password"
              :placeholder="scope.row.masked ? '（已保存，不显示）' : '授权码（非登录密码）'"
              show-password size="mini"
              @focus="scope.row.masked && (scope.row.authCode = '')" />
          </template>
        </el-table-column>
        <el-table-column label="操作" width="80" align="center">
          <template slot-scope="scope">
            <el-button type="danger" size="mini" icon="el-icon-delete"
              circle @click="removeSender(scope.$index)" />
          </template>
        </el-table-column>
      </el-table>

      <el-button size="small" icon="el-icon-plus" @click="addSender">添加发件人</el-button>

      <el-divider />

      <!-- 操作按钮 -->
      <el-row :gutter="12">
        <el-col :span="24">
          <el-button type="primary" size="small" icon="el-icon-check"
            :loading="saving" @click="doSave">保存配置</el-button>
          <el-button size="small" icon="el-icon-message"
            :loading="testing" @click="showTestDialog = true">发送测试邮件</el-button>
          <el-button size="small" icon="el-icon-refresh" @click="loadConfig">刷新</el-button>
        </el-col>
      </el-row>

      <!-- QQ邮箱获取授权码提示 -->
      <el-alert style="margin-top:16px" type="info" :closable="false" show-icon>
        <div slot="title">QQ邮箱授权码获取方式</div>
        <div>登录 QQ 邮箱 → 设置 → 账户 → POP3/SMTP 服务 → 开启 → 发短信验证 → 获得授权码。
          <b>注意：填授权码，不是 QQ 密码。</b></div>
      </el-alert>
    </el-card>

    <!-- 测试发送 Dialog -->
    <el-dialog title="发送测试邮件" :visible.sync="showTestDialog" width="420px">
      <el-form label-width="80px" size="small">
        <el-form-item label="收件邮箱">
          <el-input v-model="testTo" placeholder="输入收件邮箱地址" />
        </el-form-item>
      </el-form>
      <div slot="footer">
        <el-button size="small" @click="showTestDialog = false">取消</el-button>
        <el-button type="primary" size="small" :loading="testing" @click="doTest">发送</el-button>
      </div>
    </el-dialog>
  </div>
</template>

<script>
import request from '@/utils/request'

const API = '/system/emailConfig'

export default {
  name: 'EmailConfig',
  data() {
    return {
      cfg: { host: 'smtp.qq.com', port: '465', fromName: '系统通知', senders: [] },
      saving: false,
      testing: false,
      showTestDialog: false,
      testTo: ''
    }
  },
  created() { this.loadConfig() },
  methods: {
    loadConfig() {
      request({ url: API, method: 'get' }).then(res => {
        this.cfg = { ...this.cfg, ...res.data }
        if (!Array.isArray(this.cfg.senders)) this.cfg.senders = []
      })
    },
    addSender() {
      this.cfg.senders.push({ email: '', authCode: '', masked: false })
    },
    removeSender(idx) {
      this.cfg.senders.splice(idx, 1)
    },
    doSave() {
      if (!this.cfg.host) { this.$message.error('请填写 SMTP 主机'); return }
      const senders = this.cfg.senders.filter(s => s.email)
      this.saving = true
      request({ url: API, method: 'post', data: { ...this.cfg, senders } })
        .then(() => {
          this.$message.success('保存成功')
          this.loadConfig()
        })
        .catch(e => this.$message.error(e.msg || '保存失败'))
        .finally(() => { this.saving = false })
    },
    doTest() {
      if (!this.testTo) { this.$message.error('请输入收件邮箱'); return }
      this.testing = true
      request({ url: API + '/test', method: 'post', data: { to: this.testTo } })
        .then(res => {
          this.$message.success(res.msg || '发送成功，请查收')
          this.showTestDialog = false
        })
        .catch(e => this.$message.error(e.msg || '发送失败'))
        .finally(() => { this.testing = false })
    }
  }
}
</script>
