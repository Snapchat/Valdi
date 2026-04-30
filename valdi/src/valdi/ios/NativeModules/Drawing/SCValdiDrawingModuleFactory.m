//
//  SCValdiDrawingModuleFactory.m
//  valdi-ios
//
//  Created by Simon Corsin on 8/26/19.
//

#import "valdi/ios/NativeModules/Drawing/SCValdiDrawingModuleFactory.h"
#import "SCCDrawing/SCValdiDrawingModule.h"

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFont.h"
#import "valdi/ios/Text/SCValdiFontManager.h"
#import "valdi/ios/SCValdiContext.h"
#import "valdi/ios/Views/SCValdiLabel.h"
#import "valdi/ios/Text/SCValdiAttributedText.h"

#import "valdi_core/SCValdiError.h"

#import <UIKit/UIKit.h>

@interface SCValdiDrawingFontImpl: NSObject<SCValdiDrawingFont>

- (instancetype)initWithFont:(SCValdiFont *)font
                  lineHeight:(NSNumber* _Nullable)lineHeight;

@end

@implementation SCValdiDrawingFontImpl {
    SCValdiFont* _font;
    NSNumber* _Nullable _lineHeight;
}

- (instancetype)initWithFont:(SCValdiFont*)font
                  lineHeight:(NSNumber* _Nullable)lineHeight
{
    self = [super init];

    if (self) {
        _font = font;
        _lineHeight = lineHeight;
    }

    return self;
}

- (BOOL)shouldRetainInstanceWhenMarshalling
{
    return YES;
}

- (NSInteger)pushToValdiMarshaller:(nonnull SCValdiMarshallerRef)marshaller
{
    return SCValdiDrawingFontMarshall(marshaller, self);
}

- (SCValdiDrawingSize * _Nonnull)measureTextWithText:(NSString * _Nonnull)text maxWidth:(NSNumber * _Nullable)maxWidth maxHeight:(NSNumber * _Nullable)maxHeight maxLines:(NSNumber * _Nullable)maxLines
{
    return [self _measureText:text maxWidth:maxWidth maxHeight:maxHeight maxLines:maxLines];
}

- (SCValdiDrawingSize * _Nonnull)measureAttributedTextWithAttributedText:(SCValdiWrappedValue * _Nonnull)attributedText maxWidth:(NSNumber * _Nullable)maxWidth maxHeight:(NSNumber * _Nullable)maxHeight maxLines:(NSNumber * _Nullable)maxLines
{
    SCValdiAttributedText *valdiAttributedText = [[SCValdiAttributedText alloc] initWithWrappedValue:attributedText];
    return [self _measureText:valdiAttributedText maxWidth:maxWidth maxHeight:maxHeight maxLines:maxLines];
}

- (SCValdiDrawingSize * _Nonnull)_measureText:(id)text maxWidth:(NSNumber * _Nullable)maxWidth maxHeight:(NSNumber * _Nullable)maxHeight maxLines:(NSNumber * _Nullable)maxLines
{
    SCValdiFontAttributes *fontAttributes = [NSAttributedString fontAttributesWithFont:_font
                                                                                    color:nil
                                                                                textAlign:nil
                                                                               lineHeight:_lineHeight
                                                                           textDecoration:nil
                                                                            letterSpacing:nil
                                                                            numberOfLines:maxLines
                                                                             textOverflow:nil];

    CGFloat maxWidthF = maxWidth != nil ? maxWidth.doubleValue : CGFLOAT_MAX;
    CGFloat maxHeightF = maxHeight != nil ? maxHeight.doubleValue : CGFLOAT_MAX;
    CGSize maxSize = CGSizeMake(maxWidthF, maxHeightF);

    UITraitCollection *traitCollection = SCValdiContext.currentContext.traitCollection;

    CGSize measuredSize = [SCValdiLabel measureSizeWithMaxSize:maxSize fontAttributes:fontAttributes fontManager:_font.fontManager text:text traitCollection:traitCollection];

    return [[SCValdiDrawingSize alloc] initWithWidth:ceil(measuredSize.width) height:ceil(measuredSize.height)];
}

@end

@implementation SCValdiDrawingModuleFactory {
    SCValdiFontManager *_fontManager;
}

- (instancetype)initWithFontManager:(SCValdiFontManager *)fontManager
{
    self = [super init];

    if (self) {
        _fontManager = fontManager;
    }

    return self;
}

- (NSString *)getModulePath
{
    return @"Drawing";
}

- (nonnull NSObject *)loadModule {
    return @{
             @"Drawing": self
             };
}

- (BOOL)shouldRetainInstanceWhenMarshalling
{
    return YES;
}

- (NSInteger)pushToValdiMarshaller:(nonnull SCValdiMarshallerRef)marshaller
{
    return SCValdiDrawingModuleMarshall(marshaller, self);
}

- (id<SCValdiDrawingFont> _Nonnull)getFontWithSpecs:(SCValdiDrawingFontSpecs * _Nonnull)specs
{
    SCValdiFont *font = [SCValdiFont fontFromValdiAttribute:specs.font fontManager:_fontManager];

    return [[SCValdiDrawingFontImpl alloc] initWithFont:font
                                                   lineHeight:specs.lineHeight];
}

- (BOOL)isFontRegisteredWithFontName:(NSString * _Nonnull)fontName
{
    return [UIFont fontWithName:fontName size:12] != nil;
}

- (void)registerFontWithFontName:(NSString * _Nonnull)fontName weight:(SCValdiDrawingFontWeight _Nonnull)weight style:(SCValdiDrawingFontStyle _Nonnull)style filename:(NSString * _Nonnull)filename
{
    // Register downloaded font file
    NSError *error = nil;
    NSData *fontData = [NSData dataWithContentsOfFile:filename options:0 error:&error];

    if (fontData) {
        if (![_fontManager registerFontWithFontName:fontName data:fontData error:&error]) {
            SCValdiErrorThrow([error localizedDescription]);
        }
    } else {
        SCValdiErrorThrow([error localizedDescription]);
    }
}

@end
