// ADAPTER-INTERFACE.hpp — PROPOSED-ONLY sketch: the FName CONSTRUCTION seam
// the runtime would consume once construction is proven.
//
// worker-fname-construction, 2026-10-04-zcode-team.
//
// *** PROPOSED-ONLY. This header is a DESIGN ARTIFACT. It has never been
// compiled, reviewed, integrated, or executed, and nothing may link it. It
// sketches the interface shape only; the real implementation arrives only
// after IN-GAME-EXPERIMENT.md FC-1..FC-3 produce in-process evidence. ***
//
// Style mirrors the sibling conversion seam adapters
// (worker-3-gamethread/adapters/gamethread_seams.hpp): typed adapters over
// the existing UE4SS path, every absent-facts case an EXPLICIT REFUSAL with
// preserved state, and PROOF TOKENS that only genuine in-process evidence
// can mint — a path, hash, boolean or caller flag can never mint them.
// Family vocabulary matches that adapter's fact families ("FName" is already
// a family there, gamethread_seams.hpp:190). CONSTRUCTION stays a separate
// family from CONVERSION (P2 rule; worker-5 kFNamePoolPredicateExcluded):
// nothing in this header serves FName->string, and nothing in the conversion
// path satisfies anything below.
//
// Facts this adapter CONSUMES (never creates):
//   - worker-2 profile: fname_ctor_call_candidate elf_image_vaddr 0x23d590
//     (candidate-only), FName width entries 0 -> width MUST NOT be defaulted.
//   - worker-1 live facts: image identity + base provenance (needs-base
//     refusal otherwise).
//   - FC-1/FC-3 probe receipts (IN-GAME-EXPERIMENT.md): the only sources of
//     the two tokens below.
//
// The port-side landing spot, for reference only: FName::ConstructorInternal
// (NameTypes.hpp:228) with the fname_native_constructor_abi explicit-output
// lowering void fn(ptr output, ptr text, i32 mode) (CENSUS C-8).

#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace modbridge::seams::fname_construction
{

// ---------------------------------------------------------------------------
// Proof tokens (minted ONLY by probe receipts; see IN-GAME-EXPERIMENT.md)
// ---------------------------------------------------------------------------

// Minted iff FC-1 verdict == Validated (code shape at image_base+0x23d590
// PLUS, when materialized, five anchor call-site rel32 agreement).
struct ConstructorAddressValidated
{
    uint64_t runtime_address;      // image_base + 0x23d590, base provenance embedded
    char     base_provenance[64];  // verbatim from the live-module facts record
    uint32_t probe_seq;            // FC-1 JSONL seq for audit
};

// Minted iff FC-3 pass criteria P-a..P-d all held (four-call schedule,
// sret writes confirmed, same-name consistency, text discrimination).
// Does NOT include Add-mode evidence: Add is refused by the probe forever.
struct ConstructorCallProven
{
    uint64_t runtime_address;
    uint32_t probe_seq;            // first FC-3 post-record seq
    uint32_t observed_prefix_len;  // capture-window observation ONLY — explicitly NOT a width claim
};

// ---------------------------------------------------------------------------
// Results and refusals
// ---------------------------------------------------------------------------

enum class ConstructRefusal : std::uint8_t
{
    AddressNotValidated,      // no ConstructorAddressValidated token (FC-1 missing/refuted/inconclusive)
    CallNotProven,            // address validated but FC-3 never minted ConstructorCallProven
    BaseProvenanceMissing,    // worker-1 live facts absent/stale ("needs-base")
    ImageIdentityMismatch,    // live module sha != pinned capture sha
    TextNotStaticLifetime,    // caller passed text whose lifetime may end before the call
    ModeForbidden,            // anything other than Find requested (Add mutates the pool; refused always)
    WidthUnresolvedForParams, // marshalling consumer asked for a sized FName slot with width still unresolved (D-1)
    ConcurrentVerification,   // retained-program VerifyFNameConstructor hook active; serialization required
};

// Opaque constructed value. The fixed 32-byte capture is the same INSTRUMENT
// window the probe uses — it is NOT a storage-width claim. Raw bytes only:
// no field interpretation exists until FName width/pool facts are validated
// (CENSUS D-1, D-3); equality against raw NamePrivate bytes remains lawful.
struct FNameBytes
{
    std::array<std::uint8_t, 32> raw{};   // zero-initialized; callee fills a prefix
    std::uint32_t changed_prefix = 0;     // observational extent from the proven call
};

struct Constructed
{
    FNameBytes value;
    std::uint32_t probe_seq;              // provenance: which probe minted the capability
};

struct ConstructOutcome
{
    bool ok;
    Constructed value;                    // valid iff ok
    ConstructRefusal refusal;             // valid iff !ok; state preserved, nothing half-written
};

// ---------------------------------------------------------------------------
// The seam (all functions: noexcept-shaped, allocation-free, refusal-first)
// ---------------------------------------------------------------------------

class FNameConstructionSeam
{
public:
    // Capability query: what the current evidence tokens permit. Consumers
    // (rung-3 lookups, UFunction marshalling) gate on these BEFORE building
    // any FName-dependent request.
    struct Capability
    {
        bool address_validated;  // FC-1 token present
        bool call_proven;        // FC-3 token present
        bool width_resolved;     // FALSE today and until a width-facts predicate passes (D-1)
    };
    [[nodiscard]] auto capability() const noexcept -> Capability;

    // Construct from UTF-16 text with engine pool semantics, Find mode only.
    // Refuses (never throws, never calls) unless BOTH tokens are present,
    // base provenance is live, image identity matches, text is
    // process-static-lifetime, and mode == Find. Calls at most once per
    // invocation through the explicit-output ABI void fn(ptr,ptr,i32).
    [[nodiscard]] auto construct_find_utf16(std::u16string_view text) noexcept
        -> ConstructOutcome;

    // Same-name consistency re-check (the FC-3 P-c invariant, reusable as a
    // runtime health gate): constructs twice into distinct buffers and
    // compares. Any divergence => refusal ConstructorCallNotProven semantics
    // (capability demoted; no silent retry).
    [[nodiscard]] auto construct_and_self_check_utf16(std::u16string_view text) noexcept
        -> ConstructOutcome;

    // ---- Marshalling helper for rung-3 consumers (PROPOSED-ONLY) ----
    // Copy a constructed value's bytes into a UFunction param slot. Refuses
    // WidthUnresolvedForParams while FName width is unresolved (0 accessor
    // entries; D-1) — slot sizing may never be defaulted from this adapter.
    [[nodiscard]] auto copy_into_param_slot(const Constructed& src,
                                            std::span<std::uint8_t> slot) noexcept
        -> ConstructRefusal; // returns a refusal or valueless success

private:
    // Tokens are injected by the controller from probe receipts; there is NO
    // public setter — a boolean/path/hash can never mint them (mirrors the
    // sibling adapter's minting rule).
    FNameConstructionSeam(std::optional<ConstructorAddressValidated>,
                          std::optional<ConstructorCallProven>) noexcept;
};

// ---------------------------------------------------------------------------
// Consumer contract (why this seam exists — CENSUS E-2)
// ---------------------------------------------------------------------------
// Rung-3 gating that unlocks when call_proven == true (with width facts for
// the sized-slot parts, tracked separately):
//   * FindFirstOf / StaticFindObject (modreqs A3/A4): class-name keys become
//     constructible.
//   * StaticConstructObject(class, outer, FName) (A5): the FName parameter
//     becomes suppliable.
//   * UFunction param marshalling (B4/B5/B6): FName params need BOTH this
//     seam (value) and validated width (slot size). The two facts are
//     tracked separately on purpose.
// Conversion (FName -> string, Conv_NameToString / FNameToStringMethod) is a
// different seam and never satisfies anything in this header.

} // namespace modbridge::seams::fname_construction
