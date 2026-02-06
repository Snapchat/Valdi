#pragma once

#include "djinni_c.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef djinni_ref djinni_future_ref;
typedef djinni_ref djinni_promise_ref;

typedef void (*djinni_future_callback)(void *opaque, djinni_ref value,
                                       djinni_string_ref error);

djinni_promise_ref djinni_promise_new();
void djinni_promise_resolve(djinni_promise_ref promise, djinni_ref value);
void djinni_promise_reject(djinni_promise_ref promise, djinni_string_ref error);

djinni_future_ref djinni_promise_get_future(djinni_promise_ref promise);

void djinni_future_on_complete(djinni_future_ref future, void *opaque,
                               djinni_opaque_deallocator opaque_deallocator,
                               djinni_future_callback callback);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus