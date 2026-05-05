#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef void *djinni_ref;

typedef djinni_ref djinni_string_ref;
typedef djinni_ref djinni_binary_ref;
typedef djinni_ref djinni_number_ref;
typedef djinni_ref djinni_array_ref;
typedef djinni_ref djinni_keyval_array_ref;
typedef djinni_ref djinni_date_ref;

typedef djinni_ref djinni_record_ref;
typedef djinni_ref djinni_interface_ref;
typedef djinni_ref djinni_proxy_class_ref;

typedef void (*djinni_binary_deallocator)(uint8_t *, size_t, void *);
typedef void (*djinni_opaque_deallocator)(void *);

typedef void (*djinni_exception_callback)(void *, const char *);

void djinni_ref_retain(djinni_ref ref);
void djinni_ref_release(djinni_ref ref);

djinni_string_ref djinni_string_new(const char *str, size_t length);

const char *djinni_string_get_data(djinni_string_ref str);

size_t djinni_string_get_length(djinni_string_ref str);

djinni_binary_ref djinni_binary_new(size_t length);

djinni_binary_ref
djinni_binary_new_with_bytes(uint8_t *data, size_t length, void *opaque,
                             djinni_binary_deallocator deallocator);

djinni_binary_ref djinni_binary_new_with_bytes_copy(const uint8_t *data,
                                                    size_t length);

uint8_t *djinni_binary_get_data(djinni_binary_ref binary);
size_t djinni_binary_get_length(djinni_binary_ref binary);

djinni_number_ref djinni_number_int64_new(int64_t v);
djinni_number_ref djinni_number_uint64_new(uint64_t v);
djinni_number_ref djinni_number_double_new(double v);

int64_t djinni_number_get_int64(djinni_number_ref number);
uint64_t djinni_number_get_uint64(djinni_number_ref number);
double djinni_number_get_double(djinni_number_ref number);

djinni_keyval_array_ref djinni_keyval_array_new(size_t size);

size_t djinni_keyval_array_get_length(djinni_keyval_array_ref keyval_array);
djinni_ref djinni_keyval_array_get_key(djinni_keyval_array_ref keyval_array,
                                       size_t index);
djinni_ref djinni_keyval_array_get_value(djinni_keyval_array_ref keyval_array,
                                         size_t index);

void djinni_keyval_array_set_entry(djinni_keyval_array_ref keyval_array,
                                   size_t index, djinni_ref key,
                                   djinni_ref value);

djinni_array_ref djinni_array_new(size_t length);
size_t djinni_array_get_length(djinni_array_ref array);
djinni_ref djinni_array_get_value(djinni_array_ref array, size_t index);
void djinni_array_set_value(djinni_array_ref array, size_t index,
                            djinni_ref value);

djinni_date_ref djinni_date_new(uint64_t epoch_time_ms);
uint64_t djinni_date_get_epoch(djinni_date_ref date);

typedef struct {
    void *opaque;
    djinni_exception_callback callback;
    void *__reserved__;
} djinni_exception_handler;

void djinni_exception_handler_push(djinni_exception_handler *handler);
void djinni_exception_handler_pop();

void djinni_exception_notify(const char *error);

#define DJINNI_OPTIONAL_PRIMITIVE(__name__, __type__)                          \
  typedef struct {                                                             \
    __type__ value;                                                            \
    bool has_value;                                                            \
  } djinni_optional_##__name__;                                                \
                                                                               \
  inline djinni_optional_##__name__ djinni_optional_##__name__##_make(         \
      __type__ value) {                                                        \
    djinni_optional_##__name__ output;                                         \
    output.value = value;                                                      \
    output.has_value = true;                                                   \
    return output;                                                             \
  }                                                                            \
  inline djinni_optional_##__name__ djinni_optional_##__name__##_empty() {     \
    djinni_optional_##__name__ output;                                         \
    output.value = 0;                                                          \
    output.has_value = false;                                                  \
    return output;                                                             \
  }

DJINNI_OPTIONAL_PRIMITIVE(bool, bool);
DJINNI_OPTIONAL_PRIMITIVE(int8, int8_t);
DJINNI_OPTIONAL_PRIMITIVE(int16, int16_t);
DJINNI_OPTIONAL_PRIMITIVE(int32, int32_t);
DJINNI_OPTIONAL_PRIMITIVE(int64, int64_t);
DJINNI_OPTIONAL_PRIMITIVE(float, float);
DJINNI_OPTIONAL_PRIMITIVE(double, double);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
