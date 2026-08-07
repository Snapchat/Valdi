//
//  SCValdiTextView.h
//  Valdi
//
//  Created by Andrew Lin on 11/6/18.
//

#import "valdi/ios/Text/SCValdiAttributedTextHelper.h"
#import "valdi_core/SCValdiView.h"
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiTextView : SCValdiView <UITextViewDelegate, NSTextStorageDelegate, SCValdiTextAttributable>

+ (BOOL)valdi_managesChildFrames;

@property (nonatomic, strong) id textValue;
@property (nonatomic, strong) SCValdiFontAttributes* fontAttributes;
@property (nonatomic) SCValdiTextMode textMode;
@property (nonatomic) BOOL needAttributedTextUpdate;

@end

NS_ASSUME_NONNULL_END
