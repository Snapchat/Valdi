use std::ffi::c_void;
use std::marker::PhantomData;
use std::ptr::NonNull;
use std::rc::Rc;

#[derive(Clone, Copy)]
pub struct RuntimeRef<'a> {
    runtime: NonNull<c_void>,
    native_objects_manager: NonNull<c_void>,
    marker: PhantomData<(&'a (), Rc<()>)>,
}

impl<'a> RuntimeRef<'a> {
    /// # Safety
    ///
    /// `runtime` must point to a live Valdi `JSRuntime`, and
    /// `native_objects_manager` must point to a live C++ `shared_ptr` for the
    /// matching runtime. Both must outlive `'a`.
    pub unsafe fn from_raw(runtime: *mut c_void, native_objects_manager: *mut c_void) -> Self {
        Self {
            runtime: NonNull::new(runtime).expect("Valdi runtime is null"),
            native_objects_manager: NonNull::new(native_objects_manager)
                .expect("Valdi native objects manager is null"),
            marker: PhantomData,
        }
    }

    pub fn runtime_ptr(self) -> *mut c_void {
        self.runtime.as_ptr()
    }

    pub fn native_objects_manager_ptr(self) -> *mut c_void {
        self.native_objects_manager.as_ptr()
    }
}
