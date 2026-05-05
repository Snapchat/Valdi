//
//  SCValdiDrawingModuleFactory.h
//  valdi-ios
//
//  Created by Simon Corsin on 8/26/19.
//

#import "SCCDrawingTypes/SCValdiDrawingModule.h"
#import "valdi_core/SCNValdiCoreModuleFactory.h"
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiFontManager;

@interface SCValdiDrawingModuleFactory : NSObject <SCNValdiCoreModuleFactory, SCValdiDrawingModule>

- (instancetype)initWithFontManager:(SCValdiFontManager*)fontManager;

@end

NS_ASSUME_NONNULL_END
