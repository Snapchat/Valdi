//
//  SCValdiAttributesBinder.h
//  iOS
//
//  Created by Simon Corsin on 4/18/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi_core/SCNValdiCoreAssetOutputType.h"
#import "valdi_core/SCNValdiCoreCompositeAttributePart.h"
#import "valdi_core/SCValdiAction.h"
#import "valdi_core/SCValdiAnimatorBase.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiViewLayoutAttributes.h"
#include <CoreFoundation/CoreFoundation.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

typedef struct SCValdiDoubleValue {
    CGFloat value;
    BOOL isPercent;
} SCValdiDoubleValue;

typedef struct SCValdiCornerValues {
    SCValdiDoubleValue topLeft;
    SCValdiDoubleValue topRight;
    SCValdiDoubleValue bottomRight;
    SCValdiDoubleValue bottomLeft;
} SCValdiCornerValues;

#ifdef __cplusplus
extern "C" {
#endif
extern CGFloat SCValdiDoubleValueToRatio(SCValdiDoubleValue doubleValue);
extern SCValdiDoubleValue SCValdiDoubleMakeValue(CGFloat value);
extern SCValdiDoubleValue SCValdiDoubleMakePercent(CGFloat percent);

#ifdef __cplusplus
}
#endif

@protocol SCValdiViewLayoutAttributes;
@protocol SCValdiFontManagerProtocol;

typedef BOOL (^SCValdiAttributeBindMethodUntyped)(__kindof UIView* view,
                                                  id attributeValue,
                                                  id<SCValdiAnimatorProtocol> animator);
typedef void (^SCValdiAttributeBindMethodAction)(__kindof UIView* view, id<SCValdiAction> attributeValue);
typedef void (^SCValdiAttributeBindMethodFunction)(__kindof UIView* view, id<SCValdiFunction> attributeValue);
typedef void (^SCValdiAttributeBindMethodFunctionAndPredicate)(__kindof UIView* view,
                                                               id<SCValdiFunction> attributeValue,
                                                               id<SCValdiFunction> predicate,
                                                               id additionalValue);

typedef BOOL (^SCValdiAttributeBindMethodArray)(__kindof UIView* view,
                                                NSArray* attributeValue,
                                                id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodString)(__kindof UIView* view,
                                                 NSString* attributeValue,
                                                 id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodDouble)(__kindof UIView* view,
                                                 CGFloat attributeValue,
                                                 id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodPercent)(__kindof UIView* view,
                                                  SCValdiDoubleValue attributeValue,
                                                  id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodInt)(__kindof UIView* view,
                                              NSInteger attributeValue,
                                              id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodColor)(__kindof UIView* view,
                                                UIColor* attributeValue,
                                                id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodBool)(__kindof UIView* view,
                                               BOOL attributeValue,
                                               id<SCValdiAnimatorProtocol> animator);
typedef BOOL (^SCValdiAttributeBindMethodBorders)(__kindof UIView* view,
                                                  SCValdiCornerValues attributeValue,
                                                  id<SCValdiAnimatorProtocol> animator);
typedef void (^SCValdiAttributeBindMethodReset)(__kindof UIView* view, id<SCValdiAnimatorProtocol> animator);
typedef void (^SCValdiAttributeBindMethodResetNonAnimatable)(__kindof UIView* view);
typedef id (^SCValdiAttributePreprocessor)(id value);

typedef id (*SCValdiAttributeDeserializer)(id jsInstance);

/**
 * Measure delegate parameters:
 * - attributes: layout attributes for the node being measured.
 * - maxSize: size constraints provided by layout.
 * - traitCollection: trait collection used for measurement.
 */
typedef CGSize (^SCValdiMeasureDelegate)(id<SCValdiViewLayoutAttributes> attributes,
                                         CGSize maxSize,
                                         UITraitCollection* traitCollection);

typedef UIView* (^SCValdiPlaceholderViewMeasureDelegate)();

typedef NSDictionary<NSString*, SCValdiAttributeBindMethodUntyped> SCAttributeApplierByName;

/**
 Allows to declare binding between an attribute and a view.
 A view class will bind all attributes it knows how to handle
 so that when this attribute is used in either XML or JavaScript,
 the Valdi Runtime will know how to apply it to a native view.
 */
@protocol SCValdiAttributesBinderProtocol <NSObject>

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withUntypedBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
             withStringBlock:(SCValdiAttributeBindMethodString)stringBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withStringBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
             withDoubleBlock:(SCValdiAttributeBindMethodDouble)doubleBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withDouble:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withPercentBlock:(SCValdiAttributeBindMethodPercent)percentBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock;

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                withIntBlock:(SCValdiAttributeBindMethodInt)intBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withIntBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
              withColorBlock:(SCValdiAttributeBindMethodColor)colorBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock;

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
               withBoolBlock:(SCValdiAttributeBindMethodBool)boolBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withBoolBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
      withActionBlock:(SCValdiAttributeBindMethodAction)actionBlock
           resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:withActionBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    withFunctionBlock:(SCValdiAttributeBindMethodFunction)functionBlock
           resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:withFunctionBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
              additionalAttribute:(SCNValdiCoreCompositeAttributePart*)additionalAttribute
    withFunctionAndPredicateBlock:(SCValdiAttributeBindMethodFunctionAndPredicate)actionBlock
                       resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock;

- (void)bindAttribute:(NSString*)attributeName
    withFunctionAndPredicateBlock:(SCValdiAttributeBindMethodFunctionAndPredicate)actionBlock
                       resetBlock:(SCValdiAttributeBindMethodResetNonAnimatable)resetBlock;

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
            withBordersBlock:(SCValdiAttributeBindMethodBorders)bordersBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock;

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
              withArrayBlock:(SCValdiAttributeBindMethodArray)arrayBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindAttribute(_:invalidateLayoutOnChange:withArrayBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
               withTextBlock:(SCValdiAttributeBindMethodUntyped)textBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock;

- (void)bindCompositeAttribute:(NSString*)attributeName
                         parts:(NSArray<SCNValdiCoreCompositeAttributePart*>*)parts
              withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                    resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
    NS_SWIFT_NAME(bindCompositeAttribute(_:parts:withUntypedBlock:resetBlock:));

- (void)bindAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                deserializer:(SCValdiAttributeDeserializer)deserializer
            withUntypedBlock:(SCValdiAttributeBindMethodUntyped)untypedBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock;

- (void)registerPreprocessorForAttribute:(NSString*)attributeName withBlock:(SCValdiAttributePreprocessor)block;
- (void)registerPreprocessorForAttribute:(NSString*)attributeName
                             enableCache:(BOOL)enableCache
                               withBlock:(SCValdiAttributePreprocessor)block;

- (void)bindScrollAttributes;

- (void)bindAssetAttributesForOutputType:(SCNValdiCoreAssetOutputType)outputType;

/**
 * Registers a MeasureDelegate that will be used to measure elements.
 * Without a MeasureDelegate, the element will always have an intrinsic size of 0, 0.
 * Its size would need to be configured through flexbox attributes. View implementations
 * that have an intrinsic size based on their content, like a label for instance, should
 * register a MeasureDelegate. The MeasureDelegate can be called in arbitrary threads.
 */
- (void)setMeasureDelegate:(SCValdiMeasureDelegate)measureDelegate NS_SWIFT_NAME(setMeasureDelegate(_:));

/**
 * Registers a MeasureDelegate that uses a placeholder view instance to measure elements.
 * The given block should return a view that will be configured with attributes,
 * sizeThatFits: will then be called on it to resolve the element size, and the view will then
 * be restored to its original view. The returned view might be re-used many times to measure
 * elements of this view class. setMeasureDelegate: should be preferred over
 * setPlaceholderViewMeasureDelegate: since it can typically be implemented in a more
 * efficient way. Measuring elements configured with a placeholderViewMeasureDelegate will
 * always be measured in the main thread.
 */
- (void)setPlaceholderViewMeasureDelegate:(SCValdiPlaceholderViewMeasureDelegate)placeholderViewMeasureDelegate;

/**
 * Returns the fontManager instance. Can be used during attributes binding if an implemenetation
 * of an attribute requires the font manager.
 */
- (id<SCValdiFontManagerProtocol>)fontManager;

@end
