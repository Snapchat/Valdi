//
//  SCValdiViewLayoutAttributes.h
//  valdi_core
//
//  Created by Simon Corsin on 6/14/22.
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

@protocol SCValdiViewLayoutAttributes <NSObject>

- (id)valueForAttributeName:(NSString*)attributeName NS_SWIFT_NAME(value(forAttributeName:));
- (BOOL)boolValueForAttributeName:(NSString*)attributeName NS_SWIFT_NAME(boolValue(forAttributeName:));
- (NSString*)stringValueForAttributeName:(NSString*)attributeName NS_SWIFT_NAME(stringValue(forAttributeName:));
- (CGFloat)doubleValueForAttributeName:(NSString*)attributeName NS_SWIFT_NAME(doubleValue(forAttributeName:));

@end
