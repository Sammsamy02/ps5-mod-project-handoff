if (auto it = AActor::VTableLayoutMap.find(STR("__vecDelDtor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_ReplicateMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_ReplicateMovement"), 0x2B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TearOff")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TearOff"), 0x2B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasNetOwner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasNetOwner"), 0x2C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasNetOwner_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasNetOwner_C"), 0x2C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasLocalNetOwner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasLocalNetOwner"), 0x2C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasLocalNetOwner_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasLocalNetOwner_C"), 0x2C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_Owner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_Owner"), 0x2D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetReplicateMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetReplicateMovement"), 0x2D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetReplicateMovement__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetReplicateMovement__bool"), 0x2D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_AttachmentReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_AttachmentReplication"), 0x2E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsReplicationPausedForConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsReplicationPausedForConnection"), 0x2E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsReplicationPausedForConnection__Ref_C_FNetViewer")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsReplicationPausedForConnection__Ref_C_FNetViewer"), 0x2E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnReplicationPausedChanged")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnReplicationPausedChanged"), 0x2F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnReplicationPausedChanged__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnReplicationPausedChanged__bool"), 0x2F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReplicateSubobjects")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReplicateSubobjects"), 0x2F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReplicateSubobjects__Ptr_UActorChannel__Ptr_FOutBunch__Ptr_FReplicationFlags")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReplicateSubobjects__Ptr_UActorChannel__Ptr_FOutBunch__Ptr_FReplicationFlags"), 0x2F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectCreatedFromReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectCreatedFromReplication"), 0x300);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectCreatedFromReplication__Ptr_UObject")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectCreatedFromReplication__Ptr_UObject"), 0x300);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectDestroyFromReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectDestroyFromReplication"), 0x308);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSubobjectDestroyFromReplication__Ptr_UObject")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSubobjectDestroyFromReplication__Ptr_UObject"), 0x308);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplication"), 0x310);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplication__Ref_IRepChangedPropertyTracker")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplication__Ref_IRepChangedPropertyTracker"), 0x310);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplicationForReplay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplicationForReplay"), 0x318);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreReplicationForReplay__Ref_IRepChangedPropertyTracker")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreReplicationForReplay__Ref_IRepChangedPropertyTracker"), 0x318);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RewindForReplay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RewindForReplay"), 0x320);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_Instigator")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_Instigator"), 0x328);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EnableInput")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EnableInput"), 0x330);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EnableInput__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EnableInput__Ptr_APlayerController"), 0x330);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CreateInputComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CreateInputComponent"), 0x338);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CreateInputComponent__TSubclassOf_UInputComponent_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CreateInputComponent__TSubclassOf_UInputComponent_"), 0x338);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisableInput")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisableInput"), 0x340);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisableInput__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisableInput__Ptr_APlayerController"), 0x340);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsIncrementalPreRegisterComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsIncrementalPreRegisterComponents"), 0x348);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsIncrementalPreRegisterComponents_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsIncrementalPreRegisterComponents_C"), 0x348);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsIncrementalPreUnregisterComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsIncrementalPreUnregisterComponents"), 0x350);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsIncrementalPreUnregisterComponents_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsIncrementalPreUnregisterComponents_C"), 0x350);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorBounds")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorBounds"), 0x358);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorBounds_C__bool__Ref_UE_Math_TVector_double___Ref_UE_Math_TVector_double___bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorBounds_C__bool__Ref_UE_Math_TVector_double___Ref_UE_Math_TVector_double___bool"), 0x358);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetVelocity")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetVelocity"), 0x360);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetVelocity_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetVelocity_C"), 0x360);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorHiddenInGame")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorHiddenInGame"), 0x368);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorHiddenInGame__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorHiddenInGame__bool"), 0x368);
}

if (auto it = AActor::VTableLayoutMap.find(STR("K2_DestroyActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("K2_DestroyActor"), 0x370);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteActor"), 0x378);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteActor__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteActor__Ptr_AActor"), 0x378);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteComponent"), 0x380);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AddTickPrerequisiteComponent__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AddTickPrerequisiteComponent__Ptr_UActorComponent"), 0x380);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteActor"), 0x388);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteActor__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteActor__Ptr_AActor"), 0x388);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteComponent"), 0x390);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RemoveTickPrerequisiteComponent__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RemoveTickPrerequisiteComponent__Ptr_UActorComponent"), 0x390);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BeginPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BeginPlay"), 0x398);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndPlay")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndPlay"), 0x3A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndPlay__C_EEndPlayReason_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndPlay__C_EEndPlayReason_Type"), 0x3A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorBeginOverlap")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorBeginOverlap"), 0x3A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorBeginOverlap__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorBeginOverlap__Ptr_AActor"), 0x3A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorEndOverlap")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorEndOverlap"), 0x3B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorEndOverlap__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorEndOverlap__Ptr_AActor"), 0x3B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorBeginCursorOver")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorBeginCursorOver"), 0x3B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorEndCursorOver")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorEndCursorOver"), 0x3C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnClicked")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnClicked"), 0x3C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnClicked__FKey")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnClicked__FKey"), 0x3C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnReleased")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnReleased"), 0x3D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnReleased__FKey")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnReleased__FKey"), 0x3D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchBegin")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchBegin"), 0x3D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchBegin__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchBegin__C_ETouchIndex_Type"), 0x3D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchEnd")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchEnd"), 0x3E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchEnd__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchEnd__C_ETouchIndex_Type"), 0x3E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchEnter")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchEnter"), 0x3E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchEnter__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchEnter__C_ETouchIndex_Type"), 0x3E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchLeave")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchLeave"), 0x3F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyActorOnInputTouchLeave__C_ETouchIndex_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyActorOnInputTouchLeave__C_ETouchIndex_Type"), 0x3F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyHit")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyHit"), 0x3F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("NotifyHit__Ptr_UPrimitiveComponent__Ptr_AActor__Ptr_UPrimitiveComponent__bool__UE_Math_TVector_double___UE_Math_TVector_double___UE_Math_TVector_double___Ref_C_FHitResult")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("NotifyHit__Ptr_UPrimitiveComponent__Ptr_AActor__Ptr_UPrimitiveComponent__bool__UE_Math_TVector_double___UE_Math_TVector_double___UE_Math_TVector_double___Ref_C_FHitResult"), 0x3F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetLifeSpan")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetLifeSpan"), 0x400);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetLifeSpan__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetLifeSpan__float"), 0x400);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLifeSpan")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLifeSpan"), 0x408);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLifeSpan_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLifeSpan_C"), 0x408);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsEditorOnlyLoadedInPIE")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsEditorOnlyLoadedInPIE"), 0x410);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsEditorOnlyLoadedInPIE_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsEditorOnlyLoadedInPIE_C"), 0x410);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRuntimeOnly")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRuntimeOnly"), 0x418);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRuntimeOnly_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRuntimeOnly_C"), 0x418);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreDuplicateFromRoot")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreDuplicateFromRoot"), 0x420);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreDuplicateFromRoot__Ref_FObjectDuplicationParameters")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreDuplicateFromRoot__Ref_FObjectDuplicationParameters"), 0x420);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreSaveFromRoot")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreSaveFromRoot"), 0x428);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreSaveFromRoot__FObjectPreSaveRootContext")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreSaveFromRoot__FObjectPreSaveRootContext"), 0x428);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostSaveFromRoot")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostSaveFromRoot"), 0x430);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostSaveFromRoot__FObjectPostSaveRootContext")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostSaveFromRoot__FObjectPostSaveRootContext"), 0x430);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GatherCurrentMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GatherCurrentMovement"), 0x438);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetDefaultAttachComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetDefaultAttachComponent"), 0x440);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetDefaultAttachComponent_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetDefaultAttachComponent_C"), 0x440);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ApplyWorldOffset")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ApplyWorldOffset"), 0x448);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ApplyWorldOffset__Ref_C_UE_Math_TVector_double___bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ApplyWorldOffset__Ref_C_UE_Math_TVector_double___bool"), 0x448);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsLevelBoundsRelevant")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsLevelBoundsRelevant"), 0x450);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsLevelBoundsRelevant_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsLevelBoundsRelevant_C"), 0x450);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsHLODRelevant")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsHLODRelevant"), 0x458);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsHLODRelevant_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsHLODRelevant_C"), 0x458);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasHLODRelevantComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasHLODRelevantComponents"), 0x460);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasHLODRelevantComponents_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasHLODRelevantComponents_C"), 0x460);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHLODRelevantComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHLODRelevantComponents"), 0x468);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHLODRelevantComponents_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHLODRelevantComponents_C"), 0x468);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetPriority")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetPriority"), 0x470);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetPriority__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__Ptr_UActorChannel__float__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetPriority__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__Ptr_UActorChannel__float__bool"), 0x470);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetReplayPriority")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetReplayPriority"), 0x478);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetReplayPriority__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__UActorChannel*_const__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetReplayPriority__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__UActorChannel*_const__float"), 0x478);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetDormancy")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetDormancy"), 0x480);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetDormancy__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__Ptr_UActorChannel__float__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetDormancy__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TVector_double___Ptr_AActor__Ptr_AActor__Ptr_UActorChannel__float__bool"), 0x480);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnActorChannelOpen")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnActorChannelOpen"), 0x488);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnActorChannelOpen__Ref_FInBunch__Ptr_UNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnActorChannelOpen__Ref_FInBunch__Ptr_UNetConnection"), 0x488);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UseShortConnectTimeout")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UseShortConnectTimeout"), 0x490);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UseShortConnectTimeout_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UseShortConnectTimeout_C"), 0x490);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSerializeNewActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSerializeNewActor"), 0x498);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnSerializeNewActor__Ref_FOutBunch")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnSerializeNewActor__Ref_FOutBunch"), 0x498);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnNetCleanup")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnNetCleanup"), 0x4A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnNetCleanup__Ptr_UNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnNetCleanup__Ptr_UNetConnection"), 0x4A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorTickEnabled")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorTickEnabled"), 0x4A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetActorTickEnabled__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetActorTickEnabled__bool"), 0x4A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TickActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TickActor"), 0x4B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TickActor__float__ELevelTick__Ref_FActorTickFunction")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TickActor__float__ELevelTick__Ref_FActorTickFunction"), 0x4B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AsyncPhysicsTickActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AsyncPhysicsTickActor"), 0x4B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AsyncPhysicsTickActor__float__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AsyncPhysicsTickActor__float__float"), 0x4B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostActorCreated")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostActorCreated"), 0x4C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("LifeSpanExpired")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("LifeSpanExpired"), 0x4C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveRole")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveRole"), 0x4D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetInit")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetInit"), 0x4D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnRep_ReplicatedMovement")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnRep_ReplicatedMovement"), 0x4E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveLocationAndRotation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveLocationAndRotation"), 0x4E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveVelocity")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveVelocity"), 0x4F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceiveVelocity__Ref_C_UE_Math_TVector_double_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceiveVelocity__Ref_C_UE_Math_TVector_double_"), 0x4F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostNetReceivePhysicState")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostNetReceivePhysicState"), 0x4F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetOwner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetOwner"), 0x500);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SetOwner__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SetOwner__Ptr_AActor"), 0x500);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CheckStillInWorld")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CheckStillInWorld"), 0x508);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetPhysicsVolume")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetPhysicsVolume"), 0x510);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetPhysicsVolume_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetPhysicsVolume_C"), 0x510);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Tick")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Tick"), 0x518);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Tick__float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Tick__float"), 0x518);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ShouldTickIfViewportsOnly")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ShouldTickIfViewportsOnly"), 0x520);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ShouldTickIfViewportsOnly_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ShouldTickIfViewportsOnly_C"), 0x520);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsNetRelevantFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsNetRelevantFor"), 0x528);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsNetRelevantFor_C__Ptr_C_AActor__Ptr_C_AActor__Ref_C_UE_Math_TVector_double_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsNetRelevantFor_C__Ptr_C_AActor__Ptr_C_AActor__Ref_C_UE_Math_TVector_double_"), 0x528);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsReplayRelevantFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsReplayRelevantFor"), 0x530);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsReplayRelevantFor_C__Ptr_C_AActor__Ptr_C_AActor__Ref_C_UE_Math_TVector_double___C_float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsReplayRelevantFor_C__Ptr_C_AActor__Ptr_C_AActor__Ref_C_UE_Math_TVector_double___C_float"), 0x530);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRelevancyOwnerFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRelevancyOwnerFor"), 0x538);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRelevancyOwnerFor_C__Ptr_C_AActor__Ptr_C_AActor__Ptr_C_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRelevancyOwnerFor_C__Ptr_C_AActor__Ptr_C_AActor__Ptr_C_AActor"), 0x538);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreInitializeComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreInitializeComponents"), 0x540);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostInitializeComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostInitializeComponents"), 0x548);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DispatchPhysicsCollisionHit")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DispatchPhysicsCollisionHit"), 0x550);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DispatchPhysicsCollisionHit__Ref_C_FRigidBodyCollisionInfo__Ref_C_FRigidBodyCollisionInfo__Ref_C_FCollisionImpactData")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DispatchPhysicsCollisionHit__Ref_C_FRigidBodyCollisionInfo__Ref_C_FRigidBodyCollisionInfo__Ref_C_FCollisionImpactData"), 0x550);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetOwner")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetOwner"), 0x558);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetOwner_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetOwner_C"), 0x558);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetOwningPlayer")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetOwningPlayer"), 0x560);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetOwningPlayerAnyRole")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetOwningPlayerAnyRole"), 0x568);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetConnection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetConnection"), 0x570);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetNetConnection_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetNetConnection_C"), 0x570);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DestroyNetworkActorHandled")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DestroyNetworkActorHandled"), 0x578);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsSelectionParentOfAttachedActors")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsSelectionParentOfAttachedActors"), 0x580);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsSelectionParentOfAttachedActors_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsSelectionParentOfAttachedActors_C"), 0x580);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsSelectionChild")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsSelectionChild"), 0x588);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsSelectionChild_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsSelectionChild_C"), 0x588);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSelectionParent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSelectionParent"), 0x590);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSelectionParent_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSelectionParent_C"), 0x590);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetRootSelectionParent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetRootSelectionParent"), 0x598);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetRootSelectionParent_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetRootSelectionParent_C"), 0x598);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsSubRootSelection")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsSubRootSelection"), 0x5A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("SupportsSubRootSelection_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("SupportsSubRootSelection_C"), 0x5A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PushSelectionToProxies")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PushSelectionToProxies"), 0x5A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterAllComponents"), 0x5B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PreRegisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PreRegisterAllComponents"), 0x5B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRegisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRegisterAllComponents"), 0x5C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UnregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UnregisterAllComponents"), 0x5C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("UnregisterAllComponents__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("UnregisterAllComponents__bool"), 0x5C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostUnregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostUnregisterAllComponents"), 0x5D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ReregisterAllComponents")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ReregisterAllComponents"), 0x5D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("MarkComponentsAsPendingKill")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("MarkComponentsAsPendingKill"), 0x5E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("MarkComponentsAsGarbage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("MarkComponentsAsGarbage"), 0x5E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("MarkComponentsAsGarbage__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("MarkComponentsAsGarbage__bool"), 0x5E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InvalidateLightingCacheDetailed")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InvalidateLightingCacheDetailed"), 0x5F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InvalidateLightingCacheDetailed__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InvalidateLightingCacheDetailed__bool"), 0x5F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportTo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportTo"), 0x5F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportTo__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TRotator_double___bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportTo__Ref_C_UE_Math_TVector_double___Ref_C_UE_Math_TRotator_double___bool__bool"), 0x5F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportSucceeded")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportSucceeded"), 0x600);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TeleportSucceeded__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TeleportSucceeded__bool"), 0x600);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ClearCrossLevelReferences")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ClearCrossLevelReferences"), 0x608);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsBasedOnActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsBasedOnActor"), 0x610);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsBasedOnActor_C__Ptr_C_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsBasedOnActor_C__Ptr_C_AActor"), 0x610);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsAttachedTo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsAttachedTo"), 0x618);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsAttachedTo_C__Ptr_C_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsAttachedTo_C__Ptr_C_AActor"), 0x618);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnConstruction")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnConstruction"), 0x620);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnConstruction__Ref_C_UE_Math_TTransform_double_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnConstruction__Ref_C_UE_Math_TTransform_double_"), 0x620);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FillReplicationParams")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FillReplicationParams"), 0x628);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FillReplicationParams__Ref_C_AActor_FFillReplicationParamsContext__Ref_FActorReplicationParams")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FillReplicationParams__Ref_C_AActor_FFillReplicationParamsContext__Ref_FActorReplicationParams"), 0x628);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnReplicationStartedForIris")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnReplicationStartedForIris"), 0x630);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnReplicationStartedForIris__Ref_C_AActor_FOnReplicationStartedParams")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnReplicationStartedForIris__Ref_C_AActor_FOnReplicationStartedParams"), 0x630);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnStopReplicationForIris")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnStopReplicationForIris"), 0x638);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OnStopReplicationForIris__Ref_C_AActor_FOnStopReplicationParams")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OnStopReplicationForIris__Ref_C_AActor_FOnStopReplicationParams"), 0x638);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BeginReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BeginReplication"), 0x640);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndReplication")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndReplication"), 0x648);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndReplication__EEndPlayReason_Type")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndReplication__EEndPlayReason_Type"), 0x648);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterActorTickFunctions")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterActorTickFunctions"), 0x650);
}

if (auto it = AActor::VTableLayoutMap.find(STR("RegisterActorTickFunctions__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("RegisterActorTickFunctions__bool"), 0x650);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Destroyed")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Destroyed"), 0x658);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FellOutOfWorld")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FellOutOfWorld"), 0x660);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FellOutOfWorld__Ref_C_UDamageType")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FellOutOfWorld__Ref_C_UDamageType"), 0x660);
}

if (auto it = AActor::VTableLayoutMap.find(STR("OutsideWorldBounds")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("OutsideWorldBounds"), 0x668);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingBox")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingBox"), 0x670);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingBox_C__bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingBox_C__bool__bool"), 0x670);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalculateComponentsBoundingBoxInLocalSpace")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalculateComponentsBoundingBoxInLocalSpace"), 0x678);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalculateComponentsBoundingBoxInLocalSpace_C__bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalculateComponentsBoundingBoxInLocalSpace_C__bool__bool"), 0x678);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingCylinder")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingCylinder"), 0x680);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsBoundingCylinder_C__Ref_float__Ref_float__bool__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsBoundingCylinder_C__Ref_float__Ref_float__bool__bool"), 0x680);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSimpleCollisionCylinder")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSimpleCollisionCylinder"), 0x688);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetSimpleCollisionCylinder_C__Ref_float__Ref_float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetSimpleCollisionCylinder_C__Ref_float__Ref_float"), 0x688);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRootComponentCollisionRegistered")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRootComponentCollisionRegistered"), 0x690);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsRootComponentCollisionRegistered_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsRootComponentCollisionRegistered_C"), 0x690);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TornOff")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TornOff"), 0x698);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsCollisionResponseToChannel")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsCollisionResponseToChannel"), 0x6A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetComponentsCollisionResponseToChannel_C__ECollisionChannel")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetComponentsCollisionResponseToChannel_C__ECollisionChannel"), 0x6A0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CanBeBaseForCharacter")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CanBeBaseForCharacter"), 0x6A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CanBeBaseForCharacter_C__Ptr_APawn")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CanBeBaseForCharacter_C__Ptr_APawn"), 0x6A8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TakeDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TakeDamage"), 0x6B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("TakeDamage__float__Ref_C_FDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("TakeDamage__float__Ref_C_FDamageEvent__Ptr_AController__Ptr_AActor"), 0x6B0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakeRadialDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakeRadialDamage"), 0x6B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakeRadialDamage__float__Ref_C_FRadialDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakeRadialDamage__float__Ref_C_FRadialDamageEvent__Ptr_AController__Ptr_AActor"), 0x6B8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakePointDamage")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakePointDamage"), 0x6C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("InternalTakePointDamage__float__Ref_C_FPointDamageEvent__Ptr_AController__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("InternalTakePointDamage__float__Ref_C_FPointDamageEvent__Ptr_AController__Ptr_AActor"), 0x6C0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BecomeViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BecomeViewTarget"), 0x6C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("BecomeViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("BecomeViewTarget__Ptr_APlayerController"), 0x6C8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndViewTarget")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndViewTarget"), 0x6D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("EndViewTarget__Ptr_APlayerController")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("EndViewTarget__Ptr_APlayerController"), 0x6D0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalcCamera")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalcCamera"), 0x6D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("CalcCamera__float__Ref_FMinimalViewInfo")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("CalcCamera__float__Ref_FMinimalViewInfo"), 0x6D8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasActiveCameraComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasActiveCameraComponent"), 0x6E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasActiveCameraComponent_C__bool")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasActiveCameraComponent_C__bool"), 0x6E0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasActivePawnControlCameraComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasActivePawnControlCameraComponent"), 0x6E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("HasActivePawnControlCameraComponent_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("HasActivePawnControlCameraComponent_C"), 0x6E8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHumanReadableName")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHumanReadableName"), 0x6F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetHumanReadableName_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetHumanReadableName_C"), 0x6F0);
}

if (auto it = AActor::VTableLayoutMap.find(STR("Reset")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("Reset"), 0x6F8);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLastRenderTime")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLastRenderTime"), 0x700);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetLastRenderTime_C")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetLastRenderTime_C"), 0x700);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ForceNetRelevant")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ForceNetRelevant"), 0x708);
}

if (auto it = AActor::VTableLayoutMap.find(STR("ForceNetUpdate")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("ForceNetUpdate"), 0x710);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PrestreamTextures")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PrestreamTextures"), 0x718);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PrestreamTextures__float__bool__int32")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PrestreamTextures__float__bool__int32"), 0x718);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorEyesViewPoint")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorEyesViewPoint"), 0x720);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetActorEyesViewPoint_C__Ref_UE_Math_TVector_double___Ref_UE_Math_TRotator_double_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetActorEyesViewPoint_C__Ref_UE_Math_TVector_double___Ref_UE_Math_TRotator_double_"), 0x720);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetTargetLocation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetTargetLocation"), 0x728);
}

if (auto it = AActor::VTableLayoutMap.find(STR("GetTargetLocation_C__Ptr_AActor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("GetTargetLocation_C__Ptr_AActor"), 0x728);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRenderFor")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRenderFor"), 0x730);
}

if (auto it = AActor::VTableLayoutMap.find(STR("PostRenderFor__Ptr_APlayerController__Ptr_UCanvas__UE_Math_TVector_double___UE_Math_TVector_double_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("PostRenderFor__Ptr_APlayerController__Ptr_UCanvas__UE_Math_TVector_double___UE_Math_TVector_double_"), 0x730);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByClass")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByClass"), 0x738);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByClass_C__C_TSubclassOf_UActorComponent_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByClass_C__C_TSubclassOf_UActorComponent_"), 0x738);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByInterface")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByInterface"), 0x740);
}

if (auto it = AActor::VTableLayoutMap.find(STR("FindComponentByInterface_C__C_TSubclassOf_UInterface_")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("FindComponentByInterface_C__C_TSubclassOf_UInterface_"), 0x740);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AllowActorComponentToReplicate")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AllowActorComponentToReplicate"), 0x748);
}

if (auto it = AActor::VTableLayoutMap.find(STR("AllowActorComponentToReplicate_C__Ptr_C_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("AllowActorComponentToReplicate_C__Ptr_C_UActorComponent"), 0x748);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsComponentRelevantForNavigation")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsComponentRelevantForNavigation"), 0x750);
}

if (auto it = AActor::VTableLayoutMap.find(STR("IsComponentRelevantForNavigation_C__Ptr_UActorComponent")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("IsComponentRelevantForNavigation_C__Ptr_UActorComponent"), 0x750);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisplayDebug")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisplayDebug"), 0x758);
}

if (auto it = AActor::VTableLayoutMap.find(STR("DisplayDebug__Ptr_UCanvas__Ref_C_FDebugDisplayInfo__Ref_float__Ref_float")); it == AActor::VTableLayoutMap.end())
{
    AActor::VTableLayoutMap.emplace(STR("DisplayDebug__Ptr_UCanvas__Ref_C_FDebugDisplayInfo__Ref_float__Ref_float"), 0x758);
}

