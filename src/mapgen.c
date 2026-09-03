#include "mapgen.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_exists(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void dirname_of(char *path) {
    char *slash = strrchr(path, '\\');
    if (slash) *slash = 0;
}

bool mapgen_ensure(uint32_t seed, uint32_t difficulty, char *out_json_path, size_t path_len,
                   int allow_mapgen) {
    char exe_dir[MAX_PATH];
    char project[MAX_PATH];
    char cache_dir[MAX_PATH];
    char cache_file[MAX_PATH];
    char mapgen[MAX_PATH];
    char cmd[2048];
    char tmp_raw[MAX_PATH];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    FILE *in, *out;
    char *line;
    size_t linecap = 1 << 20;
    int first = 1;

    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    dirname_of(exe_dir);
    /* LoD install = parent directory (d2-mapgen /C). */
    snprintf(project, sizeof(project), "%s\\..", exe_dir);

    snprintf(cache_dir, sizeof(cache_dir), "%s\\cache", exe_dir);
    CreateDirectoryA(cache_dir, NULL);
    snprintf(cache_file, sizeof(cache_file), "%s\\%u_%u.json", cache_dir, seed, difficulty);
    snprintf(out_json_path, path_len, "%s", cache_file);

    if (file_exists(cache_file)) {
        return true;
    }

    if (!allow_mapgen) {
        return false;
    }

    snprintf(mapgen, sizeof(mapgen), "%s\\bin\\d2-mapgen.exe", project);
    if (!file_exists(mapgen)) {
        return false;
    }

    snprintf(tmp_raw, sizeof(tmp_raw), "%s\\_raw_%u_%u.txt", cache_dir, seed, difficulty);
    /* d2-mapgen /C <lodpath> --seed N --difficulty D */
    snprintf(cmd, sizeof(cmd),
             "\"%s\" /C \"%s\" --seed %u --difficulty %u",
             mapgen, project, seed, difficulty);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    {
        SECURITY_ATTRIBUTES sa;
        HANDLE hOut;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        hOut = CreateFileA(tmp_raw, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut == INVALID_HANDLE_VALUE) {
            return false;
        }
        si.hStdOutput = hOut;
        si.hStdError = hOut;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        memset(&pi, 0, sizeof(pi));
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, project, &si, &pi)) {
            CloseHandle(hOut);
            return false;
        }
        WaitForSingleObject(pi.hProcess, 180000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(hOut);
    }

    line = (char *)malloc(linecap);
    if (!line) return false;
    in = fopen(tmp_raw, "rb");
    if (!in) {
        free(line);
        return false;
    }
    out = fopen(cache_file, "wb");
    if (!out) { fclose(in); free(line); return false; }

    fprintf(out, "{\"seed\":%u,\"difficulty\":%u,\"levels\":[", seed, difficulty);
    while (fgets(line, (int)linecap, in)) {
        char *s = line;
        size_t L;
        while (*s == ' ' || *s == '\t') s++;
        if (s[0] != '{') continue;
        if (!strstr(s, "\"id\"") || !strstr(s, "\"map\"")) continue;
        if (strstr(s, "\"msg\"") && strstr(s, "\"source\"")) continue;
        L = strlen(s);
        while (L && (s[L - 1] == '\n' || s[L - 1] == '\r')) s[--L] = 0;
        if (!first) fputc(',', out);
        fputs(s, out);
        first = 0;
    }
    fputs("]}", out);
    fclose(out);
    fclose(in);
    free(line);

    if (first) {
        DeleteFileA(cache_file);
        return false;
    }
    DeleteFileA(tmp_raw);
    return true;
}
