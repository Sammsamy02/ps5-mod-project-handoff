std::unordered_map<File::StringType, int32_t> FUObjectHashTables::MemberOffsets{};
std::unordered_map<File::StringType, BitfieldInfo> FUObjectHashTables::BitfieldInfos{};

TBucketMap<int, FSetHashBucket>& FUObjectHashTables::GetHash()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("Hash"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::Hash' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TBucketMap<int, FSetHashBucket>*>(this, offset);
}
const TBucketMap<int, FSetHashBucket>& FUObjectHashTables::GetHash() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("Hash"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::Hash' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TBucketMap<int, FSetHashBucket>*>(this, offset);
}

TMultiMap<int, UObjectBase *>& FUObjectHashTables::GetHashOuterBase()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("HashOuter"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::HashOuter' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMultiMap<int, UObjectBase *>*>(this, offset);
}
const TMultiMap<int, UObjectBase *>& FUObjectHashTables::GetHashOuterBase() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("HashOuter"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::HashOuter' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMultiMap<int, UObjectBase *>*>(this, offset);
}

TMultiMap<int, unsigned int>& FUObjectHashTables::GetHashOuter500()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("HashOuter"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::HashOuter' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMultiMap<int, unsigned int>*>(this, offset);
}
const TMultiMap<int, unsigned int>& FUObjectHashTables::GetHashOuter500() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("HashOuter"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::HashOuter' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMultiMap<int, unsigned int>*>(this, offset);
}

TMap<UObjectBase *, TSet<UObjectBase *>>& FUObjectHashTables::GetObjectOuterMapBase()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectOuterMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectOuterMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UObjectBase *, TSet<UObjectBase *>>*>(this, offset);
}
const TMap<UObjectBase *, TSet<UObjectBase *>>& FUObjectHashTables::GetObjectOuterMapBase() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectOuterMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectOuterMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UObjectBase *, TSet<UObjectBase *>>*>(this, offset);
}

TMap<UObjectBase *, FHashBucket>& FUObjectHashTables::GetObjectOuterMap416()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectOuterMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectOuterMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UObjectBase *, FHashBucket>*>(this, offset);
}
const TMap<UObjectBase *, FHashBucket>& FUObjectHashTables::GetObjectOuterMap416() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectOuterMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectOuterMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UObjectBase *, FHashBucket>*>(this, offset);
}

TMap<UClass *, TSet<UObjectBase *>>& FUObjectHashTables::GetClassToObjectListMapBase()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UClass *, TSet<UObjectBase *>>*>(this, offset);
}
const TMap<UClass *, TSet<UObjectBase *>>& FUObjectHashTables::GetClassToObjectListMapBase() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UClass *, TSet<UObjectBase *>>*>(this, offset);
}

TMap<UClass *, FHashBucket>& FUObjectHashTables::GetClassToObjectListMap422()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UClass *, FHashBucket>*>(this, offset);
}
const TMap<UClass *, FHashBucket>& FUObjectHashTables::GetClassToObjectListMap422() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UClass *, FHashBucket>*>(this, offset);
}

TMap<UClass *, TSet<UClass *>>& FUObjectHashTables::GetClassToChildListMap()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToChildListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToChildListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UClass *, TSet<UClass *>>*>(this, offset);
}
const TMap<UClass *, TSet<UClass *>>& FUObjectHashTables::GetClassToChildListMap() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToChildListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToChildListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UClass *, TSet<UClass *>>*>(this, offset);
}

TAtomic<unsigned __int64>& FUObjectHashTables::GetClassToChildListMapVersion()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToChildListMapVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToChildListMapVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TAtomic<unsigned __int64>*>(this, offset);
}
const TAtomic<unsigned __int64>& FUObjectHashTables::GetClassToChildListMapVersion() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ClassToChildListMapVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ClassToChildListMapVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TAtomic<unsigned __int64>*>(this, offset);
}

TBucketMap<UPackage *, FSetHashBucket>& FUObjectHashTables::GetPackageToObjectListMap()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("PackageToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::PackageToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TBucketMap<UPackage *, FSetHashBucket>*>(this, offset);
}
const TBucketMap<UPackage *, FSetHashBucket>& FUObjectHashTables::GetPackageToObjectListMap() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("PackageToObjectListMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::PackageToObjectListMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TBucketMap<UPackage *, FSetHashBucket>*>(this, offset);
}

TMap<UObjectBase *, UPackage *>& FUObjectHashTables::GetObjectToPackageMap()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectToPackageMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectToPackageMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<TMap<UObjectBase *, UPackage *>*>(this, offset);
}
const TMap<UObjectBase *, UPackage *>& FUObjectHashTables::GetObjectToPackageMap() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ObjectToPackageMap"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::ObjectToPackageMap' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const TMap<UObjectBase *, UPackage *>*>(this, offset);
}

std::atomic<unsigned __int64>& FUObjectHashTables::GetAllClassesVersion()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("AllClassesVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::AllClassesVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<std::atomic<unsigned __int64>*>(this, offset);
}
const std::atomic<unsigned __int64>& FUObjectHashTables::GetAllClassesVersion() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("AllClassesVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::AllClassesVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const std::atomic<unsigned __int64>*>(this, offset);
}

std::atomic<unsigned __int64>& FUObjectHashTables::GetNativeClassesVersion()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("NativeClassesVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::NativeClassesVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<std::atomic<unsigned __int64>*>(this, offset);
}
const std::atomic<unsigned __int64>& FUObjectHashTables::GetNativeClassesVersion() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("NativeClassesVersion"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FUObjectHashTables::NativeClassesVersion' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const std::atomic<unsigned __int64>*>(this, offset);
}

int32_t& FUObjectHashTables::UEP_TotalSize()
{
    static int32_t cached = []() {
        auto it = MemberOffsets.find(STR("UEP_TotalSize"));
        return it != MemberOffsets.end() ? it->second : -1;
    }();
    return cached;
}

