#include "metadata.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static unsigned checks,failures;
#define CHECK(x) do {++checks; if(!(x)) {++failures; std::printf("FAIL %u: %s\n",__LINE__,#x);}} while(0)
#define REQUIRE(x) do {++checks; if(!(x)) {++failures; std::printf("FAIL %u: %s\n",__LINE__,#x); return;}} while(0)
static mb_title_metadata_result_v1 result;
static const char *sdk_fixture;
static const std::string minimal="{\"titleId\":\"Future.Title-2_\"}";

static mb_title_metadata_status decode(const std::string &text) {
    mb_title_metadata_input_v1 input{sizeof(input),1,
        reinterpret_cast<const uint8_t *>(text.data()),text.size(),MB_TITLE_PARAM_JSON_ROOT_V1,0};
    return mb_title_metadata_decode_v1(&input,&result);
}
static void no_authority() {
    CHECK(!result.installed_observed && !result.running_observed && !result.build_fingerprint_available);
    CHECK(!result.engine_acceptance_verified && !result.runtime_supported && !result.installation_supported);
    CHECK(!result.authenticity_verified && !result.game_acceptance_verified);
    CHECK(result.reserved[0]==0 && result.reserved[1]==0);
}
static void rejected(const std::string &text,mb_title_metadata_status status,mb_title_metadata_reason reason) {
    CHECK(decode(minimal)==MB_TITLE_METADATA_OK);
    CHECK(decode(text)==status);
    mb_title_metadata_result_v1 empty;
    std::memset(&empty,0,sizeof(empty)); empty.struct_size=sizeof(empty); empty.abi_version=1;
    empty.status=status; empty.reason=reason;
    CHECK(std::memcmp(&empty,&result,sizeof(result))==0);
}
static void authored_and_generic() {
    REQUIRE(decode(minimal)==MB_TITLE_METADATA_OK);
    std::ifstream file(sdk_fixture,std::ios::binary); REQUIRE(file.good());
    const std::string text((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    REQUIRE(!text.empty()); CHECK(decode(text)==MB_TITLE_METADATA_OK);
    CHECK(result.struct_size==sizeof(result) && result.abi_version==1 && result.metadata_complete==1);
    CHECK(result.title_id_available && !std::strcmp(result.title_id,"FAKE02932"));
    CHECK(result.name_available && !std::strcmp(result.name,"FAKE02932"));
    CHECK(result.locale_available && !std::strcmp(result.locale,"en-US"));
    CHECK(!result.version_available && result.version[0]==0);
    CHECK(result.captured_bytes==text.size() && result.metadata_sha256_available);
    CHECK(result.source_scope==MB_TITLE_SOURCE_METADATA_CAPTURE);
    CHECK(result.metadata_identity_scope==MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256);
    CHECK(std::strlen(result.metadata_sha256)==64); no_authority();
    std::printf("SDK_SHA256 %s\n",result.metadata_sha256);
    CHECK(decode("{\"titleId\":\"Other_Custom.8-Alpha\",\"contentVersion\":\"future-42\","
        "\"localizedParameters\":{\"defaultLanguage\":\"fr-CA\","
        "\"en-US\":{\"titleName\":\"Wrong fallback\"},\"fr-CA\":{\"titleName\":\"Création\"}}}")
        ==MB_TITLE_METADATA_OK);
    CHECK(!std::strcmp(result.title_id,"Other_Custom.8-Alpha"));
    CHECK(!std::strcmp(result.version,"future-42") && result.version_available);
    CHECK(!std::strcmp(result.name,"Création") && result.name_available);
    CHECK(!std::strcmp(result.locale,"fr-CA")); no_authority();
    CHECK(decode("{\"titleId\":\"a\"}")==MB_TITLE_METADATA_OK && !std::strcmp(result.title_id,"a"));
    CHECK(decode("{\"titleId\":\"not_a_commercial_prefix\"}")==MB_TITLE_METADATA_OK);
}
static void optional_fields() {
    REQUIRE(decode(minimal)==MB_TITLE_METADATA_OK);
    CHECK(!result.name_available && !result.locale_available && !result.version_available);
    CHECK(decode("{\"titleId\":\"Generic_2\",\"masterVersion\":\"99\",\"titleName\":\"Guess\","
        "\"localizedParameters\":{\"en-US\":{\"titleName\":\"No default\"}}}")==MB_TITLE_METADATA_OK);
    CHECK(!result.version_available && !result.name_available && !result.locale_available);
    CHECK(decode("{\"titleId\":\"Generic_2\",\"localizedParameters\":{\"defaultLanguage\":\"missing\","
        "\"en-US\":{\"titleName\":\"No fallback\"}}}")==MB_TITLE_METADATA_OK);
    CHECK(result.locale_available && !result.name_available && !std::strcmp(result.locale,"missing"));
    CHECK(decode("{\"titleId\":\"Generic_2\",\"localizedParameters\":{\"defaultLanguage\":\"x-Test\","
        "\"x-Test\":{\"ignored\":true},\"unselected\":null}}")==MB_TITLE_METADATA_OK);
    CHECK(result.locale_available && !result.name_available);
    CHECK(decode("{\"titleId\":\"Generic_2\",\"contentVersion\":\"\","
        "\"localizedParameters\":{\"defaultLanguage\":\"\",\"\":{\"titleName\":\"\"}}}")==MB_TITLE_METADATA_OK);
    CHECK(result.version_available && result.locale_available && result.name_available);
    CHECK(!result.name[0] && !result.version[0] && !result.locale[0]);
    CHECK(decode("{\"titleId\":\"Generic_2\",\"localizedParameters\":{\"defaultLanguage\":\"locale.δ\","
        "\"locale.δ\":{\"titleName\":\"作品\"}}}")==MB_TITLE_METADATA_OK);
    CHECK(!std::strcmp(result.locale,"locale.δ") && !std::strcmp(result.name,"作品")); no_authority();
    CHECK(decode("{\"localizedParameters\":{\"x-Later\":{\"titleName\":\"Before declaration\"},"
        "\"en-US\":{\"titleName\":\"No fallback\"},\"defaultLanguage\":\"x-Later\"},\"titleId\":\"Any_3\"}")==MB_TITLE_METADATA_OK);
    CHECK(result.name_available && !std::strcmp(result.name,"Before declaration"));
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"x-Later\":null,\"defaultLanguage\":\"x-Later\"}}",
        MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_SELECTED_LOCALE_TYPE);
}
static void schema_and_encoding() {
    REQUIRE(decode(minimal)==MB_TITLE_METADATA_OK);
    rejected("[]",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_ROOT_NOT_OBJECT);
    rejected("{}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_ID_MISSING);
    rejected("{\"titleId\":42}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_ID_TYPE);
    for(const char *bad:{"",".first","with/slash","space id","é","a\\u0000b"})
        rejected(std::string("{\"titleId\":\"")+bad+"\"}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_ID_INVALID);
    rejected("{\"titleId\":\"T\",\"contentVersion\":null}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_CONTENT_VERSION_TYPE);
    rejected("{\"titleId\":\"T\",\"contentVersion\":\"a\\u0000b\"}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_CONTENT_VERSION_INVALID);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":[]}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_LOCALIZED_TYPE);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":null}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_DEFAULT_LOCALE_TYPE);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"a\\u0000b\"}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_DEFAULT_LOCALE_INVALID);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"x\",\"x\":false}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_SELECTED_LOCALE_TYPE);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"x\",\"x\":{\"titleName\":4}}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_NAME_TYPE);
    rejected("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"x\",\"x\":{\"titleName\":\"a\\u0000b\"}}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_NAME_INVALID);
    for(const std::string &bad:{std::string("\xc0\xaf",2),std::string("\x80",1),std::string("\xe2\x82",2),
        std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4)})
        rejected(std::string("{\"titleId\":\"T\",\"ignored\":\"")+bad+"\"}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_UTF8);
    rejected("{\"titleId\":\"T\"}{}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_JSON);
    rejected("{\"titleId\":\"T\"",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_JSON);
    rejected("{\"titleId\":\"T\",\"ignored\":\"\\uD800\"}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_INVALID_JSON);
    rejected("{\"titleId\":\"T\",\"titleId\":\"U\"}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_DUPLICATE_KEY);
    rejected("{\"titleId\":\"T\",\"\\u0074itleId\":\"U\"}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_DUPLICATE_KEY);
    rejected("{\"titleId\":\"T\",\"ignored\":{\"x\":1,\"x\":2}}",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_DUPLICATE_KEY);
}
static std::string array_nodes(unsigned n) {
    std::string s="{\"titleId\":\"T\",\"ignored\":[";
    for(unsigned i=0;i<n;++i) {if(i)s+=','; s+='0';} return s+"]}";
}
static void exact_limits() {
    REQUIRE(decode(minimal)==MB_TITLE_METADATA_OK);
    CHECK(decode(std::string("{\"titleId\":\"")+std::string(64,'a')+"\"}")==MB_TITLE_METADATA_OK);
    rejected(std::string("{\"titleId\":\"")+std::string(65,'a')+"\"}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_ID_INVALID);
    CHECK(decode(std::string("{\"titleId\":\"T\",\"contentVersion\":\"")+std::string(128,'v')+"\"}")==MB_TITLE_METADATA_OK);
    rejected(std::string("{\"titleId\":\"T\",\"contentVersion\":\"")+std::string(129,'v')+"\"}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_CONTENT_VERSION_INVALID);
    std::string locale(64,'l');
    CHECK(decode(std::string("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"")+locale+"\",\""+locale+
        "\":{\"titleName\":\""+std::string(256,'n')+"\"}}}")==MB_TITLE_METADATA_OK);
    CHECK(std::strlen(result.name)==256 && std::strlen(result.locale)==64);
    rejected(std::string("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"")+std::string(65,'l')+"\"}}",
        MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_DEFAULT_LOCALE_INVALID);
    rejected(std::string("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"x\",\"x\":{\"titleName\":\"")+
        std::string(257,'n')+"\"}}}",MB_TITLE_METADATA_SCHEMA_ERROR,MB_TITLE_METADATA_TITLE_NAME_INVALID);
    std::string emoji; for(unsigned i=0;i<64;++i) emoji+="\xf0\x9f\x8c\x9f";
    CHECK(decode(std::string("{\"titleId\":\"T\",\"localizedParameters\":{\"defaultLanguage\":\"x\",\"x\":{\"titleName\":\"")+
        emoji+"\"}}}")==MB_TITLE_METADATA_OK && std::strlen(result.name)==256);
    CHECK(decode(std::string("{\"titleId\":\"T\",\"ignored\":\"")+std::string(4096,'s')+"\"}")==MB_TITLE_METADATA_OK);
    rejected(std::string("{\"titleId\":\"T\",\"ignored\":\"")+std::string(4097,'s')+"\"}",MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_STRING_EXCEEDED);
    rejected(std::string("{\"titleId\":\"T\",\"")+std::string(4097,'k')+"\":0}",MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_STRING_EXCEEDED);
    CHECK(decode(array_nodes(4091))==MB_TITLE_METADATA_OK);
    rejected(array_nodes(4092),MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_NODES_EXCEEDED);
    CHECK(decode(std::string("{\"titleId\":\"T\",\"ignored\":")+std::string(15,'[')+"0"+std::string(15,']')+"}")==MB_TITLE_METADATA_OK);
    rejected(std::string("{\"titleId\":\"T\",\"ignored\":")+std::string(16,'[')+"0"+std::string(16,']')+"}",MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_DEPTH_EXCEEDED);
    std::string padded=minimal; padded.resize(MB_TITLE_METADATA_INPUT_LIMIT,' ');
    CHECK(decode(padded)==MB_TITLE_METADATA_OK && result.captured_bytes==MB_TITLE_METADATA_INPUT_LIMIT);
    padded+=' '; rejected(padded,MB_TITLE_METADATA_RESOURCE_LIMIT,MB_TITLE_METADATA_INPUT_TOO_LARGE);
    rejected("",MB_TITLE_METADATA_MALFORMED,MB_TITLE_METADATA_EMPTY_INPUT);
}
static void preflight_and_identity() {
    REQUIRE(decode(minimal)==MB_TITLE_METADATA_OK);
    const mb_title_metadata_result_v1 first=result;
    CHECK(decode(minimal+" ")==MB_TITLE_METADATA_OK);
    CHECK(std::strcmp(first.metadata_sha256,result.metadata_sha256)!=0);
    CHECK(!std::strcmp(first.title_id,result.title_id)); no_authority();
    mb_title_metadata_input_v1 input{sizeof(input),1,reinterpret_cast<const uint8_t *>(minimal.data()),
        minimal.size(),MB_TITLE_PARAM_JSON_ROOT_V1,0};
    const auto saved=result;
    input.abi_version=2; CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    input.abi_version=1; input.struct_size--; CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    input.struct_size++; input.reserved=1; CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    input.reserved=0; input.profile=2; CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    input.profile=1; CHECK(mb_title_metadata_decode_v1(nullptr,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    CHECK(mb_title_metadata_decode_v1(&input,nullptr)==MB_TITLE_METADATA_BAD_ARGUMENT);
    input.bytes=nullptr; CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    CHECK(std::memcmp(&saved,&result,sizeof(result))==0);
    input.bytes=reinterpret_cast<const uint8_t *>(&result); input.available=1;
    CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    CHECK(std::memcmp(&saved,&result,sizeof(result))==0);
    input.bytes=reinterpret_cast<const uint8_t *>(&input);
    CHECK(mb_title_metadata_decode_v1(&input,&result)==MB_TITLE_METADATA_BAD_ARGUMENT);
    CHECK(std::memcmp(&saved,&result,sizeof(result))==0);
    alignas(mb_title_metadata_result_v1) std::array<uint8_t,sizeof(result)+sizeof(input)> region{};
    auto *alias_input=reinterpret_cast<mb_title_metadata_input_v1 *>(region.data());
    std::memcpy(alias_input,&input,sizeof(input)); const auto saved_region=region;
    CHECK(mb_title_metadata_decode_v1(alias_input,reinterpret_cast<mb_title_metadata_result_v1 *>(region.data()))==MB_TITLE_METADATA_BAD_ARGUMENT);
    CHECK(region==saved_region);
}
int main(int argc,char **argv) {
    if(argc!=2) return 2; sdk_fixture=argv[1];
    struct group {const char *name; void (*run)();} groups[]={
        {"authored_and_generic",authored_and_generic},{"optional_fields",optional_fields},
        {"schema_and_encoding",schema_and_encoding},{"exact_limits",exact_limits},
        {"preflight_and_identity",preflight_and_identity}};
    for(const auto &g:groups) {const auto before=failures; g.run();
        std::printf("GROUP %s %s\n",g.name,failures==before ? "PASS" : "FAIL");}
    std::printf("checks=%u failures=%u\n",checks,failures); return failures ? 1 : 0;
}
