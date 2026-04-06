#pragma once
#include <drogon/HttpController.h>
#include <sstream>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/PageUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"

// 代码生成工具 /tool/gen  (完整实现：导入/预览/生成/同步)
class GenCtrl : public drogon::HttpController<GenCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(GenCtrl::list,         "/tool/gen/list",              drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::getById,      "/tool/gen/{tableId}",        drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::dbList,       "/tool/gen/db/list",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::columnList,   "/tool/gen/column/{tableId}", drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::importTable,  "/tool/gen/importTable",      drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::edit,         "/tool/gen",                  drogon::Put,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::remove,       "/tool/gen/{tableIds}",       drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::preview,      "/tool/gen/preview/{tableId}",drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::genCode,      "/tool/gen/genCode/{tableName}", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::batchGenCode, "/tool/gen/batchGenCode",     drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(GenCtrl::synchDb,      "/tool/gen/synchDb/{tableName}", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:list");
        auto page = PageParam::fromRequest(req);
        auto& db = DatabaseService::instance();
        auto res = db.query(
            "SELECT table_id,table_name,table_comment,class_name,create_time,update_time "
            "FROM gen_table ORDER BY table_id DESC LIMIT " + std::to_string(page.pageSize) +
            " OFFSET " + std::to_string(page.offset()));
        auto cntRes = db.query("SELECT COUNT(*) FROM gen_table");
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["tableId"]      = (Json::Int64)res.longVal(i, 0);
            j["tableName"]    = res.str(i, 1);
            j["tableComment"] = res.str(i, 2);
            j["className"]    = res.str(i, 3);
            j["createTime"]   = fmtTs(res.str(i, 4));
            j["updateTime"]   = fmtTs(res.str(i, 5));
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void getById(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:query");
        auto& db = DatabaseService::instance();
        auto t = db.queryParams(
            "SELECT table_id,table_name,table_comment,class_name,package_name,module_name,"
            "business_name,function_name,function_author,gen_type,gen_path,options,remark "
            "FROM gen_table WHERE table_id=$1", {std::to_string(tableId)});
        if (!t.ok() || t.rows()==0) { RESP_ERR(cb, "表不存在"); return; }
        auto info = tableRowToJson(t, 0);
        auto cols = db.queryParams(
            "SELECT column_id,column_name,column_comment,column_type,java_type,java_field,"
            "is_pk,is_increment,is_required,is_insert,is_edit,is_list,is_query,"
            "query_type,html_type,dict_type,sort "
            "FROM gen_table_column WHERE table_id=$1 ORDER BY sort",
            {std::to_string(tableId)});
        Json::Value colArr(Json::arrayValue);
        if (cols.ok()) for (int i=0;i<cols.rows();++i) colArr.append(colRowToJson(cols,i));
        Json::Value data;
        data["info"] = info;
        data["rows"] = colArr;
        RESP_OK(cb, data);
    }

    void dbList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:list");
        // 查询 PostgreSQL information_schema 获取数据库表列表
        auto& db = DatabaseService::instance();
        auto tableName = req->getParameter("tableName");
        auto tableComment = req->getParameter("tableComment");
        std::string sql =
            "SELECT table_name,COALESCE(obj_description((quote_ident(table_schema)||'.'||quote_ident(table_name))::regclass),'') AS table_comment "
            "FROM information_schema.tables WHERE table_schema='public' AND table_type='BASE TABLE' "
            "AND table_name NOT IN (SELECT table_name FROM gen_table)";
        std::vector<std::string> params;
        int idx = 1;
        if (!tableName.empty()) { sql += " AND table_name LIKE $" + std::to_string(idx++); params.push_back("%" + tableName + "%"); }
        auto page = PageParam::fromRequest(req);
        std::string countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
        auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
        long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
        sql += " ORDER BY table_name LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
            Json::Value j;
            j["tableName"]    = res.str(i, 0);
            j["tableComment"] = res.str(i, 1);
            j["createTime"]   = "";
            j["updateTime"]   = "";
            rows.append(j);
        }
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void columnList(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:list");
        auto res = DatabaseService::instance().queryParams(
            "SELECT column_id,column_name,column_comment,column_type,java_type,java_field,"
            "is_pk,is_increment,is_required,is_insert,is_edit,is_list,is_query,"
            "query_type,html_type,dict_type,sort "
            "FROM gen_table_column WHERE table_id=$1 ORDER BY sort",
            {std::to_string(tableId)});
        Json::Value rows(Json::arrayValue);
        if (res.ok()) for (int i=0;i<res.rows();++i) rows.append(colRowToJson(res,i));
        long total = res.ok() ? (long)res.rows() : 0;
        PageResult pr; pr.total = total; pr.rows = rows;
        RESP_JSON(cb, pr.toJson());
    }

    void importTable(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:import");
        auto tables = req->getParameter("tables");
        if (tables.empty()) { RESP_ERR(cb, "请选择要导入的表"); return; }
        auto& db = DatabaseService::instance();
        std::istringstream ss(tables);
        std::string tbl;
        while (std::getline(ss, tbl, ',')) {
            if (tbl.empty()) continue;
            // 检查是否已导入
            auto ex = db.queryParams("SELECT table_id FROM gen_table WHERE table_name=$1", {tbl});
            if (ex.ok() && ex.rows() > 0) continue;
            // 读表注释
            auto cm = db.queryParams(
                "SELECT COALESCE(obj_description((quote_ident('public')||'.'||quote_ident($1))::regclass),'')",
                {tbl});
            std::string comment = (cm.ok()&&cm.rows()>0) ? cm.str(0,0) : "";
            std::string cls = toCamelCase(tbl, true);
            std::string mod = tbl.find('_')!=std::string::npos ? tbl.substr(0,tbl.find('_')) : tbl;
            std::string biz = tbl.rfind('_')!=std::string::npos ? tbl.substr(tbl.rfind('_')+1) : tbl;
            db.execParams(
                "INSERT INTO gen_table(table_name,table_comment,class_name,package_name,module_name,"
                "business_name,function_name,function_author,gen_type,gen_path,create_time) "
                "VALUES($1,$2,$3,'com.ruoyi',$4,$5,$2,'ruoyi','0','/',NOW())",
                {tbl, comment, cls, mod, biz});
            auto tid = db.queryParams("SELECT table_id FROM gen_table WHERE table_name=$1 ORDER BY table_id DESC LIMIT 1", {tbl});
            if (!tid.ok() || tid.rows()==0) continue;
            std::string tableIdStr = tid.str(0,0);
            // 导入字段
            auto cols = db.queryParams(
                "SELECT column_name,data_type,"
                "COALESCE(col_description((quote_ident('public')||'.'||quote_ident($1))::regclass,ordinal_position),'') "
                "FROM information_schema.columns "
                "WHERE table_schema='public' AND table_name=$1 ORDER BY ordinal_position",
                {tbl});
            int sort = 1;
            if (cols.ok()) for (int i=0;i<cols.rows();++i) {
                std::string col  = cols.str(i,0);
                std::string type = cols.str(i,1);
                std::string colComment = cols.str(i,2);
                bool isPk = (col=="id"||col==(tbl+"_id")||col=="pk");
                std::string jtype = pgTypeToJava(type);
                std::string jfield = toCamelCase(col, false);
                db.execParams(
                    "INSERT INTO gen_table_column(table_id,column_name,column_comment,column_type,"
                    "java_type,java_field,is_pk,is_increment,is_required,"
                    "is_insert,is_edit,is_list,is_query,query_type,html_type,dict_type,sort) "
                    "VALUES($1,$2,$3,$4,$5,$6,$7,'0','0','1','1','1','0','EQ','input',''," + std::to_string(sort++) + ")",
                    {tableIdStr, col, colComment, type, jtype, jfield, isPk?"1":"0"});
            }
        }
        RESP_MSG(cb, "导入成功");
    }

    void edit(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:edit");
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
        auto& db = DatabaseService::instance();
        auto& tbl = *body;
        db.execParams(
            "UPDATE gen_table SET table_comment=$1,class_name=$2,package_name=$3,module_name=$4,"
            "business_name=$5,function_name=$6,function_author=$7,gen_type=$8,gen_path=$9,"
            "options=$10,remark=$11,update_time=NOW() WHERE table_id=$12",
            {tbl["tableComment"].asString(), tbl["className"].asString(),
             tbl["packageName"].asString(), tbl["moduleName"].asString(),
             tbl["businessName"].asString(), tbl["functionName"].asString(),
             tbl["functionAuthor"].asString(), tbl.get("genType","0").asString(),
             tbl.get("genPath","/").asString(), tbl.get("options","").asString(),
             tbl.get("remark","").asString(), std::to_string(tbl["tableId"].asInt64())});
        // 更新列配置
        if (tbl.isMember("columns") && tbl["columns"].isArray()) {
            for (auto& col : tbl["columns"]) {
                db.execParams(
                    "UPDATE gen_table_column SET column_comment=$1,java_type=$2,java_field=$3,"
                    "is_required=$4,is_insert=$5,is_edit=$6,is_list=$7,is_query=$8,"
                    "query_type=$9,html_type=$10,dict_type=$11 WHERE column_id=$12",
                    {col["columnComment"].asString(), col["javaType"].asString(),
                     col["javaField"].asString(), col.get("isRequired","0").asString(),
                     col.get("isInsert","0").asString(), col.get("isEdit","0").asString(),
                     col.get("isList","0").asString(), col.get("isQuery","0").asString(),
                     col.get("queryType","EQ").asString(), col.get("htmlType","input").asString(),
                     col.get("dictType","").asString(),
                     std::to_string(col["columnId"].asInt64())});
            }
        }
        RESP_MSG(cb, "操作成功");
    }

    void remove(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableIds) {
        CHECK_PERM(req, cb, "tool:gen:remove");
        auto& db = DatabaseService::instance();
        std::istringstream ss(tableIds); std::string id;
        while (std::getline(ss, id, ',')) {
            db.execParams("DELETE FROM gen_table_column WHERE table_id=$1", {id});
            db.execParams("DELETE FROM gen_table WHERE table_id=$1", {id});
        }
        RESP_MSG(cb, "操作成功");
    }

    void preview(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long tableId) {
        CHECK_PERM(req, cb, "tool:gen:preview");
        auto info = loadTableInfo(tableId);
        if (info.tableName.empty()) { RESP_ERR(cb, "表不存在"); return; }
        Json::Value data;
        data["vue/index.vue"]                                = genVue(info);
        data["src/controller/" + info.className + "Ctrl.h"]  = genController(info);
        data["src/service/"    + info.className + "Service.h"] = genService(info);
        data["sql/"            + info.tableName + ".sql"]     = genSql(info);
        data["api/" + info.moduleName + "/" + info.businessName + ".js"] = genApi(info);
        RESP_OK(cb, data);
    }

    void genCode(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableName) {
        CHECK_PERM(req, cb, "tool:gen:code");
        auto res = DatabaseService::instance().queryParams(
            "SELECT table_id FROM gen_table WHERE table_name=$1", {tableName});
        if (!res.ok() || res.rows()==0) { RESP_ERR(cb, "表不在代码生成列表中"); return; }
        auto info = loadTableInfo(res.longVal(0,0));
        // 返回 multipart 或 JSON（前端接受 blob，这里返回 JSON 压缩包内容）
        Json::Value data;
        data["controller"] = genController(info);
        data["service"]    = genService(info);
        data["sql"]        = genSql(info);
        data["vue"]        = genVue(info);
        data["api"]        = genApi(info);
        // 以 JSON 下载形式返回
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        Json::StreamWriterBuilder wb; wb["indentation"] = "  ";
        resp->setBody(Json::writeString(wb, data));
        resp->addHeader("Content-Disposition",
            "attachment; filename=\"" + tableName + "_gen.json\"");
        cb(resp);
    }

    void batchGenCode(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "tool:gen:code");
        auto tables = req->getParameter("tables");
        if (tables.empty()) { RESP_ERR(cb, "请指定表名"); return; }
        Json::Value result;
        std::istringstream ss(tables); std::string tbl;
        while (std::getline(ss, tbl, ',')) {
            auto res = DatabaseService::instance().queryParams(
                "SELECT table_id FROM gen_table WHERE table_name=$1", {tbl});
            if (!res.ok() || res.rows()==0) continue;
            auto info = loadTableInfo(res.longVal(0,0));
            Json::Value item;
            item["controller"] = genController(info);
            item["service"]    = genService(info);
            item["sql"]        = genSql(info);
            item["vue"]        = genVue(info);
            item["api"]        = genApi(info);
            result[tbl] = item;
        }
        RESP_OK(cb, result);
    }

    void synchDb(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, const std::string &tableName) {
        CHECK_PERM(req, cb, "tool:gen:edit");
        auto& db = DatabaseService::instance();
        auto tr = db.queryParams("SELECT table_id FROM gen_table WHERE table_name=$1", {tableName});
        if (!tr.ok() || tr.rows()==0) { RESP_ERR(cb, "表不存在"); return; }
        std::string tid = std::to_string(tr.longVal(0,0));
        // 获取最新列信息并与已有列对比
        auto dbCols = db.queryParams(
            "SELECT column_name,data_type,ordinal_position,"
            "COALESCE(col_description((quote_ident('public')||'.'||quote_ident($1))::regclass,ordinal_position),'')"
            " FROM information_schema.columns WHERE table_schema='public' AND table_name=$1 ORDER BY ordinal_position",
            {tableName});
        auto existCols = db.queryParams(
            "SELECT column_name FROM gen_table_column WHERE table_id=$1", {tid});
        std::set<std::string> existing;
        if (existCols.ok()) for (int i=0;i<existCols.rows();++i) existing.insert(existCols.str(i,0));
        if (dbCols.ok()) for (int i=0;i<dbCols.rows();++i) {
            std::string col = dbCols.str(i,0);
            if (existing.count(col)) continue; // 已存在跳过
            std::string type = dbCols.str(i,1);
            std::string cmt  = dbCols.str(i,3);
            db.execParams(
                "INSERT INTO gen_table_column(table_id,column_name,column_comment,column_type,"
                "java_type,java_field,is_pk,is_increment,is_required,"
                "is_insert,is_edit,is_list,is_query,query_type,html_type,dict_type,sort)"
                " VALUES($1,$2,$3,$4,$5,$6,'0','0','0','1','1','1','0','EQ','input',''," + dbCols.str(i,2) + ")",
                {tid, col, cmt, type, pgTypeToJava(type), toCamelCase(col,false)});
        }
        db.execParams("UPDATE gen_table SET update_time=NOW() WHERE table_id=$1", {tid});
        RESP_MSG(cb, "同步成功");
    }

private:
    // ── 表信息结构 ───────────────────────────────────────────────────────────────
    struct ColInfo {
        std::string colName, colComment, colType, javaType, javaField;
        bool isPk=false, isInsert=true, isEdit=true, isList=true, isQuery=false;
        std::string htmlType="input", dictType, queryType="EQ";
    };
    struct TableInfo {
        long        tableId=0;
        std::string tableName, tableComment, className;
        std::string packageName="com.ruoyi", moduleName, businessName;
        std::string functionName, functionAuthor="ruoyi";
        std::vector<ColInfo> columns;
        // 注意：不缓存 pkCol 指针以避免 vector 重新分配后悬空
        const ColInfo* pkCol() const {
            for (auto& c : columns) if (c.isPk) return &c;
            return nullptr;
        }
    };

    TableInfo loadTableInfo(long tableId) {
        TableInfo info;
        auto& db = DatabaseService::instance();
        auto t = db.queryParams(
            "SELECT table_id,table_name,table_comment,class_name,package_name,module_name,"
            "business_name,function_name,function_author FROM gen_table WHERE table_id=$1",
            {std::to_string(tableId)});
        if (!t.ok() || t.rows()==0) return info;
        info.tableId      = t.longVal(0,0);
        info.tableName    = t.str(0,1);
        info.tableComment = t.str(0,2);
        info.className    = t.str(0,3);
        info.packageName  = t.str(0,4).empty() ? "com.ruoyi" : t.str(0,4);
        info.moduleName   = t.str(0,5);
        info.businessName = t.str(0,6);
        info.functionName = t.str(0,7);
        info.functionAuthor = t.str(0,8).empty() ? "ruoyi" : t.str(0,8);
        auto cols = db.queryParams(
            "SELECT column_name,column_comment,column_type,java_type,java_field,"
            "is_pk,is_insert,is_edit,is_list,is_query,html_type,dict_type,query_type "
            "FROM gen_table_column WHERE table_id=$1 ORDER BY sort",
            {std::to_string(tableId)});
        if (cols.ok()) for (int i=0;i<cols.rows();++i) {
            ColInfo c;
            c.colName    = cols.str(i,0);
            c.colComment = cols.str(i,1);
            c.colType    = cols.str(i,2);
            c.javaType   = cols.str(i,3);
            c.javaField  = cols.str(i,4);
            c.isPk       = (cols.str(i,5)=="1");
            c.isInsert   = (cols.str(i,6)=="1");
            c.isEdit     = (cols.str(i,7)=="1");
            c.isList     = (cols.str(i,8)=="1");
            c.isQuery    = (cols.str(i,9)=="1");
            c.htmlType   = cols.str(i,10);
            c.dictType   = cols.str(i,11);
            c.queryType  = cols.str(i,12);
            info.columns.push_back(c);
        }
        return info;
    }

    // ── 代码生成模板 ─────────────────────────────────────────────────────────────
    std::string genController(const TableInfo& t) {
        auto* pkColPtr = t.pkCol();
        std::string pk      = pkColPtr ? pkColPtr->colName  : "id";
        std::string pkField = pkColPtr ? pkColPtr->javaField : "id";
        std::string cls     = t.className;
        std::string path    = "/" + t.moduleName + "/" + t.businessName;
        std::string perm    = t.moduleName + ":" + t.businessName;

        // 构建 SELECT 列列表（明确列，避免 SELECT *）
        std::string selectCols;
        for (size_t i = 0; i < t.columns.size(); ++i) {
            selectCols += t.columns[i].colName;
            if (i + 1 < t.columns.size()) selectCols += ",";
        }

        std::ostringstream o;
        o << "#pragma once\n";
        o << "// 由代码生成器生成 - 表: " << t.tableName << " (" << t.tableComment << ")\n";
        o << "#include <drogon/HttpController.h>\n";
        o << "#include <sstream>\n";
        o << "#include \"../../common/AjaxResult.h\"\n";
        o << "#include \"../../common/PageUtils.h\"\n";
        o << "#include \"../../filters/PermFilter.h\"\n";
        o << "#include \"../../services/DatabaseService.h\"\n\n";
        o << "class " << cls << "Ctrl : public drogon::HttpController<" << cls << "Ctrl> {\npublic:\n";
        o << "    METHOD_LIST_BEGIN\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::list,    \"" << path << "/list\",   drogon::Get,    \"JwtAuthFilter\");\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::exportData, \"" << path << "/export\", drogon::Post, \"JwtAuthFilter\");\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::getById, \"" << path << "/{id}\",   drogon::Get,    \"JwtAuthFilter\");\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::add,     \"" << path << "\",        drogon::Post,   \"JwtAuthFilter\");\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::edit,    \"" << path << "\",        drogon::Put,    \"JwtAuthFilter\");\n";
        o << "        ADD_METHOD_TO(" << cls << "Ctrl::remove,  \"" << path << "/{ids}\",  drogon::Delete, \"JwtAuthFilter\");\n";
        o << "    METHOD_LIST_END\n\n";

        // ── list ────────────────────────────────────────────────────
        o << "    void list(const drogon::HttpRequestPtr &req,\n"
             "              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":list\");\n";
        o << "        auto page = PageParam::fromRequest(req);\n";
        o << "        auto& db  = DatabaseService::instance();\n";
        o << "        std::string where = \" WHERE 1=1\";\n";
        o << "        std::vector<std::string> params;\n";
        o << "        int idx = 1;\n";
        for (auto& c : t.columns) {
            if (!c.isQuery) continue;
            o << "        { auto v = req->getParameter(\"" << c.javaField << "\");\n";
            o << "          if (!v.empty()) { where += \" AND " << c.colName << " LIKE $\" + std::to_string(idx++);\n";
            o << "                           params.push_back(\"%\" + v + \"%\"); } }\n";
        }
        o << "        std::string cntSql = \"SELECT COUNT(*) FROM " << t.tableName << "\" + where;\n";
        o << "        auto cntRes = params.empty() ? db.query(cntSql) : db.queryParams(cntSql, params);\n";
        o << "        long total  = cntRes.ok() && cntRes.rows() > 0 ? cntRes.longVal(0, 0) : 0;\n";
        o << "        std::string sql = \"SELECT " << selectCols << " FROM " << t.tableName << "\"\n";
        o << "            + where + \" ORDER BY " << pk << " DESC\"\n";
        o << "            + \" LIMIT \" + std::to_string(page.pageSize)\n";
        o << "            + \" OFFSET \" + std::to_string(page.offset());\n";
        o << "        auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);\n";
        o << "        Json::Value rows(Json::arrayValue);\n";
        o << "        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {\n";
        o << "            Json::Value j;\n";
        { int col = 0;
          for (auto& c : t.columns)
              o << "            j[\"" << c.javaField << "\"] = res.str(i, " << col++ << ");\n"; }
        o << "            rows.append(j);\n";
        o << "        }\n";
        o << "        PageResult pr; pr.total = total; pr.rows = rows;\n";
        o << "        RESP_JSON(cb, pr.toJson());\n    }\n\n";

        // ── export ──────────────────────────────────────────────────
        o << "    void exportData(const drogon::HttpRequestPtr &req,\n"
             "                   std::function<void(const drogon::HttpResponsePtr &)> &&cb) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":export\");\n";
        o << "        auto res = DatabaseService::instance().query(\n";
        o << "            \"SELECT " << selectCols << " FROM " << t.tableName << " ORDER BY " << pk << " DESC\");\n";
        o << "        Json::Value rows(Json::arrayValue);\n";
        o << "        if (res.ok()) for (int i = 0; i < res.rows(); ++i) {\n";
        o << "            Json::Value j;\n";
        { int col = 0;
          for (auto& c : t.columns)
              o << "            j[\"" << c.javaField << "\"] = res.str(i, " << col++ << ");\n"; }
        o << "            rows.append(j);\n";
        o << "        }\n";
        o << "        RESP_OK(cb, rows);\n    }\n\n";

        // ── getById ─────────────────────────────────────────────────
        o << "    void getById(const drogon::HttpRequestPtr &req,\n"
             "                 std::function<void(const drogon::HttpResponsePtr &)> &&cb, long id) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":query\");\n";
        o << "        auto r = DatabaseService::instance().queryParams(\n";
        o << "            \"SELECT " << selectCols << " FROM " << t.tableName << " WHERE " << pk << "=$1\",\n";
        o << "            {std::to_string(id)});\n";
        o << "        if (!r.ok() || r.rows() == 0) { RESP_ERR(cb, \"记录不存在\"); return; }\n";
        o << "        Json::Value j;\n";
        { int col = 0;
          for (auto& c : t.columns)
              o << "        j[\"" << c.javaField << "\"] = r.str(0, " << col++ << ");\n"; }
        o << "        RESP_OK(cb, j);\n    }\n\n";

        // ── add ─────────────────────────────────────────────────────
        o << "    void add(const drogon::HttpRequestPtr &req,\n"
             "             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":add\");\n";
        o << "        auto b = req->getJsonObject();\n";
        o << "        if (!b) { RESP_ERR(cb, \"参数错误\"); return; }\n";
        { std::vector<std::string> iCols, iVals; int pi = 1;
          for (auto& c : t.columns) if (!c.isPk && c.isInsert)
              { iCols.push_back(c.colName); iVals.push_back("$"+std::to_string(pi++)); }
          o << "        DatabaseService::instance().execParams(\n";
          o << "            \"INSERT INTO " << t.tableName << "(";
          for (size_t i=0;i<iCols.size();++i) o << iCols[i] << (i+1<iCols.size()?",":"");
          o << ",create_time) VALUES(";
          for (size_t i=0;i<iVals.size();++i) o << iVals[i] << (i+1<iVals.size()?",":"");
          o << ",NOW())\",\n            {";
          bool first = true;
          for (auto& c : t.columns) if (!c.isPk && c.isInsert) {
              if (!first) o << ", "; first = false;
              o << "(*b)[\"" << c.javaField << "\"].asString()";
          }
          o << "});\n"; }
        o << "        RESP_MSG(cb, \"操作成功\");\n    }\n\n";

        // ── edit ────────────────────────────────────────────────────
        o << "    void edit(const drogon::HttpRequestPtr &req,\n"
             "              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":edit\");\n";
        o << "        auto b = req->getJsonObject();\n";
        o << "        if (!b) { RESP_ERR(cb, \"参数错误\"); return; }\n";
        { std::vector<std::string> uParts; int pi = 1;
          for (auto& c : t.columns) if (!c.isPk && c.isEdit)
              uParts.push_back(c.colName + "=$" + std::to_string(pi++));
          o << "        DatabaseService::instance().execParams(\n";
          o << "            \"UPDATE " << t.tableName << " SET ";
          for (size_t i=0;i<uParts.size();++i) o << uParts[i] << (i+1<uParts.size()?",":"");
          o << ",update_time=NOW() WHERE " << pk << "=$" << pi << "\",\n            {";
          bool first = true;
          for (auto& c : t.columns) if (!c.isPk && c.isEdit) {
              if (!first) o << ", "; first = false;
              o << "(*b)[\"" << c.javaField << "\"].asString()";
          }
          o << ", (*b)[\"" << pkField << "\"].asString()});\n"; }
        o << "        RESP_MSG(cb, \"操作成功\");\n    }\n\n";

        // ── remove ──────────────────────────────────────────────────
        o << "    void remove(const drogon::HttpRequestPtr &req,\n"
             "                std::function<void(const drogon::HttpResponsePtr &)> &&cb,\n"
             "                const std::string &ids) {\n";
        o << "        CHECK_PERM(req, cb, \"" << perm << ":remove\");\n";
        o << "        std::istringstream ss(ids); std::string id;\n";
        o << "        while (std::getline(ss, id, ','))\n";
        o << "            DatabaseService::instance().execParams(\n";
        o << "                \"DELETE FROM " << t.tableName << " WHERE " << pk << "=$1\", {id});\n";
        o << "        RESP_MSG(cb, \"操作成功\");\n    }\n";
        o << "};\n";
        return o.str();
    }

    std::string genService(const TableInfo& t) {
        auto* pkColPtr = t.pkCol();
        std::string pk = pkColPtr ? pkColPtr->colName : "id";
        std::ostringstream o;
        o << "#pragma once\n";
        o << "// 由代码生成器生成 - " << t.tableComment << "\n";
        o << "#include <sstream>\n";
        o << "#include <vector>\n";
        o << "#include <string>\n";
        o << "#include \"../../services/DatabaseService.h\"\n\n";
        o << "class " << t.className << "Service {\npublic:\n";
        o << "    static " << t.className << "Service& instance() {\n";
        o << "        static " << t.className << "Service inst; return inst;\n    }\n\n";

        // list
        o << "    DatabaseService::QueryResult list(\n";
        o << "        const std::string& where = \"\",\n";
        o << "        const std::vector<std::string>& params = {},\n";
        o << "        int pageSize = 10, int offset = 0) {\n";
        o << "        std::string sql = \"SELECT * FROM " << t.tableName << " WHERE 1=1\" + where\n";
        o << "            + \" ORDER BY " << pk << " DESC LIMIT \" + std::to_string(pageSize)\n";
        o << "            + \" OFFSET \" + std::to_string(offset);\n";
        o << "        return params.empty() ? DatabaseService::instance().query(sql)\n";
        o << "                             : DatabaseService::instance().queryParams(sql, params);\n    }\n\n";

        // count
        o << "    long count(const std::string& where = \"\", const std::vector<std::string>& params = {}) {\n";
        o << "        std::string sql = \"SELECT COUNT(*) FROM " << t.tableName << " WHERE 1=1\" + where;\n";
        o << "        auto r = params.empty() ? DatabaseService::instance().query(sql)\n";
        o << "                               : DatabaseService::instance().queryParams(sql, params);\n";
        o << "        return r.ok() && r.rows() > 0 ? r.longVal(0, 0) : 0;\n    }\n\n";

        // getById
        if (pkColPtr) {
            o << "    DatabaseService::QueryResult getById(long id) {\n";
            o << "        return DatabaseService::instance().queryParams(\n";
            o << "            \"SELECT * FROM " << t.tableName << " WHERE " << pk << "=$1\",\n";
            o << "            {std::to_string(id)});\n    }\n\n";
        }

        // exists
        if (pkColPtr) {
            o << "    bool exists(long id) {\n";
            o << "        auto r = DatabaseService::instance().queryParams(\n";
            o << "            \"SELECT 1 FROM " << t.tableName << " WHERE " << pk << "=$1 LIMIT 1\",\n";
            o << "            {std::to_string(id)});\n";
            o << "        return r.ok() && r.rows() > 0;\n    }\n\n";
        }

        // remove
        o << "    void remove(const std::string& ids) {\n";
        o << "        std::istringstream ss(ids); std::string id;\n";
        o << "        while (std::getline(ss, id, ','))\n";
        if (pkColPtr)
            o << "            DatabaseService::instance().execParams(\n"
                 "                \"DELETE FROM " << t.tableName << " WHERE " << pk << "=$1\", {id});\n";
        else
            o << "            (void)id;\n";
        o << "    }\n";
        o << "};\n";
        return o.str();
    }

    std::string genApi(const TableInfo& t) {
        std::ostringstream o;
        std::string api = "/" + t.moduleName + "/" + t.businessName;
        std::string fn  = t.businessName;
        std::string Cls = t.className;
        o << "import request from '@/utils/request'\n\n";
        o << "// " << t.tableComment << "\n\n";
        o << "export function list" << Cls << "(query) {\n";
        o << "  return request({ url: '" << api << "/list', method: 'get', params: query })\n}\n\n";
        o << "export function get" << Cls << "(" << fn << "Id) {\n";
        o << "  return request({ url: '" << api << "/' + " << fn << "Id, method: 'get' })\n}\n\n";
        o << "export function add" << Cls << "(data) {\n";
        o << "  return request({ url: '" << api << "', method: 'post', data })\n}\n\n";
        o << "export function update" << Cls << "(data) {\n";
        o << "  return request({ url: '" << api << "', method: 'put', data })\n}\n\n";
        o << "export function del" << Cls << "(" << fn << "Ids) {\n";
        o << "  return request({ url: '" << api << "/' + " << fn << "Ids, method: 'delete' })\n}\n\n";
        o << "export function export" << Cls << "(query) {\n";
        o << "  return request({ url: '" << api << "/export', method: 'post', params: query, responseType: 'blob' })\n}\n";
        return o.str();
    }

    std::string genSql(const TableInfo& t) {
        std::ostringstream o;
        o << "-- " << t.tableComment << "\n";
        o << "-- 表名: " << t.tableName << "\n";
        o << "-- 由代码生成器生成\n\n";
        o << "CREATE TABLE IF NOT EXISTS " << t.tableName << " (\n";
        bool hasPk = (t.pkCol() != nullptr);
        for (size_t i = 0; i < t.columns.size(); ++i) {
            auto& c = t.columns[i];
            std::string sqlType;
            if (c.isPk)
                sqlType = "BIGSERIAL";  // 自增主键
            else
                sqlType = javaTypeToSql(c.javaType);
            o << "    " << c.colName << "  " << sqlType;
            if (c.isPk) o << "  PRIMARY KEY";
            if (!c.colComment.empty()) o << ",  -- " << c.colComment << "\n";
            else o << ",\n";
        }
        // 标准审计字段（如果未在列中）
        auto hasCol = [&](const std::string& name) {
            for (auto& c : t.columns) if (c.colName == name) return true;
            return false;
        };
        if (!hasCol("create_by"))   o << "    create_by    VARCHAR(64)  DEFAULT '',\n";
        if (!hasCol("create_time")) o << "    create_time  TIMESTAMP,\n";
        if (!hasCol("update_by"))   o << "    update_by    VARCHAR(64)  DEFAULT '',\n";
        if (!hasCol("update_time")) o << "    update_time  TIMESTAMP,\n";
        if (!hasCol("remark"))      o << "    remark       VARCHAR(500) DEFAULT ''\n";
        o << ");\n\n";
        // 注释
        o << "COMMENT ON TABLE " << t.tableName << " IS '" << t.tableComment << "';\n";
        for (auto& c : t.columns) if (!c.colComment.empty())
            o << "COMMENT ON COLUMN " << t.tableName << "." << c.colName
              << " IS '" << c.colComment << "';\n";
        return o.str();
    }

    std::string genVue(const TableInfo& t) {
        std::ostringstream o;
        std::string pk   = t.pkCol() ? t.pkCol()->javaField : "id";
        std::string Cls  = t.className;
        std::string perm = t.moduleName + ":" + t.businessName;

        // 查询字段
        std::vector<const ColInfo*> queryFields, editFields, listFields;
        for (auto& c : t.columns) {
            if (c.isQuery && !c.isPk) queryFields.push_back(&c);
            if (c.isEdit  && !c.isPk) editFields.push_back(&c);
            if (c.isList  && !c.isPk) listFields.push_back(&c);
        }

        // ── template ────────────────────────────────────────────────
        o << "<template>\n  <div class=\"app-container\">\n\n";

        // 搜索栏
        if (!queryFields.empty()) {
            o << "    <!-- 搜索表单 -->\n";
            o << "    <el-form :model=\"queryParams\" ref=\"queryForm\" size=\"small\" :inline=\"true\" v-show=\"showSearch\">\n";
            for (auto* c : queryFields) {
                o << "      <el-form-item label=\"" << c->colComment << "\" prop=\"" << c->javaField << "\">\n";
                o << "        <el-input v-model=\"queryParams." << c->javaField << "\"\n";
                o << "          placeholder=\"请输入" << c->colComment << "\" clearable @keyup.enter.native=\"handleQuery\" />\n";
                o << "      </el-form-item>\n";
            }
            o << "      <el-form-item>\n";
            o << "        <el-button type=\"primary\" icon=\"el-icon-search\" size=\"mini\" @click=\"handleQuery\">搜索</el-button>\n";
            o << "        <el-button icon=\"el-icon-refresh\" size=\"mini\" @click=\"resetQuery\">重置</el-button>\n";
            o << "      </el-form-item>\n";
            o << "    </el-form>\n\n";
        }

        // 工具栏
        o << "    <!-- 操作工具栏 -->\n";
        o << "    <el-row :gutter=\"10\" class=\"mb8\">\n";
        o << "      <el-col :span=\"1.5\">\n";
        o << "        <el-button type=\"primary\" plain icon=\"el-icon-plus\" size=\"mini\"\n";
        o << "          v-hasPermi=\"['" << perm << ":add']\"\n";
        o << "          @click=\"handleAdd\">新增</el-button>\n";
        o << "      </el-col>\n";
        o << "      <el-col :span=\"1.5\">\n";
        o << "        <el-button type=\"warning\" plain icon=\"el-icon-download\" size=\"mini\"\n";
        o << "          v-hasPermi=\"['" << perm << ":export']\"\n";
        o << "          @click=\"handleExport\">导出</el-button>\n";
        o << "      </el-col>\n";
        o << "      <right-toolbar :showSearch.sync=\"showSearch\" @queryTable=\"getList\" />\n";
        o << "    </el-row>\n\n";

        // 表格
        o << "    <!-- 数据表格 -->\n";
        o << "    <el-table v-loading=\"loading\" :data=\"list\" @selection-change=\"handleSelectionChange\">\n";
        o << "      <el-table-column type=\"selection\" width=\"55\" align=\"center\" />\n";
        for (auto* c : listFields)
            o << "      <el-table-column prop=\"" << c->javaField << "\" label=\"" << c->colComment << "\" align=\"center\" />\n";
        o << "      <el-table-column label=\"操作\" align=\"center\" class-name=\"small-padding fixed-width\">\n";
        o << "        <template slot-scope=\"scope\">\n";
        o << "          <el-button size=\"mini\" type=\"text\" icon=\"el-icon-edit\"\n";
        o << "            v-hasPermi=\"['" << perm << ":edit']\"\n";
        o << "            @click=\"handleUpdate(scope.row)\">修改</el-button>\n";
        o << "          <el-button size=\"mini\" type=\"text\" icon=\"el-icon-delete\"\n";
        o << "            v-hasPermi=\"['" << perm << ":remove']\"\n";
        o << "            @click=\"handleDelete(scope.row)\">删除</el-button>\n";
        o << "        </template>\n";
        o << "      </el-table-column>\n";
        o << "    </el-table>\n\n";

        // 分页
        o << "    <pagination v-show=\"total > 0\" :total=\"total\"\n";
        o << "      :page.sync=\"queryParams.pageNum\" :limit.sync=\"queryParams.pageSize\"\n";
        o << "      @pagination=\"getList\" />\n\n";

        // 对话框
        o << "    <!-- 新增/编辑对话框 -->\n";
        o << "    <el-dialog :title=\"title\" :visible.sync=\"open\" width=\"500px\" append-to-body>\n";
        o << "      <el-form ref=\"form\" :model=\"form\" :rules=\"rules\" label-width=\"100px\">\n";
        for (auto* c : editFields) {
            o << "        <el-form-item label=\"" << c->colComment << "\" prop=\"" << c->javaField << "\">\n";
            if (!c->dictType.empty()) {
                o << "          <el-select v-model=\"form." << c->javaField << "\" placeholder=\"请选择" << c->colComment << "\">\n";
                o << "            <el-option v-for=\"d in dict.type." << c->dictType << "\"\n";
                o << "              :key=\"d.value\" :label=\"d.label\" :value=\"d.value\" />\n";
                o << "          </el-select>\n";
            } else if (c->htmlType == "textarea") {
                o << "          <el-input v-model=\"form." << c->javaField << "\" type=\"textarea\" placeholder=\"请输入" << c->colComment << "\" />\n";
            } else {
                o << "          <el-input v-model=\"form." << c->javaField << "\" placeholder=\"请输入" << c->colComment << "\" />\n";
            }
            o << "        </el-form-item>\n";
        }
        o << "      </el-form>\n";
        o << "      <div slot=\"footer\" class=\"dialog-footer\">\n";
        o << "        <el-button type=\"primary\" @click=\"submitForm\">确 定</el-button>\n";
        o << "        <el-button @click=\"cancel\">取 消</el-button>\n";
        o << "      </div>\n";
        o << "    </el-dialog>\n";
        o << "  </div>\n</template>\n\n";

        // ── script ──────────────────────────────────────────────────
        // 收集所有 dictType
        std::vector<std::string> dictTypes;
        for (auto& c : t.columns)
            if (!c.dictType.empty()) dictTypes.push_back(c.dictType);

        o << "<script>\n";
        o << "import { list" << Cls << ", get" << Cls << ", add" << Cls
          << ", update" << Cls << ", del" << Cls << ", export" << Cls << " } from '@/api/"
          << t.moduleName << "/" << t.businessName << "'\n\n";
        o << "export default {\n";
        o << "  name: '" << Cls << "',\n";
        if (!dictTypes.empty()) {
            o << "  dicts: [";
            for (size_t i = 0; i < dictTypes.size(); ++i)
                o << "'" << dictTypes[i] << "'" << (i+1<dictTypes.size()?", ":"");
            o << "],\n";
        }
        o << "  data() {\n    return {\n";
        o << "      loading: false,\n      showSearch: true,\n";
        o << "      total: 0,\n      list: [],\n      ids: [],\n";
        o << "      open: false,\n      title: '',\n";
        o << "      queryParams: { pageNum: 1, pageSize: 10";
        for (auto* c : queryFields) o << ", " << c->javaField << ": undefined";
        o << " },\n";
        o << "      form: {},\n";
        // 生成校验规则（isRequired 的字段）
        o << "      rules: {\n";
        bool firstRule = true;
        for (auto& c : t.columns) {
            if (!c.isEdit || c.isPk) continue;
            // 这里简单对所有编辑字段生成规则占位，required 字段加必填
            if (!firstRule) o << ",\n"; firstRule = false;
            o << "        " << c.javaField << ": [{ required: true, message: '请输入" << c.colComment << "', trigger: 'blur' }]";
        }
        o << "\n      }\n    }\n  },\n";
        o << "  created() { this.getList() },\n";
        o << "  methods: {\n";
        o << "    getList() {\n";
        o << "      this.loading = true\n";
        o << "      list" << Cls << "(this.queryParams).then(res => {\n";
        o << "        this.list = res.rows\n        this.total = res.total\n        this.loading = false\n      })\n    },\n";
        o << "    handleQuery() { this.queryParams.pageNum = 1; this.getList() },\n";
        o << "    resetQuery() { this.$refs.queryForm && this.$refs.queryForm.resetFields(); this.handleQuery() },\n";
        o << "    handleSelectionChange(selection) { this.ids = selection.map(s => s." << pk << ") },\n";
        o << "    cancel() { this.open = false; this.reset() },\n";
        o << "    reset() { this.form = {}; this.$refs.form && this.$refs.form.resetFields() },\n";
        o << "    handleAdd() { this.reset(); this.open = true; this.title = '新增" << t.tableComment << "' },\n";
        o << "    handleUpdate(row) {\n      this.reset()\n";
        o << "      get" << Cls << "(row." << pk << ").then(res => {\n";
        o << "        this.form = res.data\n        this.open = true\n        this.title = '修改" << t.tableComment << "'\n      })\n    },\n";
        o << "    submitForm() {\n";
        o << "      this.$refs.form.validate(valid => {\n        if (!valid) return\n";
        o << "        const fn = this.form." << pk << " ? update" << Cls << " : add" << Cls << "\n";
        o << "        fn(this.form).then(() => {\n";
        o << "          this.$modal.msgSuccess(this.form." << pk << " ? '修改成功' : '新增成功')\n";
        o << "          this.open = false\n          this.getList()\n        })\n      })\n    },\n";
        o << "    handleDelete(row) {\n";
        o << "      const delIds = row." << pk << " || this.ids\n";
        o << "      this.$modal.confirm('确认删除选中数据项？').then(() =>\n";
        o << "        del" << Cls << "(delIds).then(() => { this.getList(); this.$modal.msgSuccess('删除成功') }))\n    },\n";
        o << "    handleExport() {\n";
        o << "      this.$modal.confirm('确认导出所有数据项？').then(() => export" << Cls << "(this.queryParams))\n    }\n";
        o << "  }\n}\n</script>\n";
        return o.str();
    }

    // ── 辅助函数 ─────────────────────────────────────────────────────────────────
    static std::string toCamelCase(const std::string& s, bool upper) {
        std::string r; bool cap = upper;
        for (char c : s) {
            if (c=='_') { cap=true; continue; }
            r += cap ? (char)toupper(c) : c;
            cap = false;
        }
        return r;
    }
    static std::string pgTypeToJava(const std::string& t) {
        if (t=="bigint"||t=="int8") return "Long";
        if (t=="integer"||t=="int4"||t=="int") return "Integer";
        if (t=="smallint"||t=="int2") return "Integer";
        if (t=="numeric"||t=="decimal") return "BigDecimal";
        if (t=="double precision"||t=="float8") return "Double";
        if (t=="real"||t=="float4") return "Float";
        if (t=="boolean"||t=="bool") return "Boolean";
        if (t=="timestamp"||t=="timestamptz"||t=="date") return "Date";
        return "String";
    }
    static std::string javaTypeToSql(const std::string& t) {
        if (t=="Long")    return "BIGINT";
        if (t=="Integer") return "INTEGER";
        if (t=="Double"||t=="Float"||t=="BigDecimal") return "NUMERIC";
        if (t=="Boolean") return "BOOLEAN";
        if (t=="Date")    return "TIMESTAMP";
        return "VARCHAR(255)";
    }
    static Json::Value tableRowToJson(const DatabaseService::QueryResult& r, int row) {
        Json::Value j;
        j["tableId"]       = (Json::Int64)r.longVal(row,0);
        j["tableName"]     = r.str(row,1); j["tableComment"] = r.str(row,2);
        j["className"]     = r.str(row,3); j["packageName"]  = r.str(row,4);
        j["moduleName"]    = r.str(row,5); j["businessName"] = r.str(row,6);
        j["functionName"]  = r.str(row,7); j["functionAuthor"]= r.str(row,8);
        return j;
    }
    static Json::Value colRowToJson(const DatabaseService::QueryResult& r, int row) {
        Json::Value j;
        j["columnId"]      = (Json::Int64)r.longVal(row,0);
        j["columnName"]    = r.str(row,1); j["columnComment"] = r.str(row,2);
        j["columnType"]    = r.str(row,3); j["javaType"]      = r.str(row,4);
        j["javaField"]     = r.str(row,5); j["isPk"]          = r.str(row,6);
        j["isIncrement"]   = r.str(row,7); j["isRequired"]    = r.str(row,8);
        j["isInsert"]      = r.str(row,9); j["isEdit"]        = r.str(row,10);
        j["isList"]        = r.str(row,11); j["isQuery"]      = r.str(row,12);
        j["queryType"]     = r.str(row,13); j["htmlType"]     = r.str(row,14);
        j["dictType"]      = r.str(row,15);
        return j;
    }
};

