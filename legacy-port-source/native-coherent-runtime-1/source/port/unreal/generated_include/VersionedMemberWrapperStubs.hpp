// Auto-generated stubs for versioned member variable wrappers
// These members have different types across UE versions and need manual wrapper implementation
// Implement public wrapper functions that handle version switching

// UStruct::SuperStruct
// Available versioned getters:
//   GetSuperStructBase() -> ObjectPtr_Private::TNonAccessTrackedObjectPtr<UStruct> (versions < 5.7)
//   GetSuperStruct507() -> TObjectPtr<UStruct> (versions >= 5.7)

// UStruct::MinAlignment
// Available versioned getters:
//   GetMinAlignmentBase() -> int32 (versions < 5.6)
//   GetMinAlignment506() -> int16 (versions >= 5.6)

// UGameViewportClient::Viewport
// Available versioned getters:
//   GetViewportBase() -> FViewport* (versions < 5.8)
//   GetViewport508() -> FSceneViewport* (versions >= 5.8)

// UGameViewportClient::ViewportFrame
// Available versioned getters:
//   GetViewportFrameBase() -> FViewportFrame* (versions < 5.8)
//   GetViewportFrame508() -> FSceneViewport* (versions >= 5.8)

// UGameViewportClient::Window
// Available versioned getters:
//   GetWindowBase() -> TWeakPtr<SWindow,0> (versions < 5.0)
//   GetWindow500() -> TWeakPtr<SWindow,1> (versions >= 5.0)

// UGameViewportClient::ViewportOverlayWidget
// Available versioned getters:
//   GetViewportOverlayWidgetBase() -> TWeakPtr<SOverlay,0> (versions < 5.0)
//   GetViewportOverlayWidget500() -> TWeakPtr<SOverlay,1> (versions >= 5.0)
//   GetViewportOverlayWidget508() -> TSharedPtr<SOverlay,1> (versions >= 5.8)

// UGameViewportClient::HighResScreenshotDialog
// Available versioned getters:
//   GetHighResScreenshotDialogBase() -> TWeakPtr<SWindow,0> (versions < 5.0)
//   GetHighResScreenshotDialog500() -> TWeakPtr<SWindow,1> (versions >= 5.0)

// UGameViewportClient::CursorWidgets
// Available versioned getters:
//   GetCursorWidgetsBase() -> TMap<enum EMouseCursor::Type, TSharedRef<SWidget,0>> (versions < 4.23)
//   GetCursorWidgets423() -> TMap<enum EMouseCursor::Type, TSharedPtr<SWidget,0>> (versions >= 4.23)
//   GetCursorWidgets500() -> TMap<enum EMouseCursor::Type, TSharedPtr<SWidget,1>> (versions >= 5.0)

// UGameViewportClient::MouseCaptureMode
// Available versioned getters:
//   GetMouseCaptureModeBase() -> EMouseCaptureMode::Type (versions < 4.11)
//   GetMouseCaptureMode411() -> EMouseCaptureMode (versions >= 4.11)

// UGameViewportClient::GameLayerManagerPtr
// Available versioned getters:
//   GetGameLayerManagerPtrBase() -> TWeakPtr<IGameLayerManager,0> (versions < 5.0)
//   GetGameLayerManagerPtr500() -> TWeakPtr<IGameLayerManager,1> (versions >= 5.0)

// UGameViewportClient::HardwareCursorCache
// Available versioned getters:
//   GetHardwareCursorCacheBase() -> TMap<FName, TSharedPtr<FHardwareCursor,0>> (versions < 4.23)
//   GetHardwareCursorCache423() -> TMap<FName, void *> (versions >= 4.23)

// UGameViewportClient::HardwareCursors
// Available versioned getters:
//   GetHardwareCursorsBase() -> TMap<enum EMouseCursor::Type, TSharedPtr<FHardwareCursor,0>> (versions < 4.23)
//   GetHardwareCursors423() -> TMap<enum EMouseCursor::Type, void *> (versions >= 4.23)

// FUObjectArray::OpenForDisregardForGC
// Available versioned getters:
//   GetOpenForDisregardForGCBase() -> int32 (versions < 4.11)
//   GetOpenForDisregardForGC411() -> bool (versions >= 4.11)

// FUObjectArray::ObjAvailableList
// Available versioned getters:
//   GetObjAvailableListBase() -> TLockFreePointerList<int> (versions < 4.27)
//   GetObjAvailableList427() -> TArray<int,TSizedDefaultAllocator<32> > (versions >= 4.27)

// TUObjectArray::Objects
// Available versioned getters:
//   GetObjectsBase() -> FUObjectItem* (versions < 4.20)
//   GetObjects420() -> FUObjectItem** (versions >= 4.20)

// AGameMode::DefaultPlayerName
// Available versioned getters:
//   GetDefaultPlayerNameBase() -> FString (versions < 4.8)
//   GetDefaultPlayerName408() -> FText (versions >= 4.8)

// AActor::ParentComponentActor
// Available versioned getters:
//   GetParentComponentActorBase() -> TWeakObjectPtr<AActor,FWeakObjectPtr,FIndexToObject> (versions < 4.8)
//   GetParentComponentActor408() -> TWeakObjectPtr<AActor,FWeakObjectPtr> (versions >= 4.8)

// AActor::LastRenderTime
// Available versioned getters:
//   GetLastRenderTimeBase() -> float (versions < 5.5)
//   GetLastRenderTime505() -> FActorLastRenderTime (versions >= 5.5)

// FFieldClass::CastFlags
// Available versioned getters:
//   GetCastFlagsBase() -> uint64 (versions < 5.7)
//   GetCastFlags507() -> EClassCastFlags (versions >= 5.7)

// FFieldClass::ConstructFn
// Available versioned getters:
//   GetConstructFnBase() -> std::function<FField*(const void*, const void*, EObjectFlags)>* (versions < 5.8)
//   GetConstructFn508() -> std::function<FField*(const void*, const void*)>* (versions >= 5.8)

// FProperty::PropertyFlags
// Available versioned getters:
//   GetPropertyFlagsBase() -> uint64 (versions < 4.20)
//   GetPropertyFlags420() -> EPropertyFlags (versions >= 4.20)

// UWorld::NetworkActors
// Available versioned getters:
//   GetNetworkActorsBase() -> TArray<AActor *,FDefaultAllocator> (versions < 4.9)
//   GetNetworkActors409() -> TSet<AActor *> (versions >= 4.9)

// UWorld::TimeSeconds
// Available versioned getters:
//   GetTimeSecondsBase() -> float (versions < 5.1)
//   GetTimeSeconds501() -> double (versions >= 5.1)

// UWorld::RealTimeSeconds
// Available versioned getters:
//   GetRealTimeSecondsBase() -> float (versions < 5.1)
//   GetRealTimeSeconds501() -> double (versions >= 5.1)

// UWorld::AudioTimeSeconds
// Available versioned getters:
//   GetAudioTimeSecondsBase() -> float (versions < 5.1)
//   GetAudioTimeSeconds501() -> double (versions >= 5.1)

// UWorld::PauseDelay
// Available versioned getters:
//   GetPauseDelayBase() -> float (versions < 5.1)
//   GetPauseDelay501() -> double (versions >= 5.1)

// UWorld::UnpausedTimeSeconds
// Available versioned getters:
//   GetUnpausedTimeSecondsBase() -> float (versions < 5.1)
//   GetUnpausedTimeSeconds501() -> double (versions >= 5.1)

// UFunction::FunctionFlags
// Available versioned getters:
//   GetFunctionFlagsBase() -> uint32 (versions < 4.17)
//   GetFunctionFlags417() -> EFunctionFlags (versions >= 4.17)

// UFunction::Func
// Available versioned getters:
//   GetFuncBase() -> void (UObject::*)(FFrame&, void* const) (versions < 4.19)
//   GetFunc419() -> std::function<void(UObject*, FFrame&, void* const)>* (versions >= 4.19)

// UClass::ClassFlags
// Available versioned getters:
//   GetClassFlagsBase() -> uint32 (versions < 4.17)
//   GetClassFlags417() -> EClassFlags (versions >= 4.17)

// UClass::ClassCastFlags
// Available versioned getters:
//   GetClassCastFlagsBase() -> uint64 (versions < 4.20)
//   GetClassCastFlags420() -> EClassCastFlags (versions >= 4.20)

// UClass::ClassUnique
// Available versioned getters:
//   GetClassUniqueBase() -> int32 (versions < 4.18)
//   GetClassUnique418() -> uint32 (versions >= 4.18)

// UClass::bCooked
// Available versioned getters:
//   GetbCookedBase() -> bool (versions < 4.18)
//   GetbCooked418() -> uint32 (versions >= 4.18)

// UEnum::Names
// Available versioned getters:
//   GetNamesBase() -> TArray<FName,FDefaultAllocator> (versions < 4.9)
//   GetNames409() -> TArray<TPair<FName,unsigned char>,FDefaultAllocator> (versions >= 4.9)
//   GetNames415() -> TArray<TPair<FName,__int64>,FDefaultAllocator> (versions >= 4.15)
//   GetNames416() -> TArray<TTuple<FName,__int64>,FDefaultAllocator> (versions >= 4.16)
//   GetNames507() -> UEnum::FNameData (versions >= 5.7)

// FSetProperty::SetLayout
// Available versioned getters:
//   GetSetLayoutBase() -> FScriptSetLayout (versions < 5.7)
//   GetSetLayout507() -> FScriptSparseSetLayout (versions >= 5.7)

// FUObjectHashTables::HashOuter
// Available versioned getters:
//   GetHashOuterBase() -> TMultiMap<int, UObjectBase *> (versions < 5.0)
//   GetHashOuter500() -> TMultiMap<int, unsigned int> (versions >= 5.0)

// FUObjectHashTables::ObjectOuterMap
// Available versioned getters:
//   GetObjectOuterMapBase() -> TMap<UObjectBase *, TSet<UObjectBase *>> (versions < 4.16)
//   GetObjectOuterMap416() -> TMap<UObjectBase *, FHashBucket> (versions >= 4.16)

// FUObjectHashTables::ClassToObjectListMap
// Available versioned getters:
//   GetClassToObjectListMapBase() -> TMap<UClass *, TSet<UObjectBase *>> (versions < 4.22)
//   GetClassToObjectListMap422() -> TMap<UClass *, FHashBucket> (versions >= 4.22)

