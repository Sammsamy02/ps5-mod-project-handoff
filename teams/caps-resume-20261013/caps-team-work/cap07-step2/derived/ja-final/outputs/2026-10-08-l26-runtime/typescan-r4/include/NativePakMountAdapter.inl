// Included inside RC::Unreal::SeedArming AFTER NativePropertyReadBackend.inl.
// Standard and Unreal headers must be included at file scope by current-source.patch.
class NativePakMountBridge final : public ::ModBridge::PakMount::IEngineBridge {
    using Pins=::ModBridge::PakMount::EnginePins;
    Pins pins_;
    bool admitted(uint64_t address,const Pins::Proof& proof) {
        const auto base=module_view_base();
        return base&&proof.linked<=UINT64_MAX-base&&address==base+proof.linked&&
            verify_code(address,proof.bytes,proof.size);
    }
public:
    explicit NativePakMountBridge(Pins pins):pins_(pins){}
    uint64_t module_view_base() override {return SeedArming::module_view_base();}
    std::string image_sha256() override {
        const rc::codecache::Metadata* m=nullptr;
        if(bridge_code_cache_context(module_view_base(),&m)!=1||!m||!m->ready)return {};
        return std::string(m->sha256,64);
    }
    bool read_ptr(uint64_t address,uint64_t& out) override {return PropertyReadBytes(address,sizeof out,&out);}
    bool read_u32(uint64_t address,uint32_t& out) override {return PropertyReadBytes(address,sizeof out,&out);}
    bool verify_code(uint64_t address,const uint8_t* bytes,size_t n) override {
        return n&&n<=64&&bridge_code_cache_signature(module_view_base(),address,bytes,n)==1;
    }
    bool is_game_thread() override {return IsInGameThreadRaw();}
    uint64_t platform_chain_head() override {
        const auto b=module_view_base();
        if(!is_game_thread()||!admitted(b+pins_.platform_mgr_get,pins_.proofs[0])||!admitted(b+pins_.platform_mgr_getpf,pins_.proofs[1]))return 0;
        using Get=void*(*)();using Current=void*(*)(void*);
        auto* manager=reinterpret_cast<Get>(b+pins_.platform_mgr_get)();
        if(!manager)return 0;
        return reinterpret_cast<uint64_t>(reinterpret_cast<Current>(b+pins_.platform_mgr_getpf)(manager));
    }
    void* make_fstring(const std::string& path) override {
        if(!is_game_thread()||!GMalloc||!*GMalloc)return nullptr;
        RC::LuaType::LuaFNameBridge::Bridge bridge(path.data(),path.size());
        if(!bridge.ok())return nullptr;
        return new FString(bridge.to_fstring());
    }
    void free_fstring(void* value) override {delete static_cast<FString*>(value);}
    uint64_t call_execute(uint64_t fn,uint64_t inst,void* string,int32_t order) override {
        if(!is_game_thread()||!admitted(fn,pins_.proofs[2])||!string||order<0)return 0;
        using Execute=void*(*)(void*,const FString*,int32_t);
        return reinterpret_cast<uint64_t>(reinterpret_cast<Execute>(fn)(reinterpret_cast<void*>(inst),static_cast<const FString*>(string),order));
    }
    // Keep legacy interface implementation synchronized; core uses snapshots only.
    bool mounted_pak_count(uint64_t layer,uint32_t& out) override {
        std::vector<::ModBridge::PakMount::PakInfo> rows;
        if(!snapshot_paks(layer,rows))return false;
        out=static_cast<uint32_t>(rows.size());return true;
    }
    bool snapshot_paks(uint64_t layer,std::vector<::ModBridge::PakMount::PakInfo>& out) override {
        out.clear();const auto base=module_view_base();
        if(!is_game_thread()||!GMalloc||!*GMalloc||!admitted(base+pins_.snapshot_execute,pins_.proofs[4]))return false;
        uint64_t inst=0,vt=0,fn=0,receiver=0,member=0,adjust=1;uint32_t size=0;
        // Same bound-target identity standard as the mount delegate: receiver,
        // non-null member target and zero this-adjustor. The exact member
        // address is not separately pinned; row validation stays fail-closed.
        if(!read_ptr(base+pins_.snapshot_delegate,inst)||!read_u32(base+pins_.snapshot_delegate+8,size)||!inst||!size||size>64||inst>UINT64_MAX-0x30||!read_ptr(inst,vt)||vt!=base+pins_.snapshot_vtable||!read_ptr(vt+pins_.slot_delegate_execute,fn)||fn!=base+pins_.snapshot_execute||!read_ptr(inst+0x18,receiver)||receiver!=layer||!read_ptr(inst+0x20,member)||!member||!read_ptr(inst+0x28,adjust)||adjust!=0)return false;
        // Exact captured SysV aggregate-return contract: RDI output16, RSI instance.
        // Output elements are non-owning POD records; no pak pointer is dereferenced.
        struct alignas(8) Result {void* data=nullptr;int32_t count=0,capacity=0;};
        static_assert(sizeof(Result)==16);
        Result result;
        struct Owner {Result& value;~Owner() noexcept {if(value.data){try{(*GMalloc)->Free(value.data);}catch(...){}}}} owner{result};
        using Snapshot=void*(*)(void*,void*);
        reinterpret_cast<Snapshot>(fn)(&result,reinterpret_cast<void*>(inst));
        if(result.count<0||result.capacity<result.count||result.count>4096||result.capacity>65536||(result.count&&!result.data))return false;
        struct Row {uint64_t identity;uint32_t order,padding;};static_assert(sizeof(Row)==16);
        const auto start=reinterpret_cast<uint64_t>(result.data);
        if(start>UINT64_MAX-static_cast<uint64_t>(result.count)*sizeof(Row))return false;
        std::vector<::ModBridge::PakMount::PakInfo> owned;owned.reserve(result.count);
        for(int32_t i=0;i<result.count;++i){Row row{};if(!PropertyReadBytes(start+static_cast<uint64_t>(i)*sizeof(Row),sizeof row,&row)||!row.identity)return false;owned.push_back({row.identity,row.order});}
        out=std::move(owned);return true;
    }
    void log(const char* line) override {std::printf("%s\n",line);}
};

static void RunNativePakMountOnceImpl(const std::filesystem::path& mods_directory) noexcept {
    if(!IsInGameThreadRaw())return;
    static std::atomic_bool attempted{false};
    if(attempted.exchange(true,std::memory_order_acq_rel))return;
    try {
        // Native setup_mod_directory_path supplies <configured root>/ue4ss/Mods.
        if(mods_directory.filename()!="Mods"||mods_directory.parent_path().filename()!="ue4ss"){
            std::printf("[PAKMOUNT] pre-call refuse: configured native root shape unavailable\n");return;
        }
        const auto directory=mods_directory.parent_path().parent_path()/"pakmods";
        // No directory means opt-out; game files are never searched or installed.
        if(!std::filesystem::exists(directory)){std::printf("[PAKMOUNT] disabled: no pakmods directory\n");return;}
        const auto requests=::ModBridge::PakMount::discover_requests(directory);
        if(requests.empty()){std::printf("[PAKMOUNT] pre-call refuse: zero selected containers\n");return;}
        auto pins=modbridge_pak_pins_ue561();NativePakMountBridge bridge(pins);
        for(const auto& request:requests){
            const auto result=::ModBridge::PakMount::mount(pins,request,bridge);
            std::printf("[PAKMOUNT] status=%u call=%u ret=0x%llx before=%u after=%u order=%d reason=%s\n",unsigned(result.verdict),unsigned(result.call_made),static_cast<unsigned long long>(result.returned_pak),result.count_before,result.count_after,result.order_argument,result.why);
            if(result.verdict!=::ModBridge::PakMount::Verdict::Mounted)break;
        }
    }catch(const std::exception& e){std::printf("[PAKMOUNT] stopped: %s (prior mount side effects possible)\n",e.what());}
    catch(...){std::printf("[PAKMOUNT] stopped: native exception (prior mount side effects possible)\n");}
}
