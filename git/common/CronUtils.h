#pragma once
#include <string>
#include <set>
#include <vector>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <algorithm>

// Quartz-style Cron 表达式工具（6字段: sec min hour dom month dow [year]）
// dow: 1=Sun, 2=Mon, ..., 7=Sat
class CronUtils {
public:
    struct Field {
        bool any = false;
        std::set<int> vals;
        bool check(int v) const { return any || vals.count(v) > 0; }
        // 返回 >= start 的第一个匹配值，超出范围返回 -1
        int next(int start, int max) const {
            if (any) return start;
            auto it = vals.lower_bound(start);
            return (it != vals.end() && *it <= max) ? *it : -1;
        }
        int first(int min, int max) const { return next(min, max); }
    };

    struct Expr {
        Field sec, min, hour, dom, mon, dow;

        static Expr parse(const std::string& s) {
            Expr e;
            std::vector<std::string> parts;
            std::istringstream iss(s);
            std::string p;
            while (iss >> p) parts.push_back(p);
            if (parts.size() < 6)
                throw std::runtime_error("Cron needs at least 6 fields");
            e.sec  = parseField(parts[0], 0, 59);
            e.min  = parseField(parts[1], 0, 59);
            e.hour = parseField(parts[2], 0, 23);
            e.dom  = parseField(parts[3], 1, 31);
            e.mon  = parseField(parts[4], 1, 12);
            e.dow  = parseField(parts[5], 1, 7);
            return e;
        }

        bool matches(const struct tm& t) const {
            if (!sec.check(t.tm_sec))     return false;
            if (!min.check(t.tm_min))     return false;
            if (!hour.check(t.tm_hour))   return false;
            if (!mon.check(t.tm_mon + 1)) return false;
            if (!dom.any && !dow.any)
                return dom.check(t.tm_mday) || dow.check(t.tm_wday + 1);
            if (!dom.any) return dom.check(t.tm_mday);
            if (!dow.any) return dow.check(t.tm_wday + 1);
            return true;
        }

        // 智能推进算法 - 比逐秒迭代快得多
        time_t nextFireTime(time_t from) const {
            time_t cur = from + 1;
            struct tm t{};
            tlocal(t, cur);

            time_t limit = from + (time_t)5 * 366 * 24 * 3600;

            while (cur < limit) {
                // ── month ──────────────────────────────────────────────────
                int m = t.tm_mon + 1;
                if (!mon.any && !mon.check(m)) {
                    int nm = mon.next(m + 1, 12);
                    if (nm < 0) { t.tm_year++; nm = mon.first(1, 12); }
                    if (nm < 0) return -1;
                    t.tm_mon = nm - 1; t.tm_mday = 1;
                    t.tm_hour = t.tm_min = t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                // ── day ────────────────────────────────────────────────────
                if (!dayOk(t)) {
                    t.tm_mday++; t.tm_hour = t.tm_min = t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                // ── hour ───────────────────────────────────────────────────
                int nh = hour.next(t.tm_hour, 23);
                if (nh < 0) {
                    t.tm_mday++; t.tm_hour = t.tm_min = t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                if (nh > t.tm_hour) {
                    t.tm_hour = nh; t.tm_min = t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                // ── minute ─────────────────────────────────────────────────
                int nm2 = min.next(t.tm_min, 59);
                if (nm2 < 0) {
                    t.tm_hour++; t.tm_min = t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                if (nm2 > t.tm_min) {
                    t.tm_min = nm2; t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                // ── second ─────────────────────────────────────────────────
                int ns = sec.next(t.tm_sec, 59);
                if (ns < 0) {
                    t.tm_min++; t.tm_sec = 0;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                if (ns > t.tm_sec) {
                    t.tm_sec = ns;
                    cur = mktime(&t); if (cur<0) return -1;
                    tlocal(t, cur); continue;
                }
                return cur; // all fields match
            }
            return -1;
        }

    private:
        bool dayOk(const struct tm& t) const {
            int d = t.tm_mday, w = t.tm_wday + 1;
            if (!dom.any && !dow.any) return dom.check(d) || dow.check(w);
            if (!dom.any) return dom.check(d);
            if (!dow.any) return dow.check(w);
            return true;
        }
        static void tlocal(struct tm& t, time_t tt) {
#ifdef _WIN32
            localtime_s(&t, &tt);
#else
            localtime_r(&tt, &t);
#endif
        }
        static Field parseField(const std::string& expr, int lo, int hi) {
            Field f;
            if (expr == "*" || expr == "?") { f.any = true; return f; }
            std::istringstream ss(expr);
            std::string part;
            while (std::getline(ss, part, ',')) {
                auto slash = part.find('/');
                auto dash  = part.find('-');
                if (slash != std::string::npos) {
                    std::string startStr = part.substr(0, slash);
                    int step  = std::stoi(part.substr(slash + 1));
                    int start = (startStr == "*") ? lo : std::stoi(startStr);
                    for (int v = start; v <= hi; v += step) f.vals.insert(v);
                } else if (dash != std::string::npos) {
                    int a = std::stoi(part.substr(0, dash));
                    int b = std::stoi(part.substr(dash + 1));
                    for (int v = a; v <= b; ++v) f.vals.insert(v);
                } else {
                    f.vals.insert(std::stoi(part));
                }
            }
            return f;
        }
    };

    // 当前时间是否匹配 cron 表达式
    static bool matches(const std::string& cronExpr, const struct tm& t) {
        try { return Expr::parse(cronExpr).matches(t); }
        catch (...) { return false; }
    }

    // 计算下次触发时间
    static time_t nextFireTime(const std::string& cronExpr, time_t from = 0) {
        try {
            if (!from) from = std::time(nullptr);
            return Expr::parse(cronExpr).nextFireTime(from);
        } catch (...) { return -1; }
    }

    // 格式化下次触发时间为字符串
    static std::string nextFireStr(const std::string& cronExpr) {
        time_t t = nextFireTime(cronExpr);
        if (t < 0) return "--";
        struct tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }
};
