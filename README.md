# OpenTaskManager

Ever wondered how a task manager is programmed? Look no further. 

`tm.c` is a tiny, fast open source Windows task manager. One file, pure Win32 + GDI, no frameworks.

![ProcessesTab](https://raw.github.com/SaxonRah/OpenTaskManager/main/screenshots/ProcessesTab.png "Processes Tab")
![PerformanceTab](https://raw.github.com/SaxonRah/OpenTaskManager/main/screenshots/PerformanceTab.png "Performance Tab")

---

# Performance design:
 - Single syscall per refresh for all processes: NtQuerySystemInformation
   (SystemProcessInformation) returns CPU times, private working set, I/O
   counters, threads and handles for every process. No OpenProcess per
   process, no Toolhelp, no WMI, no PDH, no ETW sessions.
 - Zero heap allocations in steady state. Fixed tables in .bss plus one
   VirtualAlloc'd snapshot buffer that only ever grows.
 - CPU/IO deltas via an open addressed hash keyed on PID (+ create time to
   survive PID reuse), double buffered with generation stamps, so it never
   needs clearing or rehashing.
 - Only the visible rows are drawn. GDI into a cached back buffer that only
   grows, one BitBlt per frame. No animation, no hover repaints.
 - The thread sleeps in GetMessage between refreshes. The refresh timer is
   coalescable, so Windows can batch our wakeups with others.
 - Minimized: timer killed and working set trimmed -> 0% CPU, near-0 RAM.
 - EcoQoS / efficiency mode (Win11) so sampling runs on efficiency cores at
   low clocks. Sorting/filtering is skipped when the Processes tab is hidden.
 - Shell32/dwmapi are loaded lazily, only if a feature needs them.


---

# Building 
* Open `x64 Native Tools Command Prompt for VS`
* The shortcut for `x64 Native Tools Command Prompt for VS` is something like `%comspec% /k "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`
* Open that either by searching for `x64` in Start, you'll see `x64 Native Tools Command Prompt for VS`, run it.
* While in `x64 Native Tools Command Prompt for VS` use `cd` to the where the repo folder is.
* Build `tm.exe` by running `.\build.bat` 
* Run by double clicking the `tm.exe`

--- 

# Documentation
* Type to filter
* Esc clear filter
* Up/Down/PgUp/PgDn/Home/End select,
* Del end task
* F5 refresh now
* Pause pause/resume
* Ctrl+U change speed
* Ctrl+Tab / Ctrl+1 / Ctrl+2 switch tab.
* Right-click a row for more.


---

MIT - 2026 Robert Valentine
