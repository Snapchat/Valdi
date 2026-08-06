//
//  SCValdiInlineViewAttachmentInfo.m
//  valdi-ios
//

#import "valdi/ios/Text/SCValdiInlineViewAttachmentInfo.h"

@implementation SCValdiInlineViewAttachmentInfo {
    SCValdiInlineViewAttachmentSizeProvider _sizeProvider;
}

- (instancetype)initWithChildIndex:(NSInteger)childIndex
                 verticalAlignment:(SCValdiInlineViewVerticalAlignment)verticalAlignment
                       sizeProvider:(SCValdiInlineViewAttachmentSizeProvider)sizeProvider
{
    NSParameterAssert(sizeProvider != nil);
    self = [super init];
    if (self) {
        _childIndex = childIndex;
        _verticalAlignment = verticalAlignment;
        _sizeProvider = [sizeProvider copy];
    }
    return self;
}

- (CGSize)size
{
    return _sizeProvider();
}

@end
