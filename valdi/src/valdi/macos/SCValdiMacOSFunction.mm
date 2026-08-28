//
//  SCValdiMacOSFunction.m
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import "valdi/macos/SCValdiMacOSFunction.h"
#import "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#import "valdi/macos/SCValdiObjCUtils.h"
#import "valdi_core/cpp/Utils/SmallVector.hpp"

@implementation SCValdiMacOSFunction {
    Valdi::Ref<Valdi::ValueFunction> _cppInstance;
}

- (instancetype)initWithCppInstance:(void *)cppInstance
{
    self = [self init];

    if (self) {
        _cppInstance = (Valdi::ValueFunction *)cppInstance;
    }

    return self;
}

- (instancetype)initWithBlock:(SCValdiMacOSFunctionBlock)block
{
    self = [self init];

    if (self) {
        SCValdiMacOSFunctionBlock blockCopy = [block copy];
        _cppInstance = Valdi::makeShared<Valdi::ValueFunctionWithCallable>([blockCopy](const Valdi::ValueFunctionCallContext &callContext) -> Valdi::Value {
            NSMutableArray *objcParameters = [[NSMutableArray alloc] initWithCapacity:callContext.getParametersSize()];

            for (size_t i = 0; i < callContext.getParametersSize(); i++) {
                id object = NSObjectFromValue(callContext.getParameter(i));
                if (!object) {
                    object = [NSNull null];
                }
                [objcParameters addObject:object];
            }

            id result = blockCopy(objcParameters);
            return ValueFromNSObject(result);
        });
    }

    return self;
}

- (void)performWithParameters:(NSArray<id> *)parameters
{
    Valdi::SmallVector<Valdi::Value, 8> outParameters;
    for (id parameter in parameters) {
        outParameters.emplace_back(ValueFromNSObject(parameter));
    }

    (*_cppInstance)(outParameters.data(), outParameters.size());
}

- (id)performWithParametersAndReturnValue:(NSArray<id> *)parameters
{
    Valdi::SmallVector<Valdi::Value, 8> outParameters;
    for (id parameter in parameters) {
        outParameters.emplace_back(ValueFromNSObject(parameter));
    }

    auto result = _cppInstance->call(
        Valdi::ValueFunctionFlagsCallSync, outParameters.data(), outParameters.size());
    if (!result) {
        return nil;
    }
    return NSObjectFromValue(result.value());
}

- (void *)cppInstance
{
    return _cppInstance.get();
}

@end
