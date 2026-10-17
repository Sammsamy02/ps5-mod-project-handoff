### M1

[main.lua:41](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-path-audit/source-cache/main.lua:41)

```text
41: end
42: 
43: local function class(path)
44:     local c = StaticFindObject(path)
45:     if not isValid(c) then error("Could not find class: " .. path) end
46:     return c
47: end
48: 
49: local function construct(path, outer, name)
50:     local obj = StaticConstructObject(class(path), outer, FName(name))
51:     if not isValid(obj) then
52:         error("Could not construct: " .. path .. " as " .. tostring(name))
53:     end
54:     return obj
55: end
56: 
```

### M2

[main.lua:426](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-path-audit/source-cache/main.lua:426)

```text
426: local function refreshText()
427:     for i, widget in ipairs(ui.rowTexts or {}) do
428:         if isValid(widget) and rows[i] then
429:             local label = rows[i].label()
430:             local selected = (i == selectedIndex)
431:             local marker = selected and ">> " or "   "
432: 
433:             pcall(function()
434:                 widget:SetText(FText(marker .. label))
435:             end)
436: 
437:             if selected then
438:                 setTextColor(widget, 1.00, 0.035, 0.055, 1.0)
439:             else
440:                 setTextColor(widget, 0.78, 0.78, 0.78, 1.0)
441:             end
442:         end
443:     end
444: end
445: 
446: local function setInputLock(lock)
447:     local pc = getPlayerController()
448:     if not isValid(pc) then return end
449: 
450:     pcall(function() pc:SetIgnoreLookInput(lock) end)
451:     pcall(function() pc:SetIgnoreMoveInput(lock) end)
452: end
453: 
454: local function addStyledText(name, parent, text, size, color, padTop, padBottom)
455:     local w = construct("/Script/UMG.TextBlock", ui.tree, name)
456:     local slot = parent:AddChild(w)
457: 
458:     pcall(function()
459:         if slot then
460:             slot:SetPadding({
461:                 Left=0.0,
462:                 Top=padTop or 0.0,
463:                 Right=0.0,
464:                 Bottom=padBottom or 0.0
465:             })
466:         end
467:     end)
468: 
469:     pcall(function() w:SetText(FText(text or "")) end)
470:     pcall(function() if w.Font then w.Font.Size = size or 13 end end)
471: 
472:     if color then
473:         setTextColor(w, color[1], color[2], color[3], color[4] or 1.0)
474:     end
475: 
476:     pcall(function() w:SetAutoWrapText(false) end)
477:     pcall(function()
478:         w:SetShadowColorAndOpacity({
479:             SpecifiedColor={R=0.0,G=0.0,B=0.0,A=0.92},
480:             ColorUseRule=0
481:         })
482:     end)
483:     pcall(function() w.ShadowOffset = {X=1.0,Y=1.0} end)
484: 
485:     return w
```

### M3

[main.lua:488](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-path-audit/source-cache/main.lua:488)

```text
488: local function buildUI()
489:     if isValid(ui.root) and isValid(ui.panel) and #(ui.rowTexts or {}) == #rows then
490:         return true
491:     end
492: 
493:     local pc = getPlayerController()
494:     if not isValid(pc) then
495:         status = "Load a save first."
496:         return false
497:     end
498: 
499:     local widgetLib = StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")
500:     if not isValid(widgetLib) then
501:         log("WidgetBlueprintLibrary not found.")
502:         return false
503:     end
504: 
505:     local ok, err = pcall(function()
506:         local userWidgetClass = class("/Script/UMG.UserWidget")
507:         local created = nil
508: 
509:         local createOK, createResult = pcall(function()
510:             return widgetLib:Create(pc, userWidgetClass, pc)
511:         end)
512: 
513:         if createOK and isValid(createResult) then
514:             created = createResult
515:         else
516:             created = StaticConstructObject(userWidgetClass, pc, FName("TownfallItemSpawnerRoot"))
517:         end
518: 
519:         if not isValid(created) then
520:             error("Unable to create root UserWidget.")
521:         end
522: 
523:         ui.root = created
524:         ui.tree = construct("/Script/UMG.WidgetTree", ui.root, "TownfallItemSpawnerWidgetTree")
525:         ui.root.WidgetTree = ui.tree
526: 
527:         ui.canvas = construct("/Script/UMG.CanvasPanel", ui.tree, "TownfallItemSpawnerCanvas")
528:         ui.tree.RootWidget = ui.canvas
529: 
530:         ui.panel = construct("/Script/UMG.Border", ui.tree, "TownfallItemSpawnerFrame")
531:         local panelSlot = ui.canvas:AddChild(ui.panel)
532: 
533:         pcall(function()
534:             panelSlot:SetAnchors({
535:                 Minimum={X=1.0,Y=0.0},
536:                 Maximum={X=1.0,Y=0.0}
537:             })
538:         end)
539:         pcall(function() panelSlot:SetAlignment({X=1.0,Y=0.0}) end)
540:         pcall(function() panelSlot:SetPosition({X=-24.0,Y=24.0}) end)
541:         pcall(function() panelSlot:SetSize({X=570.0,Y=500.0}) end)
542: 
543:         pcall(function()
544:             ui.panel:SetPadding({Left=2.0,Top=2.0,Right=2.0,Bottom=2.0})
545:         end)
546:         pcall(function()
547:             ui.panel:SetBrushColor({R=0.36,G=0.005,B=0.012,A=0.97})
548:         end)
549: 
550:         ui.inner = construct("/Script/UMG.Border", ui.tree, "TownfallItemSpawnerInner")
551:         ui.panel:SetContent(ui.inner)
552: 
553:         pcall(function()
554:             ui.inner:SetPadding({Left=15.0,Top=12.0,Right=15.0,Bottom=10.0})
555:         end)
556:         pcall(function()
557:             ui.inner:SetBrushColor({R=0.006,G=0.006,B=0.009,A=0.965})
558:         end)
559: 
560:         ui.box = construct("/Script/UMG.VerticalBox", ui.tree, "TownfallItemSpawnerVBox")
561:         ui.inner:SetContent(ui.box)
562: 
563:         ui.title = addStyledText(
564:             "TownfallItemSpawnerTitle",
565:             ui.box,
566:             "TOWNFALL : ITEM SPAWNER",
567:             21,
568:             {0.98,0.025,0.045,1.0},
569:             0.0,
570:             0.0
571:         )
572: 
573:         ui.divider = addStyledText(
574:             "TownfallItemSpawnerDivider",
575:             ui.box,
576:             "------------------------------------------",
577:             10,
578:             {0.58,0.015,0.025,1.0},
579:             0.0,
580:             3.0
581:         )
582: 
583:         ui.controls = addStyledText(
584:             "TownfallItemSpawnerControls",
585:             ui.box,
586:             "UP/DOWN SELECT | ENTER SPAWN | LEFT/RIGHT PAGE | F7 CLOSE",
587:             10,
588:             {0.52,0.52,0.52,1.0},
589:             0.0,
590:             3.0
591:         )
592: 
593:         ui.rowTexts = {}
594:         ui.sectionTexts = {}
595: 
596:         local lastSection = nil
597:         local sectionCount = 0
598: 
599:         for i, row in ipairs(rows) do
600:             if row.section ~= lastSection then
601:                 sectionCount = sectionCount + 1
602: 
603:                 local header = addStyledText(
604:                     "TownfallItemSpawnerSection" .. tostring(sectionCount),
605:                     ui.box,
606:                     "[ " .. row.section .. " ]",
607:                     12,
608:                     {0.70,0.025,0.035,1.0},
609:                     (lastSection == nil) and 4.0 or 7.0,
610:                     0.0
611:                 )
612: 
613:                 table.insert(ui.sectionTexts, header)
614:                 lastSection = row.section
615:             end
616: 
617:             local rowText = addStyledText(
618:                 "TownfallItemSpawnerRow" .. tostring(i),
619:                 ui.box,
620:                 "   " .. row.label(),
621:                 13,
622:                 {0.78,0.78,0.78,1.0},
623:                 0.0,
624:                 0.0
625:             )
626: 
627:             ui.rowTexts[i] = rowText
628:         end
629: 
630:         refreshText()
631:         ui.root:AddToViewport(9999)
632:         ui.root:SetVisibility(1)
633:     end)
634: 
635:     if not ok then
636:         log("UI creation failed: " .. tostring(err))
637:         ui.root, ui.tree, ui.canvas, ui.panel, ui.inner, ui.box = nil,nil,nil,nil,nil,nil
638:         ui.title, ui.divider, ui.controls = nil,nil,nil
639:         ui.rowTexts, ui.sectionTexts = {},{}
640:         return false
641:     end
642: 
643:     log("Menu created.")
644:     return true
```

### M4

[main.lua:647](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-path-audit/source-cache/main.lua:647)

```text
647: local function showMenu(show)
648:     if show == menuOpen then return true end
649: 
650:     if show then
651:         if #items == 0 then scanItems() end
652:         if not buildUI() then return false end
653: 
654:         refreshText()
655:         pcall(function() ui.root:SetVisibility(0) end)
656:         setInputLock(true)
657:         menuOpen = true
658:     else
659:         if isValid(ui.root) then
660:             pcall(function() ui.root:SetVisibility(1) end)
661:         end
662: 
663:         setInputLock(false)
664:         menuOpen = false
665:     end
666: 
667:     return true
668: end
669: 
670: local function toggleMenu()
671:     showMenu(not menuOpen)
672: end
673: 
674: local function moveSelection(delta)
675:     if not menuOpen then return end
676: 
677:     selectedIndex = selectedIndex + delta
678:     if selectedIndex < 1 then selectedIndex = #rows end
679:     if selectedIndex > #rows then selectedIndex = 1 end
680: 
681:     refreshText()
682: end
683: 
684: local function activateSelection()
685:     if not menuOpen then return end
686: 
687:     local row = rows[selectedIndex]
688:     if row and row.activate then
689:         row.activate()
690:         refreshText()
691:     end
692: end
693: 
694: RegisterKeyBind(Key.F7, function()
695:     ExecuteInGameThread(toggleMenu)
696: end)
```

### R1

[UE4SSProgram.cpp:1783](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r4/units/UE4SSProgram.cpp:1783)

```text
1783:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("ObjectProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_objectproperty);
1784:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("ClassProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_classproperty);
1785:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("Int8Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_int8property);
1786:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("Int16Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_int16property);
1787:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("IntProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_intproperty);
1788:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("Int64Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_int64property);
1789:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("ByteProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_byteproperty);
1790:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("UInt16Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_uint16property);
1791:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("UInt32Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_uint32property);
1792:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("UInt64Property"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_uint64property);
1793:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("StructProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_structproperty);
1794:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("ArrayProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_arrayproperty);
1795:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("SetProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_setproperty);
1796:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("MapProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_mapproperty);
1797:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("FloatProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_floatproperty);
1798:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("DoubleProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_doubleproperty);
1799:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("BoolProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_boolproperty);
1800:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("EnumProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_enumproperty);
1801:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("WeakObjectProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_weakobjectproperty);
1802:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("NameProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_nameproperty);
1803:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("TextProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_textproperty);
1804:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("StrProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_strproperty);
1805:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("SoftObjectProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_softobjectproperty);
1806:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("SoftClassProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_softobjectproperty);
1807:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("InterfaceProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_interfaceproperty);
1808:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("DelegateProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_delegateproperty);
1809:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("MulticastDelegateProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_multicastdelegateproperty);
1810:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("MulticastInlineDelegateProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_multicastdelegateproperty);
1811:         LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("MulticastSparseDelegateProperty"), Unreal::FNAME_Find).GetComparisonIndex(), &LuaType::push_multicastsparsedelegateproperty);
```

### C1

[LuaMod.cpp:2231](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-r5-thread-source-integration/thread/LuaMod.cpp:2231)

```text
2231:         lua.register_function("StaticConstructObject", [](const LuaMadeSimple::Lua& lua) -> int {
2232:             std::string error_overload_not_found{R"(
2233: No overload found for function 'StaticConstructObject'.
2234: Overloads:
2235: #1: StaticConstructObject(
2236:                             UClass Class,
2237:                             UObject Outer,
2238:                             FName Name, #Optional
2239:                             EObjectFlags Flags, #Optional
2240:                             EInternalObjectFlags InternalSetFlags, #Optional
2241:                             bool CopyTransientsFromClassDefaults, #Optional
2242:                             bool AssumeTemplateIsArchetype, #Optional
2243:                             UObject Template, #Optional
2244:                             FObjectInstancingGraph InstanceGraph, #Optional
2245:                             UPackage ExternalPackage, #Optional
2246:                             void SubobjectOverrides #Optional
2247: 
2248: ))"};
2249: 
2250:             // For now, we're assuming that if there's userdata, that userdata is of the correct underlying type
2251:             if (!lua.is_userdata())
2252:             {
2253:                 lua.throw_error(error_overload_not_found);
2254:             }
2255:             Unreal::UClass* param_class = lua.get_userdata<LuaType::UClass>().get_remote_cpp_object();
2256: 
2257:             if (!lua.is_userdata())
2258:             {
2259:                 lua.throw_error(error_overload_not_found);
2260:             }
2261:             Unreal::UObject* param_outer = lua.get_userdata<LuaType::UObject>().get_remote_cpp_object();
2262: 
2263:             Unreal::FName param_name;
2264:             if (lua.is_userdata())
2265:             {
2266:                 param_name = lua.get_userdata<LuaType::FName>().get_local_cpp_object();
2267:             }
2268:             else if (lua.is_integer())
2269:             {
2270:                 param_name = Unreal::FName(lua.get_integer());
2271:             }
2272:             else
2273:             {
2274:                 param_name = Unreal::FName(static_cast<int64_t>(0));
2275:             }
2276: 
2277:             Unreal::EObjectFlags param_set_flags{};
2278:             if (lua.is_integer())
2279:             {
2280:                 param_set_flags = static_cast<Unreal::EObjectFlags>(lua.get_integer());
2281:             }
2282: 
2283:             Unreal::EInternalObjectFlags param_internal_set_flags{};
2284:             if (lua.is_integer())
2285:             {
2286:                 param_internal_set_flags = static_cast<Unreal::EInternalObjectFlags>(lua.get_integer());
2287:             }
2288: 
2289:             // The rest are all optional parameters
2290:             bool param_copy_transients_from_class_defaults{};
2291:             if (lua.is_bool())
2292:             {
2293:                 param_copy_transients_from_class_defaults = lua.get_bool();
2294:             }
2295: 
2296:             bool param_assume_template_is_archetype{};
2297:             if (lua.is_bool())
2298:             {
2299:                 param_assume_template_is_archetype = lua.get_bool();
2300:             }
2301: 
2302:             Unreal::UObject* param_template{};
2303:             if (lua.is_userdata())
2304:             {
2305:                 param_template = lua.get_userdata<LuaType::UObject>().get_remote_cpp_object();
2306:             }
2307: 
2308:             // Change this to userdata if support for 'FObjectInstancingGraph' is ever added
2309:             void* param_instance_graph{};
2310:             if (lua.is_integer())
2311:             {
2312:                 param_instance_graph = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer()));
2313:             }
2314: 
2315:             // Change this to userdata if support for 'UPackage' is ever added
2316:             void* param_external_package{};
2317:             if (lua.is_integer())
2318:             {
2319:                 param_external_package = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer()));
2320:             }
2321: 
2322:             // void* param_subobject_overrides{};
2323:             // if (lua.is_integer()) { param_subobject_overrides = reinterpret_cast<void*>(static_cast<uintptr_t>(lua.get_integer())); }
2324: 
2325:             Unreal::FStaticConstructObjectParameters params{param_class, param_outer};
2326:             params.Name = param_name;
2327:             params.SetFlags = param_set_flags;
2328:             params.InternalSetFlags = param_internal_set_flags;
2329:             params.bCopyTransientsFromClassDefaults = param_copy_transients_from_class_defaults;
2330:             params.bAssumeTemplateIsArchetype = param_assume_template_is_archetype;
2331:             params.Template = param_template;
2332:             params.InstanceGraph = static_cast<struct RC::Unreal::FObjectInstancingGraph*>(param_instance_graph);
2333:             params.ExternalPackage = static_cast<class RC::Unreal::UPackage*>(param_external_package);
2334:             Unreal::UObject* created_object = Unreal::UObjectGlobals::StaticConstructObject(params);
2335: 
2336:             LuaType::UObject::construct(lua, created_object);
2337: 
2338:             return 1;
2339:         });
```

### C2

[UObjectGlobals.cpp:19](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObjectGlobals.cpp:19)

```text
19:     RC_UE_API Function<UObject*(StaticConstructObject_Internal_Params_Base)> GlobalState::StaticConstructObjectInternalBase{};
20:     RC_UE_API Function<UObject*(StaticConstructObject_Internal_Params_411)> GlobalState::StaticConstructObjectInternal411{};
21:     RC_UE_API Function<UObject*(const FStaticConstructObjectParameters&)> GlobalState::StaticConstructObjectInternal426{};
22: 
23:     auto SetupStaticConstructObjectInternalAddress(void* FunctionAddress) -> void
24:     {
25:         GlobalState::StaticConstructObjectInternal426.assign_address(FunctionAddress);
26:         GlobalState::StaticConstructObjectInternal411.assign_address(FunctionAddress);
27:         GlobalState::StaticConstructObjectInternalBase.assign_address(FunctionAddress);
28:     }
29: 
30:     namespace Below411
31:     {
32:         static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
33:         {
34:             // Pre-4.11: No EInternalObjectFlags, no bAssumeTemplateIsArchetype, no ExternalPackage
35:             return GlobalState::StaticConstructObjectInternalBase(
36:                     const_cast<UClass*>(Params.Class),
37:                     Params.Outer,
38:                     Params.Name,
39:                     Params.SetFlags,
40:                     Params.Template,
41:                     Params.bCopyTransientsFromClassDefaults,
42:                     Params.InstanceGraph
43:             );
44:         }
45:     }
46: 
47:     namespace Below426
48:     {
49:         static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
50:         {
51:             // 4.11-4.25: Has EInternalObjectFlags, bAssumeTemplateIsArchetype, ExternalPackage
52:             return GlobalState::StaticConstructObjectInternal411(
53:                     Params.Class,
54:                     Params.Outer,
55:                     Params.Name,
56:                     Params.SetFlags,
57:                     Params.InternalSetFlags,
58:                     Params.Template,
59:                     Params.bCopyTransientsFromClassDefaults,
60:                     Params.InstanceGraph,
61:                     Params.bAssumeTemplateIsArchetype,
62:                     Params.ExternalPackage
63:             );
64:         }
65:     }
66:     namespace Below56
67:     {
68:         static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
69:         {
70:             static Function<UObject*(const FStaticConstructObjectParameters&)> StaticConstructObjectInternal = [&]() {
71:                 return GlobalState::StaticConstructObjectInternal426.get_function_address();
72:             }();
73: 
74:             if (!StaticConstructObjectInternal.is_ready()) { return nullptr; }
75:             if (Params.Class == nullptr) { return nullptr; }
76: 
77:             return StaticConstructObjectInternal(Params);
78:         }
79:     }
80:     namespace Above55
81:     {
82:         static auto StaticConstructObject(const FStaticConstructObjectParameters& Params) -> UObject*
83:         {
84:             static Function<UObject*(const FStaticConstructObjectParameters&)> StaticConstructObjectInternal = [&]() {
85:                 return GlobalState::StaticConstructObjectInternal426.get_function_address();
86:             }();
87: 
88:             if (!StaticConstructObjectInternal.is_ready()) { return nullptr; }
89:             if (Params.Class == nullptr) { return nullptr; }
90: 
91:             return StaticConstructObjectInternal(Params);
92:         }
93:     }
94: 
95:     auto StaticConstructObject(const FStaticConstructObjectParameters& GenericParams) -> UObject*
96:     {
97:         if (Version::IsBelow(4, 11))
98:         {
99:             return Below411::StaticConstructObject(GenericParams);
100:         }
101:         else if (Version::IsBelow(4, 26))
102:         {
103:             return Below426::StaticConstructObject(GenericParams);
104:         }
105:         else if (Version::IsBelow(5, 6))
106:         {
107:             Below56::FStaticConstructObjectParameters Params{};
108:             Params.Class = GenericParams.Class;
109:             Params.Outer = GenericParams.Outer;
110:             Params.Name = GenericParams.Name;
111:             Params.SetFlags = GenericParams.SetFlags;
112:             Params.InternalSetFlags = GenericParams.InternalSetFlags;
113:             Params.Template = GenericParams.Template;
114:             Params.bCopyTransientsFromClassDefaults = GenericParams.bCopyTransientsFromClassDefaults;
115:             Params.InstanceGraph = GenericParams.InstanceGraph;
116:             Params.bAssumeTemplateIsArchetype = GenericParams.bAssumeTemplateIsArchetype;
117:             Params.ExternalPackage = GenericParams.ExternalPackage;
118:             return Below56::StaticConstructObject(Params);
119:         }
120:         else
121:         {
122:             Above55::FStaticConstructObjectParameters Params{};
123:             Params.Class = GenericParams.Class;
124:             Params.Outer = GenericParams.Outer;
125:             Params.Name = GenericParams.Name;
126:             Params.SetFlags = GenericParams.SetFlags;
127:             Params.InternalSetFlags = GenericParams.InternalSetFlags;
128:             Params.Template = GenericParams.Template;
129:             Params.bCopyTransientsFromClassDefaults = GenericParams.bCopyTransientsFromClassDefaults;
130:             Params.InstanceGraph = GenericParams.InstanceGraph;
131:             Params.bAssumeTemplateIsArchetype = GenericParams.bAssumeTemplateIsArchetype;
132:             Params.ExternalPackage = GenericParams.ExternalPackage;
133:             return Above55::StaticConstructObject(Params);
134:         }
135:     }
```

### C3

[LuaUFunction.cpp:60](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUFunction.cpp:60)

```text
60: 
61:         setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);
62: 
63:         setup_metamethods(construct_to);
64: 
65:         return table;
66:     }
67: 
68:     auto UFunction::setup_metamethods(BaseObject& base_object) -> void
69:     {
70:         base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Call, [](const LuaMadeSimple::Lua& lua) -> int {
71:             return call_ufunction_from_lua(lua);
72:         });
73:     }
74: 
75:     template <LuaMadeSimple::Type::IsFinal is_final>
76:     auto UFunction::setup_member_functions(const LuaMadeSimple::Lua::Table& table) -> void
```

### C4

[LuaUObject.cpp:154](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:154)

```text
154: #endif
155:     {
156:         if (!lua.is_userdata())
157:         {
158:             lua.throw_error("[UFunction::setup_metamethods -> __call] Attempted to call a UFunction without UFunction userdata attached");
159:         }
160: 
161:         auto& lua_object = lua.get_userdata<LuaType::UFunction>();
162: 
163:         Unreal::UFunction* func{};
164:         Unreal::UObject* calling_context{};
165:         bool is_first_userdata_function = lua_object.get_remote_cpp_object()->IsA<Unreal::UFunction>();
166: 
167:         if (is_first_userdata_function)
168:         {
169:             func = lua_object.get_remote_cpp_object();
170:             calling_context = lua_object.get_base();
171:         }
172:         else
173:         {
174:             calling_context = lua_object.get_remote_cpp_object();
175:         }
176: 
177:         if (lua.is_userdata())
178:         {
179:             auto& lua_object2 = lua.get_userdata<LuaType::UE4SSBaseObject>(1, true);
180:             if (!is_first_userdata_function && lua_object2.derives_from_function())
181:             {
182:                 func = lua.get_userdata<LuaType::UFunction>(1).get_remote_cpp_object();
183:             }
184:             else if (func && !calling_context)
185:             {
186:                 calling_context = lua.get_userdata<LuaType::UObject>(1).get_remote_cpp_object();
187:             }
188:             else
189:             {
190:                 lua.discard_value();
191:             }
192:         }
193: 
194:         if (!func || !calling_context)
195:         {
196:             lua.throw_error("[UFunction::call_ufunction_from_lua] Tried calling function without both UFunction and calling context");
197:         }
198: 
199:         uint8_t num_ufunc_params = func->GetNumParms();
200:         uint16_t return_value_offset = func->GetReturnValueOffset();
201: 
202:         // Unreal::FProperty* param_next = func->get_child_properties<Unreal::FProperty*>();
203:         Unreal::FProperty* return_value_property{};
204:         Unreal::FName return_value_property_type{0u, 0u};
205:         int32_t return_value_property_offset_internal{};
206: 
207:         DynamicUnrealFunctionData dynamic_unreal_function_data{};
208: #ifdef __PROSPERO__
209:         auto& dynamic_unreal_function_out_parameters = Invocation.OutParameters;
210: #else
211:         DynamicUnrealFunctionOutParameters dynamic_unreal_function_out_parameters{};
212: #endif
213:         bool has_out_params{false};
214: 
215:         // if (param_next)
216:         if (func->HasChildren())
217:         {
218:             uint8_t num_supplied_params = Helper::Integer::to<uint8_t>(lua.get_stack_size());
219: 
220:             // When return_value_offset is 0xFFFF it means that there is no return value so num_ufunc_params is accurate
221:             // Otherwise you must subtract 1 to account for the return value (stored in the same struct and counts as a param)
222:             // The ternary makes sure that we never have a negative number of params
223:             bool has_return_value = return_value_offset != 0xFFFF;
224:             uint8_t num_expected_params = num_ufunc_params;
225:             uint8_t num_expected_params_with_return_value = has_return_value ? (num_expected_params - 1 < 0 ? 0 : num_expected_params - 1) : num_expected_params;
226: 
227:             if (num_supplied_params != num_expected_params_with_return_value)
228:             {
229:                 lua.throw_error(fmt::format("[UFunction::setup_metamethods -> __call] UFunction expected {} parameters, received {}",
230:                                             num_expected_params_with_return_value,
231:                                             num_supplied_params));
232:             }
233: 
234:             // for (uint8_t i = 0; i < num_expected_params; ++i)
235:             // uint8_t i = 0;
236:             for (Unreal::FProperty* param_next : Unreal::TFieldRange<Unreal::FProperty>(func, Unreal::EFieldIterationFlags::IncludeDeprecated))
237:             {
238:                 // if (i > 0)
239:                 //{
240:                 //     // If not the first iteration then get the next param
241:                 //     // If it's the first iteration then paramNext was already set earlier
242:                 //     param_next = param_next->get_next<Unreal::XProperty*>();
243:                 // }
244: 
245:                 // if (!param_next)
246:                 //{
247:                 //     break;
248:                 // }
249: 
250:                 if (!param_next->HasAnyPropertyFlags(Unreal::CPF_Parm))
251:                 {
252:                     continue;
253:                 }
254: 
255:                 int32_t offset_internal = param_next->GetOffset_Internal();
256:                 Unreal::FName property_type_fname = param_next->GetClass().GetFName();
257: 
258:                 // Is the offset of this parameter is the same as the ReturnValueOffset in the UFunction ?
259:                 // If yes, then this parameter should be treated as the return value
260:                 // If no, then treat this as just another parameter
261:                 if (offset_internal == return_value_offset)
262:                 {
263:                     return_value_property = param_next;
264:                     return_value_property_type = property_type_fname;
265:                     return_value_property_offset_internal = offset_internal;
266:                 }
267:                 else
268:                 {
269:                     if (param_next->HasAnyPropertyFlags(Unreal::CPF_OutParm) && !param_next->HasAnyPropertyFlags(Unreal::CPF_ReturnParm) &&
270:                         !param_next->HasAnyPropertyFlags(Unreal::CPF_ConstParm))
271:                     {
272:                         has_out_params = true;
273: 
274:                         // Store pointer to params property, and a Lua ref to the corresponding table
275: 
276:                         if (!lua.is_table())
277:                         {
278:                             lua.throw_error(
279:                                     "Tried storing reference to a Lua table for an 'Out' parameter when calling a UFunction but no table was on the stack");
280:                         }
281: 
282: #ifdef __PROSPERO__
283:                         Invocation.StoreOutTableReference(param_next);
284: #else
285:                         // Duplicate the Lua function to the top of the stack for luaL_ref
286:                         lua_pushvalue(lua.get_lua_state(), 1);
287: 
288:                         // Take a reference to the Lua function (it also pops it of the stack)
289:                         dynamic_unreal_function_out_parameters.add({.property = param_next, .lua_ref = lua.registry().make_ref()});
290: #endif
291: 
292:                         if (!param_next->IsA<Unreal::FStructProperty>() && !param_next->IsA<Unreal::FArrayProperty>())
293:                         {
294:                             continue;
295:                         }
296:                     }
297: 
298:                     int32_t name_comparison_index = property_type_fname.GetComparisonIndex();
299: 
300:                     if (StaticState::m_property_value_pushers.contains(name_comparison_index))
301:                     {
302:                         void* data = &dynamic_unreal_function_data.data[offset_internal];
303: 
304:                         const PusherParams pusher_params{.operation = Operation::Set,
305:                                                          .lua = lua,
306:                                                          .base = static_cast<Unreal::UObject*>(static_cast<void*>(dynamic_unreal_function_data.data)),
307:                                                          .data = data,
308:                                                          .property = param_next};
309:                         StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
310:                     }
311:                     else
312:                     {
313:                         std::string parameter_type_name = to_string(property_type_fname.ToString());
314:                         std::string parameter_name = to_string(param_next->GetName());
315:                         lua.throw_error(
316:                                 fmt::format("Tried calling UFunction without a registered handler for parameter. Parameter '{}' of type '{}' not supported.",
317:                                             parameter_name,
318:                                             parameter_type_name));
319:                     }
320:                 }
321:             };
322:         }
323: 
324:         // It's assumed that everything is safe here
325:         // Every part of the system is designed to throw when stuff goes wrong
326:         // This code should never be executed in those cases
327:         calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);
```

### C5

[LuaUObject.cpp:330](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:330)

```text
330:         if (has_out_params)
331:         {
332:             for (auto const [param, lua_table_ref] : dynamic_unreal_function_out_parameters.get())
333:             {
334:                 if (!param)
335:                 {
336:                     break;
337:                 }
338: 
339:                 // Use the stored registry index to put the original Lua table for the 'Out' param back on the stack
340:                 lua.registry().get_table_ref(lua_table_ref);
341:                 auto lua_table = lua.get_table();
342: 
343:                 auto reuse_same_table = param->IsA<Unreal::FArrayProperty>() || param->IsA<Unreal::FStructProperty>() || param->IsA<Unreal::FSetProperty>();
344:                 if (!reuse_same_table)
345:                 {
346:                     lua_table.add_key(to_string(param->GetName()).c_str());
347:                 }
348: 
349:                 Unreal::FName param_type_fname = param->GetClass().GetFName();
350:                 int32_t name_comparison_index = param_type_fname.GetComparisonIndex();
351: 
352:                 if (StaticState::m_property_value_pushers.contains(name_comparison_index))
353:                 {
354:                     uint8_t* data = &dynamic_unreal_function_data.data[param->GetOffset_Internal()];
355: 
356:                     const PusherParams pusher_params{
357:                             .operation = Operation::GetNonTrivialLocal,
358:                             .lua = lua,
359:                             .base = static_cast<Unreal::UObject*>(static_cast<void*>(dynamic_unreal_function_data.data)),
360:                             .data = data,
361:                             .property = param,
362:                             .create_new_if_get_non_trivial_local = !reuse_same_table,
363:                     };
364:                     StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
365:                 }
366:                 else
367:                 {
368:                     std::string param_type_name = to_string(param_type_fname.ToString());
369:                     lua.throw_error(fmt::format("Tried calling UFunction without a registered handler 'Out' param. Type '{}' not supported.", param_type_name));
370:                 }
371: 
372:                 if (!reuse_same_table)
373:                 {
374:                     lua_table.fuse_pair();
375:                 }
376:             }
377:         }
378: 
379:         // If there's a return value, then forward it to the Lua script
380:         if (return_value_property)
381:         {
382:             int32_t name_comparison_index = return_value_property_type.GetComparisonIndex();
383: 
384:             if (StaticState::m_property_value_pushers.contains(name_comparison_index))
385:             {
386:                 uint8_t* data = &dynamic_unreal_function_data.data[return_value_property_offset_internal];
387: 
388:                 const PusherParams pusher_params{.operation = Operation::GetNonTrivialLocal,
389:                                                  .lua = lua,
390:                                                  .base = static_cast<Unreal::UObject*>(static_cast<void*>(data)), // Base is the start of the params struct
391:                                                  .data = data,
392:                                                  .property = return_value_property};
393:                 StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
394: 
395:                 return 1;
396:             }
397:             else
398:             {
399:                 std::string return_value_type_name = to_string(return_value_property_type.ToString());
400:                 lua.throw_error(fmt::format("Tried calling UFunction without a registered handler for return value. Return value of type '{}' not supported.",
401:                                             return_value_type_name));
402:             }
403: 
404:             return 0;
405:         }
```

### C6

[UObject.cpp:448](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UObject.cpp:448)

```text
448:     void UObject::ProcessEvent(UFunction* Function, void* Parms)
449:     {
450:         IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(UObject, ProcessEvent, void, PARAMS(UFunction*, void*), ARGS(Function, Parms))
451:     }
```

### P1

[LuaUObject.cpp:440](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:440)

```text
440:         // If the UObject is nullptr (which is valid), then construct an empty Lua object to enable chaining
441:         if (!object)
442:         {
443:             UObject::construct(lua, nullptr);
444:         }
445:         else if (object->IsA<Unreal::UFunction>())
446:         {
447:             UFunction::construct(lua, nullptr, static_cast<Unreal::UFunction*>(object));
448:         }
449:         else if (object->IsA<Unreal::UClass>())
450:         {
451:             UClass::construct(lua, static_cast<Unreal::UClass*>(object));
452:         }
453:         else if (object->IsA<Unreal::UScriptStruct>())
454:         {
455:             ScriptStructWrapper script_struct_wrapper{static_cast<Unreal::UScriptStruct*>(object), nullptr, nullptr};
456:             UScriptStruct::construct(lua, script_struct_wrapper);
457:         }
458:         else if (object->IsA<Unreal::UDataTable>())
459:         {
460:             UDataTable::construct(lua, static_cast<Unreal::UDataTable*>(object));
461:         }
462:         else if (object->IsA<Unreal::UStruct>())
463:         {
464:             UStruct::construct(lua, static_cast<Unreal::UStruct*>(object));
465:         }
466:         else if (object->IsA<Unreal::UEnum>())
467:         {
468:             UEnum::construct(lua, static_cast<Unreal::UEnum*>(object));
469:         }
470:         else if (object->IsA<Unreal::UWorld>())
471:         {
472:             UWorld::construct(lua, static_cast<Unreal::UWorld*>(object));
473:         }
474:         else if (object->IsA<Unreal::AActor>())
475:         {
476:             AActor::construct(lua, static_cast<Unreal::AActor*>(object));
477:         }
478:         else
479:         {
480:             UObject::construct(lua, object);
481:         }
482:     }
483: 
484:     auto construct_fname(const LuaMadeSimple::Lua& lua) -> void
485:     {
486:         const auto& lua_object = lua.get_userdata<UObject>();
487:         LuaType::FName::construct(lua, lua_object.get_remote_cpp_object()->GetNamePrivate());
488:     }
489: 
490:     auto construct_uclass(const LuaMadeSimple::Lua& lua) -> void
491:     {
492:         const auto& lua_object = lua.get_userdata<UObject>();
493:         LuaType::UClass::construct(lua, lua_object.get_remote_cpp_object()->GetClassPrivate());
494:     }
495: 
496:     auto construct_xproperty(const LuaMadeSimple::Lua& lua, Unreal::FProperty* property) -> void
497:     {
498:         auto_construct_property(lua, property);
499:     }
500: 
501:     auto push_unhandledproperty([[maybe_unused]] const PusherParams& params) -> void
502:     {
503:         // TODO: Implement or remove this function
504:     }
505: 
506:     auto push_objectproperty(const PusherParams& params) -> void
507:     {
508:         // We're already setup for the alternate branch inside the get_property_vc() function
509:         // We don't need to supply a property name because the alternate branch has already been told about the property from the push_unreal_object() function()
510:         Unreal::UObject** property_value = static_cast<Unreal::UObject**>(params.data);
511: 
512:         // Finally construct the Lua object
513:         switch (params.operation)
514:         {
515:         case Operation::GetNonTrivialLocal:
516:         case Operation::Get:
517:             auto_construct_object(params.lua, *property_value);
518:             break;
519:         case Operation::Set: {
520:             if (params.lua.is_userdata(params.stored_at_index))
521:             {
522:                 const auto& lua_object = params.lua.get_userdata<LuaType::UObject>(params.stored_at_index);
523:                 *property_value = lua_object.get_remote_cpp_object();
524:             }
525:             else if (params.lua.is_nil(params.stored_at_index))
526:             {
527:                 params.lua.discard_value(params.stored_at_index);
528:                 *property_value = nullptr;
529:             }
530:             else
531:             {
532:                 params.throw_error("push_objectproperty", "Value must be UObject or nil");
533:             }
534:             break;
535:         }
536:         case Operation::GetParam:
537:             RemoteUnrealParam::construct(params.lua, params.data, params.base, params.property);
538:             break;
539:         default:
540:             params.throw_error("push_objectproperty", "Unhandled Operation");
541:             break;
542:         }
543:     }
544: 
545:     auto push_classproperty(const PusherParams& params) -> void
546:     {
547:         Unreal::UClass** property_value = static_cast<Unreal::UClass**>(params.data);
548: 
549:         switch (params.operation)
550:         {
551:         case Operation::GetNonTrivialLocal:
552:         case Operation::Get:
553:             LuaType::UClass::construct(params.lua, *property_value);
554:             break;
555:         case Operation::Set: {
556:             if (params.lua.is_userdata(params.stored_at_index))
557:             {
558:                 const auto& lua_object = params.lua.get_userdata<LuaType::UClass>(params.stored_at_index);
559:                 *property_value = lua_object.get_remote_cpp_object();
560:             }
561:             else if (params.lua.is_nil(params.stored_at_index))
562:             {
563:                 params.lua.discard_value(params.stored_at_index);
564:             }
565:             else
566:             {
567:                 params.throw_error("push_classproperty", "Value must be UClass or nil");
568:             }
569:             break;
570:         }
571:         case Operation::GetParam:
572:             RemoteUnrealParam::construct(params.lua, params.data, params.base, params.property);
573:             break;
574:         default:
575:             params.throw_error("push_classproperty", "Unhandled Operation");
576:             break;
```

### P2

[LuaUObject.cpp:634](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:634)

```text
634:      * @param table_index Stack index where the Lua table is located
635:      * @param base Optional UObject base for context
636:      */
637:     auto convert_lua_table_to_struct(
638:             const LuaMadeSimple::Lua& lua,
639:             Unreal::UScriptStruct* script_struct,
640:             void* data,
641:             int table_index,
642:             Unreal::UObject* base
643:             ) -> void
644:     {
645:         if (!script_struct || !data)
646:         {
647:             lua.throw_error("convert_lua_table_to_struct: script_struct or data is null");
648:             return;
649:         }
650: 
651:         auto handle_property = [&](Unreal::FProperty* field, const std::string& field_name) {
652:             Unreal::FName field_type_fname = field->GetClass().GetFName();
653: 
654:             // Push the field name (key for the table)
655:             lua_pushstring(lua.get_lua_state(), field_name.c_str());
656: 
657:             // Get the value from the table
658:             // For lua_rawget, we need the actual stack position of the table
659:             // If table_index is positive, it stays the same even after pushing the key
660:             // If it's negative, we need to adjust for the key we just pushed
661: 
662:             // Adjust index for negative values after pushing key
663:             int adjusted_index = table_index;
664:             if (table_index < 0)
665:             {
666:                 adjusted_index = table_index - 1;
667:             }
668: 
669:             auto table_value_type = lua_rawget(lua.get_lua_state(), adjusted_index);
670: 
671:             // If there was nothing in the table for this field, leave default value
672:             if (table_value_type == LUA_TNIL || table_value_type == LUA_TNONE)
673:             {
674:                 lua.discard_value(-1);
675:                 return;
676:             }
677: 
678:             int32_t name_comparison_index = field_type_fname.GetComparisonIndex();
679: 
680:             if (StaticState::m_property_value_pushers.contains(name_comparison_index))
681:             {
682:                 void* field_data = &static_cast<uint8_t*>(data)[field->GetOffset_Internal()];
683: 
684:                 const PusherParams pusher_params{
685:                         .operation = Operation::Set,
686:                         .lua = lua,
687:                         .base = base ? base : static_cast<Unreal::UObject*>(data),
688:                         .data = field_data,
689:                         .property = field,
690:                         .stored_at_index = -1 // Value is at top of stack
691:                 };
692: 
693:                 StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
694:             }
695:             else
696:             {
697:                 // Just skip fields without handlers and pop the value
698:                 lua.discard_value(-1);
699:                 Output::send<LogLevel::Verbose>(
700:                         STR("convert_lua_table_to_struct: Skipping field '{}' of type '{}' (no handler)\n"),
701:                         to_wstring(field_name),
702:                         field_type_fname.ToString()
703:                         );
704: 
705:             }
706:         };
707: 
708:         LuaCustomProperty::StaticStorage::property_list.for_each(script_struct, [&](LuaCustomProperty const& lua_property) {
709:             handle_property(lua_property.m_property.get(), to_utf8_string(lua_property.m_name));
710:             return true;
711:         });
712: 
713:         for (Unreal::FProperty* field : Unreal::TFieldRange<Unreal::FProperty>(script_struct, Unreal::EFieldIterationFlags::IncludeSuper | Unreal::EFieldIterationFlags::IncludeDeprecated))
714:         {
715:             handle_property(field, to_utf8_string(field->GetName()));
716:         }
717:     }
```

### P3

[LuaUObject.cpp:816](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:816)

```text
816:     auto push_structproperty(const PusherParams& params) -> void
817:     {
818:         // params.base = Base of the object that this struct belongs in
819:         // params.data = Start of the struct
820:         // params.property = The corresponding FStructProperty
821:         auto* struct_property = Unreal::CastField<Unreal::FStructProperty>(params.property);
822:         Unreal::UScriptStruct* script_struct = struct_property->GetStruct();
823: 
824:         auto iterate_struct_and_turn_into_lua_table = [&](const LuaMadeSimple::Lua& lua, void* data_ptr) {
825:             convert_struct_to_lua_table(
826:                     lua,
827:                     script_struct,
828:                     data_ptr,
829:                     params.create_new_if_get_non_trivial_local,
830:                     params.base
831:                     );
832:         };
833: 
834:         auto lua_table_to_memory = [&]() {
835:             convert_lua_table_to_struct(
836:                     params.lua,
837:                     script_struct,
838:                     params.data,
839:                     params.stored_at_index,
840:                     params.base
841:                     );
842:             params.lua.discard_value(params.stored_at_index);
843:         };
844: 
845:         auto lua_to_memory = [&]() {
846:             if (params.lua.is_userdata(params.stored_at_index))
847:             {
848:                 // StructData as userdata
849:                 auto& lua_scriptstruct = params.lua.get_userdata<UScriptStruct>().get_local_cpp_object();
850:                 if (lua_scriptstruct.script_struct == script_struct)
851:                 {
852:                     struct_property->CopyCompleteValue(params.data, lua_scriptstruct.get_data_ptr());
853:                 }
854:                 else
855:                 {
856:                     params.throw_error("push_structproperty::lua_to_memory",
857:                                        fmt::format("Can't copy struct of type {} into {}",
858:                                                    to_string(lua_scriptstruct.script_struct->GetName()),
859:                                                    to_string(script_struct->GetName())));
860:                 }
861:             }
862:             else if (params.lua.is_table(params.stored_at_index))
863:             {
864:                 // StructData as table
865:                 lua_table_to_memory();
866:             }
867:             else if (params.lua.is_nil(params.stored_at_index))
868:             {
869:                 params.lua.discard_value(params.stored_at_index);
870:             }
871:             else
872:             {
873:                 params.throw_error("push_structproperty::lua_to_memory", "Parameter must be of type 'StructProperty' or table");
874:             }
875:         };
876: 
877:         auto property_value = ScriptStructWrapper{struct_property->GetStruct(), params.data, struct_property};
878: 
879:         switch (params.operation)
880:         {
881:         case Operation::Get:
882:             UScriptStruct::construct(params.lua, property_value);
883:             return;
884:         case Operation::GetNonTrivialLocal:
885:             iterate_struct_and_turn_into_lua_table(params.lua, params.data);
886:             return;
887:         case Operation::Set:
888:             lua_to_memory();
889:             return;
890:         case Operation::GetParam:
891:             LocalUnrealParam<ScriptStructWrapper>::construct(params.lua, property_value, params.base, params.property);
892:             return;
893:         default:
894:             params.throw_error("push_structproperty", "Unhandled Operation");
895:             break;
896:         }
897: 
898:         params.throw_error("push_structproperty", "Operation not supported");
```

### P4

[LuaUObject.cpp:1513](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:1513)

```text
1513:     auto push_floatproperty(const PusherParams& params) -> void
1514:     {
1515:         float* float_ptr = static_cast<float*>(params.data);
1516:         if (!float_ptr)
1517:         {
1518:             params.throw_error("push_floatproperty", "data pointer is nullptr");
1519:         }
1520: 
1521:         switch (params.operation)
1522:         {
1523:         case Operation::GetNonTrivialLocal:
1524:         case Operation::Get:
1525:             params.lua.set_float(*float_ptr);
1526:             return;
1527:         case Operation::Set:
1528:             *float_ptr = params.lua.get_float(params.stored_at_index);
1529:             return;
1530:         case Operation::GetParam:
1531:             RemoteUnrealParam::construct(params.lua, float_ptr, params.base, params.property);
1532:             return;
1533:         default:
1534:             params.throw_error("push_floatproperty", "Unhandled Operation");
1535:             break;
1536:         }
1537: 
1538:         params.throw_error("push_floatproperty", "Operation not supported");
1539:     }
1540: 
1541:     auto push_doubleproperty(const PusherParams& params) -> void
1542:     {
1543:         double* double_ptr = static_cast<double*>(params.data);
1544:         if (!double_ptr)
1545:         {
1546:             params.throw_error("push_doubleproperty", "data pointer is nullptr");
1547:         }
1548: 
1549:         switch (params.operation)
1550:         {
1551:         case Operation::GetNonTrivialLocal:
1552:         case Operation::Get:
1553:             params.lua.set_number(*double_ptr);
1554:             return;
1555:         case Operation::Set:
1556:             *double_ptr = params.lua.get_number(params.stored_at_index);
1557:             return;
1558:         case Operation::GetParam:
1559:             RemoteUnrealParam::construct(params.lua, double_ptr, params.base, params.property);
1560:             return;
1561:         default:
1562:             params.throw_error("push_doubleproperty", "Unhandled Operation");
1563:             break;
1564:         }
1565: 
1566:         params.throw_error("push_doubleproperty", "Operation not supported");
1567:     }
1568: 
1569:     auto push_boolproperty(const PusherParams& params) -> void
1570:     {
1571:         uint8_t* bitfield_ptr = static_cast<uint8_t*>(params.data);
1572:         if (!bitfield_ptr)
1573:         {
1574:             params.throw_error("push_boolproperty", "data pointer is nullptr");
1575:         }
1576: 
1577:         Unreal::FBoolProperty* bp = static_cast<Unreal::FBoolProperty*>(params.property);
1578:         uint8_t field_mask = bp->GetFieldMask();
1579:         uint8_t byte_offset = bp->GetByteOffset();
1580:         uint8_t* byte_value = bitfield_ptr + byte_offset;
1581: 
1582:         switch (params.operation)
1583:         {
1584:         case Operation::Get:
1585:             params.lua.set_bool(!!(*byte_value & field_mask));
1586:             return;
1587:         case Operation::GetNonTrivialLocal:
1588:             params.lua.set_bool(*static_cast<bool*>(params.data));
1589:             return;
1590:         case Operation::Set: {
1591:             uint8_t byte_mask = bp->GetByteMask();
1592: 
1593:             *byte_value = ((*byte_value) & ~field_mask) | (params.lua.get_bool(params.stored_at_index) ? byte_mask : 0);
1594:             return;
1595:         }
1596:         case Operation::GetParam:
1597:             RemoteUnrealParam::construct(params.lua, bitfield_ptr, params.base, params.property);
1598:             return;
1599:         default:
1600:             params.throw_error("push_boolproperty", "Unhandled Operation");
1601:             break;
1602:         }
1603: 
1604:         params.throw_error("push_boolproperty", "Operation not supported");
1605:     }
1606: 
1607:     auto push_enumproperty(const PusherParams& params) -> void
1608:     {
1609:         Unreal::UEnum* enum_ptr = static_cast<Unreal::FEnumProperty*>(params.property)->GetEnum();
1610:         if (!enum_ptr)
1611:         {
1612:             params.throw_error("push_enumproperty", "data pointer is nullptr");
1613:         }
1614: 
1615:         switch (params.operation)
1616:         {
1617:         case Operation::GetNonTrivialLocal:
1618:         case Operation::Get: {
1619:             // Make global table to store enum name/value pairs -> START
1620:             auto table = params.lua.prepare_new_table();
1621: 
1622:             std::string prop_name = to_string(params.property->GetName());
1623: 
1624:             for (const auto& elem : enum_ptr->ForEachName())
1625:             {
1626:                 std::string elem_name = to_string(elem.Key.ToString());
1627:                 table.add_pair(elem_name.c_str(), elem.Value);
1628:             }
1629: 
1630:             // TODO: Optimize this... it'll probably do a dynamic allocation here in order to fit the new beginning of the string
1631:             prop_name.insert(0, "Enum_");
1632:             table.make_global(prop_name);
1633:             // Make global table to store enum name/value pairs -> END
1634: 
1635:             // Push the enum value to the Lua stack based on underlying type size and signedness
1636:             auto underlying_prop = static_cast<Unreal::FEnumProperty*>(params.property)->GetUnderlyingProperty();
1637:             auto cast_flags = underlying_prop->GetClass().GetClassCastFlags();
1638:             constexpr auto unsigned_flags = Unreal::CASTCLASS_FByteProperty | Unreal::CASTCLASS_FUInt16Property |
1639:                                             Unreal::CASTCLASS_FUInt32Property | Unreal::CASTCLASS_FUInt64Property;
1640:             bool is_unsigned = (cast_flags & unsigned_flags) != 0;
1641: 
1642:             switch (underlying_prop->GetElementSize())
1643:             {
1644:             case 1:
1645:                 params.lua.set_integer(is_unsigned ? static_cast<int64_t>(*static_cast<uint8_t*>(params.data))
1646:                                                    : static_cast<int64_t>(*static_cast<int8_t*>(params.data)));
1647:                 break;
1648:             case 2:
1649:                 params.lua.set_integer(is_unsigned ? static_cast<int64_t>(*static_cast<uint16_t*>(params.data))
1650:                                                    : static_cast<int64_t>(*static_cast<int16_t*>(params.data)));
1651:                 break;
1652:             case 4:
1653:                 params.lua.set_integer(is_unsigned ? static_cast<int64_t>(*static_cast<uint32_t*>(params.data))
1654:                                                    : static_cast<int64_t>(*static_cast<int32_t*>(params.data)));
1655:                 break;
1656:             case 8:
1657:                 params.lua.set_integer(*static_cast<int64_t*>(params.data));
1658:                 break;
1659:             default:
1660:                 params.throw_error("push_enumproperty", "Unsupported enum underlying type size");
1661:                 break;
1662:             }
1663: 
1664:             return;
1665:         }
1666:         case Operation::Set: {
1667:             // For Set, signedness doesn't matter - bit pattern is preserved
1668:             auto underlying_prop = static_cast<Unreal::FEnumProperty*>(params.property)->GetUnderlyingProperty();
1669:             int64_t value = params.lua.get_integer(params.stored_at_index);
1670:             switch (underlying_prop->GetElementSize())
1671:             {
1672:             case 1:
1673:                 *static_cast<uint8_t*>(params.data) = static_cast<uint8_t>(value);
1674:                 break;
1675:             case 2:
1676:                 *static_cast<uint16_t*>(params.data) = static_cast<uint16_t>(value);
1677:                 break;
1678:             case 4:
1679:                 *static_cast<uint32_t*>(params.data) = static_cast<uint32_t>(value);
1680:                 break;
1681:             case 8:
1682:                 *static_cast<uint64_t*>(params.data) = static_cast<uint64_t>(value);
1683:                 break;
1684:             default:
1685:                 params.throw_error("push_enumproperty", "Unsupported enum underlying type size");
1686:                 break;
1687:             }
1688:             return;
1689:         }
1690:         case Operation::GetParam:
1691:             // TODO: Verify that this works
1692:             RemoteUnrealParam::construct(params.lua, params.data, params.base, params.property);
1693:             return;
1694:         default:
1695:             params.throw_error("push_enumproperty", "Unhandled Operation");
1696:             break;
1697:         }
1698: 
1699:         params.throw_error("push_enumproperty", "Operation not supported");
```

### P5

[LuaUObject.cpp:1793](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:1793)

```text
1793:     auto push_textproperty(const PusherParams& params) -> void
1794:     {
1795:         Unreal::FText* text = static_cast<Unreal::FText*>(params.data);
1796:         if (!text)
1797:         {
1798:             params.throw_error("push_textproperty", "data pointer is nullptr");
1799:         }
1800: 
1801:         switch (params.operation)
1802:         {
1803:         case Operation::GetNonTrivialLocal:
1804:         case Operation::Get:
1805:             LuaType::FText::construct(params.lua, *text);
1806:             return;
1807:         case Operation::Set: {
1808:             auto& lua_other_object = params.lua.get_userdata<LuaType::FText>(params.stored_at_index);
1809:             text->SetString(std::move(lua_other_object.get_local_cpp_object().ToFString()));
1810:             return;
1811:         }
1812:         case Operation::GetParam:
1813:             RemoteUnrealParam::construct(params.lua, params.data, params.base, params.property);
1814:             return;
1815:         default:
1816:             params.throw_error("push_textproperty", "Unhandled Operation");
1817:             break;
1818:         }
1819: 
1820:         params.throw_error("push_textproperty", "Operation not supported");
1821:     }
```

### P6

[LuaUObject.cpp:2303](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp:2303)

```text
2303:     auto handle_unreal_property_value(
2304:             const Operation operation, const LuaMadeSimple::Lua& lua, Unreal::UObject* base, Unreal::FName property_name, Unreal::FField* field) -> void
2305:     {
2306:         // In UE versions prior to 4.25, UFunctions can be found with 'find_property', and thus 'property' will not be nullptr
2307:         // So you must take that into account when checking if the Lua script is trying to call a UFunction
2308:         if (!field || field->GetClass().GetFName() == Unreal::GFunctionName)
2309:         {
2310:             Unreal::UFunction* func{};
2311: 
2312:             // We can take a shortcut if property is non-nullptr
2313:             // It means that the UFunction was found and is stored in 'property', so we don't need to do anything to find it
2314:             if (!field && property_name != Unreal::FName(0u, 0u))
2315:             {
2316:                 func = base->GetFunctionByNameInChain(property_name);
2317:             }
2318:             else
2319:             {
2320:                 // TODO: Figure out a better way to do this, ideally, there shouldn't be a need to bit_cast here
2321:                 func = std::bit_cast<Unreal::UFunction*>(field);
2322:             }
2323: 
2324:             if (func)
2325:             {
2326:                 push_functionproperty(FunctionPusherParams{.lua = lua, .base = base, .function = func});
2327:             }
2328:             else
2329:             {
2330:                 // Construct an empty object, this is for compatibility reasons because people may have written code that takes for granted that
2331:                 // all objects are userdata even invalid fields and functions being accessed from a valid UObject.
2332:                 LuaMadeSimple::Type::RemoteObject<Unreal::UObject>::construct(lua, static_cast<Unreal::UObject*>(LuaMadeSimple::Type::special_invalid_ptr()));
2333:             }
2334: 
2335:             return;
2336:         }
2337: 
2338:         // Casting to XProperty here so that we can get access to property members
2339:         // It needed to be FField above so that it could be converted to UFunction without force
2340:         // This is because UFunction & XProperty both inherit from XField, but UFunction doesn't inherit from XProperty
2341:         Unreal::FProperty* property = static_cast<Unreal::FProperty*>(field);
2342: 
2343:         Unreal::FName property_type = property->GetClass().GetFName();
2344:         int32_t name_comparison_index = property_type.GetComparisonIndex();
2345: 
2346:         if (StaticState::m_property_value_pushers.contains(name_comparison_index))
2347:         {
2348:             void* data = static_cast<uint8_t*>(static_cast<void*>(base)) + property->GetOffset_Internal();
2349: 
2350:             const PusherParams pusher_params{.operation = operation, .lua = lua, .base = base, .data = data, .property = property};
2351:             StaticState::m_property_value_pushers[name_comparison_index](pusher_params);
2352:         }
2353:         else
2354:         {
2355:             // We can either throw an error and kill the execution
2356:             /**/
2357:             std::string property_type_name = to_string(property_type.ToString());
2358:             lua.throw_error(fmt::format(
2359:                     "[handle_unreal_property_value] Tried accessing unreal property without a registered handler. Property type '{}' not supported.",
2360:                     property_type_name));
2361:             //*/
2362: 
2363:             // Or we can treat unhandled property types as some sort of generic type
2364:             // For example, we might want to pass the unhandled property value to some sort of low-level Lua API
2365:             // Then the Lua script can deal with figuring out how to handle the type
2366:             /*
2367:             push_unhandledproperty(lua, base);
2368:             //*/
2369:         }
2370:     }
```

### B1

[LuaUFunction.hpp:14](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaType/LuaUFunction.hpp:14)

```text
14: namespace RC::LuaType
15: {
16:     struct DynamicUnrealFunctionData
17:     {
18:         uint8_t data[0x200]{};
19:     };
```

### T1

[LuaFText.cpp:52](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaFText.cpp:52)

```text
52: #1: FText(string Text)
53: )"};
54: 
55:             if (lua.is_userdata())
56:             {
57:                 // Discard the userdata since it's unwanted data
58:                 lua.discard_value();
59:             }
60: 
61:             StringType text_string{};
62:             if (lua.is_string())
63:             {
64:                 text_string = ensure_str(lua.get_string());
65:             }
66:             else
67:             {
68:                 lua.throw_error(error_overload_not_found);
69:             }
70: 
71:             LuaType::FText::construct(lua, Unreal::FText(text_string));
72: 
73:             return 1;
74:         });
75: 
76:         base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Equal, []([[maybe_unused]] const LuaMadeSimple::Lua& lua) -> int {
77:             if (!lua.is_userdata(1) || !lua.is_userdata(2))
78:             {
79:                 lua.throw_error("FText __eq metamethod called but there was not two userdata to compare");
80:             }
81: 
82:             auto text_a = lua.get_userdata<LuaType::FText>();
83:             auto text_b = lua.get_userdata<LuaType::FText>();
84:             // FText objects are equal if their string representations are equal
85:             return text_a.get_local_cpp_object().ToString() == text_b.get_local_cpp_object().ToString();
86:         });
87:     }
88: 
89:     template <LuaMadeSimple::Type::IsFinal is_final>
90:     auto FText::setup_member_functions(const LuaMadeSimple::Lua::Table& table) -> void
91:     {
92:         table.add_pair("ToString", [](const LuaMadeSimple::Lua& lua) -> int {
```

### T2

[TypeChecker.cpp:528](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:528)

```text
528:                     if (type_name == FName(STR("EnumProperty"), FNAME_Find))
529:                     {
530:                         FEnumProperty::StaticClassStorage = ffield_class;
531:                         FEnumProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
532:                     }
533:                     if (type_name == FName(STR("TextProperty"), FNAME_Find))
534:                     {
535:                         FTextProperty::StaticClassStorage = ffield_class;
536:                         FTextProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
537:                         FText::StaticSize_Private = static_cast<FTextProperty*>(field)->GetSize();
538:                         FText::ValueOpsProperty = static_cast<FProperty*>(field);
539:                     }
540:                     if (type_name == FName(STR("StrProperty"), FNAME_Find))
541:                     {
```

### T3

[TypeChecker.cpp:626](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:626)

```text
626:             // TODO: Maybe replace all of the calls to 'find_all_property_types' with a GUObjectArray iteration with an early exit once everything's been found ?
627: 
628:             // Bool, Enum, Byte, Struct, Float, Object, Name, Int, Array
629:             // WeakObject, MulticastSparseDelegate
630:             find_all_property_types(STR("/Script/Engine.Actor"));
631:             // Class
632:             find_all_property_types(STR("/Script/Engine.Pawn"));
633:             // Str, Map
634:             find_all_property_types(STR("/Script/Engine.NetDriver"));
635:             // Double
636:             find_all_property_types(STR("/Script/Engine.NetConnection"));
637:             // Text
638:             find_all_property_types(STR("/Script/Engine.GameModeBase"));
639:             // Int8, LazyObject
640:             find_all_property_types(STR("/Script/Landscape.LandscapeComponent"));
641:             find_all_property_types(STR("/Script/VariantManagerContent.VariantObjectBinding"));
642:             // UInt16, UInt32, UInt64, Int8, Int16, Int64
643:             find_all_property_types(STR("/Script/Engine.IntSerialization"));
644:             // Set, MulticastInlineDelegate, Delegate
645:             find_all_property_types(STR("/Script/Engine.AudioComponent"));
646:             // MulticastInlineDelegate, Interface
647:             find_all_property_types(STR("/Script/MovieScene.MovieSceneSequencePlayer"));
648:             // Delegate
649:             find_all_property_types(STR("/Script/UMG.Widget"));
650:             // SoftObject
651:             find_all_property_types(STR("/Script/Engine.LevelStreaming"));
652:             // SoftClass
653:             find_all_property_types(STR("/Script/Engine.PrimaryAssetTypeInfo"));
654:             // FieldPath
655:             find_all_property_types(STR("/Script/Engine.InterpTrackInstProperty"));
656:             find_all_property_types(STR("/Script/MovieSceneTracks.MovieSceneEventPtrs"));
657:             // TOptional
658:             if (Version::IsAtLeast(5, 4))
659:             {
660:                 find_all_property_types(STR("/Script/Engine.WorldPartitionRuntimeCellData"));
661:             }
662:             // FUtf8StrProperty
663:             if (Version::IsAtLeast(5, 6))
664:             {
665:                 find_all_property_types(STR("/Script/CoreUObject.VerseEnum"));
666:             }
667:             // FAnsiStrProperty
668:             if (Version::IsAtLeast(5, 7))
669:             {
670:                 find_all_property_types(STR("/Script/CoreUObject.VerseClass"));
671:             }
```

### T4

[TypeChecker.cpp:790](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/TypeChecker.cpp:790)

```text
790:         if (FText::StaticSize() == 0)
791:         {
792:             FText::StaticSize_Private = sizeof(FText);
793:             Output::send<LogLevel::Warning>(STR("Was unable to detect FText size, using default: 0x{:X} bytes.\n"), FText::StaticSize());
794:         }
795:         else
796:         {
797:             Output::send(STR("FText size detected as 0x{:X} bytes.\n"), FText::StaticSize());
798:         }
```

### T5

[FText.cpp:12](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/FText.cpp:12)

```text
12:     int32 FText::StaticSize_Private{-1};
13:     FProperty* FText::ValueOpsProperty{nullptr};
14: 
15:     int32 FText::StaticSize()
16:     {
17:         if (StaticSize_Private < 0)
18:         {
19:             throw std::runtime_error{"[FText::StaticSize] StaticSize_Private is not valid"};
20:         }
21:         else
22:         {
23:             return StaticSize_Private;
24:         }
25:     }
26: 
27:     auto FText::ActiveSize() -> int32
28:     {
29:         if (StaticSize_Private > 0 && StaticSize_Private <= MaxStorageSize)
30:         {
31:             return StaticSize_Private;
32:         }
33:         return MaxStorageSize;
34:     }
35: 
36:     // The engine's value operations construct/copy/destroy a full-size engine FText in place,
37:     // so they may only be used when the engine's FText actually fits in our storage. They also
38:     // require a valid source: a zero-initialized FText is not a valid engine FText (its shared
39:     // reference is null), and the engine's copy path does not expect one.
40:     auto FText::CanUseValueOps() -> bool
41:     {
42:         return ValueOpsProperty && StaticSize_Private > 0 && StaticSize_Private <= MaxStorageSize;
43:     }
44: 
45:     auto FText::GetTextData() const -> ITextData*
46:     {
47:         return *std::bit_cast<ITextData* const*>(&Storage[0]);
48:     }
49: 
50:     auto FText::SetTextData(ITextData* TextData) -> void
51:     {
52:         *std::bit_cast<ITextData**>(&Storage[0]) = TextData;
53:     }
54: 
55:     auto FText::GetFlags() const -> uint32
56:     {
57:         // Flags are the last 4-byte field, 8 bytes before the end in every known layout
58:         // (0x20 in the 0x28 layout, 0x10 in the 0x18 layout, 0x8 in the 0x10 layout)
59:         return *std::bit_cast<const uint32*>(&Storage[ActiveSize() - 8]);
60:     }
61: 
62:     FText::FText(const FText& Other)
63:     {
64:         if (CanUseValueOps() && Other.GetTextData())
65:         {
66:             // Engine-correct copy: takes a reference using whatever refcounting scheme this version has.
67:             // The zero-initialized destination is safe for the engine's assignment in every era.
68:             ValueOpsProperty->CopyCompleteValue(Storage, Other.Storage);
69:         }
70:         else
71:         {
72:             // Empty source, or the property system isn't usable: borrowed bitwise copy
73:             std::memcpy(Storage, Other.Storage, ActiveSize());
74:         }
75:     }
76: 
77:     FText& FText::operator=(const FText& Other)
78:     {
79:         if (this == &Other)
80:         {
81:             return *this;
82:         }
83: 
84:         if (CanUseValueOps() && Other.GetTextData())
85:         {
86:             ValueOpsProperty->CopyCompleteValue(Storage, Other.Storage);
87:         }
88:         else
89:         {
90:             if (CanUseValueOps() && GetTextData())
91:             {
92:                 ValueOpsProperty->DestroyValue(Storage);
93:             }
94:             std::memset(Storage, 0, ActiveSize());
95:             std::memcpy(Storage, Other.Storage, ActiveSize());
96:         }
97:         return *this;
98:     }
99: 
100:     FText::FText(FText&& Other) noexcept
101:     {
102:         std::memcpy(Storage, Other.Storage, ActiveSize());
103:         std::memset(Other.Storage, 0, ActiveSize());
104:     }
105: 
106:     FText& FText::operator=(FText&& Other) noexcept
107:     {
108:         if (this == &Other)
109:         {
110:             return *this;
111:         }
112: 
113:         if (CanUseValueOps() && GetTextData())
114:         {
115:             ValueOpsProperty->DestroyValue(Storage);
116:         }
117:         std::memcpy(Storage, Other.Storage, ActiveSize());
118:         std::memset(Other.Storage, 0, ActiveSize());
119:         return *this;
120:     }
121: 
122:     FText::~FText()
123:     {
124:         // Objects borrow-copied before the ops were resolved are also released here; that window
125:         // closes before any engine data exists, so in practice everything with live data owns it
126:         if (CanUseValueOps() && GetTextData())
127:         {
128:             ValueOpsProperty->DestroyValue(Storage);
129:             std::memset(Storage, 0, ActiveSize());
130:         }
131:     }
132: 
133:     auto FText::StealFromBuffer(void* SourceBuffer) -> FText
134:     {
135:         FText Result{};
136:         const auto Size = ActiveSize();
137:         std::memcpy(Result.Storage, SourceBuffer, Size);
138:         std::memset(SourceBuffer, 0, Size);
139:         return Result;
140:     }
141: 
142:     auto FText::CopyBorrowedTo(void* DestBuffer) const -> void
143:     {
144:         std::memcpy(DestBuffer, Storage, ActiveSize());
145:     }
146: 
147:     auto FText::construct_with_string(const FString&& String) -> void
148:     {
149:         *this = UKismetTextLibrary::Conv_StringToText(std::forward<const FString>(String));
150:     }
151: 
152:     FText::FText(const CharType* String)
153:     {
154:         FString Str = FString(FromCharTypePtr<TCHAR>(String));
155:         construct_with_string(std::forward<const FString>(Str));
156:     }
157: 
158:     auto FText::ToFString() const -> FString
159:     {
160:         if (!GetTextData())
161:         {
162:             return {};
163:         }
164:         return UKismetTextLibrary::Conv_TextToString(*this);
165:     }
```

### T6

[FText.hpp:16](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FText.hpp:16)

```text
16:     // FText's engine layout varies by version: TSharedRef<ITextData> + flags (0x18 total)
17:     // before 5.4, TRefCountPtr<ITextData> + flags (0x10 total) from 5.4. Storage below covers
18:     // the largest known layout and StaticSize() decides how many bytes are live. Ownership
19:     // (copy/destroy) is routed through the engine's own FTextProperty value operations, so the
20:     // refcounting scheme of the running engine is used with no compile-time branching. Both
21:     // layouts store the ITextData pointer first, so reads need no version dispatch at all.
22:     class RC_UE_API FText
23:     {
24:     public:
25:         // These private members are public because 'friend' declarations don't work with concepts.
26:         // We use a concept in TypeChecker to determine whether these variables exist.
27:         static int32 StaticSize_Private;
28:         static int32 StaticSize();
29: 
30:         // FTextProperty used for engine-correct copy/destroy; set during TypeChecker initialization.
31:         // Until it is set, copies fall back to borrowed bitwise copies (the old behavior).
32:         static FProperty* ValueOpsProperty;
33: 
34:         // Largest known engine layout:
35:         //   4.7 to 4.10: TSharedRef<FString> DisplayString; TSharedPtr<FTextHistory> History; uint32 Flags  (0x28)
36:         //   4.11 to 5.3: TSharedRef<ITextData> TextData; uint32 Flags                                        (0x18)
37:         //   5.4+:        TRefCountPtr<ITextData> TextData; uint32 Flags                                      (0x10)
38:         // Storage must never be smaller than the running engine's FText, because the engine's
39:         // own value operations write StaticSize() bytes into it.
40:         static constexpr int32 MaxStorageSize = 0x30;
41: 
42:       private:
43:         alignas(8) uint8 Storage[MaxStorageSize]{};
44: 
45:         // Live extent of Storage: StaticSize when known, the full buffer otherwise
46:         static auto ActiveSize() -> int32;
47: 
48:         // Whether the engine's FText value operations are usable (resolved, and the engine's
49:         // FText fits in Storage so the engine cannot write past it)
50:         static auto CanUseValueOps() -> bool;
51: 
52:       private:
53:         auto construct_with_string(const FString&& String) -> void;
54: 
55:       public:
56:         FText() = default;
57: 
58:         explicit FText(const FString&& String)
59:         {
60:             construct_with_string(std::forward<const FString>(String));
61:         }
62: 
63:         explicit FText(const CharType* String);
64: 
65:         explicit FText(StringViewType String) : FText(String.data())
66:         {
67:         }
68: 
69:         FText(const FText& Other);
70:         FText& operator=(const FText& Other);
71:         FText(FText&& Other) noexcept;
72:         FText& operator=(FText&& Other) noexcept;
73:         ~FText();
74: 
75:         auto GetTextData() const -> ITextData*;
76:         // Raw pointer write with no refcounting; prefer assignment from another FText
77:         auto SetTextData(ITextData* TextData) -> void;
78:         auto GetFlags() const -> uint32;
79: 
80:         // Bitwise-takes ownership of an FText the engine wrote into a param or return buffer,
81:         // zeroing the buffer so nothing double-releases when the buffer is discarded
82:         static auto StealFromBuffer(void* SourceBuffer) -> FText;
83: 
84:         // Bitwise borrow into a param buffer for the duration of a call; no ownership moves
85:         auto CopyBorrowedTo(void* DestBuffer) const -> void;
```

### T7

[UKismetTextLibrary.cpp:7](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/UKismetTextLibrary.cpp:7)

```text
7:     FString UKismetTextLibrary::Conv_TextToString(const FText& InText)
8:     {
9:         UE_BEGIN_NATIVE_FUNCTION_BODY("/Script/Engine.KismetTextLibrary:Conv_TextToString")
10:         UE_SET_STATIC_SELF("/Script/Engine.Default__KismetTextLibrary")
11: 
12:         // Borrow the text into the param buffer for the duration of the call; the engine only
13:         // reads it, so no reference needs to be taken or released
14:         UE_COPY_PROPERTY_INTERNAL(InText)
15:         InText.CopyBorrowedTo(&ParamData[InTextProperty->GetOffset_Internal()]);
16: 
17:         UE_CALL_STATIC_FUNCTION()
18: 
19:         UE_RETURN_PROPERTY(FString)
20:     }
21: 
22:     FText UKismetTextLibrary::Conv_StringToText(const FString& InString)
23:     {
24:         UE_BEGIN_NATIVE_FUNCTION_BODY("/Script/Engine.KismetTextLibrary:Conv_StringToText")
25:         UE_SET_STATIC_SELF("/Script/Engine.Default__KismetTextLibrary")
26: 
27:         UE_COPY_PROPERTY(InString, FString)
28: 
29:         UE_CALL_STATIC_FUNCTION()
30: 
31:         // The engine constructed an owning FText into the return slot; take that reference over
32:         // instead of copying it, so freeing the param buffer cannot leak or double-release it
33:         UE_RETURN_PROPERTY_INTERNAL()
34:         auto RetValue = FText::StealFromBuffer(&ParamData[ReturnProperty->GetOffset_Internal()]);
35:         UE_FREE_PARAM_STRUCT_INTERNAL()
36:         return RetValue;
37:     }
```

### T8

[BPMacros.hpp:16](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/BPMacros.hpp:16)

```text
16: #if defined(__PROSPERO__)
17: namespace RC::Unreal::Private
18: {
19:     // Payload-owned temporary storage. Reflected FString/TArray ownership stays in the engine.
20:     using FPayloadParameterFrame = std::unique_ptr<uint8, decltype(&::free)>;
21: 
22:     inline FPayloadParameterFrame MakePayloadParameterFrame(uint16 Size)
23:     {
24:         // Zero-parameter void calls are valid: retain a non-null private byte for the call.
25:         FPayloadParameterFrame Frame(static_cast<uint8*>(::malloc(Size ? Size : 1)), &::free);
26:         if (!Frame)
27:         {
28:             throw std::bad_alloc{};
29:         }
30:         return Frame;
31:     }
32: }
33: 
34: #define UE_ALLOC_PARAM_STRUCT_INTERNAL() \
35: auto ParamOwner = RC::Unreal::Private::MakePayloadParameterFrame(ParamStructSize); \
36: auto ParamData = ParamOwner.get();
37: 
38: #define UE_FREE_PARAM_STRUCT_INTERNAL() \
39: ParamOwner.reset();
40: #else
41: #define UE_ALLOC_PARAM_STRUCT_INTERNAL() \
42: auto ParamData = static_cast<uint8*>(_malloca(ParamStructSize));
43: 
44: #define UE_FREE_PARAM_STRUCT_INTERNAL() \
45: _freea(ParamData);
46: #endif
47: 
48: #define UE_BEGIN_FUNCTION_BODY_INTERNAL(FunctionPath) \
49: if (!Function) \
50: { \
51:     throw std::runtime_error{"Unable to find '" FunctionPath "'"}; \
52: } \
53: static auto ParamStructSize = Function->GetParmsSize(); \
54: UE_ALLOC_PARAM_STRUCT_INTERNAL() \
55: FMemory::Memzero(ParamData, ParamStructSize);
56: 
57: #define UE_END_VOID_FUNCTION() \
58: UE_FREE_PARAM_STRUCT_INTERNAL()
59: 
60: #define UE_BEGIN_SCRIPT_FUNCTION_BODY(FunctionPath) \
61: auto Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR(FunctionPath)); \
62: UE_BEGIN_FUNCTION_BODY_INTERNAL(FunctionPath)
63: 
64: #define UE_BEGIN_NATIVE_FUNCTION_BODY(FunctionPath) \
65: static auto Function = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, STR(FunctionPath)); \
66: UE_BEGIN_FUNCTION_BODY_INTERNAL(FunctionPath)
67: 
68: #define UE_SET_STATIC_SELF(ObjectPath) \
69: static auto StaticSelf = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR(ObjectPath));
70: 
71: #define UE_COPY_PROPERTY_INTERNAL(PropertyName) \
72: auto PropertyName##Property = Function->FindProperty(FName(STR(#PropertyName), FNAME_Find)); \
73: if (!PropertyName##Property) { throw std::runtime_error{"Property not found: '"#PropertyName"'"}; }
74: 
75: #define UE_COPY_PROPERTY(PropertyName, TypeName) \
76: UE_COPY_PROPERTY_INTERNAL(PropertyName) \
77: *std::bit_cast<TypeName*>(&ParamData[PropertyName##Property->GetOffset_Internal()]) = static_cast<TypeName>(PropertyName);
78: 
79: #define UE_COPY_STRUCT_PROPERTY_BEGIN(PropertyName) \
80: UE_COPY_PROPERTY_INTERNAL(PropertyName) \
81: auto PropertyName##Struct = static_cast<FStructProperty*>(PropertyName##Property)->GetStruct();
82: 
83: #define UE_COPY_STRUCT_INNER_PROPERTY(StructName, PropertyName, TypeName, CXXValue) \
84: auto StructName##PropertyName##Property = StructName##Struct->FindProperty(FName(STR(#PropertyName), FNAME_Find)); \
85: *std::bit_cast<TypeName*>(&ParamData[StructName##Property->GetOffset_Internal() + StructName##PropertyName##Property->GetOffset_Internal()]) = static_cast<TypeName>(CXXValue);
86: 
87: #define UE_COPY_OUT_PROPERTY(PropertyName, TypeName) \
88: auto Out##PropertyName##Property = Function->FindProperty(FName(STR(#PropertyName), FNAME_Find)); \
89: if (!Out##PropertyName##Property) { throw std::runtime_error{"Out property not found: '"#PropertyName"'"}; } \
90: PropertyName = *std::bit_cast<TypeName*>(&ParamData[Out##PropertyName##Property->GetOffset_Internal()]);
91: 
92: #define UE_COPY_PROPERTY_CUSTOM_INTERNAL(BPPropertyName, CXXPropertyName) \
93: auto CXXPropertyName##Property = Function->FindProperty(FName(STR(#BPPropertyName), FNAME_Find)); \
94: if (!CXXPropertyName##Property) { throw std::runtime_error{"Property not found: '"#BPPropertyName"'"}; }
95: 
96: #define UE_COPY_PROPERTY_CUSTOM(BPPropertyName, CXXPropertyName, TypeName) \
97: UE_COPY_PROPERTY_CUSTOM_INTERNAL(BPPropertyName, CXXPropertyName) \
98: *std::bit_cast<TypeName*>(&ParamData[CXXPropertyName##Property->GetOffset_Internal()]) = static_cast<TypeName>(CXXPropertyName);
99: 
100: #define UE_COPY_STRUCT_PROPERTY_CUSTOM_BEGIN(BPPropertyName, CXXPropertyName) \
101: UE_COPY_PROPERTY_CUSTOM_INTERNAL(BPPropertyName, CXXPropertyName) \
102: auto CXXPropertyName##Struct = static_cast<FStructProperty*>(CXXPropertyName##Property)->GetStruct();
103: 
104: #define UE_COPY_STRUCT_INNER_PROPERTY_CUSTOM(StructName, BPPropertyName, CXXPropertyName, TypeName, CXXValue) \
105: auto StructName##CXXPropertyName##Property = StructName##Struct->FindProperty(FName(STR(#BPPropertyName), FNAME_Find)); \
106: *std::bit_cast<TypeName*>(&ParamData[StructName##Property->GetOffset_Internal() + StructName##CXXPropertyName##Property->GetOffset_Internal()]) = static_cast<TypeName>(CXXValue);
107: 
108: #define UE_COPY_OUT_PROPERTY_CUSTOM(BPPropertyName, CXXPropertyName, TypeName) \
109: auto OutPropertyName##CXXPropertyName = Function->FindProperty(FName(STR(#BPPropertyName), FNAME_Find)); \
110: if (!OutPropertyName##CXXPropertyName) { throw std::runtime_error{"Out property not found: '"#BPPropertyName"'"}; } \
111: CXXPropertyName = *std::bit_cast<TypeName*>(&ParamData[OutPropertyName##CXXPropertyName->GetOffset_Internal()]);
112: 
113: #define UE_CALL_FUNCTION() \
114: ProcessEvent(Function, ParamData);
115: 
116: #define UE_CALL_STATIC_FUNCTION() \
117: StaticSelf->ProcessEvent(Function, ParamData);
118: 
119: #define UE_RETURN_PROPERTY_INTERNAL() \
120: auto ReturnProperty = Function->GetReturnProperty(); \
121: if (!ReturnProperty) { throw std::runtime_error{fmt::format("ReturnProperty is nullptr for '{}'", to_string(Function->GetFullName()))}; }
122: 
123: #define UE_RETURN_PROPERTY(TypeName) \
124: UE_RETURN_PROPERTY_INTERNAL() \
125: auto RetValue = *std::bit_cast<TypeName*>(&ParamData[ReturnProperty->GetOffset_Internal()]); \
126: UE_FREE_PARAM_STRUCT_INTERNAL() \
127: return RetValue;
```

### V1

[FProperty.cpp:245](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/FProperty.cpp:245)

```text
245:     void FProperty::CopyValuesInternal(void* Dest, const void* Src, int32_t Count) const
246:     {
247:         IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(FProperty, CopyValuesInternal, void, PARAMS(void*, const void*, int32_t), ARGS(Dest, Src, Count))
248:     }
249: 
250:     auto FProperty::GetValueTypeHashInternal(const void* Src) const -> uint32
251:     {
252:         IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(FProperty, GetValueTypeHashInternal, uint32, PARAMS(const void*), ARGS(Src))
253:     }
254: 
255:     void FProperty::CopyCompleteValue(void* Dest, const void* Src) const
256:     {
257:         if(Dest != Src)
258:         {
259:             if (GetPropertyFlags() & CPF_IsPlainOldData)
260:             {
261:                 FMemory::Memcpy( Dest, Src, static_cast<size_t>(GetElementSize()) * GetArrayDim() );
262:             }
263:             else
264:             {
265:                 CopyValuesInternal(Dest, Src, GetArrayDim());
266:             }
267:         }
```

### V2

[FProperty.cpp:342](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/src/FProperty.cpp:342)

```text
342:     auto FProperty::DestroyValue(void* Dest) -> void
343:     {
344:         if (!(GetPropertyFlags() & CPF_NoDestructor))
345:         {
346:             DestroyValueInternal(Dest);
347:         }
348:     }
349: 
350:     auto FProperty::DestroyValue(void* Dest) const -> void
351:     {
352:         if (!(GetPropertyFlags() & CPF_NoDestructor))
353:         {
354:             DestroyValueInternal(Dest);
355:         }
356:     }
357: 
358:     auto FProperty::DestroyValue_InContainer(void* Dest) -> void
359:     {
360:         DestroyValue(ContainerPtrToValuePtr<void>(Dest));
361:     }
362: 
363:     auto FProperty::DestroyValueInternal(void* Dest) const -> void
364:     {
365:         IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(FProperty, DestroyValueInternal, void, PARAMS(void*), ARGS(Dest))
366:     }
367: 
368:     void FProperty::InitializeValue( void* Dest ) const
369:     {
370:         if (GetPropertyFlags() & CPF_ZeroConstructor)
371:         {
372:             FMemory::Memzero(Dest, GetElementSize() * GetArrayDim());
373:         }
374:         else
375:         {
376:             InitializeValueInternal(Dest);
377:         }
378:     }
379: 
380:     auto FProperty::InitializeValueInternal(void* Dest) const -> void
381:     {
382:         IMPLEMENT_UNREAL_VIRTUAL_WRAPPER(FProperty, InitializeValueInternal, void, PARAMS(void*), ARGS(Dest))
```
