#import <UIKit/UIKit.h>

#ifndef __IPHONE_14_0
#define __IPHONE_14_0 140000
#endif

void SCValdiSetDatePickerStyle(UIDatePicker *datePicker, NSInteger style) {
    // The `client` artifact currently still compiles using Xcode 11.4.1, but
    // it might already be linked into an app built with Xcode 12, which will trigger
    // the new behavior. So, we have to set the preferredDatePickerStyle value even
    // if we don't have access to it in the UIKit headers.
    //
    // That's why we're doing a build-time API availability check in addition to the
    // runtime availability check.
    #if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
        if (@available(iOS 13.4, *)) {
            datePicker.preferredDatePickerStyle = (UIDatePickerStyle)style;
        }
    #else
        SEL setPreferredDatePickerStyleSelector = NSSelectorFromString(@"setPreferredDatePickerStyle:");
        if (setPreferredDatePickerStyleSelector && [datePicker respondsToSelector:setPreferredDatePickerStyleSelector]) {
            NSMethodSignature* signature = [[datePicker class] instanceMethodSignatureForSelector:setPreferredDatePickerStyleSelector];
            NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
            [invocation setTarget:datePicker];
            [invocation setSelector:setPreferredDatePickerStyleSelector];
            [invocation setArgument:&style atIndex:2];
            [invocation invoke];
        }
    #endif
}

void SCValdiFixIOS14DatePicker(UIDatePicker *datePicker) {
    SCValdiSetDatePickerStyle(datePicker, 1 /* wheels */);
}
