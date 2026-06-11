//
//  SCValdiTextInputTraitAttributes.h
//  valdi-ios
//
//  Created by Nathaniel Parrott on 9/25/19.
//

#import <UIKit/UIKit.h>

@protocol SCValdiAttributesBinderProtocol;

extern BOOL SCValdiTextInputSetReturnKeyText(UIView<UITextInput>* view, NSString* returnKeyText);

extern BOOL SCValdiTextInputSetKeyboardAppearance(UIView<UITextInput>* view, NSString* keyboardAppearance);

extern BOOL SCValdiTextInputSetContentType(UIView<UITextInput>* view, NSString* contentType);

extern BOOL SCValdiTextInputSetAutocapitalization(UIView<UITextInput>* view, NSString* autocapitalization);

extern BOOL SCValdiTextInputSetAutocorrection(UIView<UITextInput>* view, NSString* autocorrection);

extern BOOL SCValdiTextInputSetTextDirection(UIView<UITextInput>* view, NSString* textDirection);
