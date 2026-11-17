use crate::{
    DropFunction, ModuleCreateFunction, ModuleDropFunction, ModuleInvokeFunction,
    ProxyInvokeFunction, SchemaKind, ValdiArray, ValdiBuffer, ValdiBytesView, ValdiCall,
    ValdiSlice, ValdiStatus, ValdiString,
};
use std::ffi::c_void;

unsafe extern "C" {
    pub(crate) fn valdi_rust_handle_retain(handle: *mut c_void);
    pub(crate) fn valdi_rust_handle_release(handle: *mut c_void);
}

unsafe extern "C" {
    pub(crate) fn valdi_rust_register_schema(
        schema: ValdiSlice,
        kind: SchemaKind,
        dependencies: ValdiArray,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_register_module(
        module_path: ValdiSlice,
        schema: *mut c_void,
        method_count: usize,
        create: ModuleCreateFunction,
        drop: ModuleDropFunction,
        invoke: ModuleInvokeFunction,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_new_proxy(
        schema: *mut c_void,
        method_count: usize,
        instance: *mut c_void,
        invoke: ProxyInvokeFunction,
        drop: DropFunction,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_resolve_exported_function(
        runtime: *mut c_void,
        native_objects_manager: *mut c_void,
        module_path: ValdiSlice,
        schema: *mut c_void,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_from_marshaller(
        context: *mut c_void,
        push_value: unsafe extern "C" fn(*mut c_void, *mut c_void) -> ValdiStatus,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_borrow(value: *const c_void) -> *const c_void;
    pub(crate) fn valdi_rust_value_clone_ref(
        value: *const c_void,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_marshaller_get(
        marshaller: *const c_void,
        index: usize,
    ) -> *const c_void;
    pub(crate) fn valdi_rust_marshaller_push_null(marshaller: *mut c_void);
    pub(crate) fn valdi_rust_marshaller_push_undefined(marshaller: *mut c_void);
    pub(crate) fn valdi_rust_marshaller_push_bool(marshaller: *mut c_void, value: u8);
    pub(crate) fn valdi_rust_marshaller_push_number(marshaller: *mut c_void, value: f64);
    pub(crate) fn valdi_rust_marshaller_push_long(marshaller: *mut c_void, value: i64);
    pub(crate) fn valdi_rust_marshaller_push_string(marshaller: *mut c_void, value: ValdiSlice);
    pub(crate) fn valdi_rust_marshaller_push_bytes(
        marshaller: *mut c_void,
        value: ValdiSlice,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_marshaller_push_owned_bytes(
        marshaller: *mut c_void,
        value: ValdiBuffer,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_marshaller_push_bytes_view(
        marshaller: *mut c_void,
        owner: *mut c_void,
    );
    pub(crate) fn valdi_rust_marshaller_push_value(marshaller: *mut c_void, value: *const c_void);
    pub(crate) fn valdi_rust_marshaller_push_array(marshaller: *mut c_void, length: usize)
    -> usize;
    pub(crate) fn valdi_rust_marshaller_set_array_item(
        marshaller: *mut c_void,
        array: usize,
        item: usize,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_marshaller_push_map(marshaller: *mut c_void, capacity: usize)
    -> usize;
    pub(crate) fn valdi_rust_marshaller_put_map_property(
        marshaller: *mut c_void,
        map: usize,
        key: ValdiSlice,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_kind(value: *const c_void) -> i32;
    pub(crate) fn valdi_rust_value_ref_get_bool(value: *const c_void, out: *mut u8) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_get_number(
        value: *const c_void,
        out: *mut f64,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_get_long(value: *const c_void, out: *mut i64)
    -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_get_string(
        value: *const c_void,
        out: *mut ValdiString,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_get_bytes(
        value: *const c_void,
        out: *mut ValdiSlice,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_get_bytes_view(
        value: *const c_void,
        out: *mut ValdiBytesView,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_bytes_view_retain(owner: *mut c_void);
    pub(crate) fn valdi_rust_bytes_view_release(owner: *mut c_void);
    pub(crate) fn valdi_rust_value_ref_get_property(
        value: *const c_void,
        key: ValdiSlice,
        out: *mut *const c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_array_len(
        value: *const c_void,
        out: *mut usize,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_ref_array_get(
        value: *const c_void,
        index: usize,
        out: *mut *const c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_new_array(
        values: ValdiArray,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_new_object(
        values: ValdiArray,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_new_function(
        instance: *mut c_void,
        invoke: unsafe extern "C" fn(*mut c_void, ValdiCall) -> ValdiStatus,
        drop_instance: unsafe extern "C" fn(*mut c_void),
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_call_function(
        function: *const c_void,
        context: *mut c_void,
        push_arguments: unsafe extern "C" fn(*mut c_void, *mut c_void) -> ValdiStatus,
        read_result: unsafe extern "C" fn(*mut c_void, *const c_void, usize) -> ValdiStatus,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_new_promise(
        promise: *mut *mut c_void,
        completer: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_observe_promise(
        promise: *const c_void,
        context: *mut c_void,
        success: unsafe extern "C" fn(*mut c_void, *const c_void),
        error: unsafe extern "C" fn(*mut c_void, ValdiSlice),
        drop_context: unsafe extern "C" fn(*mut c_void),
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_resolve_promise(
        completer: *mut c_void,
        context: *mut c_void,
        push_value: unsafe extern "C" fn(*mut c_void, *mut c_void) -> ValdiStatus,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_reject_promise(
        completer: *mut c_void,
        message: ValdiSlice,
    ) -> ValdiStatus;
}

unsafe extern "C" {
    pub(crate) fn valdi_rust_value_ref_map_len(
        value: *const c_void,
        out: *mut usize,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_value_map_set(
        value: *mut c_void,
        key: ValdiSlice,
        entry: *const c_void,
    ) -> ValdiStatus;
}

unsafe extern "C" {
    pub(crate) fn valdi_rust_bigint_from_i64(value: i64) -> *mut c_void;
    pub(crate) fn valdi_rust_bigint_parse(
        decimal: ValdiSlice,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_bigint_retain(value: *mut c_void);
    pub(crate) fn valdi_rust_bigint_release(value: *mut c_void);
    pub(crate) fn valdi_rust_bigint_to_i64(value: *const c_void, out: *mut i64) -> u8;
    pub(crate) fn valdi_rust_bigint_to_string(value: *const c_void) -> ValdiBuffer;
    pub(crate) fn valdi_rust_value_ref_get_bigint(
        value: *const c_void,
        out: *mut *mut c_void,
    ) -> ValdiStatus;
    pub(crate) fn valdi_rust_bigint_to_value(value: *mut c_void) -> *mut c_void;
    pub(crate) fn valdi_rust_marshaller_push_bigint(marshaller: *mut c_void, value: *mut c_void);
}
