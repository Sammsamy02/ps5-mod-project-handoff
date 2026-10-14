// fs-throw-49 P1 host test (Mac-only). Links the REAL PosixFile.cpp (either the
// unpatched CONTROL copy or the P1 copy) + the real port File.cpp and checks:
//   1. success path: File::open(create) with missing parent directories works
//      and leaves File internal_error false (must be identical for both);
//   2. error path: a parent component is a regular file, so creating the
//      parent directories fails. CONTROL throws std::filesystem::filesystem_error
//      and leaves internal_error false; P1 must behave like PC
//      (WinFile::create_all_directories): internal_error = true and a File error
//      from the std::runtime_error family that is NOT a filesystem_error.
// Prints how many checks ran; exit 0 = all pass, 1 = a check failed,
// 2 = zero checks ran or bad usage.
#include <File/File.hpp>

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::printf("usage: %s <empty scratch dir>\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root = argv[1];
    int checks = 0;
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) ++fails;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    };
#ifdef EXPECT_P1
    std::printf("variant: P1\n");
#else
    std::printf("variant: CONTROL\n");
#endif

    // 1. success path
    {
        const auto p = root / "a" / "b" / "c.txt";
        RC::File::Internal::StaticStorage::internal_error = false;
        bool threw = false;
        try
        {
            auto f = RC::File::open(p, RC::File::OpenFor::Writing, RC::File::OverwriteExistingFile::Yes, RC::File::CreateIfNonExistent::Yes);
            f.close();
        }
        catch (const std::exception& e)
        {
            threw = true;
            std::printf("     unexpected: %s\n", e.what());
        }
        check(!threw, "success: open(create) with missing parent directories does not throw");
        std::error_code ec;
        check(std::filesystem::is_regular_file(p, ec) && !ec, "success: the file exists afterwards");
        check(!RC::File::Internal::StaticStorage::internal_error, "success: internal_error stays false");
    }

    // 2. error path: 'blocker' is a regular file, so 'blocker/sub' cannot be created
    {
        const auto blocker = root / "blocker";
        if (FILE* f = std::fopen(blocker.c_str(), "w"))
        {
            std::fclose(f);
        }
        const auto p = blocker / "sub" / "x.txt";
        RC::File::Internal::StaticStorage::internal_error = false;
        bool threw_fs = false;
        bool threw_runtime_family = false;
        std::string what;
        try
        {
            auto f = RC::File::open(p, RC::File::OpenFor::Writing, RC::File::OverwriteExistingFile::Yes, RC::File::CreateIfNonExistent::Yes);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            threw_fs = true;
            threw_runtime_family = true;
            what = e.what();
        }
        catch (const std::runtime_error& e)
        {
            threw_runtime_family = true;
            what = e.what();
        }
        catch (...)
        {
            what = "(non-std exception)";
        }
        std::printf("     error what(): %s\n", what.c_str());
        check(threw_runtime_family, "error: a std::runtime_error-family exception is thrown (fail closed, like PC)");
#ifdef EXPECT_P1
        check(!threw_fs, "error: P1 throws a File error, not std::filesystem::filesystem_error");
        check(RC::File::Internal::StaticStorage::internal_error, "error: P1 sets internal_error (PC WinFile::create_all_directories parity)");
#else
        check(threw_fs, "error: CONTROL throws std::filesystem::filesystem_error");
        check(!RC::File::Internal::StaticStorage::internal_error, "error: CONTROL leaves internal_error false (the PC difference P1 removes)");
#endif
    }

    std::printf("P1-HOST-TEST: %d checks, %d failed\n", checks, fails);
    if (checks == 0)
    {
        return 2;
    }
    return fails ? 1 : 0;
}
