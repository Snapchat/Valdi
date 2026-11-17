use crate::ffi::{valdi_rust_handle_release, valdi_rust_handle_retain};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::ptr::NonNull;
use std::rc::Rc;

pub struct OwnedHandle<T> {
    raw: NonNull<c_void>,
    marker: PhantomData<(T, Rc<()>)>,
}

impl<T> OwnedHandle<T> {
    /// # Safety
    ///
    /// `raw` must be a newly owned Valdi Rust handle of the matching generated type.
    pub unsafe fn from_raw(raw: *mut c_void) -> Self {
        Self {
            raw: NonNull::new(raw).expect("Valdi returned a null owned handle"),
            marker: PhantomData,
        }
    }

    pub fn as_ptr(&self) -> *mut c_void {
        self.raw.as_ptr()
    }

    pub fn into_raw(self) -> *mut c_void {
        let raw = self.raw.as_ptr();
        std::mem::forget(self);
        raw
    }
}

impl<T> Clone for OwnedHandle<T> {
    fn clone(&self) -> Self {
        unsafe { valdi_rust_handle_retain(self.raw.as_ptr()) };
        Self {
            raw: self.raw,
            marker: PhantomData,
        }
    }
}

impl<T> Drop for OwnedHandle<T> {
    fn drop(&mut self) {
        unsafe { valdi_rust_handle_release(self.raw.as_ptr()) };
    }
}
