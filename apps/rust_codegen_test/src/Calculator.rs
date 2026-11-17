use rust_codegen_test::__valdi_core::{
    Array, ArrayRef, BigInt, BigIntRef, BytesRef, BytesView, Map, MapRef, RuntimeRef, StringRef,
    ValdiStatus, Value, ValueKind, ValueRef, ffi_boundary,
};
use rust_codegen_test::{
    Accumulator, AccumulatorProxy, AccumulatorRef, Boxed, BoxedRef, CalculatorModule,
    CalculatorModuleApplyCallbackCallbackRef, CalculatorModuleCallOptionalCallbackCallbackRef,
    CalculatorModuleEchoOptionalFutureReturnFuture,
    CalculatorModuleEchoOptionalFutureValueFutureRef,
    CalculatorModuleMakeBackgroundFutureReturnFuture, CalculatorModuleMakeCallbackReturnCallback,
    CalculatorModuleMakeCallbackReturnCallbackFn, CalculatorModuleMakeRejectedFutureReturnFuture,
    CalculatorModuleMakeResolvedFutureReturnFuture,
    CalculatorModuleMakeResolvedStringFutureReturnFuture,
    CalculatorModuleMakeResolvedVoidFutureReturnFuture,
    CalculatorModuleObserveFutureValueFutureRef, CalculatorModuleStoreCallbackCallback,
    CalculatorModuleStoreCallbackCallbackRef, KeywordModel, KeywordModelRef, MultiplyInTypeScript,
    Operation, Point, PointRef, Unit, register_calculator,
};
use std::cell::RefCell;
use std::sync::{
    Arc,
    atomic::{AtomicU64, Ordering},
};

#[derive(Default)]
struct Calculator {
    bytes: RefCell<Option<BytesView>>,
    callback: RefCell<Option<CalculatorModuleStoreCallbackCallback>>,
    point: RefCell<Option<Point>>,
    proxy: RefCell<Option<Accumulator>>,
    future_value: Arc<AtomicU64>,
}

struct RustAccumulator {
    value: f64,
}

struct OffsetCallback {
    offset: f64,
}

impl CalculatorModuleMakeCallbackReturnCallbackFn for OffsetCallback {
    fn call(&self, value: f64) -> Result<f64, String> {
        Ok(value + self.offset)
    }
}

impl AccumulatorProxy for RustAccumulator {
    fn add(&self, value: f64) -> Result<f64, String> {
        Ok(self.value + value)
    }

    fn label(&self) -> Result<String, String> {
        Ok(format!("Accumulator({})", self.value))
    }
}

impl CalculatorModule for Calculator {
    fn add(&self, left: f64, right: f64) -> Result<f64, String> {
        Ok(left + right)
    }

    fn negate(&self, value: bool) -> Result<bool, String> {
        Ok(!value)
    }

    fn greet(&self, name: &str) -> Result<String, String> {
        Ok(format!("Hello, {name}"))
    }

    fn copy_point(&self, point: PointRef<'_>) -> Result<Point, String> {
        point.to_owned()
    }

    fn echo_optional_point(&self, point: Option<PointRef<'_>>) -> Result<Option<Point>, String> {
        point.map(|point| point.to_owned()).transpose()
    }

    fn echo_string_box(&self, value: BoxedRef<'_, StringRef<'_>>) -> Result<Boxed<String>, String> {
        value.to_owned()
    }

    fn echo_point_box(&self, value: BoxedRef<'_, PointRef<'_>>) -> Result<Boxed<Point>, String> {
        value.to_owned()
    }

    fn echo_nested_box(
        &self,
        value: BoxedRef<'_, BoxedRef<'_, StringRef<'_>>>,
    ) -> Result<Boxed<Boxed<String>>, String> {
        value.to_owned()
    }

    fn echo_number_array_box(
        &self,
        value: BoxedRef<'_, ArrayRef<'_, f64>>,
    ) -> Result<Boxed<Array<f64>>, String> {
        value.to_owned()
    }

    fn echo_bytes_box(
        &self,
        value: BoxedRef<'_, BytesRef<'_>>,
    ) -> Result<Boxed<BytesView>, String> {
        value.to_owned()
    }

    fn echo_keyword(&self, value: KeywordModelRef<'_>) -> Result<KeywordModel, String> {
        value.to_owned()
    }

    fn store_point(&self, point: PointRef<'_>) -> Result<(), String> {
        self.point.replace(Some(point.to_owned()?));
        Ok(())
    }

    fn stored_point_x(&self) -> Result<f64, String> {
        Ok(self
            .point
            .borrow()
            .as_ref()
            .ok_or_else(|| "point not stored".to_owned())?
            .x)
    }

    fn call_proxy(&self, proxy: AccumulatorRef<'_>, value: f64) -> Result<f64, String> {
        proxy.add(value)
    }

    fn echo_optional_proxy(
        &self,
        proxy: Option<AccumulatorRef<'_>>,
    ) -> Result<Option<Accumulator>, String> {
        proxy.map(|proxy| proxy.to_owned()).transpose()
    }

    fn store_proxy(&self, proxy: AccumulatorRef<'_>) -> Result<(), String> {
        self.proxy.replace(Some(proxy.to_owned()?));
        Ok(())
    }

    fn call_stored_proxy(&self, value: f64) -> Result<f64, String> {
        self.proxy
            .borrow()
            .as_ref()
            .ok_or_else(|| "proxy not stored".to_owned())?
            .add(value)
    }

    fn make_proxy(&self, seed: f64) -> Result<Accumulator, String> {
        Accumulator::new(RustAccumulator { value: seed })
    }

    fn echo_operation(&self, operation: Operation) -> Result<Operation, String> {
        Ok(operation)
    }

    fn echo_optional_operation(
        &self,
        operation: Option<Operation>,
    ) -> Result<Option<Operation>, String> {
        Ok(operation)
    }

    fn echo_unit(&self, unit: Unit) -> Result<Unit, String> {
        Ok(unit)
    }

    fn echo_big_int(&self, value: BigIntRef<'_>) -> Result<BigInt, String> {
        Ok(value.to_owned())
    }

    fn echo_optional_big_int(
        &self,
        value: Option<BigIntRef<'_>>,
    ) -> Result<Option<BigInt>, String> {
        Ok(value.map(BigIntRef::to_owned))
    }

    fn echo_big_ints(&self, values: ArrayRef<'_, BigIntRef<'_>>) -> Result<Array<BigInt>, String> {
        values.to_vec().map(Array::from_vec)
    }

    fn echo_long(&self, value: i64) -> Result<i64, String> {
        Ok(value)
    }

    fn echo_bytes(&self, value: BytesRef<'_>) -> Result<BytesView, String> {
        Ok(value.to_owned())
    }

    fn echo_optional_bytes(
        &self,
        value: Option<BytesRef<'_>>,
    ) -> Result<Option<BytesView>, String> {
        Ok(value.map(BytesRef::to_owned))
    }

    fn store_bytes(&self, value: BytesRef<'_>) -> Result<(), String> {
        self.bytes.replace(Some(value.to_owned()));
        Ok(())
    }

    fn stored_bytes(&self) -> Result<BytesView, String> {
        self.bytes
            .borrow()
            .as_ref()
            .cloned()
            .ok_or_else(|| "bytes not stored".to_owned())
    }

    fn echo_numbers(&self, values: ArrayRef<'_, f64>) -> Result<Array<f64>, String> {
        values.to_vec().map(Array::from_vec)
    }

    fn echo_optional_numbers(
        &self,
        values: Option<ArrayRef<'_, f64>>,
    ) -> Result<Option<Array<f64>>, String> {
        values
            .map(|values| values.to_vec().map(Array::from_vec))
            .transpose()
    }

    fn echo_flags(&self, values: ArrayRef<'_, bool>) -> Result<Array<bool>, String> {
        values.to_vec().map(Array::from_vec)
    }

    fn echo_labels(&self, values: ArrayRef<'_, StringRef<'_>>) -> Result<Array<String>, String> {
        values.to_vec().map(Array::from_vec)
    }

    fn echo_points(&self, values: ArrayRef<'_, PointRef<'_>>) -> Result<Array<Point>, String> {
        values.to_vec().map(Array::from_vec)
    }

    fn echo_any(&self, value: ValueRef<'_>) -> Result<Value, String> {
        value.to_owned()
    }

    fn echo_optional_any(&self, value: Option<ValueRef<'_>>) -> Result<Option<Value>, String> {
        value.map(|value| value.to_owned()).transpose()
    }

    fn make_any_string(&self) -> Result<Value, String> {
        Value::from_string("forty-two")
    }

    fn read_any_number(&self, value: ValueRef<'_>) -> Result<f64, String> {
        value.as_number()
    }

    fn echo_map(&self, value: MapRef<'_, String, Value>) -> Result<Map<String, Value>, String> {
        value.to_owned()
    }

    fn echo_number_map(
        &self,
        value: MapRef<'_, String, Value>,
    ) -> Result<Map<String, Value>, String> {
        let answer = value
            .get("answer")?
            .ok_or_else(|| "missing number map entry".to_owned())?;
        if answer.as_number()? != 42.0 {
            return Err("unexpected number map entry".to_owned());
        }
        value.to_owned()
    }

    fn make_map(&self, value: ValueRef<'_>) -> Result<Map<String, Value>, String> {
        Map::new(&[("answer", value)])
    }

    fn read_map(&self, value: MapRef<'_, String, Value>, key: &str) -> Result<Value, String> {
        value
            .get(key)?
            .ok_or_else(|| format!("missing map key: {key}"))
    }

    fn make_resolved_future(
        &self,
        value: f64,
    ) -> Result<CalculatorModuleMakeResolvedFutureReturnFuture, String> {
        CalculatorModuleMakeResolvedFutureReturnFuture::resolved(value)
    }

    fn make_background_future(
        &self,
        value: f64,
    ) -> Result<CalculatorModuleMakeBackgroundFutureReturnFuture, String> {
        let (future, completer) = CalculatorModuleMakeBackgroundFutureReturnFuture::pending()?;
        std::thread::spawn(move || {
            completer
                .resolve(value)
                .expect("background Valdi future completion failed");
        });
        Ok(future)
    }

    fn make_rejected_future(
        &self,
        message: &str,
    ) -> Result<CalculatorModuleMakeRejectedFutureReturnFuture, String> {
        CalculatorModuleMakeRejectedFutureReturnFuture::rejected(message)
    }

    fn make_resolved_string_future(
        &self,
        value: &str,
    ) -> Result<CalculatorModuleMakeResolvedStringFutureReturnFuture, String> {
        CalculatorModuleMakeResolvedStringFutureReturnFuture::resolved(value)
    }

    fn make_resolved_void_future(
        &self,
    ) -> Result<CalculatorModuleMakeResolvedVoidFutureReturnFuture, String> {
        CalculatorModuleMakeResolvedVoidFutureReturnFuture::resolved()
    }

    fn observe_future(
        &self,
        value: CalculatorModuleObserveFutureValueFutureRef<'_>,
    ) -> Result<(), String> {
        let future_value = self.future_value.clone();
        value.then(move |result| {
            let value = result.unwrap_or(f64::NAN);
            future_value.store(value.to_bits(), Ordering::Release);
        })
    }

    fn echo_optional_future(
        &self,
        value: Option<CalculatorModuleEchoOptionalFutureValueFutureRef<'_>>,
    ) -> Result<Option<CalculatorModuleEchoOptionalFutureReturnFuture>, String> {
        let Some(value) = value else {
            return Ok(None);
        };
        let (future, completer) = CalculatorModuleEchoOptionalFutureReturnFuture::pending()?;
        value.then(move |result| {
            let completion = match result {
                Ok(value) => completer.resolve(value),
                Err(error) => completer.reject(&error),
            };
            if let Err(error) = completion {
                eprintln!("failed to forward optional Valdi future: {error}");
            }
        })?;
        Ok(Some(future))
    }

    fn last_future_value(&self) -> Result<f64, String> {
        Ok(f64::from_bits(self.future_value.load(Ordering::Acquire)))
    }

    fn apply_callback(
        &self,
        value: f64,
        callback: CalculatorModuleApplyCallbackCallbackRef<'_>,
    ) -> Result<f64, String> {
        callback.call(value)
    }

    fn call_optional_callback(
        &self,
        value: f64,
        callback: Option<CalculatorModuleCallOptionalCallbackCallbackRef<'_>>,
    ) -> Result<Option<f64>, String> {
        callback.map(|callback| callback.call(value)).transpose()
    }

    fn make_callback(
        &self,
        offset: f64,
    ) -> Result<CalculatorModuleMakeCallbackReturnCallback, String> {
        CalculatorModuleMakeCallbackReturnCallback::new(OffsetCallback { offset })
    }

    fn store_callback(
        &self,
        callback: CalculatorModuleStoreCallbackCallbackRef<'_>,
    ) -> Result<(), String> {
        self.callback.replace(Some(callback.to_owned()?));
        Ok(())
    }

    fn call_stored(&self, value: f64) -> Result<f64, String> {
        self.callback
            .borrow()
            .as_ref()
            .ok_or_else(|| "callback not stored".to_owned())?
            .call(value)
    }

    fn fail(&self) -> Result<(), String> {
        Err("expected Rust failure".to_owned())
    }

    fn panic_call(&self) -> Result<(), String> {
        panic!("expected Rust panic")
    }
}

register_calculator!(Calculator::default);

#[unsafe(no_mangle)]
pub extern "C" fn rust_codegen_test_inline_values_do_not_allocate_handles() -> bool {
    unsafe extern "C" {
        fn valdi_rust_handle_live_count() -> usize;
    }

    let before = unsafe { valdi_rust_handle_live_count() };
    let null = Value::null().expect("inline null creation failed");
    let undefined = Value::undefined().expect("inline undefined creation failed");
    let boolean = Value::from_bool(true).expect("inline boolean creation failed");
    let number = Value::from_number(42.0).expect("inline number creation failed");
    let long = Value::from_long(42).expect("inline long creation failed");
    let string = Value::from_string("inline").expect("inline string creation failed");
    let bytes = Value::from_owned_bytes(vec![1, 2, 3]).expect("inline bytes creation failed");
    let after = unsafe { valdi_rust_handle_live_count() };

    drop((null, undefined, boolean, number, long, string, bytes));
    before == after && unsafe { valdi_rust_handle_live_count() } == before
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_codegen_test_inline_values_materialize_through_marshaller(
    expected_tracked_handles: usize,
) -> bool {
    unsafe extern "C" {
        fn valdi_rust_handle_live_count() -> usize;
    }

    let before = unsafe { valdi_rust_handle_live_count() };
    let null = Value::null().expect("inline null creation failed");
    let undefined = Value::undefined().expect("inline undefined creation failed");
    let boolean = Value::from_bool(true).expect("inline boolean creation failed");
    let number = Value::from_number(42.0).expect("inline number creation failed");
    let long = Value::from_long(42).expect("inline long creation failed");
    let string = Value::from_string("inline").expect("inline string creation failed");
    let bytes = Value::from_owned_bytes(vec![1, 2, 3]).expect("inline bytes creation failed");

    let values_are_valid = null.as_borrowed().kind() == ValueKind::Null
        && undefined.as_borrowed().kind() == ValueKind::Undefined
        && boolean.as_borrowed().as_bool() == Ok(true)
        && number.as_borrowed().as_number() == Ok(42.0)
        && long.as_borrowed().as_long() == Ok(42)
        && string.as_borrowed().as_string() == Ok("inline".to_owned())
        && bytes.as_borrowed().as_bytes() == Ok(vec![1, 2, 3]);
    let tracked_as_expected = unsafe { valdi_rust_handle_live_count() } == before + expected_tracked_handles;

    drop((null, undefined, boolean, number, long, string, bytes));
    values_are_valid && tracked_as_expected && unsafe { valdi_rust_handle_live_count() } == before
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rust_codegen_test_call_multiply_in_type_script(
    runtime: *mut std::ffi::c_void,
    native_objects_manager: *mut std::ffi::c_void,
    left: f64,
    right: f64,
    out: *mut f64,
) -> ValdiStatus {
    ffi_boundary(|| {
        let runtime = unsafe { RuntimeRef::from_raw(runtime, native_objects_manager) };
        let function = MultiplyInTypeScript::resolve(runtime)?;
        let result = function.call(left, right)?;
        unsafe { out.write(result) };
        Ok(())
    })
}
