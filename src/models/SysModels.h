#pragma once
#include <string>
#include <vector>
#include <optional>
#include <json/json.h>

// ========== 基础实体 ==========

struct BaseFields {
    std::string createBy;
    std::string createTime;
    std::string updateBy;
    std::string updateTime;
};

// ========== sys_user 用户�?==========
struct SysUser : BaseFields {
    long userId = 0;
    long deptId = 0;
    std::string userName;
    std::string nickName;
    std::string email;
    std::string phonenumber;
    std::string sex;        // 0=�?1=�?2=未知
    std::string avatar;
    std::string password;
    std::string status;     // 0=正常 1=停用
    std::string delFlag;    // 0=存在 2=删除
    std::string loginIp;
    std::string loginDate;
    std::string remark;
    // 关联（非数据库字段）
    std::string deptName;
    std::vector<long> roleIds;
    std::vector<long> postIds;
};

// ========== sys_role 角色�?==========
struct SysRole : BaseFields {
    long roleId = 0;
    std::string roleName;
    std::string roleKey;    // admin / common
    int roleSort = 0;
    std::string dataScope;  // 1=全部 2=自定�?3=本部�?4=本部门及以下 5=仅本�?    bool menuCheckStrictly = true;
    bool deptCheckStrictly = true;
    std::string status;
    std::string delFlag;
    std::string remark;
    std::vector<long> menuIds;
    std::vector<long> deptIds;
};

// ========== sys_menu 菜单权限�?==========
struct SysMenu : BaseFields {
    long menuId = 0;
    std::string menuName;
    long parentId = 0;
    int orderNum = 0;
    std::string path;
    std::string component;
    std::string query;
    std::string isFrame;    // 0=�?1=�?    std::string isCache;    // 0=缓存 1=不缓�?    std::string menuType;   // M=目录 C=菜单 F=按钮
    std::string visible;    // 0=显示 1=隐藏
    std::string status;
    std::string perms;
    std::string icon;
    std::vector<SysMenu> children;
};

// ========== sys_dept 部门�?==========
struct SysDept : BaseFields {
    long deptId = 0;
    long parentId = 0;
    std::string ancestors;
    std::string deptName;
    int orderNum = 0;
    std::string leader;
    std::string phone;
    std::string email;
    std::string status;
    std::string delFlag;
    std::string parentName;
    std::vector<SysDept> children;
};

// ========== sys_post 岗位�?==========
struct SysPost : BaseFields {
    long postId = 0;
    std::string postCode;
    std::string postName;
    int postSort = 0;
    std::string status;
    std::string remark;
};

// ========== sys_config 参数配置�?==========
struct SysConfig : BaseFields {
    int configId = 0;
    std::string configName;
    std::string configKey;
    std::string configValue;
    std::string configType;  // Y=系统内置 N=�?    std::string remark;
};

// ========== sys_dict_type 字典类型�?==========
struct SysDictType : BaseFields {
    long dictId = 0;
    std::string dictName;
    std::string dictType;
    std::string status;
    std::string remark;
};

// ========== sys_dict_data 字典数据�?==========
struct SysDictData : BaseFields {
    long dictCode = 0;
    int dictSort = 0;
    std::string dictLabel;
    std::string dictValue;
    std::string dictType;
    std::string cssClass;
    std::string listClass;
    std::string isDefault;  // Y / N
    std::string status;
    std::string remark;
};

// ========== sys_notice 通知公告�?==========
struct SysNotice : BaseFields {
    int noticeId = 0;
    std::string noticeTitle;
    std::string noticeType;    // 1=通知 2=公告
    std::string noticeContent;
    std::string status;        // 0=正常 1=关闭
};

// ========== sys_oper_log 操作日志�?==========
struct SysOperLog {
    long operId = 0;
    std::string title;
    int businessType = 0;   // 0=其它 1=新增 2=修改 3=删除
    std::string method;
    std::string requestMethod;
    int operatorType = 0;
    std::string operName;
    std::string deptName;
    std::string operUrl;
    std::string operIp;
    std::string operLocation;
    std::string operParam;
    std::string jsonResult;
    int status = 0;          // 0=正常 1=异常
    std::string errorMsg;
    std::string operTime;
    long costTime = 0;
};

// ========== sys_logininfor 登录日志�?==========
struct SysLogininfor {
    long infoId = 0;
    std::string userName;
    std::string ipaddr;
    std::string loginLocation;
    std::string browser;
    std::string os;
    std::string status;      // 0=成功 1=失败
    std::string msg;
    std::string loginTime;
};

// ========== 关联�?==========
struct SysUserRole {
    long userId = 0;
    long roleId = 0;
};

struct SysRoleMenu {
    long roleId = 0;
    long menuId = 0;
};

struct SysRoleDept {
    long roleId = 0;
    long deptId = 0;
};

struct SysUserPost {
    long userId = 0;
    long postId = 0;
};

// ========== sys_job 定时任务�?==========
struct SysJob : BaseFields {
    long jobId = 0;
    std::string jobName;
    std::string jobGroup;
    std::string invokeTarget;
    std::string cronExpression;
    std::string misfirePolicy;  // 0=默认 1=立即执行 2=执行一�?3=放弃
    std::string concurrent;     // 0=允许 1=禁止
    std::string status;         // 0=正常 1=暂停
    std::string remark;
};
