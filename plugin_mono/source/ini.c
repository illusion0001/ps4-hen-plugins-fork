#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ini.h"

static char* trim_whitespace(char* str)
{
    char* end;

    while (isspace((unsigned char)*str))
    {
        str++;
    }

    if (*str == 0)
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }

    end[1] = '\0';
    return str;
}

static char* extract_comment(char* line, char comment_char)
{
    char* comment_pos = strchr(line, comment_char);
    if (!comment_pos)
    {
        return NULL;
    }

    int in_quotes = 0;
    char* check = line;
    while (check < comment_pos)
    {
        if (*check == '"' || *check == '\'')
        {
            in_quotes = !in_quotes;
        }
        else if (*check == '\\' && (check[1] == '"' || check[1] == '\''))
        {
            check++;
        }
        check++;
    }

    if (in_quotes)
    {
        return NULL;
    }

    *comment_pos = '\0';
    return trim_whitespace(comment_pos + 1);
}

static Comment* create_comment(const char* comment_text)
{
    Comment* new_comment = malloc(sizeof(Comment));
    if (!new_comment)
    {
        return NULL;
    }

    strncpy(new_comment->text, comment_text, MAX_COMMENT_LENGTH - 1);
    new_comment->text[MAX_COMMENT_LENGTH - 1] = '\0';
    new_comment->next = NULL;

    return new_comment;
}

static void add_comment_to_list(Comment** list, const char* comment_text)
{
    Comment* new_comment = create_comment(comment_text);
    if (!new_comment)
    {
        return;
    }

    if (!*list)
    {
        *list = new_comment;
    }
    else
    {
        Comment* current = *list;
        while (current->next)
        {
            current = current->next;
        }
        current->next = new_comment;
    }
}

static void free_comment_list(Comment* list)
{
    while (list)
    {
        Comment* next = list->next;
        free(list);
        list = next;
    }
}

static Section* find_section(INIFile* ini, const char* section_name)
{
    Section* current = ini->sections;
    while (current)
    {
        if (strcmp(current->name, section_name) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static Section* create_section(INIFile* ini, const char* section_name)
{
    Section* new_section = malloc(sizeof(Section));
    if (!new_section)
    {
        return NULL;
    }

    strncpy(new_section->name, section_name, MAX_SECTION_LENGTH - 1);
    new_section->name[MAX_SECTION_LENGTH - 1] = '\0';
    new_section->comment[0] = '\0';
    new_section->comments_before = NULL;
    new_section->keys = NULL;
    new_section->next = ini->sections;
    ini->sections = new_section;

    return new_section;
}

static KeyValue* find_key(Section* section, const char* key_name)
{
    KeyValue* current = section->keys;
    while (current)
    {
        if (strcmp(current->key, key_name) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static KeyValue* create_key(Section* section, const char* key_name)
{
    KeyValue* new_key = malloc(sizeof(KeyValue));
    if (!new_key)
    {
        return NULL;
    }

    strncpy(new_key->key, key_name, MAX_KEY_LENGTH - 1);
    new_key->key[MAX_KEY_LENGTH - 1] = '\0';
    new_key->value[0] = '\0';
    new_key->comment[0] = '\0';
    new_key->comments_before = NULL;
    new_key->next = section->keys;
    section->keys = new_key;

    return new_key;
}

INIFile* ini_load(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        printf("Warning: Could not open file '%s'. Creating new INI structure.\n", filename);
    }

    INIFile* ini = malloc(sizeof(INIFile));
    if (!ini)
    {
        return NULL;
    }

    ini->sections = NULL;
    ini->header_comments = NULL;
    ini->filename = malloc(strlen(filename) + 1);
    strcpy(ini->filename, filename);
    ini->modified = 0;

    if (!file)
    {
        return ini;
    }

    char line[MAX_LINE_LENGTH];
    Section* current_section = NULL;
    Comment** pending_comments = &ini->header_comments;

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = '\0';

        char* original_line = strdup(line);
        char* trimmed = trim_whitespace(line);

        if (*trimmed == ';' || *trimmed == '#')
        {
            char* comment_text = trim_whitespace(trimmed + 1);
            add_comment_to_list(pending_comments, comment_text);
        }

        else if (*trimmed == '\0')
        {
            add_comment_to_list(pending_comments, "");
        }

        else if (*trimmed == '[')
        {
            char* line_copy = strdup(original_line);

            char* comment = extract_comment(line_copy, ';');
            if (!comment)
            {
                comment = extract_comment(line_copy, '#');
            }

            char* end = strchr(line_copy, ']');
            if (end)
            {
                *end = '\0';
                char* section_name = trim_whitespace(line_copy + 1);
                current_section = create_section(ini, section_name);

                if (current_section)
                {
                    current_section->comments_before = *pending_comments;
                    *pending_comments = NULL;

                    if (comment && *comment)
                    {
                        strncpy(current_section->comment, comment, MAX_COMMENT_LENGTH - 1);
                        current_section->comment[MAX_COMMENT_LENGTH - 1] = '\0';
                    }

                    pending_comments = &current_section->comments_before;
                    *pending_comments = NULL;
                }
            }
            free(line_copy);
        }

        else
        {
            char* equals = strchr(original_line, '=');
            if (equals && current_section)
            {
                char* line_copy = strdup(original_line);

                char* comment = extract_comment(line_copy, ';');
                if (!comment)
                {
                    comment = extract_comment(line_copy, '#');
                }

                equals = strchr(line_copy, '=');
                if (equals)
                {
                    *equals = '\0';
                    char* key = trim_whitespace(line_copy);
                    char* value = trim_whitespace(equals + 1);

                    KeyValue* kv = create_key(current_section, key);
                    if (kv)
                    {
                        strncpy(kv->value, value, MAX_VALUE_LENGTH - 1);
                        kv->value[MAX_VALUE_LENGTH - 1] = '\0';

                        kv->comments_before = *pending_comments;
                        *pending_comments = NULL;

                        if (comment && *comment)
                        {
                            strncpy(kv->comment, comment, MAX_COMMENT_LENGTH - 1);
                            kv->comment[MAX_COMMENT_LENGTH - 1] = '\0';
                        }
                    }
                }
                free(line_copy);
            }
        }

        free(original_line);
    }

    fclose(file);
    return ini;
}

void ini_free(INIFile* ini)
{
    if (!ini)
    {
        return;
    }

    free_comment_list(ini->header_comments);

    Section* section = ini->sections;
    while (section)
    {
        free_comment_list(section->comments_before);

        KeyValue* key = section->keys;
        while (key)
        {
            free_comment_list(key->comments_before);
            KeyValue* next_key = key->next;
            free(key);
            key = next_key;
        }
        Section* next_section = section->next;
        free(section);
        section = next_section;
    }

    free(ini->filename);
    free(ini);
}

int ini_save(INIFile* ini)
{
    if (!ini || !ini->filename)
    {
        return 0;
    }

    FILE* file = fopen(ini->filename, "w");
    if (!file)
    {
        return 0;
    }

    Comment* comment = ini->header_comments;
    while (comment)
    {
        if (strlen(comment->text) == 0)
        {
            fprintf(file, "\n");
        }
        else
        {
            fprintf(file, "; %s\n", comment->text);
        }
        comment = comment->next;
    }

    Section* section = ini->sections;
    while (section)
    {
        comment = section->comments_before;
        while (comment)
        {
            if (strlen(comment->text) == 0)
            {
                fprintf(file, "\n");
            }
            else
            {
                fprintf(file, "; %s\n", comment->text);
            }
            comment = comment->next;
        }

        if (strlen(section->comment) > 0)
        {
            fprintf(file, "[%s] ; %s\n", section->name, section->comment);
        }
        else
        {
            fprintf(file, "[%s]\n", section->name);
        }

        KeyValue* key = section->keys;
        while (key)
        {
            comment = key->comments_before;
            while (comment)
            {
                if (strlen(comment->text) == 0)
                {
                    fprintf(file, "\n");
                }
                else
                {
                    fprintf(file, "; %s\n", comment->text);
                }
                comment = comment->next;
            }

            if (strlen(key->comment) > 0)
            {
                fprintf(file, "%s = %s ; %s\n", key->key, key->value, key->comment);
            }
            else
            {
                fprintf(file, "%s = %s\n", key->key, key->value);
            }
            key = key->next;
        }

        fprintf(file, "\n");
        section = section->next;
    }

    fclose(file);
    ini->modified = 0;
    return 1;
}

char* ini_get(INIFile* ini, const char* section_name, const char* key_name)
{
    if (!ini)
    {
        return NULL;
    }

    Section* section = find_section(ini, section_name);
    if (!section)
    {
        return NULL;
    }

    KeyValue* key = find_key(section, key_name);
    if (!key)
    {
        return NULL;
    }

    return key->value;
}

int ini_set(INIFile* ini, const char* section_name, const char* key_name, const char* value)
{
    return ini_set_with_comment(ini, section_name, key_name, value, NULL);
}

int ini_set_with_comment(INIFile* ini, const char* section_name, const char* key_name, const char* value, const char* comment)
{
    if (!ini || !section_name || !key_name || !value)
    {
        return 0;
    }

    Section* section = find_section(ini, section_name);
    if (!section)
    {
        section = create_section(ini, section_name);
        if (!section)
        {
            return 0;
        }
    }

    KeyValue* key = find_key(section, key_name);
    if (!key)
    {
        key = create_key(section, key_name);
        if (!key)
        {
            return 0;
        }
    }

    strncpy(key->value, value, MAX_VALUE_LENGTH - 1);
    key->value[MAX_VALUE_LENGTH - 1] = '\0';

    if (comment)
    {
        strncpy(key->comment, comment, MAX_COMMENT_LENGTH - 1);
        key->comment[MAX_COMMENT_LENGTH - 1] = '\0';
    }

    ini->modified = 1;
    return 1;
}

int ini_add_comment(INIFile* ini, const char* section_name, const char* comment_text)
{
    if (!ini || !comment_text)
    {
        return 0;
    }

    if (!section_name)
    {
        add_comment_to_list(&ini->header_comments, comment_text);
    }
    else
    {
        Section* section = find_section(ini, section_name);
        if (!section)
        {
            return 0;
        }
        add_comment_to_list(&section->comments_before, comment_text);
    }

    ini->modified = 1;
    return 1;
}

int ini_add_section_comment(INIFile* ini, const char* section_name, const char* comment_text)
{
    if (!ini || !section_name || !comment_text)
    {
        return 0;
    }

    Section* section = find_section(ini, section_name);
    if (!section)
    {
        return 0;
    }

    strncpy(section->comment, comment_text, MAX_COMMENT_LENGTH - 1);
    section->comment[MAX_COMMENT_LENGTH - 1] = '\0';
    ini->modified = 1;
    return 1;
}

int ini_delete_key(INIFile* ini, const char* section_name, const char* key_name)
{
    if (!ini)
    {
        return 0;
    }

    Section* section = find_section(ini, section_name);
    if (!section)
    {
        return 0;
    }

    KeyValue* prev = NULL;
    KeyValue* current = section->keys;

    while (current)
    {
        if (strcmp(current->key, key_name) == 0)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                section->keys = current->next;
            }
            free(current);
            ini->modified = 1;
            return 1;
        }
        prev = current;
        current = current->next;
    }

    return 0;
}

int ini_delete_section(INIFile* ini, const char* section_name)
{
    if (!ini)
    {
        return 0;
    }

    Section* prev = NULL;
    Section* current = ini->sections;

    while (current)
    {
        if (strcmp(current->name, section_name) == 0)
        {
            KeyValue* key = current->keys;
            while (key)
            {
                KeyValue* next_key = key->next;
                free(key);
                key = next_key;
            }

            free_comment_list(current->comments_before);

            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                ini->sections = current->next;
            }

            free(current);
            ini->modified = 1;
            return 1;
        }
        prev = current;
        current = current->next;
    }

    return 0;
}

void ini_print(INIFile* ini)
{
    if (!ini)
    {
        return;
    }

    printf("INI File: %s\n", ini->filename);
    printf("Modified: %s\n\n", ini->modified ? "Yes" : "No");

    Comment* comment = ini->header_comments;
    while (comment)
    {
        if (strlen(comment->text) == 0)
        {
            printf("\n");
        }
        else
        {
            printf("; %s\n", comment->text);
        }
        comment = comment->next;
    }

    Section* section = ini->sections;
    while (section)
    {
        comment = section->comments_before;
        while (comment)
        {
            if (strlen(comment->text) == 0)
            {
                printf("\n");
            }
            else
            {
                printf("; %s\n", comment->text);
            }
            comment = comment->next;
        }

        if (strlen(section->comment) > 0)
        {
            printf("[%s] ; %s\n", section->name, section->comment);
        }
        else
        {
            printf("[%s]\n", section->name);
        }

        KeyValue* key = section->keys;
        while (key)
        {
            comment = key->comments_before;
            while (comment)
            {
                if (strlen(comment->text) == 0)
                {
                    printf("\n");
                }
                else
                {
                    printf("; %s\n", comment->text);
                }
                comment = comment->next;
            }

            if (strlen(key->comment) > 0)
            {
                printf("%s = %s ; %s\n", key->key, key->value, key->comment);
            }
            else
            {
                printf("%s = %s\n", key->key, key->value);
            }
            key = key->next;
        }

        printf("\n");
        section = section->next;
    }
}
