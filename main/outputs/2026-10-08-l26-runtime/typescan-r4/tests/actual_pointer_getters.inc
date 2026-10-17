std::unordered_map<File::StringType, int32_t> FMapProperty::MemberOffsets{};
std::unordered_map<File::StringType, BitfieldInfo> FMapProperty::BitfieldInfos{};
FProperty* FMapProperty::GetKeyProp()
{
    const auto found = MemberOffsets.find(STR("KeyProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FMapProperty::KeyProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FMapProperty::KeyProp");
}
const FProperty* FMapProperty::GetKeyProp() const
{
    const auto found = MemberOffsets.find(STR("KeyProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FMapProperty::KeyProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FMapProperty::KeyProp");
}
FProperty* FMapProperty::GetValueProp()
{
    const auto found = MemberOffsets.find(STR("ValueProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FMapProperty::ValueProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FMapProperty::ValueProp");
}
const FProperty* FMapProperty::GetValueProp() const
{
    const auto found = MemberOffsets.find(STR("ValueProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FMapProperty::ValueProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FMapProperty::ValueProp");
}
std::unordered_map<File::StringType, int32_t> FSetProperty::MemberOffsets{};
std::unordered_map<File::StringType, BitfieldInfo> FSetProperty::BitfieldInfos{};
FProperty* FSetProperty::GetElementProp()
{
    const auto found = MemberOffsets.find(STR("ElementProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FSetProperty::ElementProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FSetProperty::ElementProp");
}
const FProperty* FSetProperty::GetElementProp() const
{
    const auto found = MemberOffsets.find(STR("ElementProp"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FSetProperty::ElementProp' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FSetProperty::ElementProp");
}
std::unordered_map<File::StringType, int32_t> FArrayProperty::MemberOffsets{};
std::unordered_map<File::StringType, BitfieldInfo> FArrayProperty::BitfieldInfos{};
FProperty* FArrayProperty::GetInner()
{
    const auto found = MemberOffsets.find(STR("Inner"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FArrayProperty::Inner");
}
const FProperty* FArrayProperty::GetInner() const
{
    const auto found = MemberOffsets.find(STR("Inner"));
    if (found == MemberOffsets.end()) throw std::runtime_error{"Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version."};
    return NativePropertyGuard::ReadPointer(this, found->second, "FArrayProperty::Inner");
}
