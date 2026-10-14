#include "pak_candidate_io.h"

#include <string.h>

static int valid_source(const pak_source_v1 *source)
{
    return source != NULL && source->struct_size >= sizeof(*source) &&
           source->abi_version == PAK_READER_ABI_VERSION && source->reserved == 0u &&
           source->snapshot_token != 0u && source->read_at != NULL &&
           source->snapshot_scope == PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE;
}

pak_provider_status pak_candidate_charging_read_at(
    void *context, uint64_t offset, uint8_t *destination,
    uint32_t requested, uint32_t *actual)
{
    pak_candidate_charging_context *owner = context;
    pak_provider_status status;
    if (actual != NULL) *actual = 0u;
    if (owner == NULL) return PAK_PROVIDER_CONTRACT_ERROR;
    if (owner->cause != PAK_CANDIDATE_IO_NONE) return PAK_PROVIDER_CONTRACT_ERROR;
    if (actual == NULL || destination == NULL || !valid_source(owner->base) ||
        owner->budget == NULL || owner->budget->struct_size < sizeof(*owner->budget) ||
        owner->budget->abi_version != PAK_SECONDARY_ABI_VERSION || owner->budget->reserved != 0u ||
        owner->budget->remaining_requested_bytes > PAK_CANDIDATE_MAX_TOTAL_BYTES ||
        owner->budget->remaining_calls > PAK_CANDIDATE_MAX_READ_CALLS ||
        owner->max_read_bytes == 0u || owner->max_read_bytes > PAK_CANDIDATE_MAX_READ_BYTES ||
        requested == 0u || requested > owner->max_read_bytes ||
        offset > owner->base->logical_size || requested > owner->base->logical_size - offset ||
        owner->original_callback_calls == UINT32_MAX) {
        owner->cause = PAK_CANDIDATE_IO_BAD_REQUEST;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (owner->budget->remaining_calls == 0u) {
        owner->cause = PAK_CANDIDATE_IO_CALL_BUDGET;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (requested > owner->budget->remaining_requested_bytes) {
        owner->cause = PAK_CANDIDATE_IO_BYTE_BUDGET;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    owner->budget->remaining_requested_bytes -= requested;
    owner->budget->remaining_calls--;
    owner->original_callback_calls++;
    owner->provider_observed = 1u;
    owner->last_requested = requested;
    status = owner->base->read_at(owner->base->context, offset, destination, requested, actual);
    owner->last_provider_status = (uint32_t)status;
    owner->last_actual = *actual;
    if (*actual > requested) {
        owner->cause = PAK_CANDIDATE_IO_PROVIDER_CONTRACT;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    return status;
}

pak_provider_status pak_candidate_directory_tee_read_at(
    void *context, uint64_t offset, uint8_t *destination,
    uint32_t requested, uint32_t *actual)
{
    pak_candidate_tee_context *tee = context;
    pak_provider_status status;
    if (actual != NULL) *actual = 0u;
    if (tee == NULL) return PAK_PROVIDER_CONTRACT_ERROR;
    if (tee->cause != PAK_CANDIDATE_IO_NONE) return PAK_PROVIDER_CONTRACT_ERROR;
    if (actual == NULL || destination == NULL || !valid_source(tee->raw_base) ||
        tee->capture == NULL || tee->capture_capacity == 0u ||
        tee->capture_capacity > PAK_SECONDARY_MAX_SPAN_BYTES ||
        tee->span_bytes > tee->capture_capacity || tee->copied > tee->span_bytes ||
        tee->span_offset > tee->raw_base->logical_size ||
        tee->span_bytes > tee->raw_base->logical_size - tee->span_offset ||
        requested == 0u || requested > PAK_SECONDARY_MAX_READ_BYTES ||
        offset != tee->span_offset + tee->copied ||
        requested > tee->span_bytes - tee->copied ||
        tee->original_callback_calls == UINT32_MAX) {
        tee->cause = PAK_CANDIDATE_IO_TEE_RANGE;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    tee->original_callback_calls++;
    status = tee->raw_base->read_at(tee->raw_base->context, offset, destination, requested, actual);
    if (*actual > requested) {
        tee->cause = PAK_CANDIDATE_IO_PROVIDER_CONTRACT;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (status == PAK_PROVIDER_OK && *actual != 0u) {
        memcpy(tee->capture + (size_t)tee->copied, destination, *actual);
        tee->copied += *actual;
    }
    return status;
}
