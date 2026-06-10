/**
 * @file SysModels.h
 * @brief 系统数据模型 — 核心业务实体的数据结构定义
 * 
 * 功能概述：
 *   - 用户管理：用户、角色、权限、部门、岗位
 *   - 菜单权限：菜单、权限字符串
 *   - 系统配置：参数配置、字典数据
 *   - 通知公告：系统通知和公告
 *   - 日志记录：操作日志、登录日志
 *   - 任务调度：定时任务配置
 * 
 * 数据模型关系：
 *   - 用户 → 部门、角色、岗位（多对多）
 *   - 角色 → 菜单、部门（多对多）
 *   - 菜单 → 权限字符串
 * 
 * 状态码约定：
 *   - 0：正常/启用/成功
 *   - 1：停用/禁用/失败
 *   - 2：删除
 * 
 * @see DatabaseService - 数据库服务
 * @see CacheStrategy - 缓存策略
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <json/json.h>

/**
 * @struct BaseFields
 * @brief 基础字段
 * 
 * 所有系统表都包含的公共字段。
 */
struct BaseFields {
    std::string createBy;                          ///< 创建者
    std::string createTime;                        ///< 创建时间
    std::string updateBy;                          ///< 更新者
    std::string updateTime;                        ///< 更新时间
};

/**
 * @struct SysUser
 * @brief 用户表
 * 
 * 系统用户信息，包含基本信息、登录信息、关联角色和岗位。
 */
struct SysUser : BaseFields {
    long userId = 0;                               ///< 用户 ID
    long deptId = 0;                               ///< 部门 ID
    std::string userName;                          ///< 用户名
    std::string nickName;                          ///< 昵称
    std::string email;                             ///< 邮箱
    std::string phonenumber;                       ///< 电话
    std::string sex;                               ///< 性别（0=未知 1=男 2=女）
    std::string avatar;                            ///< 头像 URL
    std::string password;                          ///< 密码（加密）
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string delFlag;                           ///< 删除标志（0=正常 2=删除）
    std::string loginIp;                           ///< 最后登录 IP
    std::string loginDate;                         ///< 最后登录时间
    std::string remark;                            ///< 备注
    // 附加字段
    std::string deptName;                          ///< 部门名称（查询时附加）
    std::vector<long> roleIds;                     ///< 角色 ID 列表
    std::vector<long> postIds;                     ///< 岗位 ID 列表
};

/**
 * @struct SysRole
 * @brief 角色表
 * 
 * 系统角色定义，包含权限范围和菜单权限。
 */
struct SysRole : BaseFields {
    long roleId = 0;                               ///< 角色 ID
    std::string roleName;                          ///< 角色名称
    std::string roleKey;                           ///< 角色标识（admin/common）
    int roleSort = 0;                              ///< 排序
    std::string dataScope;                         ///< 数据权限范围（1=全部 2=自定 3=本部门 4=本部门及以下 5=仅本人）
    bool menuCheckStrictly = true;                 ///< 菜单树选择严格模式
    bool deptCheckStrictly = true;                 ///< 部门树选择严格模式
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string delFlag;                           ///< 删除标志（0=正常 2=删除）
    std::string remark;                            ///< 备注
    std::vector<long> menuIds;                     ///< 菜单 ID 列表
    std::vector<long> deptIds;                     ///< 部门 ID 列表
};

/**
 * @struct SysMenu
 * @brief 菜单权限表
 * 
 * 系统菜单和权限定义，支持树形结构。
 */
struct SysMenu : BaseFields {
    long menuId = 0;                               ///< 菜单 ID
    std::string menuName;                          ///< 菜单名称
    long parentId = 0;                             ///< 父菜单 ID
    int orderNum = 0;                              ///< 排序
    std::string path;                              ///< 路由路径
    std::string component;                         ///< 组件路径
    std::string query;                             ///< 路由参数
    std::string isFrame;                           ///< 是否外链（0=否 1=是）
    std::string isCache;                           ///< 是否缓存（0=不缓存 1=缓存）
    std::string menuType;                          ///< 菜单类型（M=目录 C=菜单 F=按钮）
    std::string visible;                           ///< 显示状态（0=显示 1=隐藏）
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string perms;                             ///< 权限字符串
    std::string icon;                              ///< 菜单图标
    std::vector<SysMenu> children;                 ///< 子菜单列表
};

/**
 * @struct SysDept
 * @brief 部门表
 * 
 * 组织部门信息，支持树形结构。
 */
struct SysDept : BaseFields {
    long deptId = 0;                               ///< 部门 ID
    long parentId = 0;                             ///< 父部门 ID
    std::string ancestors;                         ///< 祖先部门 ID 列表
    std::string deptName;                          ///< 部门名称
    int orderNum = 0;                              ///< 排序
    std::string leader;                            ///< 负责人
    std::string phone;                             ///< 电话
    std::string email;                             ///< 邮箱
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string delFlag;                           ///< 删除标志（0=正常 2=删除）
    std::string parentName;                        ///< 父部门名称（查询时附加）
    std::vector<SysDept> children;                 ///< 子部门列表
};

/**
 * @struct SysPost
 * @brief 岗位表
 * 
 * 岗位信息定义。
 */
struct SysPost : BaseFields {
    long postId = 0;                               ///< 岗位 ID
    std::string postCode;                          ///< 岗位代码
    std::string postName;                          ///< 岗位名称
    int postSort = 0;                              ///< 排序
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string remark;                            ///< 备注
};

/**
 * @struct SysConfig
 * @brief 参数配置表
 * 
 * 系统参数配置。
 */
struct SysConfig : BaseFields {
    int configId = 0;                              ///< 配置 ID
    std::string configName;                        ///< 配置名称
    std::string configKey;                         ///< 配置键
    std::string configValue;                       ///< 配置值
    std::string configType;                        ///< 配置类型（Y=系统内置 N=用户自定义）
    std::string remark;                            ///< 备注
};

/**
 * @struct SysDictType
 * @brief 字典类型表
 * 
 * 字典分类定义。
 */
struct SysDictType : BaseFields {
    long dictId = 0;                               ///< 字典 ID
    std::string dictName;                          ///< 字典名称
    std::string dictType;                          ///< 字典类型
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string remark;                            ///< 备注
};

/**
 * @struct SysDictData
 * @brief 字典数据表
 * 
 * 字典数据项。
 */
struct SysDictData : BaseFields {
    long dictCode = 0;                             ///< 字典代码 ID
    int dictSort = 0;                              ///< 排序
    std::string dictLabel;                         ///< 字典标签
    std::string dictValue;                         ///< 字典值
    std::string dictType;                          ///< 字典类型
    std::string cssClass;                          ///< CSS 样式类
    std::string listClass;                         ///< 列表样式类
    std::string isDefault;                         ///< 是否默认（Y/N）
    std::string status;                            ///< 状态（0=正常 1=停用）
    std::string remark;                            ///< 备注
};

/**
 * @struct SysNotice
 * @brief 通知公告表
 * 
 * 系统通知和公告。
 */
struct SysNotice : BaseFields {
    int noticeId = 0;                              ///< 公告 ID
    std::string noticeTitle;                       ///< 公告标题
    std::string noticeType;                        ///< 公告类型（1=通知 2=公告）
    std::string noticeContent;                     ///< 公告内容
    std::string status;                            ///< 状态（0=正常 1=关闭）
};

/**
 * @struct SysOperLog
 * @brief 操作日志表
 * 
 * 记录用户的操作日志。
 */
struct SysOperLog {
    long operId = 0;                               ///< 操作 ID
    std::string title;                             ///< 操作标题
    int businessType = 0;                          ///< 业务类型（0=其它 1=新增 2=修改 3=删除）
    std::string method;                            ///< 方法名
    std::string requestMethod;                     ///< 请求方法（GET/POST/PUT/DELETE）
    int operatorType = 0;                          ///< 操作者类型
    std::string operName;                          ///< 操作者名称
    std::string deptName;                          ///< 部门名称
    std::string operUrl;                           ///< 操作 URL
    std::string operIp;                            ///< 操作 IP
    std::string operLocation;                      ///< 操作位置
    std::string operParam;                         ///< 操作参数
    std::string jsonResult;                        ///< 返回结果
    int status = 0;                                ///< 状态（0=正常 1=异常）
    std::string errorMsg;                          ///< 错误信息
    std::string operTime;                          ///< 操作时间
    long costTime = 0;                             ///< 耗时（毫秒）
};

/**
 * @struct SysLogininfor
 * @brief 登录日志表
 * 
 * 记录用户的登录日志。
 */
struct SysLogininfor {
    long infoId = 0;                               ///< 登录 ID
    std::string userName;                          ///< 用户名
    std::string ipaddr;                            ///< 登录 IP
    std::string loginLocation;                     ///< 登录位置
    std::string browser;                           ///< 浏览器
    std::string os;                                ///< 操作系统
    std::string status;                            ///< 登录状态（0=成功 1=失败）
    std::string msg;                               ///< 登录信息
    std::string loginTime;                         ///< 登录时间
};

/**
 * @struct SysUserRole
 * @brief 用户角色关联表
 * 
 * 用户与角色的多对多关系。
 */
struct SysUserRole {
    long userId = 0;                               ///< 用户 ID
    long roleId = 0;                               ///< 角色 ID
};

/**
 * @struct SysRoleMenu
 * @brief 角色菜单关联表
 * 
 * 角色与菜单的多对多关系。
 */
struct SysRoleMenu {
    long roleId = 0;                               ///< 角色 ID
    long menuId = 0;                               ///< 菜单 ID
};

/**
 * @struct SysRoleDept
 * @brief 角色部门关联表
 * 
 * 角色与部门的多对多关系（用于数据权限）。
 */
struct SysRoleDept {
    long roleId = 0;                               ///< 角色 ID
    long deptId = 0;                               ///< 部门 ID
};

/**
 * @struct SysUserPost
 * @brief 用户岗位关联表
 * 
 * 用户与岗位的多对多关系。
 */
struct SysUserPost {
    long userId = 0;                               ///< 用户 ID
    long postId = 0;                               ///< 岗位 ID
};

/**
 * @struct SysJob
 * @brief 定时任务表
 * 
 * 定时任务调度配置。
 */
struct SysJob : BaseFields {
    long jobId = 0;                                ///< 任务 ID
    std::string jobName;                           ///< 任务名称
    std::string jobGroup;                          ///< 任务分组
    std::string invokeTarget;                      ///< 调用目标
    std::string cronExpression;                    ///< Cron 表达式
    std::string misfirePolicy;                     ///< 错过执行策略（0=默认 1=立即执行 2=执行一次 3=放弃执行）
    std::string concurrent;                        ///< 是否并发（0=允许 1=禁止）
    std::string status;                            ///< 状态（0=正常 1=暂停）
    std::string remark;                            ///< 备注
};