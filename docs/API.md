# API 文档

> RuoYi-Cpp REST API 完整文档

## 基础信息

- **Base URL**: `http://localhost:8080`
- **认证方式**: JWT Bearer Token
- **Content-Type**: `application/json`

---

## 通用响应格式

### 成功响应

```json
{
  "code": 200,
  "msg": "操作成功",
  "data": { ... }
}
```

### 错误响应

```json
{
  "code": 401,
  "msg": "Token无效",
  "type": "TokenInvalid"
}
```

### HTTP 状态码

| 状态码 | 说明 |
|--------|------|
| 200 | 操作成功 |
| 400 | 参数错误 |
| 401 | 未认证 |
| 403 | 无权限 |
| 404 | 资源不存在 |
| 409 | 业务冲突 |
| 429 | 请求过于频繁 |
| 500 | 服务器错误 |

---

## 认证接口

### 1. 用户登录

```
POST /login
```

**请求体**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 |
| password | string | 是 | 密码 |
| code | string | 否 | 验证码 |
| uuid | string | 否 | 验证码UUID |

**响应示例**

```json
{
  "code": 200,
  "msg": "操作成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "expireTime": 604800,
    "user": {
      "userId": 1,
      "userName": "admin",
      "nickName": "管理员",
      "email": "admin@ruoyi.com",
      "phonenumber": "15888888888",
      "avatar": "/avatar.jpg",
      "deptId": 100,
      "deptName": "研发部",
      "roles": [
        { "roleId": 1, "roleName": "超级管理员" }
      ]
    }
  }
}
```

---

### 2. 用户登出

```
POST /logout
```

**请求头**

| Header | 说明 |
|--------|------|
| Authorization | Bearer {token} |

**响应示例**

```json
{
  "code": 200,
  "msg": "退出成功"
}
```

---

### 3. 获取用户信息

```
GET /getInfo
Authorization: Bearer {token}
```

**响应示例**

```json
{
  "code": 200,
  "data": {
    "user": {
      "userId": 1,
      "userName": "admin",
      "nickName": "管理员",
      "deptId": 100,
      "deptName": "研发部"
    },
    "roles": ["admin"],
    "permissions": ["*:*:*"]
  }
}
```

---

### 4. 获取路由菜单

```
GET /getRouters
Authorization: Bearer {token}
```

**响应示例**

```json
{
  "code": 200,
  "data": [
    {
      "name": "System",
      "path": "/system",
      "hidden": false,
      "redirect": "noRedirect",
      "component": "Layout",
      "meta": {
        "title": "系统管理",
        "icon": "system",
        "noCache": false
      },
      "children": [
        {
          "name": "User",
          "path": "user",
          "hidden": false,
          "component": "system/user/index",
          "meta": {
            "title": "用户管理",
            "icon": "user",
            "noCache": false
          }
        }
      ]
    }
  ]
}
```

---

### 5. 用户注册

```
POST /register
```

**请求体**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 (2-20字符) |
| password | string | 是 | 密码 (8-20字符，含大小写字母、数字、特殊字符) |
| email | string | 是 | 邮箱 |
| code | string | 否 | 邮箱验证码 |

**响应示例**

```json
{
  "code": 200,
  "msg": "注册成功",
  "data": {
    "userId": 100
  }
}
```

---

### 6. 发送注册验证码

```
POST /sendRegCode
```

**请求体**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| email | string | 是 | 邮箱地址 |

**响应示例**

```json
{
  "code": 200,
  "msg": "验证码已发送"
}
```

---

### 7. 忘记密码

```
POST /forgotPassword
```

**请求体**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 |
| email | string | 是 | 邮箱 |

**响应示例**

```json
{
  "code": 200,
  "msg": "重置链接已发送到邮箱"
}
```

---

### 8. 重置密码

```
POST /resetPassword
```

**请求体**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|
| token | string | 是 | 重置令牌 |
| password | string | 是 | 新密码 |

**响应示例**

```json
{
  "code": 200,
  "msg": "密码重置成功"
}
```

---

## 系统管理

### 用户管理 `/system/user`

#### 获取用户列表

```
GET /system/user/list
Authorization: Bearer {token}
```

**查询参数**

| 参数 | 类型 | 说明 |
|------|------|------|
| userName | string | 用户名 |
| phonenumber | string | 手机号 |
| status | string | 状态 (0正常 1停用) |
| beginTime | string | 开始时间 |
| endTime | string | 结束时间 |
| pageNum | int | 页码 |
| pageSize | int | 每页条数 |

#### 新增用户

```
POST /system/user
Authorization: Bearer {token}
```

**请求体**

```json
{
  "userName": "zhangsan",
  "nickName": "张三",
  "email": "zhangsan@example.com",
  "phonenumber": "13800138000",
  "password": "Admin@123",
  "deptId": 100,
  "roleIds": [2],
  "status": "0",
  "remark": "新用户"
}
```

#### 修改用户

```
PUT /system/user
Authorization: Bearer {token}
```

#### 删除用户

```
DELETE /system/user/{userId}
Authorization: Bearer {token}
```

#### 重置密码

```
PUT /system/user/resetPwd
Authorization: Bearer {token}
```

**请求体**

```json
{
  "userId": 1,
  "password": "Admin@123"
}
```

#### 导出用户

```
GET /system/user/export
Authorization: Bearer {token}
```

#### 导入用户

```
POST /system/user/importData
Authorization: Bearer {token}
Content-Type: multipart/form-data
```

| 字段 | 说明 |
|------|------|
| file | Excel文件 |
| updateSupport | 是否支持更新 |

---

### 角色管理 `/system/role`

#### 获取角色列表

```
GET /system/role/list
Authorization: Bearer {token}
```

#### 查询角色

```
GET /system/role/{roleId}
Authorization: Bearer {token}
```

#### 新增角色

```
POST /system/role
Authorization: Bearer {token}
```

**请求体**

```json
{
  "roleName": "普通用户",
  "roleKey": "common",
  "roleSort": 1,
  "dataScope": "1",
  "status": "0",
  "menuIds": [100, 101, 102],
  "remark": "普通用户角色"
}
```

#### 修改角色

```
PUT /system/role
Authorization: Bearer {token}
```

#### 删除角色

```
DELETE /system/role/{roleId}
Authorization: Bearer {token}
```

#### 角色数据权限修改

```
PUT /system/role/dataScope
Authorization: Bearer {token}
```

#### 角色状态修改

```
PUT /system/role/changeStatus
Authorization: Bearer {token}
```

**请求体**

```json
{
  "roleId": 2,
  "status": "0"
}
```

---

### 菜单管理 `/system/menu`

#### 获取菜单列表

```
GET /system/menu/list
Authorization: Bearer {token}
```

#### 查询菜单

```
GET /system/menu/{menuId}
Authorization: Bearer {token}
```

#### 新增菜单

```
POST /system/menu
Authorization: Bearer {token}
```

**请求体**

```json
{
  "menuName": "用户管理",
  "parentId": 0,
  "orderNum": 1,
  "path": "user",
  "component": "system/user/index",
  "menuType": "C",
  "visible": "0",
  "status": "0",
  "perms": "system:user:list",
  "icon": "user"
}
```

#### 修改菜单

```
PUT /system/menu
Authorization: Bearer {token}
```

#### 删除菜单

```
DELETE /system/menu/{menuId}
Authorization: Bearer {token}
```

---

### 部门管理 `/system/dept`

#### 获取部门列表

```
GET /system/dept/list
Authorization: Bearer {token}
```

#### 查询部门

```
GET /system/dept/{deptId}
Authorization: Bearer {token}
```

#### 新增部门

```
POST /system/dept
Authorization: Bearer {token}
```

**请求体**

```json
{
  "parentId": 0,
  "deptName": "研发部",
  "orderNum": 1,
  "leader": "张三",
  "phone": "15888888888",
  "email": "dev@example.com",
  "status": "0"
}
```

#### 修改部门

```
PUT /system/dept
Authorization: Bearer {token}
```

#### 删除部门

```
DELETE /system/dept/{deptId}
Authorization: Bearer {token}
```

---

## 系统监控

### 操作日志 `/monitor/operlog`

#### 查询操作日志

```
GET /monitor/operlog/list
Authorization: Bearer {token}
```

| 参数 | 类型 | 说明 |
|------|------|------|
| title | string | 操作模块 |
| businessType | string | 业务类型 |
| operName | string | 操作人员 |
| status | string | 操作状态 |
| beginTime | string | 开始时间 |
| endTime | string | 结束时间 |

#### 删除操作日志

```
DELETE /monitor/operlog/{operlogId}
Authorization: Bearer {token}
```

#### 清空操作日志

```
DELETE /monitor/operlog/clean
Authorization: Bearer {token}
```

---

### 登录日志 `/monitor/logininfor`

#### 查询登录日志

```
GET /monitor/logininfor/list
Authorization: Bearer {token}
```

#### 删除登录日志

```
DELETE /monitor/logininfor/{infoId}
Authorization: Bearer {token}
```

#### 解锁用户

```
UNLOCK /monitor/logininfor/unlock/{userName}
Authorization: Bearer {token}
```

#### 清空登录日志

```
DELETE /monitor/logininfor/clean
Authorization: Bearer {token}
```

---

### 在线用户 `/monitor/online`

#### 查询在线用户

```
GET /monitor/online/list
Authorization: Bearer {token}
```

#### 强制下线

```
DELETE /monitor/online/{tokenId}
Authorization: Bearer {token}
```

---

### 定时任务 `/monitor/job`

#### 获取任务列表

```
GET /monitor/job/list
Authorization: Bearer {token}
```

| 参数 | 类型 | 说明 |
|------|------|------|
| jobName | string | 任务名称 |
| jobGroup | string | 任务组名 |
| status | string | 状态 |

#### 新增任务

```
POST /monitor/job
Authorization: Bearer {token}
```

**请求体**

```json
{
  "jobName": "测试任务",
  "jobGroup": "默认",
  "invokeTarget": "ryTask.ryNoParams",
  "cronExpression": "0/10 * * * * ?",
  "misfirePolicy": "1",
  "concurrent": "1",
  "status": "0",
  "remark": "测试任务备注"
}
```

#### 修改任务

```
PUT /monitor/job
Authorization: Bearer {token}
```

#### 删除任务

```
DELETE /monitor/job/{jobId}
Authorization: Bearer {token}
```

#### 立即执行任务

```
PUT /monitor/job/run/{jobId}
Authorization: Bearer {token}
```

#### 暂停任务

```
PUT /monitor/job/pause/{jobId}
Authorization: Bearer {token}
```

#### 恢复任务

```
PUT /monitor/job/changeStatus
Authorization: Bearer {token}
```

---

### 服务监控 `/monitor/server`

#### 获取服务信息

```
GET /monitor/server
Authorization: Bearer {token}
```

**响应示例**

```json
{
  "code": 200,
  "data": {
    "cpu": {
      "core": 8,
      "used": 25.5,
      "idle": 74.5
    },
    "memory": {
      "total": 16384,
      "used": 8192,
      "available": 8192,
      "usagePercent": 50.0
    },
    "disk": [
      {
        "path": "C:",
        "total": 512000,
        "used": 256000,
        "available": 256000,
        "usagePercent": 50.0
      }
    ],
    "system": {
      "osName": "Windows 10",
      "osArch": "amd64",
      "computerName": "DESKTOP-PC",
      "uptime": 86400
    }
  }
}
```

---

### 数据源监控 `/monitor/druid`

#### 获取连接池状态

```
GET /monitor/druid
Authorization: Bearer {token}
```

---

## 运维接口

### 健康检查

```
GET /actuator/health
```

**响应示例**

```json
{
  "status": "UP",
  "components": {
    "db": { "status": "UP" },
    "redis": { "status": "UP" }
  }
}
```

---

### Prometheus 指标

```
GET /actuator/metrics
```

**响应格式**: Prometheus text format

---

### 数据库状态

```
GET /actuator/db
```

**响应示例**

```json
{
  "type": "PostgreSQL",
  "status": "connected",
  "pendingRequests": 0,
  "activeConnections": 5,
  "idleConnections": 10,
  "maxConnections": 100
}
```

---

### 热重载配置

```
POST /actuator/reload
Authorization: Bearer {token}
```

---

## OAuth2 第三方登录

### 授权跳转

```
GET /oauth2/authorize/{provider}
```

| provider | 说明 |
|----------|------|
| github | GitHub |
| google | Google |
| dingtalk | 钉钉 |
| feishu | 飞书 |
| wechat_work | 企业微信 |
| qq | QQ |

### 回调处理

```
GET /oauth2/callback/{provider}?code=xxx&state=xxx
```

### 绑定账号

```
POST /oauth2/bind/{provider}
Authorization: Bearer {token}
```

**请求体**

```json
{
  "code": "xxx"
}
```

### 解绑账号

```
DELETE /oauth2/bind/{provider}
Authorization: Bearer {token}
```

---

## 两步验证 (TOTP)

### 获取TOTP绑定信息

```
GET /system/totp/bind
Authorization: Bearer {token}
```

**响应示例**

```json
{
  "code": 200,
  "data": {
    "secret": "JBSWY3DPEHPK3PXP",
    "otpauthUri": "otpauth://totp/RuoYi-Cpp:admin?secret=JBSWY3DPEHPK3PXP...",
    "qrCode": "data:image/png;base64,..."
  }
}
```

---

### 验证TOTP

```
POST /system/totp/verify
Authorization: Bearer {token}
```

**请求体**

```json
{
  "code": "123456"
}
```

---

### 解绑TOTP

```
POST /system/totp/unbind
Authorization: Bearer {token}
```

**请求体**

```json
{
  "code": "123456"
}
```

---

## 错误码详解

| code | type | 说明 |
|------|------|------|
| 400 | ValidateError | 参数验证失败 |
| 400 | PasswordComplexityError | 密码复杂度不足 |
| 401 | Unauthorized | 未登录 |
| 401 | TokenExpired | Token过期 |
| 401 | TokenInvalid | Token无效 |
| 403 | Forbidden | 无权限访问 |
| 403 | InsufficientPermission | 权限不足 |
| 404 | NotFound | 资源不存在 |
| 409 | Conflict | 业务冲突 |
| 409 | UserExists | 用户名已存在 |
| 429 | RateLimit | 请求过于频繁 |
| 500 | DatabaseError | 数据库错误 |
| 500 | ExternalServiceError | 外部服务错误 |

---

## 版本控制

### 请求版本

通过 URL 路径指定：

```
GET /v1/system/user/list
GET /v2/system/user/list
```

或通过 Header：

```
X-API-Version: v1
Accept: application/vnd.ruoyi.v1+json
```

### 响应头

```
API-Version: v1
X-API-Latest: v2
```

---

## API 调用示例

### cURL

```bash
# 登录
curl -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}'

# 获取用户列表（带Token）
curl http://localhost:8080/system/user/list \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
```

### JavaScript (Fetch)

```javascript
// 登录
const loginRes = await fetch('/login', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ username: 'admin', password: 'admin123' })
});
const { data: { token } } = await loginRes.json();

// 获取用户列表
const usersRes = await fetch('/system/user/list', {
  headers: { 'Authorization': `Bearer ${token}` }
});
const users = await usersRes.json();
```
