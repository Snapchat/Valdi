//
//  SCValdiAttributedText.m
//  valdi-ios
//
//  Created by Simon Corsin on 12/19/22.
//

#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiWrappedValue+Private.h"

@implementation SCValdiAttributedText {
    Valdi::Ref<Valdi::TextAttributeValue> _cppInstance;
}

- (instancetype)initWithCppInstance:(void *)cppInstance
{
    self = [super init];

    if (self) {
        _cppInstance = Valdi::unsafeBridge<Valdi::TextAttributeValue>(cppInstance);
    }

    return self;
}

- (instancetype)initWithWrappedValue:(SCValdiWrappedValue*)wrappedValue
{
    self = [super init];

    if (self) {
        _cppInstance = wrappedValue.value.getTypedRef<Valdi::TextAttributeValue>();
        NSAssert(_cppInstance != nullptr, @"Wrapped value is not a TextAttributeValue");
    }

    return self;
}

- (void)dealloc
{
    _cppInstance = nullptr;
}

- (NSUInteger)partsCount
{
    return (NSUInteger)_cppInstance->getPartsSize();
}

- (NSString *)contentAtIndex:(NSUInteger)index
{
    const auto &content = _cppInstance->getContentAtIndex(index);
    return ValdiIOS::NSStringFromString(content);
}

- (nullable NSString *)fontAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.font) {
        return nil;
    }
    return ValdiIOS::NSStringFromString(style.font.value());
}

- (SCValdiTextDecoration)textDecorationAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    switch (style.textDecoration) {
        case Valdi::TextDecoration::Unset:
            return SCValdiTextDecorationUnset;
        case Valdi::TextDecoration::None:
            return SCValdiTextDecorationNone;
        case Valdi::TextDecoration::Strikethrough:
            return SCValdiTextDecorationStrikethrough;
        case Valdi::TextDecoration::Underline:
            return SCValdiTextDecorationUnderline;
    }
}

- (nullable UIColor *)colorAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.color) {
        return nil;
    }

    return UIColorFromValdiAttributeValue(style.color.value().value);
}

- (nullable id<SCValdiFunction>)onTapAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (style.onTap == nullptr) {
        return nil;
    }

    return ValdiIOS::FunctionFromValueFunction(style.onTap);
}

- (nullable id<SCValdiFunction>)onLayoutAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (style.onLayout == nullptr) {
        return nil;
    }

    return ValdiIOS::FunctionFromValueFunction(style.onLayout);
}

- (nullable UIColor *)outlineColorAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.outlineColor) {
        return nil;
    }

    return UIColorFromValdiAttributeValue(style.outlineColor.value().value);
}


- (nullable NSNumber *)outlineWidthAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.outlineWidth) {
        return nil;
    }

    return @(style.outlineWidth.value());
}

- (nullable UIColor *)outerOutlineColorAtIndex:(NSUInteger)index
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.outerOutlineColor) {
        return nil;
    }

    return UIColorFromValdiAttributeValue(style.outerOutlineColor.value().value);
}

- (nullable NSNumber*)outerOutlineWidthAtIndex:(NSUInteger)index;
{
    const auto &style = _cppInstance->getStyleAtIndex(index);
    if (!style.outerOutlineWidth) {
        return nil;
    }

    return @(style.outerOutlineWidth.value());
}


@end
