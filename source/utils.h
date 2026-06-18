#ifndef UTILS_H
#define UTILS_H
#include "cJSON.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "logs.h"
void jstrcpy(char *dst, size_t dstsz, cJSON *jstr);
void open_json(const char* path, cJSON** json);
void save_json(const char* path, cJSON* json);
bool fileExists(const char* path);
int strcasestr_custom(const char *haystack, const char *needle);

#endif