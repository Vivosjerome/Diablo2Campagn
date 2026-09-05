#include "memory.h"
#include "app.h"
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#ifndef TH32CS_SNAPPROCESS
#define TH32CS_SNAPPROCESS 0x00000002
#define TH32CS_SNAPMODULE 0x00000008
#define TH32CS_SNAPMODULE32 0x00000010

typedef struct tagPROCESSENTRY32 {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG pcPriClassBase;
    DWORD dwFlags;
    CHAR szExeFile[MAX_PATH];
} PROCESSENTRY32;

typedef struct tagMODULEENTRY32 {
    DWORD dwSize;
    DWORD th32ModuleID;
    DWORD th32ProcessID;
    DWORD GlblcntUsage;
    DWORD ProccntUsage;
    BYTE *modBaseAddr;
    DWORD modBaseSize;
    HMODULE hModule;
    char szModule[256];
    char szExePath[MAX_PATH];
} MODULEENTRY32;

typedef HANDLE (__stdcall *PFN_CreateToolhelp32Snapshot)(DWORD, DWORD);
typedef BOOL (__stdcall *PFN_Process32First)(HANDLE, PROCESSENTRY32 *);
typedef BOOL (__stdcall *PFN_Process32Next)(HANDLE, PROCESSENTRY32 *);
typedef BOOL (__stdcall *PFN_Module32First)(HANDLE, MODULEENTRY32 *);
typedef BOOL (__stdcall *PFN_Module32Next)(HANDLE, MODULEENTRY32 *);
#endif

typedef LONG NTSTATUS;
#define NT_SUCCESS(s) ((NTSTATUS)(s) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define SystemExtendedHandleInformation 64

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    PVOID Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX;

typedef NTSTATUS (NTAPI *PFN_NtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS (NTAPI *PFN_NtDuplicateObject)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS (NTAPI *PFN_NtReadVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef DWORD (WINAPI *PFN_GetProcessId)(HANDLE);

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

#ifndef TokenElevation
#define TokenElevation 20
typedef struct _TOKEN_ELEVATION {
    DWORD TokenIsElevated;
} TOKEN_ELEVATION;
#endif

static HMODULE g_kernel;
static HMODULE g_ntdll;
static PFN_CreateToolhelp32Snapshot pCreateToolhelp32Snapshot;
static PFN_Process32First pProcess32First;
static PFN_Process32Next pProcess32Next;
static PFN_Module32First pModule32First;
static PFN_Module32Next pModule32Next;
static PFN_NtQuerySystemInformation pNtQuerySystemInformation;
static PFN_NtDuplicateObject pNtDuplicateObject;
static PFN_NtReadVirtualMemory pNtReadVirtualMemory;
static PFN_GetProcessId pGetProcessId;

#define D2_ACCESS (PROCESS_VM_READ | PROCESS_QUERY_INFORMATION)

static DWORD find_pid(const char *name);

static int load_toolhelp(void) {
    if (pCreateToolhelp32Snapshot) return 1;
    g_kernel = GetModuleHandleA("kernel32.dll");
    if (!g_kernel) return 0;
    pCreateToolhelp32Snapshot = (PFN_CreateToolhelp32Snapshot)GetProcAddress(g_kernel, "CreateToolhelp32Snapshot");
    pProcess32First = (PFN_Process32First)GetProcAddress(g_kernel, "Process32First");
    pProcess32Next = (PFN_Process32Next)GetProcAddress(g_kernel, "Process32Next");
    pModule32First = (PFN_Module32First)GetProcAddress(g_kernel, "Module32First");
    pModule32Next = (PFN_Module32Next)GetProcAddress(g_kernel, "Module32Next");
    pGetProcessId = (PFN_GetProcessId)GetProcAddress(g_kernel, "GetProcessId");
    return pCreateToolhelp32Snapshot && pProcess32First && pProcess32Next && pModule32First && pModule32Next;
}

static int load_ntdll(void) {
    if (pNtReadVirtualMemory) return 1;
    g_ntdll = GetModuleHandleA("ntdll.dll");
    if (!g_ntdll) return 0;
    pNtQuerySystemInformation = (PFN_NtQuerySystemInformation)GetProcAddress(g_ntdll, "NtQuerySystemInformation");
    pNtDuplicateObject = (PFN_NtDuplicateObject)GetProcAddress(g_ntdll, "NtDuplicateObject");
    pNtReadVirtualMemory = (PFN_NtReadVirtualMemory)GetProcAddress(g_ntdll, "NtReadVirtualMemory");
    return pNtQuerySystemInformation && pNtDuplicateObject && pNtReadVirtualMemory;
}

static void enable_debug_privilege(void) {
    HANDLE token = NULL;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;
    if (!LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        CloseHandle(token);
        return;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(token);
}

int is_running_elevated(void) {
    HANDLE token = NULL;
    TOKEN_ELEVATION elev;
    DWORD n = 0;
    int elevated = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return 0;
    if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &n))
        elevated = elev.TokenIsElevated ? 1 : 0;
    CloseHandle(token);
    return elevated;
}

static int pid_in_list(DWORD pid, const DWORD *list, int n) {
    int i;
    for (i = 0; i < n; i++)
        if (list[i] == pid) return 1;
    return 0;
}

static int collect_donor_pids(DWORD self, DWORD target, DWORD *out, int max_out) {
    static const char *names[] = {
        "Battle.net.exe", "Agent.exe", "Steam.exe", "steam.exe",
        "steamwebhelper.exe", "explorer.exe", NULL
    };
    HANDLE snap;
    PROCESSENTRY32 pe;
    int n = 0, i;
    DWORD parent = 0;

    if (!load_toolhelp()) return 0;
    snap = pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (pProcess32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == target)
                parent = pe.th32ParentProcessID;
            if (pe.th32ProcessID == self || pe.th32ProcessID == target)
                continue;
            for (i = 0; names[i]; i++) {
                if (_stricmp(pe.szExeFile, names[i]) == 0) {
                    if (!pid_in_list(pe.th32ProcessID, out, n) && n < max_out)
                        out[n++] = pe.th32ProcessID;
                    break;
                }
            }
        } while (pProcess32Next(snap, &pe));
    }
    CloseHandle(snap);
    if (parent && parent != self && parent != target && !pid_in_list(parent, out, n) && n < max_out)
        out[n++] = parent;
    return n;
}

static int handle_is_process_type(USHORT type_index) {
    /* Win10/11: Process=7. Accept 5-8 to tolerate OS variance. */
    return type_index >= 5 && type_index <= 8;
}

static HANDLE try_duplicate_from_owner(DWORD owner_pid, ULONG_PTR handle_value, DWORD target_pid) {
    HANDLE owner = NULL, dup = NULL;

    owner = OpenProcess(PROCESS_DUP_HANDLE, FALSE, owner_pid);
    if (!owner) return NULL;

    /* Same rights as source handle. */
    if (NT_SUCCESS(pNtDuplicateObject(owner, (HANDLE)handle_value, GetCurrentProcess(),
                                      &dup, 0, 0, 2))) {
        if (pGetProcessId(dup) == target_pid) {
            CloseHandle(owner);
            return dup;
        }
        CloseHandle(dup);
        dup = NULL;
    }

    if (NT_SUCCESS(pNtDuplicateObject(owner, (HANDLE)handle_value, GetCurrentProcess(),
                                      &dup, D2_ACCESS, 0, 0))) {
        if (pGetProcessId(dup) == target_pid) {
            CloseHandle(owner);
            return dup;
        }
        CloseHandle(dup);
    }

    CloseHandle(owner);
    return NULL;
}

static SYSTEM_HANDLE_INFORMATION_EX *query_all_handles(void) {
    SYSTEM_HANDLE_INFORMATION_EX *info = NULL;
    ULONG buf_len = 0x100000;
    ULONG ret_len = 0;
    NTSTATUS st;

    for (;;) {
        info = (SYSTEM_HANDLE_INFORMATION_EX *)VirtualAlloc(NULL, buf_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!info) return NULL;
        st = pNtQuerySystemInformation(SystemExtendedHandleInformation, info, buf_len, &ret_len);
        if (NT_SUCCESS(st)) return info;
        VirtualFree(info, 0, MEM_RELEASE);
        if (st != STATUS_INFO_LENGTH_MISMATCH) return NULL;
        buf_len += 0x100000;
        if (buf_len > 0x8000000) return NULL;
    }
}

static HANDLE scan_handles_for_target(SYSTEM_HANDLE_INFORMATION_EX *info, DWORD target_pid,
                                      DWORD self, const DWORD *donors, int n_donors,
                                      int donors_only, int filter_type, DWORD *donor_used) {
    ULONG_PTR i;

    for (i = 0; i < info->NumberOfHandles; i++) {
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX *e = &info->Handles[i];
        DWORD owner_pid = (DWORD)e->UniqueProcessId;
        HANDLE dup;

        if (!owner_pid || owner_pid == self || owner_pid == target_pid)
            continue;
        if (filter_type && !handle_is_process_type(e->ObjectTypeIndex))
            continue;
        if (donors_only && n_donors > 0 && !pid_in_list(owner_pid, donors, n_donors))
            continue;

        dup = try_duplicate_from_owner(owner_pid, e->HandleValue, target_pid);
        if (dup) {
            if (donor_used) *donor_used = owner_pid;
            return dup;
        }
    }
    return NULL;
}

/* Pas d'OpenProcess(D2R) : on reprend un handle deja ouvert (Battle.net / parent). */
static HANDLE hijack_process_handle(DWORD target_pid, DWORD *donor_pid) {
    SYSTEM_HANDLE_INFORMATION_EX *info = NULL;
    DWORD self = GetCurrentProcessId();
    DWORD donors[64];
    int n_donors;
    int elevated = is_running_elevated();
    HANDLE found = NULL;

    if (donor_pid) *donor_pid = 0;
    if (!load_ntdll() || !load_toolhelp() || !target_pid || !pGetProcessId)
        return NULL;

    enable_debug_privilege();
    n_donors = collect_donor_pids(self, target_pid, donors, 64);

    info = query_all_handles();
    if (!info) return NULL;

    /* Battle.net / Agent / parent / explorer — tous leurs process, type Process puis sans filtre. */
    found = scan_handles_for_target(info, target_pid, self, donors, n_donors, 1, 1, donor_pid);
    if (!found)
        found = scan_handles_for_target(info, target_pid, self, donors, n_donors, 1, 0, donor_pid);

    /* Scan systeme (svchost, etc.) — Admin + SeDebugPrivilege. */
    if (!found && elevated) {
        found = scan_handles_for_target(info, target_pid, self, donors, n_donors, 0, 1, donor_pid);
        if (!found)
            found = scan_handles_for_target(info, target_pid, self, donors, n_donors, 0, 0, donor_pid);
    }

    VirtualFree(info, 0, MEM_RELEASE);
    return found;
}

bool d2_is_ptr(uint64_t a) {
    return a > 0x10000ULL && a < 0x00007FFFFFFFFFFFULL;
}

bool d2_require_admin(void) {
    SHELLEXECUTEINFOA sei;
    char path[MAX_PATH];

    if (is_running_elevated()) return true;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExA(&sei))
        return false;

    app_error("Lance en Administrateur :\nclic droit sur l'exe -> Executer en tant qu'administrateur");
    return false;
}

bool d2_is_foreground(DWORD pid) {
    HWND fg;
    DWORD fg_pid = 0;
    if (!pid) return false;
    fg = GetForegroundWindow();
    if (!fg) return false;
    GetWindowThreadProcessId(fg, &fg_pid);
    return fg_pid == pid;
}

static DWORD find_pid(const char *name) {
    HANDLE snap;
    PROCESSENTRY32 pe;
    DWORD pid = 0;
    if (!load_toolhelp()) return 0;
    snap = pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (pProcess32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (pProcess32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static uintptr_t module_base(DWORD pid, const char *modname) {
    HANDLE snap;
    MODULEENTRY32 me;
    uintptr_t base = 0;
    if (!load_toolhelp()) return 0;
    snap = pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    me.dwSize = sizeof(me);
    if (pModule32First(snap, &me)) {
        do {
            if (_stricmp(me.szModule, modname) == 0) {
                base = (uintptr_t)me.modBaseAddr;
                break;
            }
        } while (pModule32Next(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

bool d2_attach(D2Process *out) {
    DWORD donor_pid = 0;
    int attempt;

    memset(out, 0, sizeof(*out));
    load_ntdll();
    enable_debug_privilege();

    for (attempt = 0; attempt < 10; attempt++) {
        out->pid = find_pid("D2R.exe");
        if (!out->pid) {
            if (attempt == 9) {
                app_error("D2R.exe introuvable. Lance le jeu d'abord.");
                return false;
            }
            Sleep(500);
            continue;
        }

        out->process = hijack_process_handle(out->pid, &donor_pid);
        if (!out->process) {
            if (attempt == 9) {
                app_error("Connexion memoire indisponible.\nLance D2R via Battle.net, puis relance CampagneD2 en Admin.");
                return false;
            }
            Sleep(500);
            continue;
        }

        out->module_base = module_base(out->pid, "D2R.exe");
        if (!out->module_base) {
            CloseHandle(out->process);
            out->process = NULL;
            if (attempt == 9) {
                app_error("Base D2R.exe introuvable.");
                return false;
            }
            Sleep(500);
            continue;
        }

        (void)donor_pid;
        return true;
    }
    return false;
}

void d2_detach(D2Process *p) {
    if (p && p->process) {
        CloseHandle(p->process);
        p->process = NULL;
    }
}

bool d2_read(const D2Process *p, uintptr_t addr, void *buf, size_t len) {
    SIZE_T got = 0;
    if (!p || !p->process || !addr || !buf || !len) return false;

    if (load_ntdll() && pNtReadVirtualMemory) {
        if (NT_SUCCESS(pNtReadVirtualMemory(p->process, (PVOID)addr, buf, len, &got)) && got == len)
            return true;
    }

    return ReadProcessMemory(p->process, (LPCVOID)addr, buf, len, &got) && got == len;
}

bool d2_read_unit_table(const D2Process *p, uintptr_t table_addr, uint64_t slots[D2_UNIT_TABLE_SLOTS]) {
    return d2_read(p, table_addr, slots, D2_UNIT_TABLE_SLOTS * sizeof(uint64_t));
}
