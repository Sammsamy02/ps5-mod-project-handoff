# Genuine Instruction owned-result integration

The old body calls the bounded MemAccessor into an unregistered stack destination, ignores refusal, and returns the pointer-holder label. The actual old implementation fails the first owned x64 control. The native-only adaptation registers only its own zero-initialized local result and requires a complete real read. Unknown/unreadable/short reads throw instead of becoming branch targets; x86 results are zero-extended. Direct relative/absolute destination behavior is unchanged.

Eleven allocated-data controls pass ASan/UBSan, including a real RIP-relative indirect holder. The actual SDK consumer compilation passes. Initial fixture includes and missing SDK environment were corrected before the authoritative red-2, green-final and native-final receipts. No generated code executes in these controls. All consumers of the inline header must be rebuilt coherently.
