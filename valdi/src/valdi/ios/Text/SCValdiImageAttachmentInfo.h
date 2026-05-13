//
//  SCValdiImageAttachmentInfo.h
//  valdi-ios
//
//  Created by Ivan Golub on 12/11/25.
//

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

/**
 * Contains information about an inline image attachment.
 */
@interface SCValdiImageAttachmentInfo : NSObject

/** Unique identifier for the attachment */
@property (nonatomic, copy, readonly, nonnull) NSString* attachmentId;
/** Width of the image in logical pixels */
@property (nonatomic, assign, readonly) CGFloat width;
/** Height of the image in logical pixels */
@property (nonatomic, assign, readonly) CGFloat height;
/** PNG image data. May be nil for placeholders. */
@property (nonatomic, strong, readonly, nullable) NSData* imageData;

- (instancetype)initWithAttachmentId:(nonnull NSString*)attachmentId
                               width:(CGFloat)width
                              height:(CGFloat)height
                           imageData:(nullable NSData*)imageData;

@end
