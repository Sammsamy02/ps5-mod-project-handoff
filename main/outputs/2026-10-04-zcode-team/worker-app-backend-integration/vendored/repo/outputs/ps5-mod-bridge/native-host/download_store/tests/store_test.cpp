#include "../store.h"
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#ifdef MB_DOWNLOAD_STORE_SYNC_TEST
// Compile only store.cpp with -Dfsync=mb_download_store_test_fsync. The test
// translation unit keeps the real syscall for deterministic owned-file faults.
static unsigned directory_sync_failures = 0;
static std::string metadata_race_path, metadata_race_bytes;
extern "C" int mb_download_store_test_fsync(int fd) {
  struct stat state{};
  assert(fstat(fd, &state) == 0);
  if (S_ISDIR(state.st_mode) && directory_sync_failures) {
    --directory_sync_failures;
    errno = EIO;
    return -1;
  }
  const int result = fsync(fd);
  if (result == 0 && S_ISREG(state.st_mode) && !metadata_race_path.empty()) {
    int rival = open(metadata_race_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(rival >= 0 && write(rival, metadata_race_bytes.data(), metadata_race_bytes.size()) == ssize_t(metadata_race_bytes.size()));
    assert(fsync(rival) == 0 && close(rival) == 0);
    metadata_race_path.clear();
    metadata_race_bytes.clear();
  }
  return result;
}
#endif

static void metadata_tests() {
  char temporary[] = "/tmp/mb-store-metadata-XXXXXX";
  assert(mkdtemp(temporary));
  const std::string root = temporary;
  int dir = open(root.c_str(), O_RDONLY | O_DIRECTORY);
  int source = open((root + "/source").c_str(), O_CREAT | O_RDWR, 0600);
  assert(write(source, "archive", 7) == 7);
  close(source);
  source = open((root + "/source").c_str(), O_RDONLY);
  mb_download_store *store = nullptr;
  assert(mb_download_store_open(dir, nullptr, &store) == MB_DOWNLOAD_STORE_OK);
  char output[MB_DOWNLOAD_STORE_METADATA_LIMIT + 1];
  const std::string hash = picosha2::hash256_hex_string(std::string("archive"));
  assert(mb_download_store_publish(store, source, "fixture", 1, 2, 7,
      hash.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  const std::string id = nlohmann::json::parse(output)["download_id"];
  const std::string capture = "{\"source\":{\"name\":\"Owned 星\"}, \"requirements\":{\"game\":\"fixture\",\"mod_id\":1}}";
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_NOT_FOUND);
  assert(output[0] == 0);
#ifdef MB_DOWNLOAD_STORE_SYNC_TEST
  // Final link survives failed directory fsync; exact retries must retry fsync.
  directory_sync_failures = 1;
  assert(mb_download_store_publish_metadata(store, id.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_STORAGE_FAILED);
  assert(directory_sync_failures == 0);
  directory_sync_failures = 1;
  assert(mb_download_store_publish_metadata(store, id.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_STORAGE_FAILED);
  assert(directory_sync_failures == 0);
#endif
  assert(mb_download_store_publish_metadata(store, id.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_OK);
  assert(mb_download_store_publish_metadata(store, id.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_OK);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  assert(std::string(output) == capture);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, capture.size()) == MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL);
  assert(output[0] == 0);
  const std::string changed = "{\"source\":{\"name\":\"Changed\"}}";
  assert(mb_download_store_publish_metadata(store, id.c_str(), changed.data(), changed.size()) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  mb_download_store_free(store);
  assert(mb_download_store_open(dir, nullptr, &store) == MB_DOWNLOAD_STORE_OK);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  assert(std::string(output) == capture);
  assert(mb_download_store_list(store, output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  const auto public_items = nlohmann::json::parse(output)["items"];
  assert(public_items.size() == 1 && public_items[0].size() == 8 && !public_items[0].contains("metadata"));
  std::ifstream manifest(root + "/" + id + ".json");
  nlohmann::json archive_manifest;
  manifest >> archive_manifest;
  assert(archive_manifest.size() == 15);
  manifest.close();

  for (const std::string &invalid : {std::string{}, std::string("null"), std::string("[]"),
      std::string("{\"name\":1,\"name\":2}"), std::string("{}\0trailing", 11),
      std::string(MB_DOWNLOAD_STORE_METADATA_LIMIT + 1, ' ')}) {
    assert(mb_download_store_publish_metadata(store, id.c_str(), invalid.data(), invalid.size()) == MB_DOWNLOAD_STORE_BAD_ARGUMENT);
  }
  std::string deep(65, '['); deep += '0'; deep.append(65, ']');
  deep = "{\"deep\":" + deep + "}";
  assert(mb_download_store_publish_metadata(store, id.c_str(), deep.data(), deep.size()) == MB_DOWNLOAD_STORE_BAD_ARGUMENT);
  const std::string missing = "dl-" + std::string(64, '0');
  assert(mb_download_store_publish_metadata(store, missing.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_NOT_FOUND);
  assert(mb_download_store_lookup_metadata(store, missing.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_NOT_FOUND);

  const std::string sidecar = root + "/" + id + ".metadata.json";
  std::ifstream sidecar_input(sidecar);
  const std::string sidecar_bytes((std::istreambuf_iterator<char>(sidecar_input)), std::istreambuf_iterator<char>());
  sidecar_input.close();
  auto replace_sidecar = [&](const std::string &bytes) {
    int file = open(sidecar.c_str(), O_WRONLY | O_TRUNC);
    assert(file >= 0 && write(file, bytes.data(), bytes.size()) == ssize_t(bytes.size()));
    assert(close(file) == 0);
  };
  replace_sidecar("{}");
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  assert(output[0] == 0);
  assert(mb_download_store_publish_metadata(store, id.c_str(), capture.data(), capture.size()) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  replace_sidecar(sidecar_bytes);
  assert(chmod(sidecar.c_str(), 0644) == 0);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  assert(chmod(sidecar.c_str(), 0600) == 0);
  const std::string sidecar_backup = sidecar + ".old";
  assert(rename(sidecar.c_str(), sidecar_backup.c_str()) == 0);
  assert(symlink(sidecar_backup.c_str(), sidecar.c_str()) == 0);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  assert(unlink(sidecar.c_str()) == 0);
  assert(rename(sidecar_backup.c_str(), sidecar.c_str()) == 0);
  auto tampered = nlohmann::json::parse(sidecar_bytes);
  tampered["metadata"] = changed;
  replace_sidecar(tampered.dump());
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  replace_sidecar(sidecar_bytes);

  // Copying a valid capture beside a different source must not change its binding.
  assert(mb_download_store_publish(store, source, "fixture", 1, 3, 7,
      hash.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  const std::string other = nlohmann::json::parse(output)["download_id"];
  const std::string other_sidecar = root + "/" + other + ".metadata.json";
  int copied = open(other_sidecar.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  assert(copied >= 0 && write(copied, sidecar_bytes.data(), sidecar_bytes.size()) == ssize_t(sidecar_bytes.size()));
  close(copied);
  assert(mb_download_store_lookup_metadata(store, other.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  unlink(other_sidecar.c_str());
  auto rebound = nlohmann::json::parse(sidecar_bytes);
  rebound["download_id"] = other;
  const auto rebound_bytes = rebound.dump();
  copied = open(other_sidecar.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  assert(copied >= 0 && write(copied, rebound_bytes.data(), rebound_bytes.size()) == ssize_t(rebound_bytes.size()));
  close(copied);
  assert(mb_download_store_lookup_metadata(store, other.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  assert(unlink(other_sidecar.c_str()) == 0);
  const std::string maximum = "{\"text\":\"" + std::string(MB_DOWNLOAD_STORE_METADATA_LIMIT - 11, 'x') + "\"}";
  assert(maximum.size() == MB_DOWNLOAD_STORE_METADATA_LIMIT);
#ifdef MB_DOWNLOAD_STORE_SYNC_TEST
  // A same-byte rival commits after our missing check but before exclusive link.
  // Exercise the EEXIST path and require its directory-sync failure to propagate.
  std::ifstream other_manifest_input(root + "/" + other + ".json");
  nlohmann::json other_manifest;
  other_manifest_input >> other_manifest;
  other_manifest_input.close();
  metadata_race_path = other_sidecar;
  metadata_race_bytes = nlohmann::json{{"version", 1}, {"download_id", other},
      {"archive_record_sha256", picosha2::hash256_hex_string(other_manifest.dump())},
      {"metadata_sha256", picosha2::hash256_hex_string(maximum)}, {"metadata", maximum}}.dump();
  directory_sync_failures = 1;
  assert(mb_download_store_publish_metadata(store, other.c_str(), maximum.data(), maximum.size()) == MB_DOWNLOAD_STORE_STORAGE_FAILED);
  assert(metadata_race_path.empty() && directory_sync_failures == 0);
#endif
  assert(mb_download_store_publish_metadata(store, other.c_str(), maximum.data(), maximum.size()) == MB_DOWNLOAD_STORE_OK);
  assert(mb_download_store_lookup_metadata(store, other.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  assert(std::string(output) == maximum);
  // A committed final plus its unfinished staging link stays readable on reopen.
  const std::string pending_link = root + "/" + other + ".metadata-pending-123-1";
  assert(link(other_sidecar.c_str(), pending_link.c_str()) == 0);
  mb_download_store_free(store);
  assert(mb_download_store_open(dir, nullptr, &store) == MB_DOWNLOAD_STORE_OK);
  assert(mb_download_store_lookup_metadata(store, other.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_OK);
  assert(std::string(output) == maximum);
  assert(mb_download_store_publish_metadata(store, other.c_str(), maximum.data(), maximum.size()) == MB_DOWNLOAD_STORE_OK);
  assert(unlink(pending_link.c_str()) == 0);

  // Archive fingerprint changes invalidate metadata without reading its full bytes.
  const std::string archive_path = root + "/" + id + ".archive";
  int changed_archive = open(archive_path.c_str(), O_WRONLY | O_TRUNC);
  assert(changed_archive >= 0 && write(changed_archive, "altered", 7) == 7);
  close(changed_archive);
  assert(mb_download_store_lookup_metadata(store, id.c_str(), output, sizeof(output)) == MB_DOWNLOAD_STORE_INTEGRITY_FAILED);
  assert(output[0] == 0);
  close(source);
  mb_download_store_free(store);
  for (const auto &download : {id, other})
    for (const auto *extension : {".archive", ".json", ".metadata.json"})
      assert(unlink((root + "/" + download + extension).c_str()) == 0);
  assert(unlink((root + "/source").c_str()) == 0);
  close(dir);
  assert(rmdir(root.c_str()) == 0);
}

int main() {
  char temporary[] = "/tmp/mb-store-test-XXXXXX";
  assert(mkdtemp(temporary));
  char root[4096];
  assert(realpath(temporary, root));
  int dir = open(root, O_RDONLY | O_DIRECTORY);
  std::string source = std::string(root) + "/source";
  int w = open(source.c_str(), O_CREAT | O_WRONLY, 0600);
  assert(write(w, "archive", 7) == 7);
  close(w);
  int fd = open(source.c_str(), O_RDONLY);
  std::string hash = picosha2::hash256_hex_string(std::string("archive"));
  char out[262144];
  mb_download_store *c = nullptr;
  assert(!mb_download_store_open(dir, root, &c));
  assert(mb_download_store_publish(c, fd, "testgame", 1, 2, 8, hash.c_str(),
                                   out, sizeof(out)) == 3);
  assert(mb_download_store_publish(c, fd, "testgame", 1, 2, 7,
                                   std::string(64, '0').c_str(), out,
                                   sizeof(out)) == 3);
  assert(!mb_download_store_publish(c, fd, "testgame", 1, 2, 7, hash.c_str(),
                                    out, sizeof(out)));
  auto r = nlohmann::json::parse(out);
  std::string id = r["download_id"],
              archive = std::string(root) + "/" + id + ".archive";
  assert(!mb_download_store_publish(c, fd, "testgame", 1, 2, 7, hash.c_str(),
                                    out, sizeof(out)));
  close(fd);
  unlink(source.c_str());
  mb_download_store_free(c);
  assert(!mb_download_store_open(dir, root, &c));
  assert(!mb_download_store_list(c, out, sizeof(out)));
  assert(nlohmann::json::parse(out)["items"].size() == 1);
  assert(!mb_download_store_lookup(c, id.c_str(), out, sizeof(out)));
  assert(nlohmann::json::parse(out)["archive_path"] == archive);
  // Complete orphan before metadata commit is reusable.
  std::string meta = std::string(root) + "/" + id + ".json";
  unlink(meta.c_str());
  fd = open(archive.c_str(), O_RDONLY);
  assert(!mb_download_store_list(c, out, sizeof(out)));
  assert(nlohmann::json::parse(out)["items"].empty());
  assert(!mb_download_store_publish(c, fd, "testgame", 1, 2, 7, hash.c_str(),
                                    out, sizeof(out)));
  close(fd);
  // A crash after linking the final archive leaves a matching owned staging
  // link.
  unlink(meta.c_str());
  std::string stage = std::string(root) + "/" + id + ".staging-123-1";
  assert(!link(archive.c_str(), stage.c_str()));
  fd = open(archive.c_str(), O_RDONLY);
  assert(!mb_download_store_publish(c, fd, "testgame", 1, 2, 7, hash.c_str(),
                                    out, sizeof(out)));
  close(fd);
  assert(access(stage.c_str(), F_OK) != 0);
  // Partial staged bytes from a crashed copy never become a final archive.
  unlink(meta.c_str());
  unlink(archive.c_str());
  w = open(stage.c_str(), O_CREAT | O_WRONLY, 0600);
  assert(write(w, "arc", 3) == 3);
  close(w);
  w = open(source.c_str(), O_CREAT | O_WRONLY, 0600);
  assert(write(w, "archive", 7) == 7);
  close(w);
  fd = open(source.c_str(), O_RDONLY);
  assert(!mb_download_store_publish(c, fd, "testgame", 1, 2, 7, hash.c_str(),
                                    out, sizeof(out)));
  close(fd);
  unlink(source.c_str());
  struct stat partial{};
  assert(!stat(stage.c_str(), &partial) && partial.st_size == 3);
  unlink(stage.c_str());
  // Private schema fields cannot be replaced with caller-controlled response
  // keys.
  std::ifstream input(meta);
  nlohmann::json original;
  input >> original;
  input.close();
  auto malicious = original;
  malicious.erase("archive_ino");
  malicious["archive_path"] = "/secret/private";
  auto replace_meta = [&](const nlohmann::json &value) {
    int m = open(meta.c_str(), O_WRONLY | O_TRUNC);
    std::string text = value.dump();
    assert(write(m, text.data(), text.size()) == ssize_t(text.size()));
    close(m);
  };
  replace_meta(malicious);
  assert(mb_download_store_lookup(c, id.c_str(), out, sizeof(out)) ==
             MB_DOWNLOAD_STORE_INTEGRITY_FAILED &&
         out[0] == 0);
  assert(!mb_download_store_list(c, out, sizeof(out)));
  assert(nlohmann::json::parse(out)["items"].empty());
  replace_meta(original);
  // Identical bytes on a substituted inode are rejected.
  std::string old = archive + ".old";
  rename(archive.c_str(), old.c_str());
  w = open(archive.c_str(), O_CREAT | O_WRONLY, 0600);
  write(w, "archive", 7);
  close(w);
  assert(mb_download_store_lookup(c, id.c_str(), out, sizeof(out)) == 3);
  assert(!mb_download_store_list(c, out, sizeof(out)));
  assert(nlohmann::json::parse(out)["items"].empty());
  unlink(archive.c_str());
  symlink(old.c_str(), archive.c_str());
  assert(mb_download_store_lookup(c, id.c_str(), out, sizeof(out)) == 3);
  unlink(archive.c_str());
  rename(old.c_str(), archive.c_str());
  w = open(archive.c_str(), O_WRONLY | O_TRUNC);
  close(w);
  assert(mb_download_store_lookup(c, id.c_str(), out, sizeof(out)) == 3);
  mb_download_store_free(c);
  unlink(meta.c_str());
  unlink(archive.c_str());
  close(dir);
  rmdir(root);
  metadata_tests();
  std::cout << "download store functional tests passed\n";
}
