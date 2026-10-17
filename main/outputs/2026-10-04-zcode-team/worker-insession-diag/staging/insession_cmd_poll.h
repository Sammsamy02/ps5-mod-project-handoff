/* insession_cmd_poll.h — the bounded in-session command consumer entry point.
   worker-insession-diag, 2026-10-05.

   Call-site law: EXACTLY ONE call site, inside the UE4SS event loop body
   (UE4SSProgram::update, wpd1 staging lineage, PROSPERO branch, after the
   init gate :1661-1665 and pause gate :1667-1671, before the 5ms sleep
   :1754). Never call from any other thread: the poll touches no lock and
   relies on loop-thread serialization, the same law the event batch
   (:1677-1707, under m_event_queue_mutex) and mod fire_update (:1734-1744)
   already obey. */

#ifndef INSESSION_CMD_POLL_H
#define INSESSION_CMD_POLL_H

/* noexcept on the C++ declaration and definition alike; C consumers (host
   fixtures include this header as C) get the plain declaration. The
   definition carries noexcept unconditionally. */
#ifdef __cplusplus
#define INSESSION_NOEXCEPT noexcept
#else
#define INSESSION_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Never throws (the PROSPERO loop try/catch terminates the process on any
   escape, wpd1:1759-1771); every failure tolerated, never fatal. Idle ticks
   cost one integer increment; polled ticks cost one failed open. */
void insession_cmd_poll(void) INSESSION_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* INSESSION_CMD_POLL_H */
