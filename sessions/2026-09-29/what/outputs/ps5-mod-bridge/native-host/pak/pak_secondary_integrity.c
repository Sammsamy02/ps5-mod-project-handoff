#include "pak_secondary_integrity.h"

#include <string.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha1.h>

static int storage_range_valid(const void *memory, size_t bytes)
{
    uintptr_t start = (uintptr_t)memory;
    return memory != NULL && bytes != 0 && bytes - 1 <= UINTPTR_MAX - start;
}

static int storage_overlaps(const void *left, size_t left_bytes,
                            const void *right, size_t right_bytes)
{
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    /* Both complete address ranges have already been checked for overflow. */
    return a <= b ? b - a < left_bytes : a - b < right_bytes;
}

static pak_status failure(pak_secondary_result_v1 *result,
                          pak_status status, pak_secondary_reason reason)
{
    result->status = status;
    result->reason = reason;
    return status;
}

static pak_status provider_failure(pak_secondary_result_v1 *result,
                                   pak_provider_status status)
{
    switch (status) {
        case PAK_PROVIDER_IO_ERROR:
            return failure(result, PAK_STATUS_PROVIDER_ERROR, PAK_SECONDARY_REASON_PROVIDER_IO);
        case PAK_PROVIDER_UNAVAILABLE:
            return failure(result, PAK_STATUS_PROVIDER_ERROR, PAK_SECONDARY_REASON_PROVIDER_UNAVAILABLE);
        case PAK_PROVIDER_CHANGED:
            return failure(result, PAK_STATUS_PROVIDER_CHANGED, PAK_SECONDARY_REASON_PROVIDER_CHANGED);
        case PAK_PROVIDER_CANCELLED:
            return failure(result, PAK_STATUS_CANCELLED, PAK_SECONDARY_REASON_PROVIDER_CANCELLED);
        default:
            return failure(result, PAK_STATUS_PROVIDER_CONTRACT, PAK_SECONDARY_REASON_PROVIDER_CONTRACT);
    }
}

void pak_secondary_limits_v1_default(pak_secondary_limits_v1 *limits)
{
    if (limits == NULL) return;
    memset(limits, 0, sizeof(*limits));
    limits->struct_size = sizeof(*limits);
    limits->abi_version = PAK_SECONDARY_ABI_VERSION;
    limits->max_span_bytes = PAK_SECONDARY_MAX_SPAN_BYTES;
    limits->max_read_bytes = PAK_SECONDARY_MAX_READ_BYTES;
    limits->max_read_calls = PAK_SECONDARY_MAX_READ_CALLS;
}

pak_status pak_verify_v11_secondary_raw_sha1(
    const pak_source_v1 *source,
    const pak_secondary_index *span,
    const pak_secondary_limits_v1 *limits,
    pak_read_budget_v1 *budget,
    uint8_t *workspace,
    size_t workspace_bytes,
    pak_secondary_result_v1 *result)
{
    const void *inputs[4] = {source, span, limits, budget};
    const size_t sizes[4] = {sizeof(*source), sizeof(*span), sizeof(*limits), sizeof(*budget)};
    mbedtls_sha1_context sha1;
    uint8_t computed_digest[20] = {0};
    unsigned i;
    int sha1_initialized = 0;
    int bad_input_storage = 0;

    /* Do not overwrite immutable metadata or the budget through an invalid output alias. */
    if (!storage_range_valid(result, sizeof(*result))) return PAK_STATUS_BAD_ARGUMENT;
    for (i = 0; i < 4; ++i) {
        if (inputs[i] != NULL) {
            if (!storage_range_valid(inputs[i], sizes[i])) bad_input_storage = 1;
            else if (storage_overlaps(result, sizeof(*result), inputs[i], sizes[i]))
                return PAK_STATUS_BAD_ARGUMENT;
        }
    }
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_SECONDARY_ABI_VERSION;
    result->last_provider_status = UINT32_MAX;
    failure(result, PAK_STATUS_BAD_ARGUMENT, PAK_SECONDARY_REASON_BAD_ARGUMENT);
    if (bad_input_storage || source == NULL || span == NULL || limits == NULL || budget == NULL ||
        workspace_bytes != PAK_SECONDARY_WORKSPACE_BYTES ||
        !storage_range_valid(workspace, workspace_bytes))
        return result->status;

    for (i = 0; i < 4; ++i) {
        if (storage_overlaps(workspace, workspace_bytes, inputs[i], sizes[i]))
            return failure(result, PAK_STATUS_BAD_ARGUMENT, PAK_SECONDARY_REASON_STORAGE_OVERLAP);
    }
    if (storage_overlaps(workspace, workspace_bytes, result, sizeof(*result)))
        return failure(result, PAK_STATUS_BAD_ARGUMENT, PAK_SECONDARY_REASON_STORAGE_OVERLAP);
    for (i = 0; i < 3; ++i) {
        if (storage_overlaps(budget, sizeof(*budget), inputs[i], sizes[i]))
            return failure(result, PAK_STATUS_BAD_ARGUMENT, PAK_SECONDARY_REASON_STORAGE_OVERLAP);
    }

    /* Workspace can now be borrowed and wiped without altering known metadata. */
    if (source->struct_size != sizeof(*source) || source->abi_version != PAK_READER_ABI_VERSION ||
        source->snapshot_token == 0 || source->snapshot_scope != PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE ||
        source->reserved != 0 || source->read_at == NULL ||
        limits->struct_size != sizeof(*limits) || limits->abi_version != PAK_SECONDARY_ABI_VERSION ||
        limits->reserved != 0 || limits->max_span_bytes == 0 || limits->max_span_bytes > PAK_SECONDARY_MAX_SPAN_BYTES ||
        limits->max_read_bytes == 0 || limits->max_read_bytes > PAK_SECONDARY_MAX_READ_BYTES ||
        limits->max_read_calls == 0 || limits->max_read_calls > PAK_SECONDARY_MAX_READ_CALLS ||
        budget->struct_size != sizeof(*budget) || budget->abi_version != PAK_SECONDARY_ABI_VERSION || budget->reserved != 0)
        goto cleanup;

    result->snapshot_token = source->snapshot_token;
    if (span->size == 0 || span->offset > source->logical_size ||
        span->size > source->logical_size - span->offset) {
        failure(result, PAK_STATUS_MALFORMED, PAK_SECONDARY_REASON_BAD_RANGE);
        goto cleanup;
    }
    if (span->size > limits->max_span_bytes) {
        failure(result, PAK_STATUS_RESOURCE_LIMIT, PAK_SECONDARY_REASON_SPAN_LIMIT);
        goto cleanup;
    }

    mbedtls_sha1_init(&sha1);
    sha1_initialized = 1;
    if (mbedtls_sha1_starts(&sha1) != 0) {
        failure(result, PAK_STATUS_INTERNAL_ERROR, PAK_SECONDARY_REASON_SHA1_BACKEND);
        goto cleanup;
    }
    while (result->bytes_hashed < span->size) {
        uint64_t remaining = span->size - result->bytes_hashed;
        uint32_t take = remaining > limits->max_read_bytes ? limits->max_read_bytes : (uint32_t)remaining;
        uint32_t actual = 0;
        pak_provider_status status;
        if (result->read_calls >= limits->max_read_calls || budget->remaining_calls == 0) {
            failure(result, PAK_STATUS_RESOURCE_LIMIT, PAK_SECONDARY_REASON_CALL_BUDGET);
            goto cleanup;
        }
        if (budget->remaining_requested_bytes < take) {
            failure(result, PAK_STATUS_RESOURCE_LIMIT, PAK_SECONDARY_REASON_BYTE_BUDGET);
            goto cleanup;
        }

        /* Reserve both counters before the callback; errors never refund attempts. */
        --budget->remaining_calls;
        budget->remaining_requested_bytes -= take;
        ++result->read_calls;
        result->requested_read_bytes += take;
        result->provider_observed = 1;
        result->last_requested = take;
        status = source->read_at(source->context, span->offset + result->bytes_hashed,
                                  workspace, take, &actual);
        result->last_provider_status = (uint32_t)status;
        result->last_actual = actual;
        if (actual > take) {
            failure(result, PAK_STATUS_PROVIDER_CONTRACT, PAK_SECONDARY_REASON_PROVIDER_CONTRACT);
            goto cleanup;
        }
        if (status != PAK_PROVIDER_OK) {
            provider_failure(result, status);
            goto cleanup;
        }
        if (actual != take) {
            failure(result, PAK_STATUS_PROVIDER_SHORT_READ, PAK_SECONDARY_REASON_PROVIDER_SHORT);
            goto cleanup;
        }
        if (mbedtls_sha1_update(&sha1, workspace, take) != 0) {
            failure(result, PAK_STATUS_INTERNAL_ERROR, PAK_SECONDARY_REASON_SHA1_BACKEND);
            goto cleanup;
        }
        result->bytes_hashed += take;
    }
    if (mbedtls_sha1_finish(&sha1, computed_digest) != 0) {
        failure(result, PAK_STATUS_INTERNAL_ERROR, PAK_SECONDARY_REASON_SHA1_BACKEND);
        goto cleanup;
    }
    result->span_complete = 1;
    if (memcmp(computed_digest, span->sha1, sizeof(computed_digest)) != 0) {
        failure(result, PAK_STATUS_CHECKSUM_MISMATCH, PAK_SECONDARY_REASON_DIGEST_MISMATCH);
        goto cleanup;
    }
    memcpy(result->digest, computed_digest, sizeof(result->digest));
    result->digest_valid = 1;
    result->status = PAK_STATUS_OK;
    result->reason = PAK_SECONDARY_REASON_NONE;

cleanup:
    if (sha1_initialized) mbedtls_sha1_free(&sha1);
    mbedtls_platform_zeroize(computed_digest, sizeof(computed_digest));
    mbedtls_platform_zeroize(workspace, workspace_bytes);
    return result->status;
}
