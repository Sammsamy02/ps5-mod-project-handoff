#include "modbridge/core.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <picosha2/picosha2.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <dirent.h>
#include <memory>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace modbridge  {
  namespace  {
    struct Fd  {
      int n=-1;
      explicit Fd(int v):n(v) {
        if(v<0)throw std::runtime_error(std::string("open failed: ")+std::strerror(errno));
      }
      ~Fd() {
        if(n>=0)::close(n);
      }
      Fd(const Fd&)=delete;
      Fd&operator=(const Fd&)=delete;
    };
    struct Stamp  {
      struct stat s {
      };
      explicit Stamp(int fd) {
        if(fstat(fd,&s))throw std::runtime_error("fstat failed");
      }
    };
    bool same(const Stamp&a,const Stamp&b) {
#ifdef __APPLE__
      return a.s.st_dev==b.s.st_dev&&a.s.st_ino==b.s.st_ino&&a.s.st_size==b.s.st_size&&a.s.st_mtimespec.tv_sec==b.s.st_mtimespec.tv_sec&&a.s.st_mtimespec.tv_nsec==b.s.st_mtimespec.tv_nsec&&a.s.st_ctimespec.tv_sec==b.s.st_ctimespec.tv_sec&&a.s.st_ctimespec.tv_nsec==b.s.st_ctimespec.tv_nsec;
#else
      return a.s.st_dev==b.s.st_dev&&a.s.st_ino==b.s.st_ino&&a.s.st_size==b.s.st_size&&a.s.st_mtim.tv_sec==b.s.st_mtim.tv_sec&&a.s.st_mtim.tv_nsec==b.s.st_mtim.tv_nsec&&a.s.st_ctim.tv_sec==b.s.st_ctim.tv_sec&&a.s.st_ctim.tv_nsec==b.s.st_ctim.tv_nsec;
#endif
    }
    std::string digest_fd(int fd) {
      picosha2::hash256_one_by_one h;
      std::array<unsigned char,65536>b {
      };
      while(true) {
        auto n=::read(fd,b.data(),b.size());
        if(n<0) {
          if(errno==EINTR)continue;
          throw std::runtime_error("input hash read failed");
        }
        if(!n)break;
        h.process(b.begin(),b.begin()+n);
      }
      h.finish();
      return picosha2::get_hash_hex_string(h);
    }
    void issue(Json&j,const std::string&code,const std::string&path,const std::string&detail,
               bool invalidates_members=true) {
      j["issues"].push_back({{"code",code},{"path",path},{"detail",detail}});
      j["inventory_complete"]=false;
      if(invalidates_members) {
        j["member_inventory_complete"]=false;
        j["container_groups"]=Json::array();
      }
    }
    std::string logical(const std::string&name) {
      if(name.size()>4096)throw std::runtime_error("path length exceeds 4096 bytes");
      if(name.empty()||name[0]=='/'||name[0]=='\\')throw std::runtime_error("absolute or empty path");
      std::string s=name;
      for(auto&c:s) {
        if(c=='\\')c='/';
        if(static_cast<unsigned char>(c)<32||c==127||c==':')throw std::runtime_error("unsafe path character");
      }
      std::string result;
      std::size_t from=0;
      while(from<=s.size()) {
        auto end=s.find('/',from);
        if(end==std::string::npos)end=s.size();
        auto part=s.substr(from,end-from);
        if(part=="..")throw std::runtime_error("parent traversal");
        if(!part.empty()&&part!=".") {
          if(part.back()=='.'||part.back()==' ')throw std::runtime_error("ambiguous destination spelling");
          if(!result.empty())result+='/';
          result+=part;
        }
        if(end==s.size())break;
        from=end+1;
      }
      if(result.empty())throw std::runtime_error("empty logical destination");
      return result;
    }
    std::string fold(std::string s) {
      for(auto&c:s)c=char(std::tolower(static_cast<unsigned char>(c)));
      return s;
    }
    std::string container_ascii_fold(std::string value) {
      for(auto& ch:value)if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');
      return value;
    }
    // Candidate names are metadata only. Complete member identities do not
    // prove container validity, platform cooking, runtime or install authority.
    Json container_groups(const Json& inventory) {
      using Key=std::pair<std::string,std::string>;
      struct Candidate {
        std::map<std::string,Json> members;
        std::set<std::string> reasons;
      };
      std::map<Key,Candidate> candidates;
      std::map<Key,std::vector<Key>> spellings;
      for(const auto& entry:inventory.at("entries")) {
        if(entry.at("kind")=="symlink"||entry.at("kind")=="hardlink")continue;
        const auto path=entry.at("path").get<std::string>();
        auto slash=path.rfind('/');
        auto parent=slash==std::string::npos?std::string{}:path.substr(0,slash);
        auto filename=slash==std::string::npos?path:path.substr(slash+1);
        auto dot=filename.rfind('.');
        if(dot==std::string::npos)continue;
        auto role=container_ascii_fold(filename.substr(dot+1));
        if(role!="pak"&&role!="utoc"&&role!="ucas")continue;
        Key key{parent,filename.substr(0,dot)};
        auto& candidate=candidates[key];
        if(!candidate.members.emplace(role,Json{{"role",role},{"path",path},
          {"original_path",entry.at("original_path")},{"size",entry.at("size")},
          {"sha256",entry.at("sha256")}}).second)
          throw std::runtime_error("ambiguous container member identity");
      }
      for(const auto& [key,_]:candidates)
        spellings[{key.first,container_ascii_fold(key.second)}].push_back(key);
      for(const auto& [_,keys]:spellings)if(keys.size()>1)
        for(const auto& key:keys)candidates.at(key).reasons.insert("container_case_ambiguity");
      // An adjacent partition-shaped UCAS is never guessed into a complete
      // bundle. Keep its original group and flag both names for a format reader.
      for(auto& [key,candidate]:candidates) {
        if(!candidate.members.contains("ucas"))continue;
        auto suffix=key.second.rfind("_s");
        if(suffix==std::string::npos||suffix==0||suffix+2==key.second.size())continue;
        if(!std::all_of(key.second.begin()+suffix+2,key.second.end(),
            [](char ch){return ch>='0'&&ch<='9';}))continue;
        auto base=candidates.find({key.first,key.second.substr(0,suffix)});
        if(base==candidates.end()||!base->second.members.contains("utoc"))continue;
        base->second.reasons.insert("container_partition_unresolved");
        candidate.reasons.insert("container_partition_unresolved");
      }
      Json groups=Json::array();
      for(auto& [key,candidate]:candidates) {
        const bool iostore=candidate.members.contains("utoc")||candidate.members.contains("ucas");
        const bool names_complete=iostore
          ?candidate.members.contains("utoc")&&candidate.members.contains("ucas")
          :candidate.members.contains("pak");
        if(!names_complete)candidate.reasons.insert("container_member_missing");
        Json members=Json::array();
        for(const auto& [_,member]:candidate.members)members.push_back(member);
        const auto kind=iostore?"unreal_iostore":"unreal_pak";
        Json identity={{"schema_version",1},{"source_package_sha256",inventory.at("package_sha256")},
          {"parent",key.first},{"stem",key.second},{"kind",kind},{"members",members}};
        const auto serialized=canonical_json(identity);
        const auto group_id="container-"+sha256_bytes(std::span(
          reinterpret_cast<const std::uint8_t*>(serialized.data()),serialized.size()));
        const bool complete=names_complete&&candidate.reasons.empty();
        groups.push_back({{"group_id",group_id},{"source_package_sha256",inventory.at("package_sha256")},
          {"parent",key.first},{"stem",key.second},{"kind",kind},{"members",std::move(members)},
          {"members_complete",complete},{"status",complete?"candidate":"unresolved"},
          {"reason_codes",candidate.reasons},{"validation_state","unverified"},
          {"installation_supported",false}});
      }
      return groups;
    }
    constexpr std::size_t manifest_max_depth = 64;
    constexpr std::size_t manifest_max_nodes = 10000;

    // Validate immutable input with SAX before building a DOM. DOM callbacks that
    // merely discard a deep value are insufficient: copying/destruction can still
    // recurse through already-created containers. This pass retains no JSON tree.
    struct ManifestBounds final : nlohmann::json_sax<Json> {
      std::size_t depth = 0;
      std::size_t nodes = 0;
      std::string reason;

      bool node() {
        if (nodes == manifest_max_nodes) {
          reason = "manifest exceeds node limit 10000 (values and object keys)";
          return false;
        }
        ++nodes;
        return true;
      }
      bool start() {
        if (depth == manifest_max_depth) {
          reason = "manifest exceeds container depth limit 64";
          return false;
        }
        ++depth;
        return node();
      }
      bool null() override { return node(); }
      bool boolean(bool) override { return node(); }
      bool number_integer(number_integer_t) override { return node(); }
      bool number_unsigned(number_unsigned_t) override { return node(); }
      bool number_float(number_float_t, const string_t&) override { return node(); }
      bool string(string_t&) override { return node(); }
      bool binary(binary_t&) override { return node(); }
      bool key(string_t&) override { return node(); }
      bool start_object(std::size_t) override { return start(); }
      bool start_array(std::size_t) override { return start(); }
      bool end_object() override { --depth; return true; }
      bool end_array() override { --depth; return true; }
      bool parse_error(std::size_t position, const std::string&,
                       const nlohmann::detail::exception&) override {
        reason = "invalid manifest JSON at byte " + std::to_string(position);
        return false;
      }
    };

    struct Inventory  {
      Json j;
      Limits limits;
      std::set<std::string> names;
      std::uint64_t count=0,total=0;
      explicit Inventory(const Limits&l):j({{"schema_version",1},{"parser_version","0.1.0"},{"status","analyzed"},{"inventory_complete",true},{"package_sha256",nullptr},{"entries",Json::array()},{"requirements",Json::array()},{"dependencies",Json::array()},{"issues",Json::array()},{"limits",{{"max_entries",l.max_entries},{"max_total_bytes",l.max_total_bytes},{"max_entry_bytes",l.max_entry_bytes},{"max_inspection_bytes",l.max_inspection_bytes}}},{"provenance",{{"input_stable",false}}}}),limits(l) {
        j["member_inventory_complete"]=true;
        j["container_groups"]=Json::array();
        j["limits"]["max_manifest_depth"] = manifest_max_depth;
        j["limits"]["max_manifest_nodes"] = manifest_max_nodes;
      }
      std::string accept(const std::string&original) {
        if(count==limits.max_entries) {
          issue(j,"max_entries",original,"entry count limit reached");
          return  {
          };
        }
        count++;
        try {
          auto n=logical(original),k=fold(n);
          for(const auto&seen:names) {
            if(k==seen||k.starts_with(seen+"/")||seen.starts_with(k+"/")) {
              issue(j,"path_collision",original,"logical destination aliases or overlaps another member");
              return  {
              };
            }
          }
          names.insert(k);
          return n;
        }
        catch(const std::exception&e) {
          issue(j,"unsafe_path",original,e.what());
          return  {
          };
        }
      }
      bool budget(std::uint64_t entry_size,std::uint64_t chunk,const std::string&path) {
        if(chunk>limits.max_entry_bytes||entry_size>limits.max_entry_bytes-chunk) {
          issue(j,"max_entry_bytes",path,"expanded entry size exceeds limit");
          return false;
        }
        if(chunk>limits.max_total_bytes||total>limits.max_total_bytes-chunk) {
          issue(j,"max_total_bytes",path,"total expanded bytes exceed limit");
          return false;
        }
        total+=chunk;
        return true;
      }
      void add(const std::string&original,const std::string&path,std::uint64_t size,const std::string&hash,const std::vector<std::uint8_t>&b) {
        auto e=inspect_binary(b,path,size);
        e["path"]=path;
        e["original_path"]=original;
        e["size"]=size;
        e["sha256"]=hash;
        if(size>b.size()) {
          e["facts"]["inspection_complete"]=false;
          e["issues"].push_back({{"code","inspection_cap"},{"path",path},{"detail","content hashed entirely; structural prefix inspection capped"}});
          e["requirements"].push_back({{"id","inspection.complete"},{"consumer",path},{"context","preparation"},{"status","unresolved"},{"discovery","inspection cap"}});
        }
        for(const auto&r:e["requirements"])j["requirements"].push_back(r);
        for(const auto&i:e["issues"])j["issues"].push_back(i);
        if(!e["facts"]["inspection_complete"].get<bool>())j["inventory_complete"]=false;
        if(path=="modbridge.json") {
          if(size>b.size())issue(j,"manifest_incomplete",path,"manifest exceeds inspection cap",false);
          else try {
            Json declarations= {
              {
                "requirements",Json::array()
              }, {
                "dependencies",Json::array()
              }
            };
            ManifestBounds bounds;
            if (!Json::sax_parse(b.begin(), b.end(), &bounds))
              throw std::runtime_error(bounds.reason.empty() ? "invalid manifest JSON" : bounds.reason);
            auto m=Json::parse(b.begin(),b.end());
            if(!m.is_object())throw std::runtime_error("manifest must be an object");
            if(m.contains("metadata")) {
              if(!m["metadata"].is_object())throw std::runtime_error("metadata must be an object");
              declarations["metadata"]=m["metadata"];
            }
            if(m.contains("requirements")) {
              if(!m["requirements"].is_array())throw std::runtime_error("requirements must be an array");
              for(auto r:m["requirements"]) {
                if(!r.is_object()||!r.contains("id")||!r["id"].is_string()||r["id"].get<std::string>().empty())throw std::runtime_error("requirement requires an id");
                r["status"]="unresolved";
                r["discovery"]="modbridge.json declaration";
                declarations["requirements"].push_back(r);
              }
            }
            if(m.contains("dependencies")) {
              if(!m["dependencies"].is_array())throw std::runtime_error("dependencies must be an array");
              Json deps=Json::array();
              for(const auto&d:m["dependencies"]) {
                if(!d.is_object()||!d.contains("from")||!d.contains("to")||!d.contains("type")||!d["from"].is_string()||!d["to"].is_string()||!d["type"].is_string())throw std::runtime_error("dependency requires string from/to/type");
                auto type=d["type"].get<std::string>();
                if(type!="reference"&&type!="runtime"&&type!="order"&&type!="conflict")throw std::runtime_error("unknown dependency type");
                if(d["from"].get<std::string>().empty()||d["to"].get<std::string>().empty())throw std::runtime_error("empty dependency endpoint");
                deps.push_back(d);
              }
              declarations["dependencies"]=deps;
            }
            if(declarations.contains("metadata"))j["metadata"]=declarations["metadata"];
            for(const auto&r:declarations["requirements"])j["requirements"].push_back(r);
            j["dependencies"]=declarations["dependencies"];
          }
          catch(const std::exception&x) {
            issue(j,"manifest_invalid",path,x.what(),false);
          }
        }
        if(path!="modbridge.json"&&(std::filesystem::path(path).extension()==".xml"||std::filesystem::path(path).extension()==".ini"||std::filesystem::path(path).extension()==".bat"||std::filesystem::path(path).extension()==".sh"))j["requirements"].push_back({{"id","installer.interpretation"},{"consumer",path},{"context","preparation"},{"status","unresolved"},{"discovery","uninterpreted installer/configuration dialect"}});
        j["entries"].push_back(e);
      }
      void link(const std::string&original,const std::string&path,const std::string&target,bool hard=false) {
        auto raw=std::span(reinterpret_cast<const std::uint8_t*>(target.data()),target.size());
        j["entries"].push_back({{"path",path},{"original_path",original},{"kind",hard?"hardlink":"symlink"},{"size",0},{"sha256",sha256_bytes(raw)},{"facts",{{"target",target},{"hash_scope","link_target_metadata"},{"followed",false},{"inspection_complete",true}}},{"requirements",Json::array({{{"id","filesystem.link_policy"},{"consumer",path},{"context","preparation"},{"status","unresolved"},{"discovery","link metadata"}}})}});
        j["requirements"].push_back(j["entries"].back()["requirements"][0]);
      }
    };
    // Walk a relative path through directory descriptors; no parent symlink can be followed.
    int secure_open(int root,const std::filesystem::path&p) {
      Fd cur(::dup(root));
      auto parts=std::vector<std::string> {
      };
      for(const auto&v:p)parts.push_back(v.string());
      for(std::size_t i=0;i<parts.size();i++) {
        int n=::openat(cur.n,parts[i].c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|(i+1<parts.size()?O_DIRECTORY:0));
        if(n<0)return -1;
        ::close(cur.n);
        cur.n=n;
      }
      int result=cur.n;
      cur.n=-1;
      return result;
    }
    Json directory_snapshot(int root,std::uint64_t cap) {
      Json result=Json::array();
      std::uint64_t count=0;
      auto walk=[&](auto&&self,int fd,const std::string&prefix)->void {
        Fd copy(::openat(fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW));
        DIR*raw=::fdopendir(copy.n);
        if(!raw)throw std::runtime_error("cannot enumerate directory");
        copy.n=-1;
        std::unique_ptr<DIR,decltype(&closedir)>dir(raw,closedir);
        while(true) {
          errno=0;
          auto*entry=::readdir(dir.get());
          if(!entry) {
            if(errno)throw std::runtime_error("directory enumeration failed");
            break;
          }
          std::string name=entry->d_name;
          if(name=="."||name=="..")continue;
          if(++count>cap)throw std::length_error("directory enumeration max_entries exceeded");
          struct stat st {
          };
          if(::fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW))throw std::runtime_error("directory member changed");
          auto path=prefix.empty()?name:prefix+"/"+name;
          Json item= {
            {
              "path",path
            }, {
              "type",S_ISDIR(st.st_mode)?int(std::filesystem::file_type::directory):S_ISLNK(st.st_mode)?int(std::filesystem::file_type::symlink):int(std::filesystem::file_type::regular)
            }, {
              "device",st.st_dev
            }, {
              "inode",st.st_ino
            }, {
              "size",st.st_size
            }
          };
#ifdef __APPLE__
          item["mtime_ns"]= {
            st.st_mtimespec.tv_sec,st.st_mtimespec.tv_nsec
          };
          item["ctime_ns"]= {
            st.st_ctimespec.tv_sec,st.st_ctimespec.tv_nsec
          };
#else
          item["mtime_ns"]= {
            st.st_mtim.tv_sec,st.st_mtim.tv_nsec
          };
          item["ctime_ns"]= {
            st.st_ctim.tv_sec,st.st_ctim.tv_nsec
          };
#endif
          if(S_ISLNK(st.st_mode)) {
            std::array<char,65536>target {
            };
            auto n=::readlinkat(fd,name.c_str(),target.data(),target.size());
            if(n<0||std::size_t(n)==target.size())throw std::runtime_error("link target unavailable or too long");
            item["target"]=std::string(target.data(),n);
          }
          result.push_back(item);
          if(S_ISDIR(st.st_mode)) {
            Fd sub(::openat(fd,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW));
            self(self,sub.n,path);
          }
        }
      };
      walk(walk,root,"");
      std::sort(result.begin(),result.end(),[](const Json&x,const Json&y){return x.at("path")<y.at("path");});
      return result;
    }
  }
  // namespace
  namespace {
    struct OverrideReject { OverrideStatus status; };
    void reject_override(OverrideStatus status) { throw OverrideReject{status}; }
    struct OverrideWork {
      std::uint64_t remaining;
      void charge(std::uint64_t amount) {
        if (amount > remaining) reject_override(OverrideStatus::limit_exceeded);
        remaining -= amount;
      }
      bool equal(std::string_view a, std::string_view b) {
        charge(1);
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
          charge(1);
          if (a[i] != b[i]) return false;
        }
        return true;
      }
      bool starts_with(std::string_view text, std::string_view prefix) {
        charge(1);
        return text.size() >= prefix.size() && equal(text.substr(0, prefix.size()), prefix);
      }
      int compare(std::string_view a, std::string_view b) {
        const auto common = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < common; ++i) {
          charge(1);
          const auto ac = static_cast<unsigned char>(a[i]);
          const auto bc = static_cast<unsigned char>(b[i]);
          if (ac != bc) return ac < bc ? -1 : 1;
        }
        return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
      }
      std::string normalize(std::string_view text, OverrideCase case_policy) {
        charge(1 + 8 * text.size());
        if (case_policy == OverrideCase::ascii_insensitive)
          for (const unsigned char c : text)
            if (c >= 128) reject_override(OverrideStatus::unsupported);
        auto key = logical(std::string(text));
        if (case_policy == OverrideCase::ascii_insensitive)
          for (auto& c : key) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return key;
      }
    };
    void admit_override_path(std::string_view text, std::uint64_t& remaining) {
      if (text.size() > 4096 || text.size() > remaining)
        reject_override(OverrideStatus::limit_exceeded);
      remaining -= text.size();
    }
    template<class Roots>
    std::string override_request_key(std::string_view request, const Roots& roots,
                                    OverrideCase case_policy, OverrideWork& work) {
      auto relative = request;
      for (const auto& root : roots) {
        if (work.starts_with(request, root)) {
          work.charge(1);
          if (request.size() <= root.size() ||
              (request[root.size()] != '/' && request[root.size()] != '\\'))
            reject_override(OverrideStatus::invalid_input);
          // A second leading separator remains absolute in the relative grammar.
          relative = request.substr(root.size() + 1);
          break;
        }
      }
      return work.normalize(relative, case_policy);
    }
    struct OwnedOverrideKey {
      std::uint64_t rank;
      std::string path;
      std::uint64_t source;
    };
    struct ValidatedOverrides {
      std::optional<std::uint64_t> candidate;
      std::vector<OwnedOverrideKey> winners;
      std::vector<std::string> roots;
      std::uint64_t raw_key_bytes = 0;
    };
    ValidatedOverrides validate_overrides(std::optional<std::string_view> request,
        std::span<const FileOverrideEntry> entries, const FileOverridePolicy& policy,
        const FileOverrideLimits& limits) {
      if ((policy.case_policy != OverrideCase::byte_exact &&
           policy.case_policy != OverrideCase::ascii_insensitive) ||
          (policy.winner != OverrideWinner::first_package &&
           policy.winner != OverrideWinner::last_package))
        reject_override(OverrideStatus::invalid_input);

      const FileOverrideLimits ceilings;
      if (limits.max_entries > ceilings.max_entries ||
          limits.max_key_bytes > ceilings.max_key_bytes ||
          limits.max_work_bytes > ceilings.max_work_bytes ||
          limits.max_virtual_roots > ceilings.max_virtual_roots ||
          entries.size() > limits.max_entries ||
          policy.virtual_roots.size() > limits.max_virtual_roots)
        reject_override(OverrideStatus::limit_exceeded);

      // Admit raw sizes before allocating/copying or choosing a candidate.
      auto remaining_keys = limits.max_key_bytes;
      if (request) admit_override_path(*request, remaining_keys);
      for (const auto root : policy.virtual_roots) admit_override_path(root, remaining_keys);
      for (const auto& entry : entries) admit_override_path(entry.relative_key, remaining_keys);
      OverrideWork work{limits.max_work_bytes};

      // A root is trusted configuration, but still has a checked grammar and
      // unique spelling. Never strip an arbitrary colon-ended component.
      for (std::size_t i = 0; i < policy.virtual_roots.size(); ++i) {
        const auto root = policy.virtual_roots[i];
        work.charge(1);
        if (root.size() < 2 || root.back() != ':') reject_override(OverrideStatus::invalid_input);
        for (std::size_t j = 0; j + 1 < root.size(); ++j) {
          work.charge(1);
          const auto c = static_cast<unsigned char>(root[j]);
          if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-'))
            reject_override(OverrideStatus::invalid_input);
        }
        for (std::size_t j = 0; j < i; ++j)
          if (work.equal(root, policy.virtual_roots[j])) reject_override(OverrideStatus::invalid_input);
      }
      const auto request_key = request ? override_request_key(*request, policy.virtual_roots,
          policy.case_policy, work) : std::string{};
      const auto less = [&](const OwnedOverrideKey& a, const OwnedOverrideKey& b) {
        work.charge(1);
        if (a.rank != b.rank) return a.rank < b.rank;
        return work.compare(a.path, b.path) < 0;
      };
      std::set<OwnedOverrideKey, decltype(less)> seen(less);
      ValidatedOverrides result;
      result.raw_key_bytes = limits.max_key_bytes - remaining_keys;
      std::optional<std::uint64_t> previous_rank;
      for (const auto& entry : entries) {
        work.charge(1);
        if (previous_rank && entry.package_rank < *previous_rank)
          reject_override(OverrideStatus::invalid_input);
        previous_rank = entry.package_rank;
        OwnedOverrideKey key{entry.package_rank, work.normalize(entry.relative_key,
            policy.case_policy), entry.opaque_source_index};
        // Check each directory prefix, including when unrelated lexical names
        // fall between a parent and child. Child-before-parent is checked too.
        for (std::size_t i = 0; i < key.path.size(); ++i) {
          work.charge(1);
          if (key.path[i] == '/') {
            work.charge(i);
            if (seen.find(OwnedOverrideKey{key.rank, key.path.substr(0, i), 0}) != seen.end())
              reject_override(OverrideStatus::invalid_input);
          }
        }
        work.charge(key.path.size() + 1);
        const auto descendant_prefix = key.path + '/';
        const auto descendant = seen.lower_bound(OwnedOverrideKey{key.rank, descendant_prefix, 0});
        if (descendant != seen.end() && descendant->rank == key.rank &&
            work.starts_with(descendant->path, descendant_prefix))
          reject_override(OverrideStatus::invalid_input);
        const bool matches = request && work.equal(key.path, request_key);
        if (!seen.insert(std::move(key)).second) reject_override(OverrideStatus::invalid_input);
        if (matches && (!result.candidate || policy.winner == OverrideWinner::last_package))
          result.candidate = entry.opaque_source_index;
      }
      if (!request) {
        result.winners.reserve(entries.size());
        while (!seen.empty()) {
          work.charge(1);
          auto node = seen.extract(seen.begin());
          result.winners.push_back(std::move(node.value()));
        }
        std::sort(result.winners.begin(), result.winners.end(),
            [&](const OwnedOverrideKey& a, const OwnedOverrideKey& b) {
          work.charge(1);
          const auto order = work.compare(a.path, b.path);
          return order < 0 || (order == 0 && a.rank < b.rank);
        });
        std::size_t count = 0;
        for (std::size_t i = 0; i < result.winners.size(); ++i) {
          work.charge(1);
          if (!count || !work.equal(result.winners[count - 1].path, result.winners[i].path)) {
            if (count != i) result.winners[count] = std::move(result.winners[i]);
            ++count;
          } else if (policy.winner == OverrideWinner::last_package) {
            result.winners[count - 1].source = result.winners[i].source;
            result.winners[count - 1].rank = result.winners[i].rank;
          }
        }
        result.winners.resize(count);
        result.roots.reserve(policy.virtual_roots.size());
        for (const auto root : policy.virtual_roots) {
          work.charge(1 + root.size());
          result.roots.emplace_back(root);
        }
      }
      return result;
    }
    FileOverridePreparationStatus preparation_error(OverrideStatus status) {
      switch (status) {
        case OverrideStatus::limit_exceeded: return FileOverridePreparationStatus::limit_exceeded;
        case OverrideStatus::unsupported: return FileOverridePreparationStatus::unsupported;
        default: return FileOverridePreparationStatus::invalid_input;
      }
    }
  } // namespace

  struct FileOverrideTable::Data {
    std::vector<OwnedOverrideKey> winners;
    std::vector<std::string> roots;
    FileOverrideLimits limits;
    OverrideCase case_policy;
    std::uint64_t raw_key_bytes;
  };
  FileOverridePreparation prepare_file_overrides(std::span<const FileOverrideEntry> entries,
      const FileOverridePolicy& policy, const FileOverrideLimits& limits) {
    try {
      auto validated = validate_overrides(std::nullopt, entries, policy, limits);
      auto data = std::make_shared<FileOverrideTable::Data>(FileOverrideTable::Data{
          std::move(validated.winners), std::move(validated.roots), limits,
          policy.case_policy, validated.raw_key_bytes});
      return {FileOverridePreparationStatus::ready, FileOverrideTable(std::move(data))};
    } catch (const OverrideReject& error) {
      return {preparation_error(error.status), std::nullopt};
    } catch (const std::bad_alloc&) {
      return {FileOverridePreparationStatus::limit_exceeded, std::nullopt};
    } catch (const std::exception&) {
      return {FileOverridePreparationStatus::invalid_input, std::nullopt};
    }
  }
  FileOverrideChoice FileOverrideTable::lookup(std::string_view request,
                                              std::uint64_t max_work_bytes) const {
    try {
      if (!data_) reject_override(OverrideStatus::invalid_input);
      if (max_work_bytes > FileOverrideLimits{}.max_work_bytes)
        reject_override(OverrideStatus::limit_exceeded);
      auto remaining_keys = data_->limits.max_key_bytes - data_->raw_key_bytes;
      admit_override_path(request, remaining_keys);
      OverrideWork work{std::min(max_work_bytes, data_->limits.max_work_bytes)};
      const auto key = override_request_key(request, data_->roots, data_->case_policy, work);
      const auto found = std::lower_bound(data_->winners.begin(), data_->winners.end(), key,
          [&](const OwnedOverrideKey& entry, std::string_view query) {
        work.charge(1);
        return work.compare(entry.path, query) < 0;
      });
      if (found != data_->winners.end() && work.equal(found->path, key))
        return {OverrideStatus::selected, found->source};
      return {OverrideStatus::passthrough, std::nullopt};
    } catch (const OverrideReject& error) {
      return {error.status, std::nullopt};
    } catch (const std::bad_alloc&) {
      return {OverrideStatus::limit_exceeded, std::nullopt};
    } catch (const std::exception&) {
      return {OverrideStatus::invalid_input, std::nullopt};
    }
  }

  FileOverrideChoice choose_file_override(std::string_view request,
      std::span<const FileOverrideEntry> entries, const FileOverridePolicy& policy,
      const FileOverrideLimits& limits) {
    try {
      const auto validated = validate_overrides(request, entries, policy, limits);
      return {validated.candidate ? OverrideStatus::selected : OverrideStatus::passthrough,
              validated.candidate};
    } catch (const OverrideReject& error) {
      return {error.status, std::nullopt};
    } catch (const std::bad_alloc&) {
      return {OverrideStatus::limit_exceeded, std::nullopt};
    } catch (const std::exception&) {
      return {OverrideStatus::invalid_input, std::nullopt};
    }
  }

  Json analyze_package(const std::filesystem::path&input,const Limits&limits) {
    Inventory inv(limits);
    auto&j=inv.j;
    bool member_walk_complete=false;
    try {
      Fd fd(::open(input.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK));
      Stamp before(fd.n);
      bool stable=true;
      if(S_ISDIR(before.s.st_mode)) {
        j["provenance"]["hash_scope"]="canonical_directory_content_identity";
        Json snapshot=directory_snapshot(fd.n,limits.max_entries);
        for(const auto&item:snapshot) {
          if(item["type"]==int(std::filesystem::file_type::directory))continue;
          auto original=item["path"].get<std::string>(),path=inv.accept(original);
          if(path.empty())continue;
          if(item["type"]==int(std::filesystem::file_type::symlink)) {
            auto target=item["target"].get<std::string>();
            inv.link(original,path,target);
            continue;
          }
          Fd member(secure_open(fd.n,original));
          Stamp mb(member.n);
          if(!S_ISREG(mb.s.st_mode)) {
            issue(j,"unsupported_entry",path,"only regular files and link metadata are inventoried");
            continue;
          }
          picosha2::hash256_one_by_one hash;
          std::vector<std::uint8_t>b;
          std::array<std::uint8_t,65536>chunk {
          };
          std::uint64_t size=0;
          bool valid=true;
          while(true) {
            auto n=::read(member.n,chunk.data(),chunk.size());
            if(n<0) {
              if(errno==EINTR)continue;
              throw std::runtime_error("member read failed");
            }
            if(!n)break;
            if(!inv.budget(size,n,path)) {
              valid=false;
              break;
            }
            hash.process(chunk.begin(),chunk.begin()+n);
            auto keep=std::min<std::uint64_t>(n,limits.max_inspection_bytes-b.size());
            b.insert(b.end(),chunk.begin(),chunk.begin()+keep);
            size+=n;
          }
          if(!same(mb,Stamp(member.n)))stable=false;
          if(valid&&(mb.s.st_size<0||size!=std::uint64_t(mb.s.st_size))) {
            issue(j,"member_size_mismatch",path,"observed bytes differ from the opened member size");
            valid=false;
          }
          if(valid) {
            hash.finish();
            inv.add(original,path,size,picosha2::get_hash_hex_string(hash),b);
          }
        }
        if(snapshot!=directory_snapshot(fd.n,limits.max_entries))stable=false;
        Json identities=Json::array();
        std::size_t expected=0;
        for(const auto&i:snapshot) {
          if(i["type"]==int(std::filesystem::file_type::directory))identities.push_back({{"path",i["path"]},{"source_type","directory"}});
          else expected++;
        }
        for(const auto&e:j["entries"])identities.push_back({{"path",e["original_path"]},{"source_type",e["kind"]=="symlink"?"symlink":"regular"},{"size",e["size"]},{"sha256",e["sha256"]}});
        std::sort(identities.begin(),identities.end(),[](const Json&a,const Json&b){return a.at("path")<b.at("path");});
        if(j["entries"].size()==expected) {
          std::string canonical=canonical_json(identities);
          j["package_sha256"]=sha256_bytes(std::span(reinterpret_cast<const std::uint8_t*>(canonical.data()),canonical.size()));
          member_walk_complete=true;
        }
        else j["package_sha256"]=nullptr;
      }
      else if(S_ISREG(before.s.st_mode)) {
        j["provenance"]["hash_scope"]="raw_archive_bytes";
        j["package_sha256"]=digest_fd(fd.n);
        if(lseek(fd.n,0,SEEK_SET)<0)throw std::runtime_error("cannot rewind archive");
        std::unique_ptr<archive,decltype(&archive_read_free)>a(archive_read_new(),archive_read_free);
        if(!a)throw std::runtime_error("cannot allocate archive reader");
        archive_read_support_filter_all(a.get());
        archive_read_support_format_all(a.get());
        j["archive_runtime"]=archive_version_details();
        j["archive_support_policy"]="libarchive compiled formats/filters; raw input excluded";
        int opened=archive_read_open_fd(a.get(),fd.n,65536);
        if(opened!=ARCHIVE_OK) {
          issue(j,"archive_error",input.string(),archive_error_string(a.get())?archive_error_string(a.get()):"archive open failed");
        }
        else {
          archive_entry*e=nullptr;
          int rc;
          std::uint64_t headers=0;
          while((rc=archive_read_next_header(a.get(),&e))!=ARCHIVE_EOF) {
            if(rc!=ARCHIVE_OK) {
              issue(j,"archive_error",input.string(),archive_error_string(a.get())?archive_error_string(a.get()):"archive header failed");
              break;
            }
            if(headers==limits.max_entries) {
              issue(j,"max_entries",input.string(),"archive header count limit reached");
              break;
            }
            headers++;
            const char*pn=archive_entry_pathname(e);
            std::string original=pn?pn:"";
            auto type=archive_entry_filetype(e);
            if((type==AE_IFDIR||type==AE_IFLNK||archive_entry_hardlink(e))&&archive_entry_size(e)!=0) {
              issue(j,"unexpected_entry_payload",original,"directory/link entry carries an uninterpreted payload");
              break;
            }
            if(type==AE_IFDIR) {
              try {
                logical(original);
              }
              catch(const std::exception&x) {
                issue(j,"unsafe_path",original,x.what());
              }
              if(archive_read_data_skip(a.get())!=ARCHIVE_OK) {
                issue(j,"archive_error",original,"directory payload skip failed");
                break;
              }
              continue;
            }
            auto path=inv.accept(original);
            if(path.empty())break;
            if(archive_entry_hardlink(e)||type==AE_IFLNK) {
              inv.link(original,path,archive_entry_hardlink(e)?archive_entry_hardlink(e):(archive_entry_symlink(e)?archive_entry_symlink(e):""),bool(archive_entry_hardlink(e)));
              if(archive_read_data_skip(a.get())!=ARCHIVE_OK) {
                issue(j,"archive_error",path,"link payload skip failed");
                break;
              }
              continue;
            }
            if(type!=AE_IFREG) {
              issue(j,"unsupported_entry",path,"nonregular archive entry");
              break;
            }
            auto declared=archive_entry_size(e);
            if(declared<0||std::uint64_t(declared)>limits.max_entry_bytes) {
              issue(j,"max_entry_bytes",path,"declared expanded entry size invalid or over limit");
              break;
            }
            picosha2::hash256_one_by_one hash;
            std::vector<std::uint8_t>b;
            std::array<std::uint8_t,65536>chunk {
            };
            std::uint64_t size=0;
            bool valid=true;
            while(true) {
              auto n=archive_read_data(a.get(),chunk.data(),chunk.size());
              if(n<0) {
                issue(j,"archive_error",path,archive_error_string(a.get())?archive_error_string(a.get()):"member read failed");
                valid=false;
                break;
              }
              if(!n)break;
              if(!inv.budget(size,n,path)) {
                valid=false;
                break;
              }
              hash.process(chunk.begin(),chunk.begin()+n);
              auto keep=std::min<std::uint64_t>(n,limits.max_inspection_bytes-b.size());
              b.insert(b.end(),chunk.begin(),chunk.begin()+keep);
              size+=n;
            }
            if(!valid)break;
            if(size!=std::uint64_t(declared)) {
              issue(j,"archive_size_mismatch",path,"expanded bytes differ from declared size");
              break;
            }
            hash.finish();
            inv.add(original,path,size,picosha2::get_hash_hex_string(hash),b);
          }
          j["archive_format"]=archive_format_name(a.get())?archive_format_name(a.get()):"unknown";
          member_walk_complete=rc==ARCHIVE_EOF;
        }
        const int closed=archive_read_close(a.get());
        if(closed!=ARCHIVE_OK)
          issue(j,"archive_error",input.string(),"archive reader close failed");
        const int freed=archive_read_free(a.release());
        if(freed!=ARCHIVE_OK)
          issue(j,"archive_error",input.string(),"archive reader cleanup failed");
      }
      else throw std::runtime_error("input must be a regular archive or directory");
      if(!same(before,Stamp(fd.n)))stable=false;
      struct stat current {
      };
      if(lstat(input.c_str(),&current)||current.st_dev!=before.s.st_dev||current.st_ino!=before.s.st_ino)stable=false;
      j["provenance"]["input_stable"]=stable;
      if(!stable) {
        j["package_sha256"]=nullptr;
        issue(j,"input_mutation",input.string(),"input identity or content changed during ingestion");
      }
      j["member_inventory_complete"]=j.at("member_inventory_complete").get<bool>()
        &&member_walk_complete&&stable&&!j.at("package_sha256").is_null();
      if(j.at("member_inventory_complete").get<bool>())
        j["container_groups"]=container_groups(j);
    }
    catch(const std::length_error&e) {
      issue(j,"max_entries",input.string(),e.what());
      j["package_sha256"]=nullptr;
    }
    catch(const std::exception&e) {
      issue(j,"input_error",input.string(),e.what());
      j["package_sha256"]=nullptr;
    }
    return j;
  }
}
// namespace modbridge
