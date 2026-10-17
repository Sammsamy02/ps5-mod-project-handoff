/* Offline actual jobs/store: only provider planning and transfer are fixtures. */
#include "../jobs.cpp"
#include <cassert>
#include <filesystem>
#include <iostream>

static std::string mode;
static unsigned plan_calls=0,capture_calls=0,entropy_calls=0;
static bool grant_required=false;
static const std::string payload="owned acquisition original fixture";
static Json metadata(uint64_t mod) {
    return {{"source",{{"name","Private prerequisite name"},{"author","Fixture author"},{"version","2.1"}}},
        {"requirements",{{"game","fixture"},{"mod_id",mod},{"discovery_complete",true},
            {"nexus_requirements",Json::array({{{"id","dependency"},{"external_requirement",true},
                {"game",""},{"mod_id",""},{"name","Original prerequisite"},{"notes","Keep this complete note"},
                {"url","https://fixture.invalid/prerequisite"},{"satisfaction","unresolved"}}})},
            {"dlc_requirements",Json::array({{{"game","fixture"},{"id","dlc-1"},{"name","DLC fixture"},
                {"notes","Captured DLC note"},{"satisfaction","unresolved"}}})},
            {"file_version_constraints_provided",false},{"installation_supported",false}}}};
}
static Json plan() {
    Json nodes=Json::array(),selected=Json::array();
    for(uint64_t mod:{1,2}) {
        nodes.push_back({{"game","fixture"},{"mod_id",mod},{"source_visible",true},{"discovery_complete",true}});
        selected.push_back({{"game","fixture"},{"mod_id",mod},{"file_id",mod+10},
            {"selection_basis","unique_current_primary"},{"file_version","2.1"}});
    }
    return {{"plan",{{"schema_version",1},{"scope","nexus_acquisition_metadata"},{"game","fixture"},
        {"file_policy","unique_current_primary"},{"compatibility","unresolved"},{"version_satisfaction","unresolved"},
        {"download_authorized",false},{"installation_supported",false},{"file_version_constraints_provided",false},
        {"acquisition_order_is_game_load_order",false},{"root_mod_ids",{2}}, {"acquisition_ready",true},
        {"discovery_complete",true},{"unresolved",Json::array()},{"relations",Json::array()},
        {"nodes",nodes},{"selected_sources",selected}}}};
}
static int planned(const mb_nexus_auth_acquisition_plan_params_v1*p,mb_nexus_auth_result_v1*r) {
    const auto bytes=plan().dump();assert(bytes.size()<p->output_capacity);
    memcpy(p->output,bytes.c_str(),bytes.size()+1);r->struct_size=sizeof(*r);r->abi_version=1;
    r->status=MB_NEXUS_AUTH_OK;r->complete=1;r->output_bytes=bytes.size();return MB_NEXUS_AUTH_OK;
}
extern "C" int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_admit_params_v1*,mb_nexus_auth_result_v1*r) noexcept {
    r->status=MB_NEXUS_AUTH_OK;r->complete=1;return MB_NEXUS_AUTH_OK;
}
extern "C" int mb_nexus_auth_acquisition_plan_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_acquisition_plan_params_v1*p,mb_nexus_auth_result_v1*r) noexcept {
    ++plan_calls;return planned(p,r);
}
extern "C" int mb_nexus_auth_acquisition_plan_capture_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_acquisition_plan_params_v1*p,mb_nexus_auth_acquisition_capture_v1*c,mb_nexus_auth_result_v1*r) noexcept {
    assert(c->struct_size==sizeof(*c)&&c->abi_version==MB_NEXUS_AUTH_ABI_V1);
    assert(c->output_capacity&&c->output_capacity<=MB_NEXUS_AUTH_ACQUISITION_CAPTURE_LIMIT);
    ++capture_calls;planned(p,r);
    Json items=Json::array();for(uint64_t mod:{1,2})items.push_back({{"game","fixture"},{"mod_id",mod},{"file_id",mod+10},{"metadata",metadata(mod)}});
    if(mode=="unknown")items[0]["metadata"]=nullptr;
    if(mode=="wrong_order")std::swap(items[0],items[1]);
    if(mode=="wrong_file")items[1]["file_id"]=99;
    if(mode=="missing")items.erase(1);
    if(mode=="oversize")items[1]["metadata"]["source"]["name"]=std::string(128*1024,'x');
    if(mode=="wrong_requirements")items[1]["metadata"]["requirements"]["mod_id"]=3;
    if(mode=="incomplete")items[1]["metadata"]["requirements"]["discovery_complete"]=false;
    if(mode=="unexpected")items[1]["archive_path"]="/private/forbidden";
    std::string bytes=Json{{"items",items}}.dump();
    if(mode=="duplicate")bytes.insert(1,"\"items\":[],");
    assert(bytes.size()<c->output_capacity);memcpy(c->output,bytes.c_str(),bytes.size()+1);c->output_bytes=bytes.size();
    if(mode=="bad_length")c->output_bytes=c->output_capacity;
    return MB_NEXUS_AUTH_OK;
}
extern "C" int mb_nexus_auth_resolve_download_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_resolve_params_v1*p,mb_nexus_auth_resolve_result_v1*r) noexcept {
    if(grant_required) {
        r->auth.status=MB_NEXUS_AUTH_REFUSED;r->download.status=MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;return MB_NEXUS_AUTH_REFUSED;
    }
    const char*uri="https://fixture.invalid/archive";strcpy(p->download->uri,uri);
    r->auth.status=MB_NEXUS_AUTH_OK;r->auth.complete=1;r->download.status=MB_NEXUS_DOWNLOAD_OK;
    r->download.complete=1;r->download.uri_bytes=strlen(uri);r->download.mod_id=p->download->mod_id;
    r->download.file_id=p->download->file_id;r->download.user_id=1;return MB_NEXUS_AUTH_OK;
}
extern "C" int mb_curl_download_v1(const mb_curl_download_params_v1*p,mb_curl_download_result_v1*r) {
    assert(write(p->sink.fd,payload.data(),payload.size())==ssize_t(payload.size()));assert(!fsync(p->sink.fd));
    r->status=MB_CURL_DOWNLOAD_OK;r->complete=1;r->received_bytes=payload.size();
    r->file.status=MB_DOWNLOAD_FILE_SINK_OK;r->file.complete=r->file.readback_verified=1;
    r->file.bytes_received=r->file.bytes_written=payload.size();
    picosha2::hash256(payload.begin(),payload.end(),std::begin(r->file.sha256),std::end(r->file.sha256));return 0;
}
extern "C" int mb_archive_analyze_v1(const mb_archive_analysis_params_v1*,mb_archive_analysis_result_v1*) noexcept {return MB_ARCHIVE_ANALYSIS_BAD_ARGUMENT;}
static int entropy(void*,uint8_t out[32]) {memset(out,++entropy_calls,32);return 0;}
struct Fixture {
    std::filesystem::path scratch,retained;
    int sf=-1,rf=-1;
    mb_download_store*store=nullptr;
    mb_nexus_jobs_context_v1*c=nullptr;
    std::string cookie=std::string(43,'a'),csrf=std::string(43,'b'),origin="https://local.fixture";
    std::string intent=R"({"game":"fixture","mod_ids":[2],"file_choices":[],"file_policy":"unique_current_primary","idempotency_key":"metadata_test_001"})";
    std::array<char,MB_NEXUS_JOBS_OUTPUT_LIMIT> output{};
    mb_nexus_jobs_result_v1 result{};
    Fixture(const char*base):scratch(std::filesystem::path(base)/"scratch"),retained(std::filesystem::path(base)/"retained") {
        std::filesystem::create_directory(scratch);std::filesystem::create_directory(retained);
        chmod(scratch.c_str(),0700);chmod(retained.c_str(),0700);
        sf=open(scratch.c_str(),O_RDONLY|O_DIRECTORY);rf=open(retained.c_str(),O_RDONLY|O_DIRECTORY);
        assert(!mb_download_store_open(rf,nullptr,&store));
        mb_nexus_jobs_create_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.auth=(mb_nexus_auth_context_v1*)1;
        p.scratch_directory_fd=sf;p.execution_context=MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION;const uint8_t ca='x';p.ca_pem=&ca;p.ca_bytes=1;
        p.resolver_timeout_ms=p.download_timeout_ms=p.analysis_timeout_ms=3000;p.connect_timeout_ms=100;p.archive_cap_bytes=1024;p.entropy_read32=entropy;
        assert(!mb_nexus_jobs_create_v1(&p,&c,&result));assert(!mb_nexus_jobs_configure_download_store_v1(c,store,&result));
    }
    ~Fixture(){assert(!mb_nexus_jobs_free_v1(&c));assert(std::filesystem::is_empty(scratch));mb_download_store_free(store);close(rf);close(sf);}
    int submit(bool direct=false) {
        mb_nexus_jobs_submit_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=cookie.size();
        p.csrf=csrf.data();p.csrf_bytes=csrf.size();p.origin=origin.data();p.origin_bytes=origin.size();
        p.intent=(const uint8_t*)intent.data();p.intent_bytes=intent.size();p.output=output.data();p.output_capacity=output.size();
        return direct?mb_nexus_jobs_submit_v1(c,&p,&result):mb_nexus_jobs_submit_acquisition_v1(c,&p,&result);
    }
    int resume(const char*id) {
        std::string grant=R"({"nxm":"nxm://fixture/mods/1/files/11?key=owned&expires=4102444800&user_id=1"})";
        mb_nexus_jobs_resume_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=cookie.size();
        p.csrf=csrf.data();p.csrf_bytes=csrf.size();p.origin=origin.data();p.origin_bytes=origin.size();p.id=id;p.id_bytes=32;
        p.intent=(const uint8_t*)grant.data();p.intent_bytes=grant.size();p.output=output.data();p.output_capacity=output.size();
        return mb_nexus_jobs_resume_v1(c,&p,&result);
    }
};
int main(int argc,char**argv) {
    assert(argc==3);mode=argv[2];Fixture f(argv[1]);
    if(mode=="direct") {
        f.intent=R"({"kind":"download_analyze","game":"fixture","mod_id":1,"file_id":11,"idempotency_key":"direct_metadata_001"})";
        assert(!f.submit(true));assert(capture_calls==0&&plan_calls==0);
        assert(Json::parse(f.output.data())["retention"]["metadata_state"]=="unknown");
        assert(!mb_nexus_jobs_run_one_v1(f.c,&f.result));
        assert(public_job(f.c->records[0])["retention"]["metadata_state"]=="unknown");return 0;
    }
    if(mode=="wrong_order"||mode=="wrong_file"||mode=="missing"||mode=="oversize"||mode=="wrong_requirements"||
       mode=="incomplete"||mode=="unexpected"||mode=="duplicate"||mode=="bad_length") {
        assert(f.submit()==MB_NEXUS_JOBS_SOURCE_FAILED);assert(f.c->count==0&&f.c->acquisition_count==0&&entropy_calls==0);
        assert(f.output[0]==0&&std::filesystem::is_empty(f.scratch)&&std::filesystem::is_empty(f.retained));return 0;
    }
    assert(!f.submit());assert(capture_calls==1&&plan_calls==0&&f.c->count==2);
    auto first=Json::parse(f.output.data());assert(f.output[0]&&std::string(f.output.data()).find("Private prerequisite")==std::string::npos);
    assert(public_job(f.c->records[0])["retention"]["metadata_state"]==(mode=="unknown"?"unknown":"pending"));
    assert(!f.submit());assert(capture_calls==1&&entropy_calls==2&&Json::parse(f.output.data())==first);
    if(mode=="grant_retry") {
        grant_required=true;assert(!mb_nexus_jobs_run_one_v1(f.c,&f.result));
        assert(public_job(f.c->records[0])["status"]=="needs_nexus_authorization");
        assert(public_job(f.c->records[0])["retention"]["metadata_state"]=="pending");
        std::string id=f.c->records[0].id;assert(!f.resume(id.c_str()));
        assert(!mb_nexus_jobs_run_one_v1(f.c,&f.result));assert(!f.resume(id.c_str()));
        grant_required=false;assert(capture_calls==1);
    }
    std::string failed_id;
    if(mode=="sidecar_failure") {
        // A final incompatible sidecar blocks metadata alone. Publish the same
        // original first so its real store-owned identity determines the name.
        const auto path=std::filesystem::path(argv[1])/"original";
        int fd=open(path.c_str(),O_RDWR|O_CREAT|O_EXCL,0600);assert(fd>=0);
        assert(write(fd,payload.data(),payload.size())==ssize_t(payload.size()));assert(!fsync(fd));
        close(fd);fd=open(path.c_str(),O_RDONLY);assert(fd>=0);
        char receipt[4096]{};const auto sha=picosha2::hash256_hex_string(payload);
        assert(!mb_download_store_publish(f.store,fd,"fixture",1,11,payload.size(),sha.c_str(),receipt,sizeof(receipt)));close(fd);
        failed_id=Json::parse(receipt)["download_id"].get<std::string>();
        assert(std::filesystem::create_directory(f.retained/(failed_id+".metadata.json")));
    }
    assert(!mb_nexus_jobs_run_one_v1(f.c,&f.result));assert(!mb_nexus_jobs_run_one_v1(f.c,&f.result));
    std::vector<std::string> ids;
    for(size_t i=0;i<2;++i) {
        const auto job=public_job(f.c->records[i]);assert(job["retention"]["state"]=="saved");
        assert(job["error"]["code"]=="analysis_failed");
        const char*state=i==0&&mode=="unknown"?"unknown":i==0&&mode=="sidecar_failure"?"failed":"saved";
        assert(job["retention"]["metadata_state"]==state);
        assert(job.dump().find("Private prerequisite")==std::string::npos&&job.dump().find("Captured DLC")==std::string::npos);
        ids.push_back(job["retention"]["download_id"].get<std::string>());
    }
    assert(!f.submit());assert(capture_calls==1&&entropy_calls==2&&Json::parse(f.output.data())==first);
    assert(!mb_nexus_jobs_free_v1(&f.c));assert(std::filesystem::is_empty(f.scratch));
    mb_download_store_free(f.store);f.store=nullptr;assert(!mb_download_store_open(f.rf,nullptr,&f.store));
    char listed[8192]{};assert(!mb_download_store_list(f.store,listed,sizeof(listed)));
    const auto list=Json::parse(listed);assert(list["items"].size()==2&&std::string(listed).find("prerequisite")==std::string::npos);
    std::array<char,MB_DOWNLOAD_STORE_METADATA_LIMIT+1> captured{};
    for(size_t i=0;i<2;++i) {
        const auto status=mb_download_store_lookup_metadata(f.store,ids[i].c_str(),captured.data(),captured.size());
        if(i==0&&mode=="unknown")assert(status==MB_DOWNLOAD_STORE_NOT_FOUND&&captured[0]==0);
        else if(i==0&&mode=="sidecar_failure")assert(status==MB_DOWNLOAD_STORE_INTEGRITY_FAILED&&captured[0]==0);
        else {assert(status==MB_DOWNLOAD_STORE_OK);assert(Json::parse(captured.data())==metadata(i+1));}
        char original[4096]{};assert(mb_download_store_lookup(f.store,ids[i].c_str(),original,sizeof(original))==MB_DOWNLOAD_STORE_PATH_UNAVAILABLE);
    }
    std::cout<<"admission, retry, saved originals and private metadata reopen checked\n";
}
