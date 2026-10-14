static std::unordered_map<File::StringType, int32_t> MemberOffsets;
static std::unordered_map<File::StringType, BitfieldInfo> BitfieldInfos;

public:
    TBucketMap<int, FSetHashBucket>& GetHash();
    const TBucketMap<int, FSetHashBucket>& GetHash() const;

private:
    TMultiMap<int, UObjectBase *>& GetHashOuterBase();
    const TMultiMap<int, UObjectBase *>& GetHashOuterBase() const;
    TMultiMap<int, unsigned int>& GetHashOuter500();
    const TMultiMap<int, unsigned int>& GetHashOuter500() const;

private:
    TMap<UObjectBase *, TSet<UObjectBase *>>& GetObjectOuterMapBase();
    const TMap<UObjectBase *, TSet<UObjectBase *>>& GetObjectOuterMapBase() const;
    TMap<UObjectBase *, FHashBucket>& GetObjectOuterMap416();
    const TMap<UObjectBase *, FHashBucket>& GetObjectOuterMap416() const;

private:
    TMap<UClass *, TSet<UObjectBase *>>& GetClassToObjectListMapBase();
    const TMap<UClass *, TSet<UObjectBase *>>& GetClassToObjectListMapBase() const;
    TMap<UClass *, FHashBucket>& GetClassToObjectListMap422();
    const TMap<UClass *, FHashBucket>& GetClassToObjectListMap422() const;

public:
    TMap<UClass *, TSet<UClass *>>& GetClassToChildListMap();
    const TMap<UClass *, TSet<UClass *>>& GetClassToChildListMap() const;

public:
    TAtomic<unsigned __int64>& GetClassToChildListMapVersion();
    const TAtomic<unsigned __int64>& GetClassToChildListMapVersion() const;

public:
    TBucketMap<UPackage *, FSetHashBucket>& GetPackageToObjectListMap();
    const TBucketMap<UPackage *, FSetHashBucket>& GetPackageToObjectListMap() const;

public:
    TMap<UObjectBase *, UPackage *>& GetObjectToPackageMap();
    const TMap<UObjectBase *, UPackage *>& GetObjectToPackageMap() const;

public:
    std::atomic<unsigned __int64>& GetAllClassesVersion();
    const std::atomic<unsigned __int64>& GetAllClassesVersion() const;

public:
    std::atomic<unsigned __int64>& GetNativeClassesVersion();
    const std::atomic<unsigned __int64>& GetNativeClassesVersion() const;

public:
    static int32_t& UEP_TotalSize();
