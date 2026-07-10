//
//  SCValdiObjCConversionUtils.m
//  valdi-ios
//
//  Created by Simon Corsin on 9/10/19.
//

#import "djinni/objc/DJIMarshal+Private.h"

#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiAction.h"
#import "valdi_core/SCValdiFunctionWithCPPFunction+CPP.h"
#import "valdi_core/SCValdiWrappedValue+Private.h"
#import "valdi_core/SCValdiUndefinedValue.h"
#import "valdi_core/ValueFunctionWithSCValdiFunction.h"
#import "valdi_core/SCValdiNativeConvertible.h"
#import "valdi_core/SCValdiMarshaller+CPP.h"
#import "valdi_core/SCValdiMarshallable.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiError.h"

#import "valdi_core/cpp/Context/ContextBase.hpp"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/cpp/Utils/ValueFunction.hpp"
#import "valdi_core/cpp/Utils/ValueArray.hpp"
#import "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#import "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#import "valdi_core/cpp/Utils/ValueMap.hpp"
#import "valdi_core/cpp/Utils/LoggerUtils.hpp"
#import "valdi_core/cpp/Utils/StaticString.hpp"
#import "valdi_core/cpp/Utils/LazyValueConvertible.hpp"
#import "valdi_core/cpp/Resources/Asset.hpp"
#import "valdi_core/SCNValdiCoreAsset.h"
#import "valdi_core/SCNValdiCoreAsset+Private.h"
#import "valdi_core/SCValdiReferenceTable+CPP.h"

#import <utils/debugging/Assert.hpp>

@implementation SCValdiCppObject

- (instancetype)initWithRef:(Valdi::Ref<Valdi::RefCountable>)ref
{
    self = [super init];
    if (self) {
        _ref = ref;
    }
    return self;
}

+ (instancetype)objectWithRef:(Valdi::Ref<Valdi::RefCountable>)ref
{
    return [[SCValdiCppObject alloc] initWithRef:ref];
}

@end

namespace ValdiIOS {

class ObjCProxyObject: public Valdi::ValueTypedProxyObject, public ObjCObjectIndirectRef {
public:
    ObjCProxyObject(const Valdi::Ref<Valdi::ValueTypedObject>& typedObject,
                    __unsafe_unretained id object,
                    BOOL strongRef): Valdi::ValueTypedProxyObject(typedObject), ObjCObjectIndirectRef(object, strongRef) {}

    ~ObjCProxyObject() override = default;

    std::string_view getType() const final {
        return "ObjC Proxy";
    }
};

static Valdi::IOS::ObjCReferenceTable *resolveGlobalRefTable(BOOL strongRef) {
    if (strongRef) {
        return Valdi::IOS::ObjCStrongReferenceTable::getGlobal();
    } else {
        return Valdi::IOS::ObjCWeakReferenceTable::getGlobal();
    }
}

static Valdi::IOS::ObjCReferenceTable *resolveRefTable(__unsafe_unretained id value, BOOL strongRef) {
    if (!value)  {
        return nullptr;
    }
    auto *currentContext = Valdi::ContextBase::current();
    if (currentContext != nullptr) {
        if (strongRef) {
            auto *strongRefTable = dynamic_cast<Valdi::IOS::ObjCReferenceTable *>(currentContext->getStrongReferenceTable());
            if (strongRefTable != nullptr) {
                return strongRefTable;
            }
        } else {
            auto *weakRefTable = dynamic_cast<Valdi::IOS::ObjCReferenceTable *>(currentContext->getWeakReferenceTable());
            if (weakRefTable != nullptr) {
                return weakRefTable;
            }
        }
    }

    return resolveGlobalRefTable(strongRef);
}

ObjCObjectIndirectRef::ObjCObjectIndirectRef() = default;

ObjCObjectIndirectRef::ObjCObjectIndirectRef(id value, BOOL strongRef): _table(resolveRefTable(value, strongRef)), _id(_table != nullptr ? _table->store(value) : Valdi::SequenceID()) {}
ObjCObjectIndirectRef::ObjCObjectIndirectRef(id value): ObjCObjectIndirectRef(value, YES) {}

ObjCObjectIndirectRef::ObjCObjectIndirectRef(ObjCObjectIndirectRef &&other): _table(std::move(other._table)), _id(other._id) {
    other._id = Valdi::SequenceID();
}

ObjCObjectIndirectRef::~ObjCObjectIndirectRef() {
    if (_table != nullptr) {
        _table->remove(_id);
    }
}

bool ObjCObjectIndirectRef::isStrong() const {
    return _table != nullptr ? _table->retainsObjectsAsStrongReference() : false;
}

id ObjCObjectIndirectRef::getValue() const {
    if (_table == nullptr) {
        return nil;
    }
    return _table->load(_id);
}

ObjCObjectIndirectRef &ObjCObjectIndirectRef::operator=(ObjCObjectIndirectRef &&other) {
    if (this != &other) {
        if (_table != nullptr) {
            _table->remove(_id);
        }

        _table = std::move(other._table);
        _id = other._id;

        other._id = Valdi::SequenceID();
    }

    return *this;
}

ObjCObjectDirectRef::ObjCObjectDirectRef() = default;

ObjCObjectDirectRef::ObjCObjectDirectRef(id value): _value(value) {}

ObjCObjectDirectRef::ObjCObjectDirectRef(const ObjCObjectDirectRef &other): _value(other._value) {}

ObjCObjectDirectRef::ObjCObjectDirectRef(ObjCObjectDirectRef &&other): _value(other._value) {
    other._value = nil;
}

ObjCObjectDirectRef::~ObjCObjectDirectRef() {
    _value = nil;
}

bool ObjCObjectDirectRef::isStrong() const {
    return true;
}

id ObjCObjectDirectRef::getValue() const {
    return _value;
}

ObjCObjectDirectRef &ObjCObjectDirectRef::operator=(ObjCObjectDirectRef &&other) {
    if (this != &other) {
        _value = other._value;

        other._value = nil;
    }

    return *this;
}

ObjCObjectDirectRef &ObjCObjectDirectRef::operator=(const ObjCObjectDirectRef &other) {
    if (this != &other) {
        _value = other._value;
    }

    return *this;
}

ObjCObject::ObjCObject(id value): ObjCObjectDirectRef(value) {}

ObjCObject::~ObjCObject() = default;

Valdi::Value ObjCObject::toValue() {
    return ValdiIOS::ValueFromNSObject(getValue());
}

class ObjCValdiObject: public Valdi::ValdiObject, public ObjCObject {
public:
    ObjCValdiObject(id value): ObjCObject(value) {}

    VALDI_CLASS_HEADER_IMPL(ObjCValdiObject)
};

    id NSObjectWrappingValue(const Valdi::Value &value) {
        return [[SCValdiWrappedValue alloc] initWithValue:value];
    }

    id NSObjectFromValue(const Valdi::Value &value) {
        switch (value.getType()) {
            case Valdi::ValueType::Null:
                return [NSNull null];
            case Valdi::ValueType::Undefined:
                return SCValdiUndefinedValue.undefined;
            case Valdi::ValueType::InternedString:
                return NSStringFromString(value.toStringBox());
            case Valdi::ValueType::StaticString:
                return NSStringFromStaticString(*value.getStaticString());
            case Valdi::ValueType::Int:
                return @(value.toInt());
            case Valdi::ValueType::Long:
                return @(value.toLong());
            case Valdi::ValueType::Double:
                return @(value.toDouble());
            case Valdi::ValueType::Bool:
                return @(value.toBool());
            case Valdi::ValueType::Map:
            {
                const auto &valueMap = *value.getMap();
                NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:valueMap.size()];
                for (const auto &it : valueMap) {
                    dictionary[NSStringFromString(it.first)] = NSObjectFromValue(it.second);
                }

                return dictionary;
            }
            case Valdi::ValueType::Array:
            {
                const auto &valueArray = *value.getArray();
                NSMutableArray *array = [NSMutableArray arrayWithCapacity:valueArray.size()];

                for (const auto &item: valueArray) {
                    [array addObject:NSObjectFromValue(item)];
                }

                return array;
            }
            case Valdi::ValueType::TypedArray:
            {
                return NSDataFromValue(value);
            }
            case Valdi::ValueType::TypedObject:
            {
                const auto &typedObject = *value.getTypedObject();
                NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:typedObject.getPropertiesSize()];
                for (const auto &property: typedObject) {
                    dictionary[NSStringFromString(property.name)] = NSObjectFromValue(property.value);
                }
                return dictionary;
            }
            case Valdi::ValueType::ProxyTypedObject:
            {
                return NSObjectFromProxyObject(*value.getProxyObject()) ?: [NSNull null];
            }
            case Valdi::ValueType::ValdiObject:
            {
                auto nativeObject = value.getValdiObject();
                id objcObject = NSObjectFromValdiObject(nativeObject, NO);
                if (objcObject != nullptr) {
                    return objcObject;
                } else {
                    return NSObjectWrappingValue(value);
                }
            }
            case Valdi::ValueType::Error:
            {
                NSString *reason = NSStringFromSTDStringView(value.getError().toString());
                return [[SCValdiError alloc] initWithReason:reason];
            }
            case Valdi::ValueType::Function:
            {
                return FunctionFromValue(value);
            }
        }
    }

id<SCValdiFunction> FunctionFromValueFunction(const Valdi::Ref<Valdi::ValueFunction>& valueFunction) {
    auto objcFunction = Valdi::castOrNull<ValueFunctionWithSCValdiFunction>(valueFunction);

    if (objcFunction != nullptr) {
        // Unwrap the original native object.
        return objcFunction->getFunction();
    } else {
        return [[SCValdiFunctionWithCPPFunction alloc] initWithCPPFunction:valueFunction];
    }
}

    id<SCValdiFunction> FunctionFromValue(const Valdi::Value &value) {
        auto function = value.getFunctionRef();
        if (function == nullptr) {
            return nil;
        }

        return FunctionFromValueFunction(function);
    }

    NSString* NSStringFromStringView(const std::string_view& str) {
        return [[NSString alloc] initWithBytes:str.data()
                                        length:static_cast<NSUInteger>(str.size())
                                      encoding:NSUTF8StringEncoding];
    }

    NSString* NSStringFromUTF16StringView(const std::u16string_view& str) {
        return [NSString stringWithCharacters:reinterpret_cast<const unichar *>(str.data()) length:static_cast<NSUInteger>(str.size())];
    }

    NSString* NSStringFromStaticString(const Valdi::StaticString& str) {
        switch (str.encoding()) {
            case Valdi::StaticString::Encoding::UTF8:
                return NSStringFromStringView(str.utf8StringView());
            case Valdi::StaticString::Encoding::UTF16:
                return NSStringFromUTF16StringView(str.utf16StringView());
            case Valdi::StaticString::Encoding::UTF32: {
                auto storage = str.utf8Storage();
                return NSStringFromStringView(storage.toStringView());
            }
        }
    }

    NSString *NSStringFromString(const Valdi::StringBox &stringBox) {
        return NSStringFromStringView(stringBox.toStringView());
    }

    NSString *NSStringFromSTDStringView(const std::string_view &stdString) {
        return NSStringFromStringView(stdString);
    }

    Valdi::StringBox StringFromNSString(__unsafe_unretained NSString *nsString) {
        return InternedStringFromNSString(nsString);
    }

    Valdi::StringBox InternedStringFromNSString(__unsafe_unretained NSString *nsString) {
        return Valdi::StringCache::getGlobal().makeStringFromLiteral(StringViewFromNSString(nsString));
    }

    Valdi::Ref<Valdi::StaticString> StaticStringFromNSString(NSString *str) {
        auto length = str.length;
        auto output = Valdi::StaticString::makeUTF16(static_cast<size_t>(length));
        [str getCharacters:reinterpret_cast<unichar *>(output->utf16Data())];
        return output;
    }

    std::string_view StringViewFromNSString(__unsafe_unretained NSString *nsString) {
        return std::string_view(nsString.UTF8String, [nsString lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
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

    Valdi::Value ValueFromNSArray(NSArray<id> *array) {
        auto valueArray = Valdi::ValueArray::make(array.count);

        size_t i = 0;
        for (id item in array) {
            valueArray->emplace(i++, ValueFromNSObject(item));
        }

        return Valdi::Value(valueArray);
    }

    Valdi::Value ValueFromNSData(__unsafe_unretained NSData *data) {
        auto buffer = BufferFromNSData(data);

        return Valdi::Value(Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType, buffer));
    }

    template<typename T, typename F>
    Valdi::Result<T> objCEntry(F &&func) {
        @try {
            return func();
        } @catch (SCValdiError *exception) {
            return ErrorFromNSException(exception);
        }
    }

    Valdi::Value ValueFromMarshallable(id<SCValdiMarshallable> marshallable) {
        auto result = objCEntry<Valdi::Value>([&](){
            Valdi::SimpleExceptionTracker exceptionTracker;
            Valdi::Marshaller marshaller(exceptionTracker);
            NSInteger index = [marshallable pushToValdiMarshaller:SCValdiMarshallerWrap(&marshaller)];
            if (!exceptionTracker) {
                NSExceptionThrowFromError(exceptionTracker.extractError());
            }

            auto result = marshaller.get(static_cast<int>(index));

            return result;
        });

        if (!result) {
            return Valdi::Value(result.moveError());
        }

        return result.moveValue();
    }

    Valdi::Value ValueFromFunction(id<SCValdiFunction> function) {
        if ([function isKindOfClass:[SCValdiFunctionWithCPPFunction class]]) {
            return Valdi::Value([((SCValdiFunctionWithCPPFunction *)function) getFunction]);
        }

        return Valdi::Value(Valdi::makeShared<ValueFunctionWithSCValdiFunction>(function));
    }

    Valdi::Value ValueFromNSObject(id object) {
        if (!object || object == [NSNull null]) {
            return Valdi::Value();
        } else if ([object isKindOfClass:[NSString class]]) {
            const auto convertedString = StringFromNSString(object);

            return Valdi::Value(convertedString);
        } else if ([object isKindOfClass:[NSNumber class]]) {
            return Valdi::Value([object doubleValue]);
        } else if ([object isKindOfClass:[NSDictionary class]]) {
            return ValueFromNSDictionary(object);
        } else if ([object isKindOfClass:[NSArray class]]) {
            return ValueFromNSArray(object);
        } else if ([object isKindOfClass:[NSData class]]) {
            return ValueFromNSData(object);
        } else if ([object isKindOfClass:[SCValdiWrappedValue class]]) {
            SCValdiWrappedValue *cppObject = (SCValdiWrappedValue *)object;
            return cppObject.value;
        } else if ([object respondsToSelector:@selector(pushToValdiMarshaller:)]) {
            return ValueFromMarshallable(object);
        } else if ([object respondsToSelector:@selector(performWithMarshaller:)]) {
            return ValueFromFunction(object);
        } else if ([object respondsToSelector:@selector(valdi_toNative:)]) {
            Valdi::Value out;
            [object valdi_toNative:&out];

            return out;
        } else {
            return Valdi::Value(ValdiObjectFromNSObject(object));
        }
    }

    NSData *NSDataFromValue(const Valdi::Value &value) {
        const auto &typedArray = value.getTypedArray();
        if (typedArray == nullptr) {
            return nil;
        }
        return NSDataFromBuffer(typedArray->getBuffer());
    }

    NSData *NSDataFromBuffer(const Valdi::BytesView &buffer) {
        auto *objcCObject = dynamic_cast<ObjCObject *>(buffer.getSource().get());
        if (objcCObject != nullptr) {
            NSData *data = ObjectAs(objcCObject->getValue(), NSData);
            if (data && data.bytes == buffer.data() && data.length == buffer.size()) {
                // If the BytesView holds an NSData object that matches the BytesView pointer
                // and length, we can just re-use the NSData instance directly without
                // creating a new one.
                return data;
            }
        }

        auto *source = Valdi::unsafeBridgeRetain(buffer.getSource().get());
        return [[NSData alloc] initWithBytesNoCopy:const_cast<Valdi::Byte *>(buffer.data()) length:buffer.size() deallocator:^(void *bytes, NSUInteger length) {
            Valdi::unsafeBridgeRelease(source);
        }];
    }

    Valdi::BytesView BufferFromNSData(__unsafe_unretained NSData *data) {
        if (!data) {
            return {};
        }

        return Valdi::BytesView(ValdiObjectFromNSObject(data),
                                   reinterpret_cast<const Valdi::Byte *>(data.bytes),
                                   static_cast<size_t>(data.length));
    }

    Valdi::SharedValdiObject ValdiObjectFromNSObject(id object) {
        if ([object isKindOfClass:[SCNValdiCoreAsset class]]) {
            auto asset = Valdi::castOrNull<Valdi::Asset>(djinni_generated_client::valdi_core::Asset::toCpp(object));
            if (asset != nullptr) {
                return asset;
            }
        } else if ([object isKindOfClass:[SCValdiCppObject class]]) {
            return Valdi::castOrNull<Valdi::ValdiObject>([(SCValdiCppObject *)object ref]);
        }
        return Valdi::SharedValdiObject(Valdi::makeShared<ObjCValdiObject>(object));
    }

Valdi::Ref<Valdi::ValueTypedProxyObject> ProxyObjectFromNSObject(id object, const Valdi::Ref<Valdi::ValueTypedObject>& typedObject, BOOL strongRef) {
    return Valdi::makeShared<ObjCProxyObject>(typedObject, object, strongRef);
}

id NSObjectFromProxyObject(const Valdi::ValueTypedProxyObject& proxyObject) {
    auto objcProxy = dynamic_cast<const ObjCProxyObject *>(&proxyObject);
    if (objcProxy == nullptr) {
        return nil;
    }

    return objcProxy->getValue();
}

id NSObjectFromValdiObject(const Valdi::SharedValdiObject &valdiObject, BOOL wrapIfNeeded) {
    auto *objcObject = dynamic_cast<ObjCObject *>(valdiObject.get());
    if (objcObject != nullptr) {
        return objcObject->getValue();
    }

    auto asset = Valdi::castOrNull<Valdi::Asset>(valdiObject);
    if (asset != nullptr) {
        return djinni_generated_client::valdi_core::Asset::fromCpp(asset.toShared());
    }

    if (wrapIfNeeded && valdiObject != nullptr) {
        return [SCValdiCppObject objectWithRef:valdiObject];
    }

    return nil;
}

Valdi::Shared<Valdi::ValueConvertible> toValueConvertible(id object) {
    if (!object) {
        return nullptr;
    }

    return Valdi::makeShared<ObjCObject>(object);
}

Valdi::Shared<Valdi::ValueConvertible> toLazyValueConvertible(id object) {
    if (!object) {
        return nullptr;
    }

    auto objcObject = Valdi::makeShared<ObjCObject>(object);

    return Valdi::makeShared<Valdi::LazyValueConvertible>([objcObject]() {
        return objcObject->toValue();
    });
}

Valdi::Shared<Valdi::ValueConvertible> toLazyValueConvertible(const Valdi::Value& value) {
    return Valdi::makeShared<Valdi::LazyValueConvertible>([value]() { return value; });
}

NSURL *NSURLFromString(const Valdi::StringBox &urlString) {
    return CFBridgingRelease(CFURLCreateWithBytes(kCFAllocatorDefault,
                                                  reinterpret_cast<const UInt8 *>(urlString.getCStr()), urlString.length(),
                                                  kCFStringEncodingUTF8,
                                                  nil));
}

NSError *NSErrorFromError(const Valdi::Error &error) {
    NSString *errorMessage = NSStringFromSTDStringView(error.toString());
    return [NSError errorWithDomain:@"com.snap.valdi" code:0 userInfo:@{NSLocalizedDescriptionKey: errorMessage}];
}

Valdi::Error ErrorFromNSError(NSError *error) {
    return Valdi::Error(InternedStringFromNSString(error.localizedDescription));
}

void NSExceptionThrowFromError(const Valdi::Error& error) {
    auto flattenedError = error.flatten();
    NSString *message = NSStringFromString(flattenedError.getMessage());
    NSString *stackTrace = nil;

    if (!flattenedError.getStack().isEmpty()) {
        stackTrace = NSStringFromString(flattenedError.getStack());
    }

    SCValdiErrorThrowWithStacktrace(message, stackTrace);
}

Valdi::Error ErrorFromNSException(NSException* exception) {
    auto message = StringFromNSString(exception.reason);
    NSString *stackTrace = SCValdiErrorGetStackTrace(exception);

    if (stackTrace) {
        return Valdi::Error(std::move(message), StringFromNSString(stackTrace), nullptr);
    } else {
        return Valdi::Error(std::move(message));
    }
}


}
