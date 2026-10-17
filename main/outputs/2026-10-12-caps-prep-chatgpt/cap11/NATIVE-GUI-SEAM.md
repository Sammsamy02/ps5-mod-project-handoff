# Native GUI prerequisite: exact missing seam

Read-only source audit. No backend, registry or context component staged: the current port lacks the host ownership seam required to make such a component meaningful. No build, execution, or native GUI compatibility claim. File SHA-256s and actual Git-root probes are in NATIVE-GUI-SEAM-RECEIPT.json. All three source directories resolve Git to the containing worktree; upstream revisions remain unresolved.

| Actual source/interface | Existing behavior | Required seam before enabling |
|---|---|---|
| Port src/Mod/CppUserModBase.cpp:56–68 register_tab | Target branch throws before owning GUITab, preventing unwind cleanup from calling unavailable service | Preserve fail-closed branch until a real host can accept ownership and render |
| Port include/UE4SSProgram.hpp:157–159 and :318–324 | DebuggingGUI member is excluded on target; get_debugging_ui throws | Actual host-owned GUI instance with startup/shutdown ordering |
| Port src/UE4SSProgram.cpp:2810–2852 | stop_render_thread, add_gui_tab and remove_gui_tab throw on target | Verified frame dispatcher, registration and synchronous callback retirement |
| Retained include/GUI/GUITab.hpp:16–35 | Callback type is void(*)(CppUserModBase*); owner is raw pointer; fields private, friend DebuggingGUI | Genuine class integration, not shadow tab/base or invented access ABI |
| Retained src/GUI/GUI.cpp:197–208, :603–612 | Callback runs under m_tabs_mutex; remove_tab takes same mutex | Revoke/drain before original uninstall begins, with reentrancy/thread policy |
| Retained src/GUI/GUI.cpp:338–367, :480–491, :614–625 | Host creates context, begins frame, calls on_update, renders, shuts down backends, destroys context | One admitted context/allocator/library ownership model; actual input, frame and renderer |
| Retained include/GUI/GUI.hpp:62–138; src/GUI/GUI.cpp:582–600 | GfxBackendBase/OSBackendBase require device/window/input/render responsibilities; factory selects DX11 or GLFW/OpenGL | Implement a genuine supported backend after target renderer/input contracts exist |
| Retained imgui/imgui.h:31, :378–385, :1127; imgui.cpp:1342–1361 | Available library reports 1.92.1; globals/heaps differ across module boundaries; SetCurrentContext and allocator synchronization required | Verify exact linked host/mod version, data layout and shared allocator/context; presence on disk proves no linkage |

The tab mutex serializes list iteration/removal, but shared_ptr<GUITab> does not own CppUserModBase. Original mod destruction may begin before the base destructor removes tabs, so the mutex does not establish owner safety. A callback that synchronously unregisters itself would try to reacquire that same nonrecursive mutex. These are integration hazards derived from the actual ownership/control flow; neither was executed here.

The cap09 lifecycle adapter retains a child lease during the original mod lifetime; that lease prevents child finalization but does not retire arbitrary GUI callbacks. Future integration must revoke registrations and drain callbacks BEFORE invoking original uninstall, then release the lease after successful teardown. A throwing uninstall requires an explicit retained-owner/registration policy. No new ABI, scheduler or engine values were invented in this audit.

The smallest useful next implementation is a real host service wired into the genuine DebuggingGUI/tab ownership path, with an admitted render-thread policy and revoke/drain handshake. A standalone registry or temporary ImGui context would bypass this missing seam and cannot be claimed as native GUI support. A headless/no-op backend must not admit the original mods.

Full actual ImGui API requirements remain in API-GAP-MATRIX.md and the 76-API coverage gate. Existing Lua UMG tests do not exercise native callbacks. Once the backend/retirement seams are supplied, future compiled tests must cover registration failure, callback vs teardown, self-unregister policy, factory unwind, frame exceptions, context shutdown, allocator boundaries and real visible/input behavior. No such tests were run here.
