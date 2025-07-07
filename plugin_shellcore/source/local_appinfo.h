#pragma once

struct disk_appinfo
{
    int pid;
    char m_titleid[10];
    char m_category[8];
    char m_appver[8];
};

#define TEMP_INFO_PATH "/av_contents/content_tmp"
#define TEMP_INFO_FILE "app_info"
#define TEMP_INFO_FILE_BIN "app_info_raw.bin"
#define TEMP_INFO_PATH_SANDBOX TEMP_INFO_PATH "/" TEMP_INFO_FILE
