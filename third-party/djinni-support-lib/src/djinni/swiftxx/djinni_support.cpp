#include "djinni_support.hpp"

namespace djinni::swift {

AnyValue makeStringValue(const char* bytes, size_t size) {
    return StringValue(bytes, size);
}

AnyValue makeBinaryValue(const void* bytes, size_t size) {
    auto p = reinterpret_cast<const uint8_t*>(bytes);
    return BinaryValue{p, p + size};
}

AnyValue makeRangeValue(const void* bytes, size_t size) {
    return RangeValue{bytes, size};
}

RangeValue getBinaryRange(const AnyValue& v) {
    if (std::holds_alternative<StringValue>(v)) {
        const auto& s = std::get<StringValue>(v);
        return {s.data(), s.size()};
    } else if (std::holds_alternative<BinaryValue>(v)) {
        const auto& b = std::get<BinaryValue>(v);
        return {b.data(), b.size()};
    } else {
        return std::get<RangeValue>(v);
    }
}

size_t getSize(const AnyValue* v) {
    auto composite = std::get<CompositeValuePtr>(*v);
    return composite->getSize();
}

AnyValue getMember(const AnyValue* v, size_t i) {
    auto composite = std::get<CompositeValuePtr>(*v);
    return composite->getValue(i);
}

AnyValue getMember(const ParameterList* v, size_t i) {
    return v->getValue(i);
}

void addMember(AnyValue* c, const AnyValue& v) {
    auto composite = std::get<CompositeValuePtr>(*c);
    composite->addValue(v);
}

void setReturnValue(AnyValue* ret, const AnyValue& v) {
    *ret = v;
}

void setErrorValue(AnyValue* ret, const ErrorValue& v) {
    *ret = v;
}

void setErrorMessage(AnyValue* ret, const std::string& s) {
    *ret = ErrorValue{s};
}

WeakSwiftProxy weakify(const AnyValue& v) {
    auto i = std::get<InterfaceValue>(v);
    return i.sptr;
}

AnyValue strongify(const WeakSwiftProxy& v) {
    return {v.lock()};
}

InterfaceInfo getInterfaceInfo(const AnyValue* v) {
    auto i = std::get<InterfaceValue>(*v);
    return {i.ptr.get(), i.sptr ? i.sptr->ctx() : nullptr };
}

AnyValue makeVoidValue() {
    return VoidValue{};
}

bool isVoidValue(const AnyValue* c) {
    return std::holds_alternative<VoidValue>(*c);
}


AnyValue makeCompositeValue() {
    return { std::make_shared<CompositeValue>() }; 
}

bool isError(const AnyValue* ret) {
    return std::holds_alternative<ErrorValue>(*ret);
}

ErrorValue getError(const AnyValue* ret) {
    return std::get<ErrorValue>(*ret);
}

ProtocolWrapper::ProtocolWrapper(void* ctx, DispatchFunc dispatcher):
    _ctx(ctx), _dispatcher(dispatcher)
{}

ProtocolWrapper::~ProtocolWrapper() {
    _dispatcher(_ctx, -1, nullptr, nullptr);
}

AnyValue ProtocolWrapper::callProtocol(int idx, const ParameterList* params) {
    AnyValue ret = VoidValue(); // output parameter
    _dispatcher(_ctx, idx, params, &ret);
    if (std::holds_alternative<ErrorValue>(ret)) {
        const auto& e = std::get<ErrorValue>(ret);
        if (e.cause) {
            // previously thrown c++ exception
            std::rethrow_exception(e.cause);
        } else {
            // swift error
            throw e;
        }
    }
    return ret;
}

// -------- Provider bridge (callable from Swift as callProviderFunction / makeProviderFunction)
AnyValue callProviderFunction(const AnyValue& providerValue) {
    auto ptr = std::get<OpaqueValuePtr>(providerValue);
    auto* callable = dynamic_cast<CallableProvider*>(ptr.get());
    if (!callable) {
        throw ErrorValue("callProviderFunction: value is not a provider");
    }
    return callable->call();
}

namespace {
struct CallbackProviderHolder : CallableProvider {
    AnyValue (*callback)(void*);
    void* context;
    void (*releaseContext)(void*);
    CallbackProviderHolder(AnyValue (*cb)(void*), void* ctx, void (*release)(void*) = nullptr)
        : callback(cb), context(ctx), releaseContext(release) {}
    ~CallbackProviderHolder() override {
        if (releaseContext && context) {
            releaseContext(context);
        }
    }
    AnyValue call() override { return callback(context); }
};
}

AnyValue makeProviderFunction(AnyValue (*callback)(void*), void* context, void (*releaseContext)(void*)) {
    auto holder = std::make_shared<CallbackProviderHolder>(callback, context, releaseContext);
    return AnyValue{OpaqueValuePtr(holder)};
}

}
