//
//  SCCValdiRef.m
//  valdi-ios
//
//  Created by Simon Corsin on 12/21/18.
//

#import "valdi_core/SCValdiRefUtils.h"
#import "valdi_core/SCValdiRef.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import <UIKit/UIKit.h>

id<SCValdiViewNodeProtocol> SCValdiRefGetViewNode(SCValdiRef *ref) {
    if (!ref || ![ref isKindOfClass:[SCValdiRef class]]) {
        return nil;
    }
    id instance = ref.instance;
    if ([instance conformsToProtocol:@protocol(SCValdiViewNodeProtocol)] ) {
        return instance;
    }
    if ([instance isKindOfClass:[UIView class]]) {
        return ((UIView *)instance).valdiViewNode;
    }

    return nil;
}

UIView *SCValdiRefGetView(SCValdiRef *ref) {
    if (!ref || ![ref isKindOfClass:[SCValdiRef class]]) {
        return nil;
    }
    id instance = ref.instance;
    if ([instance isKindOfClass:[UIView class]]) {
        return instance;
    }
    if ([instance conformsToProtocol:@protocol(SCValdiViewNodeProtocol)] ) {
        return ((id<SCValdiViewNodeProtocol>)instance).view;
    }

    return nil;
}
