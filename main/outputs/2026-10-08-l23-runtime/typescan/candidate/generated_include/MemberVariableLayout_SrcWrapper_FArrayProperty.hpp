std::unordered_map<File::StringType, int32_t> FArrayProperty::MemberOffsets{};
std::unordered_map<File::StringType, BitfieldInfo> FArrayProperty::BitfieldInfos{};

#ifdef __PROSPERO__
FProperty* FArrayProperty::GetInner()
{
    const auto found = MemberOffsets.find(STR("Inner"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FArrayProperty::Inner");
}
#else
FProperty*& FArrayProperty::GetInner()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("Inner"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<FProperty**>(this, offset);
}
#endif
#ifdef __PROSPERO__
const FProperty* FArrayProperty::GetInner() const
{
    const auto found = MemberOffsets.find(STR("Inner"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FArrayProperty::Inner");
}
#else
const FProperty*& FArrayProperty::GetInner() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("Inner"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const FProperty**>(this, offset);
}
#endif

EArrayPropertyFlags& FArrayProperty::GetArrayFlags()
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ArrayFlags"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FArrayProperty::ArrayFlags' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<EArrayPropertyFlags*>(this, offset);
}
const EArrayPropertyFlags& FArrayProperty::GetArrayFlags() const
{
    static const int32_t offset = []() -> int32_t {
        auto offset_it = MemberOffsets.find(STR("ArrayFlags"));
        if (offset_it == MemberOffsets.end()) { throw std::runtime_error{"Tried getting member variable 'FArrayProperty::ArrayFlags' that doesn't exist in this engine version."}; }
        return offset_it->second;
    }();
    return *Helper::Casting::ptr_cast<const EArrayPropertyFlags*>(this, offset);
}

int32_t& FArrayProperty::UEP_TotalSize()
{
    static int32_t cached = []() {
        auto it = MemberOffsets.find(STR("UEP_TotalSize"));
        return it != MemberOffsets.end() ? it->second : -1;
    }();
    return cached;
}

