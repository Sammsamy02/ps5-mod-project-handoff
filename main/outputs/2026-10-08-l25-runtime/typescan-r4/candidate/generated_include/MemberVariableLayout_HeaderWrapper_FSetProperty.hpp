static std::unordered_map<File::StringType, int32_t> MemberOffsets;
static std::unordered_map<File::StringType, BitfieldInfo> BitfieldInfos;

public:
    #ifdef __PROSPERO__
    FProperty* GetElementProp();
#else
    FProperty*& GetElementProp();
#endif
    #ifdef __PROSPERO__
    const FProperty* GetElementProp() const;
#else
    const FProperty*& GetElementProp() const;
#endif

private:
    FScriptSetLayout& GetSetLayoutBase();
    const FScriptSetLayout& GetSetLayoutBase() const;
    FScriptSparseSetLayout& GetSetLayout507();
    const FScriptSparseSetLayout& GetSetLayout507() const;

public:
    static int32_t& UEP_TotalSize();
