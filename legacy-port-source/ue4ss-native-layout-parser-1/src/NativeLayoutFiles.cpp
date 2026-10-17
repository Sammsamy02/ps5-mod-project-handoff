#include <NativeLayoutFiles.hpp>
#include <array>
#include <climits>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>
namespace RC::Platform::NativeLayouts {
namespace {
constexpr size_t MaxBytes=8*1024*1024,MaxEntries=16384,MaxToken=512;
[[noreturn]] void fail(std::string_view file,size_t line,std::string_view reason){
    throw std::invalid_argument(std::string(file)+":"+std::to_string(line)+": "+std::string(reason));
}
auto trim(std::string_view value)->std::string_view{
    while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.remove_prefix(1);
    while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.remove_suffix(1);
    return value;
}
bool head(char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
bool tail(char c){return head(c)||(c>='0'&&c<='9');}
bool identifier(std::string_view name){
    if(name.empty()||!head(name.front()))return false;
    for(char c:name)if(!tail(c))return false;
    return true;
}
bool section_name(std::string_view name){
    while(true){const auto p=name.find("::");if(!identifier(name.substr(0,p)))return false;
        if(p==name.npos)return true;name.remove_prefix(p+2);}
}
auto number(std::string_view value,bool hex,std::string_view file,size_t line)->uint32_t{
    value=trim(value);unsigned base=10;
    if(hex&&value.size()>=2&&value[0]=='0'&&(value[1]=='x'||value[1]=='X')){base=16;value.remove_prefix(2);}
    if(value.empty())fail(file,line,"Numeric value is empty");
    uint32_t result=0;
    for(char c:value){unsigned digit;
        if(c>='0'&&c<='9')digit=static_cast<unsigned>(c-'0');
        else if(base==16&&c>='a'&&c<='f')digit=static_cast<unsigned>(c-'a'+10);
        else if(base==16&&c>='A'&&c<='F')digit=static_cast<unsigned>(c-'A'+10);
        else fail(file,line,"Numeric value must be unsigned and fully consumed");
        if(digit>=base)fail(file,line,"Invalid numeric digit");
        if(result>(std::numeric_limits<uint32_t>::max()-digit)/base)fail(file,line,"Numeric value exceeds uint32");
        result=result*base+digit;
    }
    return result;
}
void opaque(std::string_view value,std::string_view file,size_t line){
    if(value.empty())fail(file,line,"Opaque identity value must be nonempty");
    for(char byte:value)if(const auto c=static_cast<unsigned char>(byte);c<32||c>126)fail(file,line,"Opaque identity value must be printable ASCII");
}
struct Parsed {Identity identity;std::vector<Entry> entries;};
auto parse(std::string_view text,std::string_view file,bool members)->Parsed{
    if(text.size()>MaxBytes)fail(file,0,"File exceeds 8MiB");
    if(text.find('\0')!=text.npos)fail(file,0,"File contains NUL");
    if(text.starts_with("\xef\xbb\xbf"))text.remove_prefix(3);
    Parsed out;std::unordered_set<std::string> sections,keys;std::string section;
    std::array<bool,9> metadata{};bool have_metadata=false;size_t line_number=0;
    while(!text.empty()){
        ++line_number;const auto newline=text.find('\n');auto line=text.substr(0,newline);
        if(!line.empty()&&line.back()=='\r'){
            if(newline==text.npos)fail(file,line_number,"Lone CR is not a line ending");line.remove_suffix(1);
        }
        if(line.find('\r')!=line.npos)fail(file,line_number,"Only LF or CRLF line endings are supported");
        if(newline==text.npos)text={};else text.remove_prefix(newline+1);
        line=trim(line);if(line.empty()||line.front()==';'||line.front()=='#')continue;
        if(line.front()=='['){
            if(line.size()<3||line.back()!=']')fail(file,line_number,"Malformed section");
            auto name=trim(line.substr(1,line.size()-2));
            if(name.size()>MaxToken||!section_name(name))fail(file,line_number,"Section must be an ASCII C++ identifier name of at most 512 bytes");
            if(!sections.emplace(name).second)fail(file,line_number,"Duplicate section");
            // Empty sections cannot bypass the parser's bounded entry budget.
            if(sections.size()>MaxEntries+1)fail(file,line_number,"Too many sections");
            section=std::string(name);keys.clear();if(name=="NativeLayout")have_metadata=true;continue;
        }
        if(section.empty())fail(file,line_number,"Key appears outside a section");
        const auto equal=line.find('=');if(equal==line.npos)fail(file,line_number,"Expected key=value; ordered lists are unsupported");
        const auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
        if(key.size()>MaxToken||!identifier(key))fail(file,line_number,"Key must be an ASCII identifier of at most 512 bytes");
        if(value.size()>MaxToken)fail(file,line_number,"Value token exceeds 512 bytes");
        if(!keys.emplace(key).second)fail(file,line_number,"Duplicate key");
        if(section=="NativeLayout"){
            size_t which;
            if(key=="Format")which=0;else if(key=="Target")which=1;else if(key=="PointerSize")which=2;
            else if(key=="EngineMajor")which=3;else if(key=="EngineMinor")which=4;else if(key=="NonShipping")which=5;
            else if(key=="Stats")which=6;else if(key=="EngineRevision")which=7;else if(key=="BuildId")which=8;
            else fail(file,line_number,"Unknown NativeLayout metadata key");
            metadata[which]=true;
            switch(which){
            case 0:out.identity.format=number(value,false,file,line_number);if(out.identity.format!=1)fail(file,line_number,"Format must be 1");break;
            case 1:if(value!="prospero-x86_64-sysv")fail(file,line_number,"Target must be prospero-x86_64-sysv");out.identity.target=std::string(value);break;
            case 2:out.identity.pointer_size=number(value,false,file,line_number);if(out.identity.pointer_size!=8)fail(file,line_number,"PointerSize must be 8");break;
            case 3:out.identity.engine_major=number(value,false,file,line_number);if(out.identity.engine_major<4)fail(file,line_number,"EngineMajor must be at least 4");break;
            case 4:out.identity.engine_minor=number(value,false,file,line_number);break;
            case 5:{auto n=number(value,false,file,line_number);if(n>1)fail(file,line_number,"NonShipping must be 0 or 1");out.identity.non_shipping=n;break;}
            case 6:{auto n=number(value,false,file,line_number);if(n>1)fail(file,line_number,"Stats must be 0 or 1");out.identity.stats=n;break;}
            case 7:opaque(value,file,line_number);out.identity.engine_revision=std::string(value);break;
            case 8:opaque(value,file,line_number);out.identity.build_id=std::string(value);break;
            }
            continue;
        }
        if(out.entries.size()>=MaxEntries)fail(file,line_number,"More than 16384 layout entries");
        Entry entry{section,std::string(key),0,{}};
        std::array<std::string_view,4> parts{};size_t count=0;auto remaining=value;
        while(true){if(count==parts.size())fail(file,line_number,"Too many bitfield tuple components");const auto colon=remaining.find(':');parts[count++]=trim(remaining.substr(0,colon));if(colon==remaining.npos)break;remaining.remove_prefix(colon+1);}
        if(!members&&count!=1)fail(file,line_number,"Vtable offsets do not accept bitfield tuples");
        entry.offset=number(parts[0],true,file,line_number);
        if(entry.offset>INT32_MAX)fail(file,line_number,"Offset exceeds INT32_MAX");
        if(members&&key=="UEP_TotalSize"&&!entry.offset)fail(file,line_number,"UEP_TotalSize must be positive");
        if(!members&&entry.offset%8)fail(file,line_number,"Vtable byte offset must be aligned to 8");
        if(count>1){
            const auto position=number(parts[1],false,file,line_number);
            const auto length=count>2?number(parts[2],false,file,line_number):1;
            const auto storage=count>3?number(parts[3],false,file,line_number):1;
            if(storage!=1&&storage!=2&&storage!=4&&storage!=8)fail(file,line_number,"Bitfield storage must be 1,2,4 or 8 bytes");
            const auto bits=storage*8;
            if(!length||length>bits||position>=bits||length>bits-position)fail(file,line_number,"Bitfield position/length exceeds storage");
            entry.bitfield=Bitfield{static_cast<uint8_t>(position),static_cast<uint8_t>(length),static_cast<uint8_t>(storage)};
        }
        out.entries.push_back(std::move(entry));
    }
    if(!have_metadata)fail(file,line_number,"Required NativeLayout section is missing");
    for(bool present:metadata)if(!present)fail(file,line_number,"Required NativeLayout metadata field is missing");
    return out;
}
}
auto ParsePair(std::string_view members,std::string_view vtables)->Bundle{
    auto member=parse(members,"MemberVariableLayout.ini",true);
    auto vtable=parse(vtables,"VTableLayout.ini",false);
    if(member.identity!=vtable.identity)fail("NativeLayout pair",0,"Declared identity metadata differs across files; agreement is not target proof");
    return {std::move(member.identity),std::move(member.entries),std::move(vtable.entries)};
}
}
