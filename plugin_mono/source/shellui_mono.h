#pragma once

#define ID_WAIT "id_loading"

#include <stdint.h>

enum ShellUIElementType : uint32_t
{
    Invalid = 0,
    Button = 2,
    List = 5,
    ToggleSwitch = 8,
    Message = 21,
};

void UI_ResetItem(const char* id);
void UI_AddItem(void* e);
void* UI_NewElementData(enum ShellUIElementType ElementType, const char* Id, const char* Title, const char* Title2, const char* Icon);
