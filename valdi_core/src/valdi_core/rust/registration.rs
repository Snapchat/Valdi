use crate::ffi::*;
use crate::{RuntimeRef, ValdiArray, ValdiCall, ValdiSlice, ValdiStatus, Value};
use std::ffi::c_void;
use std::panic::{AssertUnwindSafe, catch_unwind};

#[repr(i32)]
#[derive(Clone, Copy)]
pub enum SchemaKind {
    Class = 0,
    Generic = 1,
    Enum = 2,
}

pub type SchemaFunction = extern "C" fn() -> *mut c_void;
pub type ModuleCreateFunction = extern "C" fn() -> *mut c_void;
pub type ModuleDropFunction = unsafe extern "C" fn(*mut c_void);
pub type ModuleInvokeFunction = unsafe extern "C" fn(*mut c_void, usize, ValdiCall) -> ValdiStatus;
pub type ProxyInvokeFunction = unsafe extern "C" fn(*mut c_void, usize, ValdiCall) -> ValdiStatus;
pub type DropFunction = unsafe extern "C" fn(*mut c_void);

pub fn register_schema_once(
    storage: &std::sync::OnceLock<usize>,
    schema: &str,
    kind: SchemaKind,
    dependencies: &[SchemaFunction],
) -> *mut c_void {
    let value = storage.get_or_init(|| {
        let mut out = std::mem::MaybeUninit::uninit();
        let status = unsafe {
            valdi_rust_register_schema(
                ValdiSlice::from_bytes(schema.as_bytes()),
                kind,
                ValdiArray::from_slice(dependencies),
                out.as_mut_ptr(),
            )
        };
        match status.into_result() {
            Ok(()) => unsafe { out.assume_init() as usize },
            Err(_) => 0,
        }
    });
    *value as *mut c_void
}

pub fn register_module(
    module_path: &str,
    schema: *mut c_void,
    method_count: usize,
    create: ModuleCreateFunction,
    drop: ModuleDropFunction,
    invoke: ModuleInvokeFunction,
) -> Result<(), String> {
    unsafe {
        valdi_rust_register_module(
            ValdiSlice::from_bytes(module_path.as_bytes()),
            schema,
            method_count,
            create,
            drop,
            invoke,
        )
    }
    .into_result()
}

pub fn run_static_initializer(body: impl FnOnce() -> Result<(), String>) {
    if !matches!(catch_unwind(AssertUnwindSafe(body)), Ok(Ok(()))) {
        std::process::abort();
    }
}

pub fn new_proxy(
    schema: *mut c_void,
    method_count: usize,
    instance: *mut c_void,
    invoke: ProxyInvokeFunction,
    drop: DropFunction,
) -> Result<Value, String> {
    let mut out = std::mem::MaybeUninit::uninit();
    unsafe {
        valdi_rust_value_new_proxy(
            schema,
            method_count,
            instance,
            invoke,
            drop,
            out.as_mut_ptr(),
        )
    }
    .into_result()?;
    Ok(unsafe { Value::from_raw(out.assume_init()) })
}

pub fn resolve_exported_function(
    runtime: RuntimeRef<'_>,
    module_path: &str,
    schema: *mut c_void,
) -> Result<Value, String> {
    let mut out = std::mem::MaybeUninit::uninit();
    unsafe {
        valdi_rust_resolve_exported_function(
            runtime.runtime_ptr(),
            runtime.native_objects_manager_ptr(),
            ValdiSlice::from_bytes(module_path.as_bytes()),
            schema,
            out.as_mut_ptr(),
        )
    }
    .into_result()?;
    Ok(unsafe { Value::from_raw(out.assume_init()) })
}
