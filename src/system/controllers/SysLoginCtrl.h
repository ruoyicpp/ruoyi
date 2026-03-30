#pragma once
#include <drogon/HttpController.h>
#include <drogon/utils/Utilities.h>
#include <sstream>
#include <random>
#include <unordered_map>
#include <algorithm>
#include "../../common/AjaxResult.h"
#include "../../common/TokenCache.h"
#include "../../common/SecurityUtils.h"
#include "../../filters/PermFilter.h"
#include "../../services/DatabaseService.h"
#include "../services/SysLoginService.h"
#include "../services/TokenService.h"
#include "../services/SysMenuService.h"
#include "../services/SysConfigService.h"
#include "../services/SysPasswordService.h"

// POST /login
// POST /logout
// GET  /getInfo
// GET  /getRouters
// POST /register
// GET  /captchaImage
class SysLoginCtrl : public drogon::HttpController<SysLoginCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysLoginCtrl::login,      "/login",      drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::logout,     "/logout",     drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::getInfo,    "/getInfo",    drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysLoginCtrl::getRouters, "/getRouters", drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysLoginCtrl::doRegister, "/register",   drogon::Post);
        ADD_METHOD_TO(SysLoginCtrl::captcha,    "/captchaImage", drogon::Get);
    METHOD_LIST_END

    // POST /login
    void login(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }

        std::string username = (*body).get("username", "").asString();
        std::string password = (*body).get("password", "").asString();
        std::string code     = (*body).get("code",     "").asString();
        std::string uuid     = (*body).get("uuid",     "").asString();

        try {
            auto token = SysLoginService::instance().login(username, password, code, uuid, req);
            auto result = AjaxResult::successMap();
            result["token"] = token;
            RESP_JSON(cb, result);
        } catch (const std::exception &e) {
            RESP_ERR(cb, e.what());
        }
    }

    // POST /logout
    void logout(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        try {
            auto userOpt = TokenService::instance().getLoginUser(req);
            if (userOpt) {
                TokenService::instance().delLoginUser(userOpt->token);
                DatabaseService::instance().execParams(
                    "INSERT INTO sys_logininfor(user_name,ipaddr,status,msg,login_time) "
                    "VALUES($1,$2,'0','�˳��ɹ�',NOW())",
                    {userOpt->userName, req->getPeerAddr().toIp()});
            }
        } catch (...) {}
        RESP_MSG(cb, "�˳��ɹ�");
    }

    // GET /getInfo
    void getInfo(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto user = GET_LOGIN_USER(req);
        long userId = user.userId;

        auto& db = DatabaseService::instance();
        // ��: user_id(0),dept_id(1),user_name(2),nick_name(3),email(4),phonenumber(5),
        //     sex(6),avatar(7),status(8),login_ip(9),login_date(10),create_time(11),
        //     remark(12),dept_id2(13),dept_name(14),leader(15)
        auto res = db.queryParams(
            "SELECT u.user_id,u.dept_id,u.user_name,u.nick_name,u.email,"
            "u.phonenumber,u.sex,u.avatar,u.status,u.login_ip,u.login_date,"
            "u.create_time,u.remark,"
            "d.dept_id,d.dept_name,COALESCE(d.leader,'') "
            "FROM sys_user u LEFT JOIN sys_dept d ON u.dept_id=d.dept_id "
            "WHERE u.user_id=$1", {std::to_string(userId)});

        if (!res.ok() || res.rows() == 0) { RESP_401(cb); return; }

        Json::Value userJson;
        userJson["userId"]      = (Json::Int64)res.longVal(0, 0);
        userJson["deptId"]      = (Json::Int64)res.longVal(0, 1);
        userJson["userName"]    = res.str(0, 2);
        userJson["nickName"]    = res.str(0, 3);
        userJson["email"]       = res.str(0, 4);
        userJson["phonenumber"] = res.str(0, 5);
        userJson["sex"]         = res.str(0, 6);
        userJson["avatar"]      = res.str(0, 7);
        userJson["status"]      = res.str(0, 8);
        userJson["loginIp"]     = res.str(0, 9);
        userJson["loginDate"]   = fmtTs(res.str(0, 10));
        userJson["createTime"]  = fmtTs(res.str(0, 11));
        userJson["remark"]      = res.str(0, 12);
        userJson["admin"]       = SecurityUtils::isAdmin(userId);

        // Ƕ�� dept ����ǰ�� profile ҳ���� user.dept.deptName��
        Json::Value dept;
        dept["deptId"]   = (Json::Int64)res.longVal(0, 13);
        dept["deptName"] = res.str(0, 14);
        dept["leader"]   = res.str(0, 15);
        userJson["dept"] = dept;

        // ��ɫ�б�Ƕ�׵� user �ǰ�� authRole ҳ���ã�
        auto roleRes = db.queryParams(
            "SELECT r.role_id,r.role_name,r.role_key,r.role_sort,r.status "
            "FROM sys_role r INNER JOIN sys_user_role ur ON r.role_id=ur.role_id "
            "WHERE ur.user_id=$1 AND r.del_flag='0'", {std::to_string(userId)});
        Json::Value userRoles(Json::arrayValue);
        if (roleRes.ok()) for (int i = 0; i < roleRes.rows(); ++i) {
            Json::Value r;
            r["roleId"]   = (Json::Int64)roleRes.longVal(i, 0);
            r["roleName"] = roleRes.str(i, 1);
            r["roleKey"]  = roleRes.str(i, 2);
            r["roleSort"] = roleRes.str(i, 3);
            r["status"]   = roleRes.str(i, 4);
            r["admin"]    = (roleRes.longVal(i, 0) == 1);
            userRoles.append(r);
        }
        userJson["roles"] = userRoles;

        // ���� roles����ɫȨ�ޱ�ʶ���ϣ��� permissions
        Json::Value roles(Json::arrayValue);
        for (auto &r : user.roles) roles.append(r);
        Json::Value perms(Json::arrayValue);
        for (auto &p : user.permissions) perms.append(p);

        auto result = AjaxResult::successMap();
        result["user"]        = userJson;
        result["roles"]       = roles;
        result["permissions"] = perms;
        RESP_JSON(cb, result);
    }

    // GET /getRouters
    void getRouters(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        long userId = GET_USER_ID(req);
        auto menus = SysMenuService::instance().buildMenusForUser(userId);
        RESP_OK(cb, menus);
    }

    // POST /register
    void doRegister(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        if (!SysConfigService::instance().isRegisterEnabled()) {
            RESP_ERR(cb, "��ǰϵͳû�п���ע�Ṧ�ܣ�");
            return;
        }
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "�������ʽ����"); return; }

        std::string username = (*body).get("username", "").asString();
        std::string password = (*body).get("password", "").asString();
        std::string code     = (*body).get("code",     "").asString();
        std::string uuid     = (*body).get("uuid",     "").asString();

        // 1. ��֤�루C# ˳������֤�룬������У�飩
        if (SysConfigService::instance().isCaptchaEnabled()) {
            auto cacheKey = Constants::CAPTCHA_CODE_KEY + uuid;
            auto cached = MemCache::instance().getString(cacheKey);
            MemCache::instance().remove(cacheKey);
            if (!cached || *cached != code) {
                RESP_ERR(cb, "��֤�����"); return;
            }
        }

        // 2. ����У��
        if (username.empty()) { RESP_ERR(cb, "�û�������Ϊ��"); return; }
        if (password.empty()) { RESP_ERR(cb, "�û����벻��Ϊ��"); return; }
        if (username.size() < 2 || username.size() > 20)
            { RESP_ERR(cb, "�˻����ȱ�����2��20���ַ�֮��"); return; }
        if (password.size() < 5 || password.size() > 20)
            { RESP_ERR(cb, "���볤�ȱ�����5��20���ַ�֮��"); return; }

        // 3. �û���Ψһ�ԣ���Ӧ C# CheckUserNameUniqueAsync��
        auto& db = DatabaseService::instance();
        auto exist = db.queryParams(
            "SELECT user_id FROM sys_user WHERE user_name=$1 LIMIT 1", {username});
        if (exist.ok() && exist.rows() > 0)
            { RESP_ERR(cb, "�����û�'" + username + "'ʧ�ܣ�ע���˺��Ѵ���"); return; }

        // 4. ע�ᣨ��Ӧ C# RegisterUserAsync��
        std::string encPwd = SecurityUtils::encryptPassword(password);
        bool ok = db.execParams(
            "INSERT INTO sys_user(user_name,nick_name,password,status,del_flag,create_time) "
            "VALUES($1,$1,$2,'0','0',NOW())",
            {username, encPwd});
        if (!ok) { RESP_ERR(cb, "ע��ʧ�ܣ�����ϵϵͳ������Ա"); return; }

        // 5. ע��ɹ�д��¼��־����Ӧ C# SysLogininforService.AddAsync��
        std::string ip = req->getPeerAddr().toIp();
        db.execParams(
            "INSERT INTO sys_logininfor(user_name,ipaddr,login_location,browser,os,status,msg,login_time) "
            "VALUES($1,$2,'','','','0','ע��ɹ�',NOW())",
            {username, ip});
        RESP_MSG(cb, "ע��ɹ�");
    }

    // GET /captchaImage �� 3����֤��������������ּ��� / ��ĸ / ��������
    void captcha(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto uuid = drogon::utils::getUuid();
        int mode = randInt(0, 2); // 0:���ּ��� 1:��ĸ 2:��������

        std::string answer;
        std::vector<RenderCmd> cmds;
        switch (mode) {
            case 0: makeMathCaptcha(cmds, answer);    break;
            case 1: makeLetterCaptcha(cmds, answer);  break;
            case 2: makeChineseCaptcha(cmds, answer);  break;
        }

        MemCache::instance().setString(Constants::CAPTCHA_CODE_KEY + uuid, answer, 120);

        std::string gifData = generateCaptchaGif(cmds);
        std::string base64Img = drogon::utils::base64Encode(
            (const unsigned char*)gifData.data(), gifData.size());

        Json::Value result = AjaxResult::successMap();
        result["uuid"]           = uuid;
        result["img"]            = base64Img;
        result["captchaEnabled"] = SysConfigService::instance().isCaptchaEnabled();
        RESP_JSON(cb, result);
    }

private:
    // ===================== ������� =====================
    static std::mt19937& rng() {
        static std::mt19937 gen(std::random_device{}());
        return gen;
    }
    static int randInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(rng());
    }

    // ===================== ��Ⱦָ�� =====================
    struct RenderCmd {
        enum Type { ASCII, CN_DIGIT } type;
        char ch;      // ASCII �ַ�
        int  digit;   // �������� 1-9
    };
    static RenderCmd asciiCmd(char c)  { return {RenderCmd::ASCII,    c, 0}; }
    static RenderCmd cnCmd(int d)      { return {RenderCmd::CN_DIGIT, 0, d}; }

    // ===================== ģʽ1: ���ּ��� =====================
    static void makeMathCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        int a = randInt(1, 10), b = randInt(1, 10);
        int op = randInt(0, 2);
        std::string expr;
        switch (op) {
            case 0: expr = std::to_string(a) + "+" + std::to_string(b) + "=?";
                    answer = std::to_string(a + b); break;
            case 1: if (a < b) std::swap(a, b);
                    expr = std::to_string(a) + "-" + std::to_string(b) + "=?";
                    answer = std::to_string(a - b); break;
            default: a = randInt(1,9); b = randInt(1,9);
                     expr = std::to_string(a) + "x" + std::to_string(b) + "=?";
                     answer = std::to_string(a * b); break;
        }
        for (char c : expr) cmds.push_back(asciiCmd(c));
    }

    // ===================== ģʽ2: ��ĸ =====================
    static void makeLetterCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        static const char pool[] = "ABCDEFGHJKMNPRSTUWXYZ"; // ȥ���׻����� I/L/O/Q/V
        answer.clear();
        for (int i = 0; i < 4; i++) {
            char c = pool[randInt(0, (int)sizeof(pool) - 2)];
            cmds.push_back(asciiCmd(c));
            answer += (char)std::tolower(c);
        }
    }

    // ===================== ģʽ3: �������ּ��� =====================
    static void makeChineseCaptcha(std::vector<RenderCmd>& cmds, std::string& answer) {
        int a = randInt(1, 9), b = randInt(1, 9);
        int op = randInt(0, 1); // 0:�� 1:��
        if (op == 1 && a < b) std::swap(a, b);
        cmds.push_back(cnCmd(a));
        cmds.push_back(asciiCmd(op == 0 ? '+' : '-'));
        cmds.push_back(cnCmd(b));
        cmds.push_back(asciiCmd('='));
        cmds.push_back(asciiCmd('?'));
        answer = std::to_string(op == 0 ? a + b : a - b);
    }

    // ===================== 5x7 ASCII ���� =====================
    static const std::vector<uint8_t>& getGlyph(char c) {
        static const std::unordered_map<char, std::vector<uint8_t>> font = {
            {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
            {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
            {'2', {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}},
            {'3', {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}},
            {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
            {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
            {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
            {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
            {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
            {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
            {'+', {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}},
            {'-', {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
            {'x', {0x00,0x11,0x0A,0x04,0x0A,0x11,0x00}},
            {'=', {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}},
            {'?', {0x0E,0x11,0x01,0x06,0x04,0x00,0x04}},
            {'A', {0x04,0x0A,0x11,0x1F,0x11,0x11,0x11}},
            {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
            {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
            {'D', {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
            {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
            {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
            {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}},
            {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
            {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
            {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
            {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
            {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
            {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
            {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
            {'S', {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}},
            {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
            {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
            {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
            {'X', {0x11,0x0A,0x04,0x04,0x0A,0x11,0x11}},
            {'Y', {0x11,0x0A,0x04,0x04,0x04,0x04,0x04}},
            {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
        };
        static const std::vector<uint8_t> blank = {0,0,0,0,0,0,0};
        auto it = font.find(c);
        return it != font.end() ? it->second : blank;
    }

    // ===================== �������ֱʻ���12x12 ����ϵ��=====================
    struct Stroke { int x0, y0, x1, y1; };
    using StrokeVec = std::vector<Stroke>;

    static const StrokeVec& getCnStrokes(int d) {
        // һ~�� �ļ�ʻ�����
        static const std::vector<StrokeVec> defs = {
            {},                                                        // 0 (unused)
            {{1,5, 10,5}},                                             // һ
            {{2,3, 9,3}, {1,8, 10,8}},                                 // ��
            {{2,1, 9,1}, {3,5, 8,5}, {1,10, 10,10}},                   // ��
            {{1,0,10,0},{1,0,1,10},{10,0,10,10},{1,10,10,10},           // ��
             {4,0,4,7},{7,0,7,7},{4,7,7,7}},
            {{1,0,10,0},{5,0,5,5},{2,5,9,5},{2,5,1,10},{1,10,10,10}},  // ��
            {{5,0,6,1},{1,3,10,3},{4,4,1,10},{7,4,10,10}},             // ��
            {{1,4,10,4},{6,0,6,10},{6,10,10,10}},                      // ��
            {{5,1,1,10},{6,1,10,10}},                                  // ��
            {{2,2,10,2},{4,2,4,6},{4,6,1,10},{4,6,10,9}},              // ��
        };
        if (d >= 1 && d <= 9) return defs[(size_t)d];
        static const StrokeVec empty;
        return empty;
    }

    // ===================== GIF ���� =====================
    static std::string generateCaptchaGif(const std::vector<RenderCmd>& cmds) {
        const int W = 160, H = 60;
        struct Color { uint8_t r, g, b; };
        std::vector<Color> palette(256);
        palette[0] = {240, 240, 240};
        palette[1] = {0, 0, 0};
        for (int i = 2; i < 8; ++i)
            palette[i] = {(uint8_t)randInt(40,220), (uint8_t)randInt(40,220), (uint8_t)randInt(40,220)};
        for (int i = 8; i < 16; ++i)
            palette[i] = {(uint8_t)randInt(20,140), (uint8_t)randInt(20,140), (uint8_t)randInt(20,140)};
        for (int i = 16; i < 256; ++i)
            palette[i] = {240, 240, 240};

        std::vector<uint8_t> pixels(W * H, 0);

        // Bresenham ����
        auto drawLine = [&](int x0, int y0, int x1, int y1, uint8_t color) {
            int dx = abs(x1-x0), dy = abs(y1-y0);
            int sx = x0<x1?1:-1, sy = y0<y1?1:-1;
            int err = dx-dy;
            while (true) {
                if (x0>=0 && x0<W && y0>=0 && y0<H) pixels[y0*W+x0] = color;
                if (x0==x1 && y0==y1) break;
                int e2 = 2*err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx)  { err += dx; y0 += sy; }
            }
        };

        // ������
        for (int i = 0; i < 8; ++i)
            drawLine(randInt(0,W-1), randInt(0,H-1), randInt(0,W-1), randInt(0,H-1), 2+(i%6));
        // ���ŵ�
        for (int i = 0; i < 80; ++i) {
            int px = randInt(0,W-1), py = randInt(0,H-1);
            pixels[py*W+px] = 2 + randInt(0,5);
        }

        // �����ܿ��
        const int asciiCharW = 17; // 5*3+2
        const int cnCharW    = 26; // 12*2+2
        int totalW = 0;
        for (auto& cmd : cmds)
            totalW += (cmd.type == RenderCmd::ASCII ? asciiCharW : cnCharW);
        int curX = (W - totalW) / 2;

        // ��Ⱦÿ���ַ�
        for (size_t ci = 0; ci < cmds.size(); ++ci) {
            uint8_t color = 8 + (ci % 8);
            int jitter = randInt(-3, 3);

            if (cmds[ci].type == RenderCmd::ASCII) {
                // 5x7 λͼ���壬�Ŵ�3��
                int scale = 3;
                auto& glyph = getGlyph(cmds[ci].ch);
                int oy = (H - 7*scale) / 2 + jitter;
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (glyph[row] & (0x10 >> col))
                            for (int sy = 0; sy < scale; ++sy)
                                for (int sx = 0; sx < scale; ++sx) {
                                    int px = curX + col*scale + sx;
                                    int py = oy + row*scale + sy;
                                    if (px>=0 && px<W && py>=0 && py<H)
                                        pixels[py*W+px] = color;
                                }
                curX += asciiCharW;
            } else {
                // �������֣��ʻ�ʸ����scale=2���Ӵ�
                int scale = 2;
                auto& strokes = getCnStrokes(cmds[ci].digit);
                int oy = (H - 12*scale) / 2 + jitter;
                for (auto& s : strokes) {
                    for (int t = 0; t < scale; ++t) {
                        drawLine(curX+s.x0*scale, oy+s.y0*scale+t,
                                 curX+s.x1*scale, oy+s.y1*scale+t, color);
                        drawLine(curX+s.x0*scale+t, oy+s.y0*scale,
                                 curX+s.x1*scale+t, oy+s.y1*scale, color);
                    }
                }
                curX += cnCharW;
            }
        }

        // ========== GIF89a ���� ==========
        std::string gif;
        auto w8  = [&](uint8_t  v) { gif += (char)v; };
        auto w16 = [&](uint16_t v) { gif += (char)(v&0xFF); gif += (char)((v>>8)&0xFF); };

        gif += "GIF89a";
        w16(W); w16(H);
        w8(0xF7); w8(0); w8(0);
        for (int i = 0; i < 256; ++i) { w8(palette[i].r); w8(palette[i].g); w8(palette[i].b); }
        w8(0x2C); w16(0); w16(0); w16(W); w16(H); w8(0);

        uint8_t minCodeSize = 8;
        w8(minCodeSize);
        int clearCode = 256, eoiCode = 257, codeSize = 9;

        std::vector<uint8_t> lzwBytes;
        uint32_t bitBuf = 0;
        int bitCnt = 0;
        auto emitCode = [&](int code) {
            bitBuf |= ((uint32_t)code << bitCnt);
            bitCnt += codeSize;
            while (bitCnt >= 8) { lzwBytes.push_back(bitBuf & 0xFF); bitBuf >>= 8; bitCnt -= 8; }
        };

        emitCode(clearCode);
        int sinceReset = 0;
        for (size_t i = 0; i < pixels.size(); ++i) {
            emitCode(pixels[i]);
            if (++sinceReset >= 250) { emitCode(clearCode); sinceReset = 0; }
        }
        emitCode(eoiCode);
        if (bitCnt > 0) lzwBytes.push_back(bitBuf & 0xFF);

        size_t pos = 0;
        while (pos < lzwBytes.size()) {
            size_t chunk = std::min<size_t>(255, lzwBytes.size() - pos);
            w8((uint8_t)chunk);
            for (size_t j = 0; j < chunk; ++j) w8(lzwBytes[pos + j]);
            pos += chunk;
        }
        w8(0);
        w8(0x3B);
        return gif;
    }
};
