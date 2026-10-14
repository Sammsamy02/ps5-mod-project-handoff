#include "modbridge/dds.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace modbridge {
namespace {
using U = std::uint64_t;
bool add(U a,U b,U& out) { if(b>std::numeric_limits<U>::max()-a)return false;out=a+b;return true; }
bool mul(U a,U b,U& out) { if(a&&b>std::numeric_limits<U>::max()/a)return false;out=a*b;return true; }
U u32(std::span<const std::uint8_t> b,std::size_t p) {
    // Every call is inside a previously checked complete header.
    U v=0;for(unsigned i=0;i<4;++i)v|=U(b[p+i])<<(8*i);return v;
}
void requirement(Json& j,const std::string& path,const char* id,const char* context,const std::string& why) {
    j["requirements"].push_back({{"id",id},{"consumer",path},{"context",context},{"status","unresolved"},{"discovery",why}});
}
struct Encoding { const char* name;U size;const char* numeric;bool typeless=false,srgb=false; };
std::optional<Encoding> dxgi(U id) {
    static constexpr std::array<Encoding,15> low={{{"BC1",8,"typeless",true},{"BC1",8,"UNORM"},{"BC1",8,"UNORM",false,true},
        {"BC2",16,"typeless",true},{"BC2",16,"UNORM"},{"BC2",16,"UNORM",false,true},
        {"BC3",16,"typeless",true},{"BC3",16,"UNORM"},{"BC3",16,"UNORM",false,true},
        {"BC4",8,"typeless",true},{"BC4",8,"UNORM"},{"BC4",8,"SNORM"},
        {"BC5",16,"typeless",true},{"BC5",16,"UNORM"},{"BC5",16,"SNORM"}}};
    static constexpr std::array<Encoding,6> high={{{"BC6H",16,"typeless",true},{"BC6H",16,"UF16"},{"BC6H",16,"SF16"},
        {"BC7",16,"typeless",true},{"BC7",16,"UNORM"},{"BC7",16,"UNORM",false,true}}};
    if(id>=70&&id<=84)return low[id-70];if(id>=94&&id<=99)return high[id-94];return {};
}
std::optional<Encoding> legacy_encoding(const std::string& code) {
    if(code=="DXT1")return Encoding{"BC1",8,"UNORM"};
    if(code=="DXT2"||code=="DXT3")return Encoding{"BC2",16,"UNORM"};
    if(code=="DXT4"||code=="DXT5")return Encoding{"BC3",16,"UNORM"};
    if(code=="ATI1"||code=="BC4U")return Encoding{"BC4",8,"UNORM"};
    if(code=="BC4S")return Encoding{"BC4",8,"SNORM"};
    if(code=="ATI2"||code=="BC5U")return Encoding{"BC5",16,"UNORM"};
    if(code=="BC5S")return Encoding{"BC5",16,"SNORM"};return {};
}
}
Json inspect_dds(std::span<const std::uint8_t> b,const std::string& path,U total) {
    Json j={{"kind","dds_texture"},{"facts",{{"classification_confidence","magic"},{"header_status","unavailable"},
        {"layout_status","unavailable"},{"input_total_size",total},{"input_bytes_available",b.size()},
        {"payload_complete",false},{"subresources_hashed",0},{"normal_form_eligible",false},
        {"inspection_complete",false},{"semantic_inventory_complete",false},{"subresources",Json::array()}}},
        {"requirements",Json::array()},{"issues",Json::array()}};
    auto& f=j["facts"];
    requirement(j,path,"texture.ps5_writer","preparation","verified PS5 texture writer required");
    requirement(j,path,"texture.game_acceptance","game_process","verified game/build texture acceptance required");
    auto fail=[&](const char* status,const char* reason,bool issue=true)->Json {
        f["layout_status"]=status;f["reason"]=reason;
        if(issue)j["issues"].push_back({{"code",reason},{"path",path}});
        requirement(j,path,"format.dds.payload_layout","preparation",reason);
        requirement(j,path,"inspection.complete","preparation",reason);return j;
    };
    if(total<b.size())return fail("unavailable","input_size_contradiction");
    if(b.size()<4)return fail("unavailable",total>=4?"magic_prefix_incomplete":"magic_truncated");
    if(b[0]!='D'||b[1]!='D'||b[2]!='S'||b[3]!=' ')return fail("unavailable","invalid_magic");
    if(b.size()<128)return fail("unavailable",total>=128?"header_prefix_incomplete":"header_truncated");
    Json raw={{"size",u32(b,4)},{"flags",u32(b,8)},{"height",u32(b,12)},{"width",u32(b,16)},
        {"pitch_or_linear_size",u32(b,20)},{"depth",u32(b,24)},{"mip_map_count",u32(b,28)},
        {"pixel_format_size",u32(b,76)},{"pixel_format_flags",u32(b,80)},{"fourcc_value",u32(b,84)},
        {"rgb_bit_count",u32(b,88)},{"r_mask",u32(b,92)},{"g_mask",u32(b,96)},{"b_mask",u32(b,100)},
        {"a_mask",u32(b,104)},{"caps",u32(b,108)},{"caps2",u32(b,112)},{"caps3",u32(b,116)},
        {"caps4",u32(b,120)},{"reserved2",u32(b,124)},{"reserved1",Json::array()}};
    for(std::size_t p=32;p<76;p+=4)raw["reserved1"].push_back(u32(b,p));f["raw_header"]=raw;
    if(u32(b,4)!=124){f["header_status"]="noncanonical";return fail("unavailable","noncanonical_header_size");}
    if(u32(b,76)!=32){f["header_status"]="noncanonical";return fail("unavailable","noncanonical_pixel_format_size");}
    const std::string code(reinterpret_cast<const char*>(b.data()+84),4);
    const bool extended=code=="DX10",fourcc=(u32(b,80)&4)!=0;
    U header=extended?148:128,width=u32(b,16),height=u32(b,12),depth=u32(b,24),mips=std::max<U>(1,u32(b,28));
    U flags=u32(b,8),caps=u32(b,112),dimension=3,array=1,misc=0,alpha=0;
    f["header_size"]=header;f["width"]=width;f["height"]=height;f["raw_depth"]=depth;f["mip_count"]=mips;
    if(b.size()<header)return fail("unavailable",total>=header?"dx10_header_prefix_incomplete":"dx10_header_truncated");
    f["header_status"]="complete";f["header_sha256"]=sha256_bytes(b.first(header));
    std::optional<Encoding> enc;
    // FourCC is four arbitrary bytes. Only printable ASCII is exposed as text;
    // numeric and byte-array facts retain exact evidence for opaque identifiers.
    const bool printable=std::all_of(b.begin()+84,b.begin()+88,[](std::uint8_t c){return c>=32&&c<=126;});
    f["format"]={{"fourcc",printable?Json(code):Json(nullptr)},{"fourcc_value",u32(b,84)},
        {"fourcc_bytes",Json::array({b[84],b[85],b[86],b[87]})},{"alpha_mode","unknown"}};
    bool alpha_known=true;
    if(extended) {
        dimension=u32(b,132);misc=u32(b,136);array=u32(b,140);alpha=u32(b,144);
        f["raw_dx10"]={{"dxgi_format",u32(b,128)},{"resource_dimension",dimension},{"misc_flag",misc},{"array_size",array},{"misc_flags2",alpha}};
        f["format"]["dxgi_format"]=u32(b,128);f["format"]["alpha_bits"]=alpha;
        static constexpr const char* modes[]={"unknown","straight","premultiplied","opaque","custom"};
        alpha_known=!(alpha&~U(7))&&(alpha&7)<=4;
        if(alpha_known)f["format"]["alpha_mode"]=modes[alpha&7];
        else f["format"]["alpha_mode"]="reserved";
        if(fourcc)enc=dxgi(u32(b,128));
    } else {
        if(fourcc)enc=legacy_encoding(code);
        if(code=="DXT2"||code=="DXT4")f["format"]["alpha_mode"]="premultiplied";
        if(caps&0x200000)dimension=4;
    }
    if(fourcc&&(u32(b,80)&0x20242))return fail("invalid","pixel_format_declaration_conflict");
    if(!enc)return fail("unsupported_format","unsupported_format",false);
    auto& fmt=f["format"];fmt["family"]=enc->name;fmt["block_bytes"]=enc->size;fmt["numeric_type"]=enc->numeric;
    fmt["typeless"]=enc->typeless;fmt["color_tag"]=enc->srgb?"srgb":"no_explicit_srgb_tag";
    const bool legacy_cube=(caps&0x200)!=0,faces=(caps&0xfc00)!=0;
    bool cube=extended?(misc&4)!=0:legacy_cube;
    f["resource_dimension"]=dimension;f["array_size"]=array;f["cube"]=cube;
    if(!array)return fail("invalid","zero_array_size");
    if(dimension<2||dimension>4)return fail("invalid","invalid_resource_dimension");
    if((cube||legacy_cube||faces)&&dimension!=3)return fail("invalid","cube_dimension_conflict");
    if(extended&&((legacy_cube&&!cube)||(faces&&!cube)))return fail("invalid","cube_declaration_conflict");
    if(extended&&cube&&(legacy_cube||faces)&&(caps&0xfc00)!=0xfc00)return fail("invalid","cube_declaration_conflict");
    if(!legacy_cube&&faces&&!extended)return fail("invalid","cube_declaration_conflict");
    if(!width||!height)return fail("invalid","zero_extent");
    if(dimension==2) {
        if(height!=1)return fail("invalid","invalid_1d_height");
        return fail("unsupported_topology","bc_1d",false);
    }
    if(dimension==4) {
        if(!(flags&0x800000)||!depth)return fail("invalid","volume_depth_declaration");
        if(array!=1)return fail("invalid","volume_array");
    } else {
        if((caps&0x200000)||(flags&0x800000)||depth>1)return fail("invalid","volume_dimension_conflict");
        depth=1;
    }
    if(!extended&&cube&&(caps&0xfc00)!=0xfc00)return fail("unsupported_topology","partial_cube",false);
    U extent=std::max({width,height,depth}),max_mips=0;
    for(;extent;extent>>=1)++max_mips;
    if(mips>max_mips)return fail("invalid","invalid_mip_count");
    U items,count;
    if(!mul(array,cube?6:1,items)||!mul(items,mips,count))return fail("resource_limit","arithmetic_overflow");
    if(count>dds_max_subresources)return fail("resource_limit","subresource_limit");
    // Preflight all arithmetic before allocating/iterating a subresource list.
    struct Mip { U width,height,depth,row,slice,size; };
    std::array<Mip,32> chain{};U per_item=0,w=width,h=height,d=depth;
    for(U mip=0;mip<mips;++mip) {
        auto& m=chain[mip];m.width=w;m.height=h;m.depth=d;
        if(!mul((w+3)/4,enc->size,m.row)||!mul(m.row,(h+3)/4,m.slice)||!mul(m.slice,d,m.size)||!add(per_item,m.size,per_item))
            return fail("resource_limit","arithmetic_overflow");
        w=std::max<U>(1,w/2);h=std::max<U>(1,h/2);d=std::max<U>(1,d/2);
    }
    U payload,end;if(!mul(per_item,items,payload)||!add(header,payload,end))return fail("resource_limit","arithmetic_overflow");
    f["depth"]=depth;f["subresource_count"]=count;f["payload_expected_end"]=end;f["layout_status"]="complete";
    f["payload_complete"]=end<=b.size();f["payload_truncated"]=end>total;
    f["payload_status"]=end>total?"truncated":end>b.size()?"prefix_incomplete":"complete";
    f["payload_bytes_available"]=std::min<U>(payload,b.size()-header);
    U offset=header,hashed=0;static constexpr const char* face_names[]={"+X","-X","+Y","-Y","+Z","-Z"};
    const bool hash_budget=b.size()<=dds_max_normal_bytes;
    for(U item=0;item<items;++item)for(U mip=0;mip<mips;++mip) {
        const auto& m=chain[mip];Json s={{"item",item},{"mip",mip},{"width",m.width},{"height",m.height},{"depth",m.depth},
            {"offset",offset},{"row_pitch",m.row},{"slice_pitch",m.slice},{"size",m.size}};
        if(cube){s["cube"]=item/6;s["face"]=face_names[item%6];}
        if(hash_budget&&offset<=b.size()&&m.size<=b.size()-offset){s["sha256"]=sha256_bytes(b.subspan(offset,m.size));++hashed;}
        f["subresources"].push_back(std::move(s));offset+=m.size;
    }
    f["subresources_hashed"]=hashed;
    if(end<total) {
        f["trailer"]={{"status","trailing_opaque"},{"offset",end},{"size",total-end},{"complete",total==b.size()}};
        if(hash_budget&&total==b.size())f["trailer"]["sha256"]=sha256_bytes(b.subspan(end));
    }
    if(hash_budget&&total==b.size())f["source_sha256"]=sha256_bytes(b);
    f["inspection_complete"]=end<=b.size()&&total==b.size()&&hash_budget;
    f["normal_form_eligible"]=end<=b.size()&&total==b.size()&&hash_budget&&alpha_known;
    if(end>total){f["reason"]="payload_truncated";j["issues"].push_back({{"code","payload_truncated"},{"path",path}});}
    else if(total>b.size())f["reason"]="inspection_prefix_incomplete";
    else if(!hash_budget)f["reason"]="normal_byte_limit";
    else if(!alpha_known)f["reason"]="reserved_alpha_bits";
    else f["reason"]="complete";
    if(!f["inspection_complete"].get<bool>())requirement(j,path,"inspection.complete","preparation",f["reason"]);
    if(!alpha_known)requirement(j,path,"format.dds.alpha_semantics","preparation","reserved_alpha_bits");
    return j;
}
const Json& DdsNeutral::metadata() const { return metadata_; }
std::span<const std::uint8_t> DdsNeutral::header() const { return header_; }
const std::vector<DdsBlockPayload>& DdsNeutral::subresources() const { return subresources_; }
std::span<const std::uint8_t> DdsNeutral::trailer() const { return trailer_; }
std::vector<std::uint8_t> DdsNeutral::lossless_dds() const {
    std::vector<std::uint8_t> result;result.reserve(metadata_.at("input_total_size").get<std::size_t>());
    result.insert(result.end(),header_.begin(),header_.end());
    for(const auto& s:subresources_)result.insert(result.end(),s.bytes.begin(),s.bytes.end());
    result.insert(result.end(),trailer_.begin(),trailer_.end());return result;
}
std::optional<DdsNeutral> normalize_dds(std::span<const std::uint8_t> b,std::string* reason) {
    if(reason)reason->clear();
    // Check the byte cap before parsing, hashing, or allocating retained storage.
    if(b.size()>dds_max_normal_bytes){if(reason)*reason="normal_byte_limit";return {};}
    auto facts=inspect_dds(b,"",b.size()).at("facts");
    if(!facts.at("normal_form_eligible").get<bool>()) {
        if(reason)*reason=facts.at("reason").get<std::string>();return {};
    }
    DdsNeutral n;n.metadata_=std::move(facts);
    const auto header=n.metadata_.at("header_size").get<std::size_t>();
    n.header_.assign(b.begin(),b.begin()+header);
    n.subresources_.reserve(n.metadata_.at("subresources").size());
    for(const auto& s:n.metadata_.at("subresources")) {
        auto offset=s.at("offset").get<std::size_t>(),size=s.at("size").get<std::size_t>();
        n.subresources_.push_back({s,{b.begin()+offset,b.begin()+offset+size}});
    }
    const auto end=n.metadata_.at("payload_expected_end").get<std::size_t>();
    n.trailer_.assign(b.begin()+end,b.end());return n;
}
}
