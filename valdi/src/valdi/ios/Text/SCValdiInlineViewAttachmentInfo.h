//
//  SCValdiInlineViewAttachmentInfo.h
//  valdi-ios
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SCValdiInlineViewVerticalAlignment) {
    SCValdiInlineViewVerticalAlignmentCenter = 0,
    SCValdiInlineViewVerticalAlignmentTop = 1,
    SCValdiInlineViewVerticalAlignmentBottom = 2,
    SCValdiInlineViewVerticalAlignmentBaseline = 3,
};

typedef CGSize (^SCValdiInlineViewAttachmentSizeProvider)(void);

/**
 * Metadata for one inline Valdi child embedded in an iOS attributed string.
 *
 * The attachment stores the Valdi child index and vertical alignment while
 * resolving size lazily through a provider. Lazy size lookup is important
 * because Yoga may update the child size between text layout passes.
 */
@interface SCValdiInlineViewAttachmentInfo : NSObject

@property (nonatomic, assign, readonly) NSInteger childIndex;
@property (nonatomic, assign, readonly) SCValdiInlineViewVerticalAlignment verticalAlignment;
@property (nonatomic, assign, readonly) CGSize size;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithChildIndex:(NSInteger)childIndex
                 verticalAlignment:(SCValdiInlineViewVerticalAlignment)verticalAlignment
                       sizeProvider:(SCValdiInlineViewAttachmentSizeProvider)sizeProvider NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
