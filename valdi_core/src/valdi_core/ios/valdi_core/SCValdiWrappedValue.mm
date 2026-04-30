//
//  SCValdiWrappedValue.m
//  valdi-ios
//
//  Created by Simon Corsin on 8/29/18.
//

#import "valdi_core/SCValdiWrappedValue+Private.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiMarshallableObjectDescriptor.h"

@implementation SCValdiWrappedValue {
    Valdi::Value _value;
}

- (instancetype)initWithValue:(Valdi::Value)value
{
    self = [super init];

    if (self) {
        _value = std::move(value);
    }

    return self;
}

- (const Valdi::Value &)value
{
    return _value;
}

- (void)valdi_toNative:(void *)value
{
    *reinterpret_cast<Valdi::Value *>(value) = _value;
}

- (NSString *)debugString
{
    return ValdiIOS::NSStringFromString(_value.toStringBox());
}

- (NSString *)debugDescription
{
    return [NSString stringWithFormat:@"<%@ wrapping %@>", self.description, self.debugString];
}

+ (SCValdiMarshallableObjectDescriptor)valdiMarshallableObjectDescriptor
{
    return SCValdiMarshallableObjectDescriptorMake(nil, nil, nil, SCValdiMarshallableObjectTypeUntyped);
}

@end
