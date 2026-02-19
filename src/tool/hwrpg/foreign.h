/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <stdint.h>
#include <stddef.h>

extern "C" {
struct ForeignContext;
struct ValueData;
struct FuncData;

struct FuncDataAsync {
    ValueData* (*get_temporary)(FuncDataAsync* data);
    void (*set_boolean)(ValueData* data, int value);
    void (*set_number)(ValueData* data, int64_t value);
    void (*set_string)(ValueData* data, const char* value);
    void (*set_bytes)(ValueData* data, const char* value, size_t size);
    void (*set_foreign_object)(ValueData* data, void* p_shared_ptr, int tag);
    void (*set_array)(ValueData* data, size_t size);
    void (*set_object)(ValueData* data);
    ValueData* (*get_array_element)(ValueData* data, size_t index, int may_append);
    ValueData* (*get_object_element)(ValueData* data, const char* key, int may_create);
    void (*push_arg)(FuncDataAsync* data, ValueData* value);
    int (*call)(FuncDataAsync* data);     // call with args and return 1 and clear args if success else 0
    void (*unload)(FuncDataAsync* data);  // only delete
};

struct ForeignCallback {
    ForeignContext* data;
    ValueData* (*get_arg)(ForeignContext* data, size_t index);
    ValueData* (*get_return_value_location)(ForeignContext* data);
    void (*set_boolean)(ValueData* data, int value);
    void (*set_number)(ValueData* data, int64_t value);
    void (*set_string)(ValueData* data, const char* value);
    void (*set_bytes)(ValueData* data, const char* value, size_t size);
    void (*set_array)(ValueData* data, size_t size);
    void (*set_object)(ValueData* data);
    // tag must not be 0
    // p_shared_ptr must be a pointer to std::shared_ptr<void>
    void (*set_foreign_object)(ValueData* data, void* p_shared_ptr, int tag);
    int (*get_boolean)(ValueData* data, int* value);
    int (*get_number)(ValueData* data, int64_t* value);
    int (*get_string)(ValueData* data, char* value, size_t buf_size, size_t* size);
    int (*get_string_callback)(ValueData* data, void* user, void (*callback)(const char*, size_t, void*));
    int (*get_bytes)(ValueData* data, char* value, size_t buf_size, size_t* size);
    int (*get_bytes_callback)(ValueData* data, void* user, void (*callback)(const char*, size_t, void*));
    int (*get_foreign_object)(ValueData* data, void* p_shared_ptr, int* tag);
    ValueData* (*get_array_element)(ValueData* data, size_t index, int may_append);
    ValueData* (*get_object_element)(ValueData* data, const char* key, int may_create);
    FuncData* (*get_func)(ValueData* data);
    FuncDataAsync* (*get_async_func)(ForeignContext* ctx, FuncData* data);
    ValueData* (*get_temporary)(ForeignContext* data);
    void (*push_arg)(ForeignContext* data, ValueData* value);
    void (*call)(ForeignContext* data, FuncData* func);
    ValueData* (*get_call_result)(ForeignContext* data);
    void (*abort)(ForeignContext* data, const char* reason);
    size_t (*get_callback_phase)(ForeignContext* data);
};
}