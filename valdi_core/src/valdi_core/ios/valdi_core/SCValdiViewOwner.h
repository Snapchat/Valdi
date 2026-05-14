//
//  SCValdiViewOwner.h
//  Valdi
//
//  Created by Simon Corsin on 12/14/17.
//  Copyright © 2017 Snap Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiViewOwner <NSObject>

@optional

/**
 Called when a view was recreated from Valdi.
 Because of the hot reloading support, this method might be called multiple time.
 */
- (void)didAwakeViewFromValdi:(UIView*)view;

/**
 Called by the Valdi runtime when a view is going to be created.
 You can override this function to provide a different implementation
 of the view or instantiate the view class yourself if you know
 that this class needs additional parameters that you are able to provide.
 If you return nil or don't implement this method, the runtime will
 use initWithFrame:
 */
- (UIView* _Nullable)valdiWillCreateViewForClass:(Class)aClass nodeId:(NSString*)nodeId;

- (void)didRenderValdiView:(UIView*)view;

@end

NS_ASSUME_NONNULL_END
