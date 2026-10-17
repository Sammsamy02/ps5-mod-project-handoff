#include "test.hpp"
#include "modbridge/core.hpp"
#include <fstream>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <csignal>

using namespace modbridge;
namespace fs = std::filesystem;
namespace {
void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out << bytes; out.close(); CHECK(out.good());
}
std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
const std::string installed_hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
struct Fixture {
    test::TempDir temp;
    fs::path root = temp.path / "scratch", prepared = temp.path / "prepared";
    Fixture() { fs::create_directories(root); fs::create_directories(prepared);
        write(root / ".modbridge-scratch", "modbridge scratch v1\n"); write(prepared / "input", "abc"); }
    Json manifest(const std::string& destination = "config.txt") {
        return {{"files", Json::array({{{"source", "input"}, {"destination", destination}, {"sha256", installed_hash}}})}};
    }
};
std::string id(const Json& result) { return result.at("transaction_id").get<std::string>(); }
}

// Missing originals or wrong backup restoration changes consumer-visible bytes.
TEST(restore_original) {
    Fixture f; write(f.root / "config.txt", "original");
    auto deployed = deploy_scratch(f.root, f.prepared, f.manifest());
    CHECK_EQ(deployed.at("status"), "deployed"); CHECK_EQ(read(f.root / "config.txt"), "abc");
    auto removed = uninstall_scratch(f.root, id(deployed));
    CHECK_EQ(removed.at("status"), "removed"); CHECK_EQ(read(f.root / "config.txt"), "original");
    CHECK_EQ(uninstall_scratch(f.root, id(deployed)).at("status"), "removed");
}

TEST(new_file_removal) {
    Fixture f; auto deployed = deploy_scratch(f.root, f.prepared, f.manifest("nested/new.txt"));
    CHECK_EQ(read(f.root / "nested/new.txt"), "abc");
    CHECK_EQ(uninstall_scratch(f.root, id(deployed)).at("status"), "removed");
    CHECK(!fs::exists(f.root / "nested/new.txt"));
}

// Uninstall must compare current content, not infer ownership from filenames.
TEST(edit_before_removal_preserved) {
    Fixture f; write(f.root / "config.txt", "original");
    auto deployed = deploy_scratch(f.root, f.prepared, f.manifest());
    write(f.root / "config.txt", "user edit");
    auto removed = uninstall_scratch(f.root, id(deployed));
    CHECK_EQ(removed.at("status"), "conflict"); CHECK(!removed.at("conflicts").empty());
    CHECK_EQ(read(f.root / "config.txt"), "user edit");
    const auto tx = f.root / ".modbridge/transactions" / id(deployed);
    CHECK_EQ(read(tx / "0.backup"), "original"); CHECK_EQ(read(tx / "0.output"), "abc");
    auto recovered = recover_scratch(f.root); CHECK(recovered.is_object());
    CHECK_EQ(read(f.root / "config.txt"), "user edit");
}

// A captured destination is a precondition for installation and recovery.
TEST(destination_precondition) {
    Fixture f; write(f.root / "config.txt", "original");
    auto interrupted = deploy_scratch(f.root, f.prepared, f.manifest(), 2);
    CHECK_EQ(interrupted.at("status"), "interrupted");
    CHECK_EQ(read(f.root / "config.txt"), "original");
    write(f.root / "config.txt", "external change");
    auto recovered = recover_scratch(f.root);
    CHECK_EQ(recovered.at("transactions").at(0).at("status"), "conflict");
    CHECK_EQ(read(f.root / "config.txt"), "external change");
    CHECK_EQ(read(f.root / ".modbridge/transactions" / id(interrupted) / "0.backup"), "original");
}

TEST(symlink_parent_escape) {
    Fixture f; auto outside = f.temp.path / "outside"; fs::create_directories(outside);
    write(outside / "config.txt", "outside bytes");
    fs::create_directory_symlink(outside, f.root / "escape");
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest("escape/config.txt")));
    CHECK_EQ(read(outside / "config.txt"), "outside bytes");
    fs::create_symlink(outside / "config.txt", f.prepared / "linked-input");
    auto manifest = f.manifest(); manifest["files"][0]["source"] = "linked-input";
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, manifest));
}

TEST(unmarked_root_refused) {
    Fixture f; fs::remove(f.root / ".modbridge-scratch");
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest()));
    CHECK_THROWS(recover_scratch(f.root));
    CHECK(!fs::exists(f.root / "config.txt"));
}

TEST(conflicting_job_lock) {
    Fixture f; fs::create_directories(f.root / ".modbridge");
    int lock = ::open((f.root / ".modbridge/lock").c_str(), O_CREAT | O_RDWR, 0600);
    CHECK(lock >= 0); CHECK_EQ(::flock(lock, LOCK_EX | LOCK_NB), 0);
    try { CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest())); }
    catch (...) { ::close(lock); throw; }
    ::close(lock);
    auto deployed = deploy_scratch(f.root, f.prepared, f.manifest());
    CHECK_EQ(deployed.at("status"), "deployed");
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest()));
    CHECK_EQ(read(f.root / "config.txt"), "abc");
}

// All persisted phases, including rename before installed-state recording, must
// recover by bytes. This catches trusting a state label after a partial write.
TEST(interrupt_each_operation) {
    unsigned interrupted_count = 0;
    for (int checkpoint = 0; checkpoint < 20; ++checkpoint) {
        Fixture f; write(f.root / "config.txt", "original");
        auto manifest = f.manifest();
        manifest["files"].push_back({{"source", "input"}, {"destination", "new.txt"}, {"sha256", installed_hash}});
        auto result = deploy_scratch(f.root, f.prepared, manifest, checkpoint);
        if (result.at("status") == "deployed") {
            CHECK_EQ(uninstall_scratch(f.root, id(result)).at("status"), "removed");
            break;
        }
        ++interrupted_count; CHECK_EQ(result.at("status"), "interrupted");
        auto recovered = recover_scratch(f.root);
        CHECK_EQ(recovered.at("transactions").at(0).at("status"), "rolled_back");
        CHECK_EQ(read(f.root / "config.txt"), "original"); CHECK(!fs::exists(f.root / "new.txt"));
        CHECK_EQ(recover_scratch(f.root).at("transactions").at(0).at("status"), "rolled_back");
    }
    CHECK(interrupted_count >= 10);
}

TEST(interrupted_recovery_preserves_unexpected_bytes) {
    for (int checkpoint = 3; checkpoint <= 4; ++checkpoint) {
        Fixture f; write(f.root / "config.txt", "original");
        auto result = deploy_scratch(f.root, f.prepared, f.manifest(), checkpoint);
        CHECK_EQ(result.at("status"), "interrupted");
        write(f.root / "config.txt", "later user bytes");
        auto recovered = recover_scratch(f.root);
        CHECK_EQ(recovered.at("transactions").at(0).at("status"), "conflict");
        CHECK_EQ(read(f.root / "config.txt"), "later user bytes");
        auto tx = f.root / ".modbridge/transactions" / id(result);
        CHECK_EQ(read(tx / "0.backup"), "original"); CHECK_EQ(read(tx / "0.output"), "abc");
    }
}

TEST(unsafe_manifest_and_hash_refused) {
    for (const auto& path : {"../escape", "/absolute", ".modbridge/journal", "a/../b", "a//b", "C:\\outside"}) {
        Fixture f; CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest(path)));
    }
    Fixture f; auto manifest = f.manifest(); manifest["files"][0]["sha256"] = std::string(64, '0');
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, manifest)); CHECK(!fs::exists(f.root / "config.txt"));
    manifest = f.manifest(); manifest["files"].push_back(manifest["files"][0]);
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, manifest));
}

TEST(state_symlinks_refused) {
    Fixture f; auto outside = f.temp.path / "outside"; fs::create_directories(outside);
    fs::create_directory_symlink(outside, f.root / ".modbridge");
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest()));
    CHECK(fs::is_empty(outside));
}

TEST(backup_corruption_preserves_current_file) {
    Fixture f; write(f.root / "config.txt", "original");
    auto result = deploy_scratch(f.root, f.prepared, f.manifest());
    write(f.root / ".modbridge/transactions" / id(result) / "0.backup", "bad backup");
    CHECK_EQ(uninstall_scratch(f.root, id(result)).at("status"), "conflict");
    CHECK_EQ(read(f.root / "config.txt"), "abc");
}

TEST(case_alias_control_paths_refused) {
    Fixture f;
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest(".MODBRIDGE/lock")));
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, f.manifest(".ModBridge-Scratch")));
    CHECK_EQ(read(f.root / ".modbridge-scratch"), "modbridge scratch v1\n");
}

TEST(orphan_and_linked_journals_report_metadata_conflicts) {
    Fixture f;
    const std::string orphan = "tx-" + std::string(32, 'a');
    const std::string linked = "tx-" + std::string(32, 'b');
    fs::create_directories(f.root / ".modbridge/transactions" / orphan);
    fs::create_directories(f.root / ".modbridge/transactions" / linked);
    auto outside = f.temp.path / "outside.json"; write(outside, "outside bytes");
    fs::create_symlink(outside, f.root / ".modbridge/transactions" / linked / "journal.json");
    auto recovered = recover_scratch(f.root);
    CHECK_EQ(recovered.at("transactions").size(), 2U);
    for (const auto& journal : recovered.at("transactions")) CHECK_EQ(journal.at("status"), "conflict");
    CHECK_EQ(read(outside), "outside bytes");
    CHECK(fs::exists(f.root / ".modbridge/transactions" / orphan));
}


// A special file is unsafe before it is read, including a FIFO with no writer.
TEST(special_destination_does_not_block_removal) {
    Fixture f; auto deployed = deploy_scratch(f.root, f.prepared, f.manifest());
    fs::remove(f.root / "config.txt");
    CHECK_EQ(::mkfifo((f.root / "config.txt").c_str(), 0600), 0);
    const auto child = ::fork(); CHECK(child >= 0);
    if (child == 0) {
        try { auto result = uninstall_scratch(f.root, id(deployed)); ::_exit(result.at("status") == "conflict" ? 0 : 1); }
        catch (...) { ::_exit(2); }
    }
    int status = 0; bool completed = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (::waitpid(child, &status, WNOHANG) == child) { completed = true; break; }
        ::usleep(10000);
    }
    if (!completed) { ::kill(child, SIGKILL); ::waitpid(child, &status, 0); }
    CHECK(completed); CHECK(WIFEXITED(status)); CHECK_EQ(WEXITSTATUS(status), 0);
    CHECK(fs::is_fifo(fs::symlink_status(f.root / "config.txt")));
}

// Real POSIX file-size exhaustion exercises failed writes without mocking I/O.
TEST(failed_staging_write_never_replaces_original) {
    Fixture f; write(f.root / "config.txt", "original"); write(f.prepared / "input", std::string(8192, 'x'));
    auto manifest = f.manifest();
    // Independent Python hashlib.sha256(b'x' * 8192), not the production helper.
    manifest["files"][0]["sha256"] = "18f8d2eb4a387bbc1e37ec099a7326805739bc9c99ecf0f14b808a5bcb65bf49";
    const auto child = ::fork(); CHECK(child >= 0);
    if (child == 0) {
        ::signal(SIGXFSZ, SIG_IGN);
        struct rlimit limit{4096, 4096}; if (::setrlimit(RLIMIT_FSIZE, &limit) != 0) ::_exit(2);
        try { (void)deploy_scratch(f.root, f.prepared, manifest); ::_exit(1); }
        catch (const std::exception&) { ::_exit(0); }
    }
    int status = 0; CHECK_EQ(::waitpid(child, &status, 0), child);
    CHECK(WIFEXITED(status)); CHECK_EQ(WEXITSTATUS(status), 0);
    CHECK_EQ(read(f.root / "config.txt"), "original");
    CHECK_EQ(recover_scratch(f.root).at("transactions").at(0).at("status"), "rolled_back");
    CHECK_EQ(read(f.root / "config.txt"), "original");
}

TEST(deep_journal_rejected_without_recursive_copy) {
    Fixture f; write(f.root / "config.txt", "original");
    auto bad = deploy_scratch(f.root, f.prepared, f.manifest());
    auto other = deploy_scratch(f.root, f.prepared, f.manifest("other.txt"), 4);
    const auto tx = f.root / ".modbridge/transactions" / id(bad);
    const auto path = tx / "journal.json";
    const auto initial = read(path);
    const auto close = initial.find_last_of('}'); CHECK(close != std::string::npos);
    const auto deep = initial.substr(0, close) + ",\"extra\":" + std::string(20000, '[') + "0" + std::string(20000, ']') + "}\n";
    write(path, deep);
    const auto output = read(tx / "0.output"), backup = read(tx / "0.backup");
    const auto child = ::fork(); CHECK(child >= 0);
    if (child == 0) {
        try { write(f.temp.path / "result.json", recover_scratch(f.root).dump()); ::_exit(0); }
        catch (...) { ::_exit(2); }
    }
    int status = 0; CHECK_EQ(::waitpid(child, &status, 0), child);
    CHECK(WIFEXITED(status)); CHECK_EQ(WEXITSTATUS(status), 0);
    const auto recovered = Json::parse(read(f.temp.path / "result.json"));
    bool conflict = false, continued = false;
    for (const auto& result : recovered.at("transactions")) {
        if (result.at("transaction_id") == id(bad)) conflict = result.at("status") == "conflict";
        if (result.at("transaction_id") == id(other)) continued = result.at("status") == "rolled_back";
    }
    CHECK(conflict); CHECK(continued); CHECK(!fs::exists(f.root / "other.txt"));
    CHECK_EQ(read(f.root / "config.txt"), "abc"); CHECK_EQ(read(path), deep);
    CHECK_EQ(read(tx / "0.output"), output); CHECK_EQ(read(tx / "0.backup"), backup);
}

TEST(malformed_entire_journal_refused_before_mutation) {
    const std::vector<std::function<void(Json&)>> corruptions{
        [](Json& j) { j["files"][0].erase("phase"); },
        [](Json& j) { j["files"][0]["phase"] = "invalid-phase"; },
        [](Json& j) { j["files"][0]["phase"] = 7; },
        [](Json& j) { j["files"][0]["original_exists"] = "true"; },
        [](Json& j) { j["files"][0].erase("original_mode"); },
        [](Json& j) { j["files"][0]["original_mode"] = -1; },
        [](Json& j) { j["files"][0]["original_mode"] = 010000; },
        [](Json& j) { j["files"][0]["original_mode"] = 0600.5; },
        [](Json& j) { j["files"][0]["original_sha256"] = "invalid"; },
        [](Json& j) { j["files"][0]["sha256"] = Json::array({1}); },
        [](Json& j) { j["files"][0]["source"] = "../escape"; },
        [](Json& j) { j["status"] = "unknown-status"; },
        [](Json& j) { j["intent"] = "unknown-intent"; },
        [](Json& j) { j["scope"] = "live_console"; },
        [](Json& j) { j["schema_version"] = 1.0; },
        [](Json& j) { j["conflicts"] = "invalid"; },
        [](Json& j) { j["checkpoint"] = -1; },
        [](Json& j) { j["files"][1]["destination"] = "CONFIG.TXT"; },
        [](Json& j) { j["files"][1]["destination"] = "config.txt/nested"; },
        [](Json& j) { j["status"] = "deployed"; j["files"][0]["phase"] = "staged"; },
        [](Json& j) { j["status"] = "removed"; j["intent"] = "remove"; },
        [](Json& j) { j["status"] = "removing"; j["intent"] = "remove"; j["files"][0]["phase"] = "staged"; }
    };
    for (const auto& corrupt : corruptions) {
        Fixture f; write(f.root / "config.txt", "original");
        auto manifest = f.manifest();
        manifest["files"].push_back({{"source", "input"}, {"destination", "other.txt"}, {"sha256", installed_hash}});
        auto deployed = deploy_scratch(f.root, f.prepared, manifest);
        const auto tx = f.root / ".modbridge/transactions" / id(deployed);
        auto journal = Json::parse(read(tx / "journal.json"));
        journal["status"] = "interrupted"; journal["intent"] = "deploy";
        corrupt(journal);
        const auto text = journal.dump(); write(tx / "journal.json", text);
        const auto backup = read(tx / "0.backup"), first_output = read(tx / "0.output"), second_output = read(tx / "1.output");
        const auto recovered = recover_scratch(f.root);
        CHECK_EQ(recovered.at("transactions").at(0).at("status"), "conflict");
        CHECK_EQ(recovered.at("transactions").at(0).at("conflicts").at(0).at("scope"), "journal_metadata");
        CHECK_EQ(read(f.root / "config.txt"), "abc"); CHECK_EQ(read(f.root / "other.txt"), "abc");
        CHECK_EQ(read(tx / "journal.json"), text); CHECK_EQ(read(tx / "0.backup"), backup);
        CHECK_EQ(read(tx / "0.output"), first_output); CHECK_EQ(read(tx / "1.output"), second_output);
        CHECK_THROWS(uninstall_scratch(f.root, id(deployed)));
        CHECK_EQ(read(tx / "journal.json"), text); CHECK_EQ(read(f.root / "other.txt"), "abc");
    }
}

namespace {
unsigned mode(const fs::path& path) { struct stat st{}; CHECK_EQ(::stat(path.c_str(), &st), 0); return st.st_mode & 0777; }
}
TEST(identical_bytes_restore_original_permissions) {
    for (unsigned permissions : {0755U, 0640U}) {
        Fixture f; write(f.root / "config.txt", "abc");
        CHECK_EQ(::chmod((f.root / "config.txt").c_str(), permissions), 0);
        auto deployed = deploy_scratch(f.root, f.prepared, f.manifest());
        CHECK_EQ(mode(f.root / "config.txt"), 0600U);
        CHECK_EQ(uninstall_scratch(f.root, id(deployed)).at("status"), "removed");
        CHECK_EQ(mode(f.root / "config.txt"), permissions); CHECK_EQ(read(f.root / "config.txt"), "abc");
    }
}
TEST(identical_bytes_interruption_restores_original_permissions) {
    for (int checkpoint : {2, 3, 4, 5}) {
        Fixture f; write(f.root / "config.txt", "abc"); CHECK_EQ(::chmod((f.root / "config.txt").c_str(), 0755), 0);
        auto interrupted = deploy_scratch(f.root, f.prepared, f.manifest(), checkpoint);
        CHECK_EQ(interrupted.at("status"), "interrupted");
        CHECK_EQ(recover_scratch(f.root).at("transactions").at(0).at("status"), "rolled_back");
        CHECK_EQ(mode(f.root / "config.txt"), 0755U); CHECK_EQ(read(f.root / "config.txt"), "abc");
    }
}

TEST(oversized_generated_journal_is_not_published) {
    Fixture f; write(f.root / "config.txt", "original");
    auto manifest = f.manifest();
    manifest["files"][0]["source"] = std::string(9 * 1024 * 1024, 'x');
    manifest["files"].push_back({{"source", std::string(9 * 1024 * 1024, 'y')}, {"destination", "other.txt"}, {"sha256", installed_hash}});
    CHECK_THROWS(deploy_scratch(f.root, f.prepared, manifest));
    CHECK_EQ(read(f.root / "config.txt"), "original");
    for (const auto& directory : fs::directory_iterator(f.root / ".modbridge/transactions")) {
        const auto journal = directory.path() / "journal.json";
        CHECK(!fs::exists(journal) || fs::file_size(journal) <= 16777216);
    }
}

TEST(intervening_sorted_path_cannot_hide_journal_ancestor) {
    for (bool ancestor_first : {true, false}) {
        Fixture f;
        Json files = Json::array();
        for (const auto& destination : (ancestor_first ? std::vector<std::string>{"a", "a-foo", "third"}
                                                     : std::vector<std::string>{"third", "a-foo", "a"})) {
            files.push_back({{"source", "input"}, {"destination", destination}, {"sha256", installed_hash}});
        }
        auto deployed = deploy_scratch(f.root, f.prepared, {{"files", files}});
        const auto tx = f.root / ".modbridge/transactions" / id(deployed);
        auto journal = Json::parse(read(tx / "journal.json"));
        journal["status"] = "interrupted"; journal["intent"] = "deploy";
        journal["files"][ancestor_first ? 2 : 0]["destination"] = "a/b";
        const auto text = journal.dump(); write(tx / "journal.json", text);
        auto recovered = recover_scratch(f.root);
        CHECK_EQ(read(f.root / "a"), "abc"); CHECK_EQ(read(f.root / "a-foo"), "abc");
        CHECK_EQ(read(f.root / "third"), "abc"); CHECK_EQ(read(tx / "journal.json"), text);
        for (unsigned i = 0; i < 3; ++i) CHECK_EQ(read(tx / (std::to_string(i) + ".output")), "abc");
        const auto& result = recovered.at("transactions").at(0);
        CHECK_EQ(result.at("status"), "conflict");
        CHECK_EQ(result.at("conflicts").at(0).at("scope"), "journal_metadata");
        CHECK_THROWS(uninstall_scratch(f.root, id(deployed)));
        CHECK_EQ(read(f.root / "a"), "abc"); CHECK_EQ(read(tx / "journal.json"), text);
    }
}
