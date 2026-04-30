#import "valdi_core/SCValdiMarshallableObjectDescriptor.h"
#import "valdi_core/SCValdiJSConversionUtils.h"

#import "SCValdiINavigatorPageConfig.h"

@implementation SCValdiINavigatorPageConfig

@dynamic componentPath;
@dynamic componentViewModel;
@dynamic componentContext;
@dynamic showPlatformNavigationBar;
@dynamic wrapInPlatformNavigationController;
@dynamic platformNavigationTitle;
@dynamic isPartiallyHiding;
@dynamic disableSwipeToDismissModal;
@dynamic showDimmingOverlay;

+ (SCValdiMarshallableObjectDescriptor)valdiMarshallableObjectDescriptor
{
    static const SCValdiMarshallableObjectFieldDescriptor kFields[] = {
        {.name = "componentPath", .selName = nil, .type = "s"},
        {.name = "componentViewModel", .selName = nil, .type = "m?<s, u>"},
        {.name = "componentContext", .selName = nil, .type = "m?<s, u>"},
        {.name = "showPlatformNavigationBar", .selName = nil, .type = "b@?"},
        {.name = "wrapInPlatformNavigationController", .selName = nil, .type = "b@?"},
        {.name = "platformNavigationTitle", .selName = nil, .type = "s?"},
        {.name = "isPartiallyHiding", .selName = nil, .type = "b@?"},
        {.name = "disableSwipeToDismissModal", .selName = nil, .type = "b@?"},
        {.name = "showDimmingOverlay", .selName = nil, .type = "b@?"},
        {.name = nil},
    };
    return SCValdiMarshallableObjectDescriptorMake(kFields, nil, nil, NO);
}

@end
