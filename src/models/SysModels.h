/*
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <json/json.h>

// ========== 闁糕晞娅ｉ、鍛偓鍦仒缂嶏拷 ==========

struct BaseFields {
    std::string createBy;
    std::string createTime;
    std::string updateBy;
    std::string updateTime;
};

// ========== sys_user 闁烩偓鍔嶉崺娑氭偘閿燂拷 ==========
struct SysUser : BaseFields {
    long userId = 0;
    long deptId = 0;
    std::string userName;
    std::string nickName;
    std::string email;
    std::string phonenumber;
    std::string sex;        // 0=闁跨噦鎷�?1=闁跨噦鎷�?2=闁哄牜浜為悡锟�
    std::string avatar;
    std::string password;
    std::string status;     // 0=婵繐绲介悥锟� 1=闁稿绮庨弫锟�
    std::string delFlag;    // 0=閻庢稒锚濠€锟� 2=闁告帞濞€濞咃拷
    std::string loginIp;
    std::string loginDate;
    std::string remark;
    // 闁稿繐鐤囨禒鍫ユ晬閸儲濮滈柡浣哄瀹撲焦鎯旈幘宕囨憻婵炲牏顣槐锟�
    std::string deptName;
    std::vector<long> roleIds;
    std::vector<long> postIds;
};

// ========== sys_role 閻熸瑦甯熸竟濠勬偘閿燂拷 ==========
struct SysRole : BaseFields {
    long roleId = 0;
    std::string roleName;
    std::string roleKey;    // admin / common
    int roleSort = 0;
    std::string dataScope;  // 1=闁稿繈鍔戦崕锟� 2=闁煎浜滈悾楣冩晸閿燂拷?3=闁哄牜鍓熼崕鎾晸閿燂拷?4=闁哄牜鍓熼崕鎾⒒閵娿儱鎸ゅù鐘劙缁楋拷 5=濞寸姴鎳忓﹢浼存晸閿燂拷?    bool menuCheckStrictly = true;
    bool deptCheckStrictly = true;
    std::string status;
    std::string delFlag;
    std::string remark;
    std::vector<long> menuIds;
    std::vector<long> deptIds;
};

// ========== sys_menu 闁兼寧绮屽畷鐔煎级閸愵喗顎欓悶娑虫嫹 ==========
struct SysMenu : BaseFields {
    long menuId = 0;
    std::string menuName;
    long parentId = 0;
    int orderNum = 0;
    std::string path;
    std::string component;
    std::string query;
    std::string isFrame;    // 0=闁跨噦鎷�?1=闁跨噦鎷�?    std::string isCache;    // 0=缂傚倹鎸搁悺锟� 1=濞戞挸绉剁槐锕傛晸閿燂拷?    std::string menuType;   // M=闁烩晩鍠栫紞锟� C=闁兼寧绮屽畷锟� F=闁圭ǹ顦甸幐锟�
    std::string visible;    // 0=闁哄嫬澧介妵锟� 1=闂傚懏鍔樺Λ锟�
    std::string status;
    std::string perms;
    std::string icon;
    std::vector<SysMenu> children;
};

// ========== sys_dept 闂侇喓鍔戝Λ顒傛偘閿燂拷 ==========
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

// ========== sys_post 鐎光偓濡炲墽绉撮悶娑虫嫹 ==========
struct SysPost : BaseFields {
    long postId = 0;
    std::string postCode;
    std::string postName;
    int postSort = 0;
    std::string status;
    std::string remark;
};

// ========== sys_config 闁告瑥鍊归弳鐔兼煀瀹ュ洨鏋傞悶娑虫嫹 ==========
struct SysConfig : BaseFields {
    int configId = 0;
    std::string configName;
    std::string configKey;
    std::string configValue;
    std::string configType;  // Y=缂侇垵宕电划娲礃閸涱垳鏋� N=闁跨噦鎷�?    std::string remark;
};

// ========== sys_dict_type 閻庢稒顨呴崥鈧紒顐ヮ嚙閻庨鎮伴敓锟� ==========
struct SysDictType : BaseFields {
    long dictId = 0;
    std::string dictName;
    std::string dictType;
    std::string status;
    std::string remark;
};

// ========== sys_dict_data 閻庢稒顨呴崥鈧柡浣哄瀹撲胶鎮伴敓锟� ==========
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

// ========== sys_notice 闂侇偅姘ㄩ悡锟犲礂椤掆偓閹诧紕鎮伴敓锟� ==========
struct SysNotice : BaseFields {
    int noticeId = 0;
    std::string noticeTitle;
    std::string noticeType;    // 1=闂侇偅姘ㄩ悡锟� 2=闁稿浚鍓欓幉锟�
    std::string noticeContent;
    std::string status;        // 0=婵繐绲介悥锟� 1=闁稿繑濞婂Λ锟�
};

// ========== sys_oper_log 闁瑰灝绉崇紞鏃堝籍閵夈儳绠堕悶娑虫嫹 ==========
struct SysOperLog {
    long operId = 0;
    std::string title;
    int businessType = 0;   // 0=闁稿繗娉涢悾锟� 1=闁哄倹婢橀·锟� 2=濞ｅ浂鍠楅弫锟� 3=闁告帞濞€濞咃拷
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
    int status = 0;          // 0=婵繐绲介悥锟� 1=鐎殿喖鍊搁悥锟�
    std::string errorMsg;
    std::string operTime;
    long costTime = 0;
};

// ========== sys_logininfor 闁谎嗩嚙缂嶅秹寮妷銉х閻炴冻鎷� ==========
struct SysLogininfor {
    long infoId = 0;
    std::string userName;
    std::string ipaddr;
    std::string loginLocation;
    std::string browser;
    std::string os;
    std::string status;      // 0=闁瑰瓨鍔曟慨锟� 1=濠㈡儼绮剧憴锟�
    std::string msg;
    std::string loginTime;
};

// ========== 闁稿繐鐤囨禒鍫㈡偘閿燂拷 ==========
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

// ========== sys_job 閻庤纰嶅鍌涚鐠囨彃顫ら悶娑虫嫹 ==========
struct SysJob : BaseFields {
    long jobId = 0;
    std::string jobName;
    std::string jobGroup;
    std::string invokeTarget;
    std::string cronExpression;
    std::string misfirePolicy;  // 0=濮掓稒枪椤擄拷 1=缂佹柨顑呭畵鍡涘箥瑜戦、锟� 2=闁圭瑳鍡╂斀濞戞挴鍋撻柨鐕傛嫹?3=闁衡偓閹呯＞
    std::string concurrent;     // 0=闁稿繋娴囬锟� 1=缂佸倷鐒﹂锟�
    std::string status;         // 0=婵繐绲介悥锟� 1=闁哄棗鍊告禒锟�
    std::string remark;
};
*/

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <json/json.h>

// ========== 公共字段 ==========

struct BaseFields {
    std::string createBy;
    std::string createTime;
    std::string updateBy;
    std::string updateTime;
};

// ========== sys_user 用户表 ==========
struct SysUser : BaseFields {
    long userId = 0;
    long deptId = 0;
    std::string userName;
    std::string nickName;
    std::string email;
    std::string phonenumber;
    std::string sex;        // 0=未知 1=男 2=女
    std::string avatar;
    std::string password;
    std::string status;     // 0=正常 1=停用
    std::string delFlag;    // 0=正常 2=删除
    std::string loginIp;
    std::string loginDate;
    std::string remark;
    // 附加字段
    std::string deptName;
    std::vector<long> roleIds;
    std::vector<long> postIds;
};

// ========== sys_role 角色表 ==========
struct SysRole : BaseFields {
    long roleId = 0;
    std::string roleName;
    std::string roleKey;    // admin / common
    int roleSort = 0;
    std::string dataScope;  // 1=全部数据权限 2=自定数据权限 3=本部门数据权限 4=本部门及以下数据权限 5=仅本人数据权限
    bool menuCheckStrictly = true;
    bool deptCheckStrictly = true;
    std::string status;
    std::string delFlag;
    std::string remark;
    std::vector<long> menuIds;
    std::vector<long> deptIds;
};

// ========== sys_menu 菜单权限表 ==========
struct SysMenu : BaseFields {
    long menuId = 0;
    std::string menuName;
    long parentId = 0;
    int orderNum = 0;
    std::string path;
    std::string component;
    std::string query;
    std::string isFrame;    // 0=否 1=是
    std::string isCache;    // 0=不缓存 1=缓存
    std::string menuType;   // M=目录 C=菜单 F=按钮
    std::string visible;    // 0=显示 1=隐藏
    std::string status;
    std::string perms;
    std::string icon;
    std::vector<SysMenu> children;
};

// ========== sys_dept 部门表 ==========
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

// ========== sys_post 岗位信息表 ==========
struct SysPost : BaseFields {
    long postId = 0;
    std::string postCode;
    std::string postName;
    int postSort = 0;
    std::string status;
    std::string remark;
};

// ========== sys_config 参数配置表 ==========
struct SysConfig : BaseFields {
    int configId = 0;
    std::string configName;
    std::string configKey;
    std::string configValue;
    std::string configType;  // Y=系统内置 N=否
    std::string remark;
};

// ========== sys_dict_type 字典类型表 ==========
struct SysDictType : BaseFields {
    long dictId = 0;
    std::string dictName;
    std::string dictType;
    std::string status;
    std::string remark;
};

// ========== sys_dict_data 字典数据表 ==========
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

// ========== sys_notice 通知公告表 ==========
struct SysNotice : BaseFields {
    int noticeId = 0;
    std::string noticeTitle;
    std::string noticeType;    // 1=通知 2=公告
    std::string noticeContent;
    std::string status;        // 0=正常 1=关闭
};

// ========== sys_oper_log 操作日志记录 ==========
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

// ========== sys_logininfor 登录日志表 ==========
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

// ========== 关联表 ==========
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

// ========== sys_job 定时任务调度表 ==========
struct SysJob : BaseFields {
    long jobId = 0;
    std::string jobName;
    std::string jobGroup;
    std::string invokeTarget;
    std::string cronExpression;
    std::string misfirePolicy;  // 0=默认 1=立即执行 2=执行一次 3=放弃执行
    std::string concurrent;     // 0=允许 1=禁止
    std::string status;         // 0=正常 1=暂停
    std::string remark;
};