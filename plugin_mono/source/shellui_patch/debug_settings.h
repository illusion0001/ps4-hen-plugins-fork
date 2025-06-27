#pragma once

#include <orbis/libkernel.h>

void UploadDebugSettingsPatch(void);
void UploadNewPkgInstallerPath(void* app_exe);
void UploadBgftFixup(const struct OrbisKernelModuleInfo* info);
void UploadShellUICheck(void);
