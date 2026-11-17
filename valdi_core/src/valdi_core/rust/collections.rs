use crate::ffi::{valdi_rust_value_map_set, valdi_rust_value_ref_map_len};
use crate::{
    Marshaller, ValdiDeserialize, ValdiPush, ValdiSerialize, ValdiSlice, ValdiToOwned, Value,
    ValueKind, ValueRef,
};
use std::ffi::c_void;
use std::marker::PhantomData;

pub struct Array<T>(Vec<T>);

impl<T> Array<T> {
    pub fn from_vec(value: Vec<T>) -> Self {
        Self(value)
    }

    pub fn into_vec(self) -> Vec<T> {
        self.0
    }
}

impl<T> std::ops::Deref for Array<T> {
    type Target = [T];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> From<Vec<T>> for Array<T> {
    fn from(value: Vec<T>) -> Self {
        Self::from_vec(value)
    }
}

impl<'a, T: ValdiDeserialize<'a>> ValdiDeserialize<'a> for Array<T> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        let len = value.array_len()?;
        let mut result = Vec::with_capacity(len);
        for index in 0..len {
            result.push(T::from_valdi(value.array_get(index)?)?);
        }
        Ok(Self(result))
    }
}

impl<T: ValdiSerialize> ValdiSerialize for Array<T> {
    fn to_valdi(&self) -> Result<Value, String> {
        let values = self
            .iter()
            .map(ValdiSerialize::to_valdi)
            .collect::<Result<Vec<_>, _>>()?;
        let refs = values.iter().map(Value::as_borrowed).collect::<Vec<_>>();
        Value::array(&refs)
    }
}

impl<T: ValdiPush> ValdiPush for Array<T> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        let array = marshaller.push_array(self.len());
        for (index, value) in self.iter().enumerate() {
            value.push_to(marshaller)?;
            marshaller.set_array_item(array, index)?;
        }
        Ok(())
    }
}

impl<'a> ValdiDeserialize<'a> for Vec<u8> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_bytes()
    }
}

impl ValdiSerialize for Vec<u8> {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_bytes(self)
    }
}

impl ValdiPush for Vec<u8> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_bytes(self)
    }
}

impl ValdiToOwned for &[u8] {
    type Owned = Vec<u8>;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok(self.to_vec())
    }
}

#[derive(Clone, Copy)]
pub struct StringRef<'a>(ValueRef<'a>);

impl<'a> StringRef<'a> {
    pub fn as_str(self) -> Result<std::borrow::Cow<'a, str>, String> {
        self.0.as_str()
    }

    pub fn to_owned(self) -> Result<String, String> {
        self.0.as_string()
    }
}

impl<'a> ValdiDeserialize<'a> for StringRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value))
    }
}

impl ValdiPush for StringRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_value(self.0);
        Ok(())
    }
}

impl ValdiToOwned for StringRef<'_> {
    type Owned = String;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        self.0.as_string()
    }
}

#[derive(Clone, Copy)]
pub struct ArrayRef<'a, T> {
    value: ValueRef<'a>,
    marker: PhantomData<T>,
}

impl<'a, T: ValdiDeserialize<'a>> ArrayRef<'a, T> {
    pub fn len(&self) -> usize {
        self.value
            .array_len()
            .expect("ArrayRef always contains a Valdi array")
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn get(&self, index: usize) -> Result<T, String> {
        T::from_valdi(self.value.array_get(index)?)
    }
}

impl<'a, T> ArrayRef<'a, T>
where
    T: ValdiDeserialize<'a> + ValdiToOwned,
{
    pub fn to_vec(&self) -> Result<Vec<T::Owned>, String> {
        let len = self.len();
        let mut result = Vec::with_capacity(len);
        for index in 0..len {
            result.push(self.get(index)?.to_owned_value()?);
        }
        Ok(result)
    }
}

impl<'a, T> ValdiToOwned for ArrayRef<'a, T>
where
    T: ValdiDeserialize<'a> + ValdiToOwned,
{
    type Owned = Array<T::Owned>;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        self.to_vec().map(Array::from_vec)
    }
}

impl<'a, T> ValdiDeserialize<'a> for ArrayRef<'a, T> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.array_len()?;
        Ok(Self {
            value,
            marker: PhantomData,
        })
    }
}

impl<T> ValdiPush for ArrayRef<'_, T> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_value(self.value);
        Ok(())
    }
}

pub struct Map<K, V>(Value, PhantomData<(K, V)>);
pub struct MapRef<'a, K, V>(ValueRef<'a>, PhantomData<(K, V)>);

impl<K, V> Map<K, V> {
    pub fn as_borrowed(&self) -> MapRef<'_, K, V> {
        MapRef(self.0.as_borrowed(), PhantomData)
    }
}

impl<K, V> Clone for Map<K, V> {
    fn clone(&self) -> Self {
        Self(self.0.clone(), PhantomData)
    }
}

impl<K, V> MapRef<'_, K, V> {
    pub fn as_ptr(&self) -> *const c_void {
        self.0.as_ptr()
    }
}

impl Map<String, Value> {
    pub fn new(entries: &[(&str, ValueRef<'_>)]) -> Result<Self, String> {
        Ok(Self(Value::object(entries)?, PhantomData))
    }

    pub fn len(&self) -> usize {
        self.as_borrowed().len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn get(&self, key: &str) -> Result<Option<Value>, String> {
        self.as_borrowed().get(key)
    }

    pub fn set(&mut self, key: &str, value: ValueRef<'_>) -> Result<(), String> {
        unsafe {
            valdi_rust_value_map_set(
                self.0.as_ptr(),
                ValdiSlice::from_bytes(key.as_bytes()),
                value.as_ptr(),
            )
        }
        .into_result()
    }
}

impl MapRef<'_, String, Value> {
    pub fn len(&self) -> usize {
        let mut out = 0;
        unsafe { valdi_rust_value_ref_map_len(self.0.as_ptr(), &mut out) }
            .into_result()
            .expect("MapRef always contains a Valdi map");
        out
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn to_owned(&self) -> Result<Map<String, Value>, String> {
        Ok(Map(self.0.to_owned()?, PhantomData))
    }

    pub fn get(&self, key: &str) -> Result<Option<Value>, String> {
        let value = self.0.property(key)?;
        if value.kind() == ValueKind::Undefined {
            Ok(None)
        } else {
            Ok(Some(value.to_owned()?))
        }
    }
}

impl<'a> ValdiDeserialize<'a> for Map<String, Value> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value.to_owned()?, PhantomData))
    }
}

impl<'a> ValdiDeserialize<'a> for MapRef<'a, String, Value> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value, PhantomData))
    }
}

impl ValdiSerialize for Map<String, Value> {
    fn to_valdi(&self) -> Result<Value, String> {
        Ok(self.0.clone())
    }
}

impl ValdiSerialize for MapRef<'_, String, Value> {
    fn to_valdi(&self) -> Result<Value, String> {
        self.0.to_owned()
    }
}

impl ValdiPush for Map<String, Value> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}

impl ValdiPush for MapRef<'_, String, Value> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}

impl ValdiToOwned for MapRef<'_, String, Value> {
    type Owned = Map<String, Value>;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        self.to_owned()
    }
}
