#include "metadata.h"
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <string_view>

namespace {
using Json=nlohmann::json;
struct Span {uintptr_t start,end;};
bool span(const void *p,uint64_t size,size_t alignment,Span &out) noexcept {
    const auto at=reinterpret_cast<uintptr_t>(p);
    if((size && !p) || (p && at%alignment) || size>std::numeric_limits<size_t>::max() || size>UINTPTR_MAX-at)
        return false;
    out={at,at+static_cast<uintptr_t>(size)}; return true;
}
bool overlap(Span a,Span b) noexcept {
    return a.start<a.end && b.start<b.end && a.start<b.end && b.start<a.end;
}
mb_title_metadata_status fail(mb_title_metadata_result_v1 *out,
    mb_title_metadata_status status,mb_title_metadata_reason reason) noexcept {
    std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->abi_version=1;
    out->status=status; out->reason=reason; return status;
}
bool utf8(const uint8_t *data,size_t size) noexcept {
    size_t i=0;
    while(i<size) {
        const uint8_t first=data[i++];
        if(first<0x80) continue;
        unsigned more;
        if(first>=0xc2 && first<=0xdf) more=1;
        else if(first>=0xe0 && first<=0xef) more=2;
        else if(first>=0xf0 && first<=0xf4) more=3;
        else return false;
        if(more>size-i) return false;
        const uint8_t second=data[i];
        if((first==0xe0 && second<0xa0) || (first==0xed && second>=0xa0) ||
            (first==0xf0 && second<0x90) || (first==0xf4 && second>=0x90)) return false;
        for(unsigned n=0;n<more;++n) if((data[i++]&0xc0)!=0x80) return false;
    }
    return true;
}

/* Whole-input streaming validation. Containers, scalar values and
 * every object key count once. Decoded keys identify duplicates even if their
 * original escape spellings differ. All input strings, including ignored
 * fields, are checked. The tokenizer/SAX sets may allocate within 64KiB input;
 * this is not an allocation-free parser. */
struct Bounds final: nlohmann::json_sax<Json> {
    struct Frame {bool object=false; std::set<std::string> keys;};
    std::array<Frame,MB_TITLE_METADATA_DEPTH_LIMIT> frames;
    size_t depth=0,nodes=0;
    mb_title_metadata_reason reason=MB_TITLE_METADATA_REASON_NONE;
    bool node() noexcept {
        if(nodes==MB_TITLE_METADATA_NODE_LIMIT) {reason=MB_TITLE_METADATA_NODES_EXCEEDED; return false;}
        ++nodes; return true;
    }
    bool start(bool object) {
        if(depth==MB_TITLE_METADATA_DEPTH_LIMIT) {reason=MB_TITLE_METADATA_DEPTH_EXCEEDED; return false;}
        if(!node()) return false;
        auto &frame=frames[depth++]; frame.object=object; frame.keys.clear(); return true;
    }
    bool end(bool object) noexcept {
        if(!depth || frames[depth-1].object!=object) {reason=MB_TITLE_METADATA_INVALID_JSON; return false;}
        --depth; return true;
    }
    bool string_size(const std::string &text) noexcept {
        if(text.size()>MB_TITLE_METADATA_STRING_LIMIT) {reason=MB_TITLE_METADATA_STRING_EXCEEDED; return false;}
        return true;
    }
    bool null() override {return node();}
    bool boolean(bool) override {return node();}
    bool number_integer(number_integer_t) override {return node();}
    bool number_unsigned(number_unsigned_t) override {return node();}
    bool number_float(number_float_t,const string_t &) override {return node();}
    bool string(string_t &text) override {return string_size(text) && node();}
    bool binary(binary_t &) override {reason=MB_TITLE_METADATA_INVALID_JSON; return false;}
    bool key(string_t &text) override {
        if(!string_size(text) || !node()) return false;
        if(!depth || !frames[depth-1].object) {reason=MB_TITLE_METADATA_INVALID_JSON; return false;}
        if(!frames[depth-1].keys.insert(text).second) {reason=MB_TITLE_METADATA_DUPLICATE_KEY; return false;}
        return true;
    }
    bool start_object(size_t) override {return start(true);}
    bool end_object() override {return end(true);}
    bool start_array(size_t) override {return start(false);}
    bool end_array() override {return end(false);}
    bool parse_error(size_t,const std::string &,const nlohmann::detail::exception &) override {
        reason=MB_TITLE_METADATA_INVALID_JSON; return false;
    }
};
bool alnum(unsigned char c) noexcept {
    return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9');
}
bool title_id(std::string_view value) noexcept {
    if(value.empty() || value.size()>64 || !alnum(static_cast<unsigned char>(value.front()))) return false;
    for(const unsigned char c:value) if(!alnum(c) && c!='.' && c!='_' && c!='-') return false;
    return true;
}
template<size_t N> void copy(char (&out)[N],std::string_view value) noexcept {
    if(!value.empty()) std::memcpy(out,value.data(),value.size());
    out[value.size()]=0;
}
/* Two field passes follow whole-input validation. The first captures root
 * identity/version and declared locale; the second reads exactly that locale's
 * name, regardless of JSON key order. No DOM is constructed: pinned JSON DOM
 * destruction can allocate beneath a noexcept destructor. SAX tokenizer/key
 * storage still allocates, and its allocation failures are contained. */
struct Fields final: nlohmann::json_sax<Json> {
    enum class Role {ignored,root,localized,selected};
    enum class Field {ignored,id,version,localized,locale,selected,name};
    struct Frame {Role role=Role::ignored; Field next=Field::ignored; bool object=false;};
    std::array<Frame,MB_TITLE_METADATA_DEPTH_LIMIT> frames;
    size_t depth=0; bool name_pass;
    mb_title_metadata_result_v1 &owned;
    mb_title_metadata_reason reason=MB_TITLE_METADATA_REASON_NONE;
    Fields(mb_title_metadata_result_v1 &value,bool names):name_pass(names),owned(value) {}
    bool reject(mb_title_metadata_reason value) noexcept {reason=value; return false;}
    Field take() noexcept {
        if(!depth || !frames[depth-1].object) return Field::ignored;
        const auto value=frames[depth-1].next; frames[depth-1].next=Field::ignored; return value;
    }
    bool wrong_type(Field field) noexcept {
        switch(field) {
        case Field::id: return reject(MB_TITLE_METADATA_TITLE_ID_TYPE);
        case Field::version: return reject(MB_TITLE_METADATA_CONTENT_VERSION_TYPE);
        case Field::localized: return reject(MB_TITLE_METADATA_LOCALIZED_TYPE);
        case Field::locale: return reject(MB_TITLE_METADATA_DEFAULT_LOCALE_TYPE);
        case Field::selected: return reject(MB_TITLE_METADATA_SELECTED_LOCALE_TYPE);
        case Field::name: return reject(MB_TITLE_METADATA_TITLE_NAME_TYPE);
        default: return true;
        }
    }
    bool scalar() noexcept {
        if(!depth) return reject(MB_TITLE_METADATA_ROOT_NOT_OBJECT);
        return wrong_type(take());
    }
    bool start(bool object) noexcept {
        if(depth==MB_TITLE_METADATA_DEPTH_LIMIT) return reject(MB_TITLE_METADATA_DEPTH_EXCEEDED);
        Role role=Role::ignored;
        if(!depth) {
            if(!object) return reject(MB_TITLE_METADATA_ROOT_NOT_OBJECT);
            role=Role::root;
        } else {
            const auto field=take();
            if(field==Field::localized || field==Field::selected) {
                if(!object) return wrong_type(field);
                role=field==Field::localized ? Role::localized : Role::selected;
            } else if(!wrong_type(field)) return false;
        }
        frames[depth++]={role,Field::ignored,object}; return true;
    }
    bool end() noexcept {
        if(!depth) return reject(MB_TITLE_METADATA_INVALID_JSON);
        --depth; return true;
    }
    bool key(string_t &text) override {
        if(!depth || !frames[depth-1].object) return reject(MB_TITLE_METADATA_INVALID_JSON);
        auto &frame=frames[depth-1]; frame.next=Field::ignored;
        if(frame.role==Role::root) {
            if(text=="localizedParameters") frame.next=Field::localized;
            else if(!name_pass && text=="titleId") frame.next=Field::id;
            else if(!name_pass && text=="contentVersion") frame.next=Field::version;
        } else if(frame.role==Role::localized) {
            if(name_pass && owned.locale_available && text==owned.locale) frame.next=Field::selected;
            else if(!name_pass && text=="defaultLanguage") frame.next=Field::locale;
        } else if(frame.role==Role::selected && text=="titleName") frame.next=Field::name;
        return true;
    }
    bool string(string_t &text) override {
        if(!depth) return reject(MB_TITLE_METADATA_ROOT_NOT_OBJECT);
        const auto field=take();
        if(field==Field::ignored) return true;
        if(field==Field::localized || field==Field::selected) return wrong_type(field);
        const bool nul=text.find('\0')!=std::string::npos;
        switch(field) {
        case Field::id:
            if(nul || !title_id(text)) return reject(MB_TITLE_METADATA_TITLE_ID_INVALID);
            copy(owned.title_id,text); owned.title_id_available=1; return true;
        case Field::version:
            if(nul || text.size()>128) return reject(MB_TITLE_METADATA_CONTENT_VERSION_INVALID);
            copy(owned.version,text); owned.version_available=1; return true;
        case Field::locale:
            if(nul || text.size()>64) return reject(MB_TITLE_METADATA_DEFAULT_LOCALE_INVALID);
            copy(owned.locale,text); owned.locale_available=1; return true;
        case Field::name:
            if(nul || text.size()>256) return reject(MB_TITLE_METADATA_TITLE_NAME_INVALID);
            copy(owned.name,text); owned.name_available=1; return true;
        default: return false;
        }
    }
    bool null() override {return scalar();}
    bool boolean(bool) override {return scalar();}
    bool number_integer(number_integer_t) override {return scalar();}
    bool number_unsigned(number_unsigned_t) override {return scalar();}
    bool number_float(number_float_t,const string_t &) override {return scalar();}
    bool binary(binary_t &) override {return reject(MB_TITLE_METADATA_INVALID_JSON);}
    bool start_object(size_t) override {return start(true);}
    bool start_array(size_t) override {return start(false);}
    bool end_object() override {return end();}
    bool end_array() override {return end();}
    bool parse_error(size_t,const std::string &,const nlohmann::detail::exception &) override {
        return reject(MB_TITLE_METADATA_INVALID_JSON);
    }
};
}

extern "C" mb_title_metadata_status mb_title_metadata_decode_v1(
    const mb_title_metadata_input_v1 *input,mb_title_metadata_result_v1 *out) noexcept {
    Span request,result,bytes;
    if(!span(input,sizeof(*input),alignof(mb_title_metadata_input_v1),request) ||
        !span(out,sizeof(*out),alignof(mb_title_metadata_result_v1),result) || overlap(request,result))
        return MB_TITLE_METADATA_BAD_ARGUMENT;
    if(input->struct_size!=sizeof(*input) || input->abi_version!=1 || input->reserved ||
        input->profile!=MB_TITLE_PARAM_JSON_ROOT_V1 || !span(input->bytes,input->available,1,bytes) ||
        overlap(request,bytes) || overlap(result,bytes)) return MB_TITLE_METADATA_BAD_ARGUMENT;
    if(input->available>MB_TITLE_METADATA_INPUT_LIMIT)
        return fail(out,MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_INPUT_TOO_LARGE);
    if(!input->available) return fail(out,MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_EMPTY_INPUT);
    const auto size=static_cast<size_t>(input->available);
    if(!utf8(input->bytes,size)) return fail(out,MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_UTF8);
    try {
        {
            Bounds bounds;
            if(!Json::sax_parse(input->bytes,input->bytes+size,&bounds) || bounds.depth) {
                const auto reason=bounds.reason==MB_TITLE_METADATA_REASON_NONE ? MB_TITLE_METADATA_INVALID_JSON : bounds.reason;
                const bool limit=reason==MB_TITLE_METADATA_DEPTH_EXCEEDED || reason==MB_TITLE_METADATA_NODES_EXCEEDED ||
                    reason==MB_TITLE_METADATA_STRING_EXCEEDED;
                return fail(out,limit ? MB_TITLE_METADATA_RESOURCE_LIMIT : MB_TITLE_METADATA_MALFORMED,reason);
            }
        }
        mb_title_metadata_result_v1 owned;
        std::memset(&owned,0,sizeof(owned));
        Fields fields(owned,false);
        if(!Json::sax_parse(input->bytes,input->bytes+size,&fields))
            return fail(out,MB_TITLE_METADATA_SCHEMA_ERROR,fields.reason);
        if(!owned.title_id_available) return fail(out,MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_ID_MISSING);
        if(owned.locale_available) {
            Fields names(owned,true);
            if(!Json::sax_parse(input->bytes,input->bytes+size,&names))
                return fail(out,MB_TITLE_METADATA_SCHEMA_ERROR,names.reason);
        }
        picosha2::hash256_one_by_one hash;
        hash.process(input->bytes,input->bytes+size); hash.finish();
        uint8_t identity[picosha2::k_digest_size];
        static_assert(picosha2::k_digest_size==32);
        hash.get_hash_bytes(identity,identity+sizeof(identity));
        static constexpr char hex[]="0123456789abcdef";
        for(size_t n=0;n<sizeof(identity);++n) {
            owned.metadata_sha256[n*2]=hex[identity[n]>>4];
            owned.metadata_sha256[n*2+1]=hex[identity[n]&15];
        }
        owned.struct_size=sizeof(owned); owned.abi_version=1;
        owned.status=MB_TITLE_METADATA_OK; owned.reason=MB_TITLE_METADATA_REASON_NONE;
        owned.captured_bytes=input->available; owned.source_scope=MB_TITLE_SOURCE_METADATA_CAPTURE;
        owned.metadata_identity_scope=MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256;
        owned.metadata_complete=owned.title_id_available=owned.metadata_sha256_available=1;
        std::memcpy(out,&owned,sizeof(owned)); return MB_TITLE_METADATA_OK;
    } catch(const std::bad_alloc &) {
        return fail(out,MB_TITLE_METADATA_ALLOCATION_FAILED,MB_TITLE_METADATA_ALLOCATION_FAILURE);
    } catch(const Json::parse_error &) {
        return fail(out,MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_JSON);
    } catch(...) {
        return fail(out,MB_TITLE_METADATA_INTERNAL_ERROR,MB_TITLE_METADATA_INTERNAL_FAILURE);
    }
}
