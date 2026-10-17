#include "pak_reconciled_payload.h"
#include <mbedtls/sha1.h>
#include <string.h>

typedef struct { uintptr_t begin, end; } byte_range;

static int range_for(const void *pointer, size_t bytes, byte_range *range) {
    const uintptr_t begin=(uintptr_t)pointer;
    if ((bytes && !pointer) || bytes>UINTPTR_MAX-begin) return 0;
    *range=(byte_range){begin,begin+bytes};
    return 1;
}

static int overlaps(byte_range a, byte_range b) {
    return a.begin!=a.end && b.begin!=b.end && a.begin<b.end && b.begin<a.end;
}

static void clear_bytes(void *pointer, size_t bytes) {
    volatile uint8_t *p=pointer;
    for (size_t i=0;i<bytes;++i) p[i]=0;
}

static int preflight(const pak_source_v1 *source, const pak_entry_result_v1 *entry,
    const pak_entry_block_v1 *blocks, size_t blocks_count, const uint8_t *expected,
    const pak_reconciled_payload_limits_v1 *limits, pak_reconciled_payload_buffers_v1 b,
    bz_budget *budget) {
    if (!source || !entry || !expected || !limits || !budget || !b.codec_state ||
        blocks_count>PAK_RECONCILED_PAYLOAD_MAX_BLOCKS ||
        !b.compressed_bytes || b.compressed_bytes>BZ_MAX_BLOCK_BYTES ||
        !b.codec_arena_bytes || b.codec_arena_bytes>BZ_MAX_ARENA_BYTES ||
        b.output_capacity>PAK_RECONCILED_PAYLOAD_MAX_OUTPUT_BYTES ||
        (uintptr_t)source%_Alignof(pak_source_v1) ||
        (uintptr_t)entry%_Alignof(pak_entry_result_v1) ||
        (blocks_count && (uintptr_t)blocks%_Alignof(pak_entry_block_v1)) ||
        (uintptr_t)limits%_Alignof(pak_reconciled_payload_limits_v1) ||
        (uintptr_t)budget%_Alignof(bz_budget) ||
        (uintptr_t)b.codec_state%_Alignof(bz_state)) return 0;
    const void *pointers[10]={source,entry,blocks,expected,limits,
        b.compressed,b.codec_arena,b.codec_state,b.output,budget};
    const size_t sizes[10]={sizeof(*source),sizeof(*entry),blocks_count*sizeof(*blocks),20,
        sizeof(*limits),b.compressed_bytes,b.codec_arena_bytes,sizeof(*b.codec_state),
        b.output_capacity,sizeof(*budget)};
    byte_range ranges[10];
    for (size_t i=0;i<10;++i) {
        if (!range_for(pointers[i],sizes[i],&ranges[i])) return 0;
        for (size_t j=0;j<i;++j) if (overlaps(ranges[i],ranges[j])) return 0;
    }
    return source->struct_size>=sizeof(*source) && source->abi_version==PAK_READER_ABI_VERSION &&
        !source->reserved && source->read_at && source->snapshot_token &&
        source->snapshot_scope==PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE &&
        entry->struct_size>=sizeof(*entry) && entry->abi_version==PAK_ENTRY_DECODER_ABI_VERSION &&
        !entry->reserved && entry->status==PAK_ENTRY_STATUS_OK && entry->record_complete==1 &&
        limits->struct_size>=sizeof(*limits) && limits->abi_version==PAK_RECONCILED_PAYLOAD_ABI_VERSION &&
        !limits->reserved && limits->max_read_bytes &&
        limits->max_read_bytes<=PAK_RECONCILED_PAYLOAD_MAX_READ_BYTES &&
        limits->max_read_calls<=PAK_RECONCILED_PAYLOAD_MAX_READ_CALLS &&
        limits->max_requested_read_bytes<=PAK_RECONCILED_PAYLOAD_MAX_REQUESTED_BYTES &&
        limits->max_output_bytes<=PAK_RECONCILED_PAYLOAD_MAX_OUTPUT_BYTES &&
        limits->codec.max_input_bytes<=BZ_MAX_BLOCK_BYTES &&
        limits->codec.max_output_bytes<=BZ_MAX_BLOCK_BYTES &&
        limits->codec.max_arena_bytes<=BZ_MAX_ARENA_BYTES &&
        b.codec_arena_bytes<=limits->codec.max_arena_bytes &&
        limits->codec.input_per_step && limits->codec.input_per_step<=BZ_MAX_STEP_BYTES &&
        limits->codec.output_per_step && limits->codec.output_per_step<=BZ_MAX_STEP_BYTES;
}

static void clean(pak_reconciled_payload_buffers_v1 b, int failed) {
    if (failed) clear_bytes(b.output,b.output_capacity);
    clear_bytes(b.compressed,b.compressed_bytes);
    clear_bytes(b.codec_arena,b.codec_arena_bytes);
    clear_bytes(b.codec_state,sizeof(*b.codec_state));
}

static pak_reconciled_payload_status geometry(const pak_source_v1 *source,
    const pak_entry_result_v1 *entry, const pak_entry_block_v1 *blocks, size_t count,
    pak_reconciled_payload_buffers_v1 b, const pak_reconciled_payload_limits_v1 *limits,
    int *none) {
    *none=entry->method_index==0 && entry->method_name_bytes==4 &&
          memcmp(entry->method_name,"None",4)==0;
    const int zlib=entry->method_index!=0 && entry->method_name_bytes==4 &&
                   memcmp(entry->method_name,"Zlib",4)==0;
    if (!*none && !zlib) return PAK_RECONCILED_PAYLOAD_UNSUPPORTED;
    if (entry->method_index>=PAK_ENTRY_DECODER_MAX_METHODS || entry->block_count!=count)
        return PAK_RECONCILED_PAYLOAD_MALFORMED;
    if (entry->uncompressed_size>limits->max_output_bytes ||
        entry->uncompressed_size>b.output_capacity) return PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT;
    const uint64_t header_bytes=*none ? 53 : 57+16*(uint64_t)count;
    if (entry->header_bytes!=header_bytes || entry->archive_offset>UINT64_MAX-header_bytes ||
        entry->data_offset!=entry->archive_offset+header_bytes ||
        entry->data_offset>source->logical_size ||
        entry->compressed_size>source->logical_size-entry->data_offset)
        return PAK_RECONCILED_PAYLOAD_MALFORMED;
    if (*none) {
        if (count) return PAK_RECONCILED_PAYLOAD_UNSUPPORTED;
        if (entry->compressed_size!=entry->uncompressed_size) return PAK_RECONCILED_PAYLOAD_MALFORMED;
        /* Chunked copy/hash uses the checked total output/caller cap above. */
        return PAK_RECONCILED_PAYLOAD_OK;
    }
    if (!count) return PAK_RECONCILED_PAYLOAD_UNSUPPORTED;
    uint64_t compressed_position=entry->data_offset, output_position=0;
    for (size_t i=0;i<count;++i) {
        const pak_entry_block_v1 *block=&blocks[i];
        if (block->compressed_size>BZ_MAX_BLOCK_BYTES ||
            block->compressed_size>limits->codec.max_input_bytes ||
            block->compressed_size>b.compressed_bytes ||
            block->uncompressed_size>BZ_MAX_BLOCK_BYTES ||
            block->uncompressed_size>limits->codec.max_output_bytes)
            return PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT;
        if (!block->compressed_size || block->compressed_offset!=compressed_position ||
            block->uncompressed_offset!=output_position ||
            compressed_position>source->logical_size ||
            block->compressed_size>source->logical_size-compressed_position ||
            output_position>entry->uncompressed_size ||
            block->uncompressed_size>entry->uncompressed_size-output_position)
            return PAK_RECONCILED_PAYLOAD_MALFORMED;
        compressed_position+=block->compressed_size;
        output_position+=block->uncompressed_size;
    }
    return compressed_position-entry->data_offset==entry->compressed_size &&
           output_position==entry->uncompressed_size ? PAK_RECONCILED_PAYLOAD_OK
                                                   : PAK_RECONCILED_PAYLOAD_MALFORMED;
}

static pak_reconciled_payload_status read_stored(const pak_source_v1 *source,
    const pak_reconciled_payload_limits_v1 *limits, uint64_t offset, uint8_t *destination,
    uint32_t requested, pak_reconciled_payload_result_v1 *result) {
    if (!requested || requested>limits->max_read_bytes || offset>source->logical_size ||
        requested>source->logical_size-offset) return PAK_RECONCILED_PAYLOAD_MALFORMED;
    if (result->read_calls>=limits->max_read_calls ||
        result->requested_read_bytes>limits->max_requested_read_bytes ||
        requested>limits->max_requested_read_bytes-result->requested_read_bytes)
        return PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT;
    /* Local reservation ONLY. The supplied composition source owns any
     * global reservation before its original-provider callback. */
    ++result->read_calls;
    result->requested_read_bytes+=requested;
    result->last_requested=requested;
    uint32_t actual=0;
    const pak_provider_status status=source->read_at(source->context,offset,destination,requested,&actual);
    result->provider_observed=1;
    result->last_provider_status=(uint32_t)status;
    result->last_actual=actual;
    if (actual>requested) return PAK_RECONCILED_PAYLOAD_PROVIDER_CONTRACT;
    switch (status) {
        case PAK_PROVIDER_OK: return actual==requested ? PAK_RECONCILED_PAYLOAD_OK
                                                     : PAK_RECONCILED_PAYLOAD_SHORT_READ;
        case PAK_PROVIDER_CHANGED: return PAK_RECONCILED_PAYLOAD_SOURCE_CHANGED;
        case PAK_PROVIDER_CANCELLED: return PAK_RECONCILED_PAYLOAD_CANCELLED;
        case PAK_PROVIDER_IO_ERROR:
        case PAK_PROVIDER_UNAVAILABLE: return PAK_RECONCILED_PAYLOAD_PROVIDER_ERROR;
        case PAK_PROVIDER_CONTRACT_ERROR:
        default: return PAK_RECONCILED_PAYLOAD_PROVIDER_CONTRACT;
    }
}

static int reserve_copy(bz_budget *budget, uint64_t bytes) {
    if (budget->work_used>budget->work_limit || budget->steps_used>=budget->step_limit ||
        budget->allocations_used>budget->allocation_limit ||
        bytes>budget->work_limit-budget->work_used) return 0;
    budget->work_used+=bytes;
    ++budget->steps_used;
    return 1;
}

void pak_reconciled_payload_limits_v1_default(pak_reconciled_payload_limits_v1 *limits) {
    if (!limits) return;
    *limits = (pak_reconciled_payload_limits_v1){0};
    limits->struct_size = sizeof(*limits);
    limits->abi_version = PAK_RECONCILED_PAYLOAD_ABI_VERSION;
    limits->coverage_profile = PAK_RECONCILED_PROFILE_RAW_UNENCRYPTED_STORED_SHA1_V1;
    limits->max_read_bytes = PAK_RECONCILED_PAYLOAD_MAX_READ_BYTES;
    limits->max_read_calls = PAK_RECONCILED_PAYLOAD_MAX_READ_CALLS;
    limits->max_requested_read_bytes = PAK_RECONCILED_PAYLOAD_MAX_REQUESTED_BYTES;
    limits->max_output_bytes = PAK_RECONCILED_PAYLOAD_MAX_OUTPUT_BYTES;
    limits->codec = bz_default_limits();
}

pak_reconciled_payload_result_v1 pak_decode_reconciled_v11_payload(
    const pak_source_v1 *source, const pak_entry_result_v1 *entry,
    const pak_entry_block_v1 *blocks, size_t blocks_count,
    const uint8_t expected_stored_sha1[20], const pak_reconciled_payload_limits_v1 *limits,
    pak_reconciled_payload_buffers_v1 buffers, bz_budget *codec_budget) {
    pak_reconciled_payload_result_v1 result = {0};
    result.struct_size = sizeof(result);
    result.abi_version = PAK_RECONCILED_PAYLOAD_ABI_VERSION;
    result.status = PAK_RECONCILED_PAYLOAD_BAD_ARGUMENT;
    if (!preflight(source,entry,blocks,blocks_count,expected_stored_sha1,limits,buffers,codec_budget))
        return result;
    const pak_source_v1 stable_source=*source;
    const pak_entry_result_v1 stable_entry=*entry;
    const pak_reconciled_payload_limits_v1 stable_limits=*limits;
    uint8_t expected[20], digest[20]={0};
    memcpy(expected,expected_stored_sha1,20);
    result.coverage_profile=stable_limits.coverage_profile;
    result.snapshot_token=stable_source.snapshot_token;
    mbedtls_sha1_context hash;
    int hash_initialized=0;
    if (stable_limits.coverage_profile!=PAK_RECONCILED_PROFILE_RAW_UNENCRYPTED_STORED_SHA1_V1) {
        result.status=PAK_RECONCILED_PAYLOAD_UNSUPPORTED;
        goto done;
    }
    if (codec_budget->work_used>codec_budget->work_limit ||
        codec_budget->steps_used>codec_budget->step_limit ||
        codec_budget->allocations_used>codec_budget->allocation_limit) {
        result.status=PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT;
        goto done;
    }
    int none;
    result.status=geometry(&stable_source,&stable_entry,blocks,blocks_count,buffers,&stable_limits,&none);
    if (result.status!=PAK_RECONCILED_PAYLOAD_OK) goto done;
    mbedtls_sha1_init(&hash);
    hash_initialized=1;
    if (mbedtls_sha1_starts(&hash)!=0) {
        result.status=PAK_RECONCILED_PAYLOAD_INTERNAL_ERROR;
        goto done;
    }
    if (none) {
        size_t copied=0;
        while (copied<stable_entry.uncompressed_size) {
            size_t request=(size_t)stable_entry.uncompressed_size-copied;
            if (request>stable_limits.max_read_bytes) request=stable_limits.max_read_bytes;
            if (request>buffers.compressed_bytes) request=buffers.compressed_bytes;
            result.status=read_stored(&stable_source,&stable_limits,stable_entry.data_offset+copied,
                                     buffers.compressed,(uint32_t)request,&result);
            if (result.status!=PAK_RECONCILED_PAYLOAD_OK) goto done;
            if (mbedtls_sha1_update(&hash,buffers.compressed,request)!=0) {
                result.status=PAK_RECONCILED_PAYLOAD_INTERNAL_ERROR;
                goto done;
            }
            result.bytes_hashed+=request;
            if (!reserve_copy(codec_budget,request)) {
                result.status=PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT;
                goto done;
            }
            memcpy(buffers.output+copied,buffers.compressed,request);
            copied+=request;
        }
    } else {
        for (size_t i=0;i<blocks_count;++i) {
            const pak_entry_block_v1 block=blocks[i];
            size_t loaded=0;
            while (loaded<block.compressed_size) {
                size_t request=(size_t)block.compressed_size-loaded;
                if (request>stable_limits.max_read_bytes) request=stable_limits.max_read_bytes;
                result.status=read_stored(&stable_source,&stable_limits,block.compressed_offset+loaded,
                                         buffers.compressed+loaded,(uint32_t)request,&result);
                if (result.status!=PAK_RECONCILED_PAYLOAD_OK) goto done;
                if (mbedtls_sha1_update(&hash,buffers.compressed+loaded,request)!=0) {
                    result.status=PAK_RECONCILED_PAYLOAD_INTERNAL_ERROR;
                    goto done;
                }
                result.bytes_hashed+=request;
                loaded+=request;
            }
            result.codec_attempted=1;
            uint8_t *block_output=buffers.output ? buffers.output+(size_t)block.uncompressed_offset : NULL;
            const bz_result decoded=bz_decode(buffers.compressed,(size_t)block.compressed_size,
                (size_t)block.uncompressed_size,block_output,(size_t)block.uncompressed_size,
                buffers.codec_arena,buffers.codec_arena_bytes,buffers.codec_state,codec_budget,stable_limits.codec);
            result.codec_status=decoded.status;
            if (decoded.status!=BZ_OK || !decoded.complete || decoded.decoded_size!=block.uncompressed_size) {
                result.status=decoded.status==BZ_RESOURCE_LIMIT ? PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT
                                                              : PAK_RECONCILED_PAYLOAD_CODEC_REJECTED;
                goto done;
            }
        }
    }
    if (result.bytes_hashed!=stable_entry.compressed_size || mbedtls_sha1_finish(&hash,digest)!=0) {
        result.status=PAK_RECONCILED_PAYLOAD_INTERNAL_ERROR;
        goto done;
    }
    uint8_t different=0;
    for (size_t i=0;i<20;++i) different|=(uint8_t)(digest[i]^expected[i]);
    if (different) {
        result.status=PAK_RECONCILED_PAYLOAD_DIGEST_MISMATCH;
        goto done;
    }
    result.status=PAK_RECONCILED_PAYLOAD_OK;
    result.complete=1;
    result.digest_valid=1;
    result.decoded_bytes=(size_t)stable_entry.uncompressed_size;
    memcpy(result.digest,digest,20);
done:
    if (hash_initialized) mbedtls_sha1_free(&hash);
    clear_bytes(expected,sizeof(expected));
    clear_bytes(digest,sizeof(digest));
    clean(buffers,result.status!=PAK_RECONCILED_PAYLOAD_OK);
    return result;
}
