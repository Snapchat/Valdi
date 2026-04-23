#import "SCValdiUnsetAndReleaseInMainThread.h"

static void SCValdiReleaseObject(void *context)
{
    CFRelease(context);
}

void SCValdiUnsetAndReleaseInMainThread(__strong id _Nullable * _Nullable object)
{
    if (!object || !*object) {
        return;
    }

    CFTypeRef retainedObject = CFBridgingRetain(*object);
    *object = nil;

    if (NSThread.isMainThread) {
        CFRelease(retainedObject);
        return;
    }

    dispatch_async_f(dispatch_get_main_queue(), (void *)retainedObject, SCValdiReleaseObject);
}
