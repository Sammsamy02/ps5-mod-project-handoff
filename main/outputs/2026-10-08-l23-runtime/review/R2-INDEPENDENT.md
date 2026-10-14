# Independent review of the typescan r2 repair

Result: no actionable correctness findings in the reviewed r2 delta. This is a local source and receipt review; it does not establish execution on the console. No source, CHANGES manifest, package, or frozen build artifact was edited, and no broad tests were rerun.

## Scope and source counts

Compared `typescan-r2` with the retained `typescan` overlay and checked every r2 replacement's candidate, copied-original, and original SHA-256 against its CHANGES record. The replacement list grows from 11 to 12: TypeChecker.cpp changes, CustomProperty.cpp is newly selected, and the other ten replacements are byte-identical to the earlier candidates. Of four added helper headers, only NativePropertyPointerGuard.hpp and NativePropertyPointerGuard.inl change; NativePropertyReadBackend.inl and NativeTypeScanPolicy.hpp remain byte-identical. The r2 repair therefore changes four implementation/header files relative to the prior selection. The profile, initializer candidate, and existing Map/Set/Array getter candidates remain unchanged.

Independently scanned the 374 selected source units in the retained l22-build-r3 pass1 COMPOSITION-RECEIPT for assignments through GetInner/GetKeyProp/GetValueProp/GetElementProp. Exactly two matches occur, both in the original lua-custom-property unit (lines 45 and 61). Both are covered by this repair. This is a bounded census of the selected source units, not a claim about all upstream source or every possible syntactic assignment form.

## Guarded next-field traversal

The native branch in candidate/src/TypeChecker.cpp:193 now invokes ReadNext before the current field is dereferenced by the native bootstrap class lookup. Operations before that call only compare/store pointer values for cycle detection. ReadNext resolves the existing FField::Next entry with the established exact missing-key behavior, validates the current field with ReadClass, reads the next pointer through the guarded read backend, permits null termination, rejects self-links and misalignment, and validates a non-null target before returning it. The existing visited vector rejects longer cycles.

A missing traversal key remains collectable by MissingKeys and leaves next null for that chain; unreadable fields, invalid targets, and unknown classes remain hard failures rather than becoming missing-layout observations. The offset lookup does not insert a map entry. The repair adds no fixed offset, raw engine getter call on the native path, or engine-memory write. Published reflection lifetime and guarded-read concurrency assumptions remain those of the existing backend; this change does not establish arbitrary concurrent engine-object lifetime safety.

## Custom-owned array initialization

The new TU-local SetOwnedArrayInner in candidate/src/CustomProperty.cpp:13 accepts a unique_ptr to a CustomProperty-derived allocation. Its only two call sites receive fresh allocations made by these constructors. It looks up the already published FArrayProperty::Inner offset without insertion, rejects a missing key, null ownership, a negative offset, and any pointer-width write extending past sizeof the actual deduced allocation type. Short-circuit evaluation makes the subtraction safe after the negative and upper-bound checks. memcpy copies the pointer representation into the owned custom object's storage without dereferencing the borrowed inner pointer or requiring an aligned pointer store.

The selected CustomProperty.hpp supplies the existing custom storage backing this representation (m_data[0x128]); CustomArrayProperty adds no extra storage. The first overload deduces CustomArrayProperty and the second CustomProperty, matching their actual allocations. Neither call receives an engine-owned FArrayProperty. This is a construction seam, not an exported generic setter, and it retains the existing runtime layout key rather than shipping a new layout fact.

The dummy inner property in the second overload is now held by unique_ptr through SetClass, outer-property construction, and the checked Inner write. Any exception destroys the local owned allocations. release occurs only after successful initialization and retains the pre-existing successful dummy-inner lifetime convention. The first overload continues to borrow its caller-supplied inner property. No change to that ownership convention is implied.

## Existing verification inspected

Read the existing r2 HOST-TESTS.json receipt: five suites in plain and fatal ASan/UBSan configurations, ten exit-zero runs. Read TARGET-COMPILE.json: six exit-zero target compile records. Reviewed the focused tests for guarded initial/next pointers, null/self/missing Next, refusal of unsafe raw traversal, long cycles, both real extracted custom-array constructors, missing Inner, negative offset, and pointer-width boundary refusal. These receipts and tests are corroborating evidence, not fresh executions performed by this review. The full runtime twin build and packaging remain the root task's checks.
