#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ValdiRustHandle ValdiRustHandle;

typedef struct ValdiRustSlice {
    const uint8_t* data;
    size_t len;
} ValdiRustSlice;

/** A byte slice whose owner is a borrowed Valdi ValueTypedArray. */
typedef struct ValdiRustBytesView {
    const uint8_t* data;
    size_t len;
    void* owner;
} ValdiRustBytesView;

typedef struct ValdiRustArray {
    const void* data;
    size_t len;
} ValdiRustArray;

/** A native call backed by one stack-owned Valdi::Marshaller. */
typedef struct ValdiRustCall {
    void* marshaller;
    size_t argument_count;
} ValdiRustCall;

typedef struct ValdiRustObjectEntry {
    ValdiRustSlice key;
    const void* value;
} ValdiRustObjectEntry;

typedef struct ValdiRustBuffer {
    uint8_t* data;
    size_t len;
    size_t capacity;
} ValdiRustBuffer;

/** A UTF-8 string that is borrowed when possible and owned when conversion is required. */
typedef struct ValdiRustString {
    ValdiRustSlice slice;
    ValdiRustBuffer owned;
} ValdiRustString;

typedef struct ValdiRustStatus {
    int32_t code;
    ValdiRustBuffer message;
} ValdiRustStatus;

typedef ValdiRustStatus (*ValdiRustValueInvokeFunction)(void* instance, ValdiRustCall call);
typedef void (*ValdiRustValueDropFunction)(void* instance);
typedef ValdiRustStatus (*ValdiRustProxyInvokeFunction)(void* instance, size_t method, ValdiRustCall call);
typedef void* (*ValdiRustSchemaFunction)(void);
typedef void* (*ValdiRustModuleCreateFunction)(void);
typedef void (*ValdiRustModuleDropFunction)(void* instance);
typedef ValdiRustStatus (*ValdiRustModuleInvokeFunction)(void* instance, size_t method, ValdiRustCall call);
typedef ValdiRustStatus (*ValdiRustMarshallerFunction)(void* context, void* marshaller);
typedef ValdiRustStatus (*ValdiRustMarshallerReadFunction)(void* context, const void* marshaller, size_t index);
typedef void (*ValdiRustPromiseSuccessFunction)(void* context, const void* value);
typedef void (*ValdiRustPromiseErrorFunction)(void* context, ValdiRustSlice message);

void valdi_rust_handle_retain(ValdiRustHandle* handle);
void valdi_rust_handle_release(ValdiRustHandle* handle);
size_t valdi_rust_handle_live_count(void);
void valdi_rust_buffer_drop(ValdiRustBuffer buffer);
ValdiRustBuffer valdi_rust_buffer_clone(ValdiRustSlice slice);
ValdiRustStatus valdi_rust_status_error(ValdiRustSlice message);

typedef enum ValdiRustValueKind {
    ValdiRustValueKindNull = 0,
    ValdiRustValueKindUndefined = 1,
    ValdiRustValueKindBool = 2,
    ValdiRustValueKindNumber = 3,
    ValdiRustValueKindString = 4,
    ValdiRustValueKindBytes = 5,
    ValdiRustValueKindObject = 6,
    ValdiRustValueKindLong = 7,
    ValdiRustValueKindBigInt = 8,
} ValdiRustValueKind;

typedef enum ValdiRustSchemaKind {
    ValdiRustSchemaKindClass = 0,
    ValdiRustSchemaKindGeneric = 1,
    ValdiRustSchemaKindEnum = 2,
} ValdiRustSchemaKind;

ValdiRustStatus valdi_rust_register_schema(ValdiRustSlice schema,
                                           ValdiRustSchemaKind kind,
                                           ValdiRustArray dependencies,
                                           void** out);
ValdiRustStatus valdi_rust_register_module(ValdiRustSlice module_path,
                                           void* schema,
                                           size_t method_count,
                                           ValdiRustModuleCreateFunction create,
                                           ValdiRustModuleDropFunction drop,
                                           ValdiRustModuleInvokeFunction invoke);
ValdiRustStatus valdi_rust_value_new_proxy(void* schema,
                                           size_t method_count,
                                           void* instance,
                                           ValdiRustProxyInvokeFunction invoke,
                                           ValdiRustValueDropFunction drop,
                                           ValdiRustHandle** out);
ValdiRustStatus valdi_rust_resolve_exported_function(
    void* runtime, void* native_objects_manager, ValdiRustSlice module_path, void* schema, ValdiRustHandle** out);

ValdiRustStatus valdi_rust_value_from_marshaller(void* context,
                                                 ValdiRustMarshallerFunction push_value,
                                                 ValdiRustHandle** out);
const void* valdi_rust_value_borrow(const ValdiRustHandle* value);
ValdiRustStatus valdi_rust_value_clone_ref(const void* value, ValdiRustHandle** out);
const void* valdi_rust_marshaller_get(const void* marshaller, size_t index);
void valdi_rust_marshaller_push_null(void* marshaller);
void valdi_rust_marshaller_push_undefined(void* marshaller);
void valdi_rust_marshaller_push_bool(void* marshaller, uint8_t value);
void valdi_rust_marshaller_push_number(void* marshaller, double value);
void valdi_rust_marshaller_push_long(void* marshaller, int64_t value);
void valdi_rust_marshaller_push_string(void* marshaller, ValdiRustSlice value);
ValdiRustStatus valdi_rust_marshaller_push_bytes(void* marshaller, ValdiRustSlice value);
ValdiRustStatus valdi_rust_marshaller_push_owned_bytes(void* marshaller, ValdiRustBuffer value);
void valdi_rust_marshaller_push_bytes_view(void* marshaller, void* owner);
void valdi_rust_marshaller_push_value(void* marshaller, const void* value);
size_t valdi_rust_marshaller_push_array(void* marshaller, size_t length);
ValdiRustStatus valdi_rust_marshaller_set_array_item(void* marshaller, size_t array, size_t item);
size_t valdi_rust_marshaller_push_map(void* marshaller, size_t capacity);
ValdiRustStatus valdi_rust_marshaller_put_map_property(void* marshaller, size_t map, ValdiRustSlice key);
ValdiRustValueKind valdi_rust_value_ref_kind(const void* value);
ValdiRustStatus valdi_rust_value_ref_get_bool(const void* value, uint8_t* out);
ValdiRustStatus valdi_rust_value_ref_get_number(const void* value, double* out);
ValdiRustStatus valdi_rust_value_ref_get_long(const void* value, int64_t* out);
ValdiRustStatus valdi_rust_value_ref_get_string(const void* value, ValdiRustString* out);
ValdiRustStatus valdi_rust_value_ref_get_bytes(const void* value, ValdiRustSlice* out);
ValdiRustStatus valdi_rust_value_ref_get_bytes_view(const void* value, ValdiRustBytesView* out);
void* valdi_rust_bigint_from_i64(int64_t value);
ValdiRustStatus valdi_rust_bigint_parse(ValdiRustSlice decimal, void** out);
void valdi_rust_bigint_retain(void* value);
void valdi_rust_bigint_release(void* value);
uint8_t valdi_rust_bigint_to_i64(const void* value, int64_t* out);
ValdiRustBuffer valdi_rust_bigint_to_string(const void* value);
ValdiRustStatus valdi_rust_value_ref_get_bigint(const void* value, void** out);
ValdiRustHandle* valdi_rust_bigint_to_value(void* value);
void valdi_rust_marshaller_push_bigint(void* marshaller, void* value);

void valdi_rust_bytes_view_retain(void* owner);
void valdi_rust_bytes_view_release(void* owner);
ValdiRustStatus valdi_rust_value_ref_get_property(const void* value, ValdiRustSlice key, const void** out);
ValdiRustStatus valdi_rust_value_ref_map_len(const void* value, size_t* out);
ValdiRustStatus valdi_rust_value_map_set(ValdiRustHandle* value, ValdiRustSlice key, const void* entry);
ValdiRustStatus valdi_rust_value_ref_array_len(const void* value, size_t* out);
ValdiRustStatus valdi_rust_value_ref_array_get(const void* value, size_t index, const void** out);
ValdiRustStatus valdi_rust_value_new_array(ValdiRustArray values, ValdiRustHandle** out);
ValdiRustStatus valdi_rust_value_new_object(ValdiRustArray entries, ValdiRustHandle** out);
ValdiRustStatus valdi_rust_value_new_function(void* instance,
                                              ValdiRustValueInvokeFunction invoke,
                                              ValdiRustValueDropFunction drop,
                                              ValdiRustHandle** out);
ValdiRustStatus valdi_rust_value_call_function(const void* function,
                                               void* context,
                                               ValdiRustMarshallerFunction push_arguments,
                                               ValdiRustMarshallerReadFunction read_result);
ValdiRustStatus valdi_rust_value_new_promise(ValdiRustHandle** promise, ValdiRustHandle** completer);
ValdiRustStatus valdi_rust_value_observe_promise(const void* promise,
                                                 void* context,
                                                 ValdiRustPromiseSuccessFunction success,
                                                 ValdiRustPromiseErrorFunction error,
                                                 ValdiRustValueDropFunction drop_context);
ValdiRustStatus valdi_rust_value_resolve_promise(ValdiRustHandle* completer,
                                                 void* context,
                                                 ValdiRustMarshallerFunction push_value);
ValdiRustStatus valdi_rust_value_reject_promise(ValdiRustHandle* completer, ValdiRustSlice message);

#ifdef __cplusplus
}
#endif
