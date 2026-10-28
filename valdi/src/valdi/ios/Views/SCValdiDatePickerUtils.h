#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/// UIDatePicker defaults to a new "compact" date picker style starting from iOS 14,
/// this function configures a UIDatePicker instance to use the old style.
void SCValdiFixIOS14DatePicker(UIDatePicker* datePicker);

/// Sets the preferred date picker style on iOS 14+.
/// Values: 1 = spinner (wheels), 2 = overlay (compact), 3 = expanded (inline).
/// Source of truth: STYLE_MAP in the date picker's TypeScript preferred-style definitions
void SCValdiSetDatePickerStyle(UIDatePicker* datePicker, NSInteger style);

/// Reset path for the `preferredStyle` attribute. Restores wheels immediately, or when
/// +[SCValdiConfiguration deferCompactPickerStyleResetEnabled] is set, records it in `needsWheelsStyle`
/// and schedules a layout pass so a picker that stays on screen still picks it up.
void SCValdiDatePickerResetStyle(UIDatePicker* datePicker, BOOL* needsWheelsStyle);

/// Applies a reset recorded by SCValdiDatePickerResetStyle. Call before measuring or laying out.
void SCValdiDatePickerApplyDeferredStyle(UIDatePicker* datePicker, BOOL* needsWheelsStyle);

NS_ASSUME_NONNULL_END
