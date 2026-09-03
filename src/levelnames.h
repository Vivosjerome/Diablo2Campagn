#ifndef MYTURN_LEVELNAMES_H
#define MYTURN_LEVELNAMES_H

#include <stdbool.h>

bool level_names_load(const char *json_path);
const char *level_name(int id); /* never NULL; "Area N" fallback */
bool level_is_town(int id);
int level_act(int id);

#endif
