#include "modbridge/core.hpp"
#include <picosha2.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <random>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace modbridge {
// POSIX desktop scratch adapter. Internal path walks are descriptor-relative
// and never follow links. flock serializes cooperating jobs; hash and parent
// rechecks narrow external-writer races but are not an atomic content compare
// and replacement. fsync failures are checked; power-loss/console durability
// remains a separate filesystem/platform acceptance question.
namespace {
class Fd {
    int fd_ = -1;
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { if (fd_ >= 0) ::close(fd_); fd_ = other.fd_; other.fd_ = -1; }
        return *this;
    }
    int get() const { return fd_; }
};
[[noreturn]] void error(const std::string& operation) {
    throw std::runtime_error(operation + ": " + std::strerror(errno));
}
Fd checked(int fd, const std::string& operation) { if (fd < 0) error(operation); return Fd(fd); }
void sync(int fd) { if (::fsync(fd) != 0) error("Synchronize scratch storage"); }
struct stat stat_fd(int fd) { struct stat st{}; if (::fstat(fd, &st) != 0) error("Inspect open file"); return st; }
void regular(int fd) { if (!S_ISREG(stat_fd(fd).st_mode)) throw std::runtime_error("Expected regular scratch file"); }
std::vector<std::string> components(const std::string& path, bool destination = false) {
    if (path.empty() || path[0] == '/' || path.find('\\') != std::string::npos || path.find('\0') != std::string::npos)
        throw std::runtime_error("Unsafe relative scratch path");
    std::vector<std::string> parts;
    std::size_t pos = 0;
    while (pos <= path.size()) {
        const auto end = path.find('/', pos);
        const auto part = path.substr(pos, end == std::string::npos ? end : end - pos);
        if (part.empty() || part == "." || part == ".." || part.find(':') != std::string::npos)
            throw std::runtime_error("Unsafe relative scratch path");
        parts.push_back(part);
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    auto control_name = parts[0];
    for (auto& c : control_name) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (destination && (control_name == ".modbridge" || control_name == ".modbridge-scratch"))
        throw std::runtime_error("Destination overlaps scratch control files");
    return parts;
}
Fd directory_at(int parent, const std::string& name, bool create) {
    int fd = ::openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT && create) {
        if (::mkdirat(parent, name.c_str(), 0700) != 0 && errno != EEXIST) error("Create scratch directory");
        sync(parent);
        fd = ::openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    }
    return checked(fd, "Open scratch directory without following links: " + name);
}
struct Parent { Fd directory; std::string leaf; };
Parent parent_at(int anchor, const std::string& path, bool create = false, bool destination = false) {
    auto parts = components(path, destination);
    auto dir = checked(::dup(anchor), "Duplicate scratch directory");
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) dir = directory_at(dir.get(), parts[i], create);
    return {std::move(dir), parts.back()};
}
void same_parent(int anchor, const std::string& path, const Parent& held) {
    auto fresh = parent_at(anchor, path, false, true);
    auto a = stat_fd(fresh.directory.get()), b = stat_fd(held.directory.get());
    if (a.st_dev != b.st_dev || a.st_ino != b.st_ino) throw std::runtime_error("Destination parent changed");
}
Fd read_at(int parent, const std::string& name) {
    auto fd = checked(::openat(parent, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC), "Open regular scratch file: " + name);
    regular(fd.get()); return fd;
}
void write_all(int fd, const void* bytes, std::size_t size) {
    auto data = static_cast<const char*>(bytes);
    while (size != 0) {
        const auto count = ::write(fd, data, size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) error("Write scratch file");
        data += count; size -= static_cast<std::size_t>(count);
    }
}
std::string stream(int source, int destination = -1) {
    if (::lseek(source, 0, SEEK_SET) < 0) error("Rewind scratch file");
    picosha2::hash256_one_by_one hash;
    std::array<std::uint8_t, 65536> bytes{};
    for (;;) {
        const auto count = ::read(source, bytes.data(), bytes.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) error("Read scratch file");
        if (count == 0) break;
        hash.process(bytes.begin(), bytes.begin() + count);
        if (destination >= 0) write_all(destination, bytes.data(), static_cast<std::size_t>(count));
    }
    hash.finish(); return picosha2::get_hash_hex_string(hash);
}
std::string token() {
    std::random_device random;
    std::array<std::uint8_t, 32> bytes{};
    for (auto& byte : bytes) byte = static_cast<std::uint8_t>(random());
    return sha256_bytes(bytes).substr(0, 32);
}
void transaction_id(const std::string& id) {
    if (id.size() != 35 || id.substr(0, 3) != "tx-" ||
        !std::all_of(id.begin() + 3, id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        throw std::runtime_error("Invalid scratch transaction ID");
}
std::string load_text(int parent, const std::string& name) {
    auto fd = read_at(parent, name);
    if (stat_fd(fd.get()).st_size > 16777216) throw std::runtime_error("Scratch journal exceeds 16 MiB");
    std::string text; std::array<char, 8192> bytes{};
    for (;;) {
        auto count = ::read(fd.get(), bytes.data(), bytes.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) error("Read scratch journal");
        if (count == 0) break;
        text.append(bytes.data(), static_cast<std::size_t>(count));
        if (text.size() > 16777216) throw std::runtime_error("Scratch journal exceeds 16 MiB");
    }
    return text;
}
Json parse_journal_text(const std::string& text) {
    if (text.size() > 16777216) throw std::runtime_error("Scratch journal exceeds 16 MiB");
    std::size_t events = 0;
    return Json::parse(text,
        [&events](int depth, Json::parse_event_t, Json&) {
            // Reject during descent, before a deep DOM can be copied, dumped,
            // or recursively destroyed. The byte limit alone cannot do this.
            if (depth > 128 || ++events > 100000)
                throw std::runtime_error("Scratch journal JSON depth or node/event budget exceeded");
            return true;
        });
}
void save(int tx, const Json& journal) {
    const auto name = "journal-" + token() + ".tmp";
    auto fd = checked(::openat(tx, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600), "Create journal update");
    try {
        const auto text = canonical_json(journal) + "\n";
        // Do not publish state that the same recovery reader cannot accept.
        (void)parse_journal_text(text);
        write_all(fd.get(), text.data(), text.size()); sync(fd.get());
        if (::renameat(tx, name.c_str(), tx, "journal.json") != 0) error("Publish scratch journal");
        sync(tx);
    } catch (...) { ::unlinkat(tx, name.c_str(), 0); throw; }
}
std::vector<std::string> directories(int fd) {
    int copy = ::openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (copy < 0) error("Open transaction listing");
    DIR* dir = ::fdopendir(copy);
    if (!dir) { ::close(copy); error("List transactions"); }
    std::vector<std::string> names;
    errno = 0;
    while (auto* entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name != "." && name != "..") names.push_back(name);
    }
    const int saved = errno; ::closedir(dir);
    if (saved != 0) { errno = saved; error("Read transaction listing"); }
    std::sort(names.begin(), names.end()); return names;
}
struct Storage {
    Fd root, state, transactions, lock;
    explicit Storage(const std::filesystem::path& path) {
        root = checked(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "Open scratch root");
        auto marker = read_at(root.get(), ".modbridge-scratch");
        state = directory_at(root.get(), ".modbridge", true);
        lock = checked(::openat(state.get(), "lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600), "Open scratch lock");
        regular(lock.get());
        if (stat_fd(lock.get()).st_nlink != 1) throw std::runtime_error("Scratch lock cannot be hard linked");
        if (::flock(lock.get(), LOCK_EX | LOCK_NB) != 0) error("Conflicting scratch job owns the lock");
        transactions = directory_at(state.get(), "transactions", true);
    }
};
std::string folded(std::string path);
bool overlaps(const std::string& a, const std::string& b);
[[noreturn]] void invalid_journal() { throw std::runtime_error("Invalid scratch journal schema or state"); }
void keys(const Json& object, std::initializer_list<const char*> allowed) {
    if (!object.is_object()) invalid_journal();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (std::none_of(allowed.begin(), allowed.end(), [&](const char* key) { return it.key() == key; })) invalid_journal();
    }
}
const std::string& string_field(const Json& object, const char* key) {
    const auto& value = object.at(key);
    if (!value.is_string()) invalid_journal();
    return value.get_ref<const std::string&>();
}
bool one_of(const std::string& value, std::initializer_list<const char*> choices) {
    return std::any_of(choices.begin(), choices.end(), [&](const char* choice) { return value == choice; });
}
std::uint64_t unsigned_value(const Json& value, std::uint64_t maximum) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0)) invalid_journal();
    const auto number = value.get<std::uint64_t>();
    if (number > maximum) invalid_journal();
    return number;
}
void digest(const Json& value) {
    if (!value.is_string()) invalid_journal();
    const auto& text = value.get_ref<const std::string&>();
    if (text.size() != 64 || !std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    })) invalid_journal();
}
void validate_journal(const Json& journal, const std::string& id) {
    keys(journal, {"schema_version", "transaction_id", "scope", "status", "intent", "files", "conflicts", "checkpoint", "interruption", "error"});
    if (unsigned_value(journal.at("schema_version"), record_schema_version) != record_schema_version ||
        string_field(journal, "transaction_id") != id || string_field(journal, "scope") != "desktop_scratch") invalid_journal();
    const auto& status = string_field(journal, "status");
    const auto& intent = string_field(journal, "intent");
    if (!one_of(status, {"preparing", "interrupted", "deployed", "removing", "removed", "rolled_back", "conflict"}) ||
        !one_of(intent, {"deploy", "rollback", "remove", "validate"})) invalid_journal();
    (void)unsigned_value(journal.at("checkpoint"), 2147483647);
    if (journal.contains("interruption") && !one_of(string_field(journal, "interruption"), {"test_failpoint", "operation_failed"})) invalid_journal();
    if (journal.contains("error")) (void)string_field(journal, "error");
    const auto& files = journal.at("files");
    const auto& conflicts = journal.at("conflicts");
    if (!files.is_array() || files.empty() || !conflicts.is_array()) invalid_journal();
    std::set<std::string> destinations;
    std::vector<std::string> phases;
    for (const auto& entry : files) {
        keys(entry, {"source", "destination", "sha256", "phase", "original_exists", "original_sha256", "original_mode"});
        components(string_field(entry, "source"));
        const auto& destination = string_field(entry, "destination");
        components(destination, true);
        const auto path = folded(destination);
        if (!destinations.insert(path).second) invalid_journal();
        digest(entry.at("sha256"));
        const auto& phase = string_field(entry, "phase");
        if (!one_of(phase, {"pending", "staged", "captured", "applying", "installed", "removing", "removed", "cancelled"})) invalid_journal();
        phases.push_back(phase);
        const bool capture_required = one_of(phase, {"captured", "applying", "installed", "removing", "removed"});
        const bool has_original = entry.contains("original_exists") || entry.contains("original_sha256") || entry.contains("original_mode");
        if (!capture_required && has_original) invalid_journal();
        if (capture_required) {
            if (!entry.at("original_exists").is_boolean()) invalid_journal();
            if (entry.at("original_exists").get<bool>()) {
                digest(entry.at("original_sha256"));
                (void)unsigned_value(entry.at("original_mode"), 0777);
            } else if (entry.contains("original_sha256") || entry.contains("original_mode")) invalid_journal();
        }
    }
    // Check every component ancestor against the complete set. Lexical
    // neighbors alone can be separated by punctuation in another filename.
    for (const auto& path : destinations) {
        for (auto slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1)) {
            if (destinations.contains(path.substr(0, slash))) invalid_journal();
        }
    }
    for (const auto& conflict : conflicts) {
        keys(conflict, {"destination", "reason"});
        const auto& destination = string_field(conflict, "destination");
        components(destination, true);
        if (!destinations.contains(folded(destination)) || string_field(conflict, "reason").empty()) invalid_journal();
    }
    const auto all_phases = [&](std::initializer_list<const char*> allowed) {
        return std::all_of(phases.begin(), phases.end(), [&](const std::string& phase) { return one_of(phase, allowed); });
    };
    if (status == "preparing" || status == "interrupted") {
        if (intent != "deploy" || !conflicts.empty() || !all_phases({"pending", "staged", "captured", "applying", "installed"})) invalid_journal();
        const std::vector<std::string> order{"pending", "staged", "captured", "applying", "installed"};
        int prior = 5, highest = 0, lowest = 5, applying = 0;
        for (const auto& phase : phases) {
            const auto rank = static_cast<int>(std::find(order.begin(), order.end(), phase) - order.begin());
            if (rank > prior) invalid_journal();
            prior = rank; highest = std::max(highest, rank); lowest = std::min(lowest, rank);
            if (phase == "applying") ++applying;
        }
        if ((highest >= 2 && lowest < 1) || (highest >= 3 && lowest < 2) || applying > 1) invalid_journal();
    } else if (status == "deployed" || intent == "validate") {
        if (!one_of(status, {"deployed", "conflict"}) || !one_of(intent, {"deploy", "validate"}) || !all_phases({"installed"})) invalid_journal();
        if ((status == "deployed") != conflicts.empty()) invalid_journal();
    } else if (status == "removed" || status == "rolled_back") {
        if (intent != (status == "removed" ? "remove" : "rollback") || !conflicts.empty() || !all_phases({"removed", "cancelled"})) invalid_journal();
    } else {
        if (!one_of(intent, {"remove", "rollback"}) || (status == "conflict" && conflicts.empty())) invalid_journal();
    }
}
Json load(int tx, const std::string& id) {
    auto journal = parse_journal_text(load_text(tx, "journal.json"));
    // Validate every record and supported transition before callers can save
    // a changed journal or touch any destination, including later entries.
    validate_journal(journal, id);
    return journal;
}
struct Observation { bool exists = false; std::string hash; unsigned mode = 0; };
Observation observe(const Parent& parent) {
    int raw = ::openat(parent.directory.get(), parent.leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (raw < 0 && errno == ENOENT) return {};
    auto fd = checked(raw, "Open destination without following links");
    regular(fd.get()); return {true, stream(fd.get()), static_cast<unsigned>(stat_fd(fd.get()).st_mode & 0777)};
}
bool matches_original(const Observation& current, const Json& entry) {
    return entry.at("original_exists").get<bool>() ? current.exists && current.hash == entry.at("original_sha256").get<std::string>() : !current.exists;
}
std::string numbered(std::size_t index, const std::string& suffix) { return std::to_string(index) + suffix; }
void copy_into(int tx, const std::string& name, int source, const std::string& expected, mode_t mode = 0600) {
    auto output = checked(::openat(tx, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600), "Create staged scratch file");
    const auto hash = stream(source, output.get());
    if (hash != expected) throw std::runtime_error("Scratch file hash changed or mismatched");
    if (::fchmod(output.get(), mode & 0777) != 0) error("Set scratch file permissions");
    sync(output.get()); sync(tx);
}
void publish(int root, int tx, const Json& entry, std::size_t index, bool restore) {
    const std::string destination = entry.at("destination");
    auto parent = parent_at(root, destination, false, true);
    const auto expected_current = restore ? entry.at("sha256").get<std::string>() : std::string{};
    const auto current = observe(parent);
    if (restore ? (!current.exists || current.hash != expected_current || current.mode != 0600) : !matches_original(current, entry))
        throw std::runtime_error("Destination precondition changed");
    if (restore && !entry.at("original_exists").get<bool>()) {
        same_parent(root, destination, parent);
        const auto recheck = observe(parent);
        if (!recheck.exists || recheck.hash != expected_current || recheck.mode != 0600) throw std::runtime_error("Destination changed before removal");
        if (::unlinkat(parent.directory.get(), parent.leaf.c_str(), 0) != 0) error("Remove installed scratch file");
        sync(parent.directory.get()); return;
    }
    const auto source_name = numbered(index, restore ? ".backup" : ".output");
    auto source = read_at(tx, source_name);
    const std::string hash = restore ? entry.at("original_sha256").get<std::string>() : entry.at("sha256").get<std::string>();
    const auto temporary = "replace-" + token();
    const auto mode = restore ? entry.at("original_mode").get<unsigned>() : 0600;
    copy_into(tx, temporary, source.get(), hash, static_cast<mode_t>(mode));
    try {
        same_parent(root, destination, parent);
        const auto recheck = observe(parent);
        if (restore ? (!recheck.exists || recheck.hash != expected_current || recheck.mode != 0600) : !matches_original(recheck, entry))
            throw std::runtime_error("Destination changed before replacement");
        if (::renameat(tx, temporary.c_str(), parent.directory.get(), parent.leaf.c_str()) != 0) error("Replace scratch destination");
        sync(parent.directory.get()); sync(tx);
    } catch (...) { ::unlinkat(tx, temporary.c_str(), 0); throw; }
}
struct Interrupted {};
void checkpoint(int tx, Json& journal, int& count, int fail_after) {
    journal["checkpoint"] = count;
    save(tx, journal);
    if (count++ == fail_after) {
        journal["status"] = "interrupted";
        journal["interruption"] = "test_failpoint";
        save(tx, journal); throw Interrupted{};
    }
}
Json rollback(Storage& storage, int tx, Json journal, bool removal) {
    journal["intent"] = removal ? "remove" : "rollback";
    journal["status"] = "removing";
    journal["conflicts"] = Json::array(); save(tx, journal);
    for (std::size_t i = journal["files"].size(); i-- > 0;) {
        auto& entry = journal["files"][i];
        const std::string phase = entry.at("phase");
        if (phase == "removed" || phase == "cancelled") continue;
        if (phase == "pending" || phase == "staged") { entry["phase"] = "cancelled"; save(tx, journal); continue; }
        try {
            auto parent = parent_at(storage.root.get(), entry.at("destination"), false, true);
            const auto current = observe(parent);
            if (matches_original(current, entry)) {
                // Identical original/output bytes do not prove restoration:
                // rename may already have replaced original permissions with
                // the installed mode. A never-applied capture stays untouched.
                const bool mode_restored = !current.exists || current.mode == entry.at("original_mode").get<unsigned>();
                if (phase == "captured" || mode_restored) {
                    entry["phase"] = "removed"; save(tx, journal); continue;
                }
            }
            if (phase == "captured" || !current.exists || current.hash != entry.at("sha256").get<std::string>() || current.mode != 0600)
                throw std::runtime_error("Current destination differs from transaction-owned bytes or permissions");
            entry["phase"] = "removing"; save(tx, journal);
            publish(storage.root.get(), tx, entry, i, true);
            entry["phase"] = "removed"; save(tx, journal);
        } catch (const std::exception& failure) {
            journal["conflicts"].push_back({{"destination", entry.at("destination")}, {"reason", failure.what()}});
            save(tx, journal);
        }
    }
    journal["status"] = journal["conflicts"].empty() ? (removal ? "removed" : "rolled_back") : "conflict";
    save(tx, journal); return journal;
}
Json validate_deployed(Storage& storage, int tx, Json journal) {
    journal["conflicts"] = Json::array();
    for (const auto& entry : journal["files"]) {
        try {
            auto parent = parent_at(storage.root.get(), entry.at("destination"), false, true);
            const auto current = observe(parent);
            if (!current.exists || current.hash != entry.at("sha256").get<std::string>()) throw std::runtime_error("Installed output changed");
        } catch (const std::exception& failure) {
            journal["conflicts"].push_back({{"destination", entry.at("destination")}, {"reason", failure.what()}});
        }
    }
    journal["intent"] = "validate";
    journal["status"] = journal["conflicts"].empty() ? "deployed" : "conflict";
    save(tx, journal); return journal;
}
std::string folded(std::string path) {
    for (auto& c : path) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return path;
}
bool overlaps(const std::string& a, const std::string& b) {
    return a == b || a.starts_with(b + "/") || b.starts_with(a + "/");
}
} // namespace

Json deploy_scratch(const std::filesystem::path& root, const std::filesystem::path& prepared,
                    const Json& manifest, int fail_after) {
    Storage storage(root);
    if (!manifest.is_object() || !manifest.contains("files") || !manifest.at("files").is_array() || manifest.at("files").empty())
        throw std::runtime_error("Manifest requires nonempty files array");
    auto inputs = checked(::open(prepared.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "Open prepared directory without following links");
    Json files = Json::array(); std::set<std::string> destinations;
    for (const auto& entry : manifest.at("files")) {
        const std::string source = entry.at("source"), destination = entry.at("destination"), hash = entry.at("sha256");
        components(source); components(destination, true);
        if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            throw std::runtime_error("Manifest requires lowercase SHA-256 digest");
        const auto folded_path = folded(destination);
        for (const auto& prior : destinations) if (overlaps(prior, folded_path)) throw std::runtime_error("Aliased or overlapping manifest destinations");
        destinations.insert(folded_path);
        files.push_back({{"source", source}, {"destination", destination}, {"sha256", hash}, {"phase", "pending"}});
    }
    for (const auto& prior_id : directories(storage.transactions.get())) {
        transaction_id(prior_id);
        auto prior = directory_at(storage.transactions.get(), prior_id, false);
        auto journal = load(prior.get(), prior_id);
        if (journal.at("status") == "removed" || journal.at("status") == "rolled_back") continue;
        for (const auto& entry : journal.at("files"))
            for (const auto& destination : destinations)
                if (overlaps(folded(entry.at("destination")), destination)) throw std::runtime_error("Destination belongs to an active scratch transaction");
    }
    std::string id;
    for (unsigned tries = 0; tries < 100; ++tries) {
        id = "tx-" + token();
        if (::mkdirat(storage.transactions.get(), id.c_str(), 0700) == 0) break;
        if (errno != EEXIST || tries == 99) error("Create scratch transaction");
    }
    sync(storage.transactions.get());
    auto tx = directory_at(storage.transactions.get(), id, false);
    Json journal{{"schema_version", record_schema_version}, {"transaction_id", id}, {"scope", "desktop_scratch"},
        {"status", "preparing"}, {"intent", "deploy"}, {"files", files}, {"conflicts", Json::array()}};
    int count = 0;
    try {
        checkpoint(tx.get(), journal, count, fail_after);
        for (std::size_t i = 0; i < files.size(); ++i) {
            auto& entry = journal["files"][i];
            auto source_parent = parent_at(inputs.get(), entry.at("source"));
            auto source = read_at(source_parent.directory.get(), source_parent.leaf);
            copy_into(tx.get(), numbered(i, ".output"), source.get(), entry.at("sha256"));
            entry["phase"] = "staged"; checkpoint(tx.get(), journal, count, fail_after);
        }
        for (std::size_t i = 0; i < files.size(); ++i) {
            auto& entry = journal["files"][i];
            auto parent = parent_at(storage.root.get(), entry.at("destination"), true, true);
            auto current = observe(parent);
            if (current.exists) {
                auto original = read_at(parent.directory.get(), parent.leaf);
                const auto original_mode = stat_fd(original.get()).st_mode & 0777;
                copy_into(tx.get(), numbered(i, ".backup"), original.get(), current.hash);
                entry["original_sha256"] = current.hash;
                entry["original_mode"] = original_mode;
            }
            entry["original_exists"] = current.exists;
            entry["phase"] = "captured"; checkpoint(tx.get(), journal, count, fail_after);
        }
        for (std::size_t i = 0; i < files.size(); ++i) {
            auto& entry = journal["files"][i];
            entry["phase"] = "applying"; checkpoint(tx.get(), journal, count, fail_after);
            publish(storage.root.get(), tx.get(), entry, i, false);
            checkpoint(tx.get(), journal, count, fail_after);
            entry["phase"] = "installed"; checkpoint(tx.get(), journal, count, fail_after);
        }
        journal["status"] = "deployed"; checkpoint(tx.get(), journal, count, fail_after);
    } catch (const Interrupted&) { return journal; }
    catch (const std::exception& failure) {
        journal["status"] = "interrupted"; journal["interruption"] = "operation_failed"; journal["error"] = failure.what();
        try { save(tx.get(), journal); } catch (...) { /* Retain the last durable state; report the original failure. */ }
        throw;
    }
    return journal;
}

Json recover_scratch(const std::filesystem::path& root) {
    Storage storage(root);
    Json result{{"schema_version", record_schema_version}, {"scope", "desktop_scratch"}, {"transactions", Json::array()}};
    for (const auto& id : directories(storage.transactions.get())) {
        try {
            transaction_id(id);
            auto tx = directory_at(storage.transactions.get(), id, false);
            auto journal = load(tx.get(), id);
            const std::string status = journal.at("status"), intent = journal.at("intent");
            if (status == "deployed" || intent == "validate") journal = validate_deployed(storage, tx.get(), journal);
            else if (status != "removed" && status != "rolled_back") journal = rollback(storage, tx.get(), journal, intent == "remove");
            result["transactions"].push_back(journal);
        } catch (const std::exception& failure) {
            // An interrupted initial publication or unusable metadata cannot
            // authorize changes. Retain everything and continue other journals.
            result["transactions"].push_back({{"transaction_id", id}, {"status", "conflict"},
                {"conflicts", Json::array({{{"reason", failure.what()}, {"scope", "journal_metadata"}}})}});
        }
    }
    return result;
}

Json uninstall_scratch(const std::filesystem::path& root, const std::string& id) {
    transaction_id(id); Storage storage(root);
    auto tx = directory_at(storage.transactions.get(), id, false);
    auto journal = load(tx.get(), id);
    if (journal.at("status") == "removed" || journal.at("status") == "rolled_back") return journal;
    return rollback(storage, tx.get(), journal, true);
}
} // namespace modbridge
