//
//  SCValdiBridgeFunction.m
//  valdi_core-ios
//
//  Created by Simon Corsin on 1/31/23.
//

#import "valdi_core/SCValdiBridgeFunction.h"
#import "valdi_core/SCValdiMarshallableObjectRegistry.h"
#import "valdi_core/SCValdiMarshallableObjectUtils.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiMarshaller.h"

@implementation SCValdiBridgeFunction

- (id)callBlock
{
    return SCValdiFieldValueGetObject(SCValdiGetMarshallableObjectFieldsStorage(self)[0]);
}

+ (NSString *)modulePath
{
    NSString *className = NSStringFromClass([self class]);
    SCValdiErrorThrow([NSString stringWithFormat:@"Function class %@ should override the 'modulePath' class method", className]);
}

+ (BOOL)asyncStrictMode
{
    return NO;
}

+ (instancetype)functionWithJSRuntime:(id<SCValdiJSRuntime>)jsRuntime
{
    return SCValdiMakeBridgeFunctionFromJSRuntime(self, jsRuntime, [self modulePath]);
}

@end

id SCValdiMakeBridgeFunctionFromJSRuntime(Class objectClass,
                                             id<SCValdiJSRuntime> jsRuntime,
                                             NSString *path) {
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallableObjectRegistry *objectRegistry = SCValdiMarshallableObjectRegistryGetSharedInstance();
        [objectRegistry setSchemaOfClass:objectClass inMarshaller:marshaller];
        NSInteger objectIndex = [jsRuntime pushModuleAthPath:path inMarshaller:marshaller];
        SCValdiMarshallerCheck(marshaller);

        return [objectRegistry unmarshallObjectOfClass:objectClass fromMarshaller:marshaller atIndex:objectIndex];
    })
}
