#import "valdi_core/SCValdiBigInt.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

@implementation SCValdiBigInt {
    Valdi::Ref<Valdi::BigInt> _value;
}

- (instancetype)initWithBigInt:(void*)bigInt {
    self = [super init];
    if (self) {
        _value = Valdi::unsafeBridge<Valdi::BigInt>(bigInt);
    }
    return self;
}

- (instancetype)initWithInt64:(int64_t)value {
    auto bigInt = Valdi::BigInt::make(value);
    return [self initWithBigInt:Valdi::unsafeBridgeCast(bigInt.get())];
}

- (instancetype)initWithString:(NSString*)decimal error:(NSError**)error {
    Valdi::SimpleExceptionTracker exceptionTracker;
    auto value = Valdi::BigInt::make(ValdiIOS::StringViewFromNSString(decimal), exceptionTracker);
    if (!exceptionTracker) {
        auto nativeError = exceptionTracker.extractError();
        if (error != nullptr) {
            *error = ValdiIOS::NSErrorFromError(nativeError);
        }
        return nil;
    }
    return [self initWithBigInt:Valdi::unsafeBridgeCast(value.get())];
}

- (void*)bigInt {
    return Valdi::unsafeBridgeCast(_value.get());
}

- (SCValdiBigIntInt64Result)toInt64 {
    auto value = _value->toInt64();
    return {value.value_or(0), value.has_value()};
}

- (NSString*)toString {
    return ValdiIOS::NSStringFromStringView(_value->toString());
}

- (NSString*)description {
    return [self toString];
}

- (BOOL)isEqual:(id)object {
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[SCValdiBigInt class]]) {
        return NO;
    }
    SCValdiBigInt* other = object;
    return *_value == *other->_value;
}

- (NSUInteger)hash {
    return _value->hash();
}

- (void)valdi_toNative:(void*)value {
    *static_cast<Valdi::Value*>(value) = Valdi::Value(_value);
}

@end
