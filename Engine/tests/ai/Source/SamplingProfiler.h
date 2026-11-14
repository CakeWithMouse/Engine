// A small sampling profiler for the AI benchmark (Windows only): a second thread stops the measured
// thread about once a millisecond and records where it was. The report names functions and source
// lines from the PDB, so code inlined by the optimizer is still found by its line.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <timeapi.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")

class SamplingProfiler
{
public:
    ~SamplingProfiler() { Stop(); }

    /** Starts sampling the calling thread. Every sample remembers the value of tag at that moment. */
    void Start(const std::atomic<int>* tag = nullptr)
    {
        const HANDLE process = GetCurrentProcess();
        if (!DuplicateHandle(process, GetCurrentThread(), process, &target_, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, 0)) return;
        tag_ = tag;
        timeBeginPeriod(1);
        running_ = true;
        thread_ = std::thread([this] {
            while (running_)
            {
                Sleep(1);
                if (SuspendThread(target_) == static_cast<DWORD>(-1)) continue;
                CONTEXT context{};
                context.ContextFlags = CONTEXT_CONTROL;
                if (GetThreadContext(target_, &context)) { samples_.push_back(context.Rip); tags_.push_back(tag_ ? tag_->load() : 0); }
                ResumeThread(target_);
            }
        });
    }

    void Stop()
    {
        if (!running_) return;
        running_ = false;
        thread_.join();
        timeEndPeriod(1);
        CloseHandle(target_);
    }

    /** The busiest functions and source lines, as shares of the samples whose tag passes the filter. */
    template<class Filter> void Report(size_t top, Filter&& keep) const
    {
        const HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        if (!SymInitialize(process, nullptr, TRUE)) { std::printf("  profile: no symbols\n"); return; }
        std::map<std::string, size_t> functions, lines;
        size_t kept = 0;
        for (size_t s = 0; s < samples_.size(); ++s)
        {
            if (!keep(tags_[s])) continue;
            ++kept;
            const DWORD64 address = samples_[s];
            alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512];
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 offset = 0;
            ++functions[SymFromAddr(process, address, &offset, symbol) ? std::string(symbol->Name) : std::string("?")];
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD column = 0;
            if (SymGetLineFromAddr64(process, address, &column, &line))
            {
                std::string file = line.FileName;
                const size_t slash = file.find_last_of("\\/");
                if (slash != std::string::npos) file = file.substr(slash + 1);
                ++lines[file + ":" + std::to_string(line.LineNumber)];
            }
        }
        SymCleanup(process);
        Print("functions", functions, top, kept);
        Print("lines", lines, top, kept);
    }
    void Report(size_t top) const { Report(top, [](int) { return true; }); }

private:
    void Print(const char* title, const std::map<std::string, size_t>& counts, size_t top, size_t kept) const
    {
        std::vector<std::pair<size_t, std::string>> sorted;
        for (const auto& [name, count] : counts) sorted.emplace_back(count, name);
        std::sort(sorted.rbegin(), sorted.rend());
        std::printf("  profile %s (%zu samples):\n", title, kept);
        for (size_t i = 0; i < std::min(top, sorted.size()); ++i)
            std::printf("    %5.1f%%  %s\n", 100.0 * sorted[i].first / std::max<size_t>(1, kept), sorted[i].second.substr(0, 110).c_str());
    }

    HANDLE target_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::vector<DWORD64> samples_;
    std::vector<int> tags_;
    const std::atomic<int>* tag_ = nullptr;
};
#endif
