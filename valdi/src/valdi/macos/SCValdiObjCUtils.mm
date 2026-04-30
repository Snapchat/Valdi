//
//  SCValdiObjCUtils.m
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#import "SCValdiObjCUtils.h"

#import "valdi_core/SCValdiNativeConvertible.h"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/cpp/Utils/ValueFunction.hpp"

#import "djinni/objc/DJIMarshal+Private.h"
#import "SCValdiMacOSFunction.h"

class ObjCObject: public Valdi::ValdiObject {
public:
    ObjCObject(id value, BOOL useStrongRef): _value((useStrongRef ? (void *)CFBridgingRetain(value) : (__bridge void *)value)), _strongRef(useStrongRef) {
    }

    ~ObjCObject() override {
        if (_strongRef) {
            CFBridgingRelease(_value);
        }
    }

    id getValue() const { return (__bridge id)_value; }

    VALDI_CLASS_HEADER_IMPL(ObjCObject)

private:
    void *_value;
    BOOL _strongRef;
};

NSString *NSStringFromSTDStringView(const std::string_view &str) {
    return [[NSString alloc] initWithBytes:str.data()
                                    length:static_cast<NSUInteger>(str.size())
                                    encoding:NSUTF8StringEncoding];
}

NSString *NSStringFromString(const Valdi::StringBox &stringBox) {
    return NSStringFromSTDStringView(stringBox.slowToString());
}

std::string_view StringViewFromNSString(__unsafe_unretained NSString *nsString) {
    return std::string_view(nsString.UTF8String, [nsString lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
}

Valdi::StringBox StringFromNSString(__unsafe_unretained NSString *nsString) {
    return Valdi::StringCache::getGlobal().makeStringFromLiteral(StringViewFromNSString(nsString));
}

Valdi::SharedValdiObject ValdiObjectFromNSObject(id object, BOOL useStrongRef) {
    if (!object) {
        return nullptr;
    }

    return Valdi::SharedValdiObject(Valdi::makeShared<ObjCObject>(object, useStrongRef));
}

id NSObjectFromValue(const Valdi::Value &value) {
    switch (value.getType()) {
        case Valdi::ValueType::Null:
        case Valdi::ValueType::Undefined:
            return nil;
        case Valdi::ValueType::InternedString:
        case Valdi::ValueType::StaticString:
            return NSStringFromString(value.toStringBox());
        case Valdi::ValueType::Int:
            return @(value.toInt());
        case Valdi::ValueType::Long:
            return @(value.toLong());
        case Valdi::ValueType::Double:
            return @(value.toDouble());
        case Valdi::ValueType::Bool:
            return @(value.toBool());
        case Valdi::ValueType::Map:
            return nil;
        case Valdi::ValueType::Array: {
            const Valdi::ValueArray* arr = value.getArray();
            if (!arr) return nil;
            NSMutableArray *result = [NSMutableArray arrayWithCapacity:arr->size()];
            for (const Valdi::Value& element : *arr) {
                id obj = NSObjectFromValue(element);
                [result addObject:obj ?: [NSNull null]];
            }
            return result;
        }
        case Valdi::ValueType::TypedArray:
            return nil;
        case Valdi::ValueType::Function: {
            auto functionRef = value.getFunctionRef();
            return [[SCValdiMacOSFunction alloc] initWithCppInstance:(void *)functionRef.get()];
        }
        case Valdi::ValueType::Error:
            return nil;
        case Valdi::ValueType::ValdiObject:
            return nil;
        case Valdi::ValueType::TypedObject:
        case Valdi::ValueType::ProxyTypedObject:
            return nil;
    }
}

id NSObjectFromRefCountable(const Valdi::Ref<Valdi::RefCountable> &refCountable) {
    auto objcObject = Valdi::castOrNull<ObjCObject>(refCountable);
    if (objcObject != nullptr) {
        return objcObject->getValue();
    }

    return nil;
}

id NSObjectFromValdiObject(const Valdi::SharedValdiObject &valdiObject) {
    return NSObjectFromRefCountable(valdiObject);
}

Valdi::Value ValueFromNSDictionary(NSDictionary<NSString *, id> *dict) {
    auto map = Valdi::makeShared<Valdi::ValueMap>();
    for (NSString *key in dict) {
        id value = [dict objectForKey:key];
        auto weakValue = ValueFromNSObject(value);
        auto mapKey = StringFromNSString(key);

        (*map)[std::move(mapKey)] = std::move(weakValue);
    }

    return Valdi::Value(map);
}

Valdi::Value ValueFromNSObject(id object) {
    if (!object || [object isKindOfClass:[NSNull class]]) {
        return Valdi::Value::undefined();
    }
    if ([object isKindOfClass:[NSString class]]) {
        return Valdi::Value(StringFromNSString(object));
    }
    if ([object isKindOfClass:[NSNumber class]]) {
        return Valdi::Value([object doubleValue]);
    }
    if ([object isKindOfClass:[NSDictionary class]]) {
        return ValueFromNSDictionary(object);
    }
    if ([object isKindOfClass:[SCValdiMacOSFunction class]]) {
        Valdi::ValueFunction *function = (Valdi::ValueFunction *)((SCValdiMacOSFunction *)object).cppInstance;
        return Valdi::Value(Valdi::Ref(function));
    }
    if ([object respondsToSelector:@selector(valdi_toNative:)]) {
        Valdi::Value out;
        [(id<SCValdiNativeConvertible>)object valdi_toNative:&out];
        return out;
    }

    return Valdi::Value();
}
