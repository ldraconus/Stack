#pragma once

#include <codecvt>
#include <locale>
#include <string>
#include <vector>

#include <stdarg.h>
#include <stdio.h>

using converter_type = std::codecvt_utf8<wchar_t>;

#ifndef NO
#define NO(x) x(const x&) = delete; x(x&&) = delete; x& operator=(const x&) = delete; x& operator=(x&&) = delete; // NOLINT
#endif

namespace cstd {

inline std::wstring_convert<converter_type, wchar_t> converter; // NOLINT

class file {
public:
    enum mode { Read = 1, Write = 2, Append = 4, Update = 8 };

    std::string mode2Str(mode md) {
        std::string res;
        if (md & Read) res = "r";
        else if (md & Write) res = "w";
        else if (md & Append) res = "a";
        if (md & Update) res += "+";
        return res;
    }

private:
    FILE* mFile = nullptr;

public:
    file(FILE* f)
        : mFile(f) { }
    file(const std::string& name, mode md = Read)
        : mFile(fopen(name.c_str(), mode2Str(md).c_str())) {
    }
    file(const std::wstring& name, mode md = Read)
        : file(converter.to_bytes(name), md) {
    }
    file(const file&) = delete;
    file(file&& f) noexcept
        : mFile(f.mFile) { f.mFile = nullptr; }
    virtual ~file() { if (mFile) fclose(mFile); mFile = nullptr; } // NOLINT

    file& operator=(const file&) = delete;
    file& operator=(file&& f) noexcept { mFile = f.mFile; f.mFile = nullptr; return *this; }

      void clearErrors() { ::clearerr(mFile); }
      bool eof()         { return ::feof(mFile); }
      bool error()       { return ::ferror(mFile); }

      void flush()        { ::fflush(mFile); }
    size_t postion()      { return ::ftell(mFile); }
      void seek(size_t p) { ::fseek(mFile, p, SEEK_SET); } // NOLINT

    char getChar()       { return ::getc(mFile); }    // NOLINT
    void putChar(char c) { ::putc(c, mFile); }

     std::string getString()                       { std::string r; char b[1024]; while (::fgets(b, sizeof(b), mFile) != nullptr) { r += b; if (r.ends_with('\n')) break; } return r; } // NOLINT
            void putString(const std::string& s)   { ::fputs(s.c_str(), mFile); }
    std::wstring getWString()                      { std::string r; char b[1024]; while (::fgets(b, sizeof(b), mFile) != nullptr) { r += b; if (r.ends_with('\n')) break; } return converter.from_bytes(r); } // NOLINT
            void putString(const std::wstring& ws) { std::string s = converter.to_bytes(ws); ::fputs(s.c_str(), mFile); }

    int print(const char* f, ...) { va_list v; va_start(v, f); return vfprintf(mFile, f, v); } // NOLINT
    int scan(const char* f, ...)  { va_list v; va_start(v, f); return vfscanf(mFile, f, v); } // NOLINT
    int print(const wchar_t* wf, ...) { va_list v; va_start(v, wf); std::string f = converter.to_bytes(wf); return vfprintf(mFile, f.c_str(), v); } // NOLINT
    int scan(const wchar_t* wf, ...)  { va_list v; va_start(v, wf); std::string f = converter.to_bytes(wf); return vfscanf(mFile, f.c_str(), v); } // NOLINT

    template <typename T>
    size_t read(std::vector<T>& store, size_t num) {
        T block[num]; // NOLINT
        size_t count = ::fread((void*) block, sizeof(T), num, mFile);
        for (size_t i = 0; i < count; ++i) store.emplace_back(block[i]);
        return count;
    }

    template <typename T>
    size_t write(std::vector<T>& store, size_t from, size_t count) {
        return fwrite((void*) store.data(), sizeof(T), count, mFile);
    }
};

inline file in(stdin);    // NOLINT
inline file err(stdout);  // NOLINT
inline file out(stdout);  // NOLINT

}
