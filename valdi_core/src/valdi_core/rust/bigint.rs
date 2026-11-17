use crate::ffi::*;
use crate::{
    Marshaller, ValdiDeserialize, ValdiPush, ValdiSerialize, ValdiSlice, ValdiToOwned, Value,
    ValueRef,
};
use std::ffi::c_void;
use std::fmt;
use std::marker::PhantomData;
use std::ptr::NonNull;
use std::rc::Rc;
use std::str::FromStr;

/// Owns one reference to an immutable C++ BigInt.
pub struct BigInt {
    raw: NonNull<c_void>,
    marker: PhantomData<Rc<()>>,
}

#[derive(Clone, Copy)]
pub struct BigIntRef<'a> {
    raw: NonNull<c_void>,
    marker: PhantomData<(&'a c_void, Rc<()>)>,
}

impl BigInt {
    pub fn from_i64(value: i64) -> Self {
        unsafe { Self::from_retained(valdi_rust_bigint_from_i64(value)) }
    }

    unsafe fn from_retained(raw: *mut c_void) -> Self {
        Self {
            raw: NonNull::new(raw).expect("Valdi returned a null BigInt"),
            marker: PhantomData,
        }
    }

    pub fn as_borrowed(&self) -> BigIntRef<'_> {
        BigIntRef {
            raw: self.raw,
            marker: PhantomData,
        }
    }

    pub fn to_i64(&self) -> Option<i64> {
        self.as_borrowed().to_i64()
    }
}

impl FromStr for BigInt {
    type Err = String;

    fn from_str(decimal: &str) -> Result<Self, Self::Err> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe {
            valdi_rust_bigint_parse(ValdiSlice::from_bytes(decimal.as_bytes()), out.as_mut_ptr())
        }
        .into_result()?;
        Ok(unsafe { Self::from_retained(out.assume_init()) })
    }
}

impl Clone for BigInt {
    fn clone(&self) -> Self {
        self.as_borrowed().to_owned()
    }
}

impl Drop for BigInt {
    fn drop(&mut self) {
        unsafe { valdi_rust_bigint_release(self.raw.as_ptr()) };
    }
}

impl BigIntRef<'_> {
    pub fn to_i64(self) -> Option<i64> {
        let mut out = 0;
        if unsafe { valdi_rust_bigint_to_i64(self.raw.as_ptr(), &mut out) } != 0 {
            Some(out)
        } else {
            None
        }
    }

    pub fn to_owned(self) -> BigInt {
        unsafe {
            valdi_rust_bigint_retain(self.raw.as_ptr());
            BigInt::from_retained(self.raw.as_ptr())
        }
    }
}

impl fmt::Display for BigIntRef<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if let Some(value) = self.to_i64() {
            return write!(f, "{value}");
        }
        let bytes = unsafe { valdi_rust_bigint_to_string(self.raw.as_ptr()).into_vec() };
        // BigInt's native decimal representation is always ASCII.
        f.write_str(std::str::from_utf8(&bytes).expect("BigInt decimal is not ASCII"))
    }
}

impl fmt::Display for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_borrowed().fmt(f)
    }
}

impl<'a> ValdiDeserialize<'a> for BigIntRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_ref_get_bigint(value.as_ptr(), out.as_mut_ptr()) }
            .into_result()?;
        Ok(Self {
            raw: NonNull::new(unsafe { out.assume_init() }).expect("Valdi returned a null BigInt"),
            marker: PhantomData,
        })
    }
}

impl<'a> ValdiDeserialize<'a> for BigInt {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(BigIntRef::from_valdi(value)?.to_owned())
    }
}

impl ValdiSerialize for BigIntRef<'_> {
    fn to_valdi(&self) -> Result<Value, String> {
        Ok(unsafe { Value::from_raw(valdi_rust_bigint_to_value(self.raw.as_ptr())) })
    }
}

impl ValdiSerialize for BigInt {
    fn to_valdi(&self) -> Result<Value, String> {
        self.as_borrowed().to_valdi()
    }
}

impl ValdiPush for BigIntRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        unsafe { valdi_rust_marshaller_push_bigint(marshaller.raw, self.raw.as_ptr()) };
        Ok(())
    }
}

impl ValdiPush for BigInt {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.as_borrowed().push_to(marshaller)
    }
}

impl ValdiToOwned for BigIntRef<'_> {
    type Owned = BigInt;

    fn to_owned_value(&self) -> Result<BigInt, String> {
        Ok((*self).to_owned())
    }
}

impl ValdiToOwned for BigInt {
    type Owned = BigInt;

    fn to_owned_value(&self) -> Result<BigInt, String> {
        Ok(self.clone())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ValueKind;

    #[test]
    fn exact_conversion_and_invalid_input() {
        for value in [0, -1, i64::MIN, i64::MAX] {
            let number = BigInt::from_i64(value);
            assert_eq!(number.to_i64(), Some(value));
            assert_eq!(
                number.to_string().parse::<BigInt>().unwrap().to_i64(),
                Some(value)
            );
        }
        assert!("1.5".parse::<BigInt>().is_err());
    }

    #[test]
    fn large_value_survives_owner_drop_and_value_round_trip() {
        let integer: BigInt = "18446744073709551616".parse().unwrap();
        let copy = integer.clone();
        drop(integer);
        assert_eq!(copy.to_i64(), None);
        let value = copy.to_valdi().unwrap();
        drop(copy);
        assert_eq!(value.as_borrowed().kind(), ValueKind::BigInt);
        let restored = BigIntRef::from_valdi(value.as_borrowed())
            .unwrap()
            .to_owned();
        drop(value);
        assert_eq!(restored.to_string(), "18446744073709551616");
        assert!(BigInt::from_valdi(Value::from_long(1).unwrap().as_borrowed()).is_err());
    }
}
