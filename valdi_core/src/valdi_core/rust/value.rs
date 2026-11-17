use crate::ffi::*;
use crate::{
    Marshaller, OwnedHandle, ValdiArray, ValdiObjectEntry, ValdiSlice, ValdiStatus, ffi_boundary,
};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::ptr::NonNull;
use std::rc::Rc;

pub enum ValueTag {}

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ValueKind {
    Null = 0,
    Undefined = 1,
    Bool = 2,
    Number = 3,
    String = 4,
    Bytes = 5,
    Object = 6,
    Long = 7,
    BigInt = 8,
}

#[derive(Clone)]
pub(crate) enum InlineValue {
    Null,
    Undefined,
    Bool(bool),
    Number(f64),
    Long(i64),
    String(String),
    Bytes(Vec<u8>),
}

impl InlineValue {
    pub(crate) fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        match self {
            Self::Null => marshaller.push_null(),
            Self::Undefined => marshaller.push_undefined(),
            Self::Bool(value) => marshaller.push_bool(*value),
            Self::Number(value) => marshaller.push_number(*value),
            Self::Long(value) => marshaller.push_long(*value),
            Self::String(value) => marshaller.push_string(value),
            Self::Bytes(value) => return marshaller.push_bytes(value),
        }
        Ok(())
    }
}

pub(crate) enum ValueStorage {
    Inline(InlineValue, std::cell::OnceCell<OwnedHandle<ValueTag>>),
    Handle(OwnedHandle<ValueTag>),
}

pub struct Value(pub(crate) ValueStorage);
#[derive(Clone, Copy)]
pub struct ValueRef<'a> {
    raw: NonNull<c_void>,
    marker: PhantomData<(&'a c_void, Rc<()>)>,
}

impl Value {
    pub fn null() -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Null))
    }

    pub fn undefined() -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Undefined))
    }

    pub fn from_bool(value: bool) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Bool(value)))
    }

    pub fn from_number(value: f64) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Number(value)))
    }

    pub fn from_long(value: i64) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Long(value)))
    }

    fn inline(value: InlineValue) -> Self {
        Self(ValueStorage::Inline(value, std::cell::OnceCell::new()))
    }

    fn create_inline_handle(value: &InlineValue) -> OwnedHandle<ValueTag> {
        unsafe extern "C" fn push_value(context: *mut c_void, raw: *mut c_void) -> ValdiStatus {
            ffi_boundary(|| {
                let value = unsafe { &*context.cast::<InlineValue>() };
                let mut marshaller = Marshaller {
                    raw,
                    marker: PhantomData,
                };
                value.push_to(&mut marshaller)
            })
        }

        let mut out = std::mem::MaybeUninit::uninit();
        let status = unsafe {
            valdi_rust_value_from_marshaller(
                (value as *const InlineValue).cast_mut().cast(),
                push_value,
                out.as_mut_ptr(),
            )
        };
        status
            .into_result()
            .expect("creating an inline Valdi value cannot fail");
        unsafe { OwnedHandle::from_raw(out.assume_init()) }
    }

    pub fn from_string(value: &str) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::String(value.to_owned())))
    }

    pub fn from_bytes(value: &[u8]) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Bytes(value.to_vec())))
    }

    pub fn from_owned_bytes(value: Vec<u8>) -> Result<Self, String> {
        Ok(Self::inline(InlineValue::Bytes(value)))
    }

    /// # Safety
    ///
    /// `raw` must be a newly owned Valdi value handle.
    pub unsafe fn from_raw(raw: *mut c_void) -> Self {
        Self(ValueStorage::Handle(unsafe { OwnedHandle::from_raw(raw) }))
    }

    /// # Safety
    ///
    /// `raw` must remain valid for the returned wrapper's lifetime.
    pub unsafe fn borrow_raw<'a>(raw: *mut c_void) -> ValueRef<'a> {
        let value = unsafe { valdi_rust_value_borrow(raw) };
        unsafe { ValueRef::from_raw(value) }
    }

    pub fn into_raw(self) -> *mut c_void {
        match self.0 {
            ValueStorage::Handle(handle) => handle.into_raw(),
            ValueStorage::Inline(value, materialized) => materialized
                .into_inner()
                .unwrap_or_else(|| Self::create_inline_handle(&value))
                .into_raw(),
        }
    }

    pub fn as_ptr(&self) -> *mut c_void {
        match &self.0 {
            ValueStorage::Handle(handle) => handle.as_ptr(),
            ValueStorage::Inline(value, materialized) => materialized
                .get_or_init(|| Self::create_inline_handle(value))
                .as_ptr(),
        }
    }

    pub fn as_borrowed(&self) -> ValueRef<'_> {
        let value = unsafe { valdi_rust_value_borrow(self.as_ptr()) };
        unsafe { ValueRef::from_raw(value) }
    }

    pub fn kind(&self) -> ValueKind {
        match &self.0 {
            ValueStorage::Inline(InlineValue::Null, _) => ValueKind::Null,
            ValueStorage::Inline(InlineValue::Undefined, _) => ValueKind::Undefined,
            ValueStorage::Inline(InlineValue::Bool(_), _) => ValueKind::Bool,
            ValueStorage::Inline(InlineValue::Number(_), _) => ValueKind::Number,
            ValueStorage::Inline(InlineValue::Long(_), _) => ValueKind::Long,
            ValueStorage::Inline(InlineValue::String(_), _) => ValueKind::String,
            ValueStorage::Inline(InlineValue::Bytes(_), _) => ValueKind::Bytes,
            ValueStorage::Handle(_) => self.as_borrowed().kind(),
        }
    }

    pub fn as_bool(&self) -> Result<bool, String> {
        match &self.0 {
            ValueStorage::Inline(InlineValue::Bool(value), _) => Ok(*value),
            _ => self.as_borrowed().as_bool(),
        }
    }

    pub fn as_number(&self) -> Result<f64, String> {
        match &self.0 {
            ValueStorage::Inline(InlineValue::Number(value), _) => Ok(*value),
            _ => self.as_borrowed().as_number(),
        }
    }

    pub fn as_long(&self) -> Result<i64, String> {
        match &self.0 {
            ValueStorage::Inline(InlineValue::Long(value), _) => Ok(*value),
            _ => self.as_borrowed().as_long(),
        }
    }

    pub fn as_string(&self) -> Result<String, String> {
        match &self.0 {
            ValueStorage::Inline(InlineValue::String(value), _) => Ok(value.clone()),
            _ => self.as_borrowed().as_string(),
        }
    }

    pub fn as_bytes(&self) -> Result<Vec<u8>, String> {
        match &self.0 {
            ValueStorage::Inline(InlineValue::Bytes(value), _) => Ok(value.clone()),
            _ => self.as_borrowed().as_bytes(),
        }
    }

    pub fn array(values: &[ValueRef<'_>]) -> Result<Self, String> {
        let values = values.iter().map(ValueRef::as_ptr).collect::<Vec<_>>();
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_new_array(ValdiArray::from_slice(&values), out.as_mut_ptr()) }
            .into_result()?;
        Ok(unsafe { Self::from_raw(out.assume_init()) })
    }

    pub fn object(entries: &[(&str, ValueRef<'_>)]) -> Result<Self, String> {
        let entries = entries
            .iter()
            .map(|(key, value)| ValdiObjectEntry {
                key: ValdiSlice::from_bytes(key.as_bytes()),
                value: value.as_ptr(),
            })
            .collect::<Vec<_>>();
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_new_object(ValdiArray::from_slice(&entries), out.as_mut_ptr()) }
            .into_result()?;
        Ok(unsafe { Self::from_raw(out.assume_init()) })
    }
}

impl Clone for Value {
    fn clone(&self) -> Self {
        match &self.0 {
            ValueStorage::Inline(value, _) => Self::inline(value.clone()),
            ValueStorage::Handle(handle) => Self(ValueStorage::Handle(handle.clone())),
        }
    }
}

impl<'a> ValueRef<'a> {
    /// # Safety
    ///
    /// `raw` must point to a live C++ `Valdi::Value` for the returned lifetime.
    pub unsafe fn from_raw(raw: *const c_void) -> Self {
        ValueRef {
            raw: NonNull::new(raw.cast_mut()).expect("Valdi returned a null value reference"),
            marker: PhantomData,
        }
    }

    pub fn as_ptr(&self) -> *const c_void {
        self.raw.as_ptr()
    }

    pub fn to_owned(&self) -> Result<Value, String> {
        match self.kind() {
            ValueKind::Null => return Value::null(),
            ValueKind::Undefined => return Value::undefined(),
            ValueKind::Bool => return Value::from_bool(self.as_bool()?),
            ValueKind::Number => return Value::from_number(self.as_number()?),
            ValueKind::Long => return Value::from_long(self.as_long()?),
            ValueKind::String => return Value::from_string(self.as_str()?.as_ref()),
            ValueKind::Bytes => return Value::from_bytes(self.as_bytes_slice()?),
            ValueKind::Object | ValueKind::BigInt => {}
        }
        let mut out = std::mem::MaybeUninit::uninit();
        let status = unsafe { valdi_rust_value_clone_ref(self.as_ptr(), out.as_mut_ptr()) };
        status.into_result()?;
        Ok(unsafe { Value::from_raw(out.assume_init()) })
    }

    pub fn kind(&self) -> ValueKind {
        match unsafe { valdi_rust_value_ref_kind(self.as_ptr()) } {
            0 => ValueKind::Null,
            1 => ValueKind::Undefined,
            2 => ValueKind::Bool,
            3 => ValueKind::Number,
            4 => ValueKind::String,
            5 => ValueKind::Bytes,
            7 => ValueKind::Long,
            8 => ValueKind::BigInt,
            _ => ValueKind::Object,
        }
    }

    pub fn as_bool(&self) -> Result<bool, String> {
        let mut out = 0;
        unsafe { valdi_rust_value_ref_get_bool(self.as_ptr(), &mut out) }.into_result()?;
        Ok(out != 0)
    }

    pub fn as_number(&self) -> Result<f64, String> {
        let mut out = 0.0;
        unsafe { valdi_rust_value_ref_get_number(self.as_ptr(), &mut out) }.into_result()?;
        Ok(out)
    }

    pub fn as_long(&self) -> Result<i64, String> {
        let mut out = 0;
        unsafe { valdi_rust_value_ref_get_long(self.as_ptr(), &mut out) }.into_result()?;
        Ok(out)
    }

    pub fn as_string(&self) -> Result<String, String> {
        Ok((*self).as_str()?.into_owned())
    }

    pub fn as_bytes(&self) -> Result<Vec<u8>, String> {
        Ok(self.as_bytes_slice()?.to_vec())
    }

    pub fn as_str(self) -> Result<std::borrow::Cow<'a, str>, String> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_ref_get_string(self.as_ptr(), out.as_mut_ptr()) }
            .into_result()?;
        unsafe { out.assume_init().into_cow() }
    }

    pub fn as_bytes_slice(self) -> Result<&'a [u8], String> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_ref_get_bytes(self.as_ptr(), out.as_mut_ptr()) }.into_result()?;
        Ok(unsafe { out.assume_init().as_bytes() })
    }

    pub fn property(self, key: &str) -> Result<ValueRef<'a>, String> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe {
            valdi_rust_value_ref_get_property(
                self.as_ptr(),
                ValdiSlice::from_bytes(key.as_bytes()),
                out.as_mut_ptr(),
            )
        }
        .into_result()?;
        Ok(unsafe { ValueRef::from_raw(out.assume_init()) })
    }

    pub fn array_len(&self) -> Result<usize, String> {
        let mut out = 0;
        unsafe { valdi_rust_value_ref_array_len(self.as_ptr(), &mut out) }.into_result()?;
        Ok(out)
    }

    pub fn array_get(self, index: usize) -> Result<ValueRef<'a>, String> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_ref_array_get(self.as_ptr(), index, out.as_mut_ptr()) }
            .into_result()?;
        Ok(unsafe { ValueRef::from_raw(out.assume_init()) })
    }
}
