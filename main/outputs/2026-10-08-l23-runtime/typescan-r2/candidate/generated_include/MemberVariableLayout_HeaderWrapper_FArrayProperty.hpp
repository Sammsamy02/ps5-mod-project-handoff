static std::unordered_map<File::StringType, int32_t> MemberOffsets;
static std::unordered_map<File::StringType, BitfieldInfo> BitfieldInfos;

public:
    #ifdef __PROSPERO__
    FProperty* GetInner();
#else
    FProperty*& GetInner();
#endif
    #ifdef __PROSPERO__
    const FProperty* GetInner() const;
#else
    const FProperty*& GetInner() const;
#endif

public:
    EArrayPropertyFlags& GetArrayFlags();
    const EArrayPropertyFlags& GetArrayFlags() const;

public:
    static int32_t& UEP_TotalSize();
