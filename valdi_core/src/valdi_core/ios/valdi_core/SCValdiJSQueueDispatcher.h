//
//  SCValdiJSQueueDispatcher.h
//  Valdi
//
//  Created by Simon Corsin on 9/6/18.
//

#import <Foundation/Foundation.h>

@protocol SCValdiJSQueueDispatcher <NSObject>

- (void)dispatchOnJSQueueWithBlock:(dispatch_block_t)block sync:(BOOL)sync;

/**
 * @param attribution A stable, nonempty identifier for the scheduling callsite. It must remain
 * low-cardinality and must not contain user or content identifiers.
 */
- (void)dispatchOnJSQueueWithBlock:(dispatch_block_t)block
                               sync:(BOOL)sync
                        attribution:(NSString *)attribution;

@end
