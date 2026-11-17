#import <Foundation/Foundation.h>
#import <valdi_core/SCValdiNativeConvertible.h>
#include <stdbool.h>

NS_ASSUME_NONNULL_BEGIN

typedef struct {
    int64_t value;
    bool success;
} SCValdiBigIntInt64Result;

/** An immutable, arbitrary precision integer backed by Valdi's native BigInt. */
@interface SCValdiBigInt : NSObject <SCValdiNativeConvertible>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithInt64:(int64_t)value;
- (nullable instancetype)initWithString:(NSString*)decimal error:(NSError* _Nullable* _Nullable)error;
/** Retains the borrowed native pointer produced by unsafeBridgeCast. */
- (instancetype)initWithBigInt:(void*)bigInt;
/** Returns a borrowed native pointer valid for this wrapper's lifetime. */
- (void*)bigInt;
/** value is the exact integer when success is true, and zero otherwise. */
- (SCValdiBigIntInt64Result)toInt64;
- (NSString*)toString;

@end

NS_ASSUME_NONNULL_END
