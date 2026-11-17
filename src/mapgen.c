#include "mapgen.h"
#include "app.h"
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

static int has_mpq(const char *dir) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s\\d2data.mpq", dir);
    if (file_exists(p)) return 1;
    snprintf(p, sizeof(p), "%s\\d2exp.mpq", dir);
    return file_exists(p);
}

/* d2-mapgen.exe peut etre dans le dossier du jeu, dans bin\, ou juste au-dessus. */
static int find_mapgen(const char *game, char *mapgen, size_t mapgen_n) {
    static const char *rel[] = {
        "d2-mapgen.exe",
        "bin\\d2-mapgen.exe",
        "..\\d2-mapgen.exe",
        "..\\bin\\d2-mapgen.exe",
        NULL
    };
    char cand[MAX_PATH];
    int i;

    for (i = 0; rel[i]; i++) {
        snprintf(cand, sizeof(cand), "%s\\%s", game, rel[i]);
        if (!file_exists(cand))
            continue;
        if (!GetFullPathNameA(cand, (DWORD)mapgen_n, mapgen, NULL))
            continue;
        return 1;
    }
    return 0;
}

static int resolve_lod(const char *exe_dir, char *lod, size_t lod_n, char *mapgen, size_t mapgen_n) {
    /* Le guide met souvent les MPQ dans bin ou bin\\d2lod.
       L'install classique les met dans Campagn, a cote de bin\\. */
    static const char *rel[] = {
        "Campagn\\bin",
        "Campagn\\bin\\d2lod",
        "Campagn",
        "bin\\d2lod",
        "bin",
        "",
        "..\\Campagn\\bin",
        "..\\Campagn",
        "..",
        NULL
    };
    char cand[MAX_PATH];
    char full[MAX_PATH];
    int i;

    for (i = 0; rel[i]; i++) {
        if (rel[i][0])
            snprintf(cand, sizeof(cand), "%s\\%s", exe_dir, rel[i]);
        else
            snprintf(cand, sizeof(cand), "%s", exe_dir);
        if (!GetFullPathNameA(cand, MAX_PATH, full, NULL))
            continue;
        if (!has_mpq(full))
            continue;
        if (!find_mapgen(full, mapgen, mapgen_n))
            continue;
        snprintf(lod, lod_n, "%s", full);
        {
            static int logged;
            char msg[MAX_PATH + 32];
            if (!logged) {
                logged = 1;
                snprintf(msg, sizeof(msg), "lod: %s", full);
                app_log(msg);
            }
        }
        return 1;
    }
    return 0;
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
    if (!resolve_lod(exe_dir, project, sizeof(project), mapgen, sizeof(mapgen))) {
        static int warned_lod;
        if (!warned_lod) {
            warned_lod = 1;
            app_error("d2-mapgen introuvable.\n"
                      "d2data.mpq doit etre dans Campagn\\bin (a cote de d2-mapgen.exe),\n"
                      "dans Campagn\\bin\\d2lod, ou directement dans Campagn.");
        }
        return false;
    }

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
        static int warned_empty;
        DeleteFileA(cache_file);
        if (!warned_empty) {
            warned_empty = 1;
            app_error("Generation de carte echouee.\nVerifie que Campagn contient une install LoD complete.");
        }
        return false;
    }
    DeleteFileA(tmp_raw);
    return true;
}
