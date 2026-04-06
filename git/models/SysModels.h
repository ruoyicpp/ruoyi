#pragma once
#include <string>
#include <vector>
#include <optional>
#include <json/json.h>

// ========== 閸╄櫣顢呯€圭偘缍� ==========

struct BaseFields {
    std::string createBy;
    std::string createTime;
    std::string updateBy;
    std::string updateTime;
};

// ========== sys_user 閻€劍鍩涚悰锟� ==========
struct SysUser : BaseFields {
    long userId = 0;
    long deptId = 0;
    std::string userName;
    std::string nickName;
    std::string email;
    std::string phonenumber;
    std::string sex;        // 0=閿燂拷?1=閿燂拷?2=閺堫亞鐓�
    std::string avatar;
    std::string password;
    std::string status;     // 0=濮濓絽鐖� 1=閸嬫粎鏁�
    std::string delFlag;    // 0=鐎涙ê婀� 2=閸掔娀娅�
    std::string loginIp;
    std::string loginDate;
    std::string remark;
    // 閸忓疇浠堥敍鍫ユ姜閺佺増宓佹惔鎾崇摟濞堢绱�
    std::string deptName;
    std::vector<long> roleIds;
    std::vector<long> postIds;
};

// ========== sys_role 鐟欐帟澹婄悰锟� ==========
struct SysRole : BaseFields {
    long roleId = 0;
    std::string roleName;
    std::string roleKey;    // admin / common
    int roleSort = 0;
    std::string dataScope;  // 1=閸忋劑鍎� 2=閼奉亜鐣鹃敓锟�?3=閺堫剟鍎撮敓锟�?4=閺堫剟鍎撮梻銊ュ挤娴犮儰绗� 5=娴犲懏婀伴敓锟�?    bool menuCheckStrictly = true;
    bool deptCheckStrictly = true;
    std::string status;
    std::string delFlag;
    std::string remark;
    std::vector<long> menuIds;
    std::vector<long> deptIds;
};

// ========== sys_menu 閼挎粌宕熼弶鍐鐞涳拷 ==========
struct SysMenu : BaseFields {
    long menuId = 0;
    std::string menuName;
    long parentId = 0;
    int orderNum = 0;
    std::string path;
    std::string component;
    std::string query;
    std::string isFrame;    // 0=閿燂拷?1=閿燂拷?    std::string isCache;    // 0=缂傛挸鐡� 1=娑撳秶绱﹂敓锟�?    std::string menuType;   // M=閻╊喖缍� C=閼挎粌宕� F=閹稿鎸�
    std::string visible;    // 0=閺勫墽銇� 1=闂呮劘妫�
    std::string status;
    std::string perms;
    std::string icon;
    std::vector<SysMenu> children;
};

// ========== sys_dept 闁劑妫悰锟� ==========
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

// ========== sys_post 瀹€妞剧秴鐞涳拷 ==========
struct SysPost : BaseFields {
    long postId = 0;
    std::string postCode;
    std::string postName;
    int postSort = 0;
    std::string status;
    std::string remark;
};

// ========== sys_config 閸欏倹鏆熼柊宥囩枂鐞涳拷 ==========
struct SysConfig : BaseFields {
    int configId = 0;
    std::string configName;
    std::string configKey;
    std::string configValue;
    std::string configType;  // Y=缁崵绮洪崘鍛枂 N=閿燂拷?    std::string remark;
};

// ========== sys_dict_type 鐎涙鍚€缁鐎风悰锟� ==========
struct SysDictType : BaseFields {
    long dictId = 0;
    std::string dictName;
    std::string dictType;
    std::string status;
    std::string remark;
};

// ========== sys_dict_data 鐎涙鍚€閺佺増宓佺悰锟� ==========
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

// ========== sys_notice 闁氨鐓￠崗顒€鎲＄悰锟� ==========
struct SysNotice : BaseFields {
    int noticeId = 0;
    std::string noticeTitle;
    std::string noticeType;    // 1=闁氨鐓� 2=閸忣剙鎲�
    std::string noticeContent;
    std::string status;        // 0=濮濓絽鐖� 1=閸忔娊妫�
};

// ========== sys_oper_log 閹垮秳缍旈弮銉ョ箶鐞涳拷 ==========
struct SysOperLog {
    long operId = 0;
    std::string title;
    int businessType = 0;   // 0=閸忚泛鐣� 1=閺傛澘顤� 2=娣囶喗鏁� 3=閸掔娀娅�
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
    int status = 0;          // 0=濮濓絽鐖� 1=瀵倸鐖�
    std::string errorMsg;
    std::string operTime;
    long costTime = 0;
};

// ========== sys_logininfor 閻ц缍嶉弮銉ョ箶鐞涳拷 ==========
struct SysLogininfor {
    long infoId = 0;
    std::string userName;
    std::string ipaddr;
    std::string loginLocation;
    std::string browser;
    std::string os;
    std::string status;      // 0=閹存劕濮� 1=婢惰精瑙�
    std::string msg;
    std::string loginTime;
};

// ========== 閸忓疇浠堢悰锟� ==========
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

// ========== sys_job 鐎规碍妞傛禒璇插鐞涳拷 ==========
struct SysJob : BaseFields {
    long jobId = 0;
    std::string jobName;
    std::string jobGroup;
    std::string invokeTarget;
    std::string cronExpression;
    std::string misfirePolicy;  // 0=姒涙ǹ顓� 1=缁斿宓嗛幍褑顢� 2=閹笛嗩攽娑撯偓閿燂拷?3=閺€鎯х磾
    std::string concurrent;     // 0=閸忎浇顔� 1=缁備焦顒�
    std::string status;         // 0=濮濓絽鐖� 1=閺嗗倸浠�
    std::string remark;
};
