//
//  SCValdiViewNode.m
//  ValdiIOS
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"
#import "valdi/ios/CPPBindings/UIViewHolder.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#import "valdi/runtime/Attributes/AttributeOwner.hpp"
#import "valdi/runtime/Attributes/AttributesApplier.hpp"
#import "valdi/runtime/Attributes/AttributesManager.hpp"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiInternedString+CPP.h"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/SCValdiMarshallableObject.h"

static Valdi::AttributeId SCValdiGetAttributeId(Valdi::ViewNode *viewNode,
                                               __unsafe_unretained NSString *attributeName) {
    auto cppAttributeName = ValdiIOS::StringFromNSString(attributeName);
    return viewNode->getAttributeIds().getIdForName(cppAttributeName);
}

static UIAccessibilityTraits SCValdiAccessibilityCategoryToAccessibilityTrait(Valdi::AccessibilityCategory accessibilityCategory) {
    switch (accessibilityCategory) {
        case Valdi::AccessibilityCategoryAuto:
            return UIAccessibilityTraitNone;
        case Valdi::AccessibilityCategoryView:
            return UIAccessibilityTraitNone;
        case Valdi::AccessibilityCategoryImage:
            return UIAccessibilityTraitImage;
        case Valdi::AccessibilityCategoryButton:
            return UIAccessibilityTraitButton;
        case Valdi::AccessibilityCategoryImageButton:
            return UIAccessibilityTraitImage | UIAccessibilityTraitButton;
        case Valdi::AccessibilityCategoryText:
            return UIAccessibilityTraitStaticText;
        case Valdi::AccessibilityCategoryInput:
            return UIAccessibilityTraitNone;
        case Valdi::AccessibilityCategoryLink:
            return UIAccessibilityTraitLink;
        case Valdi::AccessibilityCategoryHeader:
            return UIAccessibilityTraitHeader;
        case Valdi::AccessibilityCategoryCheckBox:
            return UIAccessibilityTraitButton;
        case Valdi::AccessibilityCategoryRadio:
            return UIAccessibilityTraitButton;
        case Valdi::AccessibilityCategoryKeyboardKey:
            return UIAccessibilityTraitKeyboardKey;
    }
}

@implementation SCValdiViewNode {
    Valdi::SharedViewNode _viewNode;
    NSArray<SCValdiViewNode *> *_accessibilityChildren;
    NSMutableDictionary<NSString *, id> *_retainedObjects;
    NSMutableDictionary<NSString *, SCValdiContextDidFinishLayoutBlock> *_didFinishLayoutBlocks;
}

- (instancetype)initWithViewNode:(Valdi::ViewNode *)cppViewNode
{
    self = [super init];
    if (self) {
        _viewNode = cppViewNode;
    }
    return self;
}

- (UIView *)view
{
    return ValdiIOS::UIViewHolder::uiViewFromRef(_viewNode->getViewAndDisablePooling());
}

- (CGRect)relativeFrame
{
    auto frame = _viewNode->getDirectionAgnosticFrame();
    return CGRectMake(CGFloatNormalize(frame.x),
                      CGFloatNormalize(frame.y),
                      CGFloatNormalize(frame.width),
                      CGFloatNormalize(frame.height));
}

- (void)_getViewNode:(void(^)(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *))block
{
    auto* viewNodeTree = _viewNode->getViewNodeTree();

    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->withLock([&]() {
        block(viewNodeTree->getCurrentViewTransactionScope(), _viewNode.get());
    });
}

- (void)setValue:(id)value forValdiAttribute:(NSString *)attributeName
{
    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        auto cppValue = ValdiIOS::ValueFromNSObject(value);
        auto attributeId = SCValdiGetAttributeId(viewNode, attributeName);
        viewNode->getAttributesApplier().setAttribute(viewTransactionScope, attributeId, Valdi::AttributeOwner::getNativeOverridenAttributeOwner(), cppValue, nullptr);
        viewNode->getAttributesApplier().flush(viewTransactionScope);
    }];
}

- (void)removeValueForValdiAttribute:(NSString *)attributeName
{
    [self setValue:nil forValdiAttribute:attributeName];
}

- (id)valueForValdiAttribute:(NSString*)attributeName
{
    __block id outValue = nil;

    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        auto attributeId = SCValdiGetAttributeId(viewNode, attributeName);

        auto value = viewNode->getAttributesApplier().getResolvedAttributeValue(attributeId);
        if (!value.isNullOrUndefined()) {
            outValue = ValdiIOS::NSObjectFromValue(value);
        }
    }];

    return outValue;
}

- (id)preprocessedValueForValdiAttribute:(NSString*)attributeName
{
    __block id outValue = nil;

    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        auto attributeId = SCValdiGetAttributeId(viewNode, attributeName);

        auto value = viewNode->getAttributesApplier().getResolvedAttributeValue(attributeId);
        if (!value.isNullOrUndefined()) {
            outValue = ValdiIOS::NSObjectFromValue(value);
        }
    }];

    return outValue;
}

- (void)setRetainedObject:(id)object forKey:(NSString *)key
{
    if (object) {
        if (!_retainedObjects) {
            _retainedObjects = [NSMutableDictionary dictionary];
        }
        [_retainedObjects setObject:object forKey:key];
    } else {
        [_retainedObjects removeObjectForKey:key];
    }
}

- (void)setStoredObject:(id)object forKey:(SCValdiInternedStringRef)key
{
    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        (void)viewTransactionScope;
        viewNode->setStoredObject(*SCValdiInternedStringUnwrap(key), ValdiIOS::ValueFromNSObject(object));
    }];
}

- (id)storedObjectForKey:(SCValdiInternedStringRef)key
{
    __block id outValue = nil;

    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        (void)viewTransactionScope;
        auto value = viewNode->getStoredObject(*SCValdiInternedStringUnwrap(key));
        if (!value.isNullOrUndefined()) {
            outValue = ValdiIOS::NSObjectFromValue(value);
        }
    }];

    return outValue;
}

- (void)setDidFinishLayoutBlock:(SCValdiContextDidFinishLayoutBlock)block forKey:(NSString *)key
{
    if (_didFinishLayoutBlocks && CFGetRetainCount((__bridge CFTypeRef)_didFinishLayoutBlocks) != 1) {
        // dictionary is being iterated on, copy before mutating
        _didFinishLayoutBlocks = [_didFinishLayoutBlocks mutableCopy];
    }

    if (block) {
        if (!_didFinishLayoutBlocks) {
            _didFinishLayoutBlocks = [NSMutableDictionary new];
        }
        _didFinishLayoutBlocks[key] = block;
    } else {
        [_didFinishLayoutBlocks removeObjectForKey:key];
    }
}

- (BOOL)hasDidFinishLayoutBlockForKey:(NSString *)key
{
    return [_didFinishLayoutBlocks objectForKey:key] != nil;
}

- (void)didApplyLayoutWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    if (!_didFinishLayoutBlocks) {
        return;
    }

    UIView *view = self.view;
    if (view) {
        NSDictionary *didFinishLayoutBlocks = _didFinishLayoutBlocks;
        for (NSString *key in didFinishLayoutBlocks) {
            SCValdiContextDidFinishLayoutBlock block = _didFinishLayoutBlocks[key];
            if (block) {
                block(view, animator);
            }
        }
    }
}

- (void)markLayoutDirty
{
    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        viewNode->invalidateMeasuredSize();
    }];
}

static void applyContentOffsetOverride(const std::optional<Valdi::Point> &contentOffsetOverride, CGPoint *updatedContentOffset) {
    if (contentOffsetOverride) {
        *updatedContentOffset = CGPointMake(CGFloatNormalize(contentOffsetOverride.value().x), CGFloatNormalize(contentOffsetOverride.value().y));
    }
}

- (void)notifyOnScrollWithContentOffset:(CGPoint)contentOffset
                   updatedContentOffset:(inout CGPoint *)updatedContentOffset
                               velocity:(CGPoint)velocity
{
    auto contentOffsetCpp = Valdi::Point(contentOffset.x, contentOffset.y);
    auto contentOffsetOverride = _viewNode->onScroll(contentOffsetCpp, contentOffsetCpp, Valdi::Point(velocity.x, velocity.y));
    applyContentOffsetOverride(contentOffsetOverride, updatedContentOffset);
}

- (void)notifyOnScrollEndWithContentOffset:(CGPoint)contentOffset
{
    auto contentOffsetCpp = Valdi::Point(contentOffset.x, contentOffset.y);
    _viewNode->onScrollEnd(contentOffsetCpp, contentOffsetCpp);
}

- (void)notifyOnDragStartWithContentOffset:(CGPoint)contentOffset velocity:(CGPoint)velocity
{
    auto contentOffsetCpp = Valdi::Point(contentOffset.x, contentOffset.y);
    _viewNode->onDragStart(contentOffsetCpp, contentOffsetCpp, Valdi::Point(velocity.x, velocity.y));
}

- (void)notifyOnDragEndingWithContentOffset:(CGPoint)contentOffset velocity:(CGPoint)velocity updatedContentOffset:(inout CGPoint *)updatedContentOffset
{
    auto contentOffsetCpp = Valdi::Point(contentOffset.x, contentOffset.y);
    auto contentOffsetOverride = _viewNode->onDragEnding(contentOffsetCpp, contentOffsetCpp, Valdi::Point(velocity.x, velocity.y));
    applyContentOffsetOverride(contentOffsetOverride, updatedContentOffset);
}

- (BOOL)canScrollAtPoint:(CGPoint)point direction:(SCValdiScrollDirection)direction
{
    return _viewNode->canScroll(Valdi::Point(point.x, point.y), (Valdi::ScrollDirection)direction);
}

- (BOOL)isRightToLeft
{
    return _viewNode->isRightToLeft();
}

- (BOOL)isLayoutDirectionHorizontal
{
    return _viewNode->isHorizontal();
}

- (CGPoint)relativeDirectionAgnosticPointFromPoint:(CGPoint)point
{
    auto directionAgnosticPoint = _viewNode->convertPointToRelativeDirectionAgnostic(Valdi::Point(point.x, point.y));
    return CGPointMake(CGFloatNormalize(directionAgnosticPoint.x), CGFloatNormalize(directionAgnosticPoint.y));
}

- (CGPoint)absoluteDirectionAgnosticPointFromPoint:(CGPoint)point
{
    auto directionAgnosticPoint = _viewNode->convertPointToAbsoluteDirectionAgnostic(Valdi::Point(point.x, point.y));
    return CGPointMake(CGFloatNormalize(directionAgnosticPoint.x), CGFloatNormalize(directionAgnosticPoint.y));
}

- (CGFloat)resolveDeltaX:(CGFloat)deltaX directionAgnostic:(BOOL)directionAgnostic
{
    // directionAgnostic is unused since we just reverse the delta in RTL currently
    if (_viewNode->isRightToLeft()) {
        return deltaX * -1;
    } else {
        return deltaX;
    }
}

- (NSString *)layoutDebugDescription
{
    return ValdiIOS::NSStringFromSTDStringView(_viewNode->getLayoutDebugDescription());
}

- (NSString *)toXML
{
    return ValdiIOS::NSStringFromSTDStringView(_viewNode->toXML());
}

- (Valdi::ViewNode *)cppViewNode
{
    return _viewNode.get();
}

- (BOOL)isAccessibilityElement
{
    return _viewNode->isAccessibilityLeaf();
}

- (NSString *)accessibilityLabel
{
    auto accessibilityLabel = _viewNode->getAccessibilityLabel();
    if (accessibilityLabel.isEmpty()) {
        return nil;
    }
    return ValdiIOS::NSStringFromString(accessibilityLabel);
}

- (NSString *)accessibilityHint
{
    auto accessibilityHint = _viewNode->getAccessibilityHint();
    if (accessibilityHint.isEmpty()) {
        return nil;
    }
    return ValdiIOS::NSStringFromString(accessibilityHint);
}

- (NSString *)accessibilityValue
{
    auto accessibilityValue = _viewNode->getAccessibilityValue();
    if (accessibilityValue.isEmpty()) {
        return [[self view] accessibilityValue]; // If undefined, we fallback to the iOS value, in case the element is a fancy control
    }
    return ValdiIOS::NSStringFromString(accessibilityValue);
}

- (NSString *)accessibilityIdentifier
{
    auto accessibilityIdentifier = _viewNode->getAccessibilityId();
    if (accessibilityIdentifier.isEmpty()) {
        return [[self view] accessibilityIdentifier];
    }
    return ValdiIOS::NSStringFromString(accessibilityIdentifier);
}

- (UIAccessibilityTraits)accessibilityTraits
{
    UIAccessibilityTraits accessibilityTraits = SCValdiAccessibilityCategoryToAccessibilityTrait(_viewNode->getAccessibilityCategory());

    if (_viewNode->getAccessibilityStateDisabled()) {
        accessibilityTraits |= UIAccessibilityTraitNotEnabled;
    }
    if (_viewNode->getAccessibilityStateSelected()) {
        accessibilityTraits |= UIAccessibilityTraitSelected;
    }
    if (_viewNode->getAccessibilityStateLiveRegion()) {
        accessibilityTraits |= UIAccessibilityTraitUpdatesFrequently;
    }

    // Note:
    // We combine the computed accessibilityTraits with the view's ones if possible
    // This is because iOS cheats and have traits that are NOT available to the public API (such as TextField)
    return accessibilityTraits | [[self view] accessibilityTraits];
}

- (void)walkCustomViewForAccessibility:(UIView*) view elements:(NSMutableArray *)elements {
    // If the view has specified it's own custom accessibility children,
    // we don't need to walk further.
    auto specifiedChildren = view.accessibilityElements;
    if (specifiedChildren != nil) {
        [elements addObjectsFromArray:specifiedChildren];
        return;
    }

    // We must include all elements here for testing purposes
    // There's a convenient "isAccessibilityElement" function but that indicates
    // that none of the child views are important.
    [elements addObject:view];

    for (UIView *subview in view.subviews) {
        [self walkCustomViewForAccessibility:subview elements:elements];
    }
}

- (NSArray *)accessibilityElements
{
    if (_viewNode->isUserSpecifiedView()) {
        // Custom views are special we have to walk them because unlike Android,
        // accessibilityElements only returns custom elements and not standard views.
        UIView *uiView = ValdiIOS::UIViewHolder::uiViewFromRef(_viewNode->getView());
        NSMutableArray *elements = [NSMutableArray new];
        if (uiView != nil) {
            [self walkCustomViewForAccessibility:uiView elements:elements];
        }
        return elements;
    }

    if (!_viewNode->accessibilityTreeNeedsUpdate() && _accessibilityChildren) {
        return _accessibilityChildren;
    }

    // Navigate the tree of accessibility nodes
    auto cppAccessibilityChildren = _viewNode->getAccessibilityChildrenRecursive();
    // Count the accessibility nodes found during traversal
    size_t oldCount = [_accessibilityChildren count];
    size_t newCount = cppAccessibilityChildren.size();
    // Check if the current child elements match the viewnodes we expect
    BOOL isDirty = false;
    if (_accessibilityChildren == nil) {
        isDirty = true;
    } else {
        if (oldCount != newCount) {
            isDirty = true;
        } else {
            for (size_t i = 0; i < newCount; i++) {
                if (_accessibilityChildren[i].cppViewNode != cppAccessibilityChildren[i]) {
                    isDirty = true;

                    break;
                }
            }
        }
    }
    // If they don't match, lets create a brand new array with the wrapped view nodes
    if (isDirty) {
        NSMutableArray<SCValdiViewNode*> *accessibilityChildren = [NSMutableArray arrayWithCapacity:newCount];
        for (size_t i = 0; i < newCount; i++) {
            [accessibilityChildren addObject: [[SCValdiViewNode alloc] initWithViewNode: cppAccessibilityChildren[i]]];
        }
        _accessibilityChildren = [accessibilityChildren copy];
    }
    // Done
    return _accessibilityChildren;
}

- (UIAccessibilityContainerType)accessibilityContainerType
{
    if (_viewNode->getAccessibilityNavigation() == Valdi::AccessibilityNavigationGroup) {
        if (@available(iOS 13.0, *)) {
            return UIAccessibilityContainerTypeSemanticGroup;
        } else {
            return UIAccessibilityContainerTypeList;
        }
    }
    return UIAccessibilityContainerTypeNone;
}

- (UIAccessibilityNavigationStyle)accessibilityNavigationStyle
{
    switch(_viewNode->getAccessibilityNavigation()) {
        case Valdi::AccessibilityNavigationGroup:
            return UIAccessibilityNavigationStyleSeparate;
        case Valdi::AccessibilityNavigationLeaf:
            return UIAccessibilityNavigationStyleCombined;
        default:
            return UIAccessibilityNavigationStyleAutomatic;
    }
}

- (CGRect)accessibilityFrame
{
    UIView *rootView = ValdiIOS::UIViewHolder::uiViewFromRef(_viewNode->getRoot()->getView());
    Valdi::Frame frameInRoot = _viewNode->computeVisualFrameInRoot();
    CGRect rectInRoot = CGRectMake(frameInRoot.x, frameInRoot.y, frameInRoot.width, frameInRoot.height);
    return UIAccessibilityConvertFrameToScreenCoordinates(rectInRoot, rootView);
}

- (BOOL)shouldGroupAccessibilityChildren
{
    return YES;
}

- (void)accessibilityElementDidBecomeFocused
{
    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        viewNode->ensureFrameIsVisibleWithinParentScrolls(viewTransactionScope, true);
    }];
}

- (void)accessibilityElementDidLoseFocus
{
}

- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
    Valdi::ScrollDirection scrollDirection;
    switch (direction) {
        case UIAccessibilityScrollDirectionUp:
            scrollDirection = Valdi::ScrollDirectionBottomToTop;
            break;
        case UIAccessibilityScrollDirectionDown:
            scrollDirection = Valdi::ScrollDirectionTopToBottom;
            break;
        case UIAccessibilityScrollDirectionLeft:
            scrollDirection = Valdi::ScrollDirectionRightToLeft;
            break;
        case UIAccessibilityScrollDirectionRight:
            scrollDirection = Valdi::ScrollDirectionLeftToRight;
            break;
        default:
            return NO;
    }
    __block BOOL scrolled = NO;

    [self _getViewNode:^(Valdi::ViewTransactionScope &viewTransactionScope, Valdi::ViewNode *viewNode) {
        float scrollPageCurrent = 0;
        float scrollPageMaximum = 0;
        if (viewNode->scrollByOnePage(viewTransactionScope, scrollDirection, true, &scrollPageCurrent, &scrollPageMaximum)) {
            int scrollPagePercent = roundf(scrollPageCurrent * 100 / scrollPageMaximum);
            NSString *notification = [NSString stringWithFormat:@"%i%%", scrollPagePercent];
            UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification, notification);
            scrolled = YES;
        }
    }];

    return scrolled;
}

- (NSArray<id<SCValdiViewNodeProtocol>> *)children
{
    NSMutableArray<id<SCValdiViewNodeProtocol>> *out = [NSMutableArray arrayWithCapacity:_viewNode->getChildCount()];
    for (auto *childViewNode: *_viewNode) {
        [out addObject:[[SCValdiViewNode alloc] initWithViewNode:childViewNode]];
    }

    return out;
}

@end

@interface SCValdiViewNodeProtocol: NSObject<SCValdiMarshallableObject>
@end

@implementation SCValdiViewNodeProtocol

+ (SCValdiMarshallableObjectDescriptor)valdiMarshallableObjectDescriptor
{
    return SCValdiMarshallableObjectDescriptorMake(nil, nil, nil, SCValdiMarshallableObjectTypeUntyped);
}

@end
