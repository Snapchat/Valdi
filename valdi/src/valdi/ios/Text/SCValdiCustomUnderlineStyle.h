//
//  SCValdiCustomUnderlineStyle.h
//  Valdi
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiCustomUnderlineStyle : NSObject

@property (readonly, nonatomic) CGFloat height;
@property (readonly, nonatomic) CGFloat onWidth;
@property (readonly, nonatomic) CGFloat offWidth;
@property (readonly, nonatomic) CGFloat offset;
@property (readonly, nonatomic, getter=isPatterned) BOOL patterned;

- (instancetype)initWithHeight:(CGFloat)height
                       onWidth:(CGFloat)onWidth
                      offWidth:(CGFloat)offWidth
                        offset:(CGFloat)offset NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

+ (nullable instancetype)styleWithString:(NSString *)styleString error:(NSError *_Nullable *_Nullable)error;

@end

NS_ASSUME_NONNULL_END
