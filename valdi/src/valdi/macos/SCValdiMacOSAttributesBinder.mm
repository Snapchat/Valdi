//
//  SCValdiMacOSAttributesBinder.m
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import "SCValdiMacOSAttributesBinder.h"

#import "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#import "valdi/runtime/Views/View.hpp"

#import "valdi/macos/SCValdiObjCUtils.h"
#import "valdi/macos/SCValdiMacOSViewManager.h"
#import "valdi/macos/Views/SCValdiSurfacePresenterView.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

typedef void (*SCValdiObjectSetter)(id, SEL, id);

static const void *SCValdiOriginalAccessibilityRoleKey = &SCValdiOriginalAccessibilityRoleKey;
static const void *SCValdiOriginalAccessibilityElementKey = &SCValdiOriginalAccessibilityElementKey;
static const void *SCValdiOriginalAccessibilityEnabledKey = &SCValdiOriginalAccessibilityEnabledKey;

@interface NSView (SCValdiAccessibility)
- (void)valdi_setAccessibilityCategory:(nullable NSString *)category;
- (void)valdi_setAccessibilityStateDisabled:(nullable NSNumber *)disabled;
- (void)valdi_setAccessibilityStateSelected:(nullable NSNumber *)selected;
@end

@implementation NSView (SCValdiAccessibility)

- (void)valdi_storeOriginalAccessibilityPropertiesIfNeeded
{
    if (objc_getAssociatedObject(self, SCValdiOriginalAccessibilityRoleKey) != nil) {
        return;
    }
    objc_setAssociatedObject(
        self,
        SCValdiOriginalAccessibilityRoleKey,
        self.accessibilityRole ?: NSNull.null,
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(
        self,
        SCValdiOriginalAccessibilityElementKey,
        @(self.isAccessibilityElement),
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)valdi_restoreOriginalAccessibilityProperties
{
    id originalRole = objc_getAssociatedObject(self, SCValdiOriginalAccessibilityRoleKey);
    NSNumber *originalElement = objc_getAssociatedObject(self, SCValdiOriginalAccessibilityElementKey);
    self.accessibilityRole = originalRole == NSNull.null ? nil : originalRole;
    if (originalElement != nil) {
        self.accessibilityElement = originalElement.boolValue;
    }
}

- (void)valdi_setAccessibilityCategory:(NSString *)category
{
    [self valdi_storeOriginalAccessibilityPropertiesIfNeeded];
    if (category == nil || [category isEqualToString:@"auto"]) {
        [self valdi_restoreOriginalAccessibilityProperties];
        return;
    }

    NSAccessibilityRole role = NSAccessibilityGroupRole;
    if ([category isEqualToString:@"text"] || [category isEqualToString:@"header"]) {
        role = NSAccessibilityStaticTextRole;
    } else if ([category isEqualToString:@"button"] ||
               [category isEqualToString:@"image-button"] ||
               [category isEqualToString:@"keyboard-key"]) {
        role = NSAccessibilityButtonRole;
    } else if ([category isEqualToString:@"image"]) {
        role = NSAccessibilityImageRole;
    } else if ([category isEqualToString:@"input"]) {
        role = NSAccessibilityTextFieldRole;
    } else if ([category isEqualToString:@"link"]) {
        role = NSAccessibilityLinkRole;
    } else if ([category isEqualToString:@"checkbox"]) {
        role = NSAccessibilityCheckBoxRole;
    } else if ([category isEqualToString:@"radio"]) {
        role = NSAccessibilityRadioButtonRole;
    }

    self.accessibilityElement = YES;
    self.accessibilityRole = role;
}

- (void)valdi_setAccessibilityStateDisabled:(NSNumber *)disabled
{
    NSNumber *originalEnabled = objc_getAssociatedObject(self, SCValdiOriginalAccessibilityEnabledKey);
    if (disabled == nil) {
        if ([self isKindOfClass:NSControl.class]) {
            self.accessibilityEnabled = ((NSControl *)self).isEnabled;
        } else if (originalEnabled != nil) {
            self.accessibilityEnabled = originalEnabled.boolValue;
        }
        objc_setAssociatedObject(self, SCValdiOriginalAccessibilityEnabledKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        return;
    }

    if (originalEnabled == nil) {
        originalEnabled = @(self.isAccessibilityEnabled);
        objc_setAssociatedObject(
            self,
            SCValdiOriginalAccessibilityEnabledKey,
            originalEnabled,
            OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    BOOL nativeEnabled = originalEnabled.boolValue;
    if ([self isKindOfClass:NSControl.class]) {
        nativeEnabled = ((NSControl *)self).isEnabled;
    }
    self.accessibilityEnabled = !disabled.boolValue && nativeEnabled;
}

- (void)valdi_setAccessibilityStateSelected:(NSNumber *)selected
{
    self.accessibilitySelected = selected != nil && selected.boolValue;
}

@end

class AttributeHandler: public Valdi::AttributeHandlerDelegate {
public:
    AttributeHandler(SEL sel): _sel(sel) {}
    ~AttributeHandler() = default;

    Valdi::Result<Valdi::Void> onApply(Valdi::ViewTransactionScope &viewTransactionScope,
                                             Valdi::ViewNode& viewNode,
                                             const Valdi::Ref<Valdi::View> & view,
                                             const Valdi::StringBox &name,
                                             const Valdi::Value & value,
                                             const Valdi::Ref<Valdi::Animator> & animator) override {
        id objcValue = NSObjectFromValue(value);
        setAttribute(view, objcValue);
        return Valdi::Void();
    }

    void onReset(Valdi::ViewTransactionScope &viewTransactionScope,
                 Valdi::ViewNode& viewNode,
                 const Valdi::Ref<Valdi::View> & view,
                 const Valdi::StringBox &name,
                 const Valdi::Ref<Valdi::Animator> & animator) override {
        setAttribute(view, nil);
    }

    void setAttribute(const Valdi::Ref<Valdi::View> &view, id value) {
        id resolvedView = ValdiMacOS::fromValdiView(view);
        if (resolvedView == nil) {
            return;
        }
        IMP imp = class_getMethodImplementation([resolvedView class], _sel);
        ((SCValdiObjectSetter)imp)(resolvedView, _sel, value);
    }
private:
    SEL _sel;
};

@implementation SCValdiMacOSAttributesBinder {
    Valdi::AttributesBindingContext *_cppInstance;
    Class _cls;
}

- (instancetype)initWithCppInstance:(void *)cppInstance cls:(Class)cls
{
    self = [self init];

    if (self) {
        _cppInstance = (Valdi::AttributesBindingContext *)cppInstance;
        _cls = cls;
    }

    return self;
}

- (void)bindUntypedAttribute:(NSString *)attributeName invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange selector:(SEL)sel
{
    auto cppAttributeName = StringFromNSString(attributeName);
    _cppInstance->bindUntypedAttribute(cppAttributeName, invalidateLayoutOnChange, Valdi::makeShared<AttributeHandler>(sel));
}

- (void)bindColorAttribute:(NSString *)attributeName invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange selector:(SEL)sel
{
    auto cppAttributeName = StringFromNSString(attributeName);
    _cppInstance->bindColorAttribute(cppAttributeName, invalidateLayoutOnChange, Valdi::makeShared<AttributeHandler>(sel));
}

- (void)bindAccessibilityAttributes
{
    [self bindUntypedAttribute:@"accessibilityId"
      invalidateLayoutOnChange:NO
                      selector:@selector(setAccessibilityIdentifier:)];
    [self bindUntypedAttribute:@"accessibilityLabel"
      invalidateLayoutOnChange:NO
                      selector:@selector(setAccessibilityLabel:)];
    [self bindUntypedAttribute:@"accessibilityHint"
      invalidateLayoutOnChange:NO
                      selector:@selector(setAccessibilityHelp:)];
    [self bindUntypedAttribute:@"accessibilityValue"
      invalidateLayoutOnChange:NO
                      selector:@selector(setAccessibilityValue:)];
    [self bindUntypedAttribute:@"accessibilityCategory"
      invalidateLayoutOnChange:NO
                      selector:@selector(valdi_setAccessibilityCategory:)];
    [self bindUntypedAttribute:@"accessibilityStateDisabled"
      invalidateLayoutOnChange:NO
                      selector:@selector(valdi_setAccessibilityStateDisabled:)];
    [self bindUntypedAttribute:@"accessibilityStateSelected"
      invalidateLayoutOnChange:NO
                      selector:@selector(valdi_setAccessibilityStateSelected:)];
}

@end
