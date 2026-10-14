#include "test.hpp"
#include "modbridge/core.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <thread>
using namespace modbridge;
namespace {
Json entry(std::string path, std::string kind, Json requirements = Json::array()) { return {{"path",path},{"kind",kind},{"sha256","source-sha"},{"requirements",requirements}}; }
Json inventory(Json entries) { return {{"entries",entries},{"dependencies",Json::array()}}; }
Json target() { return {{"fingerprints",{{"executable","target-sha"}}},{"capabilities",Json::array()}}; }
Json backend(std::string id="asset.converter", std::string kind="asset") { return {{"id",id},{"version","1"},{"mode","transformer"},{"kinds",Json::array({kind})},{"capabilities",Json::array()},{"target_fingerprints",Json::array({"executable"})}}; }
Json evidence() { return {{"id","probe"},{"version","1"},{"status","validated"},{"context","in-game"},{"scope",Json::array({"execute"})},{"source_hash","source-sha"},{"target_fingerprints",{{"executable","target-sha"}}}}; }
Json capability(std::string version="1.2") { return {{"id","runtime.lua"},{"version",version},{"status","validated"},{"context","in-game"},{"evidence",evidence()}}; }
Json context() { return {{"backend_id","asset.converter"},{"backend_version","1"},{"core_parser_version","0.1.0"},{"schema_version",1},{"source_hash","source-sha"},{"options",Json::object()},{"dependency_hashes",{{"dep","dep-sha"}}},{"target_fingerprints",{{"executable","target-sha"}}},{"mapping_versions",{{"map","1"}}},{"evidence",Json::array({evidence()})}}; }
}
TEST(same_content_different_mod_id) {
 auto inv=inventory(Json::array({entry("a","asset")})); inv["provider_mod_id"]="first";
 auto a=build_plan(inv,target(),Json::array({backend()})); inv["provider_mod_id"]="unfamiliar";
 CHECK_EQ(a,build_plan(inv,target(),Json::array({backend()}))); CHECK_EQ(a["status"],"planned"); CHECK_EQ(a["stage"],"analyzed"); CHECK_EQ(a["eligible_steps"].size(),1u); CHECK_EQ(a["eligible_steps"][0]["status"],"eligible");
}
TEST(mixed_missing_runtime) {
 auto inv=inventory(Json::array({entry("asset","asset"),entry("script","lua",Json::array({"runtime.lua"}))}));
 auto result=build_plan(inv,target(),Json::array({backend(),backend("lua.converter","lua")}));
 CHECK_EQ(result["status"],"unresolved"); CHECK_EQ(result["eligible_steps"].size(),1u); CHECK(!result["unresolved"].empty());
 auto t=target(); t["capabilities"].push_back(capability());
 CHECK_EQ(build_plan(inv,t,Json::array({backend(),backend("lua.converter","lua")}))["status"],"planned");
}
TEST(unknown_target) {
 auto inv=inventory(Json::array({entry("asset","asset")}));
 CHECK_EQ(build_plan(inv,Json::object(),Json::array({backend()}))["status"],"unresolved");
 CHECK_EQ(build_plan(inv,target(),Json::array())["status"],"unresolved");
 CHECK_EQ(build_plan(inventory(Json::array()),Json::object(),Json::array())["status"],"unresolved");
}
TEST(analyzer_cannot_transform) {
 auto b=backend(); b["mode"]="analyzer";
 auto p=build_plan(inventory(Json::array({entry("asset","asset")})),target(),Json::array({b}));
 CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty());
}
TEST(ambiguous_backends) {
 auto p=build_plan(inventory(Json::array({entry("asset","asset")})),target(),Json::array({backend("second"),backend("first")}));
 CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty()); CHECK_EQ(p["unresolved"][0]["code"],"ambiguous_backend");
 CHECK_EQ(p,build_plan(inventory(Json::array({entry("asset","asset")})),target(),Json::array({backend("first"),backend("second")})));
}
TEST(reference_cycle_vs_order_cycle) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","resource_reference"}},{{"from","b"},{"to","a"},{"type","resource_reference"}}});
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p["status"],"planned"); CHECK_EQ(p["reference_groups"].size(),1u);
 b["supports_reference_cycles"]=false; CHECK_EQ(build_plan(inv,target(),Json::array({b}))["status"],"unresolved");
 for(auto& edge:inv["dependencies"]) edge["type"]="ordering";
 b["supports_reference_cycles"]=true; p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty()); CHECK(!p["conflicts"].empty());
}
TEST(version_constraints) {
 auto inv=inventory(Json::array({entry("script","lua",Json::array({{{"id","runtime.lua"},{"version",">=1.2"},{"context","in-game"}}}))}));
 auto t=target(); t["capabilities"].push_back(capability("1.10")); auto bs=Json::array({backend("lua.converter","lua")});
 CHECK_EQ(build_plan(inv,t,bs)["status"],"planned");
 t["capabilities"][0]["version"]="1.1"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
 inv["entries"][0]["requirements"][0]["version"]="^1.0"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
 inv["entries"][0]["requirements"][0]["version"]="1.1"; CHECK_EQ(build_plan(inv,t,bs)["status"],"planned");
 t["capabilities"][0]["context"]="standalone"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
}
TEST(capability_evidence_scope_and_fingerprint) {
 auto inv=inventory(Json::array({entry("a","lua",Json::array({{{"id","runtime.lua"},{"context","in-game"},{"scope",Json::array({"execute"})}}}))}));
 auto t=target(); t["capabilities"].push_back(capability()); auto bs=Json::array({backend("lua.converter","lua")});
 CHECK_EQ(build_plan(inv,t,bs)["status"],"planned");
 t["capabilities"][0]["evidence"]["target_fingerprints"]["executable"]="other"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
 t["capabilities"][0]=capability(); t["capabilities"][0]["evidence"]["source_hash"]="other"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
 t["capabilities"][0]=capability(); t["capabilities"][0]["evidence"]["status"]="revoked"; CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
 t["capabilities"][0]=capability(); t["capabilities"][0]["evidence"]["scope"]=Json::array({"list"}); CHECK_EQ(build_plan(inv,t,bs)["status"],"unresolved");
}
TEST(malformed_records_do_not_satisfy) {
 auto inv=inventory(Json::array({entry("a","asset",Json::array({{{"id",true}}}))}));
 CHECK_THROWS(build_plan(inv,target(),Json::array({backend()})));
 auto b=backend(); b["kinds"]="asset"; CHECK_THROWS(build_plan(inventory(Json::array({entry("a","asset")})),target(),Json::array({b})));
 inv=inventory(Json::array({entry("a","asset")})); inv["dependencies"]=Json::array({{{"from","a"},{"to","absent"},{"type","resource_reference"}}});
 CHECK_EQ(build_plan(inv,target(),Json::array({backend()}))["status"],"unresolved");
}
TEST(conflicts_and_dependency_propagation) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","unknown")})); inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","native_import"}}});
 auto p=build_plan(inv,target(),Json::array({backend()})); CHECK(p["eligible_steps"].empty());
 inv["entries"][1]["kind"]="asset"; inv["dependencies"][0]["type"]="installation_conflict";
 p=build_plan(inv,target(),Json::array({backend()})); CHECK_EQ(p["status"],"unresolved"); CHECK(!p["conflicts"].empty()); CHECK(p["eligible_steps"].empty());
}
TEST(cache_every_input) {
 auto c=context(); auto key=cache_key(c); CHECK_EQ(key.size(),64u);
 for(auto field:{"backend_id","backend_version","core_parser_version","source_hash"}) { auto changed=c; changed[field]="different"; CHECK(cache_key(changed)!=key); }
 auto changed=c; changed["schema_version"]=2; CHECK(cache_key(changed)!=key);
 for(auto field:{"options","dependency_hashes","target_fingerprints","mapping_versions"}) { changed=c; changed[field]["new"]="changed"; CHECK(cache_key(changed)!=key); }
 changed=c; changed["evidence"][0]["version"]="2"; CHECK(cache_key(changed)!=key);
 Json reordered=Json::object(); for(auto it=c.rbegin();it!=c.rend();++it) reordered[it.key()]=it.value(); CHECK_EQ(cache_key(reordered),key);
 for(auto it=c.begin();it!=c.end();++it) { changed=c; changed.erase(it.key()); CHECK_THROWS(cache_key(changed)); }
 changed=c; changed["dependency_hashes"]=Json::array(); CHECK_THROWS(cache_key(changed)); changed=c; changed["options"]=7; CHECK_THROWS(cache_key(changed));
}
TEST(revoked_evidence) {
 auto c=context(); CHECK(cache_evidence_usable(c,c));
 for(auto status:{"revoked","candidate","unknown","unavailable"}) { auto current=c; current["evidence"][0]["status"]=status; CHECK(!cache_evidence_usable(c,current)); }
 auto current=c; current["evidence"]=Json::array(); CHECK(!cache_evidence_usable(c,current));
 for(auto field:{"context","source_hash","version"}) { current=c; current["evidence"][0][field]="different"; CHECK(!cache_evidence_usable(c,current)); }
 current=c; current["evidence"][0]["scope"]=Json::array({"different"}); CHECK(!cache_evidence_usable(c,current));
 current=c; current["target_fingerprints"]["executable"]="changed"; CHECK(!cache_evidence_usable(c,current));
 CHECK(!cache_evidence_usable(c,Json::object()));
}
TEST(version_conflicts_report_involved_nodes) {
 auto a=entry("a","asset",Json::array({{{"id","runtime.lua"},{"version","1"},{"context","in-game"}}}));
 auto b=entry("b","asset",Json::array({{{"id","runtime.lua"},{"version",">=2"},{"context","in-game"}}}));
 auto t=target(); t["capabilities"].push_back(capability("1"));
 auto p=build_plan(inventory(Json::array({a,b})),t,Json::array({backend()}));
 CHECK_EQ(p["status"],"unresolved"); CHECK(!p["conflicts"].empty()); CHECK(p["eligible_steps"].empty()); CHECK_EQ(p["conflicts"][0]["code"],"version_conflict");
}
TEST(mixed_strict_reference_cycle) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","reference"}},{{"from","b"},{"to","a"},{"type","order"}}});
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty()); CHECK_EQ(p["conflicts"][0]["code"],"ordering_cycle");
 inv["dependencies"][1]["type"]="runtime"; p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty());
}
TEST(unhandled_requirement_constraints_are_unresolved) {
 auto inv=inventory(Json::array({entry("a","asset",Json::array({{{"id","runtime.lua"},{"abi","unknown-abi"}}}))}));
 auto t=target(); t["capabilities"].push_back(capability());
 CHECK_EQ(build_plan(inv,t,Json::array({backend()}))["status"],"unresolved");
 inv["entries"][0]["requirements"][0]={{"id","runtime.lua"},{"consumer",false}};
 CHECK_THROWS(build_plan(inv,t,Json::array({backend()})));
}
TEST(inventory_manifest_dependency_aliases) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","order"}}});
 auto p=build_plan(inv,target(),Json::array({backend()})); CHECK_EQ(p["status"],"planned"); CHECK_EQ(p["ordered_components"][0],"b"); CHECK_EQ(p["ordered_components"][1],"a");
 inv["dependencies"][0]["type"]="conflict"; p=build_plan(inv,target(),Json::array({backend()})); CHECK(!p["conflicts"].empty()); CHECK(p["eligible_steps"].empty());
}
TEST(unbound_package_requirements_stay_unresolved) {
 auto inv=inventory(Json::array()); inv["requirements"]=Json::array({"runtime.lua"});
 CHECK_EQ(build_plan(inv,target(),Json::array({backend()}))["status"],"unresolved");
 inv=inventory(Json::array({entry("a","asset")})); inv["requirements"]=Json::array({{{"id","runtime.lua"},{"consumer","missing"}}});
 CHECK_EQ(build_plan(inv,target(),Json::array({backend()}))["status"],"unresolved");
}
TEST(backend_unsupported_constraints_cannot_be_ignored) {
 auto b=backend(); b["platform"]="another-platform";
 CHECK_EQ(build_plan(inventory(Json::array({entry("a","asset")})),target(),Json::array({b}))["status"],"unresolved");
}
TEST(ambiguous_capability_is_unresolved) {
 auto t=target(); t["capabilities"]=Json::array({capability(),capability()});
 auto inv=inventory(Json::array({entry("a","asset",Json::array({"runtime.lua"}))}));
 CHECK_EQ(build_plan(inv,t,Json::array({backend()}))["status"],"unresolved");
}
TEST(required_source_and_target_identity) {
 auto inv=inventory(Json::array({entry("a","asset")})); inv["entries"][0].erase("sha256");
 CHECK_EQ(build_plan(inv,target(),Json::array({backend()}))["status"],"unresolved");
 auto b=backend(); b["target_fingerprints"].push_back("asset_schema");
 CHECK_EQ(build_plan(inventory(Json::array({entry("a","asset")})),target(),Json::array({b}))["status"],"unresolved");
}
TEST(backend_requirement_version_conflicts) {
 auto inv=inventory(Json::array({entry("a","asset",Json::array({{{"id","runtime.lua"},{"version","1"},{"context","in-game"}}}))}));
 auto b=backend(); b["capabilities"]=Json::array({{{"id","runtime.lua"},{"version","2"},{"context","in-game"}}});
 auto t=target(); t["capabilities"]=Json::array({capability("1"),capability("2")});
 auto p=build_plan(inv,t,Json::array({b})); CHECK_EQ(p["status"],"unresolved"); CHECK(p["eligible_steps"].empty()); CHECK(!p["conflicts"].empty());
}
TEST(reference_group_blocked_by_external_dependency) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset"),entry("missing","unknown")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","reference"}},{{"from","b"},{"to","a"},{"type","reference"}},{{"from","a"},{"to","missing"},{"type","runtime"}}});
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b})); CHECK(p["eligible_steps"].empty()); CHECK_EQ(p["reference_groups"][0]["status"],"unresolved");
}

namespace {
void owned_zip(const std::filesystem::path& path, const std::vector<std::pair<std::string,std::string>>& files) {
 auto* writer=archive_write_new(); CHECK(writer!=nullptr);
 CHECK_EQ(archive_write_set_format_zip(writer),ARCHIVE_OK);
 CHECK_EQ(archive_write_open_filename(writer,path.c_str()),ARCHIVE_OK);
 for(const auto& [name,content]:files) {
  auto* item=archive_entry_new(); archive_entry_set_pathname(item,name.c_str());
  archive_entry_set_filetype(item,AE_IFREG); archive_entry_set_perm(item,0600); archive_entry_set_size(item,content.size());
  CHECK_EQ(archive_write_header(writer,item),ARCHIVE_OK); CHECK_EQ(archive_write_data(writer,content.data(),content.size()),static_cast<la_ssize_t>(content.size())); archive_entry_free(item);
 }
 CHECK_EQ(archive_write_close(writer),ARCHIVE_OK); CHECK_EQ(archive_write_free(writer),ARCHIVE_OK);
}
Json opaque_data_target() {
 auto t=target(); auto cap=capability("1"); cap["id"]="format.data_semantics"; cap["context"]="preparation";
 cap["evidence"]["context"]="preparation"; cap["evidence"].erase("source_hash"); t["capabilities"].push_back(cap); return t;
}
bool inventory_issue(const Json& inv,const std::string& code) { for(const auto& issue:inv.at("issues")) if(issue.at("code")==code) return true; return false; }
bool plan_preserves_issue(const Json& p,const std::string& code) { for(const auto& unresolved:p.at("unresolved")) if(unresolved.contains("issue") && unresolved.at("issue").at("code")==code) return true; return false; }
}
TEST(actual_rejected_empty_inventory_is_unresolved) {
 test::TempDir d; owned_zip(d.path/"unsafe.zip",{{"../escape","owned"}});
 auto inv=analyze_package(d.path/"unsafe.zip",Limits{}); CHECK(inv.at("entries").empty()); CHECK(inventory_issue(inv,"unsafe_path"));
 auto p=build_plan(inv,target(),Json::array()); CHECK_EQ(p.at("status"),"unresolved"); CHECK(plan_preserves_issue(p,"unsafe_path"));
 owned_zip(d.path/"empty.zip",{}); inv=analyze_package(d.path/"empty.zip",Limits{}); CHECK(inv.at("inventory_complete").get<bool>()); CHECK(inv.at("issues").empty());
 CHECK_EQ(build_plan(inv,target(),Json::array()).at("status"),"planned");
}
TEST(actual_partial_inventory_cannot_plan_complete_package) {
 test::TempDir d; auto bs=Json::array({backend("data.converter","data")});
 owned_zip(d.path/"limited.zip",{{"small.txt","yes"},{"big.txt",std::string(64,'x')}});
 Limits limits; limits.max_entry_bytes=16; auto inv=analyze_package(d.path/"limited.zip",limits);
 CHECK_EQ(inv.at("entries").size(),1u); CHECK(inventory_issue(inv,"max_entry_bytes"));
 auto p=build_plan(inv,opaque_data_target(),bs); CHECK_EQ(p.at("status"),"unresolved"); CHECK(plan_preserves_issue(p,"max_entry_bytes"));
 owned_zip(d.path/"collision.zip",{{"A.txt","first"},{"a.txt","second"}}); inv=analyze_package(d.path/"collision.zip",Limits{});
 CHECK_EQ(inv.at("entries").size(),1u); CHECK(inventory_issue(inv,"path_collision"));
 p=build_plan(inv,opaque_data_target(),bs); CHECK_EQ(p.at("status"),"unresolved"); CHECK(plan_preserves_issue(p,"path_collision"));
}
TEST(actual_mutated_input_provenance_stays_unresolved) {
 test::TempDir d; auto path=d.path/"changing.zip";
 {std::ofstream f(path,std::ios::binary); std::string block(65536,'x'); for(int i=0;i<128;++i) f.write(block.data(),block.size());}
 std::atomic<bool> stop=false,started=false;
 std::thread writer([&]{while(!stop.load()){std::fstream f(path,std::ios::in|std::ios::out|std::ios::binary); f.put('q'); f.flush(); started=true; std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
 while(!started.load()) std::this_thread::yield();
 Json inv; try {inv=analyze_package(path,Limits{});} catch(...){stop=true;writer.join();throw;}
 stop=true;writer.join(); CHECK(inventory_issue(inv,"input_mutation")); CHECK_EQ(inv.at("provenance").at("input_stable"),false);
 auto p=build_plan(inv,target(),Json::array()); CHECK_EQ(p.at("status"),"unresolved"); CHECK(plan_preserves_issue(p,"input_mutation"));
}
TEST(inventory_failure_fields_are_typed) {
 auto inv=inventory(Json::array());
 for(const auto& bad:Json::array({"false",0,Json::object()})) {inv["inventory_complete"]=bad; CHECK_THROWS(build_plan(inv,target(),Json::array()));}
 inv=inventory(Json::array()); inv["provenance"]={{"input_stable","false"}}; CHECK_THROWS(build_plan(inv,target(),Json::array()));
 inv["provenance"]=false; CHECK_THROWS(build_plan(inv,target(),Json::array()));
 inv=inventory(Json::array()); inv["issues"]=Json::object(); CHECK_THROWS(build_plan(inv,target(),Json::array()));
 inv["issues"]=Json::array({{{"code",false}}}); CHECK_THROWS(build_plan(inv,target(),Json::array()));
 inv=inventory(Json::array()); inv["package_sha256"]=false; CHECK_THROWS(build_plan(inv,target(),Json::array()));
}
TEST(selected_backend_versions_conflict_across_package) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","lua"),entry("dependent","plain")}));
 inv["dependencies"]=Json::array({{{"from","dependent"},{"to","a"},{"type","runtime"}}});
 auto first=backend("first","asset"),second=backend("second","lua");
 first["capabilities"]=Json::array({{{"id","runtime.lua"},{"version","1"},{"context","in-game"}}});
 second["capabilities"]=Json::array({{{"id","runtime.lua"},{"version","2"},{"context","in-game"}}});
 auto t=target(); t["capabilities"]=Json::array({capability("1"),capability("2")});
 auto p=build_plan(inv,t,Json::array({first,second,backend("plain","plain")}));
 CHECK_EQ(p.at("status"),"unresolved"); CHECK(p.at("eligible_steps").empty()); CHECK_EQ(p.at("conflicts")[0].at("code"),"version_conflict");
 CHECK_EQ(p.at("conflicts")[0].at("components"),Json::array({"a","b"}));
 second["capabilities"]=Json::array(); inv["entries"][1]["requirements"]=Json::array({{{"id","runtime.lua"},{"version","2"},{"context","in-game"}}});
 p=build_plan(inv,t,Json::array({first,second,backend("plain","plain")})); CHECK_EQ(p.at("status"),"unresolved"); CHECK(p.at("eligible_steps").empty()); CHECK(!p.at("conflicts").empty());
 inv["entries"][1]["requirements"][0].erase("context");
 p=build_plan(inv,t,Json::array({first,second,backend("plain","plain")})); CHECK_EQ(p.at("status"),"unresolved"); CHECK(p.at("eligible_steps").empty());
}
TEST(dependency_constraints_are_not_discarded) {
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","runtime"},{"version",">=999"},{"abi","other"}}});
 auto p=build_plan(inv,target(),Json::array({backend()})); CHECK_EQ(p.at("status"),"unresolved"); CHECK_EQ(p.at("eligible_steps").size(),1u);
 CHECK_EQ(p.at("unresolved")[0].at("code"),"unsupported_dependency_constraint"); CHECK_EQ(p.at("unresolved")[0].at("dependency"),inv.at("dependencies")[0]);
 inv["dependencies"][0]={{"from","a"},{"to","b"},{"type","runtime"},{"metadata",{{"note","observational only"}}}};
 CHECK_EQ(build_plan(inv,target(),Json::array({backend()})).at("status"),"planned");
 inv["dependencies"][0]["metadata"]=false; CHECK_THROWS(build_plan(inv,target(),Json::array({backend()})));
}
TEST(manifest_dependency_constraints_reach_planner) {
 test::TempDir d; std::ofstream(d.path/"a.txt")<<"first";std::ofstream(d.path/"b.txt")<<"second";
 std::ofstream(d.path/"modbridge.json")<<R"({"dependencies":[{"from":"a.txt","to":"b.txt","type":"runtime","version":">=999","abi":"other"}]})";
 auto inv=analyze_package(d.path,Limits{}); CHECK(inv.at("inventory_complete").get<bool>()); CHECK(inv.at("issues").empty()); CHECK_EQ(inv.at("dependencies")[0].at("abi"),"other");
 auto p=build_plan(inv,opaque_data_target(),Json::array({backend("data.converter","data")})); CHECK_EQ(p.at("status"),"unresolved");
 CHECK_EQ(p.at("unresolved")[0].at("code"),"unsupported_dependency_constraint"); CHECK_EQ(p.at("unresolved")[0].at("dependency"),inv.at("dependencies")[0]);
}

TEST(cycle_membership_is_shared_and_identity_is_stable) {
 // Catches membership copied per node, missing links, or identities depending on input order.
 auto inv=inventory(Json::array({entry("a","asset"),entry("b","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","b"},{"type","reference"}},{{"from","b"},{"to","a"},{"type","reference"}}});
 const std::string group_id="cycle-0473ef2dc0d324ab659d3580c1134e9d812035905c4781fdd6d529b0c6860e13";
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b}));
 CHECK_EQ(p.at("reference_groups")[0].at("group_id"),group_id);
 CHECK_EQ(p.at("reference_groups")[0].at("components"),Json::array({"a","b"}));
 for(const auto& step:p.at("eligible_steps")) CHECK_EQ(step.at("group_id"),group_id);
 std::reverse(inv["entries"].begin(),inv["entries"].end()); std::reverse(inv["dependencies"].begin(),inv["dependencies"].end());
 inv["dependencies"].push_back(inv["dependencies"][0]);
 CHECK_EQ(build_plan(inv,target(),Json::array({b})),p);
 b["supports_reference_cycles"]=false; p=build_plan(inv,target(),Json::array({b}));
 CHECK_EQ(p.at("reference_groups")[0].at("group_id"),group_id);
 for(const auto& item:p.at("unresolved")) { CHECK_EQ(item.at("group_id"),group_id); CHECK(!item.contains("components")); }
 inv["dependencies"][0]["type"]="order"; p=build_plan(inv,target(),Json::array({b}));
 CHECK_EQ(p.at("conflicts")[0].at("group_id"),group_id);
 CHECK_EQ(p.at("conflicts")[0].at("components"),Json::array({"a","b"}));
 for(const auto& item:p.at("unresolved")) { CHECK_EQ(item.at("group_id"),group_id); CHECK(!item.contains("components")); }
}
TEST(self_loops_preserve_reference_permission_and_strict_rejection) {
 auto inv=inventory(Json::array({entry("a","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","a"},{"type","reference"}}});
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p.at("status"),"planned");
 CHECK_EQ(p.at("ordered_components"),Json::array({"a"}));
 CHECK_EQ(p.at("reference_groups")[0].at("group_id"),"cycle-0eb5b8d6f81bc677da8a08567cc4fa9a06a57e9ec8da85ed73a7f62727996002");
 inv["dependencies"][0]["type"]="native_import"; p=build_plan(inv,target(),Json::array({b}));
 CHECK_EQ(p.at("conflicts")[0].at("code"),"dependency_cycle"); CHECK(p.at("eligible_steps").empty());
 inv["dependencies"][0]["type"]="order"; p=build_plan(inv,target(),Json::array({b}));
 CHECK_EQ(p.at("conflicts")[0].at("code"),"ordering_cycle"); CHECK(p.at("eligible_steps").empty());
}
TEST(blocked_closure_uses_shortest_lexical_causal_witness) {
 // Catches witnesses selected by edge order or pointing to an equally distant blocked peer.
 auto inv=inventory(Json::array({entry("root","asset"),entry("a","asset"),entry("b","asset"),entry("failed","unknown")}));
 inv["dependencies"]=Json::array({{{"from","root"},{"to","b"},{"type","runtime"}},{{"from","b"},{"to","failed"},{"type","runtime"}},
   {{"from","root"},{"to","a"},{"type","runtime"}},{{"from","a"},{"to","failed"},{"type","runtime"}},{{"from","a"},{"to","b"},{"type","reference"}}});
 auto p=build_plan(inv,target(),Json::array({backend()})); CHECK(p.at("eligible_steps").empty());
 std::map<std::string,std::string> witnesses;
 for(const auto& item:p.at("unresolved")) if(item.at("code")=="dependency_unresolved") witnesses[item.at("component")]=item.at("dependency");
 CHECK_EQ(witnesses.at("root"),"a"); CHECK_EQ(witnesses.at("a"),"failed"); CHECK_EQ(witnesses.at("b"),"failed");
 std::reverse(inv["dependencies"].begin(),inv["dependencies"].end()); CHECK_EQ(build_plan(inv,target(),Json::array({backend()})),p);
}
TEST(large_blocked_chain_closes_every_dependent) {
 auto inv=inventory(Json::array());
 for(int i=0;i<6500;++i) { auto name="node-"+std::to_string(i); inv["entries"].push_back(entry(name,i==6499?"unknown":"asset"));
  if(i!=6499) inv["dependencies"].push_back({{"from",name},{"to","node-"+std::to_string(i+1)},{"type","runtime"}}); }
 auto p=build_plan(inv,target(),Json::array({backend()})); CHECK_EQ(p.at("status"),"unresolved"); CHECK(p.at("eligible_steps").empty());
 CHECK_EQ(p.at("unresolved").size(),6500u);
 for(const auto& item:p.at("unresolved")) if(item.at("code")=="dependency_unresolved") {
  auto n=std::stoi(item.at("component").get<std::string>().substr(5)); CHECK_EQ(item.at("dependency"),"node-"+std::to_string(n+1));
 }
}
TEST(iterative_order_retains_lexical_postorder_with_reference_cycles) {
 auto inv=inventory(Json::array({entry("z","asset"),entry("a","asset"),entry("b","asset"),entry("c","asset"),entry("d","asset")}));
 inv["dependencies"]=Json::array({{{"from","a"},{"to","c"},{"type","order"}},{{"from","a"},{"to","b"},{"type","reference"}},
  {{"from","b"},{"to","a"},{"type","reference"}},{{"from","c"},{"to","d"},{"type","runtime"}}});
 auto b=backend(); b["supports_reference_cycles"]=true;
 auto p=build_plan(inv,target(),Json::array({b})); CHECK_EQ(p.at("ordered_components"),Json::array({"b","d","c","a","z"}));
 std::reverse(inv["dependencies"].begin(),inv["dependencies"].end()); CHECK_EQ(build_plan(inv,target(),Json::array({b})),p);
}

#include "dds_fixture.hpp"
// A lossless host DDS must still be blocked without validated writer/game evidence.
TEST(dds_parser_never_grants_install_plan) {
    test::TempDir d;auto b=dds_fixture::legacy();
    {std::ofstream f(d.path/"asset",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
    auto inv=analyze_package(d.path,Limits{});CHECK_EQ(inv.at("inventory_complete"),true);
    CHECK_EQ(inv.at("entries")[0].at("facts").at("normal_form_eligible"),true);
    auto p=build_plan(inv,target(),Json::array({backend("dds.converter","dds_texture")}));
    CHECK_EQ(p.at("status"),"unresolved");CHECK(p.at("eligible_steps").empty());
    std::set<std::string> ids;
    for(const auto& r:inv.at("requirements"))ids.insert(r.at("id"));
    CHECK(ids.contains("texture.ps5_writer"));CHECK(ids.contains("texture.game_acceptance"));
}
