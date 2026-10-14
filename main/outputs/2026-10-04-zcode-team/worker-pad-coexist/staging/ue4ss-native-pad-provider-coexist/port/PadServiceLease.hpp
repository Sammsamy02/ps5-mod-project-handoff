#pragma once
#include <cstdint>
#include <mutex>
namespace RC::Input::Detail {
// One native process-wide lease, shared by providers during Handler replacement.
// An externally initialized service is borrowed and never terminated by us.
class PadServiceLease {
    std::mutex m_mutex;
    unsigned m_references{};
    bool m_owned{};
    int (*m_initialize)(void*);
    int (*m_terminate)();
public:
    PadServiceLease(int (*initialize)(void*),int (*terminate)()) : m_initialize(initialize),m_terminate(terminate) {}
    int acquire(void* arg) {
        std::lock_guard lock(m_mutex);
        if (!m_references) {
            int err=m_initialize(arg);
            if (err && static_cast<uint32_t>(err)!=0x80960003u) return err;
            m_owned=err==0;
        }
        ++m_references;
        return 0; // The lease was actually acquired, including borrowed service ownership.
    }
    int release() {
        std::lock_guard lock(m_mutex);
        if (!m_references) return -1; // Internal ownership violation, not an SDK error.
        if (m_references==1 && m_owned) {
            int err=m_terminate();
            if (err) return err; // Retain the last reference for explicit retry.
        }
        --m_references;
        if (!m_references) m_owned=false;
        return 0;
    }
};
}
