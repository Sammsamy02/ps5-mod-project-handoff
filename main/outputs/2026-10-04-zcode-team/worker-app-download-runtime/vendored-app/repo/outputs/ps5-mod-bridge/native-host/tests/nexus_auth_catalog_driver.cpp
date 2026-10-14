#include "auth.h"
#include "nexus_client.h"
#include "download/oauth_code_transfer.h"
#include "download/provider_transfer.h"
#include "modbridge/nexus.hpp"
#include "modbridge/nexus_requirements_query.h"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <time.h>
#include <vector>

namespace {
std::map<std::string,unsigned> requests;
bool transport_contract_ok=true;
bool post_contract_ok=true;
unsigned control_count=0;
unsigned requirements_posts=0;
bool malformed_latest=false;
bool capture_graph=false,capture_oversized=false;
const std::string access="fixture_access_token";

std::string_view response_for(std::string_view url) {
    const auto path_at=url.find('/',url.find("//")+2);
    if(path_at==std::string_view::npos)return {};
    const auto path=url.substr(path_at);
    ++requests[std::string(path)];
    if(path=="/oauth/userinfo")
        return R"({"sub":"1234","name":"Fixture User","membership_roles":["premium"],"preferences":{"adult":false}})";
    if(path=="/v1/games")
        return R"([{"id":7,"domain_name":"townfall","name":"Townfall","mods":5}])";
    if(path=="/v1/games/townfall/mods/latest_added")
        if(malformed_latest)return R"([{"mod_id":95,"game_id":7,"domain_name":"townfall","name":"Visible mod","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":false},{"mod_id":96,"game_id":7,"domain_name":"wrong-domain","name":"Malformed final","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":false}])";
    if(path=="/v1/games/townfall/mods/latest_added")
        return R"([
          {"mod_id":95,"game_id":7,"domain_name":"townfall","name":"Visible mod","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":false},
          {"mod_id":96,"game_id":7,"domain_name":"townfall","name":"Adult mod","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":true},
          {"mod_id":97,"game_id":7,"domain_name":"townfall","name":"Unpublished mod","author":"Fixture","available":true,"status":"draft","version":"1.0","contains_adult_content":false},
          {"mod_id":98,"game_id":7,"domain_name":"townfall","name":"Unavailable mod","author":"Fixture","available":false,"status":"published","version":"1.0","contains_adult_content":false},
          {"mod_id":99,"game_id":7,"domain_name":"townfall","name":"Unknown adult flag","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":"unknown"}
        ])";
    if(path=="/v1/games/townfall/mods/95"||path=="/v1/games/townfall/mods/96"||path=="/v1/games/townfall/mods/97") {
        const bool adult=path=="/v1/games/townfall/mods/96";
        const bool published=path!="/v1/games/townfall/mods/97";
        static thread_local std::string body;
        body="{\"mod_id\":"+(adult?std::string("96"):published?std::string("95"):std::string("97"))+
             ",\"game_id\":7,\"domain_name\":\"townfall\",\"name\":\"Fixture mod\",\"author\":\"Fixture\",\"available\":true,\"status\":\""+
             (published?std::string("published"):std::string("draft"))+"\",\"version\":\"1.0\",\"contains_adult_content\":"+
             (adult?std::string("true"):std::string("false"))+"}";
        return body;
    }
    if(path=="/v1/games/townfall/mods/99")
        return R"({"mod_id":99,"game_id":7,"domain_name":"townfall","name":"Unknown adult flag","author":"Fixture","available":true,"status":"published","version":"1.0","contains_adult_content":"unknown"})";
    if(path=="/v1/games/townfall/mods/95/files")
        return R"({"files":[{"file_id":5,"name":"Main file","file_name":"mod.zip","version":"1.0","description":"Fixture archive","size_kb":12.5}]})";
    if(capture_graph&&path=="/v1/games/townfall/mods/94")
        return R"({"mod_id":94,"game_id":7,"domain_name":"townfall","name":"Prerequisite mod","author":"Dependency author","available":true,"status":"published","version":"mod-label","contains_adult_content":false})";
    if(capture_graph&&path=="/v1/games/townfall/mods/94/files")
        return R"({"files":[{"file_id":4,"name":"Dependency file","file_name":"dependency.zip","version":"file-label","description":"Fixture archive","size_kb":1}]})";
    return {};
}
void init_transfer_result(mb_curl_provider_transfer_result_v1 *result) {
    *result={};result->struct_size=sizeof(*result);result->abi_version=MB_CURL_DOWNLOAD_ABI_V1;
}
int provide_body(void *consume_context,int (*consume)(void *,const uint8_t *,size_t),uint8_t *body,
                 size_t capacity,std::string_view bytes,mb_curl_provider_transfer_result_v1 *result) {
    if(bytes.empty()||bytes.size()>capacity) {
        result->status=MB_CURL_DOWNLOAD_LIMIT;return result->status;
    }
    std::memcpy(body,bytes.data(),bytes.size());result->received_bytes=bytes.size();result->http_status=200;
    if(consume(consume_context,body,bytes.size())) {
        result->status=MB_CURL_DOWNLOAD_CONSUMER_FAILED;return result->status;
    }
    result->status=MB_CURL_DOWNLOAD_OK;result->complete=1;return result->status;
}
int require(bool condition,const char *name) {
    ++control_count;
    if(condition)return 0;
    std::cerr<<"FAIL "<<name<<"\n";return 1;
}
int entropy(void *context,uint8_t output[32]) {
    auto *counter=static_cast<uint32_t*>(context);
    for(size_t i=0;i<32;++i)output[i]=static_cast<uint8_t>((*counter)++);
    return 0;
}
struct PublishCancel { const char *output=nullptr;size_t calls=0;bool observed=false; };
int cancel_after_staged_output(void *context,uint64_t) {
    auto *state=static_cast<PublishCancel*>(context);++state->calls;
    if(state->output&&std::memcmp(state->output,"{\"items\":",9)==0)state->observed=true;
    return state->observed?1:0;
}
struct PlanPublishCancel { const char *output=nullptr;size_t calls=0;bool observed=false; };
int cancel_after_staged_plan(void *context,uint64_t) {
    auto *state=static_cast<PlanPublishCancel*>(context);++state->calls;
    if(state->output&&std::memcmp(state->output,"{\"plan\":",8)==0)state->observed=true;
    return state->observed?1:0;
}
bool all_zero(const char *bytes,size_t length) {
    for(size_t i=0;i<length;++i)if(bytes[i])return false;
    return true;
}
mb_nexus_auth_result_v1 result() {
    mb_nexus_auth_result_v1 value{};value.struct_size=sizeof(value);value.abi_version=MB_NEXUS_AUTH_ABI_V1;return value;
}
int call_catalog(mb_nexus_auth_context_v1 *context,const char *cookie,size_t cookie_bytes,
                 mb_nexus_auth_catalog_kind_v1 kind,const char *domain,size_t domain_bytes,
                 uint64_t mod_id,char *out,size_t capacity,mb_nexus_auth_result_v1& received,
                 void *cancel_context=nullptr,mb_nexus_auth_cancel_fn_v1 cancelled=nullptr) {
    mb_nexus_auth_catalog_params_v1 params{};params.struct_size=sizeof(params);params.abi_version=MB_NEXUS_AUTH_ABI_V1;
    params.cookie=cookie;params.cookie_bytes=cookie_bytes;params.kind=kind;params.domain=domain;params.domain_bytes=domain_bytes;params.mod_id=mod_id;
    params.timeout_ms=5000;params.connect_timeout_ms=2000;params.cancel_context=cancel_context;params.cancelled=cancelled;
    params.output=out;params.output_capacity=capacity;
    received=result();return mb_nexus_auth_catalog_v1(context,&params,&received);
}
int call_acquisition_plan(mb_nexus_auth_context_v1 *context,const char *cookie,size_t cookie_bytes,
                          const char *csrf,size_t csrf_bytes,const char *origin,size_t origin_bytes,
                          const char *intent,size_t intent_bytes,char *out,size_t capacity,
                          mb_nexus_auth_result_v1& received,void *cancel_context=nullptr,
                          mb_nexus_auth_cancel_fn_v1 cancelled=nullptr) {
    mb_nexus_auth_acquisition_plan_params_v1 params{};
    params.struct_size=sizeof(params);params.abi_version=MB_NEXUS_AUTH_ABI_V1;
    params.cookie=cookie;params.cookie_bytes=cookie_bytes;params.csrf=csrf;params.csrf_bytes=csrf_bytes;
    params.origin=origin;params.origin_bytes=origin_bytes;params.intent=intent;params.intent_bytes=intent_bytes;
    params.timeout_ms=5000;params.connect_timeout_ms=2000;params.cancel_context=cancel_context;params.cancelled=cancelled;
    params.output=out;params.output_capacity=capacity;
    received=result();return mb_nexus_auth_acquisition_plan_v1(context,&params,&received);
}

struct CaptureGate {const char *plan,*capture;bool deadline=false,observed=false;};
int after_capture(void *context,uint64_t) {
    auto& gate=*static_cast<CaptureGate*>(context);
    if(std::memcmp(gate.plan,"{\"plan\":",8)||std::memcmp(gate.capture,"{\"items\":",9))return 0;
    gate.observed=true;
    if(gate.deadline){timespec delay{0,70000000};nanosleep(&delay,nullptr);return 0;}
    return 1;
}
int capture_checks(mb_nexus_auth_context_v1 *context,const char *cookie,const char *csrf,
                   const char *origin,const char *intent,const modbridge::Json& original_plan) {
    int failures=0;
    std::vector<char> plan(8192,'x'),metadata(MB_NEXUS_AUTH_ACQUISITION_CAPTURE_LIMIT,'x');
    mb_nexus_auth_acquisition_plan_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;
    p.cookie=cookie;p.cookie_bytes=43;p.csrf=csrf;p.csrf_bytes=43;p.origin=origin;p.origin_bytes=std::strlen(origin);
    p.intent=intent;p.intent_bytes=std::strlen(intent);p.timeout_ms=5000;p.connect_timeout_ms=2000;
    p.output=plan.data();p.output_capacity=plan.size();
    mb_nexus_auth_acquisition_capture_v1 capture{sizeof(capture),1,metadata.data(),metadata.size(),0};
    auto received=result();
    auto call=[&](){return mb_nexus_auth_acquisition_plan_capture_v1(context,&p,&capture,&received);};
    const unsigned before=requirements_posts;
    const auto rc=call();
    failures+=require(rc==MB_NEXUS_AUTH_OK&&received.complete&&capture.output_bytes>0,
                      "native acquisition capture publishes an owning successful snapshot");
    if(rc!=MB_NEXUS_AUTH_OK||!capture.output_bytes)return failures;
    const auto wire=modbridge::Json::parse(plan.data()).at("plan");
    const auto rows=modbridge::Json::parse(metadata.data()).at("items");
    const auto expected_requirements=modbridge::Json{{"game","townfall"},{"mod_id",95},
        {"discovery_complete",true},{"nexus_requirements",modbridge::Json::array()},
        {"dlc_requirements",modbridge::Json::array()},
        {"file_version_constraints_provided",false},{"installation_supported",false}};
    failures+=require(wire==original_plan&&rows.size()==1&&rows[0].at("game")=="townfall"&&
        rows[0].at("mod_id")==95&&rows[0].at("file_id")==5&&
        rows[0].at("metadata").at("requirements")==expected_requirements&&
        rows[0].at("metadata").at("source")==modbridge::Json{{"name","Fixture mod"},{"author","Fixture"},{"version","1.0"}}&&
        requirements_posts==before+1&&capture.output_bytes==std::strlen(metadata.data()),
        "capture preserves the public plan and exact empty requirements without a second fetch");

    constexpr char graph_intent[]=R"({"game":"townfall","mod_ids":[95],"file_choices":[{"mod_id":95,"file_id":5},{"mod_id":94,"file_id":4}],"file_policy":"explicit_only"})";
    capture_graph=true;p.intent=graph_intent;p.intent_bytes=sizeof(graph_intent)-1;
    const auto request_counts=requests;const unsigned graph_posts=requirements_posts;
    failures+=require(call()==MB_NEXUS_AUTH_OK&&received.complete,"captured prerequisite closure is ready");
    const auto graph=modbridge::Json::parse(metadata.data()).at("items");
    failures+=require(graph.size()==2&&graph[0].at("mod_id")==94&&graph[0].at("file_id")==4&&
        graph[1].at("mod_id")==95&&graph[1].at("file_id")==5&&
        graph[0].at("metadata").at("source").at("version")=="file-label"&&
        graph[1].at("metadata").at("requirements").at("nexus_requirements")[0]==modbridge::Json{
            {"id","owned-edge"},{"external_requirement",false},{"game","townfall"},{"mod_id","94"},
            {"name","Prerequisite display"},{"notes","Owned complete requirement notes"},
            {"url","https://example.invalid/owned-requirement"},{"satisfaction","unresolved"}},
        "capture keeps prerequisite order and full notes/url/name rows with selected-file version");
    bool one_fetch=requirements_posts==graph_posts+2;
    for(const auto *path:{"/v1/games/townfall/mods/94","/v1/games/townfall/mods/94/files",
                         "/v1/games/townfall/mods/95","/v1/games/townfall/mods/95/files"}) {
        auto it=request_counts.find(path);one_fetch=one_fetch&&requests[path]==(it==request_counts.end()?0:it->second)+1;
    }
    failures+=require(one_fetch,"capture requests each closure mod/files/requirements exactly once");
    capture_oversized=true;
    failures+=require(call()==MB_NEXUS_AUTH_OK&&received.complete,"oversized optional snapshot preserves ready plan");
    const auto oversized=modbridge::Json::parse(metadata.data()).at("items");
    failures+=require(oversized.size()==2&&oversized[0].at("metadata").is_object()&&
        oversized[1].at("metadata").is_null()&&modbridge::Json::parse(plan.data()).at("plan").at("acquisition_ready")==true,
        "oversized requirements stay unknown without publishing a truncated complete snapshot");
    capture_oversized=false;capture_graph=false;

    constexpr char nonready[]=R"({"game":"townfall","mod_ids":[97],"file_choices":[],"file_policy":"explicit_only"})";
    p.intent=nonready;p.intent_bytes=sizeof(nonready)-1;
    failures+=require(call()==MB_NEXUS_AUTH_OK&&modbridge::Json::parse(metadata.data()).at("items").empty(),
        "non-ready public plan exposes no capture prefix");
    p.intent=intent;p.intent_bytes=std::strlen(intent);
    for(bool deadline:{false,true}) {
        std::fill(plan.begin(),plan.end(),'x');std::fill(metadata.begin(),metadata.end(),'x');capture.output_bytes=99;
        CaptureGate gate{plan.data(),metadata.data(),deadline,false};p.cancel_context=&gate;p.cancelled=after_capture;
        p.timeout_ms=deadline?50:5000;p.connect_timeout_ms=deadline?50:2000;
        failures+=require(call()==(deadline?MB_NEXUS_AUTH_TIMEOUT:MB_NEXUS_AUTH_CANCELLED)&&gate.observed&&
            !received.complete&&!received.output_bytes&&!capture.output_bytes&&
            all_zero(plan.data(),plan.size())&&all_zero(metadata.data(),metadata.size()),
            deadline?"late deadline wipes both staged outputs":"late cancellation wipes both staged outputs");
    }
    p.cancel_context=nullptr;p.cancelled=nullptr;p.timeout_ms=5000;p.connect_timeout_ms=2000;
    capture.output_capacity=1;metadata[0]='x';std::fill(plan.begin(),plan.end(),'x');
    failures+=require(call()==MB_NEXUS_AUTH_OUTPUT_TOO_SMALL&&!received.output_bytes&&!capture.output_bytes&&
        all_zero(plan.data(),plan.size())&&metadata[0]==0,"short capture buffer rejects both outputs");
    capture.output_capacity=metadata.size();
    const unsigned refused_before=requirements_posts;p.csrf="wrong";p.csrf_bytes=5;
    std::fill(plan.begin(),plan.end(),'x');std::fill(metadata.begin(),metadata.end(),'x');
    failures+=require(call()==MB_NEXUS_AUTH_REFUSED&&!capture.output_bytes&&
        all_zero(plan.data(),plan.size())&&all_zero(metadata.data(),metadata.size())&&requirements_posts==refused_before,
        "authorization refusal clears capture before provider requests");
    p.csrf=csrf;p.csrf_bytes=43;
    for(unsigned alias=0;alias<4;++alias) {
        if(alias==0){capture.output=p.output;capture.output_capacity=p.output_capacity;}
        if(alias==1){capture.output=reinterpret_cast<char*>(&capture);capture.output_capacity=sizeof(capture);}
        if(alias==2){capture.output=reinterpret_cast<char*>(&received);capture.output_capacity=sizeof(received);}
        if(alias==3){capture.output=const_cast<char*>(p.intent);capture.output_capacity=p.intent_bytes;}
        failures+=require(call()==MB_NEXUS_AUTH_BAD_ARGUMENT&&requirements_posts==refused_before,
            "capture rejects overlapping caller and output ownership before discovery");
    }
    return failures;
}
}

extern "C" int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *params,
                                               mb_curl_provider_transfer_result_v1 *out) {
    if(!params||!out)return MB_CURL_DOWNLOAD_BAD_ARGUMENT;init_transfer_result(out);
    if(params->auth_mode!=MB_NEXUS_CLIENT_BEARER||params->credential_bytes!=access.size()||
       std::memcmp(params->credential,access.data(),access.size())||!params->ca_bytes||
       !params->url||!params->consume_complete_body||!params->body||
       (params->resolve&&std::string_view(params->resolve)!="api.nexusmods.com:443:127.0.0.1"&&
        std::string_view(params->resolve)!="users.nexusmods.com:443:127.0.0.1")) {
        transport_contract_ok=false;out->status=MB_CURL_DOWNLOAD_BAD_ARGUMENT;return out->status;
    }
    const auto body=response_for(params->url);
    if(body.empty()){transport_contract_ok=false;out->status=MB_CURL_DOWNLOAD_RESPONSE_REFUSED;return out->status;}
    return provide_body(params->consume_context,params->consume_complete_body,params->body,
                        params->body_capacity,body,out);
}

extern "C" int mb_curl_provider_post_transfer_v1(
        const mb_curl_provider_post_transfer_params_v1 *params,
        mb_curl_provider_transfer_result_v1 *out) {
    if(!params||!out)return MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    init_transfer_result(out);++requirements_posts;
    std::array<char,MB_NEXUS_REQUIREMENTS_REQUEST_LIMIT> expected{};size_t expected_bytes=0;
    const bool root_body=modbridge::nexus_requirements_body("townfall",95,expected,expected_bytes)&&
        params->request_body&&params->request_body_bytes==expected_bytes&&
        std::memcmp(params->request_body,expected.data(),expected_bytes)==0;
    const bool child_body=capture_graph&&modbridge::nexus_requirements_body("townfall",94,expected,expected_bytes)&&
        params->request_body&&params->request_body_bytes==expected_bytes&&
        std::memcmp(params->request_body,expected.data(),expected_bytes)==0;
    const bool body_ok=root_body||child_body;
    const auto& provider=params->provider;
    if(params->struct_size!=sizeof(*params)||params->abi_version!=MB_CURL_DOWNLOAD_ABI_V1||
       provider.struct_size!=sizeof(provider)||provider.abi_version!=MB_CURL_DOWNLOAD_ABI_V1||
       provider.auth_mode!=MB_NEXUS_CLIENT_BEARER||provider.credential_bytes!=access.size()||
       !provider.credential||std::memcmp(provider.credential,access.data(),access.size())||!provider.ca_bytes||
       !provider.url||std::string_view(provider.url)!="https://api.nexusmods.com/v2/graphql"||!body_ok||
       !provider.consume_complete_body||!provider.body||
       (provider.resolve&&std::string_view(provider.resolve)!="api.nexusmods.com:443:127.0.0.1")) {
        post_contract_ok=false;out->status=MB_CURL_DOWNLOAD_BAD_ARGUMENT;return out->status;
    }
    std::string body=R"({"data":{"modRequirements":{"nexusRequirements":{"nodes":[],"nodesCount":0,"totalCount":0},"dlcRequirements":[]}}})";
    if(capture_graph&&root_body) {
        auto value=modbridge::Json::parse(body);
        auto& page=value["data"]["modRequirements"]["nexusRequirements"];
        modbridge::Json row={{"id","owned-edge"},{"externalRequirement",false},{"gameId","townfall"},{"modId","94"},
            {"modName","Prerequisite display"},{"notes",capture_oversized?std::string(65536,'n'):"Owned complete requirement notes"},
            {"url","https://example.invalid/owned-requirement"}};
        page["nodes"].push_back(row);
        if(capture_oversized){row["id"]="second-owned-edge";page["nodes"].push_back(row);}
        page["nodesCount"]=page["nodes"].size();page["totalCount"]=page["nodes"].size();body=value.dump();
    }
    return provide_body(provider.consume_context,provider.consume_complete_body,provider.body,
                        provider.body_capacity,body,out);
}

extern "C" int mb_curl_oauth_code_transfer_v1(const mb_curl_oauth_code_transfer_params_v1 *params,
                                                 mb_curl_oauth_code_transfer_result_v1 *out) {
    if(!params||!out)return MB_CURL_DOWNLOAD_BAD_ARGUMENT;init_transfer_result(out);
    if(!params->form||!params->form_bytes||!params->ca_bytes||!params->body||!params->consume_complete_body||
       !params->resolve||std::string_view(params->resolve)!="users.nexusmods.com:443:127.0.0.1") {
        transport_contract_ok=false;out->status=MB_CURL_DOWNLOAD_BAD_ARGUMENT;return out->status;
    }
    constexpr std::string_view body=R"({"access_token":"fixture_access_token","refresh_token":"fixture_refresh_token","token_type":"Bearer","expires_in":3600})";
    return provide_body(params->consume_context,params->consume_complete_body,params->body,
                        params->body_capacity,body,out);
}

extern "C" void mb_curl_owner_fail_v1(void) {}
extern "C" int mb_nexus_resolve_download_v1(const mb_nexus_download_params_v1 *,
                                                mb_nexus_download_result_v1 *) noexcept {
    return MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
}

int main() {
    int failures=0;uint32_t entropy_counter=1;
    constexpr char origin[]="http://127.0.0.1:8123";
    constexpr char redirect[]="http://127.0.0.1:8123/auth/callback";
    constexpr char client[]="fixture-client";
    constexpr char scope[]="openid";
    constexpr char api_resolve[]="api.nexusmods.com:443:127.0.0.1";
    constexpr char users_resolve[]="users.nexusmods.com:443:127.0.0.1";
    constexpr uint8_t ca[]="fixture CA bytes";
    mb_nexus_auth_context_v1 *context=nullptr;
    mb_nexus_auth_create_params_v1 create{};create.struct_size=sizeof(create);create.abi_version=MB_NEXUS_AUTH_ABI_V1;
    create.origin=origin;create.origin_bytes=sizeof(origin)-1;create.client_id=client;create.client_id_bytes=sizeof(client)-1;
    create.redirect_uri=redirect;create.redirect_uri_bytes=sizeof(redirect)-1;create.scope=scope;create.scope_bytes=sizeof(scope)-1;
    create.ca_pem=ca;create.ca_bytes=sizeof(ca)-1;create.api_resolve=api_resolve;create.api_resolve_bytes=sizeof(api_resolve)-1;
    create.users_resolve=users_resolve;create.users_resolve_bytes=sizeof(users_resolve)-1;
    create.entropy_context=&entropy_counter;create.entropy_read32=entropy;
    auto received=result();
    failures+=require(mb_nexus_auth_create_v1(&create,&context,&received)==MB_NEXUS_AUTH_OK&&received.complete,"configured context create");
    char cookie[44]{},csrf[44]{};mb_nexus_auth_session_params_v1 session{};
    session.struct_size=sizeof(session);session.abi_version=MB_NEXUS_AUTH_ABI_V1;session.cookie_output=cookie;session.cookie_capacity=sizeof(cookie);session.csrf_output=csrf;session.csrf_capacity=sizeof(csrf);
    received=result();failures+=require(mb_nexus_auth_session_v1(context,&session,&received)==MB_NEXUS_AUTH_OK&&received.complete,"local session create");
    constexpr char simple_plan[]="{\"game\":\"townfall\",\"mod_ids\":[95],\"file_choices\":[{\"mod_id\":95,\"file_id\":5}],\"file_policy\":\"explicit_only\"}";
    std::array<char,1024> plan_output{};plan_output.fill('x');received=result();
    int plan_rc=call_acquisition_plan(context,cookie,43,csrf,43,origin,sizeof(origin)-1,
        simple_plan,sizeof(simple_plan)-1,plan_output.data(),plan_output.size(),received);
    failures+=require(plan_rc==MB_NEXUS_AUTH_RECONNECT_REQUIRED&&!received.complete&&
                      all_zero(plan_output.data(),plan_output.size())&&requirements_posts==0&&
                      requests["/v1/games/townfall/mods/95"]==0,
                      "acquisition plan requires an authorized account before provider calls");

    mb_nexus_auth_admit_params_v1 admission{};admission.struct_size=sizeof(admission);admission.abi_version=MB_NEXUS_AUTH_ABI_V1;
    admission.cookie=cookie;admission.cookie_bytes=43;admission.admission=MB_NEXUS_AUTH_ADMISSION_READ;admission.require_account=1;
    received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_RECONNECT_REQUIRED,"account gate before authorization");
    admission.require_account=0;received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_OK&&received.complete,"cookie-only read admission");
    admission.admission=MB_NEXUS_AUTH_ADMISSION_MUTATION;admission.csrf=csrf;admission.csrf_bytes=43;admission.origin=origin;admission.origin_bytes=sizeof(origin)-1;
    received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_OK,"cookie csrf origin mutation admission");
    admission.csrf="wrong";admission.csrf_bytes=5;received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_REFUSED,"wrong csrf refused");
    admission.csrf=csrf;admission.csrf_bytes=43;admission.origin="http://elsewhere.invalid";admission.origin_bytes=25;
    received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_REFUSED,"wrong origin refused");

    char auth_url[4096]{};mb_nexus_auth_start_params_v1 start{};start.struct_size=sizeof(start);start.abi_version=MB_NEXUS_AUTH_ABI_V1;
    start.cookie=cookie;start.cookie_bytes=43;start.csrf=csrf;start.csrf_bytes=43;start.origin=origin;start.origin_bytes=sizeof(origin)-1;
    start.authorization_url=auth_url;start.authorization_capacity=sizeof(auth_url);
    received=result();failures+=require(mb_nexus_auth_start_v1(context,&start,&received)==MB_NEXUS_AUTH_OK&&received.complete,"OAuth authorization state start");
    const std::string_view url(auth_url);const auto state_at=url.find("state=");
    const auto state_end=url.find('&',state_at);const auto state=state_at==std::string_view::npos?std::string_view{}:url.substr(state_at+6,state_end-(state_at+6));
    std::string callback="/auth/callback?state="+std::string(state)+"&code=fixture-code";
    char summary[8192]{};mb_nexus_auth_callback_params_v1 cb{};cb.struct_size=sizeof(cb);cb.abi_version=MB_NEXUS_AUTH_ABI_V1;
    cb.cookie=cookie;cb.cookie_bytes=43;cb.target=callback.data();cb.target_bytes=callback.size();cb.timeout_ms=5000;cb.connect_timeout_ms=2000;cb.summary=summary;cb.summary_capacity=sizeof(summary);
    received=result();failures+=require(mb_nexus_auth_callback_v1(context,&cb,&received)==MB_NEXUS_AUTH_OK&&received.complete&&received.credentials_present,"fake token exchange plus real live-user decoder");
    failures+=require(requests["/oauth/userinfo"]==1,"callback verified account through real typed client");
    admission.admission=MB_NEXUS_AUTH_ADMISSION_READ;admission.csrf=nullptr;admission.csrf_bytes=0;admission.origin=nullptr;admission.origin_bytes=0;admission.require_account=1;
    received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_OK&&received.credentials_present,"unexpired account admission");
    admission.admission=MB_NEXUS_AUTH_ADMISSION_MUTATION;admission.csrf=csrf;admission.csrf_bytes=43;admission.origin=origin;admission.origin_bytes=sizeof(origin)-1;
    received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_OK,"account-required mutation admission");
    admission.admission=MB_NEXUS_AUTH_ADMISSION_READ;admission.csrf=nullptr;admission.csrf_bytes=0;admission.origin=nullptr;admission.origin_bytes=0;

    plan_output.fill(0);received=result();
    const unsigned mod_before_plan=requests["/v1/games/townfall/mods/95"];
    const unsigned files_before_plan=requests["/v1/games/townfall/mods/95/files"];
    plan_rc=call_acquisition_plan(context,cookie,43,csrf,43,origin,sizeof(origin)-1,
        simple_plan,sizeof(simple_plan)-1,plan_output.data(),plan_output.size(),received);
    const auto simple_plan_json=modbridge::Json::parse(plan_output.data()).at("plan");
    failures+=require(plan_rc==MB_NEXUS_AUTH_OK&&received.complete&&
                      simple_plan_json.at("acquisition_ready").get<bool>()&&
                      simple_plan_json.at("selected_sources").size()==1&&
                      simple_plan_json.at("selected_sources")[0].at("mod_id")==95&&
                      simple_plan_json.at("selected_sources")[0].at("file_id")==5&&
                      simple_plan_json.at("selected_sources")[0].at("selection_basis")=="explicit_file"&&
                      simple_plan_json.at("selected_sources")[0].at("file_version")=="1.0"&&
                      simple_plan_json.at("download_authorized")==false&&
                      simple_plan_json.at("installation_supported")==false,
                      "acquisition plan publishes a selected source without download or install authority");
    failures+=require(requests["/v1/games/townfall/mods/95"]==mod_before_plan+1&&
                      requests["/v1/games/townfall/mods/95/files"]==files_before_plan+1&&
                      requirements_posts==1&&post_contract_ok,
                      "acquisition uses one visible mod read, one fixed requirements POST, and one file read");

    constexpr char hidden_plan[]="{\"game\":\"townfall\",\"mod_ids\":[97],\"file_choices\":[],\"file_policy\":\"unique_current_primary\"}";
    const unsigned hidden_files_before=requests["/v1/games/townfall/mods/97/files"];
    const unsigned hidden_requirements_before=requirements_posts;
    plan_output.fill('x');received=result();
    plan_rc=call_acquisition_plan(context,cookie,43,csrf,43,origin,sizeof(origin)-1,
        hidden_plan,sizeof(hidden_plan)-1,plan_output.data(),plan_output.size(),received);
    const auto hidden_plan_json=modbridge::Json::parse(plan_output.data()).at("plan");
    failures+=require(plan_rc==MB_NEXUS_AUTH_OK&&!hidden_plan_json.at("acquisition_ready").get<bool>()&&
                      hidden_plan_json.at("selected_sources").empty()&&
                      hidden_plan_json.at("unresolved")[0].at("reason")=="hidden_mod"&&
                      requests["/v1/games/townfall/mods/97/files"]==hidden_files_before&&
                      requirements_posts==hidden_requirements_before,
                      "hidden source is refused before its files or requirements are requested");

    constexpr char unknown_plan_key[]="{\"game\":\"townfall\",\"mod_ids\":[95],\"file_choices\":[],\"file_policy\":\"explicit_only\",\"url\":\"https://example.invalid\"}";
    const unsigned calls_before_bad_intent=requirements_posts;
    plan_output.fill('x');received=result();
    plan_rc=call_acquisition_plan(context,cookie,43,csrf,43,origin,sizeof(origin)-1,
        unknown_plan_key,sizeof(unknown_plan_key)-1,plan_output.data(),plan_output.size(),received);
    failures+=require(plan_rc==MB_NEXUS_AUTH_BAD_ARGUMENT&&all_zero(plan_output.data(),plan_output.size())&&
                      requirements_posts==calls_before_bad_intent,
                      "unknown intent properties fail before provider calls and wipe output");

    PlanPublishCancel plan_cancel{plan_output.data()};plan_output.fill('x');received=result();
    plan_rc=call_acquisition_plan(context,cookie,43,csrf,43,origin,sizeof(origin)-1,
        simple_plan,sizeof(simple_plan)-1,plan_output.data(),plan_output.size(),received,
        &plan_cancel,cancel_after_staged_plan);
    failures+=require(plan_cancel.observed&&plan_rc==MB_NEXUS_AUTH_CANCELLED&&!received.complete&&
                      received.output_bytes==0&&all_zero(plan_output.data(),plan_output.size()),
                      "acquisition final cancellation after plan staging wipes full caller output");

    std::array<char,65536> output{};received=result();
    int rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_GAMES,nullptr,0,0,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_OK&&received.complete&&modbridge::Json::parse(output.data()).at("items").size()==1,"games UI items projection");
    failures+=require(requests["/v1/games"]==1,"games fixed typed route");

    output.fill('x');PublishCancel publish_cancel{output.data()};received=result();
    rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_GAMES,nullptr,0,0,output.data(),output.size(),received,
                    &publish_cancel,cancel_after_staged_output);
    failures+=require(publish_cancel.observed&&rc==MB_NEXUS_AUTH_CANCELLED&&!received.complete&&
                      received.output_bytes==0&&all_zero(output.data(),output.size()),
                      "catalog final cancellation after output staging wipes the full caller buffer");

    output.fill(0);received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MODS,"townfall",8,0,output.data(),output.size(),received);
    const auto mods=modbridge::Json::parse(output.data());
    failures+=require(rc==MB_NEXUS_AUTH_OK&&received.complete&&mods.at("items").size()==1&&mods.at("items")[0].at("mod_id")==95,"latest mods filter unpublished unavailable adult and unknown");
    failures+=require(requests["/v1/games/townfall/mods/latest_added"]==1&&requests["/oauth/userinfo"]==2,"adult list uses fresh live preference only when needed");
    malformed_latest=true;output.fill('x');received=result();
    rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MODS,"townfall",8,0,output.data(),output.size(),received);
    malformed_latest=false;
    failures+=require(rc==MB_NEXUS_AUTH_PAYLOAD_FAILED&&!received.complete&&received.output_bytes==0&&output[0]==0,"malformed final record rejects entire catalog without prefix");

    output.fill(0);received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MOD,"townfall",8,95,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_OK&&received.complete&&modbridge::Json::parse(output.data()).at("item").at("mod_id")==95,"direct mod UI item projection");

    output.fill(0);received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_FILES,"townfall",8,95,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_OK&&received.complete&&modbridge::Json::parse(output.data()).at("items").size()==1,"files verifies parent mod and returns items");
    failures+=require(requests["/v1/games/townfall/mods/95"]==4&&requests["/v1/games/townfall/mods/95/files"]==3,"files request follows parent-mod visibility read");

    output.fill('x');received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MOD,"townfall",8,96,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_CONTENT_HIDDEN&&!received.complete&&received.output_bytes==0&&output[0]==0,"adult direct lookup hidden by live preference");
    failures+=require(requests["/v1/games/townfall/mods/96"]==1&&requests["/oauth/userinfo"]==3,"adult direct lookup obtains fresh preference");

    output.fill('x');received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_FILES,"townfall",8,97,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_CONTENT_HIDDEN&&!received.complete&&output[0]==0,"hidden parent mod prevents file lookup");
    failures+=require(requests["/v1/games/townfall/mods/97"]==2&&requests["/v1/games/townfall/mods/97/files"]==0,"hidden file parent stops before files route");
    output.fill('x');received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_FILES,"townfall",8,96,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_CONTENT_HIDDEN&&!received.complete&&output[0]==0,"adult parent mod prevents file lookup");
    failures+=require(requests["/v1/games/townfall/mods/96"]==2&&requests["/v1/games/townfall/mods/96/files"]==0,"adult file parent stops before files route");
    output.fill('x');received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MOD,"townfall",8,99,output.data(),output.size(),received);
    failures+=require(rc==MB_NEXUS_AUTH_CONTENT_HIDDEN&&!received.complete&&output[0]==0,"unknown adult status is hidden for direct mod");

    char tiny[1]={'x'};received=result();rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_GAMES,nullptr,0,0,tiny,sizeof(tiny),received);
    failures+=require(rc==MB_NEXUS_AUTH_OUTPUT_TOO_SMALL&&!received.complete&&received.output_bytes==0&&tiny[0]==0,"bounded output short buffer wipes without size disclosure");

    char invalid[64];std::memset(invalid,'x',sizeof(invalid));received=result();
    rc=call_catalog(context,cookie,43,MB_NEXUS_AUTH_CATALOG_MODS,"../townfall",11,0,invalid,sizeof(invalid),received);
    failures+=require(rc==MB_NEXUS_AUTH_BAD_ARGUMENT&&invalid[0]==0,"invalid identity rejected with valid output wiped");
    failures+=require(transport_contract_ok&&post_contract_ok,"fake transport observed only fixed bearer and requirements POST contracts");
    failures+=capture_checks(context,cookie,csrf,origin,simple_plan,simple_plan_json);

    mb_nexus_auth_disconnect_params_v1 disconnect{};disconnect.struct_size=sizeof(disconnect);disconnect.abi_version=MB_NEXUS_AUTH_ABI_V1;
    disconnect.cookie=cookie;disconnect.cookie_bytes=43;disconnect.csrf=csrf;disconnect.csrf_bytes=43;disconnect.origin=origin;disconnect.origin_bytes=sizeof(origin)-1;
    received=result();failures+=require(mb_nexus_auth_disconnect_v1(context,&disconnect,&received)==MB_NEXUS_AUTH_OK,"account disconnect");
    admission.require_account=1;received=result();failures+=require(mb_nexus_auth_admit_v1(context,&admission,&received)==MB_NEXUS_AUTH_RECONNECT_REQUIRED,"account gate after disconnect");
    failures+=require(mb_nexus_auth_free_v1(&context)==MB_NEXUS_AUTH_OK&&context==nullptr,"context secret cleanup");
    mb_nexus_auth_context_v1 *unconfigured=nullptr;mb_nexus_auth_create_params_v1 no_account{};
    no_account.struct_size=sizeof(no_account);no_account.abi_version=MB_NEXUS_AUTH_ABI_V1;no_account.origin=origin;
    no_account.origin_bytes=sizeof(origin)-1;no_account.entropy_context=&entropy_counter;no_account.entropy_read32=entropy;
    received=result();failures+=require(mb_nexus_auth_create_v1(&no_account,&unconfigured,&received)==MB_NEXUS_AUTH_OK&&!received.configured,"unconfigured context create");
    char local_cookie[44]{},local_csrf[44]{};mb_nexus_auth_session_params_v1 local_session{};
    local_session.struct_size=sizeof(local_session);local_session.abi_version=MB_NEXUS_AUTH_ABI_V1;
    local_session.cookie_output=local_cookie;local_session.cookie_capacity=sizeof(local_cookie);
    local_session.csrf_output=local_csrf;local_session.csrf_capacity=sizeof(local_csrf);
    received=result();failures+=require(mb_nexus_auth_session_v1(unconfigured,&local_session,&received)==MB_NEXUS_AUTH_OK,"unconfigured local session create");
    mb_nexus_auth_admit_params_v1 local_admission{};local_admission.struct_size=sizeof(local_admission);local_admission.abi_version=MB_NEXUS_AUTH_ABI_V1;
    local_admission.cookie=local_cookie;local_admission.cookie_bytes=43;local_admission.admission=MB_NEXUS_AUTH_ADMISSION_READ;local_admission.require_account=1;
    received=result();failures+=require(mb_nexus_auth_admit_v1(unconfigured,&local_admission,&received)==MB_NEXUS_AUTH_NOT_CONFIGURED,"account gate on unconfigured context");
    failures+=require(mb_nexus_auth_free_v1(&unconfigured)==MB_NEXUS_AUTH_OK&&unconfigured==nullptr,"unconfigured context cleanup");
    if(failures)return 1;
    std::cout<<"PASS auth-admission-and-catalog-acquisition controls="<<control_count<<" transport=fake-lower-provider actual-client=1 actual-decoder=1\n";
    return 0;
}
