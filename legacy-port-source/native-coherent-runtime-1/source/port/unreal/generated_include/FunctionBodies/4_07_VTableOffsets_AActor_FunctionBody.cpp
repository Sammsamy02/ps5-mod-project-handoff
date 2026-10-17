if (auto it = AActor::VTableLayoutMap.find(STR("__vecDelDtor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasNetOwner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasNetOwner"), 0x1D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasNetOwner_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasNetOwner_C"), 0x1D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_AttachmentReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_AttachmentReplication"), 0x1D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReplicateSubobjects")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReplicateSubobjects"), 0x1E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReplicateSubobjects__Ptr_UActorChannel__Ptr_FOutBunch__Ptr_FReplicationFlags")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReplicateSubobjects__Ptr_UActorChannel__Ptr_FOutBunch__Ptr_FReplicationFlags"), 0x1E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectCreatedFromReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectCreatedFromReplication"), 0x1E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectCreatedFromReplication__Ptr_UObject")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectCreatedFromReplication__Ptr_UObject"), 0x1E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectDestroyFromReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectDestroyFromReplication"), 0x1F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectDestroyFromReplication__Ptr_UObject")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectDestroyFromReplication__Ptr_UObject"), 0x1F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplication"), 0x1F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplication__Ref_IRepChangedPropertyTracker")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplication__Ref_IRepChangedPropertyTracker"), 0x1F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_Instigator")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_Instigator"), 0x200);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EnableInput")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EnableInput"), 0x208);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EnableInput__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EnableInput__Ptr_APlayerController"), 0x208);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisableInput")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisableInput"), 0x210);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisableInput__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisableInput__Ptr_APlayerController"), 0x210);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetVelocity")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetVelocity"), 0x218);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetVelocity_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetVelocity_C"), 0x218);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorHiddenInGame")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorHiddenInGame"), 0x220);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorHiddenInGame__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorHiddenInGame__bool"), 0x220);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_DestroyActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_DestroyActor"), 0x228);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteActor"), 0x230);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteActor__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteActor__Ptr_AActor"), 0x230);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteComponent"), 0x238);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteComponent__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteComponent__Ptr_UActorComponent"), 0x238);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteActor"), 0x240);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteActor__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteActor__Ptr_AActor"), 0x240);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteComponent"), 0x248);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteComponent__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteComponent__Ptr_UActorComponent"), 0x248);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveBeginPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveBeginPlay"), 0x250);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BeginPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BeginPlay"), 0x258);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveAnyDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveAnyDamage"), 0x260);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveAnyDamage__float__Ptr_C_UDamageType__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveAnyDamage__float__Ptr_C_UDamageType__Ptr_AController__Ptr_AActor"), 0x260);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveRadialDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveRadialDamage"), 0x268);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveRadialDamage__float__Ptr_C_UDamageType__FVector__Ref_C_FHitResult__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveRadialDamage__float__Ptr_C_UDamageType__FVector__Ref_C_FHitResult__Ptr_AController__Ptr_AActor"), 0x268);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceivePointDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceivePointDamage"), 0x270);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceivePointDamage__float__Ptr_C_UDamageType__FVector__FVector__Ptr_UPrimitiveComponent__FName__FVector__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceivePointDamage__float__Ptr_C_UDamageType__FVector__FVector__Ptr_UPrimitiveComponent__FName__FVector__Ptr_AController__Ptr_AActor"), 0x270);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveTick")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveTick"), 0x278);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveTick__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveTick__float"), 0x278);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorBeginOverlap")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorBeginOverlap"), 0x280);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorBeginOverlap__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorBeginOverlap__Ptr_AActor"), 0x280);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorEndOverlap")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorEndOverlap"), 0x288);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorEndOverlap__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorEndOverlap__Ptr_AActor"), 0x288);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorBeginCursorOver")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorBeginCursorOver"), 0x290);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorEndCursorOver")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorEndCursorOver"), 0x298);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnClicked")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnClicked"), 0x2A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnReleased")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnReleased"), 0x2A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchBegin")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchBegin"), 0x2B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchBegin__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchBegin__C_ETouchIndex_Type"), 0x2B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchEnd")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchEnd"), 0x2B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchEnd__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchEnd__C_ETouchIndex_Type"), 0x2B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchEnter")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchEnter"), 0x2C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchEnter__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchEnter__C_ETouchIndex_Type"), 0x2C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchLeave")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchLeave"), 0x2C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveActorOnInputTouchLeave__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveActorOnInputTouchLeave__C_ETouchIndex_Type"), 0x2C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveInput")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveInput"), 0x2D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveInput__Ref_C_FString__float__FVector__bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveInput__Ref_C_FString__float__FVector__bool__bool"), 0x2D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveHit")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveHit"), 0x2D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveHit__Ptr_UPrimitiveComponent__Ptr_AActor__Ptr_UPrimitiveComponent__bool__FVector__FVector__FVector__Ref_C_FHitResult")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveHit__Ptr_UPrimitiveComponent__Ptr_AActor__Ptr_UPrimitiveComponent__bool__FVector__FVector__FVector__Ref_C_FHitResult"), 0x2D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetLifeSpan")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetLifeSpan"), 0x2E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetLifeSpan__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetLifeSpan__float"), 0x2E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLifeSpan")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLifeSpan"), 0x2E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLifeSpan_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLifeSpan_C"), 0x2E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UserConstructionScript")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UserConstructionScript"), 0x2F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveDestroyed")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveDestroyed"), 0x2F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveEndPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveEndPlay"), 0x300);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReceiveEndPlay__EEndPlayReason_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReceiveEndPlay__EEndPlayReason_Type"), 0x300);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GatherCurrentMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GatherCurrentMovement"), 0x308);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ApplyWorldOffset")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ApplyWorldOffset"), 0x310);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ApplyWorldOffset__Ref_C_FVector__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ApplyWorldOffset__Ref_C_FVector__bool"), 0x310);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsLevelBoundsRelevant")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsLevelBoundsRelevant"), 0x318);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsLevelBoundsRelevant_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsLevelBoundsRelevant_C"), 0x318);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetPriority")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetPriority"), 0x320);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetPriority__Ref_C_FVector__Ref_C_FVector__Ptr_APlayerController__Ptr_UActorChannel__float__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetPriority__Ref_C_FVector__Ref_C_FVector__Ptr_APlayerController__Ptr_UActorChannel__float__bool"), 0x320);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetDormancy")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetDormancy"), 0x328);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetDormancy__Ref_C_FVector__Ref_C_FVector__Ptr_APlayerController__Ptr_UActorChannel__float__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetDormancy__Ref_C_FVector__Ref_C_FVector__Ptr_APlayerController__Ptr_UActorChannel__float__bool"), 0x328);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnActorChannelOpen")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnActorChannelOpen"), 0x330);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnActorChannelOpen__Ref_FInBunch__Ptr_UNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnActorChannelOpen__Ref_FInBunch__Ptr_UNetConnection"), 0x330);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSerializeNewActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSerializeNewActor"), 0x338);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSerializeNewActor__Ref_FOutBunch")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSerializeNewActor__Ref_FOutBunch"), 0x338);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnNetCleanup")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnNetCleanup"), 0x340);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnNetCleanup__Ptr_UNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnNetCleanup__Ptr_UNetConnection"), 0x340);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TickActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TickActor"), 0x348);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TickActor__float__ELevelTick__Ref_FActorTickFunction")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TickActor__float__ELevelTick__Ref_FActorTickFunction"), 0x348);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostActorCreated")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostActorCreated"), 0x350);
}

if (auto it = AActor::VTableLayoutMap.find(STR("LifeSpanExpired")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("LifeSpanExpired"), 0x358);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetInit")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetInit"), 0x360);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_ReplicatedMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_ReplicatedMovement"), 0x368);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveLocation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveLocation"), 0x370);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveLocationAndRotation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveLocationAndRotation"), 0x378);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveVelocity")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveVelocity"), 0x380);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveVelocity__Ref_C_FVector")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveVelocity__Ref_C_FVector"), 0x380);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceivePhysicState")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceivePhysicState"), 0x388);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CheckStillInWorld")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CheckStillInWorld"), 0x390);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Tick")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Tick"), 0x398);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Tick__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Tick__float"), 0x398);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ShouldTickIfViewportsOnly")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ShouldTickIfViewportsOnly"), 0x3A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ShouldTickIfViewportsOnly_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ShouldTickIfViewportsOnly_C"), 0x3A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsNetRelevantFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsNetRelevantFor"), 0x3A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsNetRelevantFor_C__Ptr_C_APlayerController__Ptr_C_AActor__Ref_C_FVector")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsNetRelevantFor_C__Ptr_C_APlayerController__Ptr_C_AActor__Ref_C_FVector"), 0x3A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRelevancyOwnerFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRelevancyOwnerFor"), 0x3B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRelevancyOwnerFor__Ptr_AActor__Ptr_AActor__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRelevancyOwnerFor__Ptr_AActor__Ptr_AActor__Ptr_AActor"), 0x3B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreInitializeComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreInitializeComponents"), 0x3B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostInitializeComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostInitializeComponents"), 0x3C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetOwningPlayer")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetOwningPlayer"), 0x3C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetConnection"), 0x3D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterAllComponents"), 0x3D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRegisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRegisterAllComponents"), 0x3E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UnregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UnregisterAllComponents"), 0x3E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostUnregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostUnregisterAllComponents"), 0x3F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReregisterAllComponents"), 0x3F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("MarkComponentsAsPendingKill")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("MarkComponentsAsPendingKill"), 0x400);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InvalidateLightingCacheDetailed")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InvalidateLightingCacheDetailed"), 0x408);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InvalidateLightingCacheDetailed__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InvalidateLightingCacheDetailed__bool"), 0x408);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportTo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportTo"), 0x410);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportTo__Ref_C_FVector__Ref_C_FRotator__bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportTo__Ref_C_FVector__Ref_C_FRotator__bool__bool"), 0x410);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportSucceeded")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportSucceeded"), 0x418);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportSucceeded__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportSucceeded__bool"), 0x418);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ClearCrossLevelReferences")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ClearCrossLevelReferences"), 0x420);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndPlay"), 0x428);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndPlay__C_EEndPlayReason_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndPlay__C_EEndPlayReason_Type"), 0x428);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsBasedOnActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsBasedOnActor"), 0x430);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsBasedOnActor_C__Ptr_C_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsBasedOnActor_C__Ptr_C_AActor"), 0x430);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsAttachedTo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsAttachedTo"), 0x438);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsAttachedTo_C__Ptr_C_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsAttachedTo_C__Ptr_C_AActor"), 0x438);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RerunConstructionScripts")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RerunConstructionScripts"), 0x440);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnConstruction")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnConstruction"), 0x448);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnConstruction__Ref_C_FTransform")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnConstruction__Ref_C_FTransform"), 0x448);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterActorTickFunctions")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterActorTickFunctions"), 0x450);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterActorTickFunctions__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterActorTickFunctions__bool"), 0x450);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachParentActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachParentActor"), 0x458);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachParentActor_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachParentActor_C"), 0x458);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachParentSocketName")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachParentSocketName"), 0x460);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachParentSocketName_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachParentSocketName_C"), 0x460);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachedActors")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachedActors"), 0x468);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetAttachedActors_C__Ref_TArray_AActor_*_FDefaultAllocator_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetAttachedActors_C__Ref_TArray_AActor_*_FDefaultAllocator_"), 0x468);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Destroyed")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Destroyed"), 0x470);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FellOutOfWorld")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FellOutOfWorld"), 0x478);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FellOutOfWorld__Ref_C_UDamageType")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FellOutOfWorld__Ref_C_UDamageType"), 0x478);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OutsideWorldBounds")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OutsideWorldBounds"), 0x480);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingBox")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingBox"), 0x488);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingBox_C__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingBox_C__bool"), 0x488);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingCylinder")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingCylinder"), 0x490);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingCylinder_C__Ref_float__Ref_float__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingCylinder_C__Ref_float__Ref_float__bool"), 0x490);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSimpleCollisionCylinder")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSimpleCollisionCylinder"), 0x498);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSimpleCollisionCylinder_C__Ref_float__Ref_float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSimpleCollisionCylinder_C__Ref_float__Ref_float"), 0x498);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRootComponentCollisionRegistered")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRootComponentCollisionRegistered"), 0x4A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRootComponentCollisionRegistered_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRootComponentCollisionRegistered_C"), 0x4A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TornOff")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TornOff"), 0x4A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsCollisionResponseToChannel")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsCollisionResponseToChannel"), 0x4B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsCollisionResponseToChannel_C__ECollisionChannel")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsCollisionResponseToChannel_C__ECollisionChannel"), 0x4B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CanBeBaseForCharacter")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CanBeBaseForCharacter"), 0x4B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CanBeBaseForCharacter_C__Ptr_APawn")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CanBeBaseForCharacter_C__Ptr_APawn"), 0x4B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TakeDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TakeDamage"), 0x4C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TakeDamage__float__Ref_C_FDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TakeDamage__float__Ref_C_FDamageEvent__Ptr_AController__Ptr_AActor"), 0x4C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakeRadialDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakeRadialDamage"), 0x4C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakeRadialDamage__float__Ref_C_FRadialDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakeRadialDamage__float__Ref_C_FRadialDamageEvent__Ptr_AController__Ptr_AActor"), 0x4C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakePointDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakePointDamage"), 0x4D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakePointDamage__float__Ref_C_FPointDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakePointDamage__float__Ref_C_FPointDamageEvent__Ptr_AController__Ptr_AActor"), 0x4D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BecomeViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BecomeViewTarget"), 0x4D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BecomeViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BecomeViewTarget__Ptr_APlayerController"), 0x4D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndViewTarget"), 0x4E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndViewTarget__Ptr_APlayerController"), 0x4E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_OnBecomeViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_OnBecomeViewTarget"), 0x4E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_OnBecomeViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_OnBecomeViewTarget__Ptr_APlayerController"), 0x4E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_OnEndViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_OnEndViewTarget"), 0x4F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_OnEndViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_OnEndViewTarget__Ptr_APlayerController"), 0x4F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalcCamera")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalcCamera"), 0x4F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalcCamera__float__Ref_FMinimalViewInfo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalcCamera__float__Ref_FMinimalViewInfo"), 0x4F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHumanReadableName")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHumanReadableName"), 0x500);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHumanReadableName_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHumanReadableName_C"), 0x500);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Reset")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Reset"), 0x508);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLastRenderTime")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLastRenderTime"), 0x510);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLastRenderTime_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLastRenderTime_C"), 0x510);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ForceNetRelevant")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ForceNetRelevant"), 0x518);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ForceNetUpdate")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ForceNetUpdate"), 0x520);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PrestreamTextures")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PrestreamTextures"), 0x528);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PrestreamTextures__float__bool__int32")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PrestreamTextures__float__bool__int32"), 0x528);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorEyesViewPoint")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorEyesViewPoint"), 0x530);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorEyesViewPoint_C__Ref_FVector__Ref_FRotator")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorEyesViewPoint_C__Ref_FVector__Ref_FRotator"), 0x530);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetTargetLocation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetTargetLocation"), 0x538);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetTargetLocation_C__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetTargetLocation_C__Ptr_AActor"), 0x538);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRenderFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRenderFor"), 0x540);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRenderFor__Ptr_APlayerController__Ptr_UCanvas__FVector__FVector")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRenderFor__Ptr_APlayerController__Ptr_UCanvas__FVector__FVector"), 0x540);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByClass")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByClass"), 0x548);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByClass_C__C_TSubclassOf_UActorComponent_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByClass_C__C_TSubclassOf_UActorComponent_"), 0x548);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentByClass")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentByClass"), 0x550);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentByClass__TSubclassOf_UActorComponent_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentByClass__TSubclassOf_UActorComponent_"), 0x550);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsComponentRelevantForNavigation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsComponentRelevantForNavigation"), 0x558);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsComponentRelevantForNavigation_C__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsComponentRelevantForNavigation_C__Ptr_UActorComponent"), 0x558);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisplayDebug")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisplayDebug"), 0x560);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisplayDebug__Ptr_UCanvas__Ref_C_FDebugDisplayInfo__Ref_float__Ref_float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisplayDebug__Ptr_UCanvas__Ref_C_FDebugDisplayInfo__Ref_float__Ref_float"), 0x560);
}

