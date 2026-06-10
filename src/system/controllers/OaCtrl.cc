#include "OaCtrl.h"
#include "../../common/WsBus.h"
#include "../../common/NotifyService.h"
#include <sstream>

namespace {

Json::Value rowToApproval(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["title"] = res.str(i, 1);
    j["approvalType"] = res.str(i, 2);
    j["templateId"] = res.intVal(i, 3);
    j["status"] = res.str(i, 4);
    j["applicantId"] = Json::Int64(res.longVal(i, 5));
    j["applicantName"] = res.str(i, 6);
    j["reviewerId"] = Json::Int64(res.longVal(i, 7));
    j["reviewerName"] = res.str(i, 8);
    j["formData"] = res.str(i, 9);
    j["ccUsers"] = res.str(i, 10);
    j["currentStep"] = res.str(i, 11);
    j["reviewComment"] = res.str(i, 12);
    j["reviewTime"] = fmtTs(res.str(i, 13));
    j["workflowId"] = Json::Int64(res.longVal(i, 14));
    j["nextReviewerId"] = Json::Int64(res.longVal(i, 15));
    j["stepOrder"] = res.intVal(i, 16);
    j["timeoutHours"] = res.intVal(i, 17);
    j["transferTargetId"] = Json::Int64(res.longVal(i, 18));
    j["submittedAt"] = fmtTs(res.str(i, 19));
    j["createdAt"] = fmtTs(res.str(i, 20));
    j["updatedAt"] = fmtTs(res.str(i, 21));
    return j;
}

Json::Value rowToWorkflowNode(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["workflowId"] = Json::Int64(res.longVal(i, 1));
    j["nodeOrder"] = res.intVal(i, 2);
    j["nodeName"] = res.str(i, 3);
    j["reviewerId"] = Json::Int64(res.longVal(i, 4));
    j["reviewerName"] = res.str(i, 5);
    j["timeoutHours"] = res.intVal(i, 6);
    j["nodeType"] = res.str(i, 7);
    j["createTime"] = fmtTs(res.str(i, 8));
    return j;
}

Json::Value rowToMeeting(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["subject"] = res.str(i, 1);
    j["roomName"] = res.str(i, 2);
    j["organizerId"] = Json::Int64(res.longVal(i, 3));
    j["organizerName"] = res.str(i, 4);
    j["startTime"] = fmtTs(res.str(i, 5));
    j["endTime"] = fmtTs(res.str(i, 6));
    j["status"] = res.str(i, 7);
    j["attendees"] = res.str(i, 8);
    j["remark"] = res.str(i, 9);
    return j;
}

Json::Value rowToMeetingRoom(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["roomName"] = res.str(i, 1);
    j["capacity"] = res.intVal(i, 2);
    j["location"] = res.str(i, 3);
    j["managerId"] = Json::Int64(res.longVal(i, 4));
    j["managerName"] = res.str(i, 5);
    j["equipment"] = res.str(i, 6);
    j["status"] = res.str(i, 7);
    return j;
}

Json::Value rowToKnowledge(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["title"] = res.str(i, 1);
    j["category"] = res.str(i, 2);
    j["tags"] = res.str(i, 3);
    j["summary"] = res.str(i, 4);
    j["content"] = res.str(i, 5);
    j["attachments"] = res.str(i, 6);
    j["authorId"] = Json::Int64(res.longVal(i, 7));
    j["authorName"] = res.str(i, 8);
    j["status"] = res.str(i, 9);
    j["viewCount"] = Json::Int64(res.longVal(i, 10));
    j["scope"] = res.str(i, 11);
    j["likeCount"] = Json::Int64(res.longVal(i, 12));
    j["favoriteCount"] = Json::Int64(res.longVal(i, 13));
    j["commentCount"] = Json::Int64(res.longVal(i, 14));
    j["createdAt"] = fmtTs(res.str(i, 15));
    j["updatedAt"] = fmtTs(res.str(i, 16));
    return j;
}

Json::Value rowToKnowledgeComment(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["articleId"] = Json::Int64(res.longVal(i, 1));
    j["userId"] = Json::Int64(res.longVal(i, 2));
    j["userName"] = res.str(i, 3);
    j["content"] = res.str(i, 4);
    j["createTime"] = fmtTs(res.str(i, 5));
    return j;
}

Json::Value rowToVehicle(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["plateNumber"] = res.str(i, 1);
    j["model"] = res.str(i, 2);
    j["driverName"] = res.str(i, 3);
    j["driverPhone"] = res.str(i, 4);
    j["status"] = res.str(i, 5);
    j["remark"] = res.str(i, 6);
    return j;
}

Json::Value rowToBorrowAsset(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["assetName"] = res.str(i, 1);
    j["assetCode"] = res.str(i, 2);
    j["applicantId"] = Json::Int64(res.longVal(i, 3));
    j["applicantName"] = res.str(i, 4);
    j["borrowDate"] = fmtTs(res.str(i, 5));
    j["expectReturnDate"] = fmtTs(res.str(i, 6));
    j["actualReturnDate"] = fmtTs(res.str(i, 7));
    j["purpose"] = res.str(i, 8);
    j["status"] = res.str(i, 9);
    j["createdAt"] = fmtTs(res.str(i, 10));
    return j;
}

Json::Value rowToTodo(DatabaseService::QueryResult& res, int i) {
    Json::Value j;
    j["id"] = Json::Int64(res.longVal(i, 0));
    j["bizType"] = res.str(i, 1);
    j["bizId"] = Json::Int64(res.longVal(i, 2));
    j["title"] = res.str(i, 3);
    j["content"] = res.str(i, 4);
    j["status"] = res.str(i, 5);
    j["priority"] = res.str(i, 6);
    j["dueDate"] = fmtTs(res.str(i, 7));
    j["createdAt"] = fmtTs(res.str(i, 8));
    return j;
}

void sendOaNotify(long userId, const std::string& title, const std::string& content, const std::string& level = "info") {
    NotifyService::sendInbox(userId, title, content, level);
}

void sendMobilePush(long userId, const std::string& title, const std::string& content) {
    auto& db = DatabaseService::instance();
    auto devices = db.queryParams(
        "SELECT push_token, device_type, channel_type FROM sys_mobile_device WHERE user_id=$1 AND enabled=1",
        {std::to_string(userId)});
    if (!devices.ok()) return;
    for (int i = 0; i < devices.rows(); ++i) {
        std::string token = devices.str(i, 0);
        std::string deviceType = devices.str(i, 1);
        std::string channelType = devices.str(i, 2);
        std::string msg = "[移动端推送][" + deviceType + "/" + channelType + "] " + title + " => " + token + " | " + content;
        LOG_INFO << msg;
    }
}

std::string escapeLikeParam(const std::string& keyword) {
    std::string out;
    out.reserve(keyword.size());
    for (char c : keyword) {
        if (c == '%' || c == '_' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool canReadKnowledge(const std::string& scope, long authorId, long userId) {
    if (scope == "public" || scope == "internal") return true;
    return authorId == userId;
}

}

std::vector<std::string> OaCtrl::splitIds(const std::string &ids) {
    std::vector<std::string> r; std::stringstream ss(ids); std::string item;
    while (std::getline(ss, item, ',')) if (!item.empty()) r.push_back(item);
    return r;
}

void OaCtrl::listApprovals(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto& db = DatabaseService::instance();
    std::string sql = R"(
        SELECT a.id,a.title,a.approval_type,a.template_id,a.status,
               a.applicant_id,a.applicant_name,a.reviewer_id,a.reviewer_name,
               a.form_data,a.cc_users,a.current_step,a.review_comment,
               a.review_time,a.workflow_id,a.next_reviewer_id,a.step_order,
               a.timeout_hours,a.transfer_target_id,a.submitted_at,
               a.create_time,a.update_time
        FROM oa_approval a WHERE 1=1)";
    std::vector<std::string> params;
    int idx = 1;
    auto status = req->getParameter("status");
    auto type = req->getParameter("approvalType");
    auto keyword = req->getParameter("keyword");
    if (!status.empty()) { sql += " AND a.status=$" + std::to_string(idx++); params.push_back(status); }
    if (!type.empty()) { sql += " AND a.approval_type=$" + std::to_string(idx++); params.push_back(type); }
    if (!keyword.empty()) {
        sql += " AND (a.title LIKE $" + std::to_string(idx) + " ESCAPE '\\' OR a.applicant_name LIKE $" + std::to_string(idx) + " ESCAPE '\\')";
        params.push_back("%" + escapeLikeParam(keyword) + "%");
        idx++;
    }
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY a.id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToApproval(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::getApproval(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    auto res = DatabaseService::instance().queryParams(
        R"(SELECT a.id,a.title,a.approval_type,a.template_id,a.status,
                  a.applicant_id,a.applicant_name,a.reviewer_id,a.reviewer_name,
                  a.form_data,a.cc_users,a.current_step,a.review_comment,
                  a.review_time,a.workflow_id,a.next_reviewer_id,a.step_order,
                  a.timeout_hours,a.transfer_target_id,a.submitted_at,
                  a.create_time,a.update_time
           FROM oa_approval a WHERE a.id=$1)",
        {std::to_string(id)});
    if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "审批单不存在"); return; }
    RESP_OK(cb, rowToApproval(res, 0));
}

void OaCtrl::createApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto& db = DatabaseService::instance();
    auto formData = (*body).get("formData", Json::Value(Json::objectValue)).toStyledString();
    auto ccUsers = (*body).get("ccUsers", Json::Value(Json::arrayValue)).toStyledString();
    long workflowId = (*body).get("workflowId", 0).asInt64();
    long reviewerId = (*body).get("reviewerId", 0).asInt64();
    std::string reviewerName = (*body).get("reviewerName", "").asString();
    int stepOrder = 1;
    int timeoutHours = 72;
    if (workflowId > 0) {
        auto node = db.queryParams(
            "SELECT reviewer_id,reviewer_name,timeout_hours,node_order FROM oa_workflow_node WHERE workflow_id=$1 ORDER BY node_order ASC LIMIT 1",
            {std::to_string(workflowId)});
        if (node.ok() && node.rows() > 0) {
            reviewerId = node.longVal(0, 0);
            reviewerName = node.str(0, 1);
            timeoutHours = node.intVal(0, 2);
            stepOrder = node.intVal(0, 3);
        }
    }
    auto res = db.queryParams(
        R"(INSERT INTO oa_approval(title,approval_type,template_id,status,applicant_id,applicant_name,
           reviewer_id,reviewer_name,form_data,cc_users,current_step,workflow_id,next_reviewer_id,
           step_order,timeout_hours,transfer_target_id,submitted_at,create_by,create_time)
           VALUES($1,$2,$3,'pending',$4,$5,$6,$7,$8,$9,'submitted',$10,$11,$12,$13,$14,NOW(),$15,NOW())
           RETURNING id)",
        {(*body).get("title","").asString(), (*body).get("approvalType","general").asString(),
         std::to_string((*body).get("templateId",0).asInt64()), std::to_string(GET_USER_ID(req)),
         GET_USER_NAME(req), std::to_string(reviewerId), reviewerName, formData, ccUsers,
         std::to_string(workflowId), std::to_string(reviewerId), std::to_string(stepOrder),
         std::to_string(timeoutHours), std::to_string((*body).get("transferTargetId",0).asInt64()), GET_USER_NAME(req)});
    if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "创建失败"); return; }
    long long newId = res.longVal(0, 0);
    LOG_OPER(req, "OA审批", BusinessType::INSERT);
    if (reviewerId > 0) {
        std::string msg = "您有新的审批单待处理：" + (*body).get("title","").asString();
        sendOaNotify(reviewerId, "待审批", msg, "warn");
        sendMobilePush(reviewerId, "待审批", msg);
    }
    Json::Value j; j["id"] = Json::Int64(newId);
    RESP_OK(cb, j);
}

void OaCtrl::updateApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto formData = (*body).get("formData", Json::Value(Json::objectValue)).toStyledString();
    auto ccUsers = (*body).get("ccUsers", Json::Value(Json::arrayValue)).toStyledString();
    DatabaseService::instance().execParams(
        R"(UPDATE oa_approval SET title=$1,approval_type=$2,template_id=$3,reviewer_id=$4,
           reviewer_name=$5,form_data=$6,cc_users=$7,workflow_id=$8,timeout_hours=$9,
           transfer_target_id=$10,update_by=$11,update_time=NOW() WHERE id=$12 AND status IN ('draft','pending'))",
        {(*body).get("title","").asString(), (*body).get("approvalType","general").asString(),
         std::to_string((*body).get("templateId",0).asInt64()), std::to_string((*body).get("reviewerId",0).asInt64()),
         (*body).get("reviewerName","").asString(), formData, ccUsers, std::to_string((*body).get("workflowId",0).asInt64()),
         std::to_string((*body).get("timeoutHours",72).asInt()), std::to_string((*body).get("transferTargetId",0).asInt64()),
         GET_USER_NAME(req), std::to_string((*body).get("id",0).asInt64())});
    LOG_OPER(req, "OA审批", BusinessType::UPDATE);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::reviewApproval(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto& db = DatabaseService::instance();
    auto action = (*body).get("action", "approve").asString();
    auto current = db.queryParams(
        "SELECT applicant_id,title,workflow_id,step_order,cc_users FROM oa_approval WHERE id=$1", {std::to_string(id)});
    if (!current.ok() || current.rows() == 0) { RESP_ERR(cb, "审批单不存在"); return; }
    long applicantId = current.longVal(0, 0);
    std::string title = current.str(0, 1);
    long workflowId = current.longVal(0, 2);
    int stepOrder = current.intVal(0, 3);
    std::string ccUsers = current.str(0, 4);
    if (action == "reject") {
        db.execParams(
            R"(UPDATE oa_approval SET status='rejected',current_step='rejected',review_comment=$1,
               review_time=NOW(),update_by=$2,update_time=NOW() WHERE id=$3 AND status='pending')",
            {(*body).get("comment","").asString(), GET_USER_NAME(req), std::to_string(id)});
        std::string msg = "您的审批[" + title + "]已被驳回";
        sendOaNotify(applicantId, "审批驳回", msg, "error");
        sendMobilePush(applicantId, "审批驳回", msg);
        RESP_MSG(cb, "已驳回");
        return;
    }
    if (workflowId > 0) {
        auto nextNode = db.queryParams(
            "SELECT reviewer_id,reviewer_name,timeout_hours,node_order,node_name FROM oa_workflow_node WHERE workflow_id=$1 AND node_order>$2 ORDER BY node_order ASC LIMIT 1",
            {std::to_string(workflowId), std::to_string(stepOrder)});
        if (nextNode.ok() && nextNode.rows() > 0) {
            long nextReviewerId = nextNode.longVal(0, 0);
            std::string nextReviewerName = nextNode.str(0, 1);
            int timeoutHours = nextNode.intVal(0, 2);
            int nextStepOrder = nextNode.intVal(0, 3);
            std::string nodeName = nextNode.str(0, 4);
            db.execParams(
                R"(UPDATE oa_approval SET reviewer_id=$1,reviewer_name=$2,next_reviewer_id=$1,
                   step_order=$3,timeout_hours=$4,current_step=$5,review_comment=$6,
                   review_time=NOW(),update_by=$7,update_time=NOW() WHERE id=$8 AND status='pending')",
                {std::to_string(nextReviewerId), nextReviewerName, std::to_string(nextStepOrder), std::to_string(timeoutHours),
                 nodeName, (*body).get("comment","").asString(), GET_USER_NAME(req), std::to_string(id)});
            std::string msg = "您有新的审批节点待处理：" + title;
            sendOaNotify(nextReviewerId, "待审批", msg, "warn");
            sendMobilePush(nextReviewerId, "待审批", msg);
            RESP_MSG(cb, "已流转到下一审批节点");
            return;
        }
    }
    db.execParams(
        R"(UPDATE oa_approval SET status='approved',current_step='approved',review_comment=$1,
           review_time=NOW(),update_by=$2,update_time=NOW() WHERE id=$3 AND status='pending')",
        {(*body).get("comment","").asString(), GET_USER_NAME(req), std::to_string(id)});
    std::string msg = "您的审批[" + title + "]已通过";
    sendOaNotify(applicantId, "审批通过", msg, "success");
    sendMobilePush(applicantId, "审批通过", msg);
    (void)ccUsers;
    RESP_MSG(cb, "已通过");
}

void OaCtrl::listApprovalTemplates(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto& db = DatabaseService::instance();
    auto keyword = req->getParameter("keyword");
    std::string sql = "SELECT id,name,approval_type,form_schema,status,create_time FROM oa_approval_template WHERE status='active'";
    std::vector<std::string> params;
    if (!keyword.empty()) {
        sql += " AND name LIKE $1 ESCAPE '\\'";
        params.push_back("%" + escapeLikeParam(keyword) + "%");
    }
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) {
        Json::Value j;
        j["id"] = Json::Int64(res.longVal(i, 0));
        j["name"] = res.str(i, 1);
        j["approvalType"] = res.str(i, 2);
        j["formSchema"] = res.str(i, 3);
        j["status"] = res.str(i, 4);
        j["createTime"] = fmtTs(res.str(i, 5));
        rows.append(j);
    }
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::listWorkflowNodes(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long workflowId) {
    auto res = DatabaseService::instance().queryParams(
        "SELECT id,workflow_id,node_order,node_name,reviewer_id,reviewer_name,timeout_hours,node_type,create_time FROM oa_workflow_node WHERE workflow_id=$1 ORDER BY node_order ASC",
        {std::to_string(workflowId)});
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToWorkflowNode(res, i));
    RESP_OK(cb, rows);
}

void OaCtrl::listMeetings(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto& db = DatabaseService::instance();
    std::string sql = "SELECT id,subject,room_name,organizer_id,organizer_name,start_time,end_time,status,attendees,remark FROM oa_meeting_room_booking WHERE 1=1";
    std::vector<std::string> params; int idx = 1;
    auto status = req->getParameter("status");
    auto room = req->getParameter("roomName");
    if (!status.empty()) { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }
    if (!room.empty()) { sql += " AND room_name=$" + std::to_string(idx++); params.push_back(room); }
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY start_time DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToMeeting(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::createMeeting(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto attendees = (*body).get("attendees", Json::Value(Json::arrayValue)).toStyledString();
    auto startTime = (*body).get("startTime", "").asString();
    auto endTime = (*body).get("endTime", "").asString();
    auto roomName = (*body).get("roomName", "").asString();
    auto& db = DatabaseService::instance();
    auto roomRes = db.queryParams("SELECT status FROM oa_meeting_room WHERE room_name=$1", {roomName});
    if (!roomRes.ok() || roomRes.rows() == 0) { RESP_ERR(cb, "会议室不存在"); return; }
    if (roomRes.str(0, 0) != "enabled") { RESP_ERR(cb, "会议室当前不可预约"); return; }
    auto conflict = db.queryParams(
        R"(SELECT id FROM oa_meeting_room_booking WHERE room_name=$1 AND status IN ('scheduled','in_progress')
           AND start_time < $3 AND end_time > $2 LIMIT 1)",
        {roomName, startTime, endTime});
    if (conflict.ok() && conflict.rows() > 0) { RESP_ERR(cb, "该时段会议室已被预约，请选择其他时间或会议室"); return; }
    db.execParams(
        R"(INSERT INTO oa_meeting_room_booking(subject,room_name,organizer_id,organizer_name,start_time,end_time,status,attendees,remark,create_by,create_time)
           VALUES($1,$2,$3,$4,$5,$6,'scheduled',$7,$8,$9,NOW()))",
        {(*body).get("subject", "").asString(), roomName, std::to_string(GET_USER_ID(req)), GET_USER_NAME(req),
         startTime, endTime, attendees, (*body).get("remark", "").asString(), GET_USER_NAME(req)});
    LOG_OPER(req, "会议室预约", BusinessType::INSERT);
    RESP_MSG(cb, "预约成功");
}

void OaCtrl::updateMeeting(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto attendees = (*body).get("attendees", Json::Value(Json::arrayValue)).toStyledString();
    auto startTime = (*body).get("startTime", "").asString();
    auto endTime = (*body).get("endTime", "").asString();
    auto roomName = (*body).get("roomName", "").asString();
    auto id = (*body).get("id", 0).asInt64();
    auto conflict = DatabaseService::instance().queryParams(
        R"(SELECT id FROM oa_meeting_room_booking WHERE room_name=$1 AND status IN ('scheduled','in_progress')
           AND id!=$4 AND start_time < $3 AND end_time > $2 LIMIT 1)",
        {roomName, startTime, endTime, std::to_string(id)});
    if (conflict.ok() && conflict.rows() > 0) { RESP_ERR(cb, "该时段会议室已被预约，请选择其他时间或会议室"); return; }
    DatabaseService::instance().execParams(
        R"(UPDATE oa_meeting_room_booking SET subject=$1,room_name=$2,start_time=$3,end_time=$4,status=$5,attendees=$6,remark=$7,update_by=$8,update_time=NOW() WHERE id=$9)",
        {(*body).get("subject", "").asString(), roomName, startTime, endTime, (*body).get("status", "scheduled").asString(),
         attendees, (*body).get("remark", "").asString(), GET_USER_NAME(req), std::to_string(id)});
    LOG_OPER(req, "会议室预约", BusinessType::UPDATE);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::listMeetingRooms(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto keyword = req->getParameter("keyword");
    std::string sql = "SELECT id,room_name,capacity,location,manager_id,manager_name,equipment,status FROM oa_meeting_room WHERE 1=1";
    std::vector<std::string> params;
    if (!keyword.empty()) {
        sql += " AND room_name LIKE $1 ESCAPE '\\'";
        params.push_back("%" + escapeLikeParam(keyword) + "%");
    }
    auto& db = DatabaseService::instance();
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToMeetingRoom(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::createMeetingRoom(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    DatabaseService::instance().execParams(
        R"(INSERT INTO oa_meeting_room(room_name,capacity,location,manager_id,manager_name,equipment,status,create_by,create_time)
           VALUES($1,$2,$3,$4,$5,$6,$7,$8,NOW()))",
        {(*body).get("roomName", "").asString(), std::to_string((*body).get("capacity", 0).asInt()),
         (*body).get("location", "").asString(), std::to_string((*body).get("managerId", 0).asInt64()),
         (*body).get("managerName", "").asString(), (*body).get("equipment", Json::Value(Json::arrayValue)).toStyledString(),
         (*body).get("status", "enabled").asString(), GET_USER_NAME(req)});
    LOG_OPER(req, "会议室台账", BusinessType::INSERT);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::updateMeetingRoom(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    DatabaseService::instance().execParams(
        R"(UPDATE oa_meeting_room SET room_name=$1,capacity=$2,location=$3,manager_id=$4,manager_name=$5,equipment=$6,status=$7,update_by=$8,update_time=NOW() WHERE id=$9)",
        {(*body).get("roomName", "").asString(), std::to_string((*body).get("capacity", 0).asInt()),
         (*body).get("location", "").asString(), std::to_string((*body).get("managerId", 0).asInt64()),
         (*body).get("managerName", "").asString(), (*body).get("equipment", Json::Value(Json::arrayValue)).toStyledString(),
         (*body).get("status", "enabled").asString(), GET_USER_NAME(req), std::to_string((*body).get("id", 0).asInt64())});
    LOG_OPER(req, "会议室台账", BusinessType::UPDATE);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::listKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto& db = DatabaseService::instance();
    long userId = GET_USER_ID(req);
    std::string sql = R"(
        SELECT id,title,category,tags,summary,content,attachments,author_id,author_name,status,view_count,scope,like_count,favorite_count,comment_count,create_time,update_time
        FROM oa_knowledge_article WHERE 1=1 AND (scope IN ('public','internal') OR author_id=)";
    sql += std::to_string(userId);
    std::vector<std::string> params; int idx = 1;
    auto category = req->getParameter("category");
    auto keyword = req->getParameter("keyword");
    auto tag = req->getParameter("tag");
    if (!category.empty()) { sql += " AND category=$" + std::to_string(idx++); params.push_back(category); }
    if (!keyword.empty()) {
        sql += " AND (title LIKE $" + std::to_string(idx) + " ESCAPE '\\' OR summary LIKE $" + std::to_string(idx) + " ESCAPE '\\')";
        params.push_back("%" + escapeLikeParam(keyword) + "%"); idx++;
    }
    if (!tag.empty()) { sql += " AND tags LIKE $" + std::to_string(idx++) + " ESCAPE '\\'"; params.push_back("%" + escapeLikeParam(tag) + "%"); }
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToKnowledge(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::getKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    auto& db = DatabaseService::instance();
    auto res = db.queryParams(
        "SELECT id,title,category,tags,summary,content,attachments,author_id,author_name,status,view_count,scope,like_count,favorite_count,comment_count,create_time,update_time FROM oa_knowledge_article WHERE id=$1",
        {std::to_string(id)});
    if (!res.ok() || res.rows() == 0) { RESP_ERR(cb, "知识文档不存在"); return; }
    long userId = GET_USER_ID(req);
    if (!canReadKnowledge(res.str(0, 11), res.longVal(0, 7), userId)) { RESP_ERR(cb, "无权查看该文档"); return; }
    db.execParams("UPDATE oa_knowledge_article SET view_count=view_count+1 WHERE id=$1", {std::to_string(id)});
    auto detail = rowToKnowledge(res, 0);
    detail["viewCount"] = Json::Int64(detail["viewCount"].asInt64() + 1);
    RESP_OK(cb, detail);
}

void OaCtrl::createKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    DatabaseService::instance().execParams(
        R"(INSERT INTO oa_knowledge_article(title,category,tags,summary,content,attachments,author_id,author_name,status,view_count,scope,like_count,favorite_count,comment_count,create_by,create_time)
           VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,0,$10,0,0,0,$11,NOW()))",
        {(*body).get("title", "").asString(), (*body).get("category", "general").asString(),
         (*body).get("tags", Json::Value(Json::arrayValue)).toStyledString(), (*body).get("summary", "").asString(),
         (*body).get("content", "").asString(), (*body).get("attachments", Json::Value(Json::arrayValue)).toStyledString(),
         std::to_string(GET_USER_ID(req)), GET_USER_NAME(req), (*body).get("status", "published").asString(),
         (*body).get("scope", "public").asString(), GET_USER_NAME(req)});
    LOG_OPER(req, "知识库", BusinessType::INSERT);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::updateKnowledge(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    DatabaseService::instance().execParams(
        R"(UPDATE oa_knowledge_article SET title=$1,category=$2,tags=$3,summary=$4,content=$5,attachments=$6,status=$7,scope=$8,update_by=$9,update_time=NOW() WHERE id=$10)",
        {(*body).get("title", "").asString(), (*body).get("category", "general").asString(),
         (*body).get("tags", Json::Value(Json::arrayValue)).toStyledString(), (*body).get("summary", "").asString(),
         (*body).get("content", "").asString(), (*body).get("attachments", Json::Value(Json::arrayValue)).toStyledString(),
         (*body).get("status", "published").asString(), (*body).get("scope", "public").asString(),
         GET_USER_NAME(req), std::to_string((*body).get("id", 0).asInt64())});
    LOG_OPER(req, "知识库", BusinessType::UPDATE);
    RESP_MSG(cb, "操作成功");
}

void OaCtrl::listKnowledgeComments(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    auto res = DatabaseService::instance().queryParams(
        "SELECT id,article_id,user_id,user_name,content,create_time FROM oa_knowledge_comment WHERE article_id=$1 ORDER BY id DESC",
        {std::to_string(id)});
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToKnowledgeComment(res, i));
    RESP_OK(cb, rows);
}

void OaCtrl::createKnowledgeComment(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto content = (*body).get("content", "").asString();
    if (content.empty()) { RESP_ERR(cb, "评论内容不能为空"); return; }
    auto& db = DatabaseService::instance();
    db.execParams(
        "INSERT INTO oa_knowledge_comment(article_id,user_id,user_name,content,create_time) VALUES($1,$2,$3,$4,NOW())",
        {std::to_string(id), std::to_string(GET_USER_ID(req)), GET_USER_NAME(req), content});
    db.execParams("UPDATE oa_knowledge_article SET comment_count=comment_count+1 WHERE id=$1", {std::to_string(id)});
    LOG_OPER(req, "知识库评论", BusinessType::INSERT);
    RESP_MSG(cb, "评论成功");
}

void OaCtrl::toggleKnowledgeLike(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    (void)req;
    auto& db = DatabaseService::instance();
    auto existed = db.queryParams("SELECT id FROM oa_knowledge_like WHERE article_id=$1 AND user_id=$2", {std::to_string(id), std::to_string(GET_USER_ID(req))});
    if (existed.ok() && existed.rows() > 0) {
        db.execParams("DELETE FROM oa_knowledge_like WHERE article_id=$1 AND user_id=$2", {std::to_string(id), std::to_string(GET_USER_ID(req))});
        db.execParams("UPDATE oa_knowledge_article SET like_count=GREATEST(like_count-1,0) WHERE id=$1", {std::to_string(id)});
        RESP_MSG(cb, "已取消点赞");
        return;
    }
    db.execParams("INSERT INTO oa_knowledge_like(article_id,user_id,create_time) VALUES($1,$2,NOW())", {std::to_string(id), std::to_string(GET_USER_ID(req))});
    db.execParams("UPDATE oa_knowledge_article SET like_count=like_count+1 WHERE id=$1", {std::to_string(id)});
    RESP_MSG(cb, "点赞成功");
}

void OaCtrl::toggleKnowledgeFavorite(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    (void)req;
    auto& db = DatabaseService::instance();
    auto existed = db.queryParams("SELECT id FROM oa_knowledge_favorite WHERE article_id=$1 AND user_id=$2", {std::to_string(id), std::to_string(GET_USER_ID(req))});
    if (existed.ok() && existed.rows() > 0) {
        db.execParams("DELETE FROM oa_knowledge_favorite WHERE article_id=$1 AND user_id=$2", {std::to_string(id), std::to_string(GET_USER_ID(req))});
        db.execParams("UPDATE oa_knowledge_article SET favorite_count=GREATEST(favorite_count-1,0) WHERE id=$1", {std::to_string(id)});
        RESP_MSG(cb, "已取消收藏");
        return;
    }
    db.execParams("INSERT INTO oa_knowledge_favorite(article_id,user_id,create_time) VALUES($1,$2,NOW())", {std::to_string(id), std::to_string(GET_USER_ID(req))});
    db.execParams("UPDATE oa_knowledge_article SET favorite_count=favorite_count+1 WHERE id=$1", {std::to_string(id)});
    RESP_MSG(cb, "收藏成功");
}

void OaCtrl::listVehicles(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto plate = req->getParameter("plateNumber");
    std::string sql = "SELECT id,plate_number,model,driver_name,driver_phone,status,remark FROM oa_vehicle WHERE 1=1";
    std::vector<std::string> params;
    if (!plate.empty()) {
        sql += " AND plate_number LIKE $1 ESCAPE '\\'";
        params.push_back("%" + escapeLikeParam(plate) + "%");
    }
    auto& db = DatabaseService::instance();
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToVehicle(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::createVehicleUsage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    auto vehicleId = (*body).get("vehicleId", 0).asInt64();
    auto startTime = (*body).get("startTime", "").asString();
    auto endTime = (*body).get("endTime", "").asString();
    auto conflict = DatabaseService::instance().queryParams(
        R"(SELECT id FROM oa_vehicle_usage WHERE vehicle_id=$1 AND status IN ('pending','approved') AND start_time < $3 AND end_time > $2 LIMIT 1)",
        {std::to_string(vehicleId), startTime, endTime});
    if (conflict.ok() && conflict.rows() > 0) { RESP_ERR(cb, "该车辆在所选时段已被预约，请选择其他时段"); return; }
    DatabaseService::instance().execParams(
        R"(INSERT INTO oa_vehicle_usage(vehicle_id,applicant_id,applicant_name,driver_name,use_date,start_time,end_time,destination,purpose,status,create_by,create_time)
           VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,'pending',$10,NOW()))",
        {std::to_string(vehicleId), std::to_string(GET_USER_ID(req)), GET_USER_NAME(req), (*body).get("driverName", "").asString(),
         (*body).get("useDate", "").asString(), startTime, endTime, (*body).get("destination", "").asString(),
         (*body).get("purpose", "").asString(), GET_USER_NAME(req)});
    LOG_OPER(req, "用车申请", BusinessType::INSERT);
    RESP_MSG(cb, "申请已提交");
}

void OaCtrl::listBorrowAssets(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    auto& db = DatabaseService::instance();
    std::string sql = "SELECT id,asset_name,asset_code,applicant_id,applicant_name,borrow_date,expect_return_date,actual_return_date,purpose,status,create_time FROM oa_asset_borrow WHERE 1=1";
    std::vector<std::string> params; int idx = 1;
    auto status = req->getParameter("status");
    auto name = req->getParameter("assetName");
    if (!status.empty()) { sql += " AND status=$" + std::to_string(idx++); params.push_back(status); }
    if (!name.empty()) { sql += " AND asset_name LIKE $" + std::to_string(idx++) + " ESCAPE '\\'"; params.push_back("%" + escapeLikeParam(name) + "%"); }
    auto countSql = "SELECT COUNT(*) FROM (" + sql + ") t";
    auto cntRes = params.empty() ? db.query(countSql) : db.queryParams(countSql, params);
    long total = (cntRes.ok() && cntRes.rows() > 0) ? cntRes.longVal(0, 0) : 0;
    sql += " ORDER BY id DESC LIMIT " + std::to_string(page.pageSize) + " OFFSET " + std::to_string(page.offset());
    auto res = params.empty() ? db.query(sql) : db.queryParams(sql, params);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToBorrowAsset(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::createBorrowAsset(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { RESP_ERR(cb, "请求体格式错误"); return; }
    DatabaseService::instance().execParams(
        R"(INSERT INTO oa_asset_borrow(asset_name,asset_code,applicant_id,applicant_name,borrow_date,expect_return_date,purpose,status,create_by,create_time)
           VALUES($1,$2,$3,$4,$5,$6,$7,'borrowed',$8,NOW()))",
        {(*body).get("assetName", "").asString(), (*body).get("assetCode", "").asString(), std::to_string(GET_USER_ID(req)),
         GET_USER_NAME(req), (*body).get("borrowDate", "").asString(), (*body).get("expectReturnDate", "").asString(),
         (*body).get("purpose", "").asString(), GET_USER_NAME(req)});
    LOG_OPER(req, "资产借用", BusinessType::INSERT);
    RESP_MSG(cb, "申请已提交");
}

void OaCtrl::returnBorrowAsset(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb, long long id) {
    (void)req;
    DatabaseService::instance().execParams(
        "UPDATE oa_asset_borrow SET status='returned',actual_return_date=CURRENT_DATE,update_by=$1,update_time=NOW() WHERE id=$2",
        {GET_USER_NAME(req), std::to_string(id)});
    LOG_OPER(req, "资产归还", BusinessType::UPDATE);
    RESP_MSG(cb, "归还成功");
}

void OaCtrl::myTodo(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto page = PageParam::fromRequest(req);
    long userId = GET_USER_ID(req);
    auto& db = DatabaseService::instance();
    std::string sql = R"(
        SELECT id,'message' AS biz_type,0 AS biz_id,title,content,'unread' AS status,'normal' AS priority,'' AS due_date,create_time
        FROM sys_message WHERE user_id=$1 AND is_read=0
        UNION ALL
        SELECT id,'approval' AS biz_type,id AS biz_id,title,form_data AS content,status,'high' AS priority,'' AS due_date,create_time
        FROM oa_approval WHERE reviewer_id=$1 AND status='pending'
        ORDER BY create_time DESC LIMIT $2 OFFSET $3)";
    auto res = db.queryParams(sql, {std::to_string(userId), std::to_string(page.pageSize), std::to_string(page.offset())});
    auto cntMsg = db.queryParams("SELECT COUNT(*) FROM sys_message WHERE user_id=$1 AND is_read=0", {std::to_string(userId)});
    auto cntAppr = db.queryParams("SELECT COUNT(*) FROM oa_approval WHERE reviewer_id=$1 AND status='pending'", {std::to_string(userId)});
    long total = (cntMsg.ok() ? cntMsg.longVal(0, 0) : 0) + (cntAppr.ok() ? cntAppr.longVal(0, 0) : 0);
    Json::Value rows(Json::arrayValue);
    if (res.ok()) for (int i = 0; i < res.rows(); ++i) rows.append(rowToTodo(res, i));
    PageResult pr; pr.total = total; pr.rows = rows; RESP_JSON(cb, pr.toJson());
}

void OaCtrl::dashboard(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    (void)req;
    auto& db = DatabaseService::instance();
    Json::Value data;
    auto approvals = db.query("SELECT status, COUNT(*) FROM oa_approval GROUP BY status");
    Json::Value approvalStats(Json::arrayValue);
    if (approvals.ok()) for (int i = 0; i < approvals.rows(); ++i) {
        Json::Value item;
        item["status"] = approvals.str(i, 0);
        item["count"] = Json::Int64(approvals.longVal(i, 1));
        approvalStats.append(item);
    }
    auto meetings = db.query("SELECT COUNT(*) FROM oa_meeting_room_booking WHERE status='scheduled'");
    auto meetingRooms = db.query("SELECT COUNT(*) FROM oa_meeting_room WHERE status='enabled'");
    auto knowledge = db.query("SELECT COUNT(*) FROM oa_knowledge_article WHERE status='published'");
    auto vehicles = db.query("SELECT COUNT(*) FROM oa_vehicle WHERE status='available'");
    auto borrowAssets = db.query("SELECT COUNT(*) FROM oa_asset_borrow WHERE status='borrowed'");
    auto overdueAssets = db.query("SELECT COUNT(*) FROM oa_asset_borrow WHERE status='borrowed' AND expect_return_date < CURRENT_DATE");
    data["approvalStats"] = approvalStats;
    data["scheduledMeetingCount"] = meetings.ok() && meetings.rows() > 0 ? Json::Int64(meetings.longVal(0, 0)) : Json::Int64(0);
    data["enabledMeetingRoomCount"] = meetingRooms.ok() && meetingRooms.rows() > 0 ? Json::Int64(meetingRooms.longVal(0, 0)) : Json::Int64(0);
    data["publishedKnowledgeCount"] = knowledge.ok() && knowledge.rows() > 0 ? Json::Int64(knowledge.longVal(0, 0)) : Json::Int64(0);
    data["availableVehicleCount"] = vehicles.ok() && vehicles.rows() > 0 ? Json::Int64(vehicles.longVal(0, 0)) : Json::Int64(0);
    data["borrowedAssetCount"] = borrowAssets.ok() && borrowAssets.rows() > 0 ? Json::Int64(borrowAssets.longVal(0, 0)) : Json::Int64(0);
    data["overdueAssetCount"] = overdueAssets.ok() && overdueAssets.rows() > 0 ? Json::Int64(overdueAssets.longVal(0, 0)) : Json::Int64(0);
    RESP_OK(cb, data);
}
