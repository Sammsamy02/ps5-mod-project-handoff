// Included inside RC::Unreal::SeedArming AFTER NativePropertyReadBackend.inl.
// Standard and Unreal headers must be included at file scope by current-source.patch.

// [cap07 review] [PAKMOUNT] lines go to UE4SS.log like every other runtime
// marker (narrow-first: format narrow, widen ASCII once, Output::send);
// stderr is only the last-resort sink. Never throws.
static void pak_log(const char* line) noexcept {
    if(!line)return;
    try {
        StringType wide{};
        for(const char* p=line;*p;++p){
            const auto c=static_cast<unsigned char>(*p);
            wide.push_back(static_cast<StringType::value_type>(c<0x80?c:'?'));
        }
        Output::send(STR("{}\n"),wide);
    } catch(...) {
        try {std::fprintf(stderr,"%s\n",line);} catch(...) {}
    }
}

// [cap07 review] The authenticated code-image cache answers only on the
// initializer thread while startup admission is armed (CopyinStartup
// InitializerOwner; disarmed right after admission, supplier-diag
// copyin_probe_gate_l26_early_release). The mount runs later on the game
// thread, so the image identity, its authenticated extent and the six
// prefix verdicts are captured ONCE here during startup admission and are
// only read afterwards. Nothing captured => every mount refuses before any
// engine call.
struct PakMountStartupAdmission {
    uint64_t base=0,extent=0;
    char sha256[65]{};
    ::ModBridge::PakMount::EnginePins::Proof proofs[6]{};
    bool proof_ok[6]{};
};
static PakMountStartupAdmission g_pak_admission{};
static std::atomic<bool> g_pak_admission_ready{false};
static void CapturePakMountAdmission(uint64_t base,const rc::codecache::Metadata* image) noexcept {
    if(g_pak_admission_ready.load(std::memory_order_acquire))return;
    if(!base||!image||!image->ready||image->base!=base||!image->extent){
        pak_log("[PAKMOUNT] startup admission: code-image cache unavailable");return;
    }
    const auto pins=modbridge_pak_pins_ue561();
    PakMountStartupAdmission a{};
    a.base=base;a.extent=image->extent;
    std::memcpy(a.sha256,image->sha256,sizeof a.sha256);a.sha256[64]=0;
    unsigned ok=0;
    for(size_t i=0;i<6;++i){
        const auto& f=pins.proofs[i];
        a.proofs[i]=f;
        a.proof_ok[i]=f.size&&f.size<=64&&f.linked<=UINT64_MAX-base&&
            bridge_code_cache_signature(base,base+f.linked,f.bytes,f.size)==1;
        ok+=a.proof_ok[i]?1u:0u;
    }
    g_pak_admission=a;
    g_pak_admission_ready.store(true,std::memory_order_release);
    char line[160];
    std::snprintf(line,sizeof line,"[PAKMOUNT] startup admission: proofs=%u/6 extent=0x%llx",ok,static_cast<unsigned long long>(a.extent));
    pak_log(line);
}
static const PakMountStartupAdmission* PakMountAdmission(uint64_t base) noexcept {
    if(!g_pak_admission_ready.load(std::memory_order_acquire)||!base||g_pak_admission.base!=base)return nullptr;
    return &g_pak_admission;
}

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
        const auto* a=PakMountAdmission(module_view_base());
        return a?std::string(a->sha256,64):std::string{};
    }
    bool read_ptr(uint64_t address,uint64_t& out) override {return PropertyReadBytes(address,sizeof out,&out);}
    bool read_u32(uint64_t address,uint32_t& out) override {return PropertyReadBytes(address,sizeof out,&out);}
    // A code address is admitted only as one of the six captured prefixes
    // (address, size, bytes) whose startup verdict (authenticated cache,
    // initializer thread) was positive.
    bool verify_code(uint64_t address,const uint8_t* bytes,size_t n) override {
        const auto* a=PakMountAdmission(module_view_base());
        if(!a||!bytes||!n||n>64)return false;
        for(size_t i=0;i<6;++i){
            const auto& f=a->proofs[i];
            if(a->proof_ok[i]&&f.size==n&&f.linked<=UINT64_MAX-a->base&&address==a->base+f.linked&&std::memcmp(f.bytes,bytes,n)==0)return true;
        }
        return false;
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
        uint64_t inst=0,vt=0,fn=0,receiver=0;uint32_t size=0;
        // [cap07 review] The snapshot delegate is bound with BindLambda: a
        // functor instance {vptr, payload, handle, captured this@+0x18} of
        // 0x20 bytes (DelegateSize counts 16-byte units => 2). Its pinned
        // Execute thunk reads only [inst+0x18]. Nothing past +0x20 is read
        // (the old +0x20/+0x28 member/adjustor reads were outside the object).
        if(!read_ptr(base+pins_.snapshot_delegate,inst)||!read_u32(base+pins_.snapshot_delegate+8,size)||!inst||size<2||size>64||inst>UINT64_MAX-0x30||!read_ptr(inst,vt)||vt!=base+pins_.snapshot_vtable||!read_ptr(vt+pins_.slot_delegate_execute,fn)||fn!=base+pins_.snapshot_execute||!read_ptr(inst+0x18,receiver)||receiver!=layer)return false;
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
    void log(const char* line) override {pak_log(line);}
};

static void RunNativePakMountOnceImpl(const std::filesystem::path& mods_directory,const std::filesystem::path& game_directory) noexcept {
    if(!IsInGameThreadRaw())return;
    static std::atomic_bool attempted{false};
    if(attempted.exchange(true,std::memory_order_acq_rel))return;
    try {
        // [#7 step 2 source, launches K/L] Two sources, the first visible one is the only one used:
        //  1. <configured root>/pakmods (native setup_mod_directory_path supplies <configured root>/ue4ss/Mods);
        //     launch K: not visible from the game process after module_start.
        //  2. the game's own <Project>/Content/Paks/~mods (PakRequestProducer game_pak_mods_directory).
        // Neither present means opt-out. Game files are never rewritten; only a ~mods child is listed.
        char line[640];
        std::filesystem::path directory;const char* source="";
        if(mods_directory.filename()=="Mods"&&mods_directory.parent_path().filename()=="ue4ss"){
            const auto configured=mods_directory.parent_path().parent_path()/"pakmods";
            std::error_code configured_ec;
            const auto configured_status=std::filesystem::status(configured,configured_ec);
            if(!configured_ec&&configured_status.type()==std::filesystem::file_type::directory){directory=configured;source="configured";}
            else{
                std::snprintf(line,sizeof line,"[PAKMOUNT] configured pakmods not visible: %s (errno %d) type=%d",configured.string().c_str(),configured_ec.value(),static_cast<int>(configured_status.type()));
                pak_log(line);
            }
        }else pak_log("[PAKMOUNT] configured native root shape unavailable");
        if(directory.empty()){
            std::string why;
            directory=::ModBridge::PakMount::game_pak_mods_directory(game_directory,why);
            if(directory.empty()){
                std::snprintf(line,sizeof line,"[PAKMOUNT] disabled: no pakmods directory (game paks: %s)",why.c_str());
                pak_log(line);return;
            }
            source="game-paks";
        }
        std::snprintf(line,sizeof line,"[PAKMOUNT] source=%s dir=%s",source,directory.string().c_str());pak_log(line);
        const auto requests=::ModBridge::PakMount::discover_requests(directory);
        if(requests.empty()){pak_log("[PAKMOUNT] pre-call refuse: zero selected containers");return;}
        const auto* admission=PakMountAdmission(SeedArming::module_view_base());
        if(!admission){pak_log("[PAKMOUNT] pre-call refuse: startup pin admission unavailable");return;}
        auto pins=modbridge_pak_pins_ue561();
        pins.image_extent=admission->extent;
        NativePakMountBridge bridge(pins);
        for(const auto& request:requests){
            const auto result=::ModBridge::PakMount::mount(pins,request,bridge);
            std::snprintf(line,sizeof line,"[PAKMOUNT] status=%u call=%u ret=0x%llx before=%u after=%u order=%d reason=%s",unsigned(result.verdict),unsigned(result.call_made),static_cast<unsigned long long>(result.returned_pak),result.count_before,result.count_after,result.order_argument,result.why);
            pak_log(line);
            if(result.verdict!=::ModBridge::PakMount::Verdict::Mounted)break;
        }
    }catch(const std::exception& e){
        char line[320];std::snprintf(line,sizeof line,"[PAKMOUNT] stopped: %s (prior mount side effects possible)",e.what());pak_log(line);
    }
    catch(...){pak_log("[PAKMOUNT] stopped: native exception (prior mount side effects possible)");}
}
