#pragma once

#include <stdint.h>
#include <orbis/libkernel.h>

#include "../../../common/path.h"

void UploadOnBranch(void* app_exe);
void UploadNewCorelibStreamReader(void);
void UploadFinishBootEffectCode(const struct OrbisKernelModuleInfo* info);
