#include "guest_thread_manager.h"
#include "win32_thread_identity.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdlib>

namespace cap13 {
namespace {
std::atomic<uint64_t> next_handle{1};
uint64_t reserve_handle() noexcept
{
    uint64_t value = next_handle.load(std::memory_order_relaxed);
    while (value != UINT64_MAX)
    {
        if (next_handle.compare_exchange_weak(value, value + 1,
                                              std::memory_order_relaxed))
            return value;
    }
    return 0;
}
}

struct GuestThreadManager::Impl
{
    struct Record
    {
        uint32_t id = 0;
        bool handle_open = true; // protected by Impl::mutex
        void* start = nullptr;
        void* argument = nullptr;
        std::mutex mutex;
        std::condition_variable cv;
        bool finished = false;
        GuestThreadRunResult run{};
        std::thread worker;
    };
    GuestPeBatchResult* batch = nullptr;
    StaticTlsBank& bank;
    Runner runner;
    std::mutex mutex;
    bool deferred = false;
    bool accepting = true;
    bool quiescing = false;
    bool quiesced = false;
    bool failed_worker = false;
    std::unordered_map<uint64_t, std::shared_ptr<Record>> handles;
    std::vector<std::shared_ptr<Record>> records;

    Impl(GuestPeBatchResult* b, StaticTlsBank& k, Runner r)
        : batch(b), bank(k), runner(r), deferred(!b) {}

    bool valid_start(void* start) const
    {
        if (!batch || !batch->ok || batch->modules.empty() || !start)
            return false;
        unsigned owners = 0;
        for (const auto& module : batch->modules)
        {
            if (!module || !module->mapped_handle()) return false;
            if (module->owns_executable_address(start)) ++owners;
        }
        return owners == 1;
    }

    void complete_without_worker(const std::shared_ptr<Record>& rec,
                                 GuestThreadRunStatus status)
    {
        {
            std::lock_guard<std::mutex> guard(rec->mutex);
            rec->run = {status, 0};
            rec->finished = true;
        }
        if (status != GuestThreadRunStatus::Ok) failed_worker = true;
        rec->cv.notify_all();
    }

    bool launch(const std::shared_ptr<Record>& rec)
    {
        try
        {
            Impl* impl = this;
            rec->worker = std::thread([impl, rec] {
                GuestThreadRunResult result{GuestThreadRunStatus::StartFailed, 0};
                try
                {
                    if (win32_bind_current_thread_id(rec->id))
                        result = impl->runner(*impl->batch, impl->bank,
                                              rec->start, rec->argument);
                }
                catch (...) { result = {GuestThreadRunStatus::StartFailed, 0}; }
                {
                    std::lock_guard<std::mutex> done_guard(rec->mutex);
                    rec->run = result;
                    rec->finished = true;
                }
                rec->cv.notify_all();
            });
            return true;
        }
        catch (...) { return false; }
    }

    void reap_closed_finished()
    {
        for (auto it = records.begin(); it != records.end();)
        {
            const auto& rec = *it;
            if (rec->handle_open) { ++it; continue; }
            bool finished;
            GuestThreadRunStatus status;
            {
                std::lock_guard<std::mutex> guard(rec->mutex);
                finished = rec->finished;
                status = rec->run.status;
            }
            if (!finished) { ++it; continue; }
            if (rec->worker.joinable()) rec->worker.join();
            if (status != GuestThreadRunStatus::Ok) failed_worker = true;
            it = records.erase(it);
        }
    }
};

GuestThreadManager::GuestThreadManager(GuestPeBatchResult& batch,
                                       StaticTlsBank& bank, Runner runner)
    : m_(new Impl(&batch, bank, runner)) {}

GuestThreadManager::GuestThreadManager(StaticTlsBank& bank, Runner runner)
    : m_(new Impl(nullptr, bank, runner)) {}

GuestThreadManager::~GuestThreadManager()
{
    // Keeping references to a destroyed batch would be unsafe. A caller that
    // destroys the manager on its own worker violates the lifetime contract.
    if (!quiesce_all()) std::terminate();
}

GuestThreadCreateResult GuestThreadManager::create(
    void* start, void* argument, void* attributes, uint64_t stack_size,
    uint32_t flags)
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    if (!m_->accepting) return {GuestThreadCreateStatus::WrongState, 0, 0};
    m_->reap_closed_finished();
    if (attributes || stack_size || flags)
        return {GuestThreadCreateStatus::UnsupportedOptions, 0, 0};
    if (!m_->runner || !start ||
        (!m_->deferred && !m_->valid_start(start)))
        return {GuestThreadCreateStatus::InvalidStart, 0, 0};
    const uint32_t id = win32_reserve_thread_id();
    const uint64_t handle = reserve_handle();
    if (!id || !handle)
        return {GuestThreadCreateStatus::Exhausted, 0, 0};
    std::shared_ptr<Impl::Record> rec;
    bool appended = false;
    try
    {
        rec = std::make_shared<Impl::Record>();
        rec->id = id;
        rec->start = start;
        rec->argument = argument;
        m_->records.push_back(rec);
        appended = true;
        m_->handles.emplace(handle, rec);
        if (!m_->deferred && !m_->launch(rec))
            throw std::bad_alloc();
    }
    catch (...)
    {
        m_->handles.erase(handle);
        if (appended) m_->records.pop_back();
        return {GuestThreadCreateStatus::StartFailed, 0, 0};
    }
    return {GuestThreadCreateStatus::Ok, handle, id};
}

bool GuestThreadManager::activate(GuestPeBatchResult& batch)
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    if (!m_->deferred || !m_->accepting || !batch.ok ||
        batch.modules.empty()) return false;
    m_->batch = &batch;
    m_->deferred = false;
    for (const auto& rec : m_->records)
    {
        if (rec->finished || rec->worker.joinable()) continue;
        if (!m_->valid_start(rec->start))
            m_->complete_without_worker(rec, GuestThreadRunStatus::InvalidStart);
        else if (!m_->launch(rec))
            m_->complete_without_worker(rec, GuestThreadRunStatus::StartFailed);
    }
    return true;
}

bool GuestThreadManager::abort_pending()
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    if (!m_->deferred || !m_->accepting) return false;
    m_->deferred = false;
    m_->accepting = false;
    for (const auto& rec : m_->records)
        if (!rec->finished)
            m_->complete_without_worker(rec, GuestThreadRunStatus::StartFailed);
    m_->reap_closed_finished();
    return true;
}

bool GuestThreadManager::awaiting_batch() const
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    return m_->deferred && m_->accepting;
}

GuestThreadWaitStatus GuestThreadManager::wait(uint64_t handle,
                                               uint32_t milliseconds)
{
    std::shared_ptr<Impl::Record> rec;
    {
        std::lock_guard<std::mutex> guard(m_->mutex);
        auto it = m_->handles.find(handle);
        if (it == m_->handles.end()) return GuestThreadWaitStatus::InvalidHandle;
        if (m_->deferred && milliseconds == UINT32_MAX)
            return GuestThreadWaitStatus::WouldDeadlock;
        rec = it->second;
    }
    if (rec->worker.get_id() == std::this_thread::get_id())
        return GuestThreadWaitStatus::WouldDeadlock;
    std::unique_lock<std::mutex> guard(rec->mutex);
    if (milliseconds == UINT32_MAX)
        rec->cv.wait(guard, [&] { return rec->finished; });
    else if (!rec->cv.wait_for(guard, std::chrono::milliseconds(milliseconds),
                               [&] { return rec->finished; }))
        return GuestThreadWaitStatus::Timeout;
    return GuestThreadWaitStatus::Object0;
}

GuestThreadQueryResult GuestThreadManager::query(uint64_t handle)
{
    std::shared_ptr<Impl::Record> rec;
    {
        std::lock_guard<std::mutex> guard(m_->mutex);
        auto it = m_->handles.find(handle);
        if (it == m_->handles.end()) return {};
        rec = it->second;
    }
    std::lock_guard<std::mutex> guard(rec->mutex);
    return {true, rec->finished, rec->run};
}

bool GuestThreadManager::close(uint64_t handle)
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    auto it = m_->handles.find(handle);
    if (it == m_->handles.end()) return false;
    it->second->handle_open = false;
    m_->handles.erase(it);
    if (!m_->quiescing) m_->reap_closed_finished();
    return true;
}

bool GuestThreadManager::quiesce_all()
{
    std::vector<std::shared_ptr<Impl::Record>> records;
    {
        std::lock_guard<std::mutex> guard(m_->mutex);
        if (m_->quiesced) return true;
        if (m_->quiescing) return false;
        if (m_->deferred)
        {
            m_->deferred = false;
            for (const auto& rec : m_->records)
                if (!rec->finished)
                    m_->complete_without_worker(rec,
                                                GuestThreadRunStatus::StartFailed);
        }
        for (const auto& rec : m_->records)
            if (rec->worker.joinable() &&
                rec->worker.get_id() == std::this_thread::get_id())
                return false;
        m_->accepting = false;
        m_->quiescing = true;
        records = m_->records;
    }
    for (const auto& rec : records)
        if (rec->worker.joinable()) rec->worker.join();
    {
        std::lock_guard<std::mutex> guard(m_->mutex);
        for (const auto& rec : records)
        {
            std::lock_guard<std::mutex> done_guard(rec->mutex);
            if (rec->run.status != GuestThreadRunStatus::Ok)
                m_->failed_worker = true;
        }
        m_->reap_closed_finished();
        m_->quiescing = false;
        m_->quiesced = true;
    }
    return true;
}

bool GuestThreadManager::had_worker_failure()
{
    std::lock_guard<std::mutex> guard(m_->mutex);
    return m_->failed_worker;
}

} // namespace cap13
