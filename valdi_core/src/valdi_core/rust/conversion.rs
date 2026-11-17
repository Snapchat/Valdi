use crate::{Marshaller, Value, ValueKind, ValueRef, ValueStorage};

pub trait ValdiDeserialize<'a>: Sized {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String>;
}

pub trait ValdiSerialize {
    fn to_valdi(&self) -> Result<Value, String>;
}

pub trait ValdiPush {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String>;
}

pub trait ValdiToOwned {
    type Owned;

    fn to_owned_value(&self) -> Result<Self::Owned, String>;
}

pub fn deserialize_optional<'a, T>(
    value: ValueRef<'a>,
    deserialize: impl FnOnce(ValueRef<'a>) -> Result<T, String>,
) -> Result<Option<T>, String> {
    match value.kind() {
        ValueKind::Null | ValueKind::Undefined => Ok(None),
        _ => deserialize(value).map(Some),
    }
}

impl<'a> ValdiDeserialize<'a> for bool {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_bool()
    }
}

impl ValdiSerialize for bool {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_bool(*self)
    }
}

impl ValdiPush for bool {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_bool(*self);
        Ok(())
    }
}

impl ValdiToOwned for bool {
    type Owned = bool;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok(*self)
    }
}

impl<'a> ValdiDeserialize<'a> for f64 {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_number()
    }
}

impl ValdiSerialize for f64 {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_number(*self)
    }
}

impl ValdiPush for f64 {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_number(*self);
        Ok(())
    }
}

impl ValdiToOwned for f64 {
    type Owned = f64;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok(*self)
    }
}

impl<'a> ValdiDeserialize<'a> for i64 {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_long()
    }
}

impl ValdiSerialize for i64 {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_long(*self)
    }
}

impl ValdiPush for i64 {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_long(*self);
        Ok(())
    }
}

impl ValdiToOwned for i64 {
    type Owned = i64;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok(*self)
    }
}

impl<'a> ValdiDeserialize<'a> for String {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_string()
    }
}

impl<'a> ValdiDeserialize<'a> for &'a [u8] {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.as_bytes_slice()
    }
}

impl ValdiSerialize for String {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_string(self)
    }
}

impl ValdiSerialize for str {
    fn to_valdi(&self) -> Result<Value, String> {
        Value::from_string(self)
    }
}

impl ValdiPush for str {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_string(self);
        Ok(())
    }
}

impl ValdiPush for String {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.as_str().push_to(marshaller)
    }
}

impl ValdiToOwned for String {
    type Owned = String;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        Ok(self.clone())
    }
}

impl<'a> ValdiDeserialize<'a> for ValueRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(value)
    }
}

impl<'a> ValdiDeserialize<'a> for Value {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        value.to_owned()
    }
}

impl ValdiSerialize for Value {
    fn to_valdi(&self) -> Result<Value, String> {
        Ok(self.clone())
    }
}

impl ValdiSerialize for ValueRef<'_> {
    fn to_valdi(&self) -> Result<Value, String> {
        self.to_owned()
    }
}

impl ValdiPush for Value {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        match &self.0 {
            ValueStorage::Inline(value, _) => return value.push_to(marshaller),
            ValueStorage::Handle(_) => marshaller.push_value(self.as_borrowed()),
        }
        Ok(())
    }
}

impl ValdiPush for ValueRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        marshaller.push_value(*self);
        Ok(())
    }
}

impl ValdiToOwned for ValueRef<'_> {
    type Owned = Value;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        self.to_owned()
    }
}

impl<T: ValdiSerialize + ?Sized> ValdiSerialize for &T {
    fn to_valdi(&self) -> Result<Value, String> {
        (*self).to_valdi()
    }
}

impl<T: ValdiPush + ?Sized> ValdiPush for &T {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        (*self).push_to(marshaller)
    }
}

impl<'a, T: ValdiDeserialize<'a>> ValdiDeserialize<'a> for Option<T> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        deserialize_optional(value, T::from_valdi)
    }
}

impl<T: ValdiSerialize> ValdiSerialize for Option<T> {
    fn to_valdi(&self) -> Result<Value, String> {
        match self {
            Some(value) => value.to_valdi(),
            None => Value::undefined(),
        }
    }
}

impl<T: ValdiPush> ValdiPush for Option<T> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        match self {
            Some(value) => value.push_to(marshaller),
            None => {
                marshaller.push_undefined();
                Ok(())
            }
        }
    }
}

impl<T: ValdiToOwned> ValdiToOwned for Option<T> {
    type Owned = Option<T::Owned>;

    fn to_owned_value(&self) -> Result<Self::Owned, String> {
        self.as_ref().map(ValdiToOwned::to_owned_value).transpose()
    }
}
