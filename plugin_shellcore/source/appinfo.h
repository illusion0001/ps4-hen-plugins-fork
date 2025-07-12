#pragma once

#include "../../common/type_struct.h"

// this is std::string members but I didn't bother :3
TYPE_BEGIN(struct appinfo, 0x170);
TYPE_FIELD(char m_titleid[10], 0x40);
TYPE_FIELD(char m_category[8], 0x98);
TYPE_FIELD(char m_appver[8], 0x160);
TYPE_END();
