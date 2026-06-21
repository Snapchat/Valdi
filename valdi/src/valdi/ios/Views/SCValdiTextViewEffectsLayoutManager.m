
#import "SCValdiTextViewEffectsLayoutManager.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"
#import "valdi/ios/Text/SCValdiTextAnimationCoordinator.h"
#import "valdi/ios/Text/SCValdiTextAnimationPresentation.h"
#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"
#import "valdi_core/SCValdiInternedString.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"
#import <CoreText/CoreText.h>
#import <QuartzCore/QuartzCore.h>

NSAttributedStringKey const kSCValdiTextViewCustomUnderlineColorAttribute = @"valdi_textViewCustomUnderlineColor";
INTERNED_STRING_CONST("valdi.textAnimationStartTimes", SCValdiTextAnimationStartTimesStorageKey);

@implementation SCValdiTextViewBackgroundEffects
@end

@interface SCValdiTextViewOutline : NSObject
@property (nonatomic, assign) NSRange range;
@property (nonatomic, strong) UIColor* color;
@property (nonatomic, assign) CGFloat width;
@end
@implementation SCValdiTextViewOutline
@end

@interface SCValdiTextViewAnimationRange : NSObject
@property (nonatomic, assign) NSRange range;
@property (nonatomic, assign) CGFloat translationY;
@property (nonatomic, assign) CGFloat scale;
@property (nonatomic, assign) CGFloat opacity;
@property (nonatomic, assign) CGFloat initialTranslationY;
@property (nonatomic, assign) CGFloat initialScale;
@property (nonatomic, assign) CGFloat initialOpacity;
@property (nonatomic, assign) double duration;
@property (nonatomic, assign) double startDelay;
@property (nonatomic, assign) double timeOffset;
@property (nonatomic, copy) NSString *rangeKey;
@property (nonatomic, copy) NSString *timelineKey;
@property (nonatomic, assign) BOOL shouldStoreStartTime;
@property (nonatomic, assign) BOOL hasStartTime;
@property (nonatomic, assign) double startTime;
@end
@implementation SCValdiTextViewAnimationRange
@end

@interface SCValdiTextViewAnimationTimelineState : NSObject
@property (nonatomic, assign) BOOL hasExistingAnimationStartTime;
@property (nonatomic, assign) double existingAnimationStartTime;
@property (nonatomic, assign) BOOL hasNewAnimationBaseStartDelay;
@property (nonatomic, assign) double newAnimationBaseStartDelay;
@property (nonatomic, assign) BOOL hasNewAnimationStartTime;
@property (nonatomic, assign) double newAnimationStartTime;
@end
@implementation SCValdiTextViewAnimationTimelineState
@end

@interface SCValdiTextViewCustomUnderline : NSObject
@property (nonatomic, assign) NSRange range;
@property (nonatomic, strong) UIColor* color;

+ (instancetype)customUnderlineWithRange:(NSRange)range color:(UIColor *)color;

@end

@implementation SCValdiTextViewCustomUnderline

- (instancetype)initWithRange:(NSRange)range color:(UIColor *)color
{
    self = [super init];
    if (!self) {
        return nil;
    }
    _range = range;
    _color = color;
    return self;
}

+ (instancetype)customUnderlineWithRange:(NSRange)range color:(UIColor *)color
{
    return [[self alloc] initWithRange:range color:color];
}

@end

@interface SCValdiTextAnimationStoredProgress : NSObject
@property (nonatomic, strong, readonly) NSMutableDictionary<NSString *, NSNumber *> *startTimes;
@end
@implementation SCValdiTextAnimationStoredProgress
- (instancetype)init
{
    self = [super init];
    if (self) {
        _startTimes = [NSMutableDictionary new];
    }
    return self;
}
@end

@interface SCValdiTextViewEffectsLayoutManager ()
@property (nonatomic, strong) NSArray<SCValdiTextViewAnimationRange *> *animationEntries;
@property (nonatomic, strong) NSArray<SCValdiTextViewAnimationRange *> *cachedAnimationRanges;
@property (nonatomic, strong) NSArray<SCValdiTextViewAnimationRange *> *cachedVisibleAnimationRanges;
@property (nonatomic, strong) NSArray<SCValdiTextViewOutline *> *cachedOutlineRanges;
@property (nonatomic, strong) NSArray<SCValdiTextViewCustomUnderline *> *cachedCustomUnderlineRanges;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSNumber *> *animationStartTimes;
@property (nonatomic, strong) SCValdiTextAnimationStoredProgress *storedAnimationProgress;
@property (nonatomic, assign, readwrite) BOOL hasActiveAnimationRanges;
@end

@implementation SCValdiTextViewEffectsLayoutManager

static BOOL SCValdiAnimationRangeHasVisibleTransform(SCValdiTextViewAnimationRange *animationRange)
{
    // TODO(CREATE-86642): Define animation-state semantics once in shared Valdi code
    // (or bridge explicit state from TS) so Android/iOS stop duplicating thresholds.
    return fabs(animationRange.translationY) > DBL_EPSILON ||
           fabs(animationRange.scale - 1.0) > DBL_EPSILON ||
           fabs(animationRange.opacity - 1.0) > DBL_EPSILON;
}

static NSString *SCValdiAnimationRangeKey(SCValdiTextAnimationTransform *animationTransform)
{
    if (animationTransform.key != nil) {
        return [NSString stringWithFormat:@"%@:%lu", animationTransform.key, (unsigned long)animationTransform.partIndex];
    }
    return [NSString stringWithFormat:@"%lu", (unsigned long)animationTransform.partIndex];
}

static NSString *SCValdiAnimationTimelineKey(SCValdiTextAnimationTransform *animationTransform)
{
    if (animationTransform.key != nil) {
        return animationTransform.key;
    }
    return [NSString stringWithFormat:@"group:%lu", (unsigned long)animationTransform.groupIndex];
}

static double SCValdiAnimationTimeOffset(SCValdiTextAnimationTransform *animationTransform)
{
    return MAX(animationTransform.timeOffsetBetweenParts, 0.0);
}

static double SCValdiAnimationStartDelay(SCValdiTextAnimationTransform *animationTransform, NSUInteger basePartIndex)
{
    return SCValdiAnimationTimeOffset(animationTransform) * (basePartIndex + animationTransform.partIndexInGroup);
}

static BOOL SCValdiAnimationShouldTrack(SCValdiTextAnimationTransform *animationTransform, double startDelay)
{
    BOOL hasVisibleStartTransform = animationTransform.translationY != 0.0 ||
                                    animationTransform.scale != 1.0 ||
                                    animationTransform.opacity != 1.0;
    return hasVisibleStartTransform && (animationTransform.duration > 0.0 || startDelay > 0.0);
}

static SCValdiTextViewAnimationTimelineState *SCValdiAnimationTimelineStateForKey(
    NSMutableDictionary<NSString *, SCValdiTextViewAnimationTimelineState *> *animationTimelineStates,
    NSString *timelineKey)
{
    SCValdiTextViewAnimationTimelineState *timelineState = animationTimelineStates[timelineKey];
    if (timelineState == nil) {
        timelineState = [SCValdiTextViewAnimationTimelineState new];
        animationTimelineStates[timelineKey] = timelineState;
    }
    return timelineState;
}

static NSArray<NSValue *> *SCValdiSubtractAnimationRanges(NSRange range,
                                                          NSArray<SCValdiTextViewAnimationRange *> *animationRanges)
{
    NSMutableArray<NSValue *> *remainingRanges = [NSMutableArray new];
    // Walk the original range left-to-right and emit only the gaps that are not animated.
    // This depends on animationRanges being in ascending document order.
    NSUInteger currentLocation = range.location;
    NSUInteger rangeEnd = NSMaxRange(range);

    for (SCValdiTextViewAnimationRange *animationRange in animationRanges) {
        NSRange intersectionRange = NSIntersectionRange(range, animationRange.range);
        // Ignore animation ranges that do not overlap the outline range at all.
        if (intersectionRange.length == 0) {
            continue;
        }

        // Preserve any static text that appears before this animated subrange. Because the
        // input ranges are monotonic, this is always the next untouched static segment.
        if (intersectionRange.location > currentLocation) {
            [remainingRanges addObject:[NSValue valueWithRange:NSMakeRange(currentLocation, intersectionRange.location - currentLocation)]];
        }

        // Advance past the animated portion. MAX keeps the cursor monotonic if adjacent or
        // overlapping animation ranges ever collapse into the same covered region.
        currentLocation = MAX(currentLocation, NSMaxRange(intersectionRange));
        // Once we've consumed the full range, there is nothing left to emit.
        if (currentLocation >= rangeEnd) {
            break;
        }
    }

    // Emit the trailing static segment after the last overlapping animation range.
    if (currentLocation < rangeEnd) {
        [remainingRanges addObject:[NSValue valueWithRange:NSMakeRange(currentLocation, rangeEnd - currentLocation)]];
    }

    return remainingRanges;
}

- (UIColor *)backgroundColor
{
    return _effects.color ? _effects.color : [UIColor clearColor];
}

- (CGFloat)backgroundBorderRadius
{
    return _effects.borderRadius ? _effects.borderRadius : 0.0;
}

- (CGFloat)backgroundPadding
{
    return _effects.padding ? _effects.padding : 0.0;
}

- (BOOL)invalidateAnimatedTextProgress
{
    [self _invalidateAnimationRangeCaches];
    [self _animationRanges];
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
    return self.hasActiveAnimationRanges;
}

- (CGFloat)opacityForAnimationRange:(NSRange)range
{
    SCValdiTextAnimationPresentation *presentation = [self presentationForAnimationRange:range];
    return presentation != nil ? presentation.opacity : 1.0;
}

- (SCValdiTextAnimationPresentation *)presentationForAnimationRange:(NSRange)range
{
    if (range.length == 0) {
        return nil;
    }

    for (SCValdiTextViewAnimationRange *animationRange in [self _animationRanges]) {
        if (NSIntersectionRange(animationRange.range, range).length > 0) {
            return [[SCValdiTextAnimationPresentation alloc] initWithTranslationY:animationRange.translationY
                                                                           scale:animationRange.scale
                                                                         opacity:animationRange.opacity];
        }
    }

    return nil;
}

- (void)setProcessedText:(SCValdiProcessedText *)processedText
{
    if (_processedText == processedText) {
        return;
    }
    _processedText = processedText;
    [self _invalidateAnimationEntries];
    self.cachedOutlineRanges = nil;
    self.cachedCustomUnderlineRanges = nil;
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
}

- (void)setTextAnimationCoordinator:(SCValdiTextAnimationCoordinator *)textAnimationCoordinator
{
    if (_textAnimationCoordinator == textAnimationCoordinator) {
        return;
    }

    _textAnimationCoordinator = textAnimationCoordinator;
    [self _invalidateAnimationEntries];
}

- (void)setTextAnimationBasePartIndex:(NSUInteger)textAnimationBasePartIndex
{
    if (_textAnimationBasePartIndex == textAnimationBasePartIndex) {
        return;
    }

    _textAnimationBasePartIndex = textAnimationBasePartIndex;
    [self _invalidateAnimationEntries];
}

- (void)setValdiViewNode:(id<SCValdiViewNodeProtocol>)valdiViewNode
{
    if (_valdiViewNode == valdiViewNode) {
        return;
    }

    _valdiViewNode = valdiViewNode;
    self.storedAnimationProgress = [self _storedAnimationProgressInViewNode:valdiViewNode createIfNeeded:NO];
}

- (void)prepareGroupedAnimatedTextProgress
{
    SCValdiTextAnimationCoordinator *coordinator = self.textAnimationCoordinator;
    if (!coordinator) {
        return;
    }

    for (SCValdiTextViewAnimationRange *animationEntry in [self _animationEntries]) {
        if (!animationEntry.hasStartTime) {
            continue;
        }

        [coordinator recordExistingAnimationScheduledStartTime:animationEntry.startTime + animationEntry.startDelay
                                                forTimelineKey:animationEntry.timelineKey];
    }
}

- (void)saveAnimatedTextProgress
{
    [self _animationRanges];
    SCValdiTextAnimationStoredProgress *storedProgress = self.storedAnimationProgress;
    BOOL didResolveStoredProgress = storedProgress != nil;
    for (SCValdiTextViewAnimationRange *animationEntry in [self _animationEntries]) {
        if (animationEntry.shouldStoreStartTime && animationEntry.hasStartTime) {
            if (!didResolveStoredProgress) {
                storedProgress = [self _storedAnimationProgressCreatingIfNeeded];
                didResolveStoredProgress = YES;
            }
            [self _storeAnimationStartTime:animationEntry.startTime
                                forRangeKey:animationEntry.rangeKey
                          inStoredProgress:storedProgress];
        }
    }
}

- (void)clearAnimatedTextProgress
{
    [self.animationStartTimes removeAllObjects];
    self.animationEntries = nil;
    self.hasActiveAnimationRanges = NO;
    [self _invalidateAnimationRangeCaches];
}

- (void)setCustomUnderlineStyle:(SCValdiCustomUnderlineStyle *)customUnderlineStyle
{
    if (_customUnderlineStyle == customUnderlineStyle) {
        return;
    }

    _customUnderlineStyle = customUnderlineStyle;
    self.cachedCustomUnderlineRanges = nil;
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
}

- (void)setCustomUnderlineSourceAttributedString:(NSAttributedString *)customUnderlineSourceAttributedString
{
    if (_customUnderlineSourceAttributedString == customUnderlineSourceAttributedString) {
        return;
    }

    _customUnderlineSourceAttributedString = customUnderlineSourceAttributedString;
    self.cachedCustomUnderlineRanges = nil;
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
}

- (void)setCustomUnderlineCharacterRanges:(NSArray<NSValue *> *)customUnderlineCharacterRanges
{
    if (_customUnderlineCharacterRanges == customUnderlineCharacterRanges) {
        return;
    }

    _customUnderlineCharacterRanges = [customUnderlineCharacterRanges copy];
    self.cachedCustomUnderlineRanges = nil;
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
}

- (void)setCustomUnderlineFallbackColor:(UIColor *)customUnderlineFallbackColor
{
    if (_customUnderlineFallbackColor == customUnderlineFallbackColor) {
        return;
    }

    _customUnderlineFallbackColor = customUnderlineFallbackColor;
    self.cachedCustomUnderlineRanges = nil;
    [self invalidateDisplayForCharacterRange:NSMakeRange(0, self.textStorage.length)];
}


#pragma mark - Outline Drawing

/// Drawing a stroke around text (attributed text key `NSStrokeWidthAttributeName`) has two options:
/// 1. A positive value draws a stroke alone around the text glyphs.
/// 2. A negative value draws the text fill and then inner strokes the glphys.
///
/// For a text outline, we want the stoke to draw around each text glyph and then fill the text.
/// This ensures a true outline around each glyph and hides any stroke artifacts that might occur from some fonts.
- (void)drawGlyphsForGlyphRange:(NSRange)glyphsToShow atPoint:(CGPoint)origin
{
    NSRange totalGlyphRange = [self glyphRangeForCharacterRange:NSMakeRange(0, self.textStorage.length) actualCharacterRange:nil];

    NSAttributedString *attributedString = self.textStorage;
    NSArray<SCValdiTextViewAnimationRange *> *animationRanges = [self _visibleAnimationRanges];
    NSArray<SCValdiTextViewOutline *> *outlineRanges = [self _outlineRanges];
    NSArray<SCValdiTextViewCustomUnderline *> *customUnderlineRanges = [self _customUnderlineRangesForAttributedString:attributedString];
    
    if (outlineRanges.count == 0 && animationRanges.count == 0 && customUnderlineRanges.count == 0) {
        // No outlines, custom underlines, or animated glyphs to draw.
        [super drawGlyphsForGlyphRange:glyphsToShow atPoint:origin];
        return;
    }

    // Adjust the origin to account for the size increase of the text container in `usedRectForTextContainer:`
    CGPoint adjustedOrigin = [self _getAdjustedOriginForPoint:origin];
    CGContextRef context = UIGraphicsGetCurrentContext();

    // First draw the outlines for the text.
    for (SCValdiTextViewOutline *outline in outlineRanges) {
        [self _drawOutline:outline attributedString:attributedString glyphsOrigin:adjustedOrigin context:context];
    }

    // Paint the non-animated glyphs now so the transformed glyph pass only needs to redraw the animated ranges.
    [self _drawStaticGlyphsForGlyphRange:glyphsToShow atPoint:adjustedOrigin animationRanges:animationRanges];

    // Last draw the animated glyphs themselves with the configured transform for each range.
    for (SCValdiTextViewAnimationRange *animationRange in animationRanges) {
        [self _drawAnimatedRange:animationRange glyphsOrigin:adjustedOrigin context:context];
    }

    [self _drawStaticCustomUnderlines:customUnderlineRanges
                      animationRanges:animationRanges
                         glyphsToShow:glyphsToShow
                         glyphsOrigin:adjustedOrigin
                              context:context];
}

- (void)processEditingForTextStorage:(NSTextStorage *)textStorage
                              edited:(NSTextStorageEditActions)editedMask
                               range:(NSRange)newCharRange
                      changeInLength:(NSInteger)delta
                    invalidatedRange:(NSRange)invalidatedCharRange
{
    [super processEditingForTextStorage:textStorage
                                 edited:editedMask
                                  range:newCharRange
                         changeInLength:delta
                       invalidatedRange:invalidatedCharRange];

    if ((editedMask & NSTextStorageEditedAttributes) != 0 || (editedMask & NSTextStorageEditedCharacters) != 0) {
        [self _invalidateAnimationEntries];
        self.cachedOutlineRanges = nil;
        self.cachedCustomUnderlineRanges = nil;
    }
}

- (CGRect)usedRectForTextContainer:(NSTextContainer *)container
{
    CGRect rect = [super usedRectForTextContainer:container];
    // Increase the size of the text container to account for the outline's width, otherwise the outline can clip with the edge of the container
    CGFloat maxOutlineWidth = [self _maximumDrawnOuterOutlineSize];
    rect.size.width += maxOutlineWidth;
    rect.size.height += maxOutlineWidth;
    return rect;
}

- (CGFloat)_maximumDrawnOuterOutlineSize
{
    CGFloat maxOutlineWidth = 0.0;
    for (SCValdiTextViewOutline *outline in [self _outlineRangesInRange:NSMakeRange(0, self.textStorage.length)]) {
        if (outline.width > maxOutlineWidth) {
            maxOutlineWidth = outline.width;
        }
    }
    return maxOutlineWidth;
}

- (CGPoint)_getAdjustedOriginForPoint:(CGPoint)origin
{
    CGFloat maxOutlineSize = [self _maximumDrawnOuterOutlineSize] / 2.0;
    if (maxOutlineSize <= 0) {
        return origin;
    }
    NSTextAlignment alignment = self.textStorage.length > 0 ?
        [[self.textStorage attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil] alignment] : NSTextAlignmentLeft;
    CGFloat adjustedX = origin.x;
    switch (alignment) {
        case NSTextAlignmentRight:
            adjustedX -= maxOutlineSize;
            break;
        case NSTextAlignmentCenter:
            adjustedX -= maxOutlineSize / 2.0;
            break;
        case NSTextAlignmentLeft:
        default:
            adjustedX += maxOutlineSize;
            break;
    }
    return CGPointMake(adjustedX, origin.y + maxOutlineSize);
}

- (void)_drawOutline:(SCValdiTextViewOutline *)outline attributedString:(NSAttributedString *)attributedString glyphsOrigin:(CGPoint)origin context:(CGContextRef)context
{
    NSRange charRange = outline.range;
    if (charRange.length == 0) {
        return;
    }

    NSUInteger charIndex = charRange.location;
    NSUInteger charRangeEnd = NSMaxRange(charRange);

    while (charIndex < charRangeEnd) {
        // Get the line fragment range for this character index as the charRange might span multiple lines
        NSRange lineRange;
        [self lineFragmentRectForGlyphAtIndex:[self glyphIndexForCharacterAtIndex:charIndex] effectiveRange:&lineRange];

        // Intersect the outline's range and this line fragment's range
        NSRange intersectionRange = NSIntersectionRange(charRange, lineRange);
        if (intersectionRange.length == 0) {
            charIndex = NSMaxRange(lineRange);
            continue;
        }

        NSRange glyphRange = [self glyphRangeForCharacterRange:intersectionRange actualCharacterRange:nil];
        CGPoint glyphLocation = [self locationForGlyphAtIndex:glyphRange.location];
        CGRect boundingRect = [self boundingRectForGlyphRange:glyphRange inTextContainer:[self textContainerForGlyphAtIndex:glyphRange.location effectiveRange:nil]];
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, origin.x + boundingRect.origin.x, origin.y + glyphLocation.y + boundingRect.origin.y);
        CGContextScaleCTM(context, 1.0, -1.0); // Flip context for CoreText

        // Get each glyph from this line's runs
        // This is done over getting the glyph directly from the layoutmanager as the run accounts for ligatures like "fi"
        NSAttributedString *subAttrString = [attributedString attributedSubstringFromRange:intersectionRange];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)subAttrString);
        CFArrayRef runs = CTLineGetGlyphRuns(line);
        CFIndex runCount = CFArrayGetCount(runs);
        for (CFIndex runIndex = 0; runIndex < runCount; runIndex++) {
            CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, runIndex);
            CFIndex glyphCount = CTRunGetGlyphCount(run);
            if (glyphCount == 0) {
                continue;
            }
            NSDictionary *runAttrs = (NSDictionary *)CTRunGetAttributes(run);
            CTFontRef runFont = (__bridge CTFontRef)runAttrs[(__bridge id)kCTFontAttributeName];
            if (!runFont) {
                continue;
            }

            CGGlyph glyphs[glyphCount];
            CGPoint positions[glyphCount];
            CTRunGetGlyphs(run, CFRangeMake(0, glyphCount), glyphs);
            CTRunGetPositions(run, CFRangeMake(0, glyphCount), positions);

            for (CFIndex glyphIndex = 0; glyphIndex < glyphCount; glyphIndex++) {
                CGGlyph glyph = glyphs[glyphIndex];
                CGPoint glyphPosition = positions[glyphIndex];
                CGPathRef glyphPath = CTFontCreatePathForGlyph(runFont, glyph, nil);
                CGPathRef strokedGlyphPath = nil;
                if (glyphPath) {
                    // Create a new path instead of stroking the original path as some fonts have overlapping shapes which will cause artifacts to render strangely outside of a true outline
                    strokedGlyphPath = CGPathCreateCopyByStrokingPath(glyphPath, nil, outline.width, kCGLineCapRound, kCGLineJoinRound, 0);
                }
                if (strokedGlyphPath) {
                    CGContextSaveGState(context);
                    CGContextTranslateCTM(context, glyphPosition.x, glyphPosition.y);
                    CGContextAddPath(context, strokedGlyphPath);
                    CGContextSetFillColorWithColor(context, outline.color.CGColor);
                    CGContextFillPath(context);
                    CGContextRestoreGState(context);
                }
                CGPathRelease(glyphPath);
                CGPathRelease(strokedGlyphPath);
            }
        }

        CFRelease(line);
        CGContextRestoreGState(context);

        charIndex = NSMaxRange(intersectionRange);
    }
}

- (void)_invalidateAnimationRangeCaches
{
    self.cachedAnimationRanges = nil;
    self.cachedVisibleAnimationRanges = nil;
}

- (void)_invalidateAnimationEntries
{
    self.animationEntries = nil;
    [self _invalidateAnimationRangeCaches];
}

- (NSArray<SCValdiTextViewAnimationRange *> *)_animationEntries
{
    if (self.animationEntries != nil) {
        return self.animationEntries;
    }

    if (self.animationStartTimes == nil) {
        self.animationStartTimes = [NSMutableDictionary new];
    }

    SCValdiProcessedText *processedText = self.processedText;
    if (processedText == nil) {
        self.animationEntries = @[];
        [self.animationStartTimes removeAllObjects];
        return self.animationEntries;
    }

    NSMutableArray<SCValdiTextViewAnimationRange *> *animationEntries = [NSMutableArray new];
    NSMutableSet<NSString *> *currentKeys = [NSMutableSet new];
    SCValdiTextAnimationStoredProgress *storedProgress = self.storedAnimationProgress;
    NSUInteger basePartIndex = self.textAnimationCoordinator ? self.textAnimationBasePartIndex : 0;
    [processedText enumerateAnimationTransformsUsingBlock:^(SCValdiTextAnimationTransform *animationTransform,
                                                            NSRange range,
                                                            BOOL *stop) {
        if (range.length == 0) {
            return;
        }

        double startDelay = SCValdiAnimationStartDelay(animationTransform, basePartIndex);
        if (!SCValdiAnimationShouldTrack(animationTransform, startDelay)) {
            return;
        }

        SCValdiTextViewAnimationRange *animationEntry = [SCValdiTextViewAnimationRange new];
        animationEntry.range = range;
        animationEntry.translationY = 0.0;
        animationEntry.scale = 1.0;
        animationEntry.opacity = 1.0;
        animationEntry.initialTranslationY = animationTransform.translationY;
        animationEntry.initialScale = animationTransform.scale;
        animationEntry.initialOpacity = animationTransform.opacity;
        animationEntry.duration = animationTransform.duration;
        animationEntry.startDelay = startDelay;
        animationEntry.timeOffset = SCValdiAnimationTimeOffset(animationTransform);
        animationEntry.rangeKey = SCValdiAnimationRangeKey(animationTransform);
        animationEntry.timelineKey = SCValdiAnimationTimelineKey(animationTransform);
        animationEntry.shouldStoreStartTime = animationTransform.key != nil;
        [currentKeys addObject:animationEntry.rangeKey];

        NSNumber *startTime = self.animationStartTimes[animationEntry.rangeKey];
        if (startTime == nil && animationEntry.shouldStoreStartTime) {
            startTime = [self _storedAnimationStartTimeForRangeKey:animationEntry.rangeKey
                                                  inStoredProgress:storedProgress];
        }
        if (startTime != nil) {
            animationEntry.hasStartTime = YES;
            animationEntry.startTime = startTime.doubleValue;
        }

        [animationEntries addObject:animationEntry];
    }];

    NSMutableSet<NSString *> *previousKeys = [NSMutableSet setWithArray:self.animationStartTimes.allKeys];
    [previousKeys minusSet:currentKeys];
    [self.animationStartTimes removeObjectsForKeys:previousKeys.allObjects];

    self.animationEntries = animationEntries;
    return self.animationEntries;
}

- (NSArray<SCValdiTextViewAnimationRange *> *)_animationRanges
{
    if (self.cachedAnimationRanges != nil) {
        return self.cachedAnimationRanges;
    }

    NSArray<SCValdiTextViewAnimationRange *> *animationEntries = [self _animationEntries];
    if (animationEntries.count == 0) {
        self.hasActiveAnimationRanges = NO;
        self.cachedAnimationRanges = @[];
        self.cachedVisibleAnimationRanges = nil;
        return self.cachedAnimationRanges;
    }

    SCValdiTextAnimationCoordinator *coordinator = self.textAnimationCoordinator;
    NSMutableDictionary<NSString *, SCValdiTextViewAnimationTimelineState *> *animationTimelineStates = coordinator ? nil : [NSMutableDictionary new];
    __block BOOL hasActiveAnimationRanges = NO;
    CFTimeInterval currentTime = CACurrentMediaTime();
    SCValdiTextAnimationStoredProgress *storedProgress = self.storedAnimationProgress;
    BOOL didResolveStoredProgress = storedProgress != nil;

    if (coordinator) {
        [self prepareGroupedAnimatedTextProgress];
    } else {
        for (SCValdiTextViewAnimationRange *animationEntry in animationEntries) {
            if (!animationEntry.hasStartTime) {
                continue;
            }
            double scheduledStartTime = animationEntry.startTime + animationEntry.startDelay;
            SCValdiTextViewAnimationTimelineState *timelineState =
                SCValdiAnimationTimelineStateForKey(animationTimelineStates, animationEntry.timelineKey);
            if (!timelineState.hasExistingAnimationStartTime ||
                scheduledStartTime > timelineState.existingAnimationStartTime) {
                timelineState.hasExistingAnimationStartTime = YES;
                timelineState.existingAnimationStartTime = scheduledStartTime;
            }
        }
    }

    for (SCValdiTextViewAnimationRange *animationEntry in animationEntries) {
        if (!animationEntry.hasStartTime) {
            double startTime = currentTime;
            if (coordinator) {
                startTime = [coordinator startTimeForNewAnimationWithTimelineKey:animationEntry.timelineKey
                                                                       timeOffset:animationEntry.timeOffset
                                                                      currentTime:currentTime];
            } else {
                SCValdiTextViewAnimationTimelineState *timelineState =
                    SCValdiAnimationTimelineStateForKey(animationTimelineStates, animationEntry.timelineKey);
                if (!timelineState.hasNewAnimationBaseStartDelay) {
                    timelineState.hasNewAnimationBaseStartDelay = YES;
                    timelineState.newAnimationBaseStartDelay = animationEntry.startDelay;
                }
                if (!timelineState.hasNewAnimationStartTime) {
                    timelineState.hasNewAnimationStartTime = YES;
                    timelineState.newAnimationStartTime = timelineState.hasExistingAnimationStartTime ?
                        MAX(currentTime, timelineState.existingAnimationStartTime + animationEntry.timeOffset) :
                        currentTime;
                }
                startTime = timelineState.newAnimationStartTime - timelineState.newAnimationBaseStartDelay;
            }
            animationEntry.hasStartTime = YES;
            animationEntry.startTime = startTime;
            self.animationStartTimes[animationEntry.rangeKey] = @(startTime);
            if (animationEntry.shouldStoreStartTime) {
                if (!didResolveStoredProgress) {
                    storedProgress = [self _storedAnimationProgressCreatingIfNeeded];
                    didResolveStoredProgress = YES;
                }
                [self _storeAnimationStartTime:startTime
                                    forRangeKey:animationEntry.rangeKey
                              inStoredProgress:storedProgress];
            }
        }

        CFTimeInterval delayedElapsedTime = currentTime - animationEntry.startTime - animationEntry.startDelay;
        double progress = animationEntry.duration > 0.0 ?
            MIN(MAX(delayedElapsedTime / animationEntry.duration, 0.0), 1.0) :
            (delayedElapsedTime >= 0.0 ? 1.0 : 0.0);
        if (progress < 1.0) {
            hasActiveAnimationRanges = YES;
        }
        double inverseProgress = 1.0 - progress;
        progress = 1.0 - inverseProgress * inverseProgress * inverseProgress;

        animationEntry.translationY = animationEntry.initialTranslationY * (1.0 - progress);
        animationEntry.scale = animationEntry.initialScale + (1.0 - animationEntry.initialScale) * progress;
        animationEntry.opacity = animationEntry.initialOpacity + (1.0 - animationEntry.initialOpacity) * progress;
    }

    self.hasActiveAnimationRanges = hasActiveAnimationRanges;
    self.cachedAnimationRanges = animationEntries;
    self.cachedVisibleAnimationRanges = nil;
    return self.cachedAnimationRanges;
}

- (SCValdiTextAnimationStoredProgress *)_storedAnimationProgressCreatingIfNeeded
{
    SCValdiTextAnimationStoredProgress *storedProgress = self.storedAnimationProgress;
    if (storedProgress != nil) {
        return storedProgress;
    }

    storedProgress = [self _storedAnimationProgressInViewNode:self.valdiViewNode createIfNeeded:YES];
    self.storedAnimationProgress = storedProgress;
    return storedProgress;
}

- (SCValdiTextAnimationStoredProgress *)_storedAnimationProgressInViewNode:(id<SCValdiViewNodeProtocol>)viewNode
                                                           createIfNeeded:(BOOL)createIfNeeded
{
    if (viewNode == nil) {
        return nil;
    }

    id storedObject = [viewNode storedObjectForKey:SCValdiTextAnimationStartTimesStorageKey()];
    SCValdiTextAnimationStoredProgress *storedProgress =
        [storedObject isKindOfClass:SCValdiTextAnimationStoredProgress.class] ? storedObject : nil;
    if (storedProgress == nil && createIfNeeded) {
        storedProgress = [SCValdiTextAnimationStoredProgress new];
        [viewNode setStoredObject:storedProgress forKey:SCValdiTextAnimationStartTimesStorageKey()];
    }
    return storedProgress;
}

- (NSNumber *)_storedAnimationStartTimeForRangeKey:(NSString *)rangeKey
                                  inStoredProgress:(SCValdiTextAnimationStoredProgress *)storedProgress
{
    id startTime = storedProgress.startTimes[rangeKey];
    return [startTime isKindOfClass:NSNumber.class] ? startTime : nil;
}

- (void)_storeAnimationStartTime:(double)startTime forRangeKey:(NSString *)rangeKey
                inStoredProgress:(SCValdiTextAnimationStoredProgress *)storedProgress
{
    if (storedProgress == nil) {
        return;
    }
    storedProgress.startTimes[rangeKey] = @(startTime);
}

- (NSArray<SCValdiTextViewAnimationRange *> *)_visibleAnimationRanges
{
    if (self.cachedVisibleAnimationRanges != nil) {
        return self.cachedVisibleAnimationRanges;
    }

    NSArray<SCValdiTextViewAnimationRange *> *animationRanges = [self _animationRanges];
    NSMutableArray<SCValdiTextViewAnimationRange *> *visibleAnimationRanges = [NSMutableArray new];
    for (SCValdiTextViewAnimationRange *animationRange in animationRanges) {
        if (SCValdiAnimationRangeHasVisibleTransform(animationRange)) {
            [visibleAnimationRanges addObject:animationRange];
        }
    }
    self.cachedVisibleAnimationRanges = visibleAnimationRanges;
    return self.cachedVisibleAnimationRanges;
}

- (NSArray<SCValdiTextViewOutline *> *)_outlineRanges
{
    if (self.cachedOutlineRanges != nil) {
        return self.cachedOutlineRanges;
    }

    NSArray<SCValdiTextViewAnimationRange *> *animationRanges = [self _visibleAnimationRanges];
    NSMutableArray<SCValdiTextViewOutline *> *outlineRanges = [NSMutableArray new];
    [self.processedText enumerateOuterOutlinesUsingBlock:^(UIColor *color, CGFloat width, NSRange range, BOOL *stop) {
        if (range.length == 0) {
            return;
        }

        for (NSValue *remainingRangeValue in SCValdiSubtractAnimationRanges(range, animationRanges)) {
            NSRange remainingRange = remainingRangeValue.rangeValue;
            if (remainingRange.length == 0) {
                continue;
            }

            SCValdiTextViewOutline *outline = [SCValdiTextViewOutline new];
            outline.range = remainingRange;
            outline.width = width;
            outline.color = color;
            [outlineRanges addObject:outline];
        }
    }];

    self.cachedOutlineRanges = outlineRanges;
    return self.cachedOutlineRanges;
}

- (NSArray<SCValdiTextViewOutline *> *)_outlineRangesInRange:(NSRange)range
{
    NSMutableArray<SCValdiTextViewOutline *> *outlineRanges = [NSMutableArray new];
    if (range.length == 0) {
        return outlineRanges;
    }

    [self.processedText enumerateOuterOutlinesUsingBlock:^(UIColor *color, CGFloat width, NSRange attributeRange, BOOL *stop) {
        NSRange intersectionRange = NSIntersectionRange(range, attributeRange);
        if (intersectionRange.length == 0) {
            return;
        }

        SCValdiTextViewOutline *outline = [SCValdiTextViewOutline new];
        outline.range = intersectionRange;
        outline.width = width;
        outline.color = color;
        [outlineRanges addObject:outline];
    }];

    return outlineRanges;
}

- (NSArray<SCValdiTextViewCustomUnderline *> *)_customUnderlineRangesForAttributedString:(NSAttributedString *)attributedString
{
    if (self.cachedCustomUnderlineRanges != nil) {
        return self.cachedCustomUnderlineRanges;
    }

    if (!self.customUnderlineStyle || attributedString.length == 0) {
        self.cachedCustomUnderlineRanges = @[];
        return self.cachedCustomUnderlineRanges;
    }

    NSMutableArray<SCValdiTextViewCustomUnderline *> *customUnderlineRanges = [NSMutableArray new];
    if (self.customUnderlineSourceAttributedString != nil && self.customUnderlineCharacterRanges.count > 0) {
        UIColor *fallbackColor = self.customUnderlineFallbackColor ?: UIColor.blackColor;
        for (NSValue *rangeValue in self.customUnderlineCharacterRanges) {
            NSRange range = rangeValue.rangeValue;
            if (range.length == 0) {
                continue;
            }

            UIColor *color = SCValdiCustomUnderlineColorForRange(self.customUnderlineSourceAttributedString,
                                                                 range,
                                                                 fallbackColor);
            [customUnderlineRanges addObject:[SCValdiTextViewCustomUnderline customUnderlineWithRange:range color:color]];
        }
        self.cachedCustomUnderlineRanges = customUnderlineRanges;
        return self.cachedCustomUnderlineRanges;
    }

    [attributedString enumerateAttribute:kSCValdiTextViewCustomUnderlineColorAttribute
                                 inRange:NSMakeRange(0, attributedString.length)
                                 options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                              usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (![value isKindOfClass:[UIColor class]] || range.length == 0) {
            return;
        }

        [customUnderlineRanges addObject:[SCValdiTextViewCustomUnderline customUnderlineWithRange:range color:value]];
    }];

    self.cachedCustomUnderlineRanges = customUnderlineRanges;
    return self.cachedCustomUnderlineRanges;
}

- (void)_drawStaticGlyphsForGlyphRange:(NSRange)glyphsToShow
                               atPoint:(CGPoint)origin
                       animationRanges:(NSArray<SCValdiTextViewAnimationRange *> *)animationRanges
{
    if (animationRanges.count == 0) {
        [super drawGlyphsForGlyphRange:glyphsToShow atPoint:origin];
        return;
    }

    NSUInteger currentGlyphLocation = glyphsToShow.location;
    NSUInteger glyphEnd = NSMaxRange(glyphsToShow);

    // _animationRanges builds ranges in document order, and this walk relies on
    // that monotonic ordering when advancing currentGlyphLocation.
    for (SCValdiTextViewAnimationRange *animationRange in animationRanges) {
        NSRange animationGlyphRange = [self glyphRangeForCharacterRange:animationRange.range actualCharacterRange:nil];
        NSRange intersectionGlyphRange = NSIntersectionRange(glyphsToShow, animationGlyphRange);
        if (intersectionGlyphRange.length == 0) {
            continue;
        }

        if (intersectionGlyphRange.location > currentGlyphLocation) {
            NSRange staticGlyphRange = NSMakeRange(currentGlyphLocation, intersectionGlyphRange.location - currentGlyphLocation);
            [super drawGlyphsForGlyphRange:staticGlyphRange atPoint:origin];
        }

        currentGlyphLocation = MAX(currentGlyphLocation, NSMaxRange(intersectionGlyphRange));
        if (currentGlyphLocation >= glyphEnd) {
            return;
        }
    }

    if (currentGlyphLocation < glyphEnd) {
        [super drawGlyphsForGlyphRange:NSMakeRange(currentGlyphLocation, glyphEnd - currentGlyphLocation) atPoint:origin];
    }
}

- (void)_drawCustomUnderlineRange:(NSRange)range
                            color:(UIColor *)color
                     glyphsToShow:(NSRange)glyphsToShow
                     glyphsOrigin:(CGPoint)origin
                          context:(CGContextRef)context
{
    NSArray<NSValue *> *underlineRects = SCValdiCustomUnderlineRectsForRange(self.textStorage,
                                                                             self,
                                                                             range,
                                                                             glyphsToShow,
                                                                             YES,
                                                                             origin,
                                                                             self.customUnderlineStyle.height,
                                                                             self.customUnderlineStyle.offset);
    [color setStroke];
    SCValdiCustomUnderlineDrawRects(context, underlineRects);
}

- (void)_drawStaticCustomUnderline:(SCValdiTextViewCustomUnderline *)customUnderline
                   animationRanges:(NSArray<SCValdiTextViewAnimationRange *> *)animationRanges
                      glyphsToShow:(NSRange)glyphsToShow
                      glyphsOrigin:(CGPoint)origin
                           context:(CGContextRef)context
{
    if (animationRanges.count == 0) {
        [self _drawCustomUnderlineRange:customUnderline.range
                                  color:customUnderline.color
                           glyphsToShow:glyphsToShow
                           glyphsOrigin:origin
                                context:context];
        return;
    }

    NSRange underlineRange = customUnderline.range;
    NSUInteger currentLocation = underlineRange.location;
    NSUInteger rangeEnd = NSMaxRange(underlineRange);
    for (SCValdiTextViewAnimationRange *animationRange in animationRanges) {
        NSRange intersectionRange = NSIntersectionRange(underlineRange, animationRange.range);
        if (intersectionRange.length == 0) {
            continue;
        }
        if (intersectionRange.location > currentLocation) {
            [self _drawCustomUnderlineRange:NSMakeRange(currentLocation, intersectionRange.location - currentLocation)
                                      color:customUnderline.color
                               glyphsToShow:glyphsToShow
                               glyphsOrigin:origin
                                    context:context];
        }
        currentLocation = MAX(currentLocation, NSMaxRange(intersectionRange));
        if (currentLocation >= rangeEnd) {
            return;
        }
    }

    if (currentLocation < rangeEnd) {
        [self _drawCustomUnderlineRange:NSMakeRange(currentLocation, rangeEnd - currentLocation)
                                  color:customUnderline.color
                           glyphsToShow:glyphsToShow
                           glyphsOrigin:origin
                                context:context];
    }
}

- (void)_drawStaticCustomUnderlines:(NSArray<SCValdiTextViewCustomUnderline *> *)customUnderlineRanges
                    animationRanges:(NSArray<SCValdiTextViewAnimationRange *> *)animationRanges
                       glyphsToShow:(NSRange)glyphsToShow
                       glyphsOrigin:(CGPoint)origin
                            context:(CGContextRef)context
{
    if (!self.customUnderlineStyle || customUnderlineRanges.count == 0 || glyphsToShow.length == 0) {
        return;
    }

    CGContextSaveGState(context);
    CGContextSetLineWidth(context, self.customUnderlineStyle.height);
    SCValdiCustomUnderlineApplyDashPattern(context, self.customUnderlineStyle);

    for (SCValdiTextViewCustomUnderline *customUnderline in customUnderlineRanges) {
        [self _drawStaticCustomUnderline:customUnderline
                         animationRanges:animationRanges
                            glyphsToShow:glyphsToShow
                            glyphsOrigin:origin
                                 context:context];
    }

    CGContextRestoreGState(context);
}

- (void)_drawCustomUnderlinesInRange:(NSRange)range
                        glyphsToShow:(NSRange)glyphsToShow
                        glyphsOrigin:(CGPoint)origin
                             context:(CGContextRef)context
{
    if (!self.customUnderlineStyle || range.length == 0 || glyphsToShow.length == 0) {
        return;
    }

    CGContextSaveGState(context);
    CGContextSetLineWidth(context, self.customUnderlineStyle.height);
    SCValdiCustomUnderlineApplyDashPattern(context, self.customUnderlineStyle);

    for (SCValdiTextViewCustomUnderline *customUnderline in [self _customUnderlineRangesForAttributedString:self.textStorage]) {
        NSRange intersectionRange = NSIntersectionRange(range, customUnderline.range);
        if (intersectionRange.length == 0) {
            continue;
        }
        [self _drawCustomUnderlineRange:intersectionRange
                                  color:customUnderline.color
                           glyphsToShow:glyphsToShow
                           glyphsOrigin:origin
                                context:context];
    }

    CGContextRestoreGState(context);
}

- (void)_drawAnimatedRange:(SCValdiTextViewAnimationRange *)animationRange
               glyphsOrigin:(CGPoint)origin
                    context:(CGContextRef)context
{
    if (animationRange.range.length == 0 || animationRange.opacity <= 0) {
        return;
    }

    NSUInteger charIndex = animationRange.range.location;
    NSUInteger charRangeEnd = NSMaxRange(animationRange.range);

    while (charIndex < charRangeEnd) {
        NSUInteger glyphIndex = [self glyphIndexForCharacterAtIndex:charIndex];
        NSRange lineGlyphRange;
        [self lineFragmentRectForGlyphAtIndex:glyphIndex effectiveRange:&lineGlyphRange];
        NSRange lineCharRange = [self characterRangeForGlyphRange:lineGlyphRange actualGlyphRange:nil];
        NSRange intersectionRange = NSIntersectionRange(animationRange.range, lineCharRange);
        if (intersectionRange.length == 0) {
            charIndex = NSMaxRange(lineCharRange);
            continue;
        }

        NSRange intersectionGlyphRange = [self glyphRangeForCharacterRange:intersectionRange actualCharacterRange:nil];
        NSTextContainer *textContainer = [self textContainerForGlyphAtIndex:intersectionGlyphRange.location effectiveRange:nil];
        CGRect boundingRect = [self boundingRectForGlyphRange:intersectionGlyphRange inTextContainer:textContainer];
        if (!CGRectIsEmpty(boundingRect)) {
            CGPoint drawCenter = CGPointMake(origin.x + CGRectGetMidX(boundingRect), origin.y + CGRectGetMidY(boundingRect));

            CGContextSaveGState(context);
            CGContextSetAlpha(context, animationRange.opacity);
            CGContextTranslateCTM(context, drawCenter.x, drawCenter.y + animationRange.translationY);
            CGContextScaleCTM(context, animationRange.scale, animationRange.scale);
            CGContextTranslateCTM(context, -drawCenter.x, -drawCenter.y);
            for (SCValdiTextViewOutline *outline in [self _outlineRangesInRange:intersectionRange]) {
                [self _drawOutline:outline attributedString:self.textStorage glyphsOrigin:origin context:context];
            }
            [super drawGlyphsForGlyphRange:intersectionGlyphRange atPoint:origin];
            [self _drawCustomUnderlinesInRange:intersectionRange
                                  glyphsToShow:intersectionGlyphRange
                                  glyphsOrigin:origin
                                       context:context];
            CGContextRestoreGState(context);
        }

        charIndex = NSMaxRange(intersectionRange);
    }
}


#pragma mark - Background Drawing

- (void)drawBackgroundForGlyphRange:(NSRange)glyphsToShow atPoint:(CGPoint)origin
{
    [super drawBackgroundForGlyphRange:glyphsToShow atPoint:origin];

    if (self.backgroundColor == [UIColor clearColor]) {
        // Don't draw any background if the color is clear
        return;
    }

    // Always render the whole glyph range to ensure there is no invalid cache for backgrounds that are not included in 'glyphsToShow'
    NSRange glyphRange = [self glyphRangeForCharacterRange:NSMakeRange(0, self.textStorage.length) actualCharacterRange:nil];

    NSMutableArray<NSValue *> *lineRects = [NSMutableArray new];
    [self enumerateLineFragmentsForGlyphRange:glyphRange
                                   usingBlock:^(CGRect rect, CGRect usedRect, NSTextContainer *_Nonnull textContainer,
                                                NSRange glyphRange, BOOL *_Nonnull stop) {
                                        NSRange lineFragmentCharacterRange = [self characterRangeForGlyphRange:glyphRange actualGlyphRange:nil];
                                        NSString *lineFragmentString = [self.textStorage.string substringWithRange:lineFragmentCharacterRange];
                                        NSString *trimmedLineFragmentString = [lineFragmentString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                                        if (trimmedLineFragmentString.length == 0) {
                                            // Don't include empty line fragments
                                            return;
                                        }
                                        NSRange trimmedLineFragmentRange = [lineFragmentString rangeOfString:trimmedLineFragmentString];
                                        NSUInteger whitespaceStartLocation = trimmedLineFragmentRange.location + trimmedLineFragmentRange.length;
                                        if (whitespaceStartLocation < lineFragmentString.length) {
                                            // Remove trailing whitespace from line fragment width
                                            BOOL hasNewLine = [lineFragmentString rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location != NSNotFound;
                                            NSUInteger whitespaceNewlineOffset = hasNewLine ? 1 : 0;
                                            NSUInteger whitespaceLocation = glyphRange.location + whitespaceStartLocation;
                                            NSRange whitespaceRange = NSMakeRange(whitespaceLocation, glyphRange.location + glyphRange.length - (whitespaceLocation) - whitespaceNewlineOffset);
                                            CGRect whitespaceBoundingRect = [self boundingRectForGlyphRange:whitespaceRange inTextContainer:textContainer];
                                            usedRect.size.width -= whitespaceBoundingRect.size.width;
                                        }

                                       CGRect paddedRect = [self _addVerticalPaddingTo:usedRect
                                                                               padding:self.backgroundPadding / 2.0];
                                       [lineRects addObject:[NSValue valueWithCGRect:paddedRect]];
                                   }];
    [self _processLineRects:lineRects];

    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, origin.x, origin.y);
    [self _drawLineRects:[lineRects copy]];
    CGContextRestoreGState(context);
}

- (CGRect)_addVerticalPaddingTo:(CGRect)rect padding:(CGFloat)padding
{
    return CGRectMake(rect.origin.x, rect.origin.y - padding, rect.size.width, rect.size.height + padding * 2);
}

- (void)_processLineRects:(NSMutableArray<NSValue *> *)lineRects
{
    if (lineRects.count < 2) {
        return;
    }
    NSInteger maxIndex = 0;
    for (NSUInteger i = 1; i < lineRects.count; i++) {
        maxIndex = i;
        [self _processLineRectAtIndex:i maxIndex:maxIndex lineRects:lineRects];
    }
}

- (void)_processLineRectAtIndex:(NSInteger)rectIndex
                       maxIndex:(NSInteger)maxIndex
                      lineRects:(NSMutableArray<NSValue *> *)lineRects
{
    if (lineRects.count < 2 || rectIndex < 1 || rectIndex > maxIndex) {
        return;
    }

    CGRect currentRect = [lineRects objectAtIndex:rectIndex].CGRectValue;
    CGRect previousRect = [lineRects objectAtIndex:rectIndex - 1].CGRectValue;

    BOOL matchPrevious = ((currentRect.origin.x - previousRect.origin.x < 2 * self.backgroundBorderRadius) &&
                          (currentRect.origin.x > previousRect.origin.x)) ||
                         ((CGRectGetMaxX(currentRect) - CGRectGetMaxX(previousRect) > -2 * self.backgroundBorderRadius) &&
                          (CGRectGetMaxX(currentRect) < CGRectGetMaxX(previousRect)));
    BOOL matchCurrent = ((previousRect.origin.x - currentRect.origin.x < 2 * self.backgroundBorderRadius) &&
                         (previousRect.origin.x > currentRect.origin.x)) ||
                        ((CGRectGetMaxX(previousRect) - CGRectGetMaxX(currentRect) > -2 * self.backgroundBorderRadius) &&
                         (CGRectGetMaxX(previousRect) < CGRectGetMaxX(currentRect)));

    if (matchCurrent) {
        // Update the previous rect to match the size of current
        CGRect newPreviousRect =
            CGRectMake(currentRect.origin.x, previousRect.origin.y, currentRect.size.width, previousRect.size.height);
        [lineRects replaceObjectAtIndex:rectIndex - 1 withObject:[NSValue valueWithCGRect:newPreviousRect]];
        // Update rect before if needed
        [self _processLineRectAtIndex:rectIndex - 1 maxIndex:maxIndex lineRects:lineRects];
    } else if (matchPrevious) {
        // Update currect rect to match the size of the previous
        CGRect newCurrentRect =
            CGRectMake(previousRect.origin.x, currentRect.origin.y, previousRect.size.width, currentRect.size.height);
        [lineRects replaceObjectAtIndex:rectIndex withObject:[NSValue valueWithCGRect:newCurrentRect]];
        // Update rect after if needed
        [self _processLineRectAtIndex:rectIndex + 1 maxIndex:maxIndex lineRects:lineRects];
    }
}

/// Must be called from Core Graphics
- (void)_drawLineRects:(NSArray<NSValue *> *)lineRects
{
    UIBezierPath *path = [UIBezierPath new];

    // start by drawing path in the top left down to the bottom left
    for (NSUInteger i = 0; i < lineRects.count; i++) {
        CGRect currentRect = [lineRects objectAtIndex:i].CGRectValue;

        if (i == 0) {
            // start -- get top left to bottom
            [path moveToPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMinY(currentRect) + self.backgroundBorderRadius)];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMinX(currentRect) + self.backgroundBorderRadius, CGRectGetMinY(currentRect))
                       controlPoint:currentRect.origin];
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(currentRect) - self.backgroundBorderRadius, CGRectGetMinY(currentRect))];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMinY(currentRect) + self.backgroundBorderRadius)
                       controlPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMinY(currentRect))];
        }

        NSUInteger nextIndex = i + 1;
        if (nextIndex >= lineRects.count) {
            continue;
        }

        // Draw the right side to the bottom right, and if needed, curve, bottom line, and curve to the next line
        CGRect nextRect = lineRects[nextIndex].CGRectValue;
        CGFloat currentRectMaxX = CGRectGetMaxX(currentRect);
        CGFloat nextRectMaxX = CGRectGetMaxX(nextRect);
        CGFloat rectMaxXDiff = currentRectMaxX - nextRectMaxX;
        if (rectMaxXDiff > 0) {
            // Next line shorter
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMaxY(currentRect) - self.backgroundBorderRadius)];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(currentRect) - self.backgroundBorderRadius, CGRectGetMaxY(currentRect))
                       controlPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMaxY(currentRect))];
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(nextRect) + self.backgroundBorderRadius, CGRectGetMaxY(currentRect))];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(nextRect), CGRectGetMaxY(currentRect) + self.backgroundBorderRadius)
                         controlPoint:CGPointMake(CGRectGetMaxX(nextRect), CGRectGetMaxY(currentRect))];
        } else if (rectMaxXDiff < 0) {
            // Next line longer
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMinY(nextRect) - self.backgroundBorderRadius)];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(currentRect) + self.backgroundBorderRadius, CGRectGetMinY(nextRect))
                         controlPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMinY(nextRect))];
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(nextRect) - self.backgroundBorderRadius, CGRectGetMinY(nextRect))];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(nextRect), CGRectGetMinY(nextRect) + self.backgroundBorderRadius)
                         controlPoint:CGPointMake(CGRectGetMaxX(nextRect), CGRectGetMinY(nextRect))];
        } else {
            // Next line same width
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(nextRect), CGRectGetMinY(nextRect) + self.backgroundBorderRadius)];
        }
    }

    // Iterate reverse, go back up to the top left and complete the loop
    for (NSInteger i = lineRects.count - 1; i >= 0; i--) {
        CGRect currentRect = lineRects[i].CGRectValue;

        // Bottom line right line, bottom right corner, bottom line, and bottom left corner
        if (i == (NSInteger)lineRects.count - 1) {
            [path addLineToPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMaxY(currentRect) - self.backgroundBorderRadius)];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMaxX(currentRect) - self.backgroundBorderRadius, CGRectGetMaxY(currentRect))
                       controlPoint:CGPointMake(CGRectGetMaxX(currentRect), CGRectGetMaxY(currentRect))];
            [path addLineToPoint:CGPointMake(CGRectGetMinX(currentRect) + self.backgroundBorderRadius, CGRectGetMaxY(currentRect))];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMaxY(currentRect) - self.backgroundBorderRadius)
                       controlPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMaxY(currentRect))];
        }

        NSInteger nextIndex = i - 1;
        if (nextIndex < 0) {
            continue;
        }

        // Each line drawing starts right after the bottom left corner was drawn for the previous line
        // This is so the top of the current line can be adjusted based on if the next line is shorter or wider
        // If the next line is shorter, use the top of the current line
        // If the next line is wider, use the bottom of the next line
        CGRect nextRect = lineRects[nextIndex].CGRectValue;
        CGFloat currentRectMinX = CGRectGetMinX(currentRect);
        CGFloat nextRectMinX = CGRectGetMinX(nextRect);
        CGFloat rectMinXDiff = currentRectMinX - nextRectMinX;
        if (rectMinXDiff < 0) {
            // Next line shorter
            [path addLineToPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMinY(currentRect) + self.backgroundBorderRadius)];
            [path
                addQuadCurveToPoint:CGPointMake(CGRectGetMinX(currentRect) + self.backgroundBorderRadius, CGRectGetMinY(currentRect))
                       controlPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMinY(currentRect))];
            [path addLineToPoint:CGPointMake(CGRectGetMinX(nextRect) - self.backgroundBorderRadius, CGRectGetMinY(currentRect))];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMinX(nextRect), CGRectGetMinY(currentRect) - self.backgroundBorderRadius)
                         controlPoint:CGPointMake(CGRectGetMinX(nextRect), CGRectGetMinY(currentRect))];
        } else if (rectMinXDiff > 0) {
            // Next line wider
            [path addLineToPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMaxY(nextRect) + self.backgroundBorderRadius)];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMinX(currentRect) - self.backgroundBorderRadius, CGRectGetMaxY(nextRect))
                         controlPoint:CGPointMake(CGRectGetMinX(currentRect), CGRectGetMaxY(nextRect))];
            [path addLineToPoint:CGPointMake(CGRectGetMinX(nextRect) + self.backgroundBorderRadius, CGRectGetMaxY(nextRect))];
            [path addQuadCurveToPoint:CGPointMake(CGRectGetMinX(nextRect), CGRectGetMaxY(nextRect) - self.backgroundBorderRadius)
                         controlPoint:CGPointMake(CGRectGetMinX(nextRect), CGRectGetMaxY(nextRect))];
        } else {
            // Next line same width
            [path addLineToPoint:CGPointMake(CGRectGetMinX(nextRect), CGRectGetMaxY(nextRect) - self.backgroundBorderRadius)];
        }
    }

    [path closePath];
    [self.backgroundColor setFill];
    [path fill];
}

@end
