#include "DatabaseInit.h"

std::vector<std::string> DatabaseInit::getCreateTableSqls() {
    return {
        // -------------------------------------------------------
        // 1. sys_dept 部门表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_dept (
            dept_id     BIGSERIAL    PRIMARY KEY,
            parent_id   BIGINT       NOT NULL DEFAULT 0,
            ancestors   VARCHAR(50)  NOT NULL DEFAULT '',
            dept_name   VARCHAR(30)  NOT NULL DEFAULT '',
            order_num   INT          NOT NULL DEFAULT 0,
            leader      VARCHAR(20),
            phone       VARCHAR(11),
            email       VARCHAR(50),
            status      CHAR(1)      NOT NULL DEFAULT '0',
            del_flag    CHAR(1)      NOT NULL DEFAULT '0',
            create_by   VARCHAR(64)  NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64)  NOT NULL DEFAULT '',
            update_time TIMESTAMP
        ))",

        // -------------------------------------------------------
        // 2. sys_user 用户表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_user (
            user_id     BIGSERIAL    PRIMARY KEY,
            dept_id     BIGINT,
            user_name   VARCHAR(30)  NOT NULL DEFAULT '',
            nick_name   VARCHAR(30)  NOT NULL DEFAULT '',
            user_type   VARCHAR(2)   NOT NULL DEFAULT '00',
            email       VARCHAR(50)  NOT NULL DEFAULT '',
            phonenumber VARCHAR(11)  NOT NULL DEFAULT '',
            sex         CHAR(1)      NOT NULL DEFAULT '0',
            avatar      VARCHAR(100) NOT NULL DEFAULT '',
            password    VARCHAR(200) NOT NULL DEFAULT '',
            status      CHAR(1)      NOT NULL DEFAULT '0',
            del_flag    CHAR(1)      NOT NULL DEFAULT '0',
            login_ip    VARCHAR(128) NOT NULL DEFAULT '',
            login_date  TIMESTAMP,
            create_by   VARCHAR(64)  NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64)  NOT NULL DEFAULT '',
            update_time TIMESTAMP,
            remark      VARCHAR(500)
        ))",
        // 添加 user_type 列（如果旧表缺少）
        R"(DO $$ BEGIN
            ALTER TABLE sys_user ADD COLUMN IF NOT EXISTS user_type VARCHAR(2) NOT NULL DEFAULT '00';
        EXCEPTION WHEN others THEN NULL; END $$)",

        // -------------------------------------------------------
        // sys_role 角色表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_role (
            role_id             BIGSERIAL   PRIMARY KEY,
            role_name           VARCHAR(30) NOT NULL DEFAULT '',
            role_key            VARCHAR(100)NOT NULL DEFAULT '',
            role_sort           INT         NOT NULL DEFAULT 0,
            data_scope          CHAR(1)     NOT NULL DEFAULT '1',
            menu_check_strictly BOOLEAN     NOT NULL DEFAULT TRUE,
            dept_check_strictly BOOLEAN     NOT NULL DEFAULT TRUE,
            status              CHAR(1)     NOT NULL DEFAULT '0',
            del_flag            CHAR(1)     NOT NULL DEFAULT '0',
            create_by           VARCHAR(64) NOT NULL DEFAULT '',
            create_time         TIMESTAMP,
            update_by           VARCHAR(64) NOT NULL DEFAULT '',
            update_time         TIMESTAMP,
            remark              VARCHAR(500)
        ))",

        // -------------------------------------------------------
        // sys_menu 菜单权限表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_menu (
            menu_id     BIGSERIAL    PRIMARY KEY,
            menu_name   VARCHAR(50)  NOT NULL DEFAULT '',
            parent_id   BIGINT       NOT NULL DEFAULT 0,
            order_num   INT          NOT NULL DEFAULT 0,
            path        VARCHAR(200) NOT NULL DEFAULT '',
            component   VARCHAR(255),
            query       VARCHAR(255),
            is_frame    CHAR(1)      NOT NULL DEFAULT '1',
            is_cache    CHAR(1)      NOT NULL DEFAULT '0',
            menu_type   CHAR(1)      NOT NULL DEFAULT '',
            visible     CHAR(1)      NOT NULL DEFAULT '0',
            status      CHAR(1)      NOT NULL DEFAULT '0',
            perms       VARCHAR(100),
            icon        VARCHAR(100) NOT NULL DEFAULT '#',
            create_by   VARCHAR(64)  NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64)  NOT NULL DEFAULT '',
            update_time TIMESTAMP,
            remark      VARCHAR(500) NOT NULL DEFAULT ''
        ))",

        // -------------------------------------------------------
        // sys_user_role 用户-角色关联表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_user_role (
            user_id BIGINT NOT NULL,
            role_id BIGINT NOT NULL,
            PRIMARY KEY (user_id, role_id)
        ))",

        // -------------------------------------------------------
        // sys_role_menu 角色-菜单关联表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_role_menu (
            role_id BIGINT NOT NULL,
            menu_id BIGINT NOT NULL,
            PRIMARY KEY (role_id, menu_id)
        ))",

        // -------------------------------------------------------
        // sys_role_dept 角色-部门关联表（数据权限）
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_role_dept (
            role_id BIGINT NOT NULL,
            dept_id BIGINT NOT NULL,
            PRIMARY KEY (role_id, dept_id)
        ))",

        // -------------------------------------------------------
        // sys_post 岗位表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_post (
            post_id     BIGSERIAL   PRIMARY KEY,
            post_code   VARCHAR(64) NOT NULL DEFAULT '',
            post_name   VARCHAR(50) NOT NULL DEFAULT '',
            post_sort   INT         NOT NULL DEFAULT 0,
            status      CHAR(1)     NOT NULL DEFAULT '0',
            create_by   VARCHAR(64) NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64) NOT NULL DEFAULT '',
            update_time TIMESTAMP,
            remark      VARCHAR(500)
        ))",

        // -------------------------------------------------------
        // sys_user_post 用户-岗位关联表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_user_post (
            user_id BIGINT NOT NULL,
            post_id BIGINT NOT NULL,
            PRIMARY KEY (user_id, post_id)
        ))",

        // -------------------------------------------------------
        // sys_dict_type 字典类型表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_dict_type (
            dict_id     BIGSERIAL    PRIMARY KEY,
            dict_name   VARCHAR(100) NOT NULL DEFAULT '',
            dict_type   VARCHAR(100) NOT NULL DEFAULT '',
            status      CHAR(1)      NOT NULL DEFAULT '0',
            create_by   VARCHAR(64)  NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64)  NOT NULL DEFAULT '',
            update_time TIMESTAMP,
            remark      VARCHAR(500),
            CONSTRAINT uk_dict_type UNIQUE (dict_type)
        ))",

        // -------------------------------------------------------
        // sys_dict_data 字典数据表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_dict_data (
            dict_code   BIGSERIAL    PRIMARY KEY,
            dict_sort   INT          NOT NULL DEFAULT 0,
            dict_label  VARCHAR(100) NOT NULL DEFAULT '',
            dict_value  VARCHAR(100) NOT NULL DEFAULT '',
            dict_type   VARCHAR(100) NOT NULL DEFAULT '',
            css_class   VARCHAR(100),
            list_class  VARCHAR(100),
            is_default  CHAR(1)      NOT NULL DEFAULT 'N',
            status      CHAR(1)      NOT NULL DEFAULT '0',
            create_by   VARCHAR(64)  NOT NULL DEFAULT '',
            create_time TIMESTAMP,
            update_by   VARCHAR(64)  NOT NULL DEFAULT '',
            update_time TIMESTAMP,
            remark      VARCHAR(500)
        ))",

        // -------------------------------------------------------
        // sys_config 参数配置表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_config (
            config_id    SERIAL       PRIMARY KEY,
            config_name  VARCHAR(100) NOT NULL DEFAULT '',
            config_key   VARCHAR(100) NOT NULL DEFAULT '',
            config_value VARCHAR(500) NOT NULL DEFAULT '',
            config_type  CHAR(1)      NOT NULL DEFAULT 'N',
            create_by    VARCHAR(64)  NOT NULL DEFAULT '',
            create_time  TIMESTAMP,
            update_by    VARCHAR(64)  NOT NULL DEFAULT '',
            update_time  TIMESTAMP,
            remark       VARCHAR(500)
        ))",

        // -------------------------------------------------------
        // sys_logininfor 登录日志表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_logininfor (
            info_id        BIGSERIAL    PRIMARY KEY,
            user_name      VARCHAR(50)  NOT NULL DEFAULT '',
            ipaddr         VARCHAR(128) NOT NULL DEFAULT '',
            login_location VARCHAR(255) NOT NULL DEFAULT '',
            browser        VARCHAR(50)  NOT NULL DEFAULT '',
            os             VARCHAR(50)  NOT NULL DEFAULT '',
            status         CHAR(1)      NOT NULL DEFAULT '0',
            msg            VARCHAR(255) NOT NULL DEFAULT '',
            login_time     TIMESTAMP
        ))",

        // -------------------------------------------------------
        // sys_oper_log 操作日志表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_oper_log (
            oper_id        BIGSERIAL    PRIMARY KEY,
            title          VARCHAR(50)  NOT NULL DEFAULT '',
            business_type  INT          NOT NULL DEFAULT 0,
            method         VARCHAR(200) NOT NULL DEFAULT '',
            request_method VARCHAR(10)  NOT NULL DEFAULT '',
            operator_type  INT          NOT NULL DEFAULT 0,
            oper_name      VARCHAR(50)  NOT NULL DEFAULT '',
            dept_name      VARCHAR(50)  NOT NULL DEFAULT '',
            oper_url       VARCHAR(255) NOT NULL DEFAULT '',
            oper_ip        VARCHAR(128) NOT NULL DEFAULT '',
            oper_location  VARCHAR(255) NOT NULL DEFAULT '',
            oper_param     VARCHAR(2000)NOT NULL DEFAULT '',
            json_result    VARCHAR(2000)NOT NULL DEFAULT '',
            status         INT          NOT NULL DEFAULT 0,
            error_msg      VARCHAR(2000)NOT NULL DEFAULT '',
            oper_time      TIMESTAMP,
            cost_time      BIGINT       NOT NULL DEFAULT 0
        ))",

        // -------------------------------------------------------
        // sys_notice 通知公告表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_notice (
            notice_id      SERIAL       PRIMARY KEY,
            notice_title   VARCHAR(50)  NOT NULL DEFAULT '',
            notice_type    CHAR(1)      NOT NULL DEFAULT '',
            notice_content TEXT,
            status         CHAR(1)      NOT NULL DEFAULT '0',
            create_by      VARCHAR(64)  NOT NULL DEFAULT '',
            create_time    TIMESTAMP,
            update_by      VARCHAR(64)  NOT NULL DEFAULT '',
            update_time    TIMESTAMP,
            remark         VARCHAR(255)
        ))",

        // -------------------------------------------------------
        // sys_job 定时任务表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_job (
            job_id          BIGSERIAL    PRIMARY KEY,
            job_name        VARCHAR(64)  NOT NULL DEFAULT '',
            job_group       VARCHAR(64)  NOT NULL DEFAULT 'DEFAULT',
            invoke_target   VARCHAR(500) NOT NULL DEFAULT '',
            cron_expression VARCHAR(255) NOT NULL DEFAULT '',
            misfire_policy  VARCHAR(20)  NOT NULL DEFAULT '3',
            concurrent      CHAR(1)      NOT NULL DEFAULT '1',
            status          CHAR(1)      NOT NULL DEFAULT '0',
            create_by       VARCHAR(64)  NOT NULL DEFAULT '',
            create_time     TIMESTAMP,
            update_by       VARCHAR(64)  NOT NULL DEFAULT '',
            update_time     TIMESTAMP,
            remark          VARCHAR(500) NOT NULL DEFAULT ''
        ))",

        // -------------------------------------------------------
        // sys_job_log 定时任务日志表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS sys_job_log (
            job_log_id    BIGSERIAL    PRIMARY KEY,
            job_name      VARCHAR(64)  NOT NULL DEFAULT '',
            job_group     VARCHAR(64)  NOT NULL DEFAULT '',
            invoke_target VARCHAR(500) NOT NULL DEFAULT '',
            job_message   VARCHAR(500),
            status        CHAR(1)      NOT NULL DEFAULT '0',
            exception_info VARCHAR(2000) NOT NULL DEFAULT '',
            create_time   TIMESTAMP
        ))",

        // -------------------------------------------------------
        // gen_table 代码生成业务表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS gen_table (
            table_id         BIGSERIAL    PRIMARY KEY,
            table_name       VARCHAR(200) NOT NULL DEFAULT '',
            table_comment    VARCHAR(500) NOT NULL DEFAULT '',
            sub_table_name   VARCHAR(64),
            sub_table_fk_name VARCHAR(64),
            class_name       VARCHAR(100) NOT NULL DEFAULT '',
            tpl_category     VARCHAR(200) NOT NULL DEFAULT 'crud',
            tpl_webtype      VARCHAR(100),
            package_name     VARCHAR(100),
            module_name      VARCHAR(30),
            business_name    VARCHAR(30),
            function_name    VARCHAR(50),
            function_author  VARCHAR(50),
            gen_type         CHAR(1)      NOT NULL DEFAULT '0',
            gen_path         VARCHAR(200) NOT NULL DEFAULT '/',
            options          VARCHAR(1000),
            create_by        VARCHAR(64)  NOT NULL DEFAULT '',
            create_time      TIMESTAMP,
            update_by        VARCHAR(64)  NOT NULL DEFAULT '',
            update_time      TIMESTAMP,
            remark           VARCHAR(500)
        ))",

        // -------------------------------------------------------
        // gen_table_column 代码生成字段表
        // -------------------------------------------------------
        R"(CREATE TABLE IF NOT EXISTS gen_table_column (
            column_id      BIGSERIAL    PRIMARY KEY,
            table_id       BIGINT,
            column_name    VARCHAR(200),
            column_comment VARCHAR(500),
            column_type    VARCHAR(100),
            cpp_type       VARCHAR(500),
            cpp_field      VARCHAR(200),
            is_pk          CHAR(1),
            is_increment   CHAR(1),
            is_required    CHAR(1),
            is_insert      CHAR(1),
            is_edit        CHAR(1),
            is_list        CHAR(1),
            is_query       CHAR(1),
            query_type     VARCHAR(200) NOT NULL DEFAULT 'EQ',
            html_type      VARCHAR(200),
            dict_type      VARCHAR(200) NOT NULL DEFAULT '',
            sort           INT,
            create_by      VARCHAR(64)  NOT NULL DEFAULT '',
            create_time    TIMESTAMP,
            update_by      VARCHAR(64)  NOT NULL DEFAULT '',
            update_time    TIMESTAMP
        ))",
    };
}

std::vector<std::string> DatabaseInit::getInitDataSqls() {
    return {
        // =====================================================================
        // 0. 清理旧版本遗留的重复字典数据（旧代码每次重启都会追加）
        // =====================================================================
        R"(DELETE FROM sys_dict_data WHERE dict_code NOT IN (
            SELECT MIN(dict_code) FROM sys_dict_data GROUP BY dict_type, dict_value
        ))",

        // =====================================================================
        // 1. 部门初始数据 (与 C# RuoYi.Net 完全一致)
        // =====================================================================
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (100,0,'0','若依科技',0,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (101,100,'0,100','深圳总公司',1,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (102,100,'0,100','长沙分公司',2,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (103,101,'0,100,101','研发部门',1,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (104,101,'0,100,101','市场部门',2,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (105,101,'0,100,101','测试部门',3,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (106,101,'0,100,101','财务部门',4,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (107,101,'0,100,101','运维部门',5,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (108,102,'0,100,102','市场部门',1,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",
        R"(INSERT INTO sys_dept (dept_id,parent_id,ancestors,dept_name,order_num,leader,phone,email,status,del_flag,create_by,create_time)
           VALUES (109,102,'0,100,102','财务部门',2,'若依','15888888888','ry@qq.com','0','0','admin',NOW()) ON CONFLICT (dept_id) DO NOTHING)",

        // =====================================================================
        // 2. 用户初始数据
        // =====================================================================
        R"(INSERT INTO sys_user (user_id,dept_id,user_name,nick_name,user_type,email,phonenumber,sex,avatar,password,status,del_flag,login_ip,login_date,create_by,create_time,remark)
           VALUES (1,103,'admin','若依','00','ry@163.com','15888888888','1','',
                   '$2a$10$7JB720yubVSZvUI0rEqK/.VqGOZTH.ulu33dHOiBE8ByOhJIrdAu2','0','0','127.0.0.1',NOW(),'admin',NOW(),'管理员')
           ON CONFLICT (user_id) DO NOTHING)",
        R"(INSERT INTO sys_user (user_id,dept_id,user_name,nick_name,user_type,email,phonenumber,sex,avatar,password,status,del_flag,login_ip,login_date,create_by,create_time,remark)
           VALUES (2,105,'ry','若依','00','ry@qq.com','15666666666','1','',
                   '$2a$10$7JB720yubVSZvUI0rEqK/.VqGOZTH.ulu33dHOiBE8ByOhJIrdAu2','0','0','127.0.0.1',NOW(),'admin',NOW(),'测试员')
           ON CONFLICT (user_id) DO NOTHING)",

        // =====================================================================
        // 3. 角色初始数据
        // =====================================================================
        R"(INSERT INTO sys_role (role_id,role_name,role_key,role_sort,data_scope,menu_check_strictly,dept_check_strictly,status,del_flag,create_by,create_time,remark)
           VALUES (1,'超级管理员','admin',1,'1',TRUE,TRUE,'0','0','admin',NOW(),'超级管理员') ON CONFLICT (role_id) DO NOTHING)",
        R"(INSERT INTO sys_role (role_id,role_name,role_key,role_sort,data_scope,menu_check_strictly,dept_check_strictly,status,del_flag,create_by,create_time,remark)
           VALUES (2,'普通角色','common',2,'2',TRUE,TRUE,'0','0','admin',NOW(),'普通角色') ON CONFLICT (role_id) DO NOTHING)",

        // =====================================================================
        // 4. 菜单初始数据 — 一级菜单
        // =====================================================================
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1,'系统管理',0,1,'system',NULL,'','1','0','M','0','0','','system','admin',NOW(),'系统管理目录') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (2,'系统监控',0,2,'monitor',NULL,'','1','0','M','0','0','','monitor','admin',NOW(),'系统监控目录') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (3,'系统工具',0,3,'tool',NULL,'','1','0','M','0','0','','tool','admin',NOW(),'系统工具目录') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (4,'若依官网',0,4,'http://ruoyi.vip',NULL,'','0','0','M','0','0','','guide','admin',NOW(),'若依官网地址') ON CONFLICT (menu_id) DO NOTHING)",

        // 二级菜单
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (100,'用户管理',1,1,'user','system/user/index','','1','0','C','0','0','system:user:list','user','admin',NOW(),'用户管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (101,'角色管理',1,2,'role','system/role/index','','1','0','C','0','0','system:role:list','peoples','admin',NOW(),'角色管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (102,'菜单管理',1,3,'menu','system/menu/index','','1','0','C','0','0','system:menu:list','tree-table','admin',NOW(),'菜单管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (103,'部门管理',1,4,'dept','system/dept/index','','1','0','C','0','0','system:dept:list','tree','admin',NOW(),'部门管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (104,'岗位管理',1,5,'post','system/post/index','','1','0','C','0','0','system:post:list','post','admin',NOW(),'岗位管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (105,'字典管理',1,6,'dict','system/dict/index','','1','0','C','0','0','system:dict:list','dict','admin',NOW(),'字典管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (106,'参数设置',1,7,'config','system/config/index','','1','0','C','0','0','system:config:list','edit','admin',NOW(),'参数设置菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (107,'通知公告',1,8,'notice','system/notice/index','','1','0','C','0','0','system:notice:list','message','admin',NOW(),'通知公告菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (108,'日志管理',1,9,'log','','','1','0','M','0','0','','log','admin',NOW(),'日志管理菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (109,'在线用户',2,1,'online','monitor/online/index','','1','0','C','0','0','monitor:online:list','online','admin',NOW(),'在线用户菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (110,'定时任务',2,2,'job','monitor/job/index','','1','0','C','0','0','monitor:job:list','job','admin',NOW(),'定时任务菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (111,'数据监控',2,3,'druid','monitor/druid/index','','1','0','C','0','0','monitor:druid:list','druid','admin',NOW(),'数据监控菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (112,'服务监控',2,4,'server','monitor/server/index','','1','0','C','0','0','monitor:server:list','server','admin',NOW(),'服务监控菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (113,'缓存监控',2,5,'cache','monitor/cache/index','','1','0','C','0','0','monitor:cache:list','redis','admin',NOW(),'缓存监控菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (114,'缓存列表',2,6,'cacheList','monitor/cache/list','','1','0','C','0','0','monitor:cache:list','redis-list','admin',NOW(),'缓存列表菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (115,'表单构建',3,1,'build','tool/build/index','','1','0','C','0','0','tool:build:list','build','admin',NOW(),'表单构建菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (116,'代码生成',3,2,'gen','tool/gen/index','','1','0','C','0','0','tool:gen:list','code','admin',NOW(),'代码生成菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (117,'系统接口',3,3,'swagger','tool/swagger/index','','1','0','C','0','0','tool:swagger:list','swagger','admin',NOW(),'系统接口菜单') ON CONFLICT (menu_id) DO NOTHING)",

        // 三级菜单 — 操作日志 / 登录日志
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (500,'操作日志',108,1,'operlog','monitor/operlog/index','','1','0','C','0','0','monitor:operlog:list','form','admin',NOW(),'操作日志菜单') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (501,'登录日志',108,2,'logininfor','monitor/logininfor/index','','1','0','C','0','0','monitor:logininfor:list','logininfor','admin',NOW(),'登录日志菜单') ON CONFLICT (menu_id) DO NOTHING)",

        // 用户管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1000,'用户查询',100,1,'','','','1','0','F','0','0','system:user:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1001,'用户新增',100,2,'','','','1','0','F','0','0','system:user:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1002,'用户修改',100,3,'','','','1','0','F','0','0','system:user:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1003,'用户删除',100,4,'','','','1','0','F','0','0','system:user:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1004,'用户导出',100,5,'','','','1','0','F','0','0','system:user:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1005,'用户导入',100,6,'','','','1','0','F','0','0','system:user:import','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1006,'重置密码',100,7,'','','','1','0','F','0','0','system:user:resetPwd','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 角色管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1007,'角色查询',101,1,'','','','1','0','F','0','0','system:role:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1008,'角色新增',101,2,'','','','1','0','F','0','0','system:role:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1009,'角色修改',101,3,'','','','1','0','F','0','0','system:role:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1010,'角色删除',101,4,'','','','1','0','F','0','0','system:role:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1011,'角色导出',101,5,'','','','1','0','F','0','0','system:role:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 菜单管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1012,'菜单查询',102,1,'','','','1','0','F','0','0','system:menu:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1013,'菜单新增',102,2,'','','','1','0','F','0','0','system:menu:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1014,'菜单修改',102,3,'','','','1','0','F','0','0','system:menu:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1015,'菜单删除',102,4,'','','','1','0','F','0','0','system:menu:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 部门管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1016,'部门查询',103,1,'','','','1','0','F','0','0','system:dept:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1017,'部门新增',103,2,'','','','1','0','F','0','0','system:dept:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1018,'部门修改',103,3,'','','','1','0','F','0','0','system:dept:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1019,'部门删除',103,4,'','','','1','0','F','0','0','system:dept:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 岗位管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1020,'岗位查询',104,1,'','','','1','0','F','0','0','system:post:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1021,'岗位新增',104,2,'','','','1','0','F','0','0','system:post:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1022,'岗位修改',104,3,'','','','1','0','F','0','0','system:post:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1023,'岗位删除',104,4,'','','','1','0','F','0','0','system:post:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1024,'岗位导出',104,5,'','','','1','0','F','0','0','system:post:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 字典管理按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1025,'字典查询',105,1,'#','','','1','0','F','0','0','system:dict:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1026,'字典新增',105,2,'#','','','1','0','F','0','0','system:dict:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1027,'字典修改',105,3,'#','','','1','0','F','0','0','system:dict:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1028,'字典删除',105,4,'#','','','1','0','F','0','0','system:dict:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1029,'字典导出',105,5,'#','','','1','0','F','0','0','system:dict:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 参数设置按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1030,'参数查询',106,1,'#','','','1','0','F','0','0','system:config:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1031,'参数新增',106,2,'#','','','1','0','F','0','0','system:config:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1032,'参数修改',106,3,'#','','','1','0','F','0','0','system:config:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1033,'参数删除',106,4,'#','','','1','0','F','0','0','system:config:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1034,'参数导出',106,5,'#','','','1','0','F','0','0','system:config:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 通知公告按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1035,'公告查询',107,1,'#','','','1','0','F','0','0','system:notice:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1036,'公告新增',107,2,'#','','','1','0','F','0','0','system:notice:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1037,'公告修改',107,3,'#','','','1','0','F','0','0','system:notice:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1038,'公告删除',107,4,'#','','','1','0','F','0','0','system:notice:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 操作日志按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1039,'操作查询',500,1,'#','','','1','0','F','0','0','monitor:operlog:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1040,'操作删除',500,2,'#','','','1','0','F','0','0','monitor:operlog:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1041,'日志导出',500,3,'#','','','1','0','F','0','0','monitor:operlog:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 登录日志按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1042,'登录查询',501,1,'#','','','1','0','F','0','0','monitor:logininfor:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1043,'登录删除',501,2,'#','','','1','0','F','0','0','monitor:logininfor:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1044,'日志导出',501,3,'#','','','1','0','F','0','0','monitor:logininfor:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1045,'账户解锁',501,4,'#','','','1','0','F','0','0','monitor:logininfor:unlock','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 在线用户按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1046,'在线查询',109,1,'#','','','1','0','F','0','0','monitor:online:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1047,'批量强退',109,2,'#','','','1','0','F','0','0','monitor:online:batchLogout','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1048,'单条强退',109,3,'#','','','1','0','F','0','0','monitor:online:forceLogout','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 定时任务按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1049,'任务查询',110,1,'#','','','1','0','F','0','0','monitor:job:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1050,'任务新增',110,2,'#','','','1','0','F','0','0','monitor:job:add','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1051,'任务修改',110,3,'#','','','1','0','F','0','0','monitor:job:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1052,'任务删除',110,4,'#','','','1','0','F','0','0','monitor:job:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1053,'状态修改',110,5,'#','','','1','0','F','0','0','monitor:job:changeStatus','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1054,'任务导出',110,6,'#','','','1','0','F','0','0','monitor:job:export','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // 代码生成按钮
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1055,'生成查询',116,1,'#','','','1','0','F','0','0','tool:gen:query','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1056,'生成修改',116,2,'#','','','1','0','F','0','0','tool:gen:edit','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1057,'生成删除',116,3,'#','','','1','0','F','0','0','tool:gen:remove','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1058,'导入代码',116,4,'#','','','1','0','F','0','0','tool:gen:import','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1059,'预览代码',116,5,'#','','','1','0','F','0','0','tool:gen:preview','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",
        R"(INSERT INTO sys_menu (menu_id,menu_name,parent_id,order_num,path,component,query,is_frame,is_cache,menu_type,visible,status,perms,icon,create_by,create_time,remark) VALUES
           (1060,'生成代码',116,6,'#','','','1','0','F','0','0','tool:gen:code','#','admin',NOW(),'') ON CONFLICT (menu_id) DO NOTHING)",

        // =====================================================================
        // 5. 用户-角色关联
        // =====================================================================
        R"(INSERT INTO sys_user_role (user_id,role_id) VALUES (1,1) ON CONFLICT DO NOTHING)",
        R"(INSERT INTO sys_user_role (user_id,role_id) VALUES (2,2) ON CONFLICT DO NOTHING)",

        // =====================================================================
        // 6. 角色-菜单关联（role_id=2 普通角色 拥有所有菜单权限）
        // =====================================================================
        R"(INSERT INTO sys_role_menu (role_id,menu_id) VALUES
           (2,1),(2,2),(2,3),(2,4),
           (2,100),(2,101),(2,102),(2,103),(2,104),(2,105),(2,106),(2,107),(2,108),
           (2,109),(2,110),(2,111),(2,112),(2,113),(2,114),(2,115),(2,116),(2,117),
           (2,500),(2,501),
           (2,1000),(2,1001),(2,1002),(2,1003),(2,1004),(2,1005),(2,1006),
           (2,1007),(2,1008),(2,1009),(2,1010),(2,1011),
           (2,1012),(2,1013),(2,1014),(2,1015),
           (2,1016),(2,1017),(2,1018),(2,1019),
           (2,1020),(2,1021),(2,1022),(2,1023),(2,1024),
           (2,1025),(2,1026),(2,1027),(2,1028),(2,1029),
           (2,1030),(2,1031),(2,1032),(2,1033),(2,1034),
           (2,1035),(2,1036),(2,1037),(2,1038),
           (2,1039),(2,1040),(2,1041),
           (2,1042),(2,1043),(2,1044),(2,1045),
           (2,1046),(2,1047),(2,1048),
           (2,1049),(2,1050),(2,1051),(2,1052),(2,1053),(2,1054),
           (2,1055),(2,1056),(2,1057),(2,1058),(2,1059),(2,1060)
           ON CONFLICT DO NOTHING)",

        // =====================================================================
        // 7. 角色-部门关联
        // =====================================================================
        R"(INSERT INTO sys_role_dept (role_id,dept_id) VALUES (2,100) ON CONFLICT DO NOTHING)",
        R"(INSERT INTO sys_role_dept (role_id,dept_id) VALUES (2,101) ON CONFLICT DO NOTHING)",
        R"(INSERT INTO sys_role_dept (role_id,dept_id) VALUES (2,105) ON CONFLICT DO NOTHING)",

        // =====================================================================
        // 8. 用户-岗位关联
        // =====================================================================
        R"(INSERT INTO sys_user_post (user_id,post_id) VALUES (1,1) ON CONFLICT DO NOTHING)",
        R"(INSERT INTO sys_user_post (user_id,post_id) VALUES (2,2) ON CONFLICT DO NOTHING)",

        // =====================================================================
        // 9. 岗位初始数据
        // =====================================================================
        R"(INSERT INTO sys_post (post_id,post_code,post_name,post_sort,status,create_by,create_time)
           VALUES (1,'ceo','董事长',1,'0','admin',NOW()) ON CONFLICT (post_id) DO NOTHING)",
        R"(INSERT INTO sys_post (post_id,post_code,post_name,post_sort,status,create_by,create_time)
           VALUES (2,'se','项目经理',2,'0','admin',NOW()) ON CONFLICT (post_id) DO NOTHING)",
        R"(INSERT INTO sys_post (post_id,post_code,post_name,post_sort,status,create_by,create_time)
           VALUES (3,'hr','人力资源',3,'0','admin',NOW()) ON CONFLICT (post_id) DO NOTHING)",
        R"(INSERT INTO sys_post (post_id,post_code,post_name,post_sort,status,create_by,create_time)
           VALUES (4,'user','普通员工',4,'0','admin',NOW()) ON CONFLICT (post_id) DO NOTHING)",

        // =====================================================================
        // 10. 参数配置
        // =====================================================================
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (1,'主框架页-默认皮肤样式名称','sys.index.skinName','skin-blue','Y','admin',NOW(),'蓝色 skin-blue、绿色 skin-green、紫色 skin-purple、红色 skin-red、黄色 skin-yellow')
           ON CONFLICT (config_id) DO NOTHING)",
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (2,'用户管理-账号初始密码','sys.user.initPassword','123456','Y','admin',NOW(),'初始化密码 123456')
           ON CONFLICT (config_id) DO NOTHING)",
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (3,'主框架页-侧边栏主题','sys.index.sideTheme','theme-dark','Y','admin',NOW(),'深色主题theme-dark，浅色主题theme-light')
           ON CONFLICT (config_id) DO NOTHING)",
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (4,'账号自助-验证码开关','sys.account.captchaEnabled','true','Y','admin',NOW(),'是否开启验证码功能（true开启，false关闭）')
           ON CONFLICT (config_id) DO NOTHING)",
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (5,'账号自助-是否开启用户注册功能','sys.account.registerUser','false','Y','admin',NOW(),'是否开启注册用户功能（true开启，false关闭）')
           ON CONFLICT (config_id) DO NOTHING)",
        R"(INSERT INTO sys_config (config_id,config_name,config_key,config_value,config_type,create_by,create_time,remark)
           VALUES (6,'用户登录-黑名单列表','sys.login.blackIPList','','Y','admin',NOW(),'设置登录IP黑名单限制，多个匹配项以;分隔，支持匹配（*通配、网段）')
           ON CONFLICT (config_id) DO NOTHING)",

        // =====================================================================
        // 11. 字典类型
        // =====================================================================
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (1,'用户性别','sys_user_sex','0','admin',NOW(),'用户性别列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (2,'菜单状态','sys_show_hide','0','admin',NOW(),'菜单状态列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (3,'系统开关','sys_normal_disable','0','admin',NOW(),'系统开关列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (4,'任务状态','sys_job_status','0','admin',NOW(),'任务状态列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (5,'任务分组','sys_job_group','0','admin',NOW(),'任务分组列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (6,'系统是否','sys_yes_no','0','admin',NOW(),'系统是否列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (7,'通知类型','sys_notice_type','0','admin',NOW(),'通知类型列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (8,'通知状态','sys_notice_status','0','admin',NOW(),'通知状态列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (9,'操作类型','sys_oper_type','0','admin',NOW(),'操作类型列表') ON CONFLICT (dict_type) DO NOTHING)",
        R"(INSERT INTO sys_dict_type (dict_id,dict_name,dict_type,status,create_by,create_time,remark)
           VALUES (10,'系统状态','sys_common_status','0','admin',NOW(),'登录状态列表') ON CONFLICT (dict_type) DO NOTHING)",

        // =====================================================================
        // 12. 字典数据 (与 C# 完全一致，包含全部29条)
        // =====================================================================
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (1,1,'男','0','sys_user_sex','','','Y','0','admin',NOW(),'性别男') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (2,2,'女','1','sys_user_sex','','','N','0','admin',NOW(),'性别女') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (3,3,'未知','2','sys_user_sex','','','N','0','admin',NOW(),'性别未知') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (4,1,'显示','0','sys_show_hide','','primary','Y','0','admin',NOW(),'显示菜单') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (5,2,'隐藏','1','sys_show_hide','','danger','N','0','admin',NOW(),'隐藏菜单') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (6,1,'正常','0','sys_normal_disable','','primary','Y','0','admin',NOW(),'正常状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (7,2,'停用','1','sys_normal_disable','','danger','N','0','admin',NOW(),'停用状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (8,1,'正常','0','sys_job_status','','primary','Y','0','admin',NOW(),'正常状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (9,2,'暂停','1','sys_job_status','','danger','N','0','admin',NOW(),'停用状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (10,1,'默认','DEFAULT','sys_job_group','','','Y','0','admin',NOW(),'默认分组') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (11,2,'系统','SYSTEM','sys_job_group','','','N','0','admin',NOW(),'系统分组') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (12,1,'是','Y','sys_yes_no','','primary','Y','0','admin',NOW(),'系统默认是') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (13,2,'否','N','sys_yes_no','','danger','N','0','admin',NOW(),'系统默认否') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (14,1,'通知','1','sys_notice_type','','warning','Y','0','admin',NOW(),'通知') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (15,2,'公告','2','sys_notice_type','','success','N','0','admin',NOW(),'公告') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (16,1,'正常','0','sys_notice_status','','primary','Y','0','admin',NOW(),'正常状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (17,2,'关闭','1','sys_notice_status','','danger','N','0','admin',NOW(),'关闭状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (18,99,'其他','0','sys_oper_type','','info','N','0','admin',NOW(),'其他操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (19,1,'新增','1','sys_oper_type','','info','N','0','admin',NOW(),'新增操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (20,2,'修改','2','sys_oper_type','','info','N','0','admin',NOW(),'修改操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (21,3,'删除','3','sys_oper_type','','danger','N','0','admin',NOW(),'删除操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (22,4,'授权','4','sys_oper_type','','primary','N','0','admin',NOW(),'授权操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (23,5,'导出','5','sys_oper_type','','warning','N','0','admin',NOW(),'导出操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (24,6,'导入','6','sys_oper_type','','warning','N','0','admin',NOW(),'导入操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (25,7,'强退','7','sys_oper_type','','danger','N','0','admin',NOW(),'强退操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (26,8,'生成代码','8','sys_oper_type','','warning','N','0','admin',NOW(),'生成操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (27,9,'清空数据','9','sys_oper_type','','danger','N','0','admin',NOW(),'清空操作') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (28,1,'成功','0','sys_common_status','','primary','N','0','admin',NOW(),'正常状态') ON CONFLICT (dict_code) DO NOTHING)",
        R"(INSERT INTO sys_dict_data (dict_code,dict_sort,dict_label,dict_value,dict_type,css_class,list_class,is_default,status,create_by,create_time,remark)
           VALUES (29,2,'失败','1','sys_common_status','','danger','N','0','admin',NOW(),'停用状态') ON CONFLICT (dict_code) DO NOTHING)",

        // =====================================================================
        // 13. 通知公告初始数据
        // =====================================================================
        R"(INSERT INTO sys_notice (notice_id,notice_title,notice_type,notice_content,status,create_by,create_time,remark)
           VALUES (1,'温馨提醒：2018-07-01 若依新版本发布啦','2','新版本内容','0','admin',NOW(),'管理员') ON CONFLICT (notice_id) DO NOTHING)",
        R"(INSERT INTO sys_notice (notice_id,notice_title,notice_type,notice_content,status,create_by,create_time,remark)
           VALUES (2,'维护通知：2018-07-01 若依系统凌晨维护','1','维护内容','0','admin',NOW(),'管理员') ON CONFLICT (notice_id) DO NOTHING)",

        // =====================================================================
        // 14. 定时任务初始数据
        // =====================================================================
        R"(INSERT INTO sys_job (job_id,job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,create_by,create_time)
           VALUES (1,'系统默认（无参）','DEFAULT','ryTask.RyNoParams','0/10 * * * * ?','3','1','1','admin',NOW()) ON CONFLICT (job_id) DO NOTHING)",
        R"(INSERT INTO sys_job (job_id,job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,create_by,create_time)
           VALUES (2,'系统默认（有参）','DEFAULT','ryTask.RyParams(''ry'')','0/15 * * * * ?','3','1','1','admin',NOW()) ON CONFLICT (job_id) DO NOTHING)",
        R"(INSERT INTO sys_job (job_id,job_name,job_group,invoke_target,cron_expression,misfire_policy,concurrent,status,create_by,create_time)
           VALUES (3,'系统默认（多参）','DEFAULT','ryTask.RyMultipleParams(''ry'', true, 2000L, 316.50D, 100)','0/20 * * * * ?','3','1','1','admin',NOW()) ON CONFLICT (job_id) DO NOTHING)",

        // =====================================================================
        // 15. 重置所有序列，避免手动插入 ID 后自增冲突
        // =====================================================================
        "SELECT setval('sys_dept_dept_id_seq', GREATEST((SELECT COALESCE(MAX(dept_id),1) FROM sys_dept), 1))",
        "SELECT setval('sys_user_user_id_seq', GREATEST((SELECT COALESCE(MAX(user_id),1) FROM sys_user), 1))",
        "SELECT setval('sys_role_role_id_seq', GREATEST((SELECT COALESCE(MAX(role_id),1) FROM sys_role), 1))",
        "SELECT setval('sys_menu_menu_id_seq', GREATEST((SELECT COALESCE(MAX(menu_id),1) FROM sys_menu), 1))",
        "SELECT setval('sys_post_post_id_seq', GREATEST((SELECT COALESCE(MAX(post_id),1) FROM sys_post), 1))",
        "SELECT setval('sys_config_config_id_seq', GREATEST((SELECT COALESCE(MAX(config_id),1) FROM sys_config), 1))",
        "SELECT setval('sys_dict_type_dict_id_seq', GREATEST((SELECT COALESCE(MAX(dict_id),1) FROM sys_dict_type), 1))",
        "SELECT setval('sys_dict_data_dict_code_seq', GREATEST((SELECT COALESCE(MAX(dict_code),1) FROM sys_dict_data), 1))",
        "SELECT setval('sys_notice_notice_id_seq', GREATEST((SELECT COALESCE(MAX(notice_id),1) FROM sys_notice), 1))",
        "SELECT setval('sys_job_job_id_seq', GREATEST((SELECT COALESCE(MAX(job_id),1) FROM sys_job), 1))",
        "SELECT setval('sys_oper_log_oper_id_seq', GREATEST((SELECT COALESCE(MAX(oper_id),1) FROM sys_oper_log), 1))",
        "SELECT setval('sys_logininfor_info_id_seq', GREATEST((SELECT COALESCE(MAX(info_id),1) FROM sys_logininfor), 1))",
        "SELECT setval('sys_job_log_job_log_id_seq', GREATEST((SELECT COALESCE(MAX(job_log_id),1) FROM sys_job_log), 1))",
    };
}

// ============================================================
// SQLite DDL & Init-data 翻译
// ============================================================

// --- 工具函数 ------------------------------------------------
static std::string sqNormWS(const std::string& s) {
    std::string r; r.reserve(s.size());
    bool sp = false;
    for (unsigned char c : s) {
        if (std::isspace(c)) { if (!sp) { r += ' '; sp = true; } }
        else { r += (char)c; sp = false; }
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r;
}

static std::string sqReplAll(std::string s, const std::string& f, const std::string& t) {
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) { s.replace(pos, f.size(), t); pos += t.size(); }
    return s;
}

// PG DDL → SQLite DDL
static std::string pgDdlToSqlite(const std::string& in) {
    // Skip PG anonymous DO $$ ... $$ blocks
    size_t i = 0;
    while (i < in.size() && std::isspace((unsigned char)in[i])) ++i;
    if (i + 2 <= in.size() &&
        std::toupper((unsigned char)in[i])   == 'D' &&
        std::toupper((unsigned char)in[i+1]) == 'O')
        return "";

    std::string s = sqNormWS(in);

    // Primary key types
    s = sqReplAll(s, "BIGSERIAL PRIMARY KEY", "INTEGER PRIMARY KEY AUTOINCREMENT");
    s = sqReplAll(s, "SERIAL PRIMARY KEY",    "INTEGER PRIMARY KEY AUTOINCREMENT");
    // Boolean defaults (specific before generic)
    s = sqReplAll(s, "BOOLEAN NOT NULL DEFAULT TRUE",  "INTEGER NOT NULL DEFAULT 1");
    s = sqReplAll(s, "BOOLEAN NOT NULL DEFAULT FALSE", "INTEGER NOT NULL DEFAULT 0");
    s = sqReplAll(s, "BOOLEAN", "INTEGER");

    // VARCHAR(n) / CHAR(n) → TEXT  (word-boundary safe)
    {
        std::string r; r.reserve(s.size());
        for (size_t j = 0; j < s.size(); ) {
            auto tryKw = [&](const char* kw) -> bool {
                size_t klen = std::strlen(kw);
                if (j + klen > s.size()) return false;
                if (j > 0 && std::isalnum((unsigned char)s[j-1])) return false;
                for (size_t k = 0; k < klen; ++k)
                    if (std::toupper((unsigned char)s[j+k]) != (unsigned char)kw[k]) return false;
                size_t p = j + klen;
                if (p >= s.size() || s[p] != '(') return false;
                while (p < s.size() && s[p] != ')') ++p;
                if (p < s.size()) ++p;
                r += "TEXT"; j = p; return true;
            };
            if (!tryKw("VARCHAR(") && !tryKw("CHAR("))
                r += s[j++];
        }
        s = r;
    }

    s = sqReplAll(s, "TIMESTAMP", "TEXT");
    s = sqReplAll(s, "BIGINT",    "INTEGER");

    // Standalone INT → INTEGER  (not part of INTEGER / CONSTRAINT / etc.)
    {
        std::string r; r.reserve(s.size());
        for (size_t j = 0; j < s.size(); ) {
            if (j + 3 <= s.size() &&
                std::toupper((unsigned char)s[j])   == 'I' &&
                std::toupper((unsigned char)s[j+1]) == 'N' &&
                std::toupper((unsigned char)s[j+2]) == 'T' &&
                (j == 0 || !std::isalpha((unsigned char)s[j-1])) &&
                (j+3 >= s.size() || !std::isalpha((unsigned char)s[j+3]))) {
                r += "INTEGER"; j += 3;
            } else r += s[j++];
        }
        s = r;
    }
    return s;
}

// PG init INSERT → SQLite init INSERT
static std::string pgInitToSqlite(const std::string& in) {
    std::string s = in;

    // INSERT INTO → INSERT OR IGNORE INTO
    {
        const std::string kw = "INSERT INTO ";
        size_t pos = s.find(kw);
        if (pos != std::string::npos)
            s = s.substr(0, pos) + "INSERT OR IGNORE INTO " + s.substr(pos + kw.size());
    }

    // Remove ON CONFLICT ... DO NOTHING (strip to end or ';')
    {
        std::string upper = s;
        for (auto& c : upper) c = (char)std::toupper((unsigned char)c);
        size_t pos = upper.rfind(" ON CONFLICT");
        if (pos != std::string::npos) {
            size_t semi = s.find(';', pos);
            s = s.substr(0, pos) + (semi != std::string::npos ? s.substr(semi) : "");
        }
    }

    // NOW() → datetime('now')  (case-insensitive)
    {
        std::string r; r.reserve(s.size());
        const char* kw = "NOW()"; const size_t klen = 5;
        for (size_t i = 0; i < s.size(); ) {
            bool match = (i + klen <= s.size());
            if (match) for (size_t j = 0; j < klen; ++j)
                if (std::toupper((unsigned char)s[i+j]) != (unsigned char)kw[j]) { match = false; break; }
            if (match) { r += "datetime('now')"; i += klen; }
            else r += s[i++];
        }
        s = r;
    }

    // TRUE → 1, FALSE → 0  (word boundary)
    for (auto [from, to] : std::initializer_list<std::pair<const char*, const char*>>{{"TRUE","1"},{"FALSE","0"}}) {
        std::string r; r.reserve(s.size());
        size_t flen = std::strlen(from);
        for (size_t i = 0; i < s.size(); ) {
            bool match = (i + flen <= s.size());
            if (match) {
                for (size_t j = 0; j < flen; ++j)
                    if (std::toupper((unsigned char)s[i+j]) != (unsigned char)from[j]) { match = false; break; }
                if (match) match = (i == 0 || !std::isalnum((unsigned char)s[i-1])) &&
                                   (i+flen >= s.size() || !std::isalnum((unsigned char)s[i+flen]));
            }
            if (match) { r += to; i += flen; }
            else r += s[i++];
        }
        s = r;
    }
    return s;
}

// --- 公开实现 ------------------------------------------------
std::vector<std::string> DatabaseInit::getSqliteCreateTableSqls() {
    std::vector<std::string> result;
    for (auto& sql : getCreateTableSqls()) {
        auto t = pgDdlToSqlite(sql);
        if (!t.empty()) result.push_back(std::move(t));
    }
    return result;
}

std::vector<std::string> DatabaseInit::getSqliteInitDataSqls() {
    std::vector<std::string> result;
    for (auto& sql : getInitDataSqls()) {
        if (sql.find("setval") != std::string::npos) continue; // PG-only 序列重置
        result.push_back(pgInitToSqlite(sql));
    }
    return result;
}
