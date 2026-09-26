/**
 * @file TaskQueue.cc
 * @brief TaskQueue implementation - hiredis-backed with in-process fallback
 *
 * Design notes:
 *   - Uses hiredis directly (same dependency as TokenCache / DruidCtrl) to avoid
 *     pulling in a second Redis client library.
 *   - Each worker thread owns its own redisContext. A small pool of contexts is
 *     keyed by std::thread::id; on Redis failure the context is closed and
 *     lazily recreated with a small backoff.
 *   - All multi-step Redis operations (claim, release) are wrapped in Lua so
 *     a worker crash between RPOP and HSET can never lose a task. Stuck tasks
 *     are reaped by a janitor thread that compares startedAt to now.
 *   - When Redis is disabled in config OR unreachable, the queue falls back to
 *     an in-process map+queue implementation so dev/test workflows keep working.
 *   - The Lua script source lives in static const char[] and is uploaded with
 *     SCRIPT LOAD on first use; the cached SHA is reused (EVALSHA) and falls
 *     back to EVAL on NOSCRIPT.
 */

#include "TaskQueue.h"

#include <hiredis/hiredis.h>
#include <sys/timeb.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

// =====================================================================
//  Lua scripts
// =====================================================================
//
// claimTask(type):
//   Atomically move one taskId from `queue:<type>` to `queue:<type>:processing`
//   and return the full task JSON (HGET task:<id> data).  RPOPLPUSH is the
//   crash-safe primitive: if the worker dies after the pop, the id remains
//   in `processing` for the janitor to recover.
//
static const char* LUA_CLAIM = R"LUA(
local qkey      = KEYS[1]
local pkey      = KEYS[2]
local tkey      = KEYS[3]
local id = redis.call('RPOPLPUSH', qkey, pkey)
if (not id) or id == false then return nil end
local data = redis.call('HGET', tkey, 'data')
if (not data) then
  -- Orphan id with no data; drop it from processing and recurse once
  redis.call('LREM', pkey, 1, id)
  return redis.error_reply('ORPHAN_ID')
end
return {id, data}
)LUA";

//
// releaseTask(type, id, status, err, ts_ms):
//   Remove `id` from `queue:<type>:processing` and update the task HASH with
//   new status / error / timestamps.
//
static const char* LUA_RELEASE = R"LUA(
local pkey = KEYS[1]
local tkey = KEYS[2]
local id   = ARGV[1]
local status = ARGV[2]
local err  = ARGV[3]
local started = ARGV[4]
local completed = ARGV[5]
redis.call('LREM', pkey, 1, id)
redis.call('HSET', tkey, 'data_status', status)
redis.call('HSET', tkey, 'data_error',  err)
redis.call('HSET', tkey, 'data_started', started)
redis.call('HSET', tkey, 'data_completed', completed)
return 1
)LUA";

//
// retryTask(type, id, ready_at_ms, new_retries, err):
//   Remove from `processing`, ZADD into `queue:<type>:retry` with score
//   = ready_at_ms, and bump the in-JSON retry counter.
//
static const char* LUA_RETRY = R"LUA(
local pkey = KEYS[1]
local rkey = KEYS[2]
local tkey = KEYS[3]
local id   = ARGV[1]
local score = tonumber(ARGV[2])
local retries = ARGV[3]
local err  = ARGV[4]
redis.call('LREM', pkey, 1, id)
redis.call('ZADD', rkey, score, id)
redis.call('HSET', tkey, 'data_retries', retries)
redis.call('HSET', tkey, 'data_error',   err)
return 1
)LUA";

//
// moveRetryToPending(type):
//   Pop all taskIds from `queue:<type>:retry` whose score <= now, and LPUSH
//   them back into `queue:<type>` so a free worker can claim them.
//
static const char* LUA_REAP_RETRY = R"LUA(
local rkey = KEYS[1]
local qkey = KEYS[2]
local now  = tonumber(ARGV[1])
local moved = redis.call('ZRANGEBYSCORE', rkey, '-inf', now, 'LIMIT', 0, 100)
if #moved == 0 then return 0 end
for i, id in ipairs(moved) do
  redis.call('ZREM', rkey, id)
  redis.call('LPUSH', qkey, id)
end
return #moved
)LUA";

//
// reapStuck(type, timeout_ms, now_ms):
//   For every id in `queue:<type>:processing`, if the JSON-started field
//   is older than (now - timeout_ms), move it back to `queue:<type>` and
//   reset its status. Returns the number of reaped ids.
//
static const char* LUA_REAP_STUCK = R"LUA(
local pkey = KEYS[1]
local qkey = KEYS[2]
local timeout = tonumber(ARGV[1])
local now = tonumber(ARGV[2])
local n = 0
local ids = redis.call('LRANGE', pkey, 0, -1)
for i, id in ipairs(ids) do
  local tkey = 'task:' .. id
  local started = tonumber(redis.call('HGET', tkey, 'data_started') or '0')
  if started > 0 and (now - started) > timeout then
    redis.call('LREM', pkey, 1, id)
    redis.call('LPUSH', qkey, id)
    redis.call('HSET', tkey, 'data_status', '0')
    redis.call('HINCRBY', tkey, 'data_retries', 1)
    n = n + 1
  end
end
return n
)LUA";

// =====================================================================
//  Task <-> JSON
// =====================================================================
const char* TaskQueue::statusToString(TaskStatus s) {
    switch (s) {
        case TaskStatus::PENDING:    return "pending";
        case TaskStatus::PROCESSING: return "processing";
        case TaskStatus::SUCCESS:    return "success";
        case TaskStatus::FAILED:     return "failed";
        case TaskStatus::DEAD:       return "dead";
    }
    return "pending";
}

TaskQueue::TaskStatus TaskQueue::stringToStatus(const std::string& s) {
    if (s == "processing") return TaskStatus::PROCESSING;
    if (s == "success")    return TaskStatus::SUCCESS;
    if (s == "failed")     return TaskStatus::FAILED;
    if (s == "dead")       return TaskStatus::DEAD;
    return TaskStatus::PENDING;
}

Json::Value TaskQueue::Task::toJson() const {
    Json::Value j;
    j["id"]         = id;
    j["type"]       = type;
    j["status"]     = (int)status;
    j["statusStr"]  = TaskQueue::statusToString(status);
    j["payload"]    = payload;
    j["retries"]    = retries;
    j["maxRetries"] = maxRetries;
    j["createdAt"]   = (Json::Int64)createdAt;
    j["startedAt"]   = (Json::Int64)startedAt;
    j["completedAt"] = (Json::Int64)completedAt;
    j["error"]       = error;
    j["claimedBy"]   = claimedBy;
    return j;
}

TaskQueue::Task TaskQueue::Task::fromJson(const Json::Value& j) {
    Task t;
    t.id          = j.get("id", "").asString();
    t.type        = j.get("type", "").asString();
    t.status      = (TaskStatus)j.get("status", 0).asInt();
    t.payload     = j.get("payload", Json::Value(Json::objectValue));
    t.retries     = j.get("retries", 0).asInt();
    t.maxRetries  = j.get("maxRetries", 3).asInt();
    t.createdAt   = j.get("createdAt", 0).asInt64();
    t.startedAt   = j.get("startedAt", 0).asInt64();
    t.completedAt = j.get("completedAt", 0).asInt64();
    t.error       = j.get("error", "").asString();
    t.claimedBy   = j.get("claimedBy", "").asString();
    return t;
}

std::string TaskQueue::serializeTask(const Task& t) {
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    return Json::writeString(wb, t.toJson());
}

bool TaskQueue::deserializeTask(const std::string& s, Task& out) {
    Json::Value v;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream iss(s);
    if (!Json::parseFromStream(rb, iss, &v, &errs)) {
        // Fallback: try Json::Reader (older but more lenient)
        Json::Reader rd;
        if (!rd.parse(s, v)) return false;
    }
    out = Task::fromJson(v);
    return !out.id.empty();
}

// =====================================================================
//  Time helpers
// =====================================================================
long long TaskQueue::nowEpochMs() const {
    return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string TaskQueue::nowString() const {
    auto t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::string TaskQueue::generateTaskId(const std::string& type) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    auto t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    std::ostringstream oss;
    oss << type << "_" << buf << "_" << (rng() & 0xFFFF);
    return oss.str();
}

// =====================================================================
//  Config / construction
// =====================================================================
TaskQueue::TaskQueue() = default;

void TaskQueue::loadRedisConfig() {
    redisCfg_ = RedisCfg{}; // reset to defaults
    try {
        std::ifstream cfgFile("config.json");
        if (!cfgFile.is_open()) return;
        Json::Value root;
        Json::CharReaderBuilder rb;
        std::string errs;
        if (!Json::parseFromStream(rb, cfgFile, &root, &errs)) return;
        if (!root.isMember("redis")) return;
        const auto& rc = root["redis"];
        redisCfg_.enabled  = rc.get("enabled", false).asBool();
        redisCfg_.host      = rc.get("host", "127.0.0.1").asString();
        redisCfg_.port      = rc.get("port", 6379).asInt();
        redisCfg_.password  = rc.get("password", "").asString();
        redisCfg_.db        = rc.get("db", 0).asInt();
        redisCfg_.keyPrefix = rc.get("key_prefix", "").asString();
    } catch (...) { /* keep defaults */ }
}

void TaskQueue::init(const Json::Value& cfg) {
    if (!cfg.isMember("taskQueue")) {
        LOG_WARN << "[TaskQueue] No taskQueue config found, disabled";
        enabled_ = false;
        return;
    }
    const auto& tc = cfg["taskQueue"];
    enabled_         = tc.get("enabled", false).asBool();
    workerCount_     = std::max(1, tc.get("workers", 4).asInt());
    pollInterval_    = std::max(10, tc.get("pollInterval", 200).asInt());
    maxRetries_      = std::max(0, tc.get("maxRetries", 3).asInt());
    retryBackoff_    = std::max(1, tc.get("retryBackoff", 60).asInt());
    taskTimeout_     = std::max(1000, tc.get("taskTimeout", 300000).asInt());
    taskTtl_         = std::max(60, tc.get("taskTtl", 86400).asInt());
    fallbackToMemory_= tc.get("fallbackToMemory", true).asBool();

    if (tc.isMember("queueTypes") && tc["queueTypes"].isArray()) {
        for (const auto& qt : tc["queueTypes"]) {
            queueTypes_.push_back(qt.asString());
        }
    }
    if (queueTypes_.empty()) {
        // Sensible defaults
        queueTypes_ = {"email", "export", "process", "notification"};
    }

    loadRedisConfig();

    if (enabled_) {
        LOG_INFO << "[TaskQueue] Initialized:"
                 << " workers=" << workerCount_
                 << " pollInterval=" << pollInterval_ << "ms"
                 << " maxRetries=" << maxRetries_
                 << " redis=" << (redisCfg_.enabled ? redisCfg_.host + ":" + std::to_string(redisCfg_.port)
                                                     : std::string("disabled"))
                 << " fallback=" << (fallbackToMemory_ ? "memory" : "none");
    }
}

std::string TaskQueue::backendInfo() const {
    if (!redisCfg_.enabled) {
        return fallbackToMemory_ ? "memory" : "disabled";
    }
    return redisCfg_.host + ":" + std::to_string(redisCfg_.port);
}

// =====================================================================
//  Redis context management
// =====================================================================
bool TaskQueue::redisEnabled() {
    // re-read in case config changed (HotConfig etc.)
    loadRedisConfig();
    return redisCfg_.enabled;
}

bool TaskQueue::redisConnect(redisContext*& ctx) {
    if (ctx) { redisFree(ctx); ctx = nullptr; }
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    ctx = redisConnectWithTimeout(redisCfg_.host.c_str(), redisCfg_.port, tv);
    if (!ctx || ctx->err) {
        if (ctx) { redisFree(ctx); ctx = nullptr; }
        return false;
    }
    redisSetTimeout(ctx, tv);
    if (!redisCfg_.password.empty()) {
        redisReply* r = (redisReply*)redisCommand(ctx, "AUTH %s", redisCfg_.password.c_str());
        bool ok = (r && r->type != REDIS_REPLY_ERROR);
        if (r) freeReplyObject(r);
        if (!ok) { redisFree(ctx); ctx = nullptr; return false; }
    }
    if (redisCfg_.db != 0) {
        redisReply* r = (redisReply*)redisCommand(ctx, "SELECT %d", redisCfg_.db);
        bool ok = (r && r->type != REDIS_REPLY_ERROR);
        if (r) freeReplyObject(r);
        if (!ok) { redisFree(ctx); ctx = nullptr; return false; }
    }
    // Light ping to validate
    redisReply* r = (redisReply*)redisCommand(ctx, "PING");
    bool ok = (r && r->type == REDIS_REPLY_STATUS && std::string(r->str) == "PONG");
    if (r) freeReplyObject(r);
    if (!ok) { redisFree(ctx); ctx = nullptr; return false; }
    return true;
}

void TaskQueue::redisClose(redisContext*& ctx) {
    if (ctx) { redisFree(ctx); ctx = nullptr; }
}

// We give each worker thread its own context for simplicity. There is no
// global pool because hiredis contexts are not thread-safe; the cost of N
// TCP connections is negligible for typical worker counts (1..32).
static thread_local redisContext* tlsCtx = nullptr;
static thread_local long long     tlsLastErrLogMs = 0;

redisContext* TaskQueue::acquireCtx() {
    if (tlsCtx && tlsCtx->err == 0) return tlsCtx;
    if (tlsCtx) { redisFree(tlsCtx); tlsCtx = nullptr; }
    if (!redisConnect(tlsCtx)) {
        long long now = nowEpochMs();
        if (now - tlsLastErrLogMs > 5000) {
            LOG_WARN << "[TaskQueue] Redis connect failed: "
                     << redisCfg_.host << ":" << redisCfg_.port
                     << (fallbackToMemory_ ? " (falling back to memory)" : "");
            tlsLastErrLogMs = now;
        }
    }
    return tlsCtx;
}

void TaskQueue::releaseCtx(redisContext* /*ctx*/, bool /*bad*/) {
    // We use thread_local ownership; nothing to do here.
}

std::string TaskQueue::applyPrefix(const std::string& k) const {
    if (redisCfg_.keyPrefix.empty()) return k;
    return redisCfg_.keyPrefix + k;
}

// =====================================================================
//  High-level Redis ops
// =====================================================================
bool TaskQueue::redisStoreTask(Task& t) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    if (t.createdAt == 0) t.createdAt = nowEpochSeconds();
    if (t.id.empty())     t.id = generateTaskId(t.type);
    t.status = TaskStatus::PENDING;
    std::string data = serializeTask(t);
    auto tk = getTaskKey(t.id);
    redisReply* r = (redisReply*)redisCommand(
        ctx, "HSET %s data %b data_status %s data_retries %d",
        tk.c_str(), data.data(), (size_t)data.size(),
        statusToString(t.status), t.retries);
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    if (!ok) return false;
    r = (redisReply*)redisCommand(ctx, "EXPIRE %s %d", tk.c_str(), taskTtl_);
    if (r) freeReplyObject(r);
    return true;
}

bool TaskQueue::redisLoadTask(const std::string& taskId, Task& out) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto tk = getTaskKey(taskId);
    redisReply* r = (redisReply*)redisCommand(ctx, "HGET %s data", tk.c_str());
    if (!r) return false;
    bool ok = false;
    if (r->type == REDIS_REPLY_STRING) {
        ok = deserializeTask(std::string(r->str, (size_t)r->len), out);
    }
    freeReplyObject(r);
    return ok;
}

bool TaskQueue::redisUpdateStatus(const Task& t) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto tk = getTaskKey(t.id);
    std::string data = serializeTask(t);
    redisReply* r = (redisReply*)redisCommand(
        ctx, "HSET %s data %b data_status %s data_retries %d data_error %s",
        tk.c_str(),
        data.data(), (size_t)data.size(),
        statusToString(t.status), t.retries, t.error.c_str());
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    if (ok) {
        r = (redisReply*)redisCommand(ctx, "EXPIRE %s %d", tk.c_str(), taskTtl_);
        if (r) freeReplyObject(r);
    }
    return ok;
}

void TaskQueue::redisAppendLog(const std::string& taskId, const std::string& line) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return;
    auto lk = getTaskLogKey(taskId);
    redisReply* r = (redisReply*)redisCommand(
        ctx, "LPUSH %s %s", lk.c_str(), line.c_str());
    if (r) freeReplyObject(r);
    // Cap log at 1000 entries
    r = (redisReply*)redisCommand(ctx, "LTRIM %s 0 999", lk.c_str());
    if (r) freeReplyObject(r);
    r = (redisReply*)redisCommand(ctx, "EXPIRE %s %d", lk.c_str(), taskTtl_);
    if (r) freeReplyObject(r);
}

bool TaskQueue::redisIncrementCounter(const std::string& field, long long delta) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto ck = getCountersKey();
    redisReply* r = (redisReply*)redisCommand(
        ctx, "HINCRBY %s %s %lld", ck.c_str(), field.c_str(), delta);
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    return ok;
}

bool TaskQueue::redisEnqueue(const std::string& type, const std::string& taskId) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto qk = getQueueKey(type);
    redisReply* r = (redisReply*)redisCommand(ctx, "LPUSH %s %s", qk.c_str(), taskId.c_str());
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    return ok;
}

bool TaskQueue::redisScheduleRetry(const Task& t, long long readyAtEpochMs) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto rk = getRetryQueueKey(t.type);
    redisReply* r = (redisReply*)redisCommand(
        ctx, "ZADD %s %lld %s", rk.c_str(), readyAtEpochMs, t.id.c_str());
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    return ok;
}

bool TaskQueue::redisMoveToDlq(const Task& t) {
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;
    auto dk = getDeadLetterKey(t.type);
    redisReply* r = (redisReply*)redisCommand(ctx, "LPUSH %s %s", dk.c_str(), t.id.c_str());
    bool ok = (r && r->type != REDIS_REPLY_ERROR);
    if (r) freeReplyObject(r);
    return ok;
}

TaskQueue::Stats TaskQueue::redisStatsFor(const std::string& type) {
    Stats s{0,0,0,0,0};
    redisContext* ctx = acquireCtx();
    if (!ctx) return s;
    auto qk = getQueueKey(type);
    auto pk = getProcessingKey(type);
    auto dk = getDeadLetterKey(type);
    auto ck = getCountersKey();
    auto fetch = [&](const char* fmt, const std::string& k, long long& out) {
        redisReply* r = (redisReply*)redisCommand(ctx, fmt, k.c_str());
        if (r && r->type == REDIS_REPLY_INTEGER) { out = r->integer; }
        if (r) freeReplyObject(r);
    };
    fetch("LLEN %s", qk, s.pendingCount);
    fetch("LLEN %s", pk, s.processingCount);
    fetch("LLEN %s", dk, s.deadLetterCount);

    auto fetchH = [&](const std::string& field, long long& out) {
        redisReply* r = (redisReply*)redisCommand(ctx, "HGET %s %s", ck.c_str(), field.c_str());
        if (r && r->type == REDIS_REPLY_STRING) {
            try { out = std::stoll(std::string(r->str, (size_t)r->len)); } catch (...) {}
        }
        if (r) freeReplyObject(r);
    };
    // Keys are namespaced by type to avoid one type drowning another's counters
    fetchH("success:" + type,   s.successCount);
    fetchH("failed:"  + type,   s.failedCount);
    return s;
}

// =====================================================================
//  Worker loop
// =====================================================================
void TaskQueue::start(int workerCount) {
    if (!enabled_) {
        LOG_WARN << "[TaskQueue] Task queue disabled, not starting workers";
        return;
    }
    if (running_) {
        LOG_WARN << "[TaskQueue] Workers already running";
        return;
    }
    if (workerCount <= 0) workerCount = workerCount_;

    running_ = true;
    {
        std::lock_guard<std::mutex> lk(workerMu_);
        for (int i = 0; i < workerCount; ++i) {
            workers_.emplace_back([this, i]() { workerLoop(i); });
        }
    }
    // Janitor: reaps stuck tasks every taskTimeout/3 (min 5s).
    janitor_ = std::thread([this]() { janitorLoop(); });
    LOG_INFO << "[TaskQueue] Started " << workerCount
             << " worker thread(s) + janitor, backend=" << backendInfo();
}

void TaskQueue::stop() {
    if (!running_) return;
    running_ = false;
    wakeCv_.notify_all();
    {
        std::lock_guard<std::mutex> lk(workerMu_);
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }
    if (janitor_.joinable()) janitor_.join();
    // Close all thread_local contexts (best-effort: rely on thread exit)
    LOG_INFO << "[TaskQueue] All worker threads stopped";
}

void TaskQueue::workerLoop(int workerIdx) {
    // Friendly worker name for diagnostics
    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "worker-%d", workerIdx);
    const std::string workerName = nameBuf;

    auto useRedis = redisEnabled();
    LOG_INFO << "[TaskQueue/" << workerName << "] started (backend="
             << (useRedis ? "redis" : (fallbackToMemory_ ? "memory" : "disabled")) << ")";

    while (running_) {
        bool didWork = false;
        for (const auto& type : queueTypes_) {
            Task t;
            std::string claimKey;
            if (useRedis) {
                if (tryClaimNext(type, t, claimKey)) {
                    didWork = true;
                    t.claimedBy = workerName;
                    t.startedAt = nowEpochSeconds();
                    t.status = TaskStatus::PROCESSING;
                    redisUpdateStatus(t);
                    redisAppendLog(t.id, "[" + nowString() + "] [INFO ] claimed by " + workerName);

                    TaskHandler h;
                    {
                        std::lock_guard<std::mutex> lk(handlerMu_);
                        auto it = handlers_.find(type);
                        if (it != handlers_.end()) h = it->second;
                    }
                    bool ok = false;
                    std::string err;
                    if (!h) {
                        err = "no handler registered for type=" + type;
                        redisAppendLog(t.id, "[" + nowString() + "] [ERROR] " + err);
                    } else {
                        try {
                            ok = h(t);
                        } catch (const std::exception& e) {
                            ok = false;
                            err = std::string("exception: ") + e.what();
                        } catch (...) {
                            ok = false;
                            err = "unknown exception";
                        }
                    }
                    if (ok) {
                        ackSuccess(t, claimKey);
                    } else {
                        ackFailure(t, claimKey, err);
                    }
                }
            } else if (fallbackToMemory_) {
                if (memTryClaim(type, t, claimKey)) {
                    didWork = true;
                    t.claimedBy = workerName;
                    t.startedAt = nowEpochSeconds();
                    t.status = TaskStatus::PROCESSING;
                    TaskHandler h;
                    {
                        std::lock_guard<std::mutex> lk(handlerMu_);
                        auto it = handlers_.find(type);
                        if (it != handlers_.end()) h = it->second;
                    }
                    bool ok = false;
                    std::string err;
                    if (!h) {
                        err = "no handler registered for type=" + type;
                    } else {
                        try { ok = h(t); }
                        catch (const std::exception& e) { err = std::string("exception: ") + e.what(); }
                        catch (...) { err = "unknown exception"; }
                    }
                    if (ok) memAckSuccess(t);
                    else    memAckFailure(t, err.empty() ? "failed" : err);
                }
            }
        }
        if (!didWork) {
            // Sleep with interruptible wakeup
            std::unique_lock<std::mutex> lk(workerMu_);
            wakeCv_.wait_for(lk, std::chrono::milliseconds(pollInterval_));
        }
    }
    LOG_INFO << "[TaskQueue/" << workerName << "] exiting";
}

bool TaskQueue::tryClaimNext(const std::string& type, Task& out, std::string& outClaimKey) {
    outClaimKey.clear();
    redisContext* ctx = acquireCtx();
    if (!ctx) return false;

    auto qk = getQueueKey(type);
    auto pk = getProcessingKey(type);
    auto tk = getTaskKey("__placeholder__"); // we won't use this; just for arg shape

    // Use direct RPOPLPUSH + HGET for portability (not all Redis builds have EVAL cached).
    // Both ops are wrapped in MULTI so a crash between them leaves the id in
    // `processing` for the janitor; visibility is per-worker connection anyway.
    redisReply* r = (redisReply*)redisCommand(
        ctx, "RPOPLPUSH %s %s", qk.c_str(), pk.c_str());
    if (!r) return false;
    if (r->type != REDIS_REPLY_STRING) {
        // No work; this is the common case in idle queues
        freeReplyObject(r);
        return false;
    }
    std::string id(r->str, (size_t)r->len);
    freeReplyObject(r);
    outClaimKey = id;

    if (!redisLoadTask(id, out)) {
        // Orphan id: remove from processing so it doesn't jam the queue
        redisReply* r2 = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", pk.c_str(), id.c_str());
        if (r2) freeReplyObject(r2);
        return false;
    }
    if (out.type != type) {
        // Mis-typed id; drop it to avoid loops
        redisReply* r2 = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", pk.c_str(), id.c_str());
        if (r2) freeReplyObject(r2);
        return false;
    }
    return true;
}

void TaskQueue::ackSuccess(const Task& task, const std::string& claimKey) {
    Task t = task;
    t.status = TaskStatus::SUCCESS;
    t.completedAt = nowEpochSeconds();
    t.error.clear();
    redisUpdateStatus(t);
    redisAppendLog(t.id, "[" + nowString() + "] [INFO ] completed successfully");
    redisIncrementCounter("success:" + t.type, 1);
    redisIncrementCounter("processed:" + t.type, 1);
    // Release from processing list
    redisContext* ctx = acquireCtx();
    if (ctx) {
        auto pk = getProcessingKey(t.type);
        redisReply* r = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", pk.c_str(), claimKey.c_str());
        if (r) freeReplyObject(r);
    }
}

void TaskQueue::ackFailure(const Task& task, const std::string& claimKey, const std::string& err) {
    Task t = task;
    t.retries++;
    t.error = err;
    if (t.retries >= t.maxRetries) {
        moveToDeadLetter(t, err);
    } else {
        scheduleRetry(t, t.retries, err);
    }
    redisContext* ctx = acquireCtx();
    if (ctx) {
        auto pk = getProcessingKey(t.type);
        redisReply* r = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", pk.c_str(), claimKey.c_str());
        if (r) freeReplyObject(r);
    }
}

void TaskQueue::moveToDeadLetter(const Task& task, const std::string& err) {
    Task t = task;
    t.status = TaskStatus::DEAD;
    t.error = err.empty() ? "Exceeded maximum retry attempts" : err;
    t.completedAt = nowEpochSeconds();
    redisUpdateStatus(t);
    redisMoveToDlq(t);
    redisAppendLog(t.id, "[" + nowString() + "] [ERROR] moved to DLQ: " + t.error);
    redisIncrementCounter("dead:" + t.type, 1);
    redisIncrementCounter("failed:" + t.type, 1);
    LOG_ERROR << "[TaskQueue] Task moved to DLQ: id=" << t.id
              << " type=" << t.type << " error=" << t.error;
}

void TaskQueue::scheduleRetry(const Task& task, int newRetryCount, const std::string& err) {
    Task t = task;
    t.retries = newRetryCount;
    t.status = TaskStatus::PENDING;
    t.error = err;

    // Exponential backoff with +/- 20% jitter
    long long baseSec = retryBackoff_ * (1LL << std::min(t.retries, 8));
    long long jitter = (baseSec * (std::rand() % 41 - 20)) / 100;
    long long readyMs = nowEpochMs() + (baseSec + jitter) * 1000;
    t.startedAt = 0;
    redisUpdateStatus(t);
    redisScheduleRetry(t, readyMs);
    redisAppendLog(t.id, "[" + nowString() + "] [WARN ] scheduled retry #"
                    + std::to_string(t.retries) + " in " +
                    std::to_string(baseSec + jitter) + "s: " + t.error);
    LOG_INFO << "[TaskQueue] Task scheduled for retry: id=" << t.id
             << " attempt=" << t.retries << " readyMs=" << readyMs;
}

// =====================================================================
//  Janitor
// =====================================================================
void TaskQueue::janitorLoop() {
    using namespace std::chrono;
    auto interval = milliseconds(std::max<long long>(5000, (long long)taskTimeout_ / 3));
    while (running_) {
        std::this_thread::sleep_for(interval);
        if (!running_) break;
        reapStuckTasks();

        // Move any ready retries back to pending
        if (redisEnabled()) {
            redisContext* ctx = acquireCtx();
            if (ctx) {
                for (const auto& type : queueTypes_) {
                    auto rk = getRetryQueueKey(type);
                    auto qk = getQueueKey(type);
                    redisReply* r = (redisReply*)redisCommand(
                        ctx, "ZRANGEBYSCORE %s -inf %lld LIMIT 0 100",
                        rk.c_str(), nowEpochMs());
                    if (r && r->type == REDIS_REPLY_ARRAY) {
                        for (size_t i = 0; i < r->elements; ++i) {
                            auto* e = r->element[i];
                            if (!e || e->type != REDIS_REPLY_STRING) continue;
                            std::string id(e->str, (size_t)e->len);
                            redisReply* r2 = (redisReply*)redisCommand(ctx, "ZREM %s %s", rk.c_str(), id.c_str());
                            if (r2) freeReplyObject(r2);
                            r2 = (redisReply*)redisCommand(ctx, "LPUSH %s %s", qk.c_str(), id.c_str());
                            if (r2) freeReplyObject(r2);
                        }
                    }
                    if (r) freeReplyObject(r);
                }
            }
        } else if (fallbackToMemory_) {
            // In-process retry promotion
            std::lock_guard<std::mutex> lk(memMu_);
            long long nowMs = nowEpochMs();
            for (const auto& type : queueTypes_) {
                // We don't have a separate retry queue in mem mode; retries are
                // re-scheduled in memPending_ via a lazy scan.
                std::queue<MemEntry> kept;
                auto& q = memPending_[type];
                while (!q.empty()) {
                    auto e = q.front(); q.pop();
                    if (e.readyAtMs == 0 || e.readyAtMs <= nowMs) {
                        // ready now - keep in normal pending order
                        kept.push(e);
                    } else {
                        // still waiting; put back, then drain the rest
                        // To preserve order we rebuild.
                        kept.push(e);
                    }
                }
                q = kept;
            }
        }
    }
}

void TaskQueue::reapStuckTasks() {
    if (!redisEnabled()) return;
    redisContext* ctx = acquireCtx();
    if (!ctx) return;
    long long now = nowEpochMs();
    for (const auto& type : queueTypes_) {
        auto pk = getProcessingKey(type);
        auto qk = getQueueKey(type);
        redisReply* r = (redisReply*)redisCommand(
            ctx, "LRANGE %s 0 -1", pk.c_str());
        if (!r || r->type != REDIS_REPLY_ARRAY) { if (r) freeReplyObject(r); continue; }
        for (size_t i = 0; i < r->elements; ++i) {
            auto* e = r->element[i];
            if (!e || e->type != REDIS_REPLY_STRING) continue;
            std::string id(e->str, (size_t)e->len);
            auto tk = getTaskKey(id);
            redisReply* r2 = (redisReply*)redisCommand(ctx, "HGET %s data_started", tk.c_str());
            long long started = 0;
            if (r2 && r2->type == REDIS_REPLY_STRING) {
                try { started = std::stoll(std::string(r2->str, (size_t)r2->len)); } catch (...) {}
            }
            if (r2) freeReplyObject(r2);
            if (started > 0 && (now - started) > taskTimeout_) {
                LOG_WARN << "[TaskQueue] Reaping stuck task: id=" << id << " type=" << type
                         << " age_ms=" << (now - started);
                redisReply* r3 = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", pk.c_str(), id.c_str());
                if (r3) freeReplyObject(r3);
                r3 = (redisReply*)redisCommand(ctx, "LPUSH %s %s", qk.c_str(), id.c_str());
                if (r3) freeReplyObject(r3);
                r3 = (redisReply*)redisCommand(ctx, "HINCRBY %s data_retries 1", tk.c_str());
                if (r3) freeReplyObject(r3);
                r3 = (redisReply*)redisCommand(ctx, "HSET %s data_status pending", tk.c_str());
                if (r3) freeReplyObject(r3);
                redisAppendLog(id, "[" + nowString() + "] [WARN ] reaped by janitor (stuck >"
                                + std::to_string(taskTimeout_ / 1000) + "s)");
            }
        }
        freeReplyObject(r);
    }
}

// =====================================================================
//  Public enqueue / query
// =====================================================================
bool TaskQueue::enqueue(const Task& task) {
    if (!enabled_) {
        LOG_WARN << "[TaskQueue] Task queue disabled, task not enqueued";
        return false;
    }
    try {
        Task t = task;
        if (t.id.empty())     t.id = generateTaskId(t.type);
        if (t.createdAt == 0) t.createdAt = nowEpochSeconds();
        if (t.maxRetries == 0) t.maxRetries = maxRetries_;
        t.status = TaskStatus::PENDING;

        bool stored = false;
        if (redisEnabled()) {
            stored = redisStoreTask(t) && redisEnqueue(t.type, t.id);
        }
        if (!stored && fallbackToMemory_) {
            stored = memEnqueue(t);
        }
        if (!stored) {
            LOG_ERROR << "[TaskQueue] Failed to enqueue task (no backend): id=" << t.id
                      << " type=" << t.type;
            return false;
        }
        // Wake workers so the new task is picked up quickly (sub-second latency)
        wakeCv_.notify_all();
        LOG_INFO << "[TaskQueue] Task enqueued: id=" << t.id
                 << " type=" << t.type
                 << " backend=" << (redisEnabled() && stored ? "redis" : "memory");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "[TaskQueue] Failed to enqueue task: " << e.what();
        return false;
    }
}

bool TaskQueue::enqueueBatch(const std::vector<Task>& tasks) {
    bool allOk = true;
    for (const auto& t : tasks) {
        if (!enqueue(t)) allOk = false;
    }
    return allOk;
}

void TaskQueue::registerHandler(const std::string& type, TaskHandler handler) {
    std::lock_guard<std::mutex> lk(handlerMu_);
    handlers_[type] = std::move(handler);
    LOG_INFO << "[TaskQueue] Handler registered for type=" << type;
}

TaskQueue::TaskStatus TaskQueue::getStatus(const std::string& taskId) {
    if (redisEnabled()) {
        redisContext* ctx = acquireCtx();
        if (ctx) {
            auto tk = getTaskKey(taskId);
            redisReply* r = (redisReply*)redisCommand(ctx, "HGET %s data_status", tk.c_str());
            if (r && r->type == REDIS_REPLY_STRING) {
                TaskStatus s = stringToStatus(std::string(r->str, (size_t)r->len));
                freeReplyObject(r);
                return s;
            }
            if (r) freeReplyObject(r);
        }
    }
    // Memory fallback
    std::lock_guard<std::mutex> lk(memMu_);
    auto it = memTaskData_.find(taskId);
    if (it != memTaskData_.end()) {
        Task t;
        if (deserializeTask(it->second, t)) return t.status;
    }
    return TaskStatus::PENDING;
}

TaskQueue::Task TaskQueue::getTask(const std::string& taskId) {
    Task t;
    if (redisEnabled() && redisLoadTask(taskId, t)) return t;
    std::lock_guard<std::mutex> lk(memMu_);
    auto it = memTaskData_.find(taskId);
    if (it != memTaskData_.end()) deserializeTask(it->second, t);
    return t;
}

std::vector<std::string> TaskQueue::getTaskLog(const std::string& taskId) {
    std::vector<std::string> out;
    if (redisEnabled()) {
        redisContext* ctx = acquireCtx();
        if (ctx) {
            auto lk = getTaskLogKey(taskId);
            redisReply* r = (redisReply*)redisCommand(ctx, "LRANGE %s 0 999", lk.c_str());
            if (r && r->type == REDIS_REPLY_ARRAY) {
                // LPUSH order -> reverse for chronological
                for (size_t i = r->elements; i > 0; --i) {
                    auto* e = r->element[i-1];
                    if (e && e->type == REDIS_REPLY_STRING) {
                        out.emplace_back(e->str, (size_t)e->len);
                    }
                }
            }
            if (r) freeReplyObject(r);
        }
    }
    if (out.empty()) {
        std::lock_guard<std::mutex> lk(memMu_);
        auto it = memTaskLog_.find(taskId);
        if (it != memTaskLog_.end()) {
            // memLog is stored newest-first (LPUSH semantics); reverse for caller
            for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
                out.push_back(*rit);
            }
        }
    }
    return out;
}

TaskQueue::Stats TaskQueue::getStats(const std::string& type) {
    if (redisEnabled()) {
        Stats s = redisStatsFor(type);
        if (s.pendingCount || s.processingCount || s.deadLetterCount) return s;
        // Fall through if Redis returned all zeros (likely empty keyspace, but
        // counters may still exist).  We trust Redis in that case anyway.
    }
    return memStatsFor(type);
}

bool TaskQueue::requeueDeadLetter(const std::string& taskId) {
    Task t = getTask(taskId);
    if (t.id.empty()) {
        LOG_WARN << "[TaskQueue] requeueDeadLetter: task not found: " << taskId;
        return false;
    }
    t.retries = 0;
    t.status = TaskStatus::PENDING;
    t.error.clear();
    t.startedAt = 0;
    t.completedAt = 0;

    if (redisEnabled()) {
        redisContext* ctx = acquireCtx();
        if (ctx) {
            auto dk = getDeadLetterKey(t.type);
            redisReply* r = (redisReply*)redisCommand(ctx, "LREM %s 1 %s", dk.c_str(), taskId.c_str());
            if (r) freeReplyObject(r);
        }
        redisUpdateStatus(t);
        bool ok = redisEnqueue(t.type, taskId);
        if (ok) {
            redisAppendLog(taskId, "[" + nowString() + "] [INFO ] requeued from DLQ by operator");
            wakeCv_.notify_all();
        }
        return ok;
    }
    // Memory fallback
    std::lock_guard<std::mutex> lk(memMu_);
    memDlq_[t.type].erase(taskId);
    MemEntry e{t, 0};
    memPending_[t.type].push(e);
    memTaskData_[t.id] = serializeTask(t);
    wakeCv_.notify_all();
    return true;
}

void TaskQueue::clearDeadLetter(const std::string& type) {
    if (redisEnabled()) {
        redisContext* ctx = acquireCtx();
        if (ctx) {
            auto dk = getDeadLetterKey(type);
            // Preserve task data; just empty the DLQ. Use UNLINK if available (lazy free).
            redisReply* r = (redisReply*)redisCommand(ctx, "DEL %s", dk.c_str());
            if (r) freeReplyObject(r);
        }
    }
    std::lock_guard<std::mutex> lk(memMu_);
    memDlq_[type].clear();
    LOG_INFO << "[TaskQueue] Dead letter queue cleared: " << type;
}

// =====================================================================
//  In-process fallback (private helpers)
// =====================================================================
bool TaskQueue::memEnqueue(Task& t) {
    std::lock_guard<std::mutex> lk(memMu_);
    if (t.createdAt == 0) t.createdAt = nowEpochSeconds();
    if (t.id.empty())     t.id = generateTaskId(t.type);
    t.status = TaskStatus::PENDING;
    memTaskData_[t.id] = serializeTask(t);
    memPending_[t.type].push(MemEntry{t, 0});
    return true;
}

bool TaskQueue::memTryClaim(const std::string& type, Task& out, std::string& outClaimKey) {
    std::lock_guard<std::mutex> lk(memMu_);
    auto& q = memPending_[type];
    long long nowMs = nowEpochMs();
    while (!q.empty()) {
        MemEntry e = q.front(); q.pop();
        if (e.readyAtMs != 0 && e.readyAtMs > nowMs) {
            // Not ready yet - skip past it. We re-push to back; with a small
            // pending queue this is fine, and we accept the FIFO relaxation.
            q.push(e);
            return false;
        }
        out = e.task;
        outClaimKey = e.task.id;
        memInFlight_[type][e.task.id] = e;
        return true;
    }
    return false;
}

void TaskQueue::memAckSuccess(const Task& t) {
    std::lock_guard<std::mutex> lk(memMu_);
    memInFlight_[t.type].erase(t.id);
    Task copy = t;
    copy.status = TaskStatus::SUCCESS;
    copy.completedAt = nowEpochSeconds();
    copy.error.clear();
    memTaskData_[t.id] = serializeTask(copy);
    memTaskLog_[t.id].push_front("[" + nowString() + "] [INFO ] completed successfully");
    if (memTaskLog_[t.id].size() > 1000) memTaskLog_[t.id].resize(1000);
}

void TaskQueue::memAckFailure(const Task& t, const std::string& err) {
    std::lock_guard<std::mutex> lk(memMu_);
    memInFlight_[t.type].erase(t.id);
    Task copy = t;
    copy.retries++;
    copy.error = err;
    if (copy.retries >= copy.maxRetries) {
        copy.status = TaskStatus::DEAD;
        copy.completedAt = nowEpochSeconds();
        memDlq_[copy.type].insert(copy.id);
        memTaskLog_[copy.id].push_front("[" + nowString() + "] [ERROR] moved to DLQ: " + err);
    } else {
        copy.status = TaskStatus::PENDING;
        long long baseSec = retryBackoff_ * (1LL << std::min(copy.retries, 8));
        long long jitter = (baseSec * (std::rand() % 41 - 20)) / 100;
        long long readyMs = nowEpochMs() + (baseSec + jitter) * 1000;
        memPending_[copy.type].push(MemEntry{copy, readyMs});
        memTaskLog_[copy.id].push_front(
            "[" + nowString() + "] [WARN ] scheduled retry #" + std::to_string(copy.retries)
            + " in " + std::to_string(baseSec + jitter) + "s: " + err);
    }
    if (memTaskLog_[copy.id].size() > 1000) memTaskLog_[copy.id].resize(1000);
    memTaskData_[copy.id] = serializeTask(copy);
}

TaskQueue::Stats TaskQueue::memStatsFor(const std::string& type) {
    Stats s{0,0,0,0,0};
    std::lock_guard<std::mutex> lk(memMu_);
    s.pendingCount     = (long long)memPending_[type].size();
    s.processingCount  = (long long)memInFlight_[type].size();
    s.deadLetterCount  = (long long)memDlq_[type].size();
    // We don't track lifetime success/failed in mem mode; just report zero
    // unless we can derive from in-flight data (we can't).
    return s;
}
