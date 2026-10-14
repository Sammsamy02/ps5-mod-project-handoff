The sealed menu-path prototype remains byte-identical. Its callback-policy sentence was too broad: snapshots occur separately for Pre and Post. A callback added after its own phase snapshot waits for that phase's next invocation; a pre callback adding a post callback can run the new post in the same dispatch. R5 tests will preserve and explicitly check this phase policy.

The sealed TSan fixture checks unordered-map mutation against dispatch while registering other functions. It does not establish safe simultaneous plain Func read/store on the same UFunction or a blanket native memory-model contract. R5 will separately test actual accessor publication, including target type/alignment/lock-free constraints.

No acceptance, test result or source hash in the sealed prototype is changed by this correction.
