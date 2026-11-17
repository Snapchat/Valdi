use std::ffi::c_void;
use std::panic::{AssertUnwindSafe, catch_unwind};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ValdiSlice {
    pub data: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct ValdiBytesView {
    pub(crate) data: *const u8,
    pub(crate) len: usize,
    pub(crate) owner: *mut c_void,
}

impl ValdiSlice {
    pub fn from_bytes(value: &[u8]) -> Self {
        Self {
            data: value.as_ptr(),
            len: value.len(),
        }
    }

    /// # Safety
    ///
    /// `data` must remain valid for reads of `len` bytes for the returned lifetime.
    pub unsafe fn as_bytes<'a>(self) -> &'a [u8] {
        if self.len == 0 {
            return &[];
        }
        assert!(!self.data.is_null());
        unsafe { std::slice::from_raw_parts(self.data, self.len) }
    }

    /// # Safety
    ///
    /// The slice must contain valid UTF-8 and satisfy `as_bytes`'s requirements.
    pub unsafe fn as_str<'a>(self) -> &'a str {
        unsafe { std::str::from_utf8_unchecked(self.as_bytes()) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ValdiArray {
    pub data: *const c_void,
    pub len: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct ValdiObjectEntry {
    pub(crate) key: ValdiSlice,
    pub(crate) value: *const c_void,
}

impl ValdiArray {
    pub fn from_slice<T>(value: &[T]) -> Self {
        Self {
            data: value.as_ptr().cast(),
            len: value.len(),
        }
    }

    /// # Safety
    ///
    /// `data` must remain valid and correctly aligned for `len` values of `T`
    /// for the returned lifetime.
    pub unsafe fn as_slice<'a, T>(self) -> &'a [T] {
        if self.len == 0 {
            return &[];
        }
        assert!(!self.data.is_null());
        unsafe { std::slice::from_raw_parts(self.data.cast(), self.len) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ValdiBuffer {
    pub data: *mut u8,
    pub len: usize,
    pub capacity: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ValdiString {
    pub slice: ValdiSlice,
    pub owned: ValdiBuffer,
}

impl ValdiString {
    /// # Safety
    ///
    /// A borrowed slice must remain valid for `'a`; an owned buffer must have
    /// been allocated by the Rust buffer ABI and not consumed previously.
    pub unsafe fn into_cow<'a>(self) -> Result<std::borrow::Cow<'a, str>, String> {
        if self.owned.capacity != 0 {
            return String::from_utf8(unsafe { self.owned.into_vec() })
                .map(std::borrow::Cow::Owned)
                .map_err(|error| String::from_utf8_lossy(error.as_bytes()).into_owned());
        }
        std::str::from_utf8(unsafe { self.slice.as_bytes() })
            .map(std::borrow::Cow::Borrowed)
            .map_err(|error| error.to_string())
    }
}

impl ValdiBuffer {
    pub fn empty() -> Self {
        Self {
            data: std::ptr::null_mut(),
            len: 0,
            capacity: 0,
        }
    }

    pub fn from_vec(mut value: Vec<u8>) -> Self {
        let buffer = Self {
            data: value.as_mut_ptr(),
            len: value.len(),
            capacity: value.capacity(),
        };
        std::mem::forget(value);
        buffer
    }

    /// # Safety
    ///
    /// The buffer must have been created by `ValdiBuffer::from_vec`, and its
    /// ownership must not have been consumed previously.
    pub unsafe fn into_vec(self) -> Vec<u8> {
        if self.capacity == 0 {
            return Vec::new();
        }
        assert!(!self.data.is_null());
        unsafe { Vec::from_raw_parts(self.data, self.len, self.capacity) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn valdi_rust_buffer_drop(buffer: ValdiBuffer) {
    if buffer.capacity != 0 {
        assert!(!buffer.data.is_null());
        unsafe {
            drop(Vec::from_raw_parts(
                buffer.data,
                buffer.len,
                buffer.capacity,
            ));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn valdi_rust_buffer_clone(slice: ValdiSlice) -> ValdiBuffer {
    ValdiBuffer::from_vec(unsafe { slice.as_bytes() }.to_vec())
}

#[repr(C)]
pub struct ValdiStatus {
    pub code: i32,
    pub message: ValdiBuffer,
}

macro_rules! valdi_option {
    ($name:ident, $value:ty) => {
        #[repr(C)]
        #[derive(Clone, Copy)]
        pub struct $name {
            pub is_some: u8,
            pub value: $value,
        }
    };
}

valdi_option!(ValdiOptionU8, u8);
valdi_option!(ValdiOptionI32, i32);
valdi_option!(ValdiOptionI64, i64);
valdi_option!(ValdiOptionF64, f64);
valdi_option!(ValdiOptionSlice, ValdiSlice);
valdi_option!(ValdiOptionBuffer, ValdiBuffer);

impl ValdiStatus {
    pub fn ok() -> Self {
        Self {
            code: 0,
            message: ValdiBuffer::empty(),
        }
    }

    pub fn error(message: String) -> Self {
        Self {
            code: 1,
            message: ValdiBuffer::from_vec(message.into_bytes()),
        }
    }

    pub fn into_result(self) -> Result<(), String> {
        if self.code == 0 {
            debug_assert_eq!(self.message.capacity, 0);
            return Ok(());
        }
        match String::from_utf8(unsafe { self.message.into_vec() }) {
            Ok(message) => Err(message),
            Err(error) => Err(String::from_utf8_lossy(error.as_bytes()).into_owned()),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn valdi_rust_status_error(message: ValdiSlice) -> ValdiStatus {
    ValdiStatus::error(String::from_utf8_lossy(unsafe { message.as_bytes() }).into_owned())
}

pub fn ffi_boundary(body: impl FnOnce() -> Result<(), String>) -> ValdiStatus {
    match catch_unwind(AssertUnwindSafe(body)) {
        Ok(Ok(())) => ValdiStatus::ok(),
        Ok(Err(message)) => ValdiStatus::error(message),
        Err(payload) => {
            let message = if let Some(message) = payload.downcast_ref::<&str>() {
                (*message).to_owned()
            } else if let Some(message) = payload.downcast_ref::<String>() {
                message.clone()
            } else {
                "Rust panic crossed a Valdi native call".to_owned()
            };
            ValdiStatus::error(message)
        }
    }
}
