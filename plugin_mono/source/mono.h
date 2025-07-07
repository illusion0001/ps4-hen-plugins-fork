#pragma once

#include <stdio.h>
#include <stdint.h>
#include <wchar.h>

extern void* Root_Domain;
extern void* App_Exe;
extern void* MSCorlib;

typedef struct MonoString
{
    void* pad[2];
    uint32_t string_len;
    wchar_t str[1];
} MonoString;

typedef struct MonoObject
{
    void* pad[2];
    union v
    {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        void* v;
    } value;
} MonoObject;

extern void mono_thread_attach(void*);
extern void*
mono_class_from_name(const void* image, const char* class_namespace, const char* class_name);
extern void* mono_class_get_method_from_name(const void* method_class, const char* method_name, int param_count);
extern void* mono_runtime_invoke(const void* method, const void* obj, const void* params[], const void* exc[]);
extern void* mono_get_root_domain(void);
extern void* mono_domain_assembly_open(const void*, const char*);
extern void* mono_aot_get_method(const void*, const void*);
extern void* mono_property_get_get_method(const void*);
extern void* mono_property_get_set_method(const void*);
extern void* mono_class_get_property_from_name(const void*, const char*);
extern void* mono_object_get_class(const void*);
extern void* mono_object_unbox(const void*);
extern void* mono_assembly_get_image(const void*);
extern void* mono_array_new(const void*, const void*, uint32_t);
extern void* mono_get_byte_class(void);
extern void* mono_array_addr_with_size(const void*, size_t, size_t);
extern void* mono_object_new(const void*, const void*);

extern void* mono_object_to_string(const void* obj, void** exc);

extern char* mono_string_to_utf8(const void* monostr);
extern wchar_t* mono_string_to_utf16(const void* monostr);
extern void* mono_string_new(const void*, const char*);
extern void mono_free(const void*);
extern void* mono_runtime_object_init(void*);

void* mono_get_image(const char* p);
void* Mono_Get_Class(const void* Assembly_Image, const char* Namespace, const char* Class_Name);
void* Mono_Get_Instance(const void* Klass, const char* Instance);
void* Mono_Get_InstanceEx(const void* Assembly_Image, const char* Namespace, const char* Class_Name, const char* Instance);
void* Mono_Get_Address_of_Method(const void* Assembly_Image, const char* Name_Space, const char* Class_Name, const char* Method_Name, const int Param_Count);
MonoString* Mono_New_String(const char* text);
void* Mono_New_Stream(const void* mscorlib, const void* Buffer, const uint32_t Buffer_Size);
void* Mono_File_Stream(const void* corlib, const char* file_path);

void* GetMsCorlib(void);
const char* Mono_Read_Stream(const void* domain, const void* image, const void* stream_obj);

void* Mono_Get_Property(const void* klass, const void* instance, const char* property_name);
void Mono_Set_Property(const void* klass, const void* instance, const char* property_name, const void* value);
MonoString* Mono_Add_String(MonoString* monoStr, const char* cStr);
void* Mono_New_Object(void* Klass);
