#import "valdi/ios/Text/SCValdiFontManager.h"
#import <CoreText/CoreText.h>

@implementation SCValdiFontManager {
    id<SCValdiFontLoaderProtocol> _fontLoader;
    UITraitCollection *_defaultTraitCollection;
    NSMutableArray<id<SCValdiFontDataProvider>> * _fontDataProviders;
    NSMutableDictionary<NSString *, NSString *> *_registeredFontIdentifierByName;
}

- (NSString *)_lockFreeRegisterFontWithFontName:(NSString *)fontName data:(NSData *)data error:(NSError **)error
{
    CGDataProviderRef fontDataProvider = CGDataProviderCreateWithCFData((CFDataRef)data);
    CGFontRef newFont = CGFontCreateWithDataProvider(fontDataProvider);
    NSString *resolvedFontFullName = (NSString *)CFBridgingRelease(CGFontCopyFullName(newFont));
    CFErrorRef cfError = nil;
    BOOL success = CTFontManagerRegisterGraphicsFont(newFont, &cfError);
    CFRelease(newFont);
    CGDataProviderRelease(fontDataProvider);

    if (!success) {
        NSError *nsError = (NSError *)CFBridgingRelease(cfError);

        // https://developer.apple.com/documentation/coretext/ctfontmanagererror?language=objc
        if ([nsError code] != 105 && [nsError code] != 305) {
            // only 105/305 are allowed (kCTFontManagerErrorAlreadyRegistered/kCTFontManagerErrorDuplicatedName)
            *error = nsError;
            return nil;
        }
    }

    if (!_registeredFontIdentifierByName) {
        _registeredFontIdentifierByName = [NSMutableDictionary dictionary];
    }
    _registeredFontIdentifierByName[fontName] = resolvedFontFullName;

    return resolvedFontFullName;
}

- (BOOL)registerFontWithFontName:(NSString *)fontName data:(NSData *)data error:(NSError **)error
{
    @synchronized (self) {
        return [self _lockFreeRegisterFontWithFontName:fontName data:data error:error] != nil;
    }
}

- (UIFont *)_lockFreeModuleFontWithName:(NSString *)fontName
                               fontSize:(CGFloat)fontSize
{
    NSString *fontIdentifier = _registeredFontIdentifierByName[fontName];
    if (fontIdentifier) {
        return [UIFont fontWithName:fontIdentifier size:fontSize];
    }
    NSInteger idx = [fontName rangeOfString:@":"].location;
    NSString *moduleName = [fontName substringWithRange:NSMakeRange(0, idx)];
    NSString *fontPath = [fontName substringFromIndex:idx + 1];

    for (id<SCValdiFontDataProvider> fontDataProvider in _fontDataProviders) {
        NSData *fontData = [fontDataProvider fontDataForModuleName:moduleName fontPath:fontPath];
        if (fontData) {
            NSError *error;
            fontIdentifier = [self _lockFreeRegisterFontWithFontName:fontName data:fontData error:&error];
            if (fontIdentifier) {
                return [UIFont fontWithName:fontIdentifier size:fontSize];
            }
        }
    }

    return nil;
}

- (UIFont*)fontWithName:(NSString*)fontName
               fontSize:(CGFloat)fontSize
       legibilityWeight:(SCUILegibilityWeight)legibilityWeight
{
    @synchronized (self) {
        if ([fontName containsString:@":"]) {
            return [self _lockFreeModuleFontWithName:fontName fontSize:fontSize];
        }

        UIFont *font;
        if ([fontName isEqualToString:@"system"]) {
            font = [UIFont systemFontOfSize:fontSize];
        } else if ([fontName isEqualToString:@"system-bold"]) {
            font = [UIFont boldSystemFontOfSize:fontSize];
        } else if ([fontName isEqualToString:@"system-italic"]) {
            font = [UIFont italicSystemFontOfSize:fontSize];
        } else if (_fontLoader && [UIFont fontWithName:fontName size:fontSize]) {
            if ([_fontLoader shouldBypassContextForLegibilityWeight]) {
                font = [_fontLoader loadFontWithName:fontName fontSize:fontSize legibilityWeight:legibilityWeight];
            } else {
                font = [_fontLoader loadFontWithName:fontName fontSize:fontSize];
            }
        }

        if (!font) {
            font = [UIFont fontWithName:fontName size:fontSize];
        }

        if (!font) {
            font = [UIFont systemFontOfSize:fontSize];
        }

        return font;
    }
}

- (UITraitCollection *)defaultTraitCollection
{
    @synchronized (self) {
        if (!_defaultTraitCollection) {
            NSMutableArray *traitCollections = [NSMutableArray new];
            [traitCollections addObject:[UITraitCollection traitCollectionWithPreferredContentSizeCategory:UIContentSizeCategoryLarge]];
            if (@available(iOS 13.0, *)) {
                [traitCollections addObject:[UITraitCollection traitCollectionWithLegibilityWeight:UILegibilityWeightRegular]];
            }
            _defaultTraitCollection = [UITraitCollection traitCollectionWithTraitsFromCollections:traitCollections];
        }

        return _defaultTraitCollection;
    }
}

- (BOOL)shouldBypassContextForLegibilityWeight
{
    @synchronized (self) {
        if (_fontLoader) {
            return [_fontLoader shouldBypassContextForLegibilityWeight];
        }

        return NO;
    }
}

- (void)setFontLoader:(id<SCValdiFontLoaderProtocol>)fontLoader
{
    @synchronized (self) {
        _fontLoader = fontLoader;
    }
}

- (void)addFontDataProvider:(id<SCValdiFontDataProvider>)fontDataProvider
{
    @synchronized (self) {
        if (!_fontDataProviders) {
            _fontDataProviders = [NSMutableArray array];
        }
        [_fontDataProviders addObject:fontDataProvider];
    }
}

- (void)removeFontDataProvider:(id<SCValdiFontDataProvider>)fontDataProvider
{
    @synchronized (self) {
        [_fontDataProviders removeObject:fontDataProvider];
    }
}

@end
