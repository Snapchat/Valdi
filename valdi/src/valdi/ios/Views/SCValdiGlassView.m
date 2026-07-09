//
//  SCValdiGlassView.m
//  Valdi
//
//  Backs the `<glass>` element with an Apple "Liquid Glass" material (iOS 26+).
//

#import "valdi/ios/Views/SCValdiGlassView.h"

#import "valdi/ios/Categories/UIView+Valdi.h"

/// The `UIBlurEffect` style used when Liquid Glass is unavailable (iOS < 26, or
/// when the runtime guard below trips). Mirrors the `systemMaterial` fallback
/// used elsewhere in the app (see UIVisualEffect+Configuration.swift).
static UIBlurEffectStyle const kSCValdiGlassFallbackBlurStyle = UIBlurEffectStyleSystemMaterial;

/// `UIGlassEffect`'s initializer was observed crashing on some iOS 26 betas, so a
/// compile-time `@available` check is not sufficient — probe for the class and
/// initializer at runtime before using them. Mirrors the guard in the LensMaker
/// UIVisualEffect+Configuration.swift helper.
static BOOL _SCValdiIsGlassEffectAvailable(void)
{
#if defined(__IPHONE_26_0)
    if (@available(iOS 26.0, *)) {
        Class glassEffectClass = NSClassFromString(@"UIGlassEffect");
        return glassEffectClass != nil && [glassEffectClass respondsToSelector:@selector(effectWithStyle:)];
    }
#endif
    return NO;
}

@implementation SCValdiGlassView {
    BOOL _isClearStyle;
    BOOL _interactive;
    UIColor *_Nullable _tintColor;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _isClearStyle = NO;
        _interactive = NO;
        _tintColor = nil;
        [self _applyEffectWithAnimator:nil];
    }
    return self;
}

#pragma mark - SCValdiContentViewProviding

- (UIView *)contentViewForInsertingValdiChildren
{
    return self.contentView;
}

#pragma mark - UIView+Valdi

- (BOOL)requiresShapeLayerForBorderRadius
{
    // On iOS 26 corners are applied natively via `cornerConfiguration` (see the
    // borderRadius binding below), which rounds the whole Liquid Glass material
    // (backdrop, tint, and rim) correctly. A shape-layer mask does not clip the
    // glass tint, so only use it for the pre-26 UIBlurEffect fallback.
    return !_SCValdiIsGlassEffectAvailable();
}

- (BOOL)willEnqueueIntoValdiPool
{
    return YES;
}

#pragma mark - Internal methods

- (void)_applyEffectWithAnimator:(nullable id<SCValdiAnimatorProtocol>)animator
{
    UIVisualEffect *effect = nil;

#if defined(__IPHONE_26_0)
    if (_SCValdiIsGlassEffectAvailable()) {
        if (@available(iOS 26.0, *)) {
            UIGlassEffectStyle style = _isClearStyle ? UIGlassEffectStyleClear : UIGlassEffectStyleRegular;
            UIGlassEffect *glassEffect = [UIGlassEffect effectWithStyle:style];
            glassEffect.interactive = _interactive;
            glassEffect.tintColor = _tintColor;
            effect = glassEffect;
        }
    }
#endif

    if (effect == nil) {
        // Liquid Glass unavailable: fall back to a blur material so the surface
        // still reads as a translucent panel. tint/interactive have no analogue
        // here and are intentionally dropped.
        effect = [UIBlurEffect effectWithStyle:kSCValdiGlassFallbackBlurStyle];
    }

    self.effect = effect;
    if (animator) {
        [animator addTransitionOnLayer:self.layer];
    }
}

#if defined(__IPHONE_26_0)
- (void)_applyCornerConfiguration:(SCValdiCornerValues)corners API_AVAILABLE(ios(26.0))
{
    BOOL uniform = corners.topLeft.value == corners.topRight.value &&
                   corners.topLeft.value == corners.bottomLeft.value &&
                   corners.topLeft.value == corners.bottomRight.value &&
                   corners.topLeft.isPercent == corners.topRight.isPercent &&
                   corners.topLeft.isPercent == corners.bottomLeft.isPercent &&
                   corners.topLeft.isPercent == corners.bottomRight.isPercent;

    if (uniform && corners.topLeft.isPercent) {
        // A percentage radius (e.g. borderRadius: '50%') means a pill: let UIKit
        // compute the capsule so the glass rim follows the fully-rounded ends.
        self.cornerConfiguration = [UICornerConfiguration capsuleConfiguration];
    } else if (uniform) {
        self.cornerConfiguration =
            [UICornerConfiguration configurationWithRadius:[UICornerRadius fixedRadius:corners.topLeft.value]];
    } else {
        // Per-corner point radii. Percentage per-corner values are uncommon and
        // fall back to their raw point value here.
        self.cornerConfiguration = [UICornerConfiguration
            configurationWithTopLeftRadius:[UICornerRadius fixedRadius:corners.topLeft.value]
                            topRightRadius:[UICornerRadius fixedRadius:corners.topRight.value]
                          bottomLeftRadius:[UICornerRadius fixedRadius:corners.bottomLeft.value]
                         bottomRightRadius:[UICornerRadius fixedRadius:corners.bottomRight.value]];
    }
}
#endif

#pragma mark - Static methods

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    [attributesBinder bindAttribute:@"glassStyle"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiGlassView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            view->_isClearStyle = [attributeValue isEqualToString:@"clear"];
            [view _applyEffectWithAnimator:animator];
            return YES;
        }
        resetBlock:^(SCValdiGlassView *view, id<SCValdiAnimatorProtocol> animator) {
            view->_isClearStyle = NO;
            [view _applyEffectWithAnimator:animator];
        }];

    [attributesBinder bindAttribute:@"glassTintColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiGlassView *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            view->_tintColor = attributeValue;
            [view _applyEffectWithAnimator:animator];
            return YES;
        }
        resetBlock:^(SCValdiGlassView *view, id<SCValdiAnimatorProtocol> animator) {
            view->_tintColor = nil;
            [view _applyEffectWithAnimator:animator];
        }];

    [attributesBinder bindAttribute:@"interactive"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiGlassView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            view->_interactive = attributeValue;
            [view _applyEffectWithAnimator:animator];
            return YES;
        }
        resetBlock:^(SCValdiGlassView *view, id<SCValdiAnimatorProtocol> animator) {
            view->_interactive = NO;
            [view _applyEffectWithAnimator:animator];
        }];

#if defined(__IPHONE_26_0)
    if (_SCValdiIsGlassEffectAvailable()) {
        // Override the base <view> borderRadius binding (which installs a shape-layer
        // mask) with the native iOS 26 cornerConfiguration, so the glass material,
        // its tint, and its rim all round together. This subclass binding wins because
        // AttributesManager merges a class's own handlers after inherited ones.
        // When glass is unavailable we leave this unbound so the inherited shape-layer
        // mask still rounds the UIBlurEffect fallback.
        [attributesBinder bindAttribute:@"borderRadius"
            invalidateLayoutOnChange:NO
            withBordersBlock:^BOOL(SCValdiGlassView *view, SCValdiCornerValues attributeValue,
                                   id<SCValdiAnimatorProtocol> animator) {
                if (@available(iOS 26.0, *)) {
                    [view _applyCornerConfiguration:attributeValue];
                }
                return YES;
            }
            resetBlock:^(SCValdiGlassView *view, id<SCValdiAnimatorProtocol> animator) {
                if (@available(iOS 26.0, *)) {
                    view.cornerConfiguration =
                        [UICornerConfiguration configurationWithRadius:[UICornerRadius fixedRadius:0]];
                }
            }];
    }
#endif
}

@end
