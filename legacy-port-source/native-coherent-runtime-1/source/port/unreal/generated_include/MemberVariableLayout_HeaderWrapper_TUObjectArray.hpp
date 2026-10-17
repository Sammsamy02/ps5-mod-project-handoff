static std::unordered_map<File::StringType, int32_t> MemberOffsets;
static std::unordered_map<File::StringType, BitfieldInfo> BitfieldInfos;

public:
    int32& GetNumElements();
    const int32& GetNumElements() const;

public:
    int32& GetMaxElements();
    const int32& GetMaxElements() const;

public:
    using Chunks_Type = UObjectBase**[512];
    Chunks_Type& GetChunks();
    const Chunks_Type& GetChunks() const;

public:
    int32& GetNumChunks();
    const int32& GetNumChunks() const;

private:
    FUObjectItem*& GetObjectsBase();
    const FUObjectItem*& GetObjectsBase() const;
    FUObjectItem**& GetObjects420();
    const FUObjectItem**& GetObjects420() const;

public:
    FUObjectItem*& GetPreAllocatedObjects();
    const FUObjectItem*& GetPreAllocatedObjects() const;

public:
    int32& GetMaxChunks();
    const int32& GetMaxChunks() const;

public:
    static int32_t& UEP_TotalSize();
