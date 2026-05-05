//
//  SCValdiImageAttachmentInfo.m
//  valdi-ios
//
//  Created by Ivan Golub on 12/11/25.
//

#import "valdi/ios/Text/SCValdiImageAttachmentInfo.h"

@implementation SCValdiImageAttachmentInfo

- (instancetype)initWithAttachmentId:(nonnull NSString *)attachmentId
                               width:(CGFloat)width
                              height:(CGFloat)height
                           imageData:(nullable NSData *)imageData
{
    self = [super init];
    if (self) {
        _attachmentId = attachmentId;
        _width = width;
        _height = height;
        _imageData = imageData;
    }
    return self;
}

@end
