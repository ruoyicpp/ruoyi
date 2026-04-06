<template>
  <div class="forgot-pwd">
    <el-form ref="form" :model="form" :rules="rules" class="forgot-form">
      <h3 class="title">找回密码</h3>

      <!-- Step 1: 输入邮箱 + 发送验证码 -->
      <template v-if="step === 1">
        <el-form-item prop="email">
          <el-input v-model="form.email" placeholder="请输入注册时绑定的邮箱" auto-complete="off">
            <svg-icon slot="prefix" icon-class="email" class="el-input__icon input-icon" />
          </el-input>
        </el-form-item>
        <el-form-item>
          <el-button type="primary" style="width:100%" :loading="sending" @click="sendCode">
            {{ countdown > 0 ? `重新发送 (${countdown}s)` : '发送验证码' }}
          </el-button>
        </el-form-item>
      </template>

      <!-- Step 2: 输入验证码 + 新密码 -->
      <template v-if="step === 2">
        <el-form-item>
          <el-input :value="form.email" disabled>
            <svg-icon slot="prefix" icon-class="email" class="el-input__icon input-icon" />
          </el-input>
        </el-form-item>
        <el-form-item prop="code">
          <el-input v-model="form.code" placeholder="6位验证码" auto-complete="off" maxlength="6">
            <svg-icon slot="prefix" icon-class="validCode" class="el-input__icon input-icon" />
            <el-button slot="append" :disabled="countdown > 0" @click="sendCode">
              {{ countdown > 0 ? `${countdown}s` : '重发' }}
            </el-button>
          </el-input>
        </el-form-item>
        <el-form-item prop="newPassword">
          <el-input v-model="form.newPassword" type="password" placeholder="新密码（8-20位，含大小写字母、数字和特殊字符）" show-password>
            <svg-icon slot="prefix" icon-class="password" class="el-input__icon input-icon" />
          </el-input>
        </el-form-item>
        <el-form-item prop="confirmPassword">
          <el-input v-model="form.confirmPassword" type="password" placeholder="确认新密码" show-password>
            <svg-icon slot="prefix" icon-class="password" class="el-input__icon input-icon" />
          </el-input>
        </el-form-item>
        <el-form-item>
          <el-button type="primary" style="width:100%" :loading="loading" @click="doReset">
            确认重置密码
          </el-button>
        </el-form-item>
      </template>

      <div style="text-align:right;margin-top:8px">
        <router-link class="link-type" to="/login">返回登录</router-link>
      </div>
    </el-form>

    <div class="el-forgot-footer">
      <span>验证码将记录到系统日志（配置 SMTP 后自动发邮件）</span>
    </div>
  </div>
</template>

<script>
import { forgotPassword, resetPassword } from '@/api/login'

export default {
  name: 'ForgotPassword',
  data() {
    const equalPwd = (rule, value, callback) => {
      if (value !== this.form.newPassword) callback(new Error('两次密码不一致'))
      else callback()
    }
    return {
      step: 1,
      sending: false,
      loading: false,
      countdown: 0,
      _timer: null,
      form: { email: '', code: '', newPassword: '', confirmPassword: '' },
      rules: {
        email: [
          { required: true, trigger: 'blur', message: '请输入邮箱' },
          { type: 'email', trigger: 'blur', message: '邮箱格式不正确' }
        ],
        code: [
          { required: true, trigger: 'blur', message: '请输入验证码' },
          { len: 6, trigger: 'blur', message: '验证码为6位数字' }
        ],
        newPassword: [
          { required: true, trigger: 'blur', message: '请输入新密码' },
          { min: 8, max: 20, trigger: 'blur', message: '密码长度8-20位' }
        ],
        confirmPassword: [
          { required: true, trigger: 'blur', message: '请确认新密码' },
          { validator: equalPwd, trigger: 'blur' }
        ]
      }
    }
  },
  beforeDestroy() { clearInterval(this._timer) },
  methods: {
    sendCode() {
      this.$refs.form.validateField('email', async (err) => {
        if (err) return
        if (this.countdown > 0) return
        this.sending = true
        try {
          await forgotPassword(this.form.email)
          this.$message.success('验证码已发送，请查收（15分钟内有效）')
          this.step = 2
          this.startCountdown()
        } catch (e) {
          this.$message.error(e.msg || '发送失败，请稍后重试')
        } finally {
          this.sending = false
        }
      })
    },
    startCountdown() {
      this.countdown = 60
      this._timer = setInterval(() => {
        if (--this.countdown <= 0) clearInterval(this._timer)
      }, 1000)
    },
    doReset() {
      this.$refs.form.validate(async (valid) => {
        if (!valid) return
        this.loading = true
        try {
          await resetPassword({
            email: this.form.email,
            code: this.form.code,
            newPassword: this.form.newPassword
          })
          this.$alert('密码重置成功，请重新登录', '提示', { type: 'success' })
            .finally(() => this.$router.replace('/login'))
        } catch (e) {
          this.$message.error(e.msg || '重置失败，请检查验证码是否正确')
        } finally {
          this.loading = false
        }
      })
    }
  }
}
</script>

<style scoped lang="scss">
.forgot-pwd {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100%;
  background-image: url("../assets/images/login-background.jpg");
  background-size: cover;
}
.title {
  margin: 0 auto 24px;
  text-align: center;
  color: #707070;
}
.forgot-form {
  border-radius: 6px;
  background: #fff;
  width: 400px;
  padding: 25px 25px 10px;
  .el-input { height: 38px; input { height: 38px; } }
  .input-icon { height: 39px; width: 14px; margin-left: 2px; }
}
.link-type { font-size: 13px; }
.el-forgot-footer {
  position: fixed;
  bottom: 0;
  width: 100%;
  height: 40px;
  line-height: 40px;
  text-align: center;
  color: #fff;
  font-size: 12px;
}
</style>
