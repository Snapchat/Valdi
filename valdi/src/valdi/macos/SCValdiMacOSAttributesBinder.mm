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

#import <objc/runtime.h>

typedef void (*SCValdiObjectSetter)(id, SEL, id);

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

@end
