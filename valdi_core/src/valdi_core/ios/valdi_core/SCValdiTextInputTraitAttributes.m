//
//  SCValdiTextInputTraitAttributes.m
//  valdi-ios
//
//  Created by Nathaniel Parrott on 9/25/19.
//

#import "valdi_core/SCValdiTextInputTraitAttributes.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"

BOOL SCValdiTextInputSetReturnKeyText(UIView<UITextInput> *view, NSString *returnKeyText)
{
    returnKeyText = [returnKeyText lowercaseString];
    if ([returnKeyText isEqualToString:@"done"] || returnKeyText.length == 0) {
        view.returnKeyType = UIReturnKeyDone;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"go"]) {
        view.returnKeyType = UIReturnKeyGo;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"join"]) {
        view.returnKeyType = UIReturnKeyJoin;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"next"]) {
        view.returnKeyType = UIReturnKeyNext;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"search"]) {
        view.returnKeyType = UIReturnKeySearch;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"send"]) {
        view.returnKeyType = UIReturnKeySend;
        return YES;
    }
    if ([returnKeyText isEqualToString:@"continue"]) {
        view.returnKeyType = UIReturnKeyContinue;
        return YES;
    }
    return NO;
}

BOOL SCValdiTextInputSetKeyboardAppearance(UIView<UITextInput> *view, NSString *keyboardAppearance)
{
    keyboardAppearance = [keyboardAppearance lowercaseString];
    if ([keyboardAppearance isEqualToString:@"dark"]) {
        view.keyboardAppearance = UIKeyboardAppearanceDark;
        return YES;
    }
    if ([keyboardAppearance isEqualToString:@"light"]) {
        view.keyboardAppearance = UIKeyboardAppearanceLight;
        return YES;
    }
    if ([keyboardAppearance isEqualToString:@"default"] || keyboardAppearance.length == 0) {
        view.keyboardAppearance = UIKeyboardAppearanceDefault;
        return YES;
    }
    return NO;
}

BOOL SCValdiTextInputSetContentTypeValues(UIView<UITextInput> *view, UIKeyboardType keyboard, UITextContentType text, BOOL secured, BOOL noSuggestions) {
    view.keyboardType = keyboard;
    view.textContentType = text;
    view.secureTextEntry = secured;
    if (noSuggestions) {
        view.autocorrectionType = UITextAutocorrectionTypeNo;
        view.spellCheckingType = UITextSpellCheckingTypeNo;
        view.smartDashesType = UITextSmartDashesTypeNo;
        view.smartQuotesType = UITextSmartQuotesTypeNo;
    } else {
        view.autocorrectionType = UITextAutocorrectionTypeYes;
        view.spellCheckingType = UITextSpellCheckingTypeYes;
        view.smartDashesType = UITextSmartDashesTypeYes;
        view.smartQuotesType = UITextSmartQuotesTypeYes;
    }
    return YES;
}

BOOL SCValdiTextInputSetContentType(UIView<UITextInput> *view, NSString *contentType)
{
    contentType = [contentType lowercaseString];
    if ([contentType isEqualToString:@"default"] || contentType.length == 0) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, nil, false, false);
    }
    if ([contentType isEqualToString:@"phonenumber"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypePhonePad, UITextContentTypeTelephoneNumber, false, false);
    }
    if ([contentType isEqualToString:@"email"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeEmailAddress, UITextContentTypeEmailAddress, false, false);
    }
    if ([contentType isEqualToString:@"password"]) {
        if (@available(iOS 11.0, *)) {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, UITextContentTypePassword, true, false);
        } else {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, nil, true, false);
        }
    }
    if ([contentType isEqualToString:@"passwordnumber"]) {
        if (@available(iOS 11.0, *)) {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeNumberPad, UITextContentTypePassword, true, false);
        } else {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeNumberPad, nil, true, false);
        }
    }
    if ([contentType isEqualToString:@"passwordvisible"]) {
        if (@available(iOS 11.0, *)) {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, UITextContentTypePassword, false, false);
        } else {
            return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, nil, false, false);
        }
    }
    if ([contentType isEqualToString:@"url"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeURL, UITextContentTypeURL, false, false);
    }
    if ([contentType isEqualToString:@"numberdecimal"] || [contentType isEqualToString:@"number"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDecimalPad, nil, false, false);
    }
    if ([contentType isEqualToString:@"numberdecimalsigned"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeNumbersAndPunctuation, nil, false, false);
    }
    if ([contentType isEqualToString:@"nosuggestions"]) {
        return SCValdiTextInputSetContentTypeValues(view, UIKeyboardTypeDefault, nil, false, true);
    }
    return NO;
}

BOOL SCValdiTextInputSetAutocapitalization(UIView<UITextInput> *view, NSString *autocapitalization)
{
    autocapitalization = [autocapitalization lowercaseString];
    if ([autocapitalization isEqualToString:@"words"]) {
        view.autocapitalizationType = UITextAutocapitalizationTypeWords;
        return YES;
    }
    if ([autocapitalization isEqualToString:@"sentences"]) {
        view.autocapitalizationType = UITextAutocapitalizationTypeSentences;
        return YES;
    }
    if ([autocapitalization isEqualToString:@"characters"]) {
        view.autocapitalizationType = UITextAutocapitalizationTypeAllCharacters;
        return YES;
    }
    if (autocapitalization.length == 0 || [autocapitalization isEqualToString:@"none"]) {
        view.autocapitalizationType = UITextAutocapitalizationTypeNone;
        return YES;
    }
    return NO;
}

BOOL SCValdiTextInputSetAutocorrection(UIView<UITextInput> *view, NSString *autocorrection)
{
    autocorrection = [autocorrection lowercaseString];
    if ([autocorrection isEqualToString:@"none"]) {
        view.autocorrectionType = UITextAutocorrectionTypeNo;
        view.spellCheckingType = UITextSpellCheckingTypeNo;
        return YES;
    }
    if (autocorrection.length == 0 || [autocorrection isEqualToString:@"default"]) {
        view.autocorrectionType = UITextAutocorrectionTypeDefault;
        view.spellCheckingType = UITextSpellCheckingTypeDefault;
        return YES;
    }
    return NO;
}

BOOL SCValdiTextInputSetTextDirection(UIView<UITextInput> *view, NSString *textDirection)
{
    textDirection = [textDirection lowercaseString];
    if ([textDirection isEqualToString:@"ltr"]) {
        view.semanticContentAttribute = UISemanticContentAttributeForceLeftToRight;
        return YES;
    }
    if ([textDirection isEqualToString:@"rtl"]) {
        view.semanticContentAttribute = UISemanticContentAttributeForceRightToLeft;
        return YES;
    }
    if (textDirection.length == 0 || [textDirection isEqualToString:@"locale"]) {
        view.semanticContentAttribute = UISemanticContentAttributeUnspecified;
        return YES;
    }
    return NO;
}
