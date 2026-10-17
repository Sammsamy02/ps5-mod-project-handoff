# l19 reflection static supplement

LOCAL ONLY, same authenticated image SHA3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc. Parent requested a short extension into existing reflection/class code after the base report. Small functions next to the confirmed Invoke implementation and its base serializer provided direct evidence without new tooling or any runtime contact. `REFLECTION-FACTS.json` has per-fact byte/address/inference records; `make_reflection_report.py` verifies all citedbytes.

Nativeobjectstorage is explicitly established aschunked with65536items perchunk bybothallocate/free indexsplits (high16chunk,low16item), plus chunkpayload65536*24B andarrayheadercount65536. This is a targetaccess-pathfact; it doesnotidentify aglobalpackingmacro or addfreshlivevalidation.

UFunctionSerialize326b50 and PostLoad326ec0 close the prior FunctionFlagsB0/width4 andNumParmsB4/width1 unknowns. Serialize writes a full dword atB0; rebuilding parameter metadata incrementsbyteB4, calculatesElementSize*ArrayDim+Offset andstoreswordB6, andCPF_ReturnParm selectswordB8. FirstPropertyToInitC0 is a qword head rebuiltforinitialization throughproperty+60. These supersede only the corresponding partial/unresolved entries inbase `STATIC-FACTS.json`; the base itself remains preserved.

UStructSerialize31cbf0 establishesSuperStruct40 by repeatedparenthops,Children48 byUFieldNext28 walks/reconstruction, and callspropertySerialize31d4f0. That helper counts, serializes and rebuildsChildProperties50 usingFFieldNext18. ProcessEvent suppliesPropertiesSize58/width4,MinAlignment5C/width2,Script60(dataqword/Numdword68; fullcontainerwidthunresolved), andDestructorLink80.

The property traversal confirmsFFieldClassPrivate8 andNext18. FField helper364b80 readsOwner10: bit0set meansUObject,clearedbyAND~1; unset meansFField andfollowsfieldOwner10. UStructpropertyconstruction31d632 explicitlyformsownerUStructpointer|1. Variant-namehelper364310 usesUObjectName18 fortagset andFFieldName20 fortagclear. FFieldSerialize364710 independentlyserializesName20 andFlags28/width4. This establishes effective8B lowtagvariantencoding for the observed target path.

**FProperty candidatesArrayDim30/ElementSize34 are refuted.** Target hasArrayDim2C/ElementSize30: parameterextent multiplication326cd8/326cdc andProcessEventdestructorzeroing459bf0/459bf4 independently identifyboth. PropertyFlags38/width8,Offset_Internal44/width4,DestructorLinkNext58/width8 andPostConstructLinkNext60/width8 are also directly recovered. ArrayDim2C can reuseFField basepadding; it doesnot establishFFieldsizeof2C.

Complete UStruct/FField/FProperty sizeof values were not uniquely recovered by this bounded pass. Candidate totalsB0/30/70 are preserved asunresolved. GeneralPropertyLink70/RefLink78/PostConstructLink88 heads and FPropertyPropertyLinkNext48/NextRef50/RepIndex40/RepNotifyFunc68 likewise remain unresolved; no Windowscandidate becomesfact because adjacentmembersmatch.

No gameclass-registrationtotal record was independentlyidentified; attempting widerregistrationdecodingwouldstartanotherinvestigation. Existing lowerbounds are reported separately and cannot bepublished ascompleteclasssize. This report addsstatic targetfacts, not livebinding/menu/spawn evidence.
