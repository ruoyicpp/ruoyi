<template>
  <div class="vp-root">

    <!-- 功能未启用 -->
    <div v-if="disabled" class="vp-disabled">
      <i class="el-icon-video-pause" style="font-size:56px;margin-bottom:16px" />
      <p style="font-size:16px">随机视频功能已关闭</p>
      <p style="font-size:12px;opacity:.5;margin-top:6px">
        系统管理 → 参数设置 → <b>sys.video.enabled</b> 改为 true 后刷新
      </p>
    </div>

    <template v-else>
      <!-- 播放器区域（居中） -->
      <div class="vp-center">

        <div class="vp-card">
          <!-- 视频 -->
          <div class="vp-wrap">
            <video
              ref="videoEl"
              class="vp-video"
              autoplay
              playsinline
              muted
              :src="videoUrl"
              @ended="nextVideo"
              @canplay="onCanPlay"
            />

            <!-- 加载遮罩 -->
            <transition name="fade">
              <div v-if="loading" class="vp-overlay">
                <i class="el-icon-loading" style="font-size:32px" />
                <span>加载中...</span>
              </div>
            </transition>

            <!-- 错误遮罩 -->
            <transition name="fade">
              <div v-if="error" class="vp-overlay vp-error">
                <i class="el-icon-warning-outline" style="font-size:32px" />
                <span>获取失败</span>
                <el-button size="mini" type="danger" @click="nextVideo" style="margin-top:8px">重试</el-button>
              </div>
            </transition>

            <!-- 右上角：收藏 + 静音切换 -->
            <div class="vp-badges">
              <div class="vp-badge" @click="toggleMute" :title="muted ? '取消静音' : '静音'">
                <i :class="muted ? 'el-icon-turn-off-microphone' : 'el-icon-microphone'" />
              </div>
              <div class="vp-badge" @click="toggleFav" :title="isFaved ? '取消收藏' : '收藏'">
                <i :class="isFaved ? 'el-icon-star-on vp-star-on' : 'el-icon-star-off'" />
              </div>
            </div>
          </div>

          <!-- 控制栏 -->
          <div class="vp-ctrl">
            <el-button type="danger"  icon="el-icon-video-play" @click="nextVideo">下一个</el-button>
            <el-button               icon="el-icon-download"   @click="download">下载</el-button>
            <el-button :type="isFaved ? 'warning' : ''"
                       :icon="isFaved ? 'el-icon-star-on' : 'el-icon-star-off'"
                       @click="toggleFav">
              {{ isFaved ? '已收藏' : '收藏' }}
            </el-button>
            <el-button icon="el-icon-tickets" @click="showDrawer = true">记录</el-button>
          </div>
        </div>
      </div>

      <!-- 收藏/历史 抽屉 -->
      <el-drawer
        title=""
        :visible.sync="showDrawer"
        direction="rtl"
        size="320px"
        :modal="false"
        :wrapperClosable="true"
        custom-class="vp-drawer"
      >
        <el-tabs v-model="sideTab" style="padding:0 12px">
          <el-tab-pane label="收藏" name="fav">
            <div v-if="favList.length === 0" class="vp-empty">暂无收藏</div>
            <div v-for="(url, i) in favList" :key="url" class="vp-item" @click="playUrl(url); showDrawer=false">
              <video class="vp-thumb" :src="url" muted preload="metadata" />
              <div class="vp-item-meta">
                <span class="vp-item-title">视频 {{ i + 1 }}</span>
                <div>
                  <el-button type="text" icon="el-icon-video-play" @click.stop="playUrl(url); showDrawer=false" />
                  <el-button type="text" icon="el-icon-delete"     @click.stop="removeFav(i)" />
                </div>
              </div>
            </div>
          </el-tab-pane>

          <el-tab-pane label="历史" name="hist">
            <div v-if="histList.length === 0" class="vp-empty">暂无历史</div>
            <div v-for="(url, i) in histList" :key="url+i" class="vp-item" @click="playUrl(url); showDrawer=false">
              <video class="vp-thumb" :src="url" muted preload="metadata" />
              <div class="vp-item-meta">
                <span class="vp-item-title">记录 {{ i + 1 }}</span>
                <div>
                  <el-button type="text" icon="el-icon-video-play" @click.stop="playUrl(url); showDrawer=false" />
                  <el-button type="text" icon="el-icon-delete"     @click.stop="removeHist(i)" />
                </div>
              </div>
            </div>
            <div v-if="histList.length > 0" style="text-align:center;padding:8px 0">
              <el-button type="text" size="mini" @click="clearHist">清空历史</el-button>
            </div>
          </el-tab-pane>
        </el-tabs>
      </el-drawer>

    </template>
  </div>
</template>

<script>
const STOR_FAV  = 'ruoyi_video_fav'
const STOR_HIST = 'ruoyi_video_hist'
const MAX_HIST  = 30

function load(key) { try { return JSON.parse(localStorage.getItem(key) || '[]') } catch { return [] } }
function save(key, val) { try { localStorage.setItem(key, JSON.stringify(val)) } catch {} }

export default {
  name: 'VideoPlayer',
  data() {
    return {
      disabled:   false,
      videoUrl:   '',
      loading:    false,
      error:      false,
      muted:      true,
      showDrawer: false,
      sideTab:    'fav',
      favList:    load(STOR_FAV),
      histList:   load(STOR_HIST)
    }
  },
  computed: {
    isFaved() { return this.favList.includes(this.videoUrl) }
  },
  mounted() { this.checkEnabled() },
  methods: {
    async checkEnabled() {
      try {
        const r = await fetch('/prod-api/system/config/configKey/sys.video.enabled')
        const d = await r.json()
        const val = (d.msg || d.data || '').toString().toLowerCase()
        if (val === 'false' || val === '0') { this.disabled = true; return }
      } catch {}
      this.nextVideo()
    },

    async nextVideo() {
      this.loading = true
      this.error   = false
      try {
        const res  = await fetch('/api/video/random')
        const data = await res.json()
        if (data.ok && data.url) { this.playUrl(data.url) }
        else { this.error = true; this.loading = false }
      } catch { this.error = true; this.loading = false }
    },

    playUrl(url) {
      this.videoUrl = url
      this.addHist(url)
      this.$nextTick(() => {
        const v = this.$refs.videoEl
        if (!v) return
        v.muted = this.muted
        v.load()
        v.play().catch(() => {
          // 自动播放被拦截时保持静音再播
          v.muted = true
          this.muted = true
          v.play().catch(() => {})
        })
      })
    },

    onCanPlay() { this.loading = false },

    toggleMute() {
      this.muted = !this.muted
      if (this.$refs.videoEl) this.$refs.videoEl.muted = this.muted
    },

    download() {
      if (!this.videoUrl) return
      const a = document.createElement('a')
      a.href = this.videoUrl; a.download = 'video.mp4'; a.target = '_blank'; a.click()
    },

    toggleFav() {
      if (!this.videoUrl) return
      const idx = this.favList.indexOf(this.videoUrl)
      if (idx >= 0) this.favList.splice(idx, 1)
      else this.favList.unshift(this.videoUrl)
      save(STOR_FAV, this.favList)
    },
    removeFav(i)  { this.favList.splice(i, 1);  save(STOR_FAV,  this.favList)  },
    addHist(url)  {
      const idx = this.histList.indexOf(url)
      if (idx >= 0) this.histList.splice(idx, 1)
      this.histList.unshift(url)
      if (this.histList.length > MAX_HIST) this.histList.pop()
      save(STOR_HIST, this.histList)
    },
    removeHist(i) { this.histList.splice(i, 1); save(STOR_HIST, this.histList) },
    clearHist()   { this.histList = [];          save(STOR_HIST, [])            }
  }
}
</script>

<style scoped>
.vp-root {
  min-height: calc(100vh - 84px);
  background: #0d0d0d;
  display: flex;
  align-items: flex-start;
  justify-content: center;
  padding: 32px 16px;
  box-sizing: border-box;
  color: #fff;
}
.vp-disabled {
  display: flex; flex-direction: column;
  align-items: center; justify-content: center;
  min-height: 60vh; opacity: .6; text-align: center;
}

/* 播放器居中 */
.vp-center { width: 100%; max-width: 560px; }

.vp-card {
  background: #141414;
  border-radius: 16px;
  overflow: hidden;
  box-shadow: 0 10px 50px rgba(0,0,0,.7);
}

.vp-wrap {
  position: relative;
  width: 100%;
  background: #000;
  aspect-ratio: 9/16;
  overflow: hidden;
}

.vp-video {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
}

/* 遮罩 */
.vp-overlay {
  position: absolute; inset: 0;
  display: flex; flex-direction: column;
  align-items: center; justify-content: center;
  gap: 10px;
  background: rgba(0,0,0,.65);
  font-size: 14px;
}
.vp-error { color: #f56c6c; }

/* 右上角图标组 */
.vp-badges {
  position: absolute;
  top: 12px; right: 12px;
  display: flex; gap: 8px;
}
.vp-badge {
  width: 34px; height: 34px;
  background: rgba(0,0,0,.55);
  border-radius: 50%;
  display: flex; align-items: center; justify-content: center;
  cursor: pointer; font-size: 17px; color: #ddd;
  backdrop-filter: blur(4px);
  transition: transform .15s, background .15s;
}
.vp-badge:hover { transform: scale(1.15); background: rgba(0,0,0,.8); }
.vp-star-on { color: #f0c040; }

/* 控制栏 */
.vp-ctrl {
  display: flex; gap: 10px;
  padding: 14px 16px;
  background: #1c1c1c;
  flex-wrap: wrap;
  justify-content: center;
}

/* 抽屉内容 */
.vp-empty { text-align: center; opacity: .4; padding: 30px 0; font-size: 13px; }
.vp-item {
  display: flex; align-items: center; gap: 10px;
  padding: 8px; border-radius: 8px; cursor: pointer;
  transition: background .15s;
}
.vp-item:hover { background: #2a2a2a; }
.vp-thumb {
  width: 90px; height: 56px;
  object-fit: cover; border-radius: 6px;
  background: #000; flex-shrink: 0;
}
.vp-item-meta {
  flex: 1; display: flex;
  flex-direction: column; justify-content: space-between;
}
.vp-item-title { font-size: 12px; opacity: .7; }

/* 过渡 */
.fade-enter-active, .fade-leave-active { transition: opacity .25s; }
.fade-enter, .fade-leave-to { opacity: 0; }
</style>

<style>
.vp-drawer .el-drawer__header { margin-bottom: 0; padding: 16px; }
.vp-drawer .el-drawer__body  { background: #141414; color: #fff; overflow-y: auto; }
.vp-drawer .el-tabs__nav-wrap::after { background: #333; }
.vp-drawer .el-tabs__item { color: #aaa; }
.vp-drawer .el-tabs__item.is-active { color: #fff; }
.vp-drawer .el-tabs__active-bar { background: #e94560; }
</style>
