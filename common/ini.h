#pragma once

#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 512
#define MAX_SECTION_LENGTH 256
#define MAX_COMMENT_LENGTH 512

typedef struct Comment
{
    char text[MAX_COMMENT_LENGTH];
    struct Comment* next;
} Comment;

typedef struct KeyValue
{
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
    char comment[MAX_COMMENT_LENGTH];
    Comment* comments_before;
    struct KeyValue* next;
} KeyValue;

typedef struct Section
{
    char name[MAX_SECTION_LENGTH];
    char comment[MAX_COMMENT_LENGTH];
    Comment* comments_before;
    KeyValue* keys;
    struct Section* next;
} Section;

typedef struct
{
    Section* sections;
    Comment* header_comments;
    char* filename;
    int modified;
} INIFile;

#if defined(cplusplus)
extern "C" 
{
#endif

INIFile* ini_load(const char* filename);
void ini_free(INIFile* ini);
int ini_save(INIFile* ini);
char* ini_get(INIFile* ini, const char* section, const char* key);
int ini_set(INIFile* ini, const char* section, const char* key, const char* value);
int ini_set_with_comment(INIFile* ini, const char* section, const char* key, const char* value, const char* comment);
int ini_delete_key(INIFile* ini, const char* section, const char* key);
int ini_delete_section(INIFile* ini, const char* section);
int ini_add_comment(INIFile* ini, const char* section, const char* comment_text);
int ini_add_section_comment(INIFile* ini, const char* section, const char* comment_text);
void ini_print(INIFile* ini);

#if defined(cplusplus)
}
#endif
