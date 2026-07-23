//
//  SCValdiAttributesBinder.m
//  iOS
//
//  Created by Simon Corsin on 4/18/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/SCValdiAttributesBinder.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"
#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/Text/SCValdiAttributedText.h"

#import "valdi_core/SCNValdiCoreAnimator+Private.h"

#import "valdi_core/SCValdiActions.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/UIView+ValdiBase.h"

#import "valdi_core/UIView+ValdiObjects.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"

#import "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#import "valdi/ios/CPPBindings/UIViewHolder.h"
#import "valdi/runtime/Attributes/ValueConverters.hpp"
#import "valdi/runtime/Views/PlaceholderViewMeasureDelegate.hpp"
#import "valdi/runtime/Views/ViewAttributeHandlerDelegate.hpp"

#import "valdi/runtime/Attributes/Animator.hpp"
#import "valdi/runtime/Context/ViewNodeTree.hpp"
#import "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#import "valdi_core/SCNValdiCoreCompositeAttributePart+Private.h"
#import "valdi_core/SCValdiResult+CPP.h"

@interface SCValdiViewLayoutAttributes: NSObject <SCValdiViewLayoutAttributes>

- (instancetype)initWithAttributes:(Valdi::Ref<Valdi::ValueMap>)attributes;

@end

namespace ValdiIOS {

class IOSAttributeHandlerDelegate: public Valdi::ViewAttributeHandlerDelegate {
public:
    IOSAttributeHandlerDelegate(id applyBlock, SCValdiAttributeBindMethodReset resetBlock): _applyBlock([applyBlock copy]), _resetBlock([resetBlock copy]) {}

    ~IOSAttributeHandlerDelegate() override = default;

    Valdi::Result<Valdi::Void> onViewApply(const Valdi::Ref<Valdi::View> &view,
                                                 const Valdi::StringBox& name,
                                                 const Valdi::Value& value,
                                                 const Valdi::Ref<Valdi::Animator>& animator) final {
        UIView *uiView = ValdiIOS::UIViewHolder::uiViewFromRef(view);
        id apply = _applyBlock.getValue();

        if (!uiView || !apply) {
            return Valdi::Result<Valdi::Void>(Valdi::Error("View or apply apply is null"));
        }

        id<SCValdiAnimatorProtocol> objcAnimator = getAnimator(animator);

        if (!callApplyBlock(apply, uiView, value, objcAnimator)) {
            return Valdi::Error("Could not apply attribute");
        }

        return Valdi::Void();
    }

    void onViewReset(const Valdi::Ref<Valdi::View> &view,
                     const Valdi::StringBox& /*name*/,
                     const Valdi::Ref<Valdi::Animator>& animator) final {
        UIView *uiView = ValdiIOS::UIViewHolder::uiViewFromRef(view);
        SCValdiAttributeBindMethodReset reset = _resetBlock.getValue();
        if (reset && uiView) {
            reset(uiView, getAnimator(animator));
        }
    }

    virtual bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) = 0;

private:
    ValdiIOS::ObjCObjectDirectRef _applyBlock;
    ValdiIOS::ObjCObjectDirectRef _resetBlock;

    id<SCValdiAnimatorProtocol> getAnimator(const Valdi::Ref<Valdi::Animator> &animator) const {
        if (animator == nullptr) {
            return nil;
        }

        return (id<SCValdiAnimatorProtocol>)djinni_generated_client::valdi_core::Animator::fromCpp(animator->getNativeAnimator());
    }
};

class UntypedAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        id objcValue = ValdiIOS::NSObjectFromValue(value);
        return ((SCValdiAttributeBindMethodUntyped)apply)(view, objcValue, animator);
    }
};

class StringAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        NSString *string = ValdiIOS::NSStringFromString(value.toStringBox());
        return ((SCValdiAttributeBindMethodString)apply)(view, string, animator);
    }
};

class DoubleAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        return ((SCValdiAttributeBindMethodDouble)apply)(view, (CGFloat)value.toDouble(), animator);
    }
};

class IntAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        return ((SCValdiAttributeBindMethodInt)apply)(view, (NSInteger)value.toInt(), animator);
    }
};

class BooleanAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        return ((SCValdiAttributeBindMethodBool)apply)(view, (BOOL)value.toBool(), animator);
    }
};

class ColorAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        UIColor *color = UIColorFromValdiAttributeValue(value.toLong());

        return ((SCValdiAttributeBindMethodColor)apply)(view, color, animator);
    }
};

class TextAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        id objcValue;
        if (value.isString()) {
            objcValue = ValdiIOS::NSStringFromString(value.toStringBox());
        } else if (value.isValdiObject()) {
            auto attributedText = value.getTypedRef<Valdi::TextAttributeValue>();
            if (attributedText == nullptr) {
                return false;
            }

            objcValue = [[SCValdiAttributedText alloc] initWithCppInstance:Valdi::unsafeBridgeCast(attributedText.get())];
        } else {
            return false;
        }

        return ((SCValdiAttributeBindMethodUntyped)apply)(view, objcValue, animator);
    }
};

SCValdiDoubleValue makeDoubleValue(double value, bool isPercent) {
    return isPercent ? SCValdiDoubleMakePercent((CGFloat)value) : SCValdiDoubleMakeValue((CGFloat)value);
}

class BorderAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        auto result = Valdi::ValueConverter::toBorderValues(value);
        if (!result) {
            return false;
        }
        Valdi::Ref<Valdi::BorderRadius> border = result.moveValue();

        SCValdiCornerValues cornerValues;
        cornerValues.topLeft = makeDoubleValue(border->getTopLeftValue(), border->isTopLeftPercent());
        cornerValues.topRight = makeDoubleValue(border->getTopRightValue(), border->isTopRightPercent());
        cornerValues.bottomRight = makeDoubleValue(border->getBottomRightValue(), border->isBottomRightPercent());
        cornerValues.bottomLeft = makeDoubleValue(border->getBottomLeftValue(), border->isBottomLeftPercent());

        return ((SCValdiAttributeBindMethodBorders)apply)(view, cornerValues, animator);
    }
};

class PercentAttributeHandlerDelegate: public IOSAttributeHandlerDelegate {
public:
    using IOSAttributeHandlerDelegate::IOSAttributeHandlerDelegate;

    bool callApplyBlock(__unsafe_unretained id apply, __unsafe_unretained UIView *view, const Valdi::Value& value, __unsafe_unretained id<SCValdiAnimatorProtocol> animator) override {
        auto result = Valdi::ValueConverter::toPercent(value);
        if (!result) {
            return false;
        }
        auto percentValue = result.value();
        SCValdiDoubleValue objcValue = makeDoubleValue(percentValue.value, percentValue.isPercent);

        return ((SCValdiAttributeBindMethodPercent)apply)(view, objcValue, animator);
    }
};

class IOSPlaceholderViewMeasureDelegate: public Valdi::PlaceholderViewMeasureDelegate {
public:
    IOSPlaceholderViewMeasureDelegate(SCValdiPlaceholderViewMeasureDelegate block): _block(block) {}
    ~IOSPlaceholderViewMeasureDelegate() override = default;

    Valdi::Ref<Valdi::View> createPlaceholderView() override {
        SCValdiPlaceholderViewMeasureDelegate block = _block.getValue();

        UIView *measurePlaceholderView = nil;

        UIViewHolder::executeSyncInMainThread([&]() {
            measurePlaceholderView = block();
        });

        if (!measurePlaceholderView) {
            return nullptr;
        }

        return UIViewHolder::makeWithUIView(measurePlaceholderView);
    }

    Valdi::Size measureView(const Valdi::Ref<Valdi::View>& view, float width, Valdi::MeasureMode widthMode, float height, Valdi::MeasureMode heightMode) override {
        Valdi::Size size;

        UIViewHolder::executeSyncInMainThread([&]() {
            UIView *uiView = ValdiIOS::UIViewHolder::uiViewFromRef(view);

            const CGFloat constrainedWidth = (widthMode == Valdi::MeasureModeUnspecified) ? CGFLOAT_MAX : width;
            const CGFloat constrainedHeight = (heightMode == Valdi::MeasureModeUnspecified) ? CGFLOAT_MAX : height;

            CGSize sizeThatFits = [uiView sizeThatFits:(CGSize){
                .width = constrainedWidth, .height = constrainedHeight,
            }];
            size = Valdi::Size(static_cast<float>(sizeThatFits.width), static_cast<float>(sizeThatFits.height));
        });

        return size;
    }

private:
    ValdiIOS::ObjCObjectDirectRef _block;
};

class IOSMeasureDelegate: public Valdi::MeasureDelegate {
public:
    IOSMeasureDelegate(SCValdiMeasureDelegate block): _block([block copy]) {}

    ~IOSMeasureDelegate() override = default;

    Valdi::Size measure(Valdi::ViewNode& viewNode, float width, Valdi::MeasureMode widthMode, float height, Valdi::MeasureMode heightMode) override {
        auto cppAttributes = viewNode.copyProcessedViewLayoutAttributes();

        if (!cppAttributes) {
            return Valdi::Size();
        }

        SCValdiMeasureDelegate block = _block.getValue();
        SCValdiContext *valdiContext = getValdiContext(viewNode.getViewNodeTree()->getContext());

        const CGFloat constrainedWidth = (widthMode == Valdi::MeasureModeUnspecified) ? CGFLOAT_MAX : width;
        const CGFloat constrainedHeight = (heightMode == Valdi::MeasureModeUnspecified) ? CGFLOAT_MAX : height;

        SCValdiViewLayoutAttributes *attributes = [[SCValdiViewLayoutAttributes alloc] initWithAttributes:cppAttributes.moveValue()];

        CGSize outputSize = block(attributes, CGSizeMake(constrainedWidth, constrainedHeight), valdiContext.traitCollection);

        return Valdi::Size(static_cast<float>(outputSize.width), static_cast<float>(outputSize.height));
    }

private:
    ValdiIOS::ObjCObjectDirectRef _block;

};

}

@implementation SCValdiAttributesBinder {
    Valdi::AttributesBindingContext *_bindingContext;
    id<SCValdiFontManagerProtocol> _fontManager;
}

- (instancetype)initWithNativeAttributesBindingContext:(SCValdiAttributesBinderNative *)nativeAttributesBindingContext
                                           fontManager:(id<SCValdiFontManagerProtocol>)fontManager
{
    self = [super init];

    if (self) {
        _bindingContext = reinterpret_cast<Valdi::AttributesBindingContext *>(nativeAttributesBindingContext);
        _fontManager = fontManager;
    }

    return self;
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindUntypedAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                          invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::UntypedAttributeHandlerDelegate>(untypedBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
             withStringBlock:(SCValdiAttributeBindMethodString)stringBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindStringAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::StringAttributeHandlerDelegate>(stringBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
             withDoubleBlock:(SCValdiAttributeBindMethodDouble)doubleBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindDoubleAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                         Valdi::makeShared<ValdiIOS::DoubleAttributeHandlerDelegate>(doubleBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                withIntBlock:(SCValdiAttributeBindMethodInt)intBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindIntAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::IntAttributeHandlerDelegate>(intBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
               withBoolBlock:(SCValdiAttributeBindMethodBool)boolBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindBooleanAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::BooleanAttributeHandlerDelegate>(boolBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
              withColorBlock:(SCValdiAttributeBindMethodColor)colorBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindColorAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::ColorAttributeHandlerDelegate>(colorBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withBordersBlock:(SCValdiAttributeBindMethodBorders)bordersBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindBorderAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                         invalidateLayoutOnChange,
                                         Valdi::makeShared<ValdiIOS::BorderAttributeHandlerDelegate>(bordersBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withPercentBlock:(SCValdiAttributeBindMethodPercent)percentBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindPercentAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                          invalidateLayoutOnChange,
                                          Valdi::makeShared<ValdiIOS::PercentAttributeHandlerDelegate>(percentBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName withFunctionBlock:(SCValdiAttributeBindMethodFunction)functionBlock resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
{
    [self bindAttribute:attributeName
invalidateLayoutOnChange:NO
       withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol>animator) {
           id<SCValdiFunction> function;
           if ([attributeValue isKindOfClass:[NSString class]]) {
               function = ProtocolAs([view.valdiContext.actions actionForName:attributeValue], SCValdiFunction);
           } else {
               function = ProtocolAs(attributeValue, SCValdiFunction);
           }

           [view.valdiViewNode setRetainedObject:attributeValue forKey:attributeName];

           if (!function) {
               return NO;
           }

           functionBlock(view, function);
           return YES;
       }
             resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol>animator) {
                 [view.valdiViewNode setRetainedObject:nil forKey:attributeName];
                 if (resetBlock) {
                     resetBlock(view);
                 }
             }];
}

- (void)bindAttribute:(NSString *)attributeName
      withActionBlock:(SCValdiAttributeBindMethodAction)actionBlock
           resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
{
    [self bindAttribute:attributeName
        invalidateLayoutOnChange:NO
        withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol>animator) {
            id<SCValdiAction> action = nil;

            if ([attributeValue isKindOfClass:[NSString class]]) {
                action = [view.valdiContext.actions actionForName:attributeValue];
            } else {
                action = ProtocolAs(attributeValue, SCValdiAction);
            }

            if (!action) {
                return NO;
            }

            [view.valdiViewNode setRetainedObject:attributeValue forKey:attributeName];

            actionBlock(view, action);
            return YES;
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol>animator) {
            [view.valdiViewNode setRetainedObject:nil forKey:attributeName];
            if (resetBlock) {
                resetBlock(view);
            }
        }];
}

- (void)bindAttribute:(NSString *)attributeName
withFunctionAndPredicateBlock:(SCValdiAttributeBindMethodFunctionAndPredicate)actionBlock
           resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
{
    [self bindAttribute:attributeName additionalAttribute:nil withFunctionAndPredicateBlock:actionBlock resetBlock:resetBlock];
}

- (void)bindAttribute:(NSString *)attributeName
  additionalAttribute:(SCNValdiCoreCompositeAttributePart *)additionalAttribute
withFunctionAndPredicateBlock:(SCValdiAttributeBindMethodFunctionAndPredicate)actionBlock
           resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
{
    NSString *predicateAttributeName = [attributeName stringByAppendingString:@"Predicate"];
    NSString *compositeAttributeName = [attributeName stringByAppendingString:@"Composite"];
    NSString *disabledAttributeName = [attributeName
        stringByAppendingString:@"Disabled"];

    SCNValdiCoreCompositeAttributePart *touchHandlerCompositePart = [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:attributeName type:SCNValdiCoreAttributeTypeUntyped optional:NO invalidateLayoutOnChange:NO];
    SCNValdiCoreCompositeAttributePart *predicateHandlerCompositePart = [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:predicateAttributeName type:SCNValdiCoreAttributeTypeUntyped optional:YES invalidateLayoutOnChange:NO];
    SCNValdiCoreCompositeAttributePart *disabledHandlerCompositePart =
        [[SCNValdiCoreCompositeAttributePart alloc]
         initWithAttribute:disabledAttributeName type:SCNValdiCoreAttributeTypeBoolean optional:YES invalidateLayoutOnChange:NO];

    NSMutableArray<SCNValdiCoreCompositeAttributePart *> *parts = [NSMutableArray arrayWithCapacity:4];
    [parts addObject:touchHandlerCompositePart];
    [parts addObject:predicateHandlerCompositePart];
    [parts addObject:disabledHandlerCompositePart];
    if (additionalAttribute) {
        [parts addObject:additionalAttribute];
    }

    [self bindCompositeAttribute:compositeAttributeName parts:parts withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol>animator) {

        NSArray *parts = ObjectAs(attributeValue, NSArray);
        if (parts.count < 3) {
            return NO;
        }

        BOOL disabled = ObjectAs(parts[2], NSNumber).boolValue;
        if (disabled) {
            [view.valdiViewNode setRetainedObject:nil forKey:attributeName];
            [view.valdiViewNode setRetainedObject:nil forKey:compositeAttributeName];
            resetBlock(view);
            return YES;
        }

        id touchHandler = parts[0];

        id<SCValdiFunction> func = ProtocolAs(touchHandler, SCValdiFunction);

        if (!func) {
            return NO;
        }

        [view.valdiViewNode setRetainedObject:attributeValue forKey:attributeName];

        id predicateHandler = ProtocolAs(parts[1], SCValdiFunction);

        id additionalValue = parts.count > 3 ? parts[3] : nil;

        actionBlock(view, func, predicateHandler, additionalValue);
        return YES;

    } resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol>animator) {
        [view.valdiViewNode setRetainedObject:nil forKey:attributeName];
        [view.valdiViewNode setRetainedObject:nil forKey:compositeAttributeName];

        if (resetBlock) {
            resetBlock(view);
        }
    }];
}

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
              withArrayBlock:(SCValdiAttributeBindMethodArray)arrayBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    [self bindAttribute:attributeName
        invalidateLayoutOnChange:invalidateLayoutOnChange
                withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol>animator) {
                    if (![attributeValue isKindOfClass:[NSArray class]]) {
                        return NO;
                    }
                    return arrayBlock(view, attributeValue, animator);
                }
                      resetBlock:resetBlock];
}

- (void)bindCompositeAttribute:(NSString *)attributeName
                         parts:(NSArray<SCNValdiCoreCompositeAttributePart *> *)parts
              withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                    resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    std::vector<snap::valdi_core::CompositeAttributePart> cppParts;
    for (SCNValdiCoreCompositeAttributePart *part in parts) {
        cppParts.emplace_back(djinni_generated_client::valdi_core::CompositeAttributePart::toCpp(part));
    }

    _bindingContext->bindCompositeAttribute(ValdiIOS::InternedStringFromNSString(attributeName),
                                            cppParts,
                                            Valdi::makeShared<ValdiIOS::UntypedAttributeHandlerDelegate>(untypedBlock, resetBlock));
}

- (void)bindTransformAttributesWithUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                                     resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    _bindingContext->bindTransformAttributes(
        Valdi::makeShared<ValdiIOS::UntypedAttributeHandlerDelegate>(untypedBlock, resetBlock));
}

- (void)bindAttribute:(NSString *)attributeName
invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
       deserializer:(SCValdiAttributeDeserializer)deserializer
     withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
           resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    [self bindAttribute:attributeName invalidateLayoutOnChange:invalidateLayoutOnChange withUntypedBlock:untypedBlock resetBlock:resetBlock];

    [self registerPreprocessorForAttribute:attributeName withBlock:^id(id value) {
        return deserializer(value);
    }];
}

- (void)registerPreprocessorForAttribute:(NSString *)attributeName withBlock:(SCValdiAttributePreprocessor)block
{
    [self registerPreprocessorForAttribute:attributeName enableCache:NO withBlock:block];
}

- (void)registerPreprocessorForAttribute:(NSString *)attributeName enableCache:(BOOL)enableCache withBlock:(SCValdiAttributePreprocessor)block
{
    block = [block copy];

    Valdi::AttributePreprocessor preprocessor = [block](const Valdi::Value& value) -> Valdi::Result<Valdi::Value>  {
        id input = ValdiIOS::NSObjectFromValue(value);
        id result = block(input);
        if ([result isKindOfClass:[SCValdiResult class]]) {
            return SCValdiResultToPlatformResult(result);
        } else {
            return ValdiIOS::ValueFromNSObject(result);
        }
    };

    _bindingContext->registerPreprocessor(ValdiIOS::InternedStringFromNSString(attributeName), enableCache, std::move(preprocessor));
}

- (void)bindScrollAttributes
{
    _bindingContext->bindScrollAttributes();
}

- (void)bindAttribute:(NSString *)attributeName
invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
        withTextBlock:(SCValdiAttributeBindMethodUntyped)textBlock
           resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    auto cppAttributeName = ValdiIOS::InternedStringFromNSString(attributeName);
    _bindingContext->bindTextAttribute(cppAttributeName,
                                       invalidateLayoutOnChange,
                                       Valdi::makeShared<ValdiIOS::UntypedAttributeHandlerDelegate>(textBlock, resetBlock));
    _bindingContext->registerPreprocessor(cppAttributeName, true, [](const Valdi::Value &value) -> Valdi::Result<Valdi::Value> {
        if (value.isString()) {
            return value;
        }

        auto attributedText = value.getTypedRef<Valdi::TextAttributeValue>();
        if (attributedText == nullptr) {
            return Valdi::Error("Expecting AttributedText");
        }

        SCValdiAttributedText *objcAttributedText = [[SCValdiAttributedText alloc] initWithCppInstance:Valdi::unsafeBridgeCast(attributedText.get())];

        return Valdi::Value(ValdiIOS::ValdiObjectFromNSObject(objcAttributedText));
    });
}

- (void)bindAssetAttributesForOutputType:(SCNValdiCoreAssetOutputType)outputType
{
    _bindingContext->bindAssetAttributes(static_cast<snap::valdi_core::AssetOutputType>(outputType));
}

- (void)setPlaceholderViewMeasureDelegate:(SCValdiPlaceholderViewMeasureDelegate)placeholderViewMeasureDelegate
{
    _bindingContext->setMeasureDelegate(Valdi::makeShared<ValdiIOS::IOSPlaceholderViewMeasureDelegate>(placeholderViewMeasureDelegate));
}

- (void)setMeasureDelegate:(SCValdiMeasureDelegate)measureDelegate
{
    _bindingContext->setMeasureDelegate(Valdi::makeShared<ValdiIOS::IOSMeasureDelegate>(measureDelegate));
}

- (id<SCValdiFontManagerProtocol>)fontManager
{
    return _fontManager;
}

@end

@implementation SCValdiViewLayoutAttributes {
    Valdi::Ref<Valdi::ValueMap> _attributes;
}

- (instancetype)initWithAttributes:(Valdi::Ref<Valdi::ValueMap>)attributes
{
    self = [super init];

    if (self) {
        _attributes = std::move(attributes);
    }

    return self;
}

- (Valdi::Value)cppValueForAttributeName:(NSString *)attributeName
{
    auto str =  ValdiIOS::StringFromNSString(attributeName);
    const auto &it = _attributes->find(str);
    if (it == _attributes->end()) {
        return Valdi::Value();
    }
    return it->second;
}

- (id)valueForAttributeName:(NSString *)attributeName
{
    auto value = [self cppValueForAttributeName:attributeName];
    if (value.isNullOrUndefined()) {
        return nil;
    }
    return ValdiIOS::NSObjectFromValue(value);
}

- (BOOL)boolValueForAttributeName:(NSString *)attributeName
{
    auto value = [self cppValueForAttributeName:attributeName];
    return value.toBool();
}

- (NSString *)stringValueForAttributeName:(NSString *)attributeName
{
    id value = [self valueForAttributeName:attributeName];
    return ObjectAs(value, NSString);
}

- (CGFloat)doubleValueForAttributeName:(NSString *)attributeName
{
    auto value = [self cppValueForAttributeName:attributeName];
    return value.toDouble();
}

@end

extern "C" {
CGFloat SCValdiDoubleValueToRatio(SCValdiDoubleValue doubleValue)
{
    if (doubleValue.isPercent) {
        return doubleValue.value / 100.0;
    }
    return doubleValue.value;
}

SCValdiDoubleValue SCValdiDoubleMakeValue(CGFloat value)
{
    SCValdiDoubleValue outValue = {.value = value, .isPercent = false};
    return outValue;
}

SCValdiDoubleValue SCValdiDoubleMakePercent(CGFloat percent)
{
    SCValdiDoubleValue outValue = {.value = percent, .isPercent = true};

    return outValue;
}

}
