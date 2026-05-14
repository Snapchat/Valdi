
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi_core/SCValdiFontLoaderProtocol.h"
#import "valdi_core/SCValdiFontManagerProtocol.h"

@protocol SCValdiFontDataProvider <NSObject>

- (NSData*)fontDataForModuleName:(NSString*)moduleName fontPath:(NSString*)fontPath;

@end

@interface SCValdiFontManager : NSObject <SCValdiFontManagerProtocol>

- (void)addFontDataProvider:(id<SCValdiFontDataProvider>)fontDataProvider;

- (void)removeFontDataProvider:(id<SCValdiFontDataProvider>)fontDataProvider;

- (void)setFontLoader:(id<SCValdiFontLoaderProtocol>)fontLoader;

- (BOOL)registerFontWithFontName:(NSString*)fontName data:(NSData*)data error:(NSError**)error;

@end
