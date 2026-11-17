use crate::ffi::*;
use crate::{ValdiBuffer, ValdiPush, ValdiSlice, ValueRef};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::rc::Rc;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ValdiCall {
    marshaller: *mut c_void,
    argument_count: usize,
}

pub struct MarshallerRef<'a> {
    pub(crate) raw: *const c_void,
    pub(crate) len: usize,
    pub(crate) marker: PhantomData<(&'a c_void, Rc<()>)>,
}

pub struct Marshaller<'a> {
    pub(crate) raw: *mut c_void,
    pub(crate) marker: PhantomData<(&'a mut c_void, Rc<()>)>,
}

impl ValdiCall {
    /// # Safety
    ///
    /// The native marshaller must remain alive and unmodified while the
    /// returned argument view is in use.
    pub unsafe fn arguments<'a>(&'a self) -> MarshallerRef<'a> {
        MarshallerRef {
            raw: self.marshaller,
            len: self.argument_count,
            marker: PhantomData,
        }
    }

    /// # Safety
    ///
    /// No argument view or value borrowed from it may be used after requesting
    /// the result writer because appending can move the marshaller's storage.
    pub unsafe fn result<'a>(&'a mut self) -> Marshaller<'a> {
        Marshaller {
            raw: self.marshaller,
            marker: PhantomData,
        }
    }
}

impl<'a> MarshallerRef<'a> {
    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn get(&self, index: usize) -> Result<ValueRef<'a>, String> {
        if index >= self.len {
            return Err(format!("missing Valdi argument at index {index}"));
        }
        let value = unsafe { valdi_rust_marshaller_get(self.raw, index) };
        Ok(unsafe { ValueRef::from_raw(value) })
    }
}

impl Marshaller<'_> {
    pub fn push<T: ValdiPush + ?Sized>(&mut self, value: &T) -> Result<(), String> {
        value.push_to(self)
    }

    pub fn push_null(&mut self) {
        unsafe { valdi_rust_marshaller_push_null(self.raw) };
    }

    pub fn push_undefined(&mut self) {
        unsafe { valdi_rust_marshaller_push_undefined(self.raw) };
    }

    pub fn push_bool(&mut self, value: bool) {
        unsafe { valdi_rust_marshaller_push_bool(self.raw, u8::from(value)) };
    }

    pub fn push_number(&mut self, value: f64) {
        unsafe { valdi_rust_marshaller_push_number(self.raw, value) };
    }

    pub fn push_long(&mut self, value: i64) {
        unsafe { valdi_rust_marshaller_push_long(self.raw, value) };
    }

    pub fn push_string(&mut self, value: &str) {
        unsafe {
            valdi_rust_marshaller_push_string(self.raw, ValdiSlice::from_bytes(value.as_bytes()))
        };
    }

    pub fn push_bytes(&mut self, value: &[u8]) -> Result<(), String> {
        unsafe { valdi_rust_marshaller_push_bytes(self.raw, ValdiSlice::from_bytes(value)) }
            .into_result()
    }

    pub fn push_owned_bytes(&mut self, value: Vec<u8>) -> Result<(), String> {
        unsafe { valdi_rust_marshaller_push_owned_bytes(self.raw, ValdiBuffer::from_vec(value)) }
            .into_result()
    }

    pub(crate) fn push_bytes_view(&mut self, owner: *mut c_void) {
        unsafe { valdi_rust_marshaller_push_bytes_view(self.raw, owner) };
    }

    pub fn push_value(&mut self, value: ValueRef<'_>) {
        unsafe { valdi_rust_marshaller_push_value(self.raw, value.as_ptr()) };
    }

    pub fn push_array(&mut self, length: usize) -> usize {
        unsafe { valdi_rust_marshaller_push_array(self.raw, length) }
    }

    pub fn set_array_item(&mut self, array: usize, item: usize) -> Result<(), String> {
        unsafe { valdi_rust_marshaller_set_array_item(self.raw, array, item) }.into_result()
    }

    pub fn push_map(&mut self, capacity: usize) -> usize {
        unsafe { valdi_rust_marshaller_push_map(self.raw, capacity) }
    }

    pub fn put_map_property(&mut self, map: usize, key: &str) -> Result<(), String> {
        unsafe {
            valdi_rust_marshaller_put_map_property(
                self.raw,
                map,
                ValdiSlice::from_bytes(key.as_bytes()),
            )
        }
        .into_result()
    }
}
