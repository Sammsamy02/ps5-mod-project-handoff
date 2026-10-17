#pragma once

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <Helpers/Utf16Utf8.hpp>
#endif

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>

#pragma warning(disable: 4068)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

class IBufferWriter
{
public:

    virtual FORCEINLINE void WriteString(std::string String) = 0;
    virtual FORCEINLINE void WriteString(std::string_view String) = 0;
    virtual FORCEINLINE void Write(void* Input, size_t Size) = 0;
    virtual FORCEINLINE void Seek(int Pos, int Origin = SEEK_CUR) = 0;
    virtual uint32_t Size() = 0;
};

class StreamWriter : IBufferWriter
{
    std::stringstream m_Stream;

public:

    virtual ~StreamWriter()
    {
        m_Stream.flush();
    }

    FORCEINLINE std::stringstream& GetBuffer()
    {
        return m_Stream;
    }

    FORCEINLINE void WriteString(std::string String) override
    {
        m_Stream.write(String.c_str(), String.size());
    }

    FORCEINLINE void WriteString(std::string_view String) override
    {
        m_Stream.write(String.data(), String.size());
    }

    FORCEINLINE void Write(void* Input, size_t Size) override
    {
        m_Stream.write((char*)Input, Size);
    }

    FORCEINLINE void Seek(int Pos, int Origin = SEEK_CUR) override
    {
        std::ios_base::seekdir direction;
        switch (Origin)
        {
        case SEEK_SET: direction = std::ios_base::beg; break;
        case SEEK_CUR: direction = std::ios_base::cur; break;
        case SEEK_END: direction = std::ios_base::end; break;
        default: throw std::invalid_argument("Invalid stream seek origin");
        }
        m_Stream.seekp(Pos, direction);
    }

    uint32_t Size() override
    {
        auto pos = m_Stream.tellp();
        this->Seek(0, SEEK_END);
        auto ret = m_Stream.tellp();
        this->Seek(pos, SEEK_SET);

        return ret;
    }

    template <typename T>
    FORCEINLINE void Write(T Input)
    {
        Write(&Input, sizeof(T));
    }
};

class FileWriter : IBufferWriter
{
    FILE* m_File{};

public:

#ifdef _WIN32
    FileWriter(const wchar_t* FileName)
#else
    FileWriter(const RC::CharType* FileName)
#endif
    {
#ifdef _WIN32
        auto fopen_r = _wfopen_s(&m_File, FileName, L"wb");
#else
        if (!FileName) throw std::invalid_argument("File writer path is null");
        const auto utf8_path = RC::Unicode::EncodeUtf8(RC::StringViewType{FileName});
        m_File = std::fopen(utf8_path.c_str(), "wb");
        const auto fopen_r = m_File ? 0 : (errno ? errno : EIO);
#endif
        if (fopen_r != 0)
        {
            RC::Output::send<RC::LogLevel::Error>(STR("Unable to open file for writing: '{}': {}\n"), FileName, RC::ensure_str(std::strerror(fopen_r)));
        }
    }

    virtual ~FileWriter()
    {
        if (m_File)
        {
            std::fclose(m_File);
        }
    }

    FORCEINLINE void WriteString(std::string String) override
    {
        if (!m_File)
        {
            return;
        }
        std::fwrite(String.c_str(), String.length(), 1, m_File);
    }

    FORCEINLINE void WriteString(std::string_view String) override
    {
        if (!m_File)
        {
            return;
        }
        std::fwrite(String.data(), String.size(), 1, m_File);
    }

    FORCEINLINE void Write(void* Input, size_t Size) override
    {
        if (!m_File)
        {
            return;
        }
        std::fwrite(Input, Size, 1, m_File);
    }

    FORCEINLINE void Seek(int Pos, int Origin = SEEK_CUR) override
    {
        if (!m_File)
        {
            return;
        }
        std::fseek(m_File, Pos, Origin);
    }

    uint32_t Size() override
    {
        if (!m_File)
        {
            return 0;
        }
        auto pos = std::ftell(m_File);
        std::fseek(m_File, 0, SEEK_END);
        auto ret = std::ftell(m_File);
        std::fseek(m_File, pos, SEEK_SET);
        return ret;
    }

    template <typename T>
    FORCEINLINE void Write(T Input)
    {
        Write(&Input, sizeof(T));
    }
};

#pragma clang diagnostic pop

#pragma warning(default: 4068)
