# 数据库设计文档

## ER 图

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│    sys_user     │     │   sys_role      │     │   sys_menu      │
├─────────────────┤     ├─────────────────┤     ├─────────────────┤
│ user_id (PK)    │     │ role_id (PK)    │     │ menu_id (PK)    │
│ dept_id (FK)───┼─────┼│ role_name       │     │ parent_id (FK)──┼──┐
│ user_name       │     │ role_key        │     │ menu_name       │  │
│ nick_name       │     │ role_sort       │     │ order_num       │  │
│ user_type       │     │ data_scope      │     │ path            │  │
│ password        │     │ status          │     │ component       │  │
│ email           │     │ del_flag        │     │ menu_type       │  │
│ phonenumber     │     │ create_time     │     │ visible         │  │
│ sex             │     └────────┬────────┘     │ status          │  │
│ avatar          │              │              │ perms           │  │
│ status          │              │              │ icon            │  │
│ del_flag        │     ┌────────▼────────┐   └─────────────────┘  │
│ login_ip        │     │ sys_user_role   │            ▲            │
│ login_date      │     ├─────────────────┤            │            │
│ create_by       │     │ user_id (PK,FK)│            │            │
│ create_time     │     │ role_id (PK,FK)│            │            │
│ update_by       │     └─────────────────┘            │            │
│ update_time     │              │              ┌──────┴────────┐  │
└─────────────────┘              │              │ sys_role_menu │  │
        │                        │              ├───────────────┤  │
        │                        └──────────────│ role_id(PK,FK)│  │
        │                                       │ menu_id (FK)  │  │
        │                                       └───────────────┘  │
        │                                                 ▲        │
        │                                                 │        │
        ▼                                                 │        │
┌─────────────────┐                                       │        │
│   sys_dept      │                                       │        │
├─────────────────┤                                       │        │
│ dept_id (PK)    │                                       │        │
│ parent_id (FK)──┼───────────────────────────────────────┘        │
│ ancestors        │                                                │
│ dept_name        │                                                │
│ order_num        │                                                │
│ leader           │                                                │
│ phone            │                                                │
│ email            │                                                │
│ status           │                                                │
│ del_flag         │                                                │
│ create_by        │                                                │
│ create_time      │                                                │
│ update_by        │                                                │
│ update_time      │                                                │
└─────────────────┘
```

---

## 数据表详解

### 1. sys_user 用户表

| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | BIGSERIAL | 用户ID（主键） |
| dept_id | BIGINT | 所属部门ID（外键） |
| user_name | VARCHAR(30) | 用户名 |
| nick_name | VARCHAR(30) | 昵称 |
| user_type | VARCHAR(2) | 用户类型（00系统用户） |
| email | VARCHAR(50) | 邮箱 |
| phonenumber | VARCHAR(11) | 手机号 |
| sex | CHAR(1) | 性别（0男 1女 2未知） |
| avatar | VARCHAR(100) | 头像 |
| password | VARCHAR(200) | 密码（PBKDF2-SHA256/SM3哈希） |
| status | CHAR(1) | 状态（0正常 1停用） |
| del_flag | CHAR(1) | 删除标志（0存在 2删除） |
| login_ip | VARCHAR(128) | 最后登录IP |
| login_date | TIMESTAMP | 最后登录时间 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |
| remark | VARCHAR(500) | 备注 |

**索引**：
- `idx_user_name` ON (user_name)
- `idx_status` ON (status)

**约束**：
- UNIQUE (user_name)
- UNIQUE (email)
- UNIQUE (phonenumber)

---

### 2. sys_role 角色表

| 字段 | 类型 | 说明 |
|------|------|------|
| role_id | BIGSERIAL | 角色ID（主键） |
| role_name | VARCHAR(30) | 角色名称 |
| role_key | VARCHAR(100) | 角色权限字符串 |
| role_sort | INT | 显示顺序 |
| data_scope | CHAR(1) | 数据范围（1全部 2本部门...） |
| menu_check_strictly | BOOLEAN | 菜单树严格匹配 |
| dept_check_strictly | BOOLEAN | 部门树严格匹配 |
| status | CHAR(1) | 状态（0正常 1停用） |
| del_flag | CHAR(1) | 删除标志 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |
| remark | VARCHAR(500) | 备注 |

**索引**：
- `idx_role_key` ON (role_key)

**约束**：
- UNIQUE (role_name)
- UNIQUE (role_key)

---

### 3. sys_menu 菜单权限表

| 字段 | 类型 | 说明 |
|------|------|------|
| menu_id | BIGSERIAL | 菜单ID（主键） |
| menu_name | VARCHAR(50) | 菜单名称 |
| parent_id | BIGINT | 父菜单ID |
| order_num | INT | 显示顺序 |
| path | VARCHAR(200) | 路由地址 |
| component | VARCHAR(255) | 组件路径 |
| query | VARCHAR(255) | 路由参数 |
| is_frame | INT | 是否外链（0是 1否） |
| is_cache | INT | 是否缓存（0缓存 1不缓存） |
| menu_type | CHAR(1) | 菜单类型（M目录 C菜单 F按钮） |
| visible | CHAR(1) | 可见状态（0显示 1隐藏） |
| status | CHAR(1) | 状态（0正常 1停用） |
| perms | VARCHAR(100) | 权限字符串 |
| icon | VARCHAR(100) | 菜单图标 |

**索引**：
- `idx_parent_id` ON (parent_id)
- `idx_status` ON (status)

---

### 4. sys_dept 部门表

| 字段 | 类型 | 说明 |
|------|------|------|
| dept_id | BIGSERIAL | 部门ID（主键） |
| parent_id | BIGINT | 父部门ID |
| ancestors | VARCHAR(50) | 祖级列表 |
| dept_name | VARCHAR(30) | 部门名称 |
| order_num | INT | 显示顺序 |
| leader | VARCHAR(20) | 负责人 |
| phone | VARCHAR(11) | 联系电话 |
| email | VARCHAR(50) | 邮箱 |
| status | CHAR(1) | 状态（0正常 1停用） |
| del_flag | CHAR(1) | 删除标志 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |

**索引**：
- `idx_parent_id` ON (parent_id)

---

### 5. sys_user_role 用户和角色关联表

| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | BIGINT | 用户ID（联合主键、外键） |
| role_id | BIGINT | 角色ID（联合主键、外键） |

**约束**：
- PRIMARY KEY (user_id, role_id)

---

### 6. sys_role_menu 角色和菜单关联表

| 字段 | 类型 | 说明 |
|------|------|------|
| role_id | BIGINT | 角色ID（联合主键、外键） |
| menu_id | BIGINT | 菜单ID（联合主键、外键） |

**约束**：
- PRIMARY KEY (role_id, menu_id)

---

## 其他系统表

### 7. sys_post 岗位表

| 字段 | 类型 | 说明 |
|------|------|------|
| post_id | BIGSERIAL | 岗位ID |
| post_code | VARCHAR(64) | 岗位编码 |
| post_name | VARCHAR(50) | 岗位名称 |
| post_sort | INT | 显示顺序 |
| status | CHAR(1) | 状态 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |

---

### 8. sys_config 系统配置表

| 字段 | 类型 | 说明 |
|------|------|------|
| config_id | BIGSERIAL | 配置ID |
| config_name | VARCHAR(100) | 配置名称 |
| config_key | VARCHAR(100) | 配置键名 |
| config_value | VARCHAR(500) | 配置值 |
| config_type | CHAR(1) | 系统内置（Y是 N否） |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |

---

### 9. sys_dict_type 字典类型表

| 字段 | 类型 | 说明 |
|------|------|------|
| dict_id | BIGSERIAL | 字典ID |
| dict_name | VARCHAR(100) | 字典名称 |
| dict_type | VARCHAR(100) | 字典类型 |
| status | CHAR(1) | 状态 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |

**约束**：
- UNIQUE (dict_type)

---

### 10. sys_dict_data 字典数据表

| 字段 | 类型 | 说明 |
|------|------|------|
| dict_code | BIGSERIAL | 字典编码 |
| dict_sort | INT | 字典排序 |
| dict_label | VARCHAR(100) | 字典标签 |
| dict_value | VARCHAR(100) | 字典键值 |
| dict_type | VARCHAR(100) | 字典类型 |
| css_class | VARCHAR(100) | 样式属性 |
| list_class | VARCHAR(100) | 表格回显样式 |
| is_default | CHAR(1) | 是否默认 |
| status | CHAR(1) | 状态 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |

---

### 11. sys_notice 通知公告表

| 字段 | 类型 | 说明 |
|------|------|------|
| notice_id | BIGSERIAL | 公告ID |
| notice_title | VARCHAR(50) | 公告标题 |
| notice_type | CHAR(1) | 公告类型（1通知 2公告） |
| notice_content | TEXT | 公告内容 |
| status | CHAR(1) | 公告状态（0正常 1关闭） |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |

---

### 12. sys_post 岗位表

| 字段 | 类型 | 说明 |
|------|------|------|
| post_id | BIGSERIAL | 岗位ID |
| post_code | VARCHAR(64) | 岗位编码 |
| post_name | VARCHAR(50) | 岗位名称 |
| post_sort | INT | 显示顺序 |
| status | CHAR(1) | 状态 |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |
| remark | VARCHAR(500) | 备注 |

---

### 13. sys_login_log 登录日志表

| 字段 | 类型 | 说明 |
|------|------|------|
| info_id | BIGSERIAL | 访问ID |
| user_name | VARCHAR(50) | 用户名 |
| ipaddr | VARCHAR(128) | 登录地址 |
| login_location | VARCHAR(255) | 登录地点 |
| browser | VARCHAR(50) | 浏览器 |
| os | VARCHAR(50) | 操作系统 |
| status | CHAR(1) | 登录状态（0成功 1失败） |
| msg | VARCHAR(255) | 提示消息 |
| login_date | TIMESTAMP | 登录时间 |

**索引**：
- `idx_login_date` ON (login_date)

---

### 14. sys_oper_log 操作日志表

| 字段 | 类型 | 说明 |
|------|------|------|
| oper_id | BIGSERIAL | 日志ID |
| title | VARCHAR(50) | 模块标题 |
| business_type | VARCHAR(20) | 业务类型 |
| method | VARCHAR(100) | 方法名 |
| request_method | VARCHAR(10) | 请求方式 |
| operator_type | VARCHAR(20) | 操作类别 |
| oper_name | VARCHAR(50) | 操作人员 |
| dept_name | VARCHAR(50) | 部门名称 |
| oper_url | VARCHAR(255) | 请求URL |
| oper_ip | VARCHAR(128) | 主机地址 |
| oper_location | VARCHAR(255) | 操作地点 |
| oper_param | VARCHAR(2000) | 请求参数 |
| json_result | VARCHAR(2000) | 返回参数 |
| status | INT | 操作状态（0正常 1异常） |
| error_msg | VARCHAR(2000) | 错误消息 |
| oper_time | TIMESTAMP | 操作时间 |

**索引**：
- `idx_oper_time` ON (oper_time)

---

### 15. sys_job 定时任务表

| 字段 | 类型 | 说明 |
|------|------|------|
| job_id | BIGSERIAL | 任务ID |
| job_name | VARCHAR(100) | 任务名称 |
| job_group | VARCHAR(100) | 任务组名 |
| invoke_target | VARCHAR(500) | 调用目标 |
| cron_expression | VARCHAR(255) | cron表达式 |
| misfire_policy | VARCHAR(20) | 错误策略 |
| concurrent | CHAR(1) | 是否并发 |
| status | CHAR(1) | 状态（0正常 1暂停） |
| create_by | VARCHAR(64) | 创建者 |
| create_time | TIMESTAMP | 创建时间 |
| update_by | VARCHAR(64) | 更新者 |
| update_time | TIMESTAMP | 更新时间 |
| remark | VARCHAR(500) | 备注 |

---

### 16. sys_job_log 定时任务日志表

| 字段 | 类型 | 说明 |
|------|------|------|
| job_log_id | BIGSERIAL | 日志ID |
| job_name | VARCHAR(100) | 任务名称 |
| job_group | VARCHAR(100) | 任务组名 |
| invoke_target | VARCHAR(500) | 调用目标 |
| job_message | VARCHAR(500) | 日志信息 |
| status | CHAR(1) | 执行状态（0成功 1失败） |
| exception_info | VARCHAR(2000) | 异常信息 |
| start_time | TIMESTAMP | 开始时间 |
| end_time | TIMESTAMP | 结束时间 |
| create_time | TIMESTAMP | 创建时间 |

---

## 表关系说明

### 用户-角色-菜单关系

```
用户 ─────┬───── 用户角色关联表 ──── 角色
          │                                │
          │                                │
          └──────────── 部门表 ◄───────────┘
                              ▲
                              │
                           角色菜单关联表
                              │
                         菜单权限表
```

### 权限设计

1. **用户** 拥有多个 **角色**
2. **角色** 拥有多个 **菜单权限**
3. **用户** 属于一个 **部门**
4. **部门** 可以有子部门（自关联）

### 数据权限

| data_scope | 说明 |
|------------|------|
| 1 | 全部数据权限 |
| 2 | 自定义数据权限 |
| 3 | 本部门数据权限 |
| 4 | 本部门及以下数据权限 |
| 5 | 仅本人数据权限 |

---

## 初始化数据

### 超级管理员角色

```sql
INSERT INTO sys_role (role_id, role_name, role_key, role_sort, status, del_flag, create_by, create_time, remark)
VALUES (1, '超级管理员', 'admin', 1, '0', '0', 'system', NOW(), '超级管理员');
```

### 普通用户角色

```sql
INSERT INTO sys_role (role_id, role_name, role_key, role_sort, status, del_flag, create_by, create_time, remark)
VALUES (2, '普通角色', 'common', 2, '0', '0', 'system', NOW(), '普通角色');
```

### 管理员用户

```sql
INSERT INTO sys_user (user_id, user_name, nick_name, password, email, phonenumber, status, del_flag, create_by, create_time)
VALUES (1, 'admin', '管理员', '$pbkdf2-sha256$29000$...', 'admin@ruoyi.com', '15888888888', '0', '0', 'system', NOW());
```
