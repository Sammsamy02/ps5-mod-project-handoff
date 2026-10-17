#ifndef EXP_TLS_ATTEMPT_H
#define EXP_TLS_ATTEMPT_H
#include <stddef.h>
#include <stdint.h>

#define TLS_ATTEMPT_MAX_ADDRESS 15u
#define TLS_ATTEMPT_MAX_HOSTNAME 253u
#define TLS_ATTEMPT_MAX_ROOT_DER 4096u

struct tls_attempt_input {
    const char *numeric_address;
    size_t numeric_address_length;
    unsigned port;
    const char *expected_hostname;
    size_t expected_hostname_length;
    const unsigned char *root_der;
    size_t root_der_length;
};

/* Entirely owning metadata. No contexts, pointers, entropy/RNG output, body,
   keys, session state, or wire bytes can escape through this result. */
struct tls_attempt_result {
    char phase[24], reason[48], clock_failure[32];
    char source_failure[32], last_api_status[16], last_api_reason[32];
    unsigned passed, body_accepted, clean_peer_close;
    unsigned certificate_verified, framed_response_complete, local_close_notify_confirmed;
    unsigned verify_available;
    uint32_t verify_flags;
    int add_source_rc, seed_rc, setup_rc, handshake_rc, close_notify_rc;
    int root_parse_rc, last_tls_rc;
    unsigned source_callback_count, source_os_call_count, shared_api_count;
    unsigned source_closed, source_failed, has_last_api;
    int last_api_return_code, last_api_errno;
    size_t last_api_returned_length;
    int64_t last_api_elapsed_ms;
    unsigned rng_calls, rng_requested_bytes, tls_entries, bio_entries, poll_calls;
    unsigned wire_sent_bytes, wire_received_bytes, http_request_bytes, response_bytes;
    unsigned clock_attempts, monotonic_calls, wall_calls, library_time_reads;
    unsigned utc_calls, certificate_utc_calls;
    int clock_errno;
    unsigned wall_value_observed, utc_value_observed;
    int64_t initial_wall_epoch, last_wall_epoch;
    int utc_year, utc_month, utc_day, utc_hour, utc_minute, utc_second;
    unsigned descriptor_close_attempts, descriptor_close_failed, cleanup_completed;
    int descriptor_close_errno;
    int64_t elapsed_ms;
};

/* Synchronous, single-owner operation; the caller serializes all calls and
   other use of the configuration's global time/UTC hooks. Inputs/result must
   be valid distinct live objects. Validated inputs are copied before use.
   IPv4 only; no DNS, retries, retained RNG, general HTTP/download API, or hard
   deadline around a synchronous OS/library call. Returns 0 only on accepted
   fixed fixture body plus clean TLS/descriptor cleanup; otherwise -1. */
int tls_attempt(const struct tls_attempt_input *input,
                 struct tls_attempt_result *result);
#endif
