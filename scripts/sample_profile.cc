// A minimal sampling profiler for the renderer on Windows. Nothing else on
// this machine could profile it: the Windows tools want PDB symbols, and a
// MinGW build carries DWARF and a COFF symbol table instead.
//
// It launches a command, then every few milliseconds suspends each of the
// command's threads, reads the instruction pointer, and resumes it. Output
// is one line per sample: "w" (render worker) or "m" (main thread) and the
// address relative to the exe's load base. Samples in system DLLs are only
// counted, as "wext"/"mext". scripts/symbolize_profile.py maps the addresses
// to functions through `nm`.
//
//   g++ -O2 -std=c++17 scripts/sample_profile.cc -o build/sample_profile
//       -lpsapi -lwinmm -static
//   RT_LOG=off build/sample_profile build/profile.txt <full Windows path to
//       raytracer.exe> assets/scenes/macho-cows.lua
//   python scripts/symbolize_profile.py build/profile.txt build/raytracer.exe
//
// (Commands wrapped here; a trailing backslash would continue the comment.)
//
// Limits: self time only, no call stacks; inlined code counts toward its
// caller; the tick is ~4 ms rather than 1 ms, so profile a render of a
// second or more. It pauses the threads it samples, so do not time a
// profiled run.

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: sampler OUT EXE [ARGS...]\n");
    return 1;
  }
  std::string cmd;
  for (int i = 2; i < argc; ++i) {
    if (i > 2) cmd += ' ';
    cmd += '"';
    cmd += argv[i];
    cmd += '"';
  }
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<char> buf(cmd.begin(), cmd.end());
  buf.push_back('\0');
  if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr,
                      nullptr, &si, &pi)) {
    std::fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
    return 1;
  }
  timeBeginPeriod(1);

  DWORD64 base = 0, size = 0;
  // Worker and main-thread samples kept apart: the main thread spends the
  // render waiting in join, which is not render work.
  std::vector<DWORD64> samples[2];
  long long ext[2] = {0, 0}, ticks = 0;

  // Thread handles are cached and the list refreshed every 25 ticks:
  // snapshotting every thread on the system each tick is what limits the
  // sampling rate.
  struct Tracked { DWORD id; HANDLE handle; };
  std::vector<Tracked> threads;
  while (WaitForSingleObject(pi.hProcess, 1) == WAIT_TIMEOUT) {
    if (base == 0) {
      HMODULE mods[1];
      DWORD needed = 0;
      if (EnumProcessModules(pi.hProcess, mods, sizeof(mods), &needed) &&
          needed > 0) {
        MODULEINFO info{};
        if (GetModuleInformation(pi.hProcess, mods[0], &info, sizeof(info))) {
          base = reinterpret_cast<DWORD64>(info.lpBaseOfDll);
          size = info.SizeOfImage;
        }
      }
      if (base == 0) continue;
    }
    if (ticks % 25 == 0) {
      for (Tracked& t : threads) CloseHandle(t.handle);
      threads.clear();
      HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
      if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        for (BOOL ok = Thread32First(snap, &te); ok;
             ok = Thread32Next(snap, &te)) {
          if (te.th32OwnerProcessID != pi.dwProcessId) continue;
          HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT,
                                 FALSE, te.th32ThreadID);
          if (th) threads.push_back({te.th32ThreadID, th});
        }
        CloseHandle(snap);
      }
    }
    ++ticks;
    for (Tracked& t : threads) {
      if (SuspendThread(t.handle) == static_cast<DWORD>(-1)) continue;
      CONTEXT ctx{};
      ctx.ContextFlags = CONTEXT_CONTROL;
      if (GetThreadContext(t.handle, &ctx)) {
        const int is_main = t.id == pi.dwThreadId ? 1 : 0;
        if (ctx.Rip >= base && ctx.Rip < base + size)
          samples[is_main].push_back(ctx.Rip - base);
        else
          ++ext[is_main];
      }
      ResumeThread(t.handle);
    }
  }
  for (Tracked& t : threads) CloseHandle(t.handle);
  timeEndPeriod(1);

  FILE* out = std::fopen(argv[1], "w");
  if (!out) return 1;
  const char* tag[2] = {"w", "m"};
  for (int k = 0; k < 2; ++k) {
    for (DWORD64 rva : samples[k]) std::fprintf(out, "%s %llx\n", tag[k], rva);
    std::fprintf(out, "%sext %lld\n", tag[k], ext[k]);
  }
  std::fclose(out);
  std::fprintf(stderr,
               "sampler: %lld ticks; workers %zu in exe, %lld outside; "
               "main %zu in exe, %lld outside\n",
               ticks, samples[0].size(), ext[0], samples[1].size(), ext[1]);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  return static_cast<int>(code);
}
