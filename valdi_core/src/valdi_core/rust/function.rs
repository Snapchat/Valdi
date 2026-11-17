use crate::ffi::{valdi_rust_value_call_function, valdi_rust_value_new_function};
use crate::{
    Marshaller, MarshallerRef, ValdiCall, ValdiDeserialize, ValdiPush, ValdiSerialize, ValdiStatus,
    Value, ValueRef, ffi_boundary,
};
use std::ffi::c_void;
use std::marker::PhantomData;
use std::panic::{AssertUnwindSafe, catch_unwind};

pub struct Function(Value);
#[derive(Clone, Copy)]
pub struct FunctionRef<'a>(ValueRef<'a>);

impl Function {
    pub fn from_value(value: Value) -> Self {
        Self(value)
    }

    pub fn new<F>(function: F) -> Result<Self, String>
    where
        F: Fn(ValdiCall) -> Result<(), String> + 'static,
    {
        unsafe extern "C" fn invoke<F>(instance: *mut c_void, call: ValdiCall) -> ValdiStatus
        where
            F: Fn(ValdiCall) -> Result<(), String> + 'static,
        {
            ffi_boundary(|| {
                let function = unsafe { &*(instance as *const F) };
                function(call)
            })
        }

        unsafe extern "C" fn drop_instance<F>(instance: *mut c_void) {
            let _ = catch_unwind(AssertUnwindSafe(|| {
                unsafe { drop(Box::from_raw(instance as *mut F)) };
            }));
        }

        let instance = Box::into_raw(Box::new(function)).cast();
        let mut out = std::mem::MaybeUninit::uninit();
        unsafe {
            valdi_rust_value_new_function(
                instance,
                invoke::<F>,
                drop_instance::<F>,
                out.as_mut_ptr(),
            )
        }
        .into_result()?;
        Ok(Self(unsafe { Value::from_raw(out.assume_init()) }))
    }

    pub fn as_borrowed(&self) -> FunctionRef<'_> {
        FunctionRef(self.0.as_borrowed())
    }

    pub fn call(&self, arguments: &[ValueRef<'_>]) -> Result<Value, String> {
        self.as_borrowed().call(arguments)
    }
}

impl Clone for Function {
    fn clone(&self) -> Self {
        Self(self.0.clone())
    }
}

impl FunctionRef<'_> {
    pub fn to_owned(&self) -> Result<Function, String> {
        Ok(Function(self.0.to_owned()?))
    }

    pub fn call(&self, arguments: &[ValueRef<'_>]) -> Result<Value, String> {
        self.call_with(
            |marshaller| {
                for argument in arguments {
                    marshaller.push_value(*argument);
                }
                Ok(())
            },
            |value| value.to_owned(),
        )
    }

    pub fn call_with<R, P, D>(&self, push_arguments: P, read_result: D) -> Result<R, String>
    where
        P: FnOnce(&mut Marshaller<'_>) -> Result<(), String>,
        D: for<'a> FnOnce(ValueRef<'a>) -> Result<R, String>,
    {
        struct CallContext<P, D, R> {
            push_arguments: Option<P>,
            read_result: Option<D>,
            result: Option<Result<R, String>>,
        }

        unsafe extern "C" fn push<P, D, R>(context: *mut c_void, raw: *mut c_void) -> ValdiStatus
        where
            P: FnOnce(&mut Marshaller<'_>) -> Result<(), String>,
            D: for<'a> FnOnce(ValueRef<'a>) -> Result<R, String>,
        {
            ffi_boundary(|| {
                let context = unsafe { &mut *(context as *mut CallContext<P, D, R>) };
                let mut marshaller = Marshaller {
                    raw,
                    marker: PhantomData,
                };
                context
                    .push_arguments
                    .take()
                    .expect("Valdi argument marshaller called more than once")(
                    &mut marshaller
                )
            })
        }

        unsafe extern "C" fn read<P, D, R>(
            context: *mut c_void,
            raw: *const c_void,
            index: usize,
        ) -> ValdiStatus
        where
            P: FnOnce(&mut Marshaller<'_>) -> Result<(), String>,
            D: for<'a> FnOnce(ValueRef<'a>) -> Result<R, String>,
        {
            ffi_boundary(|| {
                let context = unsafe { &mut *(context as *mut CallContext<P, D, R>) };
                let marshaller = MarshallerRef {
                    raw,
                    len: index + 1,
                    marker: PhantomData,
                };
                let value = marshaller.get(index)?;
                context.result = Some(context
                    .read_result
                    .take()
                    .expect("Valdi result marshaller called more than once")(
                    value
                ));
                Ok(())
            })
        }

        let mut context = CallContext {
            push_arguments: Some(push_arguments),
            read_result: Some(read_result),
            result: None,
        };
        unsafe {
            valdi_rust_value_call_function(
                self.0.as_ptr(),
                (&mut context as *mut CallContext<P, D, R>).cast(),
                push::<P, D, R>,
                read::<P, D, R>,
            )
        }
        .into_result()?;
        context
            .result
            .expect("Valdi function call completed without producing a result")
    }
}

impl<'a> ValdiDeserialize<'a> for Function {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value.to_owned()?))
    }
}

impl<'a> ValdiDeserialize<'a> for FunctionRef<'a> {
    fn from_valdi(value: ValueRef<'a>) -> Result<Self, String> {
        Ok(Self(value))
    }
}

impl ValdiSerialize for Function {
    fn to_valdi(&self) -> Result<Value, String> {
        Ok(self.0.clone())
    }
}

impl ValdiSerialize for FunctionRef<'_> {
    fn to_valdi(&self) -> Result<Value, String> {
        self.0.to_owned()
    }
}

impl ValdiPush for Function {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}

impl ValdiPush for FunctionRef<'_> {
    fn push_to(&self, marshaller: &mut Marshaller<'_>) -> Result<(), String> {
        self.0.push_to(marshaller)
    }
}
