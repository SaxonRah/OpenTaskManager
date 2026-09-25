#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef int64_t i64;

/* NT defs */
typedef struct {
  USHORT Length, MaximumLength;
  PWSTR Buffer;
} UStr;
typedef struct {
  ULONG NextEntryOffset, NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize;
  ULONG HardFaultCount, NumberOfThreadsHighWatermark;
  ULONGLONG CycleTime;
  LARGE_INTEGER CreateTime, UserTime, KernelTime;
  UStr ImageName;
  LONG BasePriority;
  HANDLE UniqueProcessId, InheritedFromUniqueProcessId;
  ULONG HandleCount, SessionId;
  ULONG_PTR UniqueProcessKey;
  SIZE_T PeakVirtualSize, VirtualSize;
  ULONG PageFaultCount;
  SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage,
      QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage,
      PagefileUsage, PeakPagefileUsage, PrivatePageCount;
  LARGE_INTEGER ReadOperationCount, WriteOperationCount, OtherOperationCount,
      ReadTransferCount, WriteTransferCount, OtherTransferCount;
} SPI;
typedef struct {
  LARGE_INTEGER Idle, Kernel, User, Dpc, Intr;
  ULONG IntrCount;
} SPPI;
typedef LONG(NTAPI *NtQSI_t)(ULONG, PVOID, ULONG, PULONG);
typedef UINT_PTR(WINAPI *SetCoalTimer_t)(HWND, UINT_PTR, UINT, TIMERPROC,
                                         ULONG);
typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
typedef BOOL(WINAPI *SetDpiCtx_t)(HANDLE);
typedef BOOL(WINAPI *SetProcInfo_t)(HANDLE, int, LPVOID, DWORD);
typedef HINSTANCE(WINAPI *ShellExecW_t)(HWND, LPCWSTR, LPCWSTR, LPCWSTR,
                                        LPCWSTR, INT);
typedef HRESULT(WINAPI *DwmSetAttr_t)(HWND, DWORD, LPCVOID, DWORD);
typedef BOOL(WINAPI *QueryImageName_t)(HANDLE, DWORD, LPWSTR, PDWORD);

static NtQSI_t NtQSI;
static SetCoalTimer_t pSetCoalescableTimer;
static GetDpiForWindow_t pGetDpiForWindow;

/* data */
typedef struct {
  u32 pid, threads, handles, cpu; /* cpu in hundredths of a percent */
  u64 mem, io;                    /* private working set; IO bytes/s */
  u32 nlen;
  WCHAR name[60];
} Row;
typedef struct {
  u32 pid, gen;
  u64 create, cpu, io;
} Hist;

#define MAX_ROWS 8192
#define HBITS 14
#define HSIZE (1u << HBITS)
#define HMASK (HSIZE - 1)
#define HIST_N 60
#define MAX_CPU 256

static Row rows[MAX_ROWS];
static u32 nrows;
static u32 view[MAX_ROWS];
static u32 nview;
static Hist htab[2][HSIZE];
static u32 hcur, hgen = 1;

static BYTE *snap;
static ULONG snap_cap;
static u64 prev_tot, prev_idle, prev_qpc, qpf;
static u32 sys_cpu, tot_threads, tot_handles, nproc;
static u64 mem_total, mem_used, commit_used, commit_limit;
static u16 cpu_hist[HIST_N], mem_hist[HIST_N];
static int hist_pos, hist_count;
static int ncpu;
static u64 core_prev_busy[MAX_CPU], core_prev_tot[MAX_CPU];
static u16 core_pct[MAX_CPU];
static SPPI sppi[MAX_CPU];

/* ui state */
enum { C_NAME, C_PID, C_CPU, C_MEM, C_IO, C_THR, C_HND, NCOL };
static const WCHAR *col_title[NCOL] = {
    L"Name", L"PID", L"CPU", L"Memory", L"I/O", L"Threads", L"Handles"};
static const int col_w[NCOL] = {0, 64, 64, 96, 92, 68, 76};

static HWND hwnd;
static int dpi = 96, W, H;
static HFONT f_ui, f_bold, f_big;
static int fh_ui, fh_bold, fh_big;
static HDC mdc;
static HBITMAP mbm, mbm_old;
static int bw, bh;
static int tab_h, hdr_y, hdr_h, row_h, list_y, list_h, status_y, status_h, sb_w,
    vis_rows, pad;
static int col_x[NCOL + 1], tab_x[3];
static int tab, scroll, sort_col = C_CPU, sort_desc = 1, view_dirty = 1;
static u32 sel_pid;
static int has_sel, sel_idx = -1;
static int dragging, drag_off, minimized, paused;
static const u32 speeds[] = {250, 500, 1000, 2000, 5000};
static int speed = 2;
static WCHAR filter[64];
static int nfilter;
static WCHAR status_msg[160];
static u64 status_until;

#define S(x) MulDiv((x), dpi, 96)

static const COLORREF C_BG = RGB(24, 24, 27), C_ALT = RGB(28, 28, 32),
                      C_PANEL = RGB(32, 32, 36), C_HDR = RGB(36, 36, 41),
                      C_SEL = RGB(38, 79, 120), C_TEXT = RGB(232, 232, 236),
                      C_DIM = RGB(140, 140, 150), C_LINE = RGB(52, 52, 58),
                      C_LINE2 = RGB(74, 74, 82), C_ACC = RGB(96, 165, 250),
                      C_ACC_F = RGB(28, 52, 82), C_MEMC = RGB(167, 139, 250),
                      C_MEM_F = RGB(52, 42, 86), C_HEAT = RGB(214, 160, 40),
                      C_INPUT = RGB(18, 18, 21), C_GRID = RGB(36, 36, 42);

/* formatting (no CRT printf) */
typedef struct {
  WCHAR b[160];
  int n;
} SB;
static void sb_s(SB *s, const WCHAR *t) {
  while (*t && s->n < 159)
    s->b[s->n++] = *t++;
  s->b[s->n] = 0;
}
static void sb_u(SB *s, u64 v) {
  WCHAR t[24];
  int n = 0;
  do
    t[n++] = L'0' + (WCHAR)(v % 10);
  while (v /= 10);
  while (n && s->n < 159)
    s->b[s->n++] = t[--n];
  s->b[s->n] = 0;
}
static void sb_u2(SB *s, u64 v) {
  if (v < 10)
    sb_s(s, L"0");
  sb_u(s, v);
}
static void sb_t(SB *s, u64 tenths) {
  sb_u(s, tenths / 10);
  sb_s(s, L".");
  sb_u(s, tenths % 10);
}
static void sb_pct(SB *s, u32 h) {
  sb_t(s, (h + 5) / 10);
  sb_s(s, L"%");
}
static void sb_mb(SB *s, u64 b) {
  sb_t(s, (b * 10 + 524288) >> 20);
  sb_s(s, L" MB");
}
static void sb_gb(SB *s, u64 b) {
  sb_t(s, (b * 10 + 536870912) >> 30);
  sb_s(s, L" GB");
}
static void sb_rate(SB *s, u64 b) {
  if (b == 0) {
    sb_s(s, L"0 MB/s");
  } else if (b < 1u << 20) {
    sb_u(s, (b + 1023) >> 10);
    sb_s(s, L" KB/s");
  } else if (b < 1u << 30) {
    sb_t(s, (b * 10 + 524288) >> 20);
    sb_s(s, L" MB/s");
  } else {
    sb_t(s, (b * 10 + 536870912) >> 30);
    sb_s(s, L" GB/s");
  }
}
static u64 ft64(FILETIME f) {
  return ((u64)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

/* sampling */
static void sample(void) {
  ULONG need = 0;
  LONG st;
  for (;;) {
    st = NtQSI(5 /*SystemProcessInformation*/, snap, snap_cap, &need);
    if (st != (LONG)0xC0000004L /*STATUS_INFO_LENGTH_MISMATCH*/)
      break;
    if (snap)
      VirtualFree(snap, 0, MEM_RELEASE);
    if (need < 262144)
      need = 262144;
    snap_cap = (need + (need >> 2) + 0xFFFF) &
               ~0xFFFFu; /* headroom so we rarely regrow */
    snap = (BYTE *)VirtualAlloc(NULL, snap_cap, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
    if (!snap) {
      snap_cap = 0;
      return;
    }
  }
  if (st < 0)
    return;

  FILETIME fi, fk, fu;
  GetSystemTimes(&fi, &fk, &fu);
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  u64 idle = ft64(fi),
      tot = ft64(fk) + ft64(fu); /* kernel time includes idle */
  int first = prev_tot == 0;
  u64 dtot = tot - prev_tot, didle = idle - prev_idle;
  double dt = first ? 0 : (double)(q.QuadPart - prev_qpc) / (double)qpf;
  prev_tot = tot;
  prev_idle = idle;
  prev_qpc = q.QuadPart;
  if (!first && dtot)
    sys_cpu = didle >= dtot ? 0 : (u32)((dtot - didle) * 10000 / dtot);

  u32 pg = hgen, ng = ++hgen;
  Hist *ph = htab[hcur], *nh = htab[hcur ^ 1];
  hcur ^= 1;
  u32 n = 0, thr = 0, hnd = 0;
  SPI *p = (SPI *)snap;
  for (;;) {
    u32 pid = (u32)(ULONG_PTR)p->UniqueProcessId;
    u64 cpu = (u64)p->UserTime.QuadPart + (u64)p->KernelTime.QuadPart;
    u64 io = (u64)p->ReadTransferCount.QuadPart +
             (u64)p->WriteTransferCount.QuadPart;
    u64 ct = (u64)p->CreateTime.QuadPart;
    u32 h0 = ((pid >> 2) * 2654435761u) >> (32 - HBITS), h;

    Hist *e = NULL; /* previous sample for this pid */
    for (h = h0; ph[h].gen == pg; h = (h + 1) & HMASK)
      if (ph[h].pid == pid) {
        if (ph[h].create == ct)
          e = &ph[h];
        break;
      }
    for (h = h0; nh[h].gen == ng; h = (h + 1) & HMASK) {
    }
    nh[h].pid = pid;
    nh[h].gen = ng;
    nh[h].create = ct;
    nh[h].cpu = cpu;
    nh[h].io = io;

    thr += p->NumberOfThreads;
    hnd += p->HandleCount;
    if (pid != 0 && n < MAX_ROWS) { /* skip System Idle Process */
      Row *r = &rows[n++];
      r->pid = pid;
      r->threads = p->NumberOfThreads;
      r->handles = p->HandleCount;
      r->mem = (u64)p->WorkingSetPrivateSize.QuadPart;
      u64 c = (e && dtot && cpu >= e->cpu) ? (cpu - e->cpu) * 10000 / dtot : 0;
      r->cpu = c > 10000 ? 10000 : (u32)c;
      r->io =
          (e && dt > 0 && io >= e->io) ? (u64)((double)(io - e->io) / dt) : 0;
      u32 len = p->ImageName.Length / 2;
      if (!p->ImageName.Buffer) {
        static const WCHAR sys[] = L"System";
        len = 6;
        memcpy(r->name, sys, 14);
      } else {
        if (len > 59)
          len = 59;
        memcpy(r->name, p->ImageName.Buffer, len * 2);
      }
      r->name[len] = 0;
      r->nlen = len;
    }
    if (!p->NextEntryOffset)
      break;
    p = (SPI *)((BYTE *)p + p->NextEntryOffset);
  }
  nrows = n;
  nproc = n;
  tot_threads = thr;
  tot_handles = hnd;

  /* per-core */
  if (NtQSI(8 /*SystemProcessorPerformanceInformation*/, sppi,
            (ULONG)(sizeof(SPPI) * ncpu), &need) >= 0) {
    for (int i = 0; i < ncpu; i++) {
      u64 t = (u64)sppi[i].Kernel.QuadPart + (u64)sppi[i].User.QuadPart;
      u64 b = t - (u64)sppi[i].Idle.QuadPart;
      u64 d = t - core_prev_tot[i], db = b - core_prev_busy[i];
      core_pct[i] = (!first && d) ? (u16)(db >= d ? 10000 : db * 10000 / d) : 0;
      core_prev_tot[i] = t;
      core_prev_busy[i] = b;
    }
  }
  MEMORYSTATUSEX ms;
  ms.dwLength = sizeof ms;
  if (GlobalMemoryStatusEx(&ms)) {
    mem_total = ms.ullTotalPhys;
    mem_used = ms.ullTotalPhys - ms.ullAvailPhys;
    commit_limit = ms.ullTotalPageFile;
    commit_used = ms.ullTotalPageFile - ms.ullAvailPageFile;
  }
  if (!first) {
    cpu_hist[hist_pos] = (u16)sys_cpu;
    mem_hist[hist_pos] = mem_total ? (u16)(mem_used * 10000 / mem_total) : 0;
    hist_pos = (hist_pos + 1) % HIST_N;
    if (hist_count < HIST_N)
      hist_count++;
  }
  view_dirty = 1;
}

/* view (filter + sort) */
static int cmp_rows(const void *a, const void *b) {
  const Row *x = &rows[*(const u32 *)a], *y = &rows[*(const u32 *)b];
  int d = 0;
  switch (sort_col) {
  case C_NAME:
    d = CompareStringOrdinal(x->name, (int)x->nlen, y->name, (int)y->nlen,
                             TRUE) -
        2;
    break;
  case C_PID:
    d = (x->pid > y->pid) - (x->pid < y->pid);
    break;
  case C_CPU:
    d = (x->cpu > y->cpu) - (x->cpu < y->cpu);
    break;
  case C_MEM:
    d = (x->mem > y->mem) - (x->mem < y->mem);
    break;
  case C_IO:
    d = (x->io > y->io) - (x->io < y->io);
    break;
  case C_THR:
    d = (x->threads > y->threads) - (x->threads < y->threads);
    break;
  case C_HND:
    d = (x->handles > y->handles) - (x->handles < y->handles);
    break;
  }
  if (sort_desc)
    d = -d;
  return d ? d
           : (x->pid > y->pid) -
                 (x->pid < y->pid); /* stable: no flicker between refreshes */
}
static WCHAR fold(WCHAR c) {
  return (c >= L'A' && c <= L'Z') ? (WCHAR)(c + 32) : c;
}
static int matches(const Row *r) {
  if (!nfilter)
    return 1;
  for (u32 i = 0; i + (u32)nfilter <= r->nlen; i++) {
    int k = 0;
    while (k < nfilter && fold(r->name[i + k]) == fold(filter[k]))
      k++;
    if (k == nfilter)
      return 1;
  }
  /* also match a PID typed as digits */
  u32 v = 0;
  for (int k = 0; k < nfilter; k++) {
    if (filter[k] < L'0' || filter[k] > L'9')
      return 0;
    v = v * 10 + (filter[k] - L'0');
  }
  return r->pid == v;
}
static void clamp_scroll(void) {
  int mx = (int)nview - vis_rows;
  if (mx < 0)
    mx = 0;
  if (scroll > mx)
    scroll = mx;
  if (scroll < 0)
    scroll = 0;
}
static void rebuild_view(void) {
  nview = 0;
  for (u32 i = 0; i < nrows; i++)
    if (matches(&rows[i]))
      view[nview++] = i;
  qsort(view, nview, sizeof(u32), cmp_rows);
  sel_idx = -1;
  if (has_sel)
    for (u32 i = 0; i < nview; i++)
      if (rows[view[i]].pid == sel_pid) {
        sel_idx = (int)i;
        break;
      }
  clamp_scroll();
  view_dirty = 0;
}
static void ensure_view(void) {
  if (view_dirty)
    rebuild_view();
}

/* helpers */
static void redraw(void) { InvalidateRect(hwnd, NULL, FALSE); }
static void set_status(const WCHAR *a, const WCHAR *b, const WCHAR *c) {
  SB s;
  s.n = 0;
  s.b[0] = 0;
  sb_s(&s, a);
  if (b)
    sb_s(&s, b);
  if (c)
    sb_s(&s, c);
  memcpy(status_msg, s.b, (s.n + 1) * sizeof(WCHAR));
  status_until = GetTickCount64() + 4000;
}
static void start_timer(void) {
  u32 ms = speeds[speed];
  if (pSetCoalescableTimer)
    pSetCoalescableTimer(hwnd, 1, ms, NULL, ms / 8);
  else
    SetTimer(hwnd, 1, ms, NULL);
}
static void refresh_now(void) {
  sample();
  redraw();
}

static void make_fonts(void) {
  if (f_ui) {
    DeleteObject(f_ui);
    DeleteObject(f_bold);
    DeleteObject(f_big);
  }
  f_ui = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                     DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  f_bold =
      CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  f_big = CreateFontW(-MulDiv(18, dpi, 72), 0, 0, 0, FW_LIGHT, 0, 0, 0,
                      DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  HDC dc = GetDC(NULL);
  TEXTMETRICW tm;
  HGDIOBJ o = SelectObject(dc, f_ui);
  GetTextMetricsW(dc, &tm);
  fh_ui = tm.tmHeight;
  SelectObject(dc, f_bold);
  GetTextMetricsW(dc, &tm);
  fh_bold = tm.tmHeight;
  SelectObject(dc, f_big);
  GetTextMetricsW(dc, &tm);
  fh_big = tm.tmHeight;
  SIZE sz;
  int x = S(12);
  SelectObject(dc, f_bold);
  tab_x[0] = x;
  GetTextExtentPoint32W(dc, L"Processes", 9, &sz);
  x += sz.cx + S(28);
  tab_x[1] = x;
  GetTextExtentPoint32W(dc, L"Performance", 11, &sz);
  x += sz.cx + S(28);
  tab_x[2] = x;
  SelectObject(dc, o);
  ReleaseDC(NULL, dc);
}
static void layout(void) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  W = rc.right;
  H = rc.bottom;
  pad = S(10);
  tab_h = S(38);
  hdr_h = S(28);
  row_h = S(22);
  status_h = S(26);
  sb_w = S(10);
  hdr_y = tab_h;
  list_y = hdr_y + hdr_h;
  status_y = H - status_h;
  list_h = status_y - list_y;
  if (list_h < 0)
    list_h = 0;
  vis_rows = row_h ? list_h / row_h : 0;
  int fixed = 0;
  for (int c = 1; c < NCOL; c++)
    fixed += S(col_w[c]);
  int nw = W - sb_w - fixed;
  if (nw < S(140))
    nw = S(140);
  col_x[0] = 0;
  col_x[1] = nw;
  for (int c = 1; c < NCOL; c++)
    col_x[c + 1] = col_x[c] + S(col_w[c]);
  clamp_scroll();
}

/* drawing primitives */
static void fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
  RECT r;
  r.left = x;
  r.top = y;
  r.right = x + w;
  r.bottom = y + h;
  SetBkColor(dc, c);
  ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0,
              NULL); /* fastest solid fill in GDI */
}
static void text(HDC dc, int x, int y, const WCHAR *s, int n, COLORREF c,
                 UINT align) {
  SetTextColor(dc, c);
  SetTextAlign(dc, align | TA_TOP);
  ExtTextOutW(dc, x, y, 0, NULL, s, (UINT)n, NULL);
}
static void text_clip(HDC dc, int x, int y, RECT *clip, const WCHAR *s, int n,
                      COLORREF c) {
  SetTextColor(dc, c);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, x, y, ETO_CLIPPED, clip, s, (UINT)n, NULL);
}
static COLORREF mix(COLORREF a, COLORREF b, int t /*0..256*/) {
  int r = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / 256;
  int g = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / 256;
  int bl = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / 256;
  return RGB(r, g, bl);
}
static void thumb(int *ty, int *th) {
  int t = nview ? list_h * vis_rows / (int)nview : list_h;
  if (t < S(24))
    t = S(24);
  if (t > list_h)
    t = list_h;
  int mx = (int)nview - vis_rows;
  *th = t;
  *ty = list_y + (mx > 0 ? (list_h - t) * scroll / mx : 0);
}

/* drawing: chrome */
static void draw_tabs(HDC dc) {
  static const WCHAR *names[2] = {L"Processes", L"Performance"};
  fill(dc, 0, 0, W, tab_h, C_PANEL);
  fill(dc, 0, tab_h - 1, W, 1, C_LINE);
  SelectObject(dc, f_bold);
  for (int i = 0; i < 2; i++) {
    int x = tab_x[i] + S(8);
    text(dc, x, (tab_h - fh_bold) / 2, names[i], lstrlenW(names[i]),
         i == tab ? C_TEXT : C_DIM, TA_LEFT);
    if (i == tab)
      fill(dc, tab_x[i] + S(8), tab_h - S(3), tab_x[i + 1] - tab_x[i] - S(28),
           S(2), C_ACC);
  }
  if (tab == 0) { /* filter box */
    int fw = S(240), fh = S(24), fx = W - S(12) - fw, fy = (tab_h - fh) / 2;
    if (fx < tab_x[2])
      return;
    fill(dc, fx, fy, fw, fh, C_INPUT);
    SelectObject(dc, f_ui);
    RECT clip = {fx + S(8), fy, fx + fw - S(8), fy + fh};
    int ty = fy + (fh - fh_ui) / 2;
    if (nfilter) {
      text_clip(dc, fx + S(8), ty, &clip, filter, nfilter, C_TEXT);
      SIZE sz;
      GetTextExtentPoint32W(dc, filter, nfilter, &sz);
      int cx = fx + S(8) + sz.cx + 1;
      if (cx < clip.right)
        fill(dc, cx, ty, 1, fh_ui, C_ACC);
    } else {
      text_clip(dc, fx + S(8), ty, &clip, L"Type to filter by name or PID", 29,
                C_DIM);
    }
  }
}
static void draw_status(HDC dc) {
  fill(dc, 0, status_y, W, status_h, C_PANEL);
  fill(dc, 0, status_y, W, 1, C_LINE);
  SelectObject(dc, f_ui);
  int ty = status_y + (status_h - fh_ui) / 2;
  SB s;
  s.n = 0;
  s.b[0] = 0;
  sb_u(&s, nproc);
  sb_s(&s, L" processes    CPU ");
  sb_pct(&s, sys_cpu);
  sb_s(&s, L"    Memory ");
  sb_pct(&s, mem_total ? (u32)(mem_used * 10000 / mem_total) : 0);
  sb_s(&s, paused ? L"    Paused" : L"    Refresh ");
  if (!paused) {
    u32 ms = speeds[speed];
    if (ms < 1000) {
      sb_u(&s, ms);
      sb_s(&s, L" ms");
    } else {
      sb_u(&s, ms / 1000);
      sb_s(&s, L" s");
    }
  }
  text(dc, S(12), ty, s.b, s.n, C_DIM, TA_LEFT);
  if (status_msg[0] && GetTickCount64() < status_until)
    text(dc, W - S(12), ty, status_msg, lstrlenW(status_msg), C_ACC, TA_RIGHT);
  else {
    static const WCHAR hint[] = L"Del end task  \x00B7  F5 refresh  \x00B7  "
                                L"Pause  \x00B7  Ctrl+U speed";
    text(dc, W - S(12), ty, hint, (int)(sizeof hint / sizeof *hint) - 1, C_DIM,
         TA_RIGHT);
  }
}

/* drawing: processes */
static void draw_processes(HDC dc) {
  ensure_view();
  fill(dc, 0, hdr_y, W, hdr_h, C_HDR);
  fill(dc, 0, hdr_y + hdr_h - 1, W, 1, C_LINE);
  SelectObject(dc, f_bold);
  int ty = hdr_y + (hdr_h - fh_bold) / 2;
  for (int c = 0; c < NCOL; c++) {
    WCHAR b[32];
    int n = 0;
    const WCHAR *t = col_title[c];
    while (*t)
      b[n++] = *t++;
    if (c == sort_col) {
      b[n++] = L' ';
      b[n++] = sort_desc ? 0x25BE : 0x25B4;
    }
    COLORREF fg = c == sort_col ? C_TEXT : C_DIM;
    if (c == C_NAME)
      text(dc, col_x[0] + pad, ty, b, n, fg, TA_LEFT);
    else
      text(dc, col_x[c + 1] - pad, ty, b, n, fg, TA_RIGHT);
    if (c + 1 < NCOL)
      fill(dc, col_x[c + 1] - 1, hdr_y + S(7), 1, hdr_h - S(14), C_LINE);
  }

  fill(dc, 0, list_y, W, list_h, C_BG);
  SelectObject(dc, f_ui);
  if (!nview) {
    const WCHAR *m = nrows ? L"No matching processes" : L"Loading\x2026";
    text(dc, W / 2, list_y + S(24), m, lstrlenW(m), C_DIM, TA_CENTER);
    return;
  }
  int lw = W - sb_w;
  for (int i = 0; i <= vis_rows; i++) {
    int vi = scroll + i;
    if (vi >= (int)nview)
      break;
    int y = list_y + i * row_h;
    if (y >= status_y)
      break;
    const Row *r = &rows[view[vi]];
    int sel = has_sel && r->pid == sel_pid;
    COLORREF bg = sel ? C_SEL : ((vi & 1) ? C_ALT : C_BG);
    fill(dc, 0, y, lw, row_h, bg);
    if (!sel) { /* heat shading, like Task Manager */
      int t = (int)(r->cpu * 256 / 2500);
      if (t > 256)
        t = 256;
      if (t > 6)
        fill(dc, col_x[C_CPU], y, col_x[C_CPU + 1] - col_x[C_CPU], row_h,
             mix(bg, C_HEAT, t * 120 / 256));
      if (mem_total) {
        u64 m = r->mem * 256 * 8 / mem_total;
        int tm = m > 256 ? 256 : (int)m;
        if (tm > 6)
          fill(dc, col_x[C_MEM], y, col_x[C_MEM + 1] - col_x[C_MEM], row_h,
               mix(bg, C_HEAT, tm * 120 / 256));
      }
    }
    int yy = y + (row_h - fh_ui) / 2;
    RECT clip = {col_x[0] + pad, y, col_x[1] - pad, y + row_h};
    text_clip(dc, col_x[0] + pad, yy, &clip, r->name, (int)r->nlen, C_TEXT);
    SB s;
    s.n = 0;
    sb_u(&s, r->pid);
    text(dc, col_x[C_PID + 1] - pad, yy, s.b, s.n, C_DIM, TA_RIGHT);
    s.n = 0;
    sb_pct(&s, r->cpu);
    text(dc, col_x[C_CPU + 1] - pad, yy, s.b, s.n, C_TEXT, TA_RIGHT);
    s.n = 0;
    sb_mb(&s, r->mem);
    text(dc, col_x[C_MEM + 1] - pad, yy, s.b, s.n, C_TEXT, TA_RIGHT);
    s.n = 0;
    sb_rate(&s, r->io);
    text(dc, col_x[C_IO + 1] - pad, yy, s.b, s.n, r->io ? C_TEXT : C_DIM,
         TA_RIGHT);
    s.n = 0;
    sb_u(&s, r->threads);
    text(dc, col_x[C_THR + 1] - pad, yy, s.b, s.n, C_DIM, TA_RIGHT);
    s.n = 0;
    sb_u(&s, r->handles);
    text(dc, col_x[C_HND + 1] - pad, yy, s.b, s.n, C_DIM, TA_RIGHT);
  }
  if ((int)nview > vis_rows && vis_rows > 0) {
    int tty, th;
    thumb(&tty, &th);
    fill(dc, W - sb_w + S(3), tty + S(2), sb_w - S(6), th - S(4),
         dragging ? C_DIM : C_LINE2);
  }
}

/* drawing: performance */
static void graph(HDC dc, int x, int y, int w, int h, const u16 *hist,
                  COLORREF line, COLORREF area) {
  fill(dc, x, y, w, h, C_INPUT);
  for (int g = 1; g < 4; g++)
    fill(dc, x, y + h * g / 4, w, 1, C_GRID);
  for (int g = 1; g < 10; g++)
    fill(dc, x + w * g / 10, y, 1, h, C_GRID);
  int n = hist_count;
  if (n >= 2) {
    POINT pts[HIST_N + 2];
    for (int i = 0; i < n; i++) {
      int idx = (hist_pos - n + i + HIST_N) % HIST_N;
      pts[i].x = x + (w - 1) - (n - 1 - i) * (w - 1) / (HIST_N - 1);
      pts[i].y = y + (h - 1) - hist[idx] * (h - 2) / 10000;
    }
    pts[n].x = pts[n - 1].x;
    pts[n].y = y + h;
    pts[n + 1].x = pts[0].x;
    pts[n + 1].y = y + h;
    SelectObject(dc, GetStockObject(NULL_PEN));
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, area);
    Polygon(dc, pts, n + 2);
    SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, line);
    Polyline(dc, pts, n);
  }
  fill(dc, x, y, w, 1, C_LINE);
  fill(dc, x, y + h - 1, w, 1, C_LINE);
  fill(dc, x, y, 1, h, C_LINE);
  fill(dc, x + w - 1, y, 1, h, C_LINE);
}
static void stat_line(HDC dc, int x, int w, int *y, const WCHAR *label, SB *v) {
  text(dc, x, *y, label, lstrlenW(label), C_DIM, TA_LEFT);
  text(dc, x + w, *y, v->b, v->n, C_TEXT, TA_RIGHT);
  *y += S(22);
}
static void draw_performance(HDC dc) {
  int P = S(16), x0 = P, y0 = tab_h + P, rw = S(230);
  int gw = W - P * 3 - rw;
  if (gw < S(200))
    gw = S(200);
  int avail = status_y - P - y0, ph = (avail - P) / 2;
  int title_h = fh_big + S(6), core_h = S(34);
  fill(dc, 0, tab_h, W, status_y - tab_h, C_BG);
  if (ph < S(80))
    return;

  SB s;
  /* CPU panel */
  SelectObject(dc, f_bold);
  text(dc, x0, y0 + (fh_big - fh_bold) / 2, L"CPU", 3, C_TEXT, TA_LEFT);
  SelectObject(dc, f_big);
  s.n = 0;
  sb_pct(&s, sys_cpu);
  text(dc, x0 + gw, y0, s.b, s.n, C_ACC, TA_RIGHT);
  int gh = ph - title_h - core_h - S(6);
  graph(dc, x0, y0 + title_h, gw, gh, cpu_hist, C_ACC, C_ACC_F);
  int cy = y0 + title_h + gh + S(6);
  int gap = gw / (ncpu ? ncpu : 1) > S(6) ? S(2) : 0;
  for (int i = 0; i < ncpu; i++) {
    int bx = x0 + i * gw / ncpu, bx2 = x0 + (i + 1) * gw / ncpu - gap;
    if (bx2 <= bx)
      bx2 = bx + 1;
    int bh2 = core_pct[i] * core_h / 10000;
    fill(dc, bx, cy, bx2 - bx, core_h - bh2, C_INPUT);
    fill(dc, bx, cy + core_h - bh2, bx2 - bx, bh2, C_ACC);
  }

  /* Memory panel */
  int y1 = y0 + ph + P;
  SelectObject(dc, f_bold);
  text(dc, x0, y1 + (fh_big - fh_bold) / 2, L"Memory", 6, C_TEXT, TA_LEFT);
  SelectObject(dc, f_big);
  s.n = 0;
  sb_gb(&s, mem_used);
  sb_s(&s, L" / ");
  sb_gb(&s, mem_total);
  text(dc, x0 + gw, y1, s.b, s.n, C_MEMC, TA_RIGHT);
  graph(dc, x0, y1 + title_h, gw, ph - title_h, mem_hist, C_MEMC, C_MEM_F);

  /* stats column */
  int sx = x0 + gw + P, sy = y0 + title_h;
  SelectObject(dc, f_ui);
  s.n = 0;
  sb_pct(&s, sys_cpu);
  stat_line(dc, sx, rw, &sy, L"Utilization", &s);
  s.n = 0;
  sb_u(&s, nproc);
  stat_line(dc, sx, rw, &sy, L"Processes", &s);
  s.n = 0;
  sb_u(&s, tot_threads);
  stat_line(dc, sx, rw, &sy, L"Threads", &s);
  s.n = 0;
  sb_u(&s, tot_handles);
  stat_line(dc, sx, rw, &sy, L"Handles", &s);
  s.n = 0;
  sb_u(&s, (u64)ncpu);
  stat_line(dc, sx, rw, &sy, L"Logical processors", &s);
  u64 up = GetTickCount64() / 1000;
  s.n = 0;
  sb_u(&s, up / 86400);
  sb_s(&s, L":");
  sb_u2(&s, up / 3600 % 24);
  sb_s(&s, L":");
  sb_u2(&s, up / 60 % 60);
  sb_s(&s, L":");
  sb_u2(&s, up % 60);
  stat_line(dc, sx, rw, &sy, L"Up time", &s);

  sy = y1 + title_h;
  s.n = 0;
  sb_gb(&s, mem_used);
  stat_line(dc, sx, rw, &sy, L"In use", &s);
  s.n = 0;
  sb_gb(&s, mem_total - mem_used);
  stat_line(dc, sx, rw, &sy, L"Available", &s);
  s.n = 0;
  sb_gb(&s, commit_used);
  sb_s(&s, L" / ");
  sb_gb(&s, commit_limit);
  stat_line(dc, sx, rw, &sy, L"Committed", &s);
  s.n = 0;
  sb_pct(&s, mem_total ? (u32)(mem_used * 10000 / mem_total) : 0);
  stat_line(dc, sx, rw, &sy, L"Load", &s);
}

static void paint(void) {
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(hwnd, &ps);
  if (W > 0 && H > 0) {
    if (W > bw ||
        H > bh) { /* back buffer only grows: no realloc churn while resizing */
      if (!mdc)
        mdc = CreateCompatibleDC(dc);
      if (mbm) {
        SelectObject(mdc, mbm_old);
        DeleteObject(mbm);
      }
      bw = (W + 255) & ~255;
      bh = (H + 255) & ~255;
      mbm = CreateCompatibleBitmap(dc, bw, bh);
      mbm_old = (HBITMAP)SelectObject(mdc, mbm);
      SetBkMode(mdc, TRANSPARENT);
    }
    draw_tabs(mdc);
    if (tab == 0)
      draw_processes(mdc);
    else
      draw_performance(mdc);
    draw_status(mdc);
    BitBlt(dc, 0, 0, W, H, mdc, 0, 0, SRCCOPY);
  }
  EndPaint(hwnd, &ps);
}

/* actions */
static void select_index(int i) {
  ensure_view();
  if (!nview)
    return;
  if (i < 0)
    i = 0;
  if (i >= (int)nview)
    i = (int)nview - 1;
  sel_idx = i;
  sel_pid = rows[view[i]].pid;
  has_sel = 1;
  if (i < scroll)
    scroll = i;
  else if (i >= scroll + vis_rows)
    scroll = i - vis_rows + 1;
  clamp_scroll();
  redraw();
}
static int selected_row(u32 *pid, WCHAR *name) {
  ensure_view();
  if (sel_idx < 0)
    return 0;
  const Row *r = &rows[view[sel_idx]];
  *pid = r->pid;
  memcpy(name, r->name, (r->nlen + 1) * sizeof(WCHAR));
  return 1;
}
static void end_task(u32 pid, const WCHAR *name) {
  HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  BOOL ok = h && TerminateProcess(h, 1);
  if (h)
    CloseHandle(h);
  if (ok)
    set_status(L"Ended ", name, NULL);
  else
    set_status(L"Could not end ", name,
               L" (access denied \x2014 try running as admin)");
  SetTimer(hwnd, 2, 150, NULL); /* one-shot resample once the process is gone */
  redraw();
}
static void open_location(u32 pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) {
    set_status(L"Cannot open process (access denied)", NULL, NULL);
    redraw();
    return;
  }
  WCHAR args[MAX_PATH + 16] = L"/select,\"";
  DWORD n = MAX_PATH;
  QueryImageName_t q = (QueryImageName_t)(void *)GetProcAddress(
      GetModuleHandleW(L"kernel32.dll"), "QueryFullProcessImageNameW");
  BOOL ok = q && q(h, 0, args + 9, &n);
  CloseHandle(h);
  if (!ok) {
    set_status(L"Path unavailable for this process", NULL, NULL);
    redraw();
    return;
  }
  args[9 + n] = L'"';
  args[10 + n] = 0;
  HMODULE sh =
      LoadLibraryW(L"shell32.dll"); /* lazy: never loaded unless used */
  ShellExecW_t se =
      sh ? (ShellExecW_t)(void *)GetProcAddress(sh, "ShellExecuteW") : NULL;
  if (se)
    se(hwnd, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
}
static void copy_text(const WCHAR *s) {
  int n = lstrlenW(s);
  if (!OpenClipboard(hwnd))
    return;
  EmptyClipboard();
  HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(WCHAR));
  if (g) {
    memcpy(GlobalLock(g), s, (n + 1) * sizeof(WCHAR));
    GlobalUnlock(g);
    SetClipboardData(CF_UNICODETEXT, g);
  }
  CloseClipboard();
}
static void context_menu(int sx, int sy) {
  u32 pid;
  WCHAR name[64];
  if (!selected_row(&pid, name))
    return; /* copy now: timers keep firing inside the menu loop */
  HMENU m = CreatePopupMenu();
  AppendMenuW(m, MF_STRING, 1, L"End task\tDel");
  AppendMenuW(m, MF_STRING, 2, L"Open file location");
  AppendMenuW(m, MF_SEPARATOR, 0, NULL);
  AppendMenuW(m, MF_STRING, 3, L"Copy name");
  AppendMenuW(m, MF_STRING, 4, L"Copy PID");
  int cmd =
      TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, sx, sy, 0, hwnd, NULL);
  DestroyMenu(m);
  if (cmd == 1)
    end_task(pid, name);
  else if (cmd == 2)
    open_location(pid);
  else if (cmd == 3)
    copy_text(name);
  else if (cmd == 4) {
    SB s;
    s.n = 0;
    sb_u(&s, pid);
    copy_text(s.b);
  }
}
static void set_tab(int t) {
  if (tab != t) {
    tab = t;
    view_dirty = 1;
    redraw();
  }
}
static void set_paused(int p) {
  paused = p;
  if (paused)
    KillTimer(hwnd, 1);
  else {
    refresh_now();
    start_timer();
  }
  redraw();
}
static int row_at(int y) {
  int i = scroll + (y - list_y) / row_h;
  return (y >= list_y && i < (int)nview) ? i : -1;
}

/* window proc */
static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE:
    hwnd = h;
    if (pGetDpiForWindow)
      dpi = (int)pGetDpiForWindow(h);
    make_fonts();
    return 0;
  case WM_SIZE:
    if (wp == SIZE_MINIMIZED) {
      minimized = 1;
      KillTimer(h, 1);
      SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1,
                               (SIZE_T)-1); /* give RAM back */
    } else {
      if (minimized) {
        minimized = 0;
        if (!paused) {
          sample();
          start_timer();
        }
      }
      layout();
      redraw();
    }
    return 0;
  case WM_DPICHANGED: {
    dpi = HIWORD(wp);
    make_fonts();
    RECT *r = (RECT *)lp;
    SetWindowPos(h, NULL, r->left, r->top, r->right - r->left,
                 r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    layout();
    redraw();
    return 0;
  }
  case WM_GETMINMAXINFO:
    ((MINMAXINFO *)lp)->ptMinTrackSize.x = S(640);
    ((MINMAXINFO *)lp)->ptMinTrackSize.y = S(400);
    return 0;
  case WM_TIMER:
    if (wp == 2)
      KillTimer(h, 2);
    else if (paused || minimized)
      return 0;
    refresh_now();
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
    paint();
    return 0;

  case WM_KEYDOWN: {
    int ctrl = GetKeyState(VK_CONTROL) < 0;
    switch (wp) {
    case VK_TAB:
      if (ctrl)
        set_tab(tab ^ 1);
      break;
    case '1':
      if (ctrl)
        set_tab(0);
      break;
    case '2':
      if (ctrl)
        set_tab(1);
      break;
    case 'U':
      if (ctrl) {
        speed = (speed + 1) % 5;
        if (!paused)
          start_timer();
        redraw();
      }
      break;
    case VK_F5:
      refresh_now();
      break;
    case VK_PAUSE:
      set_paused(!paused);
      break;
    case VK_ESCAPE:
      if (nfilter) {
        nfilter = 0;
        filter[0] = 0;
        view_dirty = 1;
        redraw();
      }
      break;
    case VK_DELETE:
      if (tab == 0) {
        u32 pid;
        WCHAR nm[64];
        if (selected_row(&pid, nm))
          end_task(pid, nm);
      }
      break;
    case VK_UP:
      if (tab == 0) {
        ensure_view();
        select_index(sel_idx < 0 ? 0 : sel_idx - 1);
      }
      break;
    case VK_DOWN:
      if (tab == 0) {
        ensure_view();
        select_index(sel_idx + 1);
      }
      break;
    case VK_PRIOR:
      if (tab == 0) {
        ensure_view();
        select_index((sel_idx < 0 ? 0 : sel_idx) -
                     (vis_rows > 1 ? vis_rows - 1 : 1));
      }
      break;
    case VK_NEXT:
      if (tab == 0) {
        ensure_view();
        select_index(sel_idx + (vis_rows > 1 ? vis_rows - 1 : 1));
      }
      break;
    case VK_HOME:
      if (tab == 0)
        select_index(0);
      break;
    case VK_END:
      if (tab == 0)
        select_index(0x7FFFFFFF);
      break;
    case VK_APPS:
      if (tab == 0 && sel_idx >= 0) {
        POINT pt = {S(40), list_y + (sel_idx - scroll + 1) * row_h};
        ClientToScreen(h, &pt);
        context_menu(pt.x, pt.y);
      }
      break;
    }
    return 0;
  }
  case WM_CHAR:
    if (tab != 0)
      return 0;
    if (wp == 8) {
      if (nfilter)
        nfilter--;
    } else if (wp == 127) {
      nfilter = 0;
    } /* Ctrl+Backspace */
    else if (wp >= 32 && nfilter < 63)
      filter[nfilter++] = (WCHAR)wp;
    else
      return 0;
    filter[nfilter] = 0;
    view_dirty = 1;
    scroll = 0;
    redraw();
    return 0;

  case WM_LBUTTONDOWN: {
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    SetFocus(h);
    if (y < tab_h) {
      if (x >= tab_x[0] && x < tab_x[1])
        set_tab(0);
      else if (x >= tab_x[1] && x < tab_x[2])
        set_tab(1);
      return 0;
    }
    if (tab != 0 || y >= status_y)
      return 0;
    ensure_view();
    if (y < list_y) { /* header: sort */
      int c = 0;
      while (c < NCOL - 1 && x >= col_x[c + 1])
        c++;
      if (c == sort_col)
        sort_desc ^= 1;
      else {
        sort_col = c;
        sort_desc = (c != C_NAME && c != C_PID);
      }
      view_dirty = 1;
      redraw();
      return 0;
    }
    if (x >= W - sb_w && (int)nview > vis_rows) { /* scrollbar */
      int ty, th;
      thumb(&ty, &th);
      drag_off = (y >= ty && y < ty + th) ? y - ty : th / 2;
      dragging = 1;
      SetCapture(h);
    }
    int i = row_at(y);
    if (!dragging && i >= 0)
      select_index(i);
    if (!dragging)
      return 0;
  }
  /* fallthrough */
  case WM_MOUSEMOVE:
    if (dragging) {
      int ty, th;
      thumb(&ty, &th);
      int mx = (int)nview - vis_rows, span = list_h - th;
      int pos = (short)HIWORD(lp) - drag_off - list_y;
      int ns = span > 0 ? (pos * mx + span / 2) / span : 0;
      if (ns != scroll) {
        scroll = ns;
        clamp_scroll();
        redraw();
      }
    }
    return 0;
  case WM_LBUTTONUP:
    if (dragging) {
      dragging = 0;
      ReleaseCapture();
      redraw();
    }
    return 0;
  case WM_MOUSEWHEEL: {
    static int acc;
    if (tab != 0)
      return 0;
    acc += GET_WHEEL_DELTA_WPARAM(wp);
    int lines = acc / WHEEL_DELTA * 3;
    acc %= WHEEL_DELTA;
    if (lines) {
      int old = scroll;
      scroll -= lines;
      clamp_scroll();
      if (scroll != old)
        redraw();
    }
    return 0;
  }
  case WM_CONTEXTMENU: {
    if (tab != 0)
      return 0;
    int sx = (short)LOWORD(lp), sy = (short)HIWORD(lp);
    if (sx == -1 && sy == -1)
      return 0;
    POINT pt = {sx, sy};
    ScreenToClient(h, &pt);
    if (pt.y < list_y || pt.y >= status_y)
      return 0;
    ensure_view();
    int i = row_at(pt.y);
    if (i < 0)
      return 0;
    select_index(i);
    UpdateWindow(h);
    context_menu(sx, sy);
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h, msg, wp, lp);
}

/* entry */
int WINAPI WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmd, int show) {
  (void)prev;
  (void)cmd;
  HMODULE user = GetModuleHandleW(L"user32.dll"),
          k32 = GetModuleHandleW(L"kernel32.dll");
  SetDpiCtx_t sdc = (SetDpiCtx_t)(void *)GetProcAddress(
      user, "SetProcessDpiAwarenessContext");
  if (sdc)
    sdc((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */);
  else
    SetProcessDPIAware();
  pGetDpiForWindow =
      (GetDpiForWindow_t)(void *)GetProcAddress(user, "GetDpiForWindow");
  pSetCoalescableTimer =
      (SetCoalTimer_t)(void *)GetProcAddress(user, "SetCoalescableTimer");
  NtQSI = (NtQSI_t)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                          "NtQuerySystemInformation");
  if (!NtQSI)
    return 1;

  /* EcoQoS: ask Windows to run us on efficiency cores at low clocks (Win11;
   * harmless elsewhere). */
  SetProcInfo_t spi =
      (SetProcInfo_t)(void *)GetProcAddress(k32, "SetProcessInformation");
  if (spi) {
    struct {
      ULONG Version, ControlMask, StateMask;
    } pt = {1, 0x1 | 0x4,
            0x1 | 0x4}; /* EXECUTION_SPEED | IGNORE_TIMER_RESOLUTION */
    if (!spi(GetCurrentProcess(), 4 /*ProcessPowerThrottling*/, &pt,
             sizeof pt)) {
      pt.ControlMask = pt.StateMask = 0x1;
      spi(GetCurrentProcess(), 4, &pt, sizeof pt);
    }
  }

  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  qpf = (u64)f.QuadPart;
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  ncpu = (int)si.dwNumberOfProcessors;
  if (ncpu > MAX_CPU)
    ncpu = MAX_CPU;
  if (ncpu < 1)
    ncpu = 1;

  WNDCLASSEXW wc;
  memset(&wc, 0, sizeof wc);
  wc.cbSize = sizeof wc;
  wc.lpfnWndProc = wndproc;
  wc.hInstance = hi;
  wc.lpszClassName = L"tm_fast";
  wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
  wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
  RegisterClassExW(&wc);

  HWND h = CreateWindowExW(0, wc.lpszClassName, L"Task Manager",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           10, 10, NULL, NULL, hi, NULL);
  if (!h)
    return 1;
  RECT r = {0, 0, S(920), S(640)};
  AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
  SetWindowPos(h, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
               SWP_NOMOVE | SWP_NOZORDER);

  HMODULE dwm = LoadLibraryW(L"dwmapi.dll"); /* dark title bar, best effort */
  if (dwm) {
    DwmSetAttr_t d =
        (DwmSetAttr_t)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    BOOL on = TRUE;
    if (d)
      d(h, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &on, sizeof on);
  }

  sample(); /* baseline for deltas */
  ShowWindow(h, show);
  UpdateWindow(h);
  start_timer();
  SetTimer(h, 2, 250, NULL); /* quick first real CPU reading */

  MSG m;
  while (GetMessageW(&m, NULL, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}
