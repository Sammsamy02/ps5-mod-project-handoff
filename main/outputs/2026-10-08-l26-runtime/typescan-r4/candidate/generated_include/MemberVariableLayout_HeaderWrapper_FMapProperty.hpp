static std::unordered_map<File::StringType, int32_t> MemberOffsets;
static std::unordered_map<File::StringType, BitfieldInfo> BitfieldInfos;

public:
    #ifdef __PROSPERO__
    FProperty* GetKeyProp();
#else
    FProperty*& GetKeyProp();
#endif
    #ifdef __PROSPERO__
    const FProperty* GetKeyProp() const;
#else
    const FProperty*& GetKeyProp() const;
#endif

public:
    #ifdef __PROSPERO__
    FProperty* GetValueProp();
#else
    FProperty*& GetValueProp();
#endif
    #ifdef __PROSPERO__
    const FProperty* GetValueProp() const;
#else
    const FProperty*& GetValueProp() const;
#endif

public:
    FScriptMapLayout& GetMapLayout();
    const FScriptMapLayout& GetMapLayout() const;

public:
    EMapPropertyFlags& GetMapFlags();
    const EMapPropertyFlags& GetMapFlags() const;

public:
    static int32_t& UEP_TotalSize();
