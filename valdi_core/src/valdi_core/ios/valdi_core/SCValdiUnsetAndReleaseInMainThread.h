#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Clears the strong object reference at `object` immediately, then releases the
 * previous value on the main thread.
 *
 * Use this for UIKit/WebKit objects that may be owned by objects deallocated
 * off-main-thread but must run their final release on the main thread.
 */
void SCValdiUnsetAndReleaseInMainThread(__strong id _Nullable* _Nullable object);

NS_ASSUME_NONNULL_END
