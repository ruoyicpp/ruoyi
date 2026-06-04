/**
 * hello_plugin 前端入口
 *
 * 使用方式：
 *
 * 1. 前端按需动态 import 插件 JS（前端构建时或运行时均可）
 *
 *   // 方式 A：前端构建时打包（vue.config.js 配置 extraPluginsDir）
 *   import helloPlugin from '/plugin/hello_plugin/frontend/index.js';
 *   helloPlugin.install(app);
 *
 *   // 方式 B：运行时动态加载（推荐，无需重构建）
 *   const mod = await import('/plugin/hello_plugin/frontend/index.js');
 *   mod.install?.(app);
 *
 * 2. 前端告知后端加载插件 DLL（热激活）
 *
 *   const res = await fetch('/plugin/register/hello_plugin', { method: 'POST' });
 *   const data = await res.json();
 *   if (data.code === 200) {
 *     console.log('插件已激活：', data.data);
 *     // 后端路由 /plugin/hello/greet、/plugin/hello/echo 现在已生效
 *   }
 *
 * 3. 调用插件后端 API
 *
 *   const greeting = await fetch('/plugin/hello/greet').then(r => r.json());
 *   console.log(greeting.msg);  // "Hello from hello_plugin!"
 *
 * 4. 获取插件贡献的菜单（合并到前端路由表）
 *
 *   const menus = await fetch('/plugin/hello_plugin/menus').then(r => r.json());
 *   menus.forEach(m => router.addRoute(m));
 */

export const pluginName = 'hello_plugin';
export const version = '1.0.0';

export function install(app) {
    console.log(`[${pluginName}] v${version} installed`);
    // 在这里注册 Vue 组件、store、router 等
    // app.component('HelloPlugin', HelloPluginComponent);
}

// ── 插件提供的 API ───────────────────────────────────────────────
export async function greet() {
    const res = await fetch('/plugin/hello/greet');
    return res.json();
}

export async function echo(data) {
    const res = await fetch('/plugin/hello/echo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    });
    return res.json();
}
