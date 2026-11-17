use crate::ffi::{
    valdi_rust_bytes_view_release, valdi_rust_bytes_view_retain,
    valdi_rust_value_ref_get_bytes_view,
};
use crate::{
    Marshaller, ValdiDeserialize, ValdiPush, ValdiSerialize, ValdiToOwned, Value, ValueRef,
};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::ops::Deref;
use std::ptr::NonNull;
use std::rc::Rc;

#[derive(Clone, Copy)]
pub struct BytesRef<'a> {
    data: *const u8,
    len: usize,
    owner: Option<NonNull<c_void>>,
    marker: PhantomData<(&'a [u8], Rc<()>)>,
}

impl<'a> BytesRef<'a> {
    pub fn from_slice(value: &'a [u8]) -> Self {
        Self {
            data: value.as_ptr(),
            len: value.len(),
            owner: None,
            marker: PhantomData,
        }
    }

    pub fn to_owned(self) -> BytesView {
        match self.owner {
            Some(owner) => {
                unsafe { valdi_rust_bytes_view_retain(owner.as_ptr()) };
                BytesView::from_retained(self.data, self.len, owner)
            }
            None => BytesView::from_vec(self.to_vec()),
        }
    }
}

impl Deref for BytesRef<'_> {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        if self.len == 0 {
            return &[];
        }
        unsafe { std::slice::from_raw_parts(self.data, self.len) }
    }
}

impl AsRef<[u8]> for BytesRef<'_> {
    fn as_ref(&self) -> &[u8] {
        self
    }
}

impl<'a> From<&'a [u8]> for BytesRef<'a> {
    fn from(value: &'a [u8]) -> Self {
        Self::from_slice(value)
    }
}

impl<'a> ValdiDeserialize<'a> for BytesRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        let mut view = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_ref_get_bytes_view(value.as_ptr(), view.as_mut_ptr()) }
            .into_result()?;
        let view = unsafe { view.assume_init() };
        Ok(Self {
            data: view.data,
            len: view.len,
            owner: Some(NonNull::new(view.owner).expect("Valdi byte view has no owner")),
            marker: PhantomData,
        })
    }
}

impl ValdiPush for BytesRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        match self.owner {
            Some(owner) => marshaller.push_bytes_view(owner.as_ptr()),
            None => marshaller.push_bytes(self)?,
        }
        Ok(())
    }
}

impl ValdiSerialize for BytesRef<'_> {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_bytes(self)
    }
}

impl ValdiToOwned for BytesRef<'_> {
    type Owned = BytesView;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok((*self).to_owned())
    }
}

struct RetainedBytes {
    data: *const u8,
    len: usize,
    owner: NonNull<c_void>,
    marker: PhantomData<Rc<()>>,
}

impl Clone for RetainedBytes {
    fn clone(&self) -> Self {
        unsafe { valdi_rust_bytes_view_retain(self.owner.as_ptr()) };
        Self {
            data: self.data,
            len: self.len,
            owner: self.owner,
            marker: PhantomData,
        }
    }
}

impl Drop for RetainedBytes {
    fn drop(&mut self) {
        unsafe { valdi_rust_bytes_view_release(self.owner.as_ptr()) };
    }
}

enum BytesStorage {
    Rust(Vec<u8>),
    Valdi(RetainedBytes),
}

pub struct BytesView(BytesStorage);

impl BytesView {
    pub fn from_vec(value: Vec<u8>) -> Self {
        Self(BytesStorage::Rust(value))
    }

    fn from_retained(data: *const u8, len: usize, owner: NonNull<c_void>) -> Self {
        Self(BytesStorage::Valdi(RetainedBytes {
            data,
            len,
            owner,
            marker: PhantomData,
        }))
    }

    pub fn as_borrowed(&self) -> BytesRef<'_> {
        match &self.0 {
            BytesStorage::Rust(value) => BytesRef::from_slice(value),
            BytesStorage::Valdi(value) => BytesRef {
                data: value.data,
                len: value.len,
                owner: Some(value.owner),
                marker: PhantomData,
            },
        }
    }

    pub fn into_vec(self) -> Vec<u8> {
        match self.0 {
            BytesStorage::Rust(value) => value,
            BytesStorage::Valdi(value) => value.as_slice().to_vec(),
        }
    }

    pub fn push_owned_to(self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        match self.0 {
            BytesStorage::Rust(value) => marshaller.push_owned_bytes(value),
            BytesStorage::Valdi(value) => {
                marshaller.push_bytes_view(value.owner.as_ptr());
                Ok(())
            }
        }
    }
}

impl RetainedBytes {
    fn as_slice(&self) -> &[u8] {
        if self.len == 0 {
            return &[];
        }
        unsafe { std::slice::from_raw_parts(self.data, self.len) }
    }
}

impl Clone for BytesView {
    fn clone(&self) -> Self {
        match &self.0 {
            BytesStorage::Rust(value) => Self::from_vec(value.clone()),
            BytesStorage::Valdi(value) => Self(BytesStorage::Valdi(value.clone())),
        }
    }
}

impl Deref for BytesView {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        match &self.0 {
            BytesStorage::Rust(value) => value,
            BytesStorage::Valdi(value) => value.as_slice(),
        }
    }
}

impl AsRef<[u8]> for BytesView {
    fn as_ref(&self) -> &[u8] {
        self
    }
}

impl From<Vec<u8>> for BytesView {
    fn from(value: Vec<u8>) -> Self {
        Self::from_vec(value)
    }
}

impl<'a> ValdiDeserialize<'a> for BytesView {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(BytesRef::from_valdi(value)?.to_owned())
    }
}

impl ValdiSerialize for BytesView {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_bytes(self)
    }
}

impl ValdiPush for BytesView {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.as_borrowed().push_to(marshaller)
    }
}
