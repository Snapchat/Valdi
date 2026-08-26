//
//  SCValdiMacOSAttributesBinder.h
//  valdi-macos
//
//  Created by Simon Corsin on 10/13/20.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiMacOSAttributesBinder : NSObject

- (instancetype)initWithCppInstance:(void*)cppInstance cls:(Class)cls;

- (void)bindUntypedAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                    selector:(SEL)sel;
- (void)bindColorAttribute:(NSString*)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                    selector:(SEL)sel;
- (void)bindAccessibilityAttributes;

@end

NS_ASSUME_NONNULL_END
