#include "modbridge/core.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace modbridge {
namespace {
void require(bool condition, const std::string& why) { if(!condition) throw std::invalid_argument("planner: "+why); }
std::string string_field(const Json& o, const char* field) {
    require(o.is_object() && o.contains(field) && o.at(field).is_string() && !o.at(field).get_ref<const std::string&>().empty(), std::string("missing or invalid ")+field);
    return o.at(field).get<std::string>();
}
void array_field(const Json& o, const char* field) { require(o.contains(field) && o.at(field).is_array(),std::string(field)+" must be an array"); }
void fingerprints(const Json& f) {
    require(f.is_object(),"fingerprints must be an object");
    for(auto it=f.begin();it!=f.end();++it) require(!it.key().empty() && it.value().is_string() && !it.value().get_ref<const std::string&>().empty(),"invalid fingerprint");
}
void scope(const Json& s) { require(s.is_array(),"scope must be an array"); for(const auto& x:s) require(x.is_string() && !x.get_ref<const std::string&>().empty(),"invalid scope item"); }
void context(const Json& c) { require((c.is_string() && !c.get_ref<const std::string&>().empty()) || (c.is_object() && !c.empty()),"context must be explicit string or object"); }
Json normalize_requirement(const Json& r) {
    if(r.is_string()) { require(!r.get_ref<const std::string&>().empty(),"empty requirement"); return {{"id",r}}; }
    string_field(r,"id");
    if(r.contains("version")) string_field(r,"version");
    for(auto field:{"consumer","discovery","status"}) if(r.contains(field)) string_field(r,field);
    if(r.contains("context")) context(r.at("context"));
    if(r.contains("scope")) scope(r.at("scope"));
    if(r.contains("evidence")) require(r.at("evidence").is_object(),"requirement evidence must be an object");
    return r;
}
// Numeric dotted comparison uses decimal strings, avoiding overflow for long components.
bool numeric_version(const std::string& version, std::vector<std::string>& parts) {
    if(version.empty()) return false;
    std::string part;
    for(char ch:version) {
        if(ch=='.') { if(part.empty()) return false; parts.push_back(part); part.clear(); }
        else if(ch>='0' && ch<='9') part+=ch;
        else return false;
    }
    if(part.empty()) return false; parts.push_back(part);
    for(auto& p:parts) { auto pos=p.find_first_not_of('0'); p=pos==std::string::npos ? "0" : p.substr(pos); }
    return true;
}
int compare_version(std::vector<std::string> a, std::vector<std::string> b) {
    a.resize(std::max(a.size(),b.size()),"0"); b.resize(a.size(),"0");
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].size()!=b[i].size()) return a[i].size()<b[i].size() ? -1 : 1;
        if(a[i]!=b[i]) return a[i]<b[i] ? -1 : 1;
    }
    return 0;
}
bool constraint_supported(const std::string& c) {
    if(c.starts_with(">=")) { std::vector<std::string> parts; return numeric_version(c.substr(2),parts); }
    // Exact versions can contain ordinary version labels, but no constraint operators or whitespace.
    return !c.empty() && c.find_first_of("<>=^~*| ,\t\n")==std::string::npos;
}
bool matches_version(const std::string& version, const std::string& constraint) {
    if(!constraint_supported(constraint)) return false;
    if(!constraint.starts_with(">=")) return version==constraint;
    std::vector<std::string> a,b; return numeric_version(version,a) && numeric_version(constraint.substr(2),b) && compare_version(a,b)>=0;
}
bool requirements_conflict(const Json& a, const Json& b) {
    if(a.at("id")!=b.at("id") || !a.contains("version") || !b.contains("version")) return false;
    if(a.contains("context") && b.contains("context") && a.at("context")!=b.at("context")) return false;
    const auto av=a.at("version").get<std::string>(), bv=b.at("version").get<std::string>();
    if(!constraint_supported(av) || !constraint_supported(bv)) return false;
    if(!av.starts_with(">=") && !bv.starts_with(">=")) return av!=bv;
    if(!av.starts_with(">=")) return !matches_version(av,bv);
    if(!bv.starts_with(">=")) return !matches_version(bv,av);
    return false;
}
void validate_evidence(const Json& e) {
    require(e.is_object(),"capability evidence must be an object");
    for(auto field:{"id","version","status"}) string_field(e,field);
    require(e.contains("context") && e.contains("scope") && e.contains("target_fingerprints"),"capability evidence missing scope/identity");
    context(e.at("context")); scope(e.at("scope")); fingerprints(e.at("target_fingerprints"));
    if(e.contains("source_hash")) string_field(e,"source_hash");
}
bool capability_matches(const Json& cap, const Json& r, const Json& target, const Json& entry) {
    if(cap.at("id")!=r.at("id") || cap.at("status")!="validated") return false;
    if(r.contains("version") && !matches_version(cap.at("version"),r.at("version"))) return false;
    if(r.contains("context") && cap.at("context")!=r.at("context")) return false;
    const auto& e=cap.at("evidence");
    if(e.at("status")!="validated" || e.at("context")!=cap.at("context") || e.at("scope").empty() || e.at("target_fingerprints").empty()) return false;
    for(auto it=e.at("target_fingerprints").begin();it!=e.at("target_fingerprints").end();++it)
        if(!target.at("fingerprints").contains(it.key()) || target.at("fingerprints").at(it.key())!=it.value()) return false;
    if(e.contains("source_hash") && e.at("source_hash")!=entry.at("sha256")) return false;
    if(r.contains("scope")) for(const auto& s:r.at("scope")) if(std::find(e.at("scope").begin(),e.at("scope").end(),s)==e.at("scope").end()) return false;
    if(r.contains("evidence")) for(auto it=r.at("evidence").begin();it!=r.at("evidence").end();++it) if(!e.contains(it.key()) || e.at(it.key())!=it.value()) return false;
    return true;
}
struct Edge { std::string from,to,type; };
struct GraphFrame { std::string node; std::size_t next_neighbor=0; };
std::vector<std::vector<std::string>> cyclic_groups(const std::map<std::string,std::vector<std::string>>& graph) {
    std::map<std::string,std::size_t> index,low; std::set<std::string> on_stack;
    std::vector<std::string> scc_stack; std::vector<GraphFrame> frames;
    std::vector<std::vector<std::string>> groups; std::size_t next=0;
    auto enter=[&](const std::string& node) {
        index[node]=low[node]=next++; scc_stack.push_back(node); on_stack.insert(node);
        frames.push_back({node,0});
    };
    // The traversal stack and Tarjan's SCC stack have separate lifetimes: a
    // finished child may remain on the SCC stack until its ancestor closes.
    for(const auto& [root,_]:graph) {
        if(index.contains(root)) continue;
        enter(root);
        while(!frames.empty()) {
            auto& frame=frames.back(); const auto& neighbors=graph.at(frame.node);
            if(frame.next_neighbor<neighbors.size()) {
                const auto& child=neighbors[frame.next_neighbor++];
                if(!index.contains(child)) enter(child);
                else if(on_stack.contains(child)) low[frame.node]=std::min(low.at(frame.node),index.at(child));
                continue;
            }
            const auto node=frame.node;
            if(low.at(node)==index.at(node)) {
                std::vector<std::string> group;
                do { auto member=scc_stack.back(); scc_stack.pop_back(); on_stack.erase(member); group.push_back(member); } while(group.back()!=node);
                if(group.size()>1 || std::binary_search(neighbors.begin(),neighbors.end(),node)) {
                    std::sort(group.begin(),group.end()); groups.push_back(std::move(group));
                }
            }
            frames.pop_back();
            if(!frames.empty()) {
                const auto& parent=frames.back().node; low[parent]=std::min(low.at(parent),low.at(node));
            }
        }
    }
    std::sort(groups.begin(),groups.end()); return groups;
}
} // namespace

Json build_plan(const Json& inventory, const Json& target_input, const Json& backends_input) {
    require(inventory.is_object(),"inventory must be an object"); array_field(inventory,"entries");
    require(target_input.is_object(),"target must be an object"); require(backends_input.is_array(),"backends must be an array");
    Json target=target_input;
    if(!target.contains("fingerprints")) target["fingerprints"]=Json::object();
    if(!target.contains("capabilities")) target["capabilities"]=Json::array();
    fingerprints(target.at("fingerprints")); array_field(target,"capabilities");
    for(const auto& cap:target.at("capabilities")) {
        for(auto field:{"id","version","status"}) string_field(cap,field);
        require(cap.contains("context"),"capability missing context"); context(cap.at("context"));
        // Unknown/unavailable capabilities may lack evidence, but cannot satisfy any requirement.
        if(cap.at("status")=="validated") { require(cap.contains("evidence"),"validated capability missing evidence"); validate_evidence(cap.at("evidence")); }
        else if(cap.contains("evidence")) validate_evidence(cap.at("evidence"));
    }
    std::vector<Json> backends; std::set<std::string> backend_ids;
    for(const auto& raw:backends_input) {
        Json b=raw; auto id=string_field(b,"id"); string_field(b,"version"); auto mode=string_field(b,"mode");
        require(mode=="analyzer" || mode=="transformer" || mode=="runtime","invalid backend mode");
        require(backend_ids.insert(id).second,"duplicate backend id");
        for(auto field:{"kinds","capabilities","target_fingerprints"}) array_field(b,field);
        for(const auto& k:b.at("kinds")) require(k.is_string() && !k.get_ref<const std::string&>().empty(),"invalid backend kind");
        for(auto& r:b["capabilities"]) r=normalize_requirement(r);
        for(const auto& f:b.at("target_fingerprints")) require(f.is_string() && !f.get_ref<const std::string&>().empty(),"invalid backend fingerprint declaration");
        if(b.contains("supports_reference_cycles")) require(b.at("supports_reference_cycles").is_boolean(),"supports_reference_cycles must be boolean");
        backends.push_back(std::move(b));
    }
    std::sort(backends.begin(),backends.end(),[](const Json& a,const Json& b){return a.at("id")<b.at("id");});
    Json p={{"schema_version",record_schema_version},{"stage","analyzed"},{"status","planned"},{"eligible_steps",Json::array()},{"unresolved",Json::array()},{"conflicts",Json::array()},{"reference_groups",Json::array()},{"ordered_components",Json::array()}};
    std::map<std::string,Json> entries,selected; std::set<std::string> blocked;
    auto unresolved=[&](const std::string& node,const std::string& code,Json details=Json::object()) { details["component"]=node; details["code"]=code; p["unresolved"].push_back(details); if(!node.empty()) blocked.insert(node); };
    Json package_requirements=Json::array();
    if(inventory.contains("requirements")) { array_field(inventory,"requirements"); for(const auto& r:inventory.at("requirements")) package_requirements.push_back(normalize_requirement(r)); }
    for(const auto& raw:inventory.at("entries")) {
        Json e=raw; auto name=string_field(e,"path"); string_field(e,"kind");
        // Missing content identity is an unresolved preparation input, not a fabricated hash.
        if(e.contains("sha256")) string_field(e,"sha256"); else e["sha256"]="";
        require(!entries.contains(name),"duplicate component path");
        if(!e.contains("requirements")) e["requirements"]=Json::array(); array_field(e,"requirements");
        for(auto& r:e["requirements"]) r=normalize_requirement(r);
        entries.emplace(name,std::move(e));
    }
    for(const auto& r:package_requirements) {
        if(entries.empty() || (r.contains("consumer") && !entries.contains(r.at("consumer").get<std::string>())))
            unresolved("","unbound_package_requirement",{{"requirement",r}});
    }
    if(target.at("fingerprints").empty()) unresolved("","unknown_target");
    if(inventory.contains("status")) {
        string_field(inventory,"status");
        if(inventory.at("status")!="analyzed") unresolved("","inventory_unresolved");
    }
    if(inventory.contains("inventory_complete")) {
        require(inventory.at("inventory_complete").is_boolean(),"inventory_complete must be boolean");
        if(!inventory.at("inventory_complete").get<bool>()) unresolved("","inventory_incomplete");
    }
    if(inventory.contains("issues")) {
        array_field(inventory,"issues");
        for(const auto& issue:inventory.at("issues")) {
            string_field(issue,"code");
            for(auto field:{"path","detail"}) if(issue.contains(field)) require(issue.at(field).is_string(),std::string("inventory issue ")+field+" must be string");
            unresolved("","inventory_issue",{{"issue",issue}});
        }
    }
    if(inventory.contains("provenance")) {
        const auto& provenance=inventory.at("provenance");
        require(provenance.is_object() && provenance.contains("input_stable") && provenance.at("input_stable").is_boolean(),"provenance requires boolean input_stable");
        if(!provenance.at("input_stable").get<bool>()) unresolved("","input_provenance_unstable",{{"provenance",provenance}});
    }
    if(inventory.contains("package_sha256")) {
        if(inventory.at("package_sha256").is_null()) unresolved("","missing_package_identity");
        else string_field(inventory,"package_sha256");
    }
    if(inventory.contains("unresolved")) { array_field(inventory,"unresolved"); if(!inventory.at("unresolved").empty()) unresolved("","inventory_unresolved",{{"details",inventory.at("unresolved")}}); }
    auto missing_requirements=[&](const Json& requirements,const Json& e) {
        Json missing=Json::array();
        for(std::size_t i=0;i<requirements.size();++i) for(std::size_t j=i+1;j<requirements.size();++j)
            if(requirements_conflict(requirements.at(i),requirements.at(j)))
                missing.push_back({{"code","version_conflict"},{"requirements",Json::array({requirements.at(i),requirements.at(j)})}});
        for(const auto& r:requirements) {
            const std::set<std::string> supported={"id","version","context","scope","evidence","consumer","discovery","status","provenance"};
            Json unhandled=Json::array(); for(auto it=r.begin();it!=r.end();++it) if(!supported.contains(it.key())) unhandled.push_back(it.key());
            if(!unhandled.empty()) { missing.push_back({{"requirement",r},{"code","unsupported_requirement_constraint"},{"fields",unhandled}}); continue; }
            if(r.contains("version") && !constraint_supported(r.at("version"))) { missing.push_back({{"requirement",r},{"code","unsupported_version_constraint"}}); continue; }
            std::size_t matches=0; for(const auto& c:target.at("capabilities")) if(capability_matches(c,r,target,e)) ++matches;
            if(matches!=1) missing.push_back({{"requirement",r},{"code",matches>1?"ambiguous_capability":"missing_capability"}});
        }
        return missing;
    };
    for(const auto& [name,e]:entries) {
        if(blocked.contains(name)) continue;
        if(e.at("sha256")=="") { unresolved(name,"missing_source_hash"); continue; }
        if(target.at("fingerprints").empty()) { unresolved(name,"unknown_target"); continue; }
        auto reqs=e.at("requirements"); for(const auto& r:package_requirements) if(!r.contains("consumer") || r.at("consumer")==name) reqs.push_back(r);
        auto missing=missing_requirements(reqs,e);
        if(!missing.empty()) { unresolved(name,"missing_requirements",{{"requirements",missing}}); continue; }
        std::vector<Json> matches; Json rejected=Json::array();
        for(const auto& b:backends) {
            if(std::find(b.at("kinds").begin(),b.at("kinds").end(),e.at("kind"))==b.at("kinds").end()) continue;
            const std::set<std::string> supported={"id","version","mode","kinds","capabilities","target_fingerprints","supports_reference_cycles","metadata","provenance"};
            Json unhandled=Json::array(); for(auto it=b.begin();it!=b.end();++it) if(!supported.contains(it.key())) unhandled.push_back(it.key());
            if(!unhandled.empty()) { rejected.push_back({{"backend_id",b.at("id")},{"code","unsupported_backend_constraint"},{"fields",unhandled}}); continue; }
            if(b.at("mode")=="analyzer") { rejected.push_back({{"backend_id",b.at("id")},{"code","analyzer_only"}}); continue; }
            Json absent=Json::array(); for(const auto& f:b.at("target_fingerprints")) if(!target.at("fingerprints").contains(f.get<std::string>())) absent.push_back(f);
            auto combined=reqs; for(const auto& r:b.at("capabilities")) combined.push_back(r);
            auto requirements=missing_requirements(combined,e);
            if(!absent.empty() || !requirements.empty()) rejected.push_back({{"backend_id",b.at("id")},{"missing_fingerprints",absent},{"requirements",requirements}});
            else matches.push_back(b);
        }
        if(matches.empty()) {
            for(const auto& candidate:rejected) if(candidate.contains("requirements")) for(const auto& r:candidate.at("requirements"))
                if(r.at("code")=="version_conflict") p["conflicts"].push_back({{"code","version_conflict"},{"components",Json::array({name})},{"backend_id",candidate.at("backend_id")},{"requirements",r.at("requirements")}});
            unresolved(name,"missing_backend",{{"candidates",rejected}});
        }
        else if(matches.size()>1) { Json ids=Json::array(); for(const auto& b:matches) ids.push_back(b.at("id")); unresolved(name,"ambiguous_backend",{{"backend_ids",ids}}); }
        else selected[name]=matches.front();
    }
    // Compare the complete selected package: a backend's requirement has the
    // same compatibility meaning as a requirement discovered in an entry.
    std::map<std::string,std::vector<Json>> version_requirements;
    auto collect_requirement=[&](const std::string& name,const Json& r,const std::string& origin,const std::string& backend_id="") {
        if(!r.contains("version") || !constraint_supported(r.at("version"))) return;
        auto identity=r.at("id").get<std::string>();
        Json record={{"component",name},{"requirement",r},{"origin",origin}};
        if(!backend_id.empty()) record["backend_id"]=backend_id;
        version_requirements[identity].push_back(std::move(record));
    };
    for(const auto& [name,e]:entries) {
        for(const auto& r:e.at("requirements")) collect_requirement(name,r,"entry");
        for(const auto& r:package_requirements) if(!r.contains("consumer") || r.at("consumer")==name) collect_requirement(name,r,"package");
        if(selected.contains(name)) for(const auto& r:selected.at(name).at("capabilities")) collect_requirement(name,r,"backend",selected.at(name).at("id"));
    }
    for(const auto& [capability_id,requirements]:version_requirements) {
        std::set<std::size_t> involved;
        for(std::size_t i=0;i<requirements.size();++i) for(std::size_t j=i+1;j<requirements.size();++j)
            if(requirements_conflict(requirements.at(i).at("requirement"),requirements.at(j).at("requirement"))) {
                involved.insert(i); involved.insert(j);
            }
        if(!involved.empty()) {
            std::set<std::string> nodes; Json conflicting_requirements=Json::array();
            for(auto index:involved) {
                nodes.insert(requirements.at(index).at("component").get<std::string>());
                conflicting_requirements.push_back(requirements.at(index));
            }
            p["conflicts"].push_back({{"code","version_conflict"},{"capability",capability_id},{"components",nodes},{"requirements",conflicting_requirements}});
            for(const auto& n:nodes) unresolved(n,"version_conflict");
        }
    }
    std::vector<Edge> edges;
    const std::set<std::string> reference_types={"reference","resource_reference","runtime_reference"};
    const std::set<std::string> order_types={"ordering","order","load_order","initialization_order"};
    if(inventory.contains("dependencies")) {
        array_field(inventory,"dependencies");
        for(const auto& edge:inventory.at("dependencies")) {
            Edge e{string_field(edge,"from"),string_field(edge,"to"),string_field(edge,"type")};
            require(entries.contains(e.from),"dependency consumer does not exist");
            for(auto field:{"metadata","provenance"}) if(edge.contains(field)) require(edge.at(field).is_object(),std::string("dependency ")+field+" must be an object");
            const std::set<std::string> supported={"from","to","type","metadata","provenance"};
            Json unhandled=Json::array();
            for(auto it=edge.begin();it!=edge.end();++it) if(!supported.contains(it.key())) unhandled.push_back(it.key());
            if(!unhandled.empty()) unresolved(e.from,"unsupported_dependency_constraint",{{"dependency",edge},{"fields",unhandled}});
            if(!entries.contains(e.to)) { unresolved(e.from,"missing_dependency",{{"dependency",e.to},{"type",e.type}}); continue; }
            if(e.type=="installation_conflict" || e.type=="conflict") { p["conflicts"].push_back(edge); unresolved(e.from,"installation_conflict"); unresolved(e.to,"installation_conflict"); continue; }
            if(!reference_types.contains(e.type) && !order_types.contains(e.type) && e.type!="runtime" && e.type!="required_runtime" && e.type!="native_import") { unresolved(e.from,"unknown_dependency_type",{{"type",e.type}}); continue; }
            edges.push_back(e);
        }
    }
    std::map<std::string,std::vector<std::string>> all_dependencies,dependents;
    for(const auto& [node,_]:entries) { all_dependencies[node]; dependents[node]; }
    for(const auto& e:edges) { all_dependencies[e.from].push_back(e.to); dependents[e.to].push_back(e.from); }
    for(auto* graph:{&all_dependencies,&dependents}) for(auto& [_,neighbors]:*graph) {
        std::sort(neighbors.begin(),neighbors.end()); neighbors.erase(std::unique(neighbors.begin(),neighbors.end()),neighbors.end());
    }
    const auto groups=cyclic_groups(all_dependencies);
    std::map<std::string,std::size_t> node_group;
    std::vector<std::string> group_ids;
    struct GroupType { bool strict=false,only_references=true; };
    std::vector<GroupType> group_types(groups.size());
    for(std::size_t i=0;i<groups.size();++i) {
        const auto identity=canonical_json(Json(groups[i]));
        group_ids.push_back("cycle-"+sha256_bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size())));
        for(const auto& node:groups[i]) node_group[node]=i;
    }
    // Classify each internal typed edge once, rather than scanning every edge
    // again for each SCC. Only the group record carries the membership list.
    for(const auto& e:edges) {
        auto from=node_group.find(e.from),to=node_group.find(e.to);
        if(from!=node_group.end() && to!=node_group.end() && from->second==to->second) {
            auto& type=group_types[from->second]; type.strict=type.strict || order_types.contains(e.type);
            type.only_references=type.only_references && reference_types.contains(e.type);
        }
    }
    for(std::size_t i=0;i<groups.size();++i) {
        const auto& group=groups[i]; const auto& id=group_ids[i]; const auto& type=group_types[i];
        if(type.strict || !type.only_references) {
            const auto code=type.strict ? "ordering_cycle" : "dependency_cycle";
            p["conflicts"].push_back({{"code",code},{"components",group},{"group_id",id}});
            for(const auto& n:group) unresolved(n,code,{{"group_id",id}});
        } else {
            bool allowed=true;
            for(const auto& n:group) if(!selected.contains(n) || !selected.at(n).value("supports_reference_cycles",false) || blocked.contains(n)) allowed=false;
            p["reference_groups"].push_back({{"components",group},{"group_id",id},{"status",allowed?"eligible":"unresolved"}});
            if(!allowed) for(const auto& n:group) unresolved(n,"unsupported_reference_cycle",{{"group_id",id}});
        }
    }
    // A failed prerequisite removes every dependent step, even when that step's own backend matches.
    std::map<std::string,std::size_t> distance;
    std::vector<std::string> queue(blocked.begin(),blocked.end());
    for(const auto& node:queue) distance[node]=0;
    for(std::size_t head=0;head<queue.size();++head) {
        const auto node=queue[head];
        for(const auto& dependent:dependents.at(node)) if(!distance.contains(dependent)) {
            distance[dependent]=distance.at(node)+1; queue.push_back(dependent);
        }
    }
    // Pick the lexical prerequisite one level closer to an original failure.
    // This makes witnesses deterministic and prevents circular explanations.
    for(const auto& [node,depth]:distance) if(depth!=0) {
        for(const auto& prerequisite:all_dependencies.at(node)) {
            auto found=distance.find(prerequisite);
            if(found!=distance.end() && found->second==depth-1) {
                Json details={{"dependency",prerequisite}};
                if(node_group.contains(node)) details["group_id"]=group_ids[node_group.at(node)];
                unresolved(node,"dependency_unresolved",std::move(details)); break;
            }
        }
    }
    for(auto& group:p["reference_groups"]) for(const auto& n:group.at("components"))
        if(blocked.contains(n.get<std::string>())) group["status"]="unresolved";
    std::set<std::string> visited,visiting;
    std::vector<GraphFrame> frames;
    for(const auto& [root,_]:selected) {
        if(visited.contains(root) || blocked.contains(root)) continue;
        visiting.insert(root); frames.push_back({root,0});
        while(!frames.empty()) {
            auto& frame=frames.back(); const auto& dependencies=all_dependencies.at(frame.node);
            if(frame.next_neighbor<dependencies.size()) {
                const auto& child=dependencies[frame.next_neighbor++];
                if(!visited.contains(child) && !visiting.contains(child) && !blocked.contains(child)) {
                    visiting.insert(child); frames.push_back({child,0});
                }
                continue;
            }
            const auto node=frame.node; frames.pop_back();
            visiting.erase(node); visited.insert(node); p["ordered_components"].push_back(node);
            const auto& b=selected.at(node);
            Json step={{"component",node},{"source_hash",entries.at(node).at("sha256")},{"backend_id",b.at("id")},{"backend_version",b.at("version")},{"mode",b.at("mode")},{"stage","analyzed"},{"status","eligible"}};
            if(node_group.contains(node)) step["group_id"]=group_ids[node_group.at(node)];
            p["eligible_steps"].push_back(std::move(step));
        }
    }
    std::sort(p["unresolved"].begin(),p["unresolved"].end(),[](const Json& a,const Json& b){return canonical_json(a)<canonical_json(b);});
    std::sort(p["conflicts"].begin(),p["conflicts"].end(),[](const Json& a,const Json& b){return canonical_json(a)<canonical_json(b);});
    if(!p["unresolved"].empty() || !p["conflicts"].empty()) p["status"]="unresolved";
    return p;
}
} // namespace modbridge
