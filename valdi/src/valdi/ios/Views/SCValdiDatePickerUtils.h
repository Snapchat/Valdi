#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/// UIDatePicker defaults to a new "compact" date picker style starting from iOS 14,
/// this function configures a UIDatePicker instance to use the old style.
void SCValdiFixIOS14DatePicker(UIDatePicker* datePicker);

/// Sets the preferred date picker style on iOS 14+.
/// Values: 1 = spinner (wheels), 2 = overlay (compact), 3 = expanded (inline).
/// Source of truth: STYLE_MAP in composer/coreui/src/components/pickers/DatePickerPreferredStyle.ts
void SCValdiSetDatePickerStyle(UIDatePicker* datePicker, NSInteger style);

NS_ASSUME_NONNULL_END
