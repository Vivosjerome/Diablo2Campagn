#ifndef MYTURN_APP_H
#define MYTURN_APP_H

#include <windows.h>
#include <stdio.h>
#include <string.h>

static inline void app_log(const char *msg) {
    char dir[MAX_PATH];
    char path[MAX_PATH];
    char *slash;
    FILE *f;

    GetModuleFileNameA(NULL, dir, MAX_PATH);
    slash = strrchr(dir, '\\');
    if (slash) *slash = 0;
    snprintf(path, sizeof(path), "%s\\CampagneD2.log", dir);
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", msg);
    fclose(f);
}

static inline void app_error(const char *msg) {
    app_log(msg);
    MessageBoxA(NULL, msg, "Erreur", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

#endif
