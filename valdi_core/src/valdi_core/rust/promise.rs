use crate::ffi::*;
use crate::{
    Marshaller, OwnedHandle, ValdiDeserialize, ValdiPush, ValdiSerialize, ValdiSlice, ValdiStatus,
    Value, ValueRef, ffi_boundary,
};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::panic::{AssertUnwindSafe, catch_unwind};

pub enum PromiseCompleterTag {}
pub struct Promise(Value);
#[derive(Clone, Copy)]
pub struct PromiseRef<'a>(ValueRef<'a>);
pub struct PromiseCompleter(OwnedHandle<PromiseCompleterTag>);

// ResolvablePromise synchronizes completion and is atomically ref-counted.
unsafe impl Send for PromiseCompleter {}

impl Promise {
    pub fn pending() -> Result<(Self, PromiseCompleter), String> {
        let mut promise = std::mem::MaybeUninit::uninit();
        let mut completer = std::mem::MaybeUninit::uninit();
        unsafe { valdi_rust_value_new_promise(promise.as_mut_ptr(), completer.as_mut_ptr()) }
            .into_result()?;
        Ok((
            Self(unsafe { Value::from_raw(promise.assume_init()) }),
            PromiseCompleter(unsafe { OwnedHandle::from_raw(completer.assume_init()) }),
        ))
    }

    pub fn as_borrowed(&self) -> PromiseRef<'_> {
        PromiseRef(self.0.as_borrowed())
    }

    pub fn then<F>(&self, callback: F) -> Result<(), String>
    where
        F: FnOnce(Result<Value, String>) + Send + 'static,
    {
        self.as_borrowed().then(callback)
    }
}

impl Clone for Promise {
    fn clone(&self) -> Self {
        Self(self.0.clone())
    }
}

impl PromiseRef<'_> {
    pub fn to_owned(&self) -> Result<Promise, String> {
        Ok(Promise(self.0.to_owned()?))
    }

    pub fn then<F>(&self, callback: F) -> Result<(), String>
    where
        F: FnOnce(Result<Value, String>) + Send + 'static,
    {
        self.then_with(move |result| callback(result.and_then(|value| value.to_owned())))
    }

    pub fn then_with<F>(&self, callback: F) -> Result<(), String>
    where
        F: for<'a> FnOnce(Result<ValueRef<'a>, String>) + Send + 'static,
    {
        unsafe extern "C" fn success<F>(context: *mut c_void, value: *const c_void)
        where
            F: for<'a> FnOnce(Result<ValueRef<'a>, String>) + Send + 'static,
        {
            let _ = catch_unwind(AssertUnwindSafe(|| {
                let callback = unsafe { &mut *(context as *mut Option<F>) };
                if let Some(callback) = callback.take() {
                    callback(Ok(unsafe { ValueRef::from_raw(value) }));
                }
            }));
        }

        unsafe extern "C" fn error<F>(context: *mut c_void, message: ValdiSlice)
        where
            F: for<'a> FnOnce(Result<ValueRef<'a>, String>) + Send + 'static,
        {
            let _ = catch_unwind(AssertUnwindSafe(|| {
                let callback = unsafe { &mut *(context as *mut Option<F>) };
                if let Some(callback) = callback.take() {
                    callback(Err(
                        String::from_utf8_lossy(unsafe { message.as_bytes() }).into_owned()
                    ));
                }
            }));
        }

        unsafe extern "C" fn drop_context<F>(context: *mut c_void)
        where
            F: for<'a> FnOnce(Result<ValueRef<'a>, String>) + Send + 'static,
        {
            let _ = catch_unwind(AssertUnwindSafe(|| {
                unsafe { drop(Box::from_raw(context as *mut Option<F>)) };
            }));
        }

        let context = Box::into_raw(Box::new(Some(callback))).cast();
        unsafe {
            valdi_rust_value_observe_promise(
                self.0.as_ptr(),
                context,
                success::<F>,
                error::<F>,
                drop_context::<F>,
            )
        }
        .into_result()
    }
}

impl PromiseCompleter {
    pub fn resolve(self, value: ValueRef<'_>) -> Result<(), String> {
        self.resolve_with(|marshaller| {
            marshaller.push_value(value);
            Ok(())
        })
    }

    pub fn resolve_with<P>(self, push_value: P) -> Result<(), String>
    where
        P: FnOnce(&mut Marshaller<'_>) -> Result<(), String>,
    {
        unsafe extern "C" fn push<P>(context: *mut c_void, raw: *mut c_void) -> ValdiStatus
        where
            P: FnOnce(&mut Marshaller<'_>) -> Result<(), String>,
        {
            ffi_boundary(|| {
                let push_value = unsafe { &mut *(context as *mut Option<P>) }
                    .take()
                    .expect("Valdi future value marshaller called more than once");
                let mut marshaller = Marshaller {
                    raw,
                    marker: PhantomData,
                };
                push_value(&mut marshaller)
            })
        }

        let mut push_value = Some(push_value);
        unsafe {
            valdi_rust_value_resolve_promise(
                self.0.as_ptr(),
                (&mut push_value as *mut Option<P>).cast(),
                push::<P>,
            )
        }
        .into_result()
    }

    pub fn reject(self, message: &str) -> Result<(), String> {
        unsafe {
            valdi_rust_value_reject_promise(
                self.0.as_ptr(),
                ValdiSlice::from_bytes(message.as_bytes()),
            )
        }
        .into_result()
    }
}

impl<'a> ValdiDeserialize<'a> for Promise {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value.to_owned()?))
    }
}

impl<'a> ValdiDeserialize<'a> for PromiseRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value))
    }
}

impl ValdiSerialize for Promise {
    fn to_valdi(&self) -> Result<Value, String> {
        Ok(self.0.clone())
    }
}

impl ValdiSerialize for PromiseRef<'_> {
    fn to_valdi(&self) -> Result<Value, String> {
        self.0.to_owned()
    }
}

impl ValdiPush for Promise {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}

impl ValdiPush for PromiseRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}
