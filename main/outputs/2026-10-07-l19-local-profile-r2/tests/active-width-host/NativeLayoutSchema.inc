static const std::vector<MemberSchema> MemberSchemaEntries{
    {"UFunction", "Func", "UFunction", "Func", false},
    {"UFunction", "UEP_TotalSize", "UFunction", "UEP_TotalSize", false},
    {"UStruct", "MinAlignment", "UStruct", "MinAlignment", false},
    {"UStruct", "UEP_TotalSize", "UStruct", "UEP_TotalSize", false},
    {"UStruct", "Script", "UStruct", "Script", false},
    {"UClass", "ClassCastFlags", "UClass", "ClassCastFlags", false},
    {"UClass", "UEP_TotalSize", "UClass", "UEP_TotalSize", false},
    {"FFieldClass", "CastFlags", "FFieldClass", "CastFlags", false},
    {"FFieldClass", "UEP_TotalSize", "FFieldClass", "UEP_TotalSize", false},
};
static auto MemberTargets(){return std::array<MemberTarget,4>{{{"UFunction",&Unreal::UFunction::MemberOffsets,&Unreal::UFunction::BitfieldInfos},{"UStruct",&Unreal::UStruct::MemberOffsets,&Unreal::UStruct::BitfieldInfos},{"UClass",&Unreal::UClass::MemberOffsets,&Unreal::UClass::BitfieldInfos},{"FFieldClass",&Unreal::FFieldClass::MemberOffsets,&Unreal::FFieldClass::BitfieldInfos}}};}
static auto VirtualTargets(){return std::array<VirtualTarget,1>{{
    {"FProperty", &Unreal::FProperty::VTableLayoutMap},
}};}
