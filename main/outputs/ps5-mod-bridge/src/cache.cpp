#include "modbridge/core.hpp"
#include <set>
#include <stdexcept>

namespace modbridge {
namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::invalid_argument("cache context: " + message);
}
void identity(const Json& value, const std::string& name) {
    require(value.is_string() && !value.get_ref<const std::string&>().empty(), name + " must be a nonempty string");
}
void identities(const Json& value, const std::string& name, bool nonempty = false) {
    require(value.is_object() && (!nonempty || !value.empty()), name + " must be an identity object");
    for (auto it = value.begin(); it != value.end(); ++it) {
        require(!it.key().empty(), name + " has an empty key"); identity(it.value(), name + "." + it.key());
    }
}
void validate_context(const Json& c) {
    require(c.is_object(), "must be an object");
    const std::set<std::string> fields = {"backend_id", "backend_version", "core_parser_version", "schema_version", "source_hash", "options", "dependency_hashes", "target_fingerprints", "mapping_versions", "evidence"};
    for (const auto& field : fields) require(c.contains(field), "missing " + field);
    for (auto it=c.begin(); it!=c.end(); ++it) require(fields.contains(it.key()), "unsupported field " + it.key());
    for (auto field : {"backend_id", "backend_version", "core_parser_version", "source_hash"}) identity(c.at(field), field);
    require(c.at("schema_version").is_number_integer() && c.at("schema_version") > 0, "schema_version must be a positive integer");
    require(c.at("options").is_object(), "options must be an object");
    identities(c.at("dependency_hashes"), "dependency_hashes");
    identities(c.at("target_fingerprints"), "target_fingerprints", true);
    identities(c.at("mapping_versions"), "mapping_versions");
    require(c.at("evidence").is_array(), "evidence must be an array");
    std::set<std::string> seen;
    for (const auto& e : c.at("evidence")) {
        require(e.is_object(), "evidence item must be an object");
        for (auto field : {"id", "version", "status", "context", "scope", "target_fingerprints"}) require(e.contains(field), std::string("evidence missing ") + field);
        for (auto field : {"id", "version", "status"}) identity(e.at(field), std::string("evidence.")+field);
        require(seen.insert(e.at("id").get<std::string>()).second, "duplicate evidence id");
        require((e.at("context").is_string() && !e.at("context").get_ref<const std::string&>().empty()) || (e.at("context").is_object() && !e.at("context").empty()), "evidence.context must be explicit");
        require(e.at("scope").is_array() && !e.at("scope").empty(), "evidence.scope must be a nonempty array");
        std::set<std::string> scopes;
        for (const auto& scope : e.at("scope")) { identity(scope,"evidence.scope"); require(scopes.insert(scope.get<std::string>()).second,"duplicate evidence scope"); }
        identities(e.at("target_fingerprints"), "evidence.target_fingerprints", true);
        if(e.contains("source_hash")) identity(e.at("source_hash"),"evidence.source_hash");
    }
}
} // namespace

std::string cache_key(const Json& context) {
    validate_context(context);
    const auto bytes = std::string("modbridge-cache-v1:") + canonical_json(context);
    return sha256_bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()));
}

bool cache_evidence_usable(const Json& cached, const Json& current) {
    try {
        validate_context(cached); validate_context(current);
        if(cache_key(cached) != cache_key(current)) return false;
        for (const auto& e : current.at("evidence")) {
            if(e.at("status") != "validated") return false;
            for(auto it=e.at("target_fingerprints").begin(); it!=e.at("target_fingerprints").end(); ++it)
                if(!current.at("target_fingerprints").contains(it.key()) || current.at("target_fingerprints").at(it.key()) != it.value()) return false;
            if(e.contains("source_hash") && e.at("source_hash") != current.at("source_hash")) return false;
        }
        return true;
    } catch(const std::exception&) { return false; }
}
} // namespace modbridge
