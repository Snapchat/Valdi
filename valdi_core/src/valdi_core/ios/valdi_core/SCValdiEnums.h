//
//  SCValdiEnums.h
//  valdi-ios
//
//  Created by Simon Corsin on 6/8/20.
//

#import "valdi_core/SCValdiFieldValue.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>

SC_EXTERN_C_BEGIN

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiEnum <NSObject>

@property (readonly, nonatomic) NSUInteger count;

@end

@interface SCValdiIntEnum : NSObject <SCValdiEnum>

- (instancetype)initWithEnumCases:(NSInteger*)enumCases count:(NSUInteger)count;
- (instancetype)initWithEnumCasesCount:(NSUInteger)enumCasesCount;

- (NSInteger)enumCaseForIndex:(NSUInteger)index;

@end

@interface SCValdiStringEnum : NSObject <SCValdiEnum>

- (instancetype)initWithEnumCases:(NSArray<NSString*>*)enumCases;

- (NSString*)enumCaseForIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END

#define VALDI_ENUM_CLASS_SUFFIX @"__Enum"

#define VALDI_ENUM_CLASS_NAME(__enumName) __enumName##__Enum

#define VALDI_TRIVIAL_INT_ENUM(__name, __count)                                                                        \
    @interface VALDI_ENUM_CLASS_NAME (__name): SCValdiIntEnum\
@end                                                                                                                   \
                                                                                                                       \
    @implementation VALDI_ENUM_CLASS_NAME (__name)                                                                     \
    -(instancetype)init {                                                                                              \
        return [self initWithEnumCasesCount:__count];                                                                  \
    }                                                                                                                  \
    @end

#define VALDI_INT_ENUM(__name, ...)                                                                                    \
    @interface VALDI_ENUM_CLASS_NAME (__name): SCValdiIntEnum\
@end                                                                                                                   \
                                                                                                                       \
    @implementation VALDI_ENUM_CLASS_NAME (__name)                                                                     \
    -(instancetype)init {                                                                                              \
        static NSInteger __kAllEnumCases[] = {__VA_ARGS__};                                                            \
        return [self initWithEnumCases:__kAllEnumCases count:(sizeof(__kAllEnumCases) / sizeof(NSInteger))];           \
    }                                                                                                                  \
    @end

#define VALDI_STRING_ENUM(__name, ...)                                                                                 \
    @interface VALDI_ENUM_CLASS_NAME (__name): SCValdiStringEnum\
@end                                                                                                                   \
                                                                                                                       \
    @implementation VALDI_ENUM_CLASS_NAME (__name)                                                                     \
    -(instancetype)init {                                                                                              \
        return [self initWithEnumCases:@[ __VA_ARGS__ ]];                                                              \
    }                                                                                                                  \
    @end

SC_EXTERN_C_END