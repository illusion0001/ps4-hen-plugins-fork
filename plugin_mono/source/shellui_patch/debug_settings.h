#pragma once

#include <orbis/libkernel.h>

void UploadDebugSettingsPatch(void);
void UploadNewPkgInstallerPath(void* app_exe, int app_exe_h);
void UploadBgftFixup(const struct OrbisKernelModuleInfo* info);
void UploadShellUICheck(void);
