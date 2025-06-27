#pragma once

#include <stdint.h>
#include <orbis/libkernel.h>

#define BASE_PATH "/data/hen"
#define SHELLUI_DATA_PATH BASE_PATH "/shellui_data"
#define HEN_INI BASE_PATH "/hen.ini"
#define HEN_SECTION "HEN"
#define SHELLUI_HEN_SETTINGS SHELLUI_DATA_PATH "/hen_settings.xml"

void UploadOnBranch(void* app_exe);
void UploadNewCorelibStreamReader(void);
