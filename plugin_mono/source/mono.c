// https://github.com/OSM-Made/Orbis-Toolbox/blob/main/Orbis%20Toolbox/Mono.cpp
// Code from Orbis-Toolbox Mono helpers ported to C
// Thanks OSM!

#include "mono.h"

#include <string.h>
#include <stdlib.h>
#include "../../common/plugin_common.h"

void* Root_Domain = 0;
void* App_Exe = 0;
void* MSCorlib = 0;

void* mono_get_image(const char* p)
{
    if (!Root_Domain)
    {
        puts("No Root_Domain");
        return 0;
    }
    void* Assembly = mono_domain_assembly_open(Root_Domain, p);
    if (!Assembly)
    {
        printf("GetImage: Failed to open \"%s\" assembly.\n", p);
        return 0;
    }
    void* Assembly_Image = mono_assembly_get_image(Assembly);
    if (!Assembly_Image)
    {
        printf("GetImage: Failed to open \"%s\" Image.\n", p);
        return 0;
    }
    return Assembly_Image;
}

void* Mono_Get_Class(const void* Assembly_Image, const char* Namespace, const char* Class_Name)
{
    void* klass = mono_class_from_name(Assembly_Image, Namespace, Class_Name);
    if (!klass)
    {
        printf("Get_Class: Failed to open \"%s\" class from \"%s\" Namespace.\n", Class_Name, Namespace);
    }
    return klass;
}

void* Mono_Get_Instance(const void* Klass, const char* Instance)
{
    if (!Klass)
    {
        puts("Get_Instance: Klass was null.");
        return 0;
    }

    void* inst_prop = mono_class_get_property_from_name(Klass, Instance);

    if (!inst_prop)
    {
        puts("Failed to find Instance property");
        // printf("Failed to find Instance property \"%s\" in Class \"%s\".\n", Instance, Klass->name);
        return 0;
    }

    void* inst_get_method = mono_property_get_get_method(inst_prop);

    if (!inst_get_method)
    {
        puts("Failed to find get method");
        // printf("Failed to find get method for \"%s\" in Class \"%s\".\n", Instance, Klass->name);
        return 0;
    }

    void* inst = mono_runtime_invoke(inst_get_method, 0, 0, 0);

    if (!inst)
    {
        puts("Failed to find get Instance");
        // printf("Failed to find get Instance \"%s\" in Class \"%s\".\n", Instance, Klass->name);
        return 0;
    }

    return inst;
}

void* Mono_Get_InstanceEx(const void* Assembly_Image, const char* Namespace, const char* Class_Name, const char* Instance)
{
    return Mono_Get_Instance(Mono_Get_Class(Assembly_Image, Namespace, Class_Name), Instance);
}

void* Mono_Get_Address_of_Method(const void* Assembly_Image, const char* Name_Space, const char* Class_Name, const char* Method_Name, int Param_Count)
{
    void* klass = Mono_Get_Class(Assembly_Image, Name_Space, Class_Name);

    if (!klass)
    {
        printf("Get_Address_of_Method: failed to open class \"%s\" in namespace \"%s\"\n", Class_Name, Name_Space);
        return 0;
    }

    void* Method = mono_class_get_method_from_name(klass, Method_Name, Param_Count);

    if (!Method)
    {
        printf("Get_Address_of_Method: failed to find method \"%s\" in class \"%s\"\n", Method_Name, Class_Name);
        return 0;
    }

    return mono_aot_get_method(Root_Domain, Method);
}

MonoString* Mono_New_String(const char* text)
{
    return (MonoString*)mono_string_new(Root_Domain, text);
}

void* Mono_New_Array(const uint32_t buffer_size)
{
    void* array = mono_array_new(Root_Domain, mono_get_byte_class(), buffer_size);
    if (array)
    {
        return mono_array_addr_with_size(array, 1, 0);
    }
    return 0;
}

void* Mono_New_Stream(const void* mscorlib, const void* Buffer, const uint32_t Buffer_Size)
{
    const void* Array = mono_array_new(Root_Domain, mono_get_byte_class(), Buffer_Size);
    void* Array_addr = mono_array_addr_with_size(Array, sizeof(char), 0);
    memcpy(Array_addr, Buffer, Buffer_Size);
    const void* MemoryStream_class = Mono_Get_Class(mscorlib, "System.IO", "MemoryStream");
    void* MemoryStream_Instance = mono_object_new(Root_Domain, MemoryStream_class);
    // do it seprately because `MemoryStream_class` is needed
    const void* ctor = mono_class_get_method_from_name(MemoryStream_class, ".ctor", 2);
    const void* args[2];
    args[0] = Array;
    int writable = 1;
    args[1] = &writable;
    const void* exception = NULL;
    mono_runtime_invoke(ctor, MemoryStream_Instance, args, &exception);
    if (exception)
    {
        final_printf("Failed to allocate mono stream! Buffer 0x%p Size %u\n", Buffer, Buffer_Size);
        return NULL;
    }
    return MemoryStream_Instance;
}

void* Mono_File_Stream(const void* corlib, const char* file_path)
{
    const void* file_class = mono_class_from_name(
        corlib,
        "System.IO",
        "File");
    const void* open_method = mono_class_get_method_from_name(
        file_class,
        "Open",
        3);

    if (!open_method)
    {
        printf("Could not find File.Open method\n");
        return NULL;
    }

    void* path_str = mono_string_new(Root_Domain, file_path);
    if (!path_str)
    {
        printf("Failed to allocate string for %s\n", __FUNCTION__);
        return NULL;
    }
    const int filemode_open = 3;
    const int fileaccess_read = 1;
    const void* args[] = {path_str, &filemode_open, &fileaccess_read};
    const void* exception = NULL;
    void* filestream = mono_runtime_invoke(
        open_method,
        NULL,
        args,
        &exception);
    if (exception)
    {
        printf("Exception occurred while opening file %s\n", file_path);
        return NULL;
    }
    return filestream;
}

const char* Mono_Read_Stream(const void* domain, const void* corelib, const void* stream_obj)
{
    if (!domain || !corelib || !stream_obj)
    {
        final_printf("Invalid arguments.\n");
        return 0;
    }
    static const char space[] = "System.IO";
    static const char class[] = "StreamReader";
    const void* sr_class = Mono_Get_Class(corelib, space, class);
    if (!sr_class)
    {
        final_printf("Failed to find %s class.\n", class);
        return 0;
    }
    const void* ctor = mono_class_get_method_from_name(sr_class, ".ctor", 1);
    if (!ctor)
    {
        final_printf("Failed to find %s.%s constructor.\n", space, class);
        return 0;
    }
    const void* reader_instance = mono_object_new(domain, sr_class);
    const void* ctor_args[1] = {stream_obj};
    const void* exc = NULL;
    mono_runtime_invoke(ctor, reader_instance, ctor_args, &exc);
    if (exc)
    {
        const void* msg = mono_object_to_string(exc, NULL);
        const char* c_msg = mono_string_to_utf8(msg);
        final_printf("Constructor exception: %s\n", c_msg);
        mono_free(c_msg);
        return 0;
    }
    static const char method[] = "ReadToEnd";
    const void* read_method = mono_class_get_method_from_name(sr_class, method, 0);
    if (!read_method)
    {
        final_printf("Failed to find %s method.\n", method);
        return 0;
    }
    const void* result = mono_runtime_invoke(read_method, reader_instance, NULL, &exc);
    if (exc)
    {
        const void* msg = mono_object_to_string(exc, NULL);
        const char* c_msg = mono_string_to_utf8(msg);
        final_printf("%s exception: %s\n", method, c_msg);
        mono_free(c_msg);
        return 0;
    }
    const char* c_str = mono_string_to_utf8(result);
    return c_str;
}

void* Mono_Get_Property(const void* klass, const void* instance, const char* property_name)
{
    if (!klass || !property_name)
    {
        final_printf("Null class or property name\n");
        return NULL;
    }

    const void* prop = mono_class_get_property_from_name(klass, property_name);
    if (!prop)
    {
        final_printf("Property \"%s\" not found", property_name);
        return NULL;
    }

    const void* get_method = mono_property_get_get_method(prop);
    if (!get_method)
    {
        final_printf("Get method for \"%s\" is null", property_name);
        return NULL;
    }

    void* result = mono_runtime_invoke(get_method, instance, NULL, NULL);
    return result;
}

void Mono_Set_Property(const void* klass, const void* instance, const char* property_name, const void* value)
{
    if (!klass || !property_name)
    {
        final_printf("Null class or property name\n");
        return;
    }

    const void* prop = mono_class_get_property_from_name(klass, property_name);
    if (!prop)
    {
        final_printf("Property \"%s\" not found\n", property_name);
        return;
    }

    const void* set_method = mono_property_get_set_method(prop);
    if (!set_method)
    {
        final_printf("Set method for \"%s\" is null\n", property_name);
        return;
    }

    const void* args[] = {value};
    mono_runtime_invoke(set_method, instance, args, NULL);
}

void* GetMsCorlib(void)
{
    if (MSCorlib)
    {
        return MSCorlib;
    }
    extern char* sceKernelGetFsSandboxRandomWord(void);
    const char* sandbox_path = sceKernelGetFsSandboxRandomWord();
    if (sandbox_path)
    {
        char mscorlib_sprx[260] = {};
        snprintf(mscorlib_sprx, sizeof(mscorlib_sprx), "/%s/common/lib/mscorlib.dll", sandbox_path);
        void* mscorlib_ptr = mono_get_image(mscorlib_sprx);
        if (mscorlib_ptr)
        {
            MSCorlib = mscorlib_ptr;
            return MSCorlib;
        }
    }
    return 0;
}

MonoString* Mono_Add_String(MonoString* monoStr, const char* cStr)
{
    MonoString* monoStr2 = Mono_New_String(cStr);
    void* string_class = mono_class_from_name(MSCorlib, "System", "String");
    if (!string_class)
    {
        final_printf("Failed to get System.String class\n");
        return NULL;
    }
    void* concat_method = mono_class_get_method_from_name(string_class, "Concat", 2);
    if (!concat_method)
    {
        final_printf("Failed to find System.String::Concat(string, string)\n");
        return NULL;
    }
    const void* args[] = {monoStr, monoStr2};
    const void* exc = NULL;
    MonoString* result = (MonoString*)mono_runtime_invoke(concat_method, NULL, args, &exc);
    if (exc)
    {
        MonoString* exc_str = mono_object_to_string(exc, NULL);
        char* utf8 = mono_string_to_utf8(exc_str);
        final_printf( "Exception: %s\n", utf8);
        mono_free(utf8);
        return NULL;
    }
    return result;
}

void* Mono_New_Object(void* Klass)
{
    if (!Klass)
    {
        ffinal_printf("Klass pointer is null.\n");
        return 0;
    }
    void* Obj = mono_object_new(Root_Domain, Klass);
    if (!Obj)
    {
        ffinal_printf("Failed to Create new object\n");
    }
    return Obj;
}
