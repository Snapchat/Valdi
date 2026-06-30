//
//  UIView+Valdi.h
//  Valdi
//
//  Created by Simon Corsin on 12/12/17.
//  Copyright © 2017 Snap Inc. All rights reserved.
//

#import "valdi/ios/SCValdiContext.h"
#import "valdi/ios/SCValdiViewNode.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"
#import "valdi_core/SCValdiViewOwner.h"
#import "valdi_core/UIView+ValdiBase.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import <UIKit/UIKit.h>

@interface UIView (Valdi)

/**
 Bind all supported attributes into the given SCValdiAttributesBinder.
 This method should be used to expose your view attributes into the XML.
 Override this method to expose new attributes.
 */
+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder;

+ (BOOL)valdi_managesChildFrames;

+ (BOOL)valdi_isPerformingViewHierarchySnapshot;

- (void)valdi_performViewHierarchySnapshotWithBlock:(void (^)(void))snapshotBlock;
- (void)valdi_willPerformViewHierarchySnapshot;

- (BOOL)valdi_setBoxShadow:(NSArray<id>*)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator;

@end
