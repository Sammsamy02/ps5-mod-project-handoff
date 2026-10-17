# Two pointer handoffs — Mac-only, 2026-10-12

**A closes as in-place construction.** The `.ucas` open loop populates the partition array already owned by `container+0x1d0`. There is no separate completed-array installation step.

**B closes for a specific static queue branch.** A newly prepared task is linked into the local pending list, inserted into the queue at `factory+0x130`, removed as the same pointer, and consumed with its resource field at `task+0x68`. The proof is conditional on the queue's `+0x108` mode flag being unequal to1, a noncancelled task being selected, and successful control-flow branches. It does not establish the mode selected by a live launch or cover every priority-bucket alternative.

All VAs are image-relative in the preserved executable, SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,194241424 bytes. No names/signatures are inferred, and no values are promoted for shipping. This is a single saved-image source; byte verification is repeatability, not an independent second semantic source.

## A — open result to container array to consumer

| Edge | Exact image facts |
|---|---|
| Container owns header | `0x1754560` requests0x248 bytes at0x1754864.0x1754873 keeps returned allocation inR12;0x1754894 preserves it atstack[rbp-0x1050].0x17548ce zeroes the array header beginning[R12+0x1d0]. |
| Array pointer installed through header |0x1754c50 passesRCX=&container[0x1d0] to0x25e20 at0x1754c6c, with elementsize0x30 and alignment8.0x25e96 passes that header to0x62fc0. In0x62fc0,RBX retains the supplied header;allocation/reallocation0xbef20 returnsRAX and0x62ffe stores[header]=RAX. A separate growth branch at0x1754c98 also receives the same header. |
| Record address belongs to that array |0x1754cce loadsRBX=[container+0x1d0].0x1754cca/0x1754cd6 compute index*3*16=recordstride0x30.0x1754cdA writescontainer into record[0].0x1754ef9 formsR13=arraybase+recordoffset. |
| Open resource published into record | Factory call0x1754df0 usesRDI=&stack[rbp-0x158] as aggregate result. The previously traced callee stores the resource pointer atresult+0x108 (`factory-audit`,0x1c57a). That is stack[rbp-0x50].0x1754f36 loads precisely this slot and0x1754f3a writesrecord+0x18.0x1754f45 writes returned size into record+0x20. |
| Consumer uses the same member/record layout | Frontend0x1756600 takes arraybase=[container+0x1d0] at0x1756ab9, computes the same0x30 stride, and obtains record+0x18.0x1756bdc forms&record+0x18;0x1756bf4 loads the resource pointer once,0x1756c17 writesdescriptor[0]. The descriptor is passed tofactory+0x28 at0x1756f96. |

This closes the specifically missing array-storage edge at construction and consumption. It does not identify which container instance was selected during a live read, prove that all containers succeed at opening, or establish a different game/version's layout.

## B — pending list to queue to returned task

Let `F` be the complete factory implementation and `Q=F+0x130` its queue object. The incoming secondary factory interface is adjusted by-8 in the previously traced adapter0x20760 before0x20010. The queue's mode flag `Q+0x108` is therefore `F+0x238`; these are the same byte, not similarly named flags.

| Edge | Exact image facts |
|---|---|
| New task returned | In0x1f6c0's new-task branch,0x1f89a selects a0xb0-byte pool record intoRBX, zeroes its fields,0x1f968 saves it inlocal[rbp-0x50], and0x1f970 sets caller's fresh flag to1.0x1fa81 reloads that pointer intoR14,0x1fa99 returnsRAX=R14. Reused-task branches are outside this closure. |
| Resource written to that task |0x20010 calls0x1f6c0 at0x203bd and keeps returnedRAX inR14 at0x203c2. When fresh flag==1,0x20404 reloads the original descriptor,0x20418 loadsdescriptor[0],0x2041f writes that value totask+0x68. The following stores establish offset+0x78 and length+0x88. |
| Pending-list link |0x20423 proposes previous_tail+0x50;0x20442 proposes&local_head[rbp-0x40];0x20450 chooses the head address when previous tail isnull.0x2045f writes the same taskR14 through that pointer.0x20446 keepsR14 as the next tail. Thus this pending list uses task+0x50 asnext, not a copied task. |
| Exact list and queue passed |0x206cb suppliesRSI=&local_head[rbp-0x40];0x206cf suppliesRDI=F+0x130;0x206d6 calls0x1dc40. |
| Queue insertion preserves pointer |0x1dc51 loadsR12=[input_head].0x1dcec reads task+0x50 asnext and advances the input head;0x1dcfd clears task+0x50.0x1dd0f testsQ+0x108. Forflag!=1,0x1ddfe loadsQ+8's pointer-array and0x1de02 writes its new entry=R12. Subsequent heap operations reorder pointers. These operations write task+0x50/+0xa5 and array slots; they do not replace resource+0x68. |
| Worker reaches this queue | Worker0x1cb60 formsR13=F+0x130 at0x1cba1. If0x1dec0's lookahead returnsnull andF+0x238!=1,0x1cc3d checksF+0x140 (=Q+0x10 count),0x1cc4a loadsF+0x138 (=Q+8 array),0x1cc51 loads its first task pointer. For a selected task whosestate+0xa0!=3,0x1cbe6 calls0x1d200 withRDI=Q. |
| Dequeue returns original pointer |0x1d200 first calls0x1dec0. The relevant branch is whenQ+0x20 has no ready lookahead entry;0x1e498 returnsnull. WithQ+0x108!=1 andQ+0x10 nonzero,0x1d250 loadsQ+8,0x1d258 loadsarray[0] intoRAX, and0x1d269 retains it inR15 before pointer-array compaction/reordering. A single-entry path returns the still-preservedRAX; a short multi-entry path restoresRAX=R15 at0x1d37a, and the larger heap path retainsRAX=R15 from0x1d29b.0x1d3c2 updates onlytask+0xa5 to2 before return. |
| Worker consumes preserved resource |0x1cbf2 saves dequeuedRAX inR14. Along the noncancelled read branch,0x1cdf3 loadsresource=[R14+0x68],0x1ce06 loads handle=[resource], and0x1ce24 invokeshandle+0x28 withdestination=task+0x60,length=task+0x88, followingoffset operation+0x18. This is the same task field written at0x2041f. |

The queue-mode branch above is genuinely closed by static pointer flow. Claims about other mode branches, concurrent scheduling, successful I/O, or a selected live direct/pooled handle remain outside this receipt. No synthetic event trace is treated as execution.

## Replay

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/handoff-audit/verify_evidence.py
```

Fresh verification:10 bounded window hashes/decodes,41 criticalinstructions and2 actualPT_DYNAMIC relative relocations PASS. `verification.json` gives exact bytes and window/one-based line for each critical instruction. `receipt.json` gives SHA,PT_LOAD mapping,window byte hashes and hashes of prior read-only receipts. No builds,compilers/install,console/network,SDK research,runtime staging or original-file writes occurred. All new outputs are confined tothis folder.
