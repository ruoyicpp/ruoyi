<template>
  <div class="app-container">
    <!-- 查询输入区 -->
    <el-card class="query-card" shadow="hover">
      <div slot="header" class="card-header">
        <i class="el-icon-search" style="color:#409EFF;margin-right:8px"></i>
        <span>网站基础信息查询</span>
        <span class="sub-tip">获取目标网站的 IP、标题、图标、编码、关键词等基础信息</span>
      </div>
      <el-row :gutter="12" type="flex" align="middle">
        <el-col :span="18">
          <el-input
            v-model="queryUrl"
            placeholder="请输入网址，例如 https://www.bilibili.com/"
            clearable
            prefix-icon="el-icon-link"
            @keyup.enter.native="handleQuery"
          >
            <template slot="prepend">URL</template>
          </el-input>
        </el-col>
        <el-col :span="3">
          <el-button
            type="primary"
            icon="el-icon-search"
            :loading="loading"
            style="width:100%"
            @click="handleQuery"
          >查询</el-button>
        </el-col>
        <el-col :span="3">
          <el-button icon="el-icon-refresh" style="width:100%" @click="handleReset">重置</el-button>
        </el-col>
      </el-row>

      <!-- 快捷示例 -->
      <div class="example-chips">
        <span class="chip-label">快捷示例：</span>
        <el-tag
          v-for="ex in examples"
          :key="ex"
          size="small"
          type="info"
          class="chip"
          @click.native="useExample(ex)"
        >{{ ex }}</el-tag>
      </div>
    </el-card>

    <!-- 结果区 -->
    <div v-if="result" v-loading="loading" class="result-wrap">
      <!-- 网站头部 -->
      <el-card class="site-header-card" shadow="hover">
        <div class="site-header-inner">
          <div class="favicon-wrap">
            <img
              v-if="result.data && result.data.icon"
              :src="result.data.icon"
              class="favicon"
              @error="iconError = true"
              v-show="!iconError"
            />
            <div v-if="!result.data || !result.data.icon || iconError" class="favicon-placeholder">
              <i class="el-icon-link"></i>
            </div>
          </div>
          <div class="site-meta">
            <div class="site-title">{{ result.data && result.data.title || '（无标题）' }}</div>
            <div class="site-url">
              <el-tag type="success" size="mini">{{ result.url }}</el-tag>
            </div>
          </div>
        </div>
      </el-card>

      <!-- 基础信息网格 -->
      <el-row :gutter="16" class="info-grid">
        <el-col :xs="24" :sm="12" :md="8">
          <el-card class="info-item" shadow="hover">
            <div class="info-label"><i class="el-icon-cpu"></i> IP 地址</div>
            <div class="info-value mono">{{ result.data && result.data.ip || '—' }}</div>
          </el-card>
        </el-col>
        <el-col :xs="24" :sm="12" :md="8">
          <el-card class="info-item" shadow="hover">
            <div class="info-label"><i class="el-icon-connection"></i> 域名</div>
            <div class="info-value">{{ result.data && result.data.host || '—' }}</div>
          </el-card>
        </el-col>
        <el-col :xs="24" :sm="12" :md="8">
          <el-card class="info-item" shadow="hover">
            <div class="info-label"><i class="el-icon-document"></i> 编码</div>
            <div class="info-value mono">{{ result.data && result.data.charset || '—' }}</div>
          </el-card>
        </el-col>
      </el-row>

      <!-- 描述 / 关键词 -->
      <el-row :gutter="16" class="info-grid">
        <el-col :xs="24" :sm="12">
          <el-card class="info-item" shadow="hover">
            <div class="info-label"><i class="el-icon-chat-line-square"></i> 网站描述</div>
            <div class="info-value desc">{{ result.data && result.data.description || '（未提供）' }}</div>
          </el-card>
        </el-col>
        <el-col :xs="24" :sm="12">
          <el-card class="info-item" shadow="hover">
            <div class="info-label"><i class="el-icon-price-tag"></i> 关键词</div>
            <div class="keywords-wrap">
              <template v-if="keywords.length">
                <el-tag
                  v-for="(kw, i) in keywords"
                  :key="i"
                  size="small"
                  type="primary"
                  style="margin:3px"
                >{{ kw }}</el-tag>
              </template>
              <span v-else class="info-value">（未提供）</span>
            </div>
          </el-card>
        </el-col>
      </el-row>

      <!-- 原始 JSON -->
      <el-card class="info-item" shadow="hover">
        <div slot="header" class="raw-header">
          <span><i class="el-icon-tickets"></i> 原始返回数据</span>
          <el-button size="mini" icon="el-icon-document-copy" @click="copyRaw">复制</el-button>
        </div>
        <pre class="raw-json">{{ rawJson }}</pre>
      </el-card>
    </div>

    <!-- 空态 -->
    <el-empty v-if="!result && !loading" description="输入网址后点击查询" style="margin-top:40px" />
  </div>
</template>

<script>
import { queryWebsiteInfo } from '@/api/tool/websiteInfo'

export default {
  name: 'WebsiteInfo',
  data() {
    return {
      queryUrl: '',
      loading: false,
      result: null,
      iconError: false,
      examples: [
        'https://www.bilibili.com/',
        'https://www.baidu.com/',
        'https://github.com/',
        'https://www.taobao.com/'
      ]
    }
  },
  computed: {
    keywords() {
      const kw = this.result && this.result.data && this.result.data.keywords
      if (!kw) return []
      return kw.split(/[,，、\s]+/).map(s => s.trim()).filter(Boolean)
    },
    rawJson() {
      return JSON.stringify(this.result, null, 2)
    }
  },
  methods: {
    handleQuery() {
      if (!this.queryUrl.trim()) {
        this.$message.warning('请输入网址')
        return
      }
      if (!/^https?:\/\//i.test(this.queryUrl)) {
        this.$message.warning('URL 必须包含协议，例如 https://')
        return
      }
      this.loading = true
      this.result = null
      this.iconError = false
      queryWebsiteInfo(this.queryUrl.trim()).then(res => {
        if (res.code === 200) {
          this.result = res.data
        } else {
          this.$message.error(res.msg || '查询失败')
        }
      }).catch(() => {
        this.$message.error('请求失败，请检查网络或稍后重试')
      }).finally(() => {
        this.loading = false
      })
    },
    handleReset() {
      this.queryUrl = ''
      this.result = null
      this.iconError = false
    },
    useExample(url) {
      this.queryUrl = url
      this.handleQuery()
    },
    copyRaw() {
      navigator.clipboard.writeText(this.rawJson).then(() => {
        this.$message.success('已复制到剪贴板')
      })
    }
  }
}
</script>

<style scoped>
.query-card { margin-bottom: 20px; }
.card-header { display: flex; align-items: center; }
.sub-tip { font-size: 12px; color: #909399; margin-left: 12px; }

.example-chips { margin-top: 12px; display: flex; align-items: center; flex-wrap: wrap; gap: 6px; }
.chip-label { font-size: 13px; color: #909399; margin-right: 4px; }
.chip { cursor: pointer; transition: transform .1s; }
.chip:hover { transform: translateY(-1px); }

.result-wrap { }
.site-header-card { margin-bottom: 16px; }
.site-header-inner { display: flex; align-items: center; gap: 20px; }
.favicon-wrap { flex-shrink: 0; }
.favicon { width: 64px; height: 64px; border-radius: 12px; object-fit: contain;
  border: 1px solid #ebeef5; padding: 4px; background: #fff; }
.favicon-placeholder { width: 64px; height: 64px; border-radius: 12px;
  background: linear-gradient(135deg, #409EFF, #a855f7);
  display: flex; align-items: center; justify-content: center;
  font-size: 28px; color: #fff; }
.site-meta { min-width: 0; }
.site-title { font-size: 22px; font-weight: 700; color: #303133;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.site-url { margin-top: 6px; }

.info-grid { margin-bottom: 16px; }
.info-grid .el-col { margin-bottom: 16px; }
.info-item { height: 100%; }
.info-label { font-size: 12px; color: #909399; font-weight: 600;
  text-transform: uppercase; letter-spacing: .5px; margin-bottom: 8px; }
.info-value { font-size: 15px; color: #303133; font-weight: 500; word-break: break-all; }
.info-value.mono { font-family: "SFMono-Regular", Consolas, monospace; }
.info-value.desc { font-size: 14px; line-height: 1.6; color: #606266; }
.keywords-wrap { min-height: 32px; }

.raw-header { display: flex; justify-content: space-between; align-items: center; }
.raw-json { background: #f5f7fa; border-radius: 6px; padding: 12px 16px;
  font-size: 13px; font-family: "SFMono-Regular", Consolas, monospace;
  overflow-x: auto; color: #303133; line-height: 1.6; max-height: 300px; overflow-y: auto; }
</style>
