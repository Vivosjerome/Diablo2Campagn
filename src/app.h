#ifndef MYTURN_APP_H
#define MYTURN_APP_H

#include <windows.h>

static inline void app_error(const char *msg) {
    MessageBoxA(NULL, msg, "Erreur", MB_OK | MB_ICONERROR);
}

#endif
