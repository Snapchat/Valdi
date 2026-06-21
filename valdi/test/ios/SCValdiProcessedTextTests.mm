#import <XCTest/XCTest.h>

#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiInlineTextChildLayout.h"
#import "valdi/ios/Text/SCValdiInlineViewAttachmentInfo.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"
#import "valdi/ios/Text/SCValdiTextAnimationPresentation.h"
#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"
#import "valdi/ios/Views/SCValdiLabel.h"
#import "valdi/ios/Views/SCValdiTextView.h"
#import "valdi/ios/Views/SCValdiTextViewEffectsLayoutManager.h"
#import "valdi_core/SCValdiContentViewProviding.h"
#import "valdi_core/SCValdiFontManagerProtocol.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#import "valdi_core/cpp/Attributes/TextInlineAttachment.hpp"
#import "valdi_core/cpp/Utils/Shared.hpp"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <vector>

namespace {

class SCValdiTestInlineAttachment final : public Valdi::TextInlineAttachment {
public:
    SCValdiTestInlineAttachment(size_t childIndex, CGSize *size) : TextInlineAttachment(childIndex), _size(size) {}

    Valdi::Size getSize() const override {
        return Valdi::Size(static_cast<float>(_size->width), static_cast<float>(_size->height));
    }

private:
    CGSize *_size;
};

static Valdi::TextAttributeValueStyle &SCValdiTestAppendTextPart(Valdi::TextAttributeValue::Parts &parts,
                                                                 const Valdi::StringBox &content)
{
    auto &part = parts.emplace_back();
    part.content = content;
    return part.style;
}

static void SCValdiTestAppendInlineAttachmentPart(
    Valdi::TextAttributeValue::Parts &parts,
    const Valdi::Ref<Valdi::TextInlineAttachment> &attachment)
{
    auto &part = parts.emplace_back();
    part.style.inlineViewAttachment = attachment;
}

static SCValdiProcessedText *SCValdiProcessedTextWithParts(
    Valdi::TextAttributeValue::Parts parts,
    NSDictionary<NSAttributedStringKey, id> *attributes,
    SCValdiProcessedTextConfiguration *configuration)
{
    auto textAttributeValue = Valdi::makeShared<Valdi::TextAttributeValue>(std::move(parts));
    SCValdiAttributedText *valdiAttributedText =
        [[SCValdiAttributedText alloc] initWithCppInstance:Valdi::unsafeBridgeCast(textAttributeValue.get())];
    return [SCValdiProcessedText processedTextWithAttributeValue:valdiAttributedText
                                                     attributes:attributes
                                                  isRightToLeft:NO
                                                    fontManager:nil
                                                traitCollection:nil
                                                  configuration:configuration];
}

static SCValdiProcessedText *SCValdiProcessedTextWithInlineAttachments(
    const std::vector<Valdi::Ref<Valdi::TextInlineAttachment>> &attachments,
    UIFont *font)
{
    Valdi::TextAttributeValue::Parts parts;
    for (const auto &attachment : attachments) {
        SCValdiTestAppendTextPart(parts, STRING_LITERAL("x"));
        SCValdiTestAppendInlineAttachmentPart(parts, attachment);
    }
    SCValdiTestAppendTextPart(parts, STRING_LITERAL("end"));

    return SCValdiProcessedTextWithParts(std::move(parts), @{ NSFontAttributeName: font }, nil);
}

static Valdi::Ref<Valdi::ValueFunction> SCValdiTestNoopFunction()
{
    return Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
        [](const Valdi::ValueFunctionCallContext & /*callContext*/) -> Valdi::Value {
            return Valdi::Value::undefined();
        });
}

static void SCValdiAssertColorEqual(UIColor *actualColor, UIColor *expectedColor)
{
    XCTAssertNotNil(actualColor);
    XCTAssertEqual(actualColor.valdiAttributeValue, expectedColor.valdiAttributeValue);
}

static NSTextAttachment *SCValdiTextAttachmentAtRange(SCValdiProcessedText *processedText, NSRange range)
{
    id attachment = [processedText.attributedString attribute:NSAttachmentAttributeName
                                                     atIndex:range.location
                                              effectiveRange:nil];
    return [attachment isKindOfClass:[NSTextAttachment class]] ? attachment : nil;
}

static NSTextStorage *SCValdiConfigureLayoutManagerForProcessedText(SCValdiProcessedText *processedText,
                                                                    NSLayoutManager *layoutManager,
                                                                    NSTextContainer *textContainer)
{
    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:processedText.attributedString];
    [textStorage addLayoutManager:layoutManager];
    textContainer.lineFragmentPadding = 0;
    [layoutManager addTextContainer:textContainer];
    [layoutManager ensureLayoutForTextContainer:textContainer];
    return textStorage;
}

} // namespace

@interface SCValdiProcessedTextTests : XCTestCase

@end

@interface SCValdiProcessedTextTestFontManager : NSObject <SCValdiFontManagerProtocol>
@property (nonatomic, copy) NSString *requestedFontName;
@property (nonatomic, assign) CGFloat requestedFontSize;
@end

@implementation SCValdiProcessedTextTestFontManager

- (UIFont *)fontWithName:(NSString *)fontName
                fontSize:(CGFloat)fontSize
        legibilityWeight:(SCUILegibilityWeight)legibilityWeight
{
    (void)legibilityWeight;
    self.requestedFontName = fontName;
    self.requestedFontSize = fontSize;
    return [UIFont systemFontOfSize:fontSize];
}

- (BOOL)shouldBypassContextForLegibilityWeight
{
    return NO;
}

- (UITraitCollection *)defaultTraitCollection
{
    return [UITraitCollection currentTraitCollection];
}

@end

@interface SCValdiProcessedTextTestViewNode : NSObject <SCValdiViewNodeProtocol>

@property (nonatomic, strong, readonly) NSMutableDictionary<NSString *, id> *attributeValues;
@property (nonatomic, strong, readonly) NSMutableArray<NSString *> *removedAttributeNames;

- (instancetype)initWithView:(UIView *)view;

@end

@implementation SCValdiProcessedTextTestViewNode {
    UIView *_view;
}

- (instancetype)initWithView:(UIView *)view
{
    self = [super init];
    if (self) {
        _view = view;
        _attributeValues = [NSMutableDictionary new];
        _removedAttributeNames = [NSMutableArray new];
    }
    return self;
}

- (UIView *)view
{
    return _view;
}

- (CGRect)relativeFrame
{
    return CGRectZero;
}

- (BOOL)isLayoutDirectionHorizontal
{
    return NO;
}

- (BOOL)isRightToLeft
{
    return NO;
}

- (void)markLayoutDirty
{
}

- (void)setRetainedObject:(id)object forKey:(NSString *)key
{
    (void)object;
    (void)key;
}

- (void)didApplyLayoutWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    (void)animator;
}

- (void)setValue:(id)value forValdiAttribute:(NSString *)attributeName
{
    if (value == nil) {
        [self.attributeValues removeObjectForKey:attributeName];
        [self.removedAttributeNames addObject:attributeName];
        return;
    }
    self.attributeValues[attributeName] = value ?: [NSNull null];
}

- (id)valueForValdiAttribute:(NSString *)attributeName
{
    return self.attributeValues[attributeName];
}

- (void)removeValueForValdiAttribute:(NSString *)attributeName
{
    [self setValue:nil forValdiAttribute:attributeName];
}

- (id)preprocessedValueForValdiAttribute:(NSString *)attributeName
{
    return [self valueForValdiAttribute:attributeName];
}

- (void)setDidFinishLayoutBlock:(SCValdiContextDidFinishLayoutBlock)block forKey:(NSString *)key
{
    (void)block;
    (void)key;
}

- (BOOL)hasDidFinishLayoutBlockForKey:(NSString *)key
{
    (void)key;
    return NO;
}

- (NSArray<id<SCValdiViewNodeProtocol>> *)children
{
    return @[];
}

- (CGPoint)relativeDirectionAgnosticPointFromPoint:(CGPoint)point
{
    return point;
}

- (CGPoint)absoluteDirectionAgnosticPointFromPoint:(CGPoint)point
{
    return point;
}

- (CGFloat)resolveDeltaX:(CGFloat)deltaX directionAgnostic:(BOOL)directionAgnostic
{
    (void)directionAgnostic;
    return deltaX;
}

- (void)notifyOnScrollWithContentOffset:(CGPoint)contentOffset
                   updatedContentOffset:(inout CGPoint *)updatedContentOffset
                               velocity:(CGPoint)velocity
{
    (void)velocity;
    if (updatedContentOffset != NULL) {
        *updatedContentOffset = contentOffset;
    }
}

- (void)notifyOnScrollEndWithContentOffset:(CGPoint)contentOffset
{
    (void)contentOffset;
}

- (void)notifyOnDragStartWithContentOffset:(CGPoint)contentOffset velocity:(CGPoint)velocity
{
    (void)contentOffset;
    (void)velocity;
}

- (void)notifyOnDragEndingWithContentOffset:(CGPoint)contentOffset
                                   velocity:(CGPoint)velocity
                       updatedContentOffset:(inout CGPoint *)updatedContentOffset
{
    (void)velocity;
    if (updatedContentOffset != NULL) {
        *updatedContentOffset = contentOffset;
    }
}

- (BOOL)canScrollAtPoint:(CGPoint)point direction:(SCValdiScrollDirection)direction
{
    (void)point;
    (void)direction;
    return NO;
}

@end

@implementation SCValdiProcessedTextTests

- (void)testFactoryHandlesNilStringAndAttributedStringInputs
{
    SCValdiProcessedText *nilText =
        [SCValdiProcessedText processedTextWithAttributeValue:nil
                                                   attributes:nil
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:nil];
    XCTAssertEqualObjects(nilText.attributedString.string, @"");
    XCTAssertFalse(nilText.hasOnTap);
    XCTAssertFalse(nilText.hasOnLayout);
    XCTAssertFalse(nilText.hasInlineViewAttachment);
    XCTAssertFalse(nilText.hasAnimationTransform);
    XCTAssertFalse(nilText.hasOuterOutline);
    XCTAssertFalse(nilText.hasCustomUnderline);
    XCTAssertEqual(nilText.animationTransformsCount, 0U);
    XCTAssertNil(nilText.customUnderlineSourceString);
    XCTAssertNil(nilText.customUnderlineCharacterRanges);
    XCTAssertFalse([nilText updateInlineAttachments]);

    NSDictionary<NSAttributedStringKey, id> *attributes = @{
        NSFontAttributeName: [UIFont systemFontOfSize:17],
        NSForegroundColorAttributeName: UIColor.redColor,
    };
    SCValdiProcessedText *stringText =
        [SCValdiProcessedText processedTextWithAttributeValue:@"hello"
                                                   attributes:attributes
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:nil];
    XCTAssertEqualObjects(stringText.attributedString.string, @"hello");
    XCTAssertEqualObjects([stringText.attributedString attribute:NSForegroundColorAttributeName
                                                        atIndex:0
                                                 effectiveRange:nil],
                          UIColor.redColor);

    NSMutableAttributedString *attributedString =
        [[NSMutableAttributedString alloc] initWithString:@"existing"
                                               attributes:@{ NSForegroundColorAttributeName: UIColor.blueColor }];
    SCValdiProcessedText *processedAttributedString =
        [SCValdiProcessedText processedTextWithAttributeValue:attributedString
                                                   attributes:@{ NSForegroundColorAttributeName: UIColor.redColor }
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:nil];
    XCTAssertEqualObjects(processedAttributedString.attributedString.string, @"existing");
    XCTAssertEqualObjects([processedAttributedString.attributedString attribute:NSForegroundColorAttributeName
                                                                       atIndex:0
                                                                effectiveRange:nil],
                          UIColor.blueColor);

    [attributedString replaceCharactersInRange:NSMakeRange(0, attributedString.length) withString:@"mutated"];
    XCTAssertEqualObjects(processedAttributedString.attributedString.string, @"existing");

    SCValdiProcessedText *unknownText =
        [SCValdiProcessedText processedTextWithAttributeValue:@42
                                                   attributes:attributes
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:nil];
    XCTAssertEqualObjects(unknownText.attributedString.string, @"");
    XCTAssertFalse(unknownText.hasOnTap);
    XCTAssertFalse(unknownText.hasInlineViewAttachment);
}

- (void)testAttributedTextFontStringOverridesBaseFont
{
    SCValdiProcessedTextTestFontManager *fontManager = [SCValdiProcessedTextTestFontManager new];
    UIFont *baseFont = [UIFont systemFontOfSize:11];

    Valdi::TextAttributeValue::Parts parts;
    auto &fontStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("font"));
    fontStyle.font = STRING_LITERAL("Helvetica 24 unscaled");

    auto textAttributeValue = Valdi::makeShared<Valdi::TextAttributeValue>(std::move(parts));
    SCValdiAttributedText *valdiAttributedText =
        [[SCValdiAttributedText alloc] initWithCppInstance:Valdi::unsafeBridgeCast(textAttributeValue.get())];
    SCValdiProcessedText *processedText =
        [SCValdiProcessedText processedTextWithAttributeValue:valdiAttributedText
                                                   attributes:@{ NSFontAttributeName: baseFont }
                                                isRightToLeft:NO
                                                  fontManager:fontManager
                                              traitCollection:nil
                                                configuration:nil];

    UIFont *resolvedFont = [processedText.attributedString attribute:NSFontAttributeName
                                                            atIndex:0
                                                     effectiveRange:nil];
    XCTAssertEqualWithAccuracy(resolvedFont.pointSize, 24, 0.001);
    XCTAssertEqualObjects(fontManager.requestedFontName, @"Helvetica");
    XCTAssertEqualWithAccuracy(fontManager.requestedFontSize, 24, 0.001);
    XCTAssertNotEqualObjects(resolvedFont, baseFont);
}

- (void)testAttributedTextAppliesLineHeightAttributes
{
    UIFont *font = [UIFont systemFontOfSize:20];
    CGFloat lineHeight = font.lineHeight + 8;

    Valdi::TextAttributeValue::Parts parts;
    SCValdiTestAppendTextPart(parts, STRING_LITERAL("height"));

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts),
                                      @{
                                          NSFontAttributeName: font,
                                          SCValdiLineHeightAttributeName: @(lineHeight),
                                      },
                                      nil);

    NSParagraphStyle *paragraphStyle = [processedText.attributedString attribute:NSParagraphStyleAttributeName
                                                                        atIndex:0
                                                                 effectiveRange:nil];
    XCTAssertEqualWithAccuracy(paragraphStyle.minimumLineHeight, lineHeight, 0.001);
    XCTAssertEqualWithAccuracy(paragraphStyle.maximumLineHeight, lineHeight, 0.001);
    XCTAssertEqualWithAccuracy([[processedText.attributedString attribute:NSBaselineOffsetAttributeName
                                                                   atIndex:0
                                                            effectiveRange:nil] doubleValue],
                               (lineHeight - font.lineHeight) / 2.0,
                               0.001);
}

- (void)testConfigurationCanOverrideForegroundAndRemoveNativeUnderlines
{
    NSMutableAttributedString *attributedString =
        [[NSMutableAttributedString alloc] initWithString:@"underlined"
                                               attributes:@{
                                                   NSFontAttributeName: [UIFont systemFontOfSize:20],
                                                   NSForegroundColorAttributeName: UIColor.redColor,
                                                   NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle),
                                               }];

    SCValdiProcessedTextConfiguration *configuration = [SCValdiProcessedTextConfiguration new];
    configuration.foregroundColorOverride = UIColor.greenColor;
    configuration.customUnderlineStyle = [[SCValdiCustomUnderlineStyle alloc] initWithHeight:1
                                                                                     onWidth:0
                                                                                    offWidth:0
                                                                                      offset:0];
    configuration.customUnderlineMode = SCValdiProcessedTextCustomUnderlineModeRemoveNativeUnderline;

    SCValdiProcessedText *processedText =
        [SCValdiProcessedText processedTextWithAttributeValue:attributedString
                                                   attributes:nil
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:configuration];

    XCTAssertTrue(processedText.hasCustomUnderline);
    XCTAssertEqual(processedText.customUnderlineSourceString, processedText.attributedString);
    XCTAssertEqual(processedText.customUnderlineCharacterRanges.count, 1U);
    XCTAssertNil([processedText.attributedString attribute:NSUnderlineStyleAttributeName atIndex:0 effectiveRange:nil]);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSForegroundColorAttributeName
                                                           atIndex:0
                                                    effectiveRange:nil],
                          UIColor.greenColor);
}

- (void)testAttributedTextStyleTranslationCoversColorsOutlinesAndDecorations
{
    UIFont *font = [UIFont systemFontOfSize:18];
    NSDictionary<NSAttributedStringKey, id> *baseAttributes = @{
        NSFontAttributeName: font,
        NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle),
        NSStrikethroughStyleAttributeName: @(NSUnderlineStyleSingle),
    };

    Valdi::TextAttributeValue::Parts parts;
    auto &styled = SCValdiTestAppendTextPart(parts, STRING_LITERAL("u"));
    styled.color = Valdi::Color(1, 2, 3, 255);
    auto background = Valdi::makeShared<Valdi::TextBackgroundAttributeStyle>();
    background->color = Valdi::Color(4, 5, 6, 255);
    styled.background = background;
    styled.outlineColor = Valdi::Color(7, 8, 9, 255);
    styled.outlineWidth = 3;

    auto &none = SCValdiTestAppendTextPart(parts, STRING_LITERAL("n"));
    none.textDecoration = Valdi::TextDecoration::None;

    auto &underline = SCValdiTestAppendTextPart(parts, STRING_LITERAL("l"));
    underline.textDecoration = Valdi::TextDecoration::Underline;

    auto &strikethrough = SCValdiTestAppendTextPart(parts, STRING_LITERAL("s"));
    strikethrough.textDecoration = Valdi::TextDecoration::Strikethrough;

    auto &dashed = SCValdiTestAppendTextPart(parts, STRING_LITERAL("d"));
    dashed.textDecoration = Valdi::TextDecoration::DashedUnderline;

    auto &dotted = SCValdiTestAppendTextPart(parts, STRING_LITERAL("o"));
    dotted.textDecoration = Valdi::TextDecoration::DottedUnderline;

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts), baseAttributes, nil);
    NSAttributedString *attributedString = processedText.attributedString;
    XCTAssertEqualObjects(attributedString.string, @"unlsdo");

    SCValdiAssertColorEqual([attributedString attribute:NSForegroundColorAttributeName
                                                atIndex:0
                                         effectiveRange:nil],
                            UIColorFromValdiAttributeValue(0x010203FF));
    SCValdiAssertColorEqual([attributedString attribute:NSBackgroundColorAttributeName
                                                atIndex:0
                                         effectiveRange:nil],
                            UIColorFromValdiAttributeValue(0x040506FF));
    SCValdiAssertColorEqual([attributedString attribute:NSStrokeColorAttributeName
                                                atIndex:0
                                         effectiveRange:nil],
                            UIColorFromValdiAttributeValue(0x070809FF));
    XCTAssertEqualWithAccuracy([[attributedString attribute:NSStrokeWidthAttributeName
                                                    atIndex:0
                                             effectiveRange:nil] doubleValue],
                               -3.0,
                               0.001);
    XCTAssertEqual([[attributedString attribute:NSUnderlineStyleAttributeName
                                        atIndex:0
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle);
    XCTAssertEqual([[attributedString attribute:NSStrikethroughStyleAttributeName
                                        atIndex:0
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle);

    XCTAssertNil([attributedString attribute:NSUnderlineStyleAttributeName atIndex:1 effectiveRange:nil]);
    XCTAssertNil([attributedString attribute:NSStrikethroughStyleAttributeName atIndex:1 effectiveRange:nil]);

    XCTAssertEqual([[attributedString attribute:NSUnderlineStyleAttributeName
                                        atIndex:2
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle);
    XCTAssertNil([attributedString attribute:NSStrikethroughStyleAttributeName atIndex:2 effectiveRange:nil]);

    XCTAssertNil([attributedString attribute:NSUnderlineStyleAttributeName atIndex:3 effectiveRange:nil]);
    XCTAssertEqual([[attributedString attribute:NSStrikethroughStyleAttributeName
                                        atIndex:3
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle);

    XCTAssertEqual([[attributedString attribute:NSUnderlineStyleAttributeName
                                        atIndex:4
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle | NSUnderlinePatternDash);
    XCTAssertEqual([[attributedString attribute:NSUnderlineStyleAttributeName
                                        atIndex:5
                                 effectiveRange:nil] integerValue],
                   NSUnderlineStyleSingle | NSUnderlinePatternDot);
}

- (void)testImageAttachmentProducesAttachmentCharacterAndThinSpace
{
    UIFont *font = [UIFont systemFontOfSize:20];
    Valdi::ImageAttachment imageAttachment;
    imageAttachment.attachmentId = STRING_LITERAL("image");
    imageAttachment.width = 13;
    imageAttachment.height = 9;

    Valdi::TextAttributeValue::Parts parts;
    auto &imageStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL(""));
    imageStyle.imageAttachment = imageAttachment;

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts),
                                      @{
                                          NSFontAttributeName: font,
                                          NSForegroundColorAttributeName: UIColor.redColor,
                                      },
                                      nil);

    XCTAssertEqualObjects(processedText.attributedString.string, @"\uFFFC\u2009");
    XCTAssertFalse(processedText.hasInlineViewAttachment);
    XCTAssertFalse([processedText updateInlineAttachments]);

    NSTextAttachment *attachment = SCValdiTextAttachmentAtRange(processedText, NSMakeRange(0, 1));
    XCTAssertNotNil(attachment);
    XCTAssertNotNil(attachment.image);
    XCTAssertEqualWithAccuracy(attachment.bounds.size.width, 13, 0.001);
    XCTAssertEqualWithAccuracy(attachment.bounds.size.height, 9, 0.001);
    XCTAssertEqualWithAccuracy(attachment.bounds.origin.y,
                               (font.ascender + font.descender - 9) / 2.0,
                               0.001);
    XCTAssertNil([processedText.attributedString attribute:NSForegroundColorAttributeName
                                                   atIndex:0
                                            effectiveRange:nil]);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSForegroundColorAttributeName
                                                            atIndex:1
                                                     effectiveRange:nil],
                          UIColor.redColor);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSFontAttributeName
                                                            atIndex:1
                                                     effectiveRange:nil],
                          font);
}

- (void)testInlineViewAttachmentKeepsAttributesOnAttachmentCharacterAndThinSpace
{
    CGSize attachmentSize = CGSizeMake(11, 7);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &attachmentSize);

    UIFont *font = [UIFont systemFontOfSize:19];
    Valdi::TextAttributeValue::Parts parts;
    SCValdiTestAppendInlineAttachmentPart(parts, attachment);

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts),
                                      @{
                                          NSFontAttributeName: font,
                                          NSForegroundColorAttributeName: UIColor.blueColor,
                                      },
                                      nil);

    XCTAssertEqualObjects(processedText.attributedString.string, @"\uFFFC\u2009");
    XCTAssertEqualObjects([processedText.attributedString attribute:NSForegroundColorAttributeName
                                                            atIndex:0
                                                     effectiveRange:nil],
                          UIColor.blueColor);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSFontAttributeName
                                                            atIndex:0
                                                     effectiveRange:nil],
                          font);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSForegroundColorAttributeName
                                                            atIndex:1
                                                     effectiveRange:nil],
                          UIColor.blueColor);
    XCTAssertEqualObjects([processedText.attributedString attribute:NSFontAttributeName
                                                            atIndex:1
                                                     effectiveRange:nil],
                          font);
}

- (void)testCustomUnderlineReplacementUsesUnderlineForegroundAndFallbackColors
{
    NSMutableAttributedString *attributedString = [[NSMutableAttributedString alloc] initWithString:@"abc"];
    [attributedString addAttribute:NSUnderlineStyleAttributeName
                             value:@(NSUnderlineStyleSingle)
                             range:NSMakeRange(0, 1)];
    [attributedString addAttribute:NSUnderlineColorAttributeName
                             value:UIColor.purpleColor
                             range:NSMakeRange(0, 1)];
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:UIColor.redColor
                             range:NSMakeRange(0, 1)];
    [attributedString addAttribute:NSUnderlineStyleAttributeName
                             value:@(NSUnderlineStyleSingle | NSUnderlinePatternDash)
                             range:NSMakeRange(1, 1)];
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:UIColor.orangeColor
                             range:NSMakeRange(1, 1)];
    [attributedString addAttribute:NSUnderlineStyleAttributeName
                             value:@(NSUnderlineStyleSingle | NSUnderlinePatternDot)
                             range:NSMakeRange(2, 1)];

    SCValdiProcessedTextConfiguration *configuration = [SCValdiProcessedTextConfiguration new];
    configuration.customUnderlineStyle = [[SCValdiCustomUnderlineStyle alloc] initWithHeight:1
                                                                                     onWidth:0
                                                                                    offWidth:0
                                                                                      offset:0];
    configuration.customUnderlineMode = SCValdiProcessedTextCustomUnderlineModeReplaceNativeUnderlineWithColorAttribute;
    configuration.customUnderlineColorAttributeName = kSCValdiTextViewCustomUnderlineColorAttribute;
    configuration.customUnderlineFallbackColor = UIColor.cyanColor;

    SCValdiProcessedText *processedText =
        [SCValdiProcessedText processedTextWithAttributeValue:attributedString
                                                   attributes:nil
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:configuration];

    XCTAssertTrue(processedText.hasCustomUnderline);
    XCTAssertNil(processedText.customUnderlineSourceString);
    XCTAssertEqual(processedText.customUnderlineCharacterRanges.count, 3U);
    XCTAssertNil([processedText.attributedString attribute:NSUnderlineStyleAttributeName atIndex:0 effectiveRange:nil]);
    XCTAssertNil([processedText.attributedString attribute:NSUnderlineColorAttributeName atIndex:0 effectiveRange:nil]);
    SCValdiAssertColorEqual([processedText.attributedString attribute:kSCValdiTextViewCustomUnderlineColorAttribute
                                                              atIndex:0
                                                       effectiveRange:nil],
                            UIColor.purpleColor);
    SCValdiAssertColorEqual([processedText.attributedString attribute:kSCValdiTextViewCustomUnderlineColorAttribute
                                                              atIndex:1
                                                       effectiveRange:nil],
                            UIColor.orangeColor);
    SCValdiAssertColorEqual([processedText.attributedString attribute:kSCValdiTextViewCustomUnderlineColorAttribute
                                                              atIndex:2
                                                       effectiveRange:nil],
                            UIColor.cyanColor);
}

- (void)testAttributedTextAccessorsEnumeratorsAndComputedFlags
{
    CGSize attachmentSize = CGSizeMake(12, 10);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(1, &attachmentSize);
    attachment->setVerticalAlignment(Valdi::InlineViewVerticalAlignment::Bottom);

    auto onTap = SCValdiTestNoopFunction();
    auto onLayout = SCValdiTestNoopFunction();

    Valdi::TextAttributeValue::Parts parts;
    auto &interactiveStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("tap"));
    interactiveStyle.onTap = onTap;
    interactiveStyle.onLayout = onLayout;

    auto &inlineStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("i"));
    inlineStyle.inlineViewAttachment = attachment;

    auto &animatedStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("anim"));
    animatedStyle.animationTransform = Valdi::TextAnimationTransform{
        STRING_LITERAL("intro"),
        4,
        1.5,
        0.75,
        0.2,
        0.6,
        7,
        3,
    };

    auto &outlineStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("outline"));
    outlineStyle.outerOutlineColor = Valdi::Color(10, 20, 30, 255);
    outlineStyle.outerOutlineWidth = 2.5;

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts), @{ NSFontAttributeName: [UIFont systemFontOfSize:20] }, nil);

    XCTAssertEqualObjects(processedText.attributedString.string, @"tap\uFFFC\u2009animoutline");
    XCTAssertTrue(processedText.hasOnTap);
    XCTAssertTrue(processedText.hasOnLayout);
    XCTAssertTrue(processedText.hasInlineViewAttachment);
    XCTAssertTrue(processedText.hasAnimationTransform);
    XCTAssertTrue(processedText.hasOuterOutline);
    XCTAssertEqual(processedText.animationTransformsCount, 1U);

    NSRange range = NSMakeRange(NSNotFound, 0);
    XCTAssertNotNil([processedText onTapAtIndex:1 effectiveRange:&range]);
    XCTAssertTrue(NSEqualRanges(range, NSMakeRange(0, 3)));
    XCTAssertNil([processedText onTapAtIndex:4 effectiveRange:NULL]);

    XCTAssertNotNil([processedText onLayoutAtIndex:2 effectiveRange:&range]);
    XCTAssertTrue(NSEqualRanges(range, NSMakeRange(0, 3)));
    XCTAssertNil([processedText onLayoutAtIndex:4 effectiveRange:NULL]);

    SCValdiInlineViewAttachmentInfo *inlineAttachment =
        [processedText inlineViewAttachmentAtIndex:3 effectiveRange:&range];
    XCTAssertNotNil(inlineAttachment);
    XCTAssertEqual(inlineAttachment.childIndex, 1);
    XCTAssertEqual(inlineAttachment.verticalAlignment, SCValdiInlineViewVerticalAlignmentBottom);
    XCTAssertTrue(NSEqualRanges(range, NSMakeRange(3, 1)));
    XCTAssertNil([processedText inlineViewAttachmentAtIndex:0 effectiveRange:NULL]);
    XCTAssertFalse([processedText hasInlineViewAttachmentForIndex:0]);
    XCTAssertTrue([processedText hasInlineViewAttachmentForIndex:1]);
    XCTAssertFalse([processedText hasInlineViewAttachmentForIndex:2]);
    XCTAssertNil([processedText inlineViewAttachmentForViewIndex:0 effectiveRange:NULL]);
    XCTAssertEqual([processedText inlineViewAttachmentForViewIndex:1 effectiveRange:NULL], inlineAttachment);
    NSRange inlineAttachmentRange = NSMakeRange(NSNotFound, 0);
    XCTAssertEqual([processedText inlineViewAttachmentForViewIndex:1 effectiveRange:&inlineAttachmentRange], inlineAttachment);
    XCTAssertTrue(NSEqualRanges(inlineAttachmentRange, NSMakeRange(3, 1)));

    __block NSUInteger onLayoutCount = 0;
    [processedText enumerateOnLayoutCallbacksUsingBlock:^(id<SCValdiFunction> callback, NSRange callbackRange, BOOL *stop) {
        XCTAssertNotNil(callback);
        XCTAssertTrue(NSEqualRanges(callbackRange, NSMakeRange(0, 3)));
        onLayoutCount++;
        *stop = YES;
    }];
    XCTAssertEqual(onLayoutCount, 1U);

    __block NSUInteger inlineAttachmentCount = 0;
    [processedText enumerateInlineViewAttachmentsUsingBlock:^(SCValdiInlineViewAttachmentInfo *enumeratedAttachment,
                                                              NSRange attachmentRange,
                                                              BOOL *stop) {
        XCTAssertEqual(enumeratedAttachment.childIndex, 1);
        XCTAssertTrue(NSEqualRanges(attachmentRange, NSMakeRange(3, 1)));
        inlineAttachmentCount++;
        *stop = YES;
    }];
    XCTAssertEqual(inlineAttachmentCount, 1U);

    __block NSUInteger animationCount = 0;
    [processedText enumerateAnimationTransformsUsingBlock:^(SCValdiTextAnimationTransform *animationTransform,
                                                            NSRange animationRange,
                                                            BOOL *stop) {
        XCTAssertEqualObjects(animationTransform.key, @"intro");
        XCTAssertEqual(animationTransform.partIndex, 2U);
        XCTAssertEqualWithAccuracy(animationTransform.translationY, 4, 0.001);
        XCTAssertEqualWithAccuracy(animationTransform.scale, 1.5, 0.001);
        XCTAssertEqualWithAccuracy(animationTransform.opacity, 0.75, 0.001);
        XCTAssertEqualWithAccuracy(animationTransform.duration, 0.2, 0.001);
        XCTAssertEqualWithAccuracy(animationTransform.timeOffsetBetweenParts, 0.6, 0.001);
        XCTAssertEqual(animationTransform.groupIndex, 7U);
        XCTAssertEqual(animationTransform.partIndexInGroup, 3U);
        XCTAssertTrue(NSEqualRanges(animationRange, NSMakeRange(5, 4)));
        animationCount++;
        *stop = YES;
    }];
    XCTAssertEqual(animationCount, 1U);

    __block NSUInteger outerOutlineCount = 0;
    [processedText enumerateOuterOutlinesUsingBlock:^(UIColor *color, CGFloat width, NSRange outlineRange, BOOL *stop) {
        SCValdiAssertColorEqual(color, UIColorFromValdiAttributeValue(0x0A141EFF));
        XCTAssertEqualWithAccuracy(width, 2.5, 0.001);
        XCTAssertTrue(NSEqualRanges(outlineRange, NSMakeRange(9, 7)));
        outerOutlineCount++;
        *stop = YES;
    }];
    XCTAssertEqual(outerOutlineCount, 1U);

    NSLayoutManager *layoutManager = [NSLayoutManager new];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithSize:CGSizeMake(400, CGFLOAT_MAX)];
    __unused NSTextStorage *textStorage =
        SCValdiConfigureLayoutManagerForProcessedText(processedText, layoutManager, textContainer);
    XCTAssertFalse(CGRectIsNull([processedText rectForInlineViewAttachment:inlineAttachment
                                                       layoutManager:layoutManager
                                                       textContainer:textContainer]));
    NSLayoutManager *nilLayoutManager = nil;
    XCTAssertTrue(CGRectIsNull([processedText rectForInlineViewAttachment:inlineAttachment
                                                      layoutManager:nilLayoutManager
                                                      textContainer:textContainer]));
}

- (void)testTextAnimationLayoutManagerReportsCurrentPresentationForRange
{
    Valdi::TextAttributeValue::Parts parts;
    auto &animatedStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("fade"));
    animatedStyle.animationTransform = Valdi::TextAnimationTransform{
        std::nullopt,
        8,
        0.7,
        0,
        100,
        0,
        0,
        0,
    };

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts), @{ NSFontAttributeName: [UIFont systemFontOfSize:20] }, nil);
    SCValdiTextViewEffectsLayoutManager *layoutManager = [SCValdiTextViewEffectsLayoutManager new];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithSize:CGSizeMake(400, CGFLOAT_MAX)];
    __unused NSTextStorage *textStorage =
        SCValdiConfigureLayoutManagerForProcessedText(processedText, layoutManager, textContainer);
    layoutManager.processedText = processedText;

    [layoutManager invalidateAnimatedTextProgress];

    SCValdiTextAnimationPresentation *presentation =
        [layoutManager presentationForAnimationRange:NSMakeRange(0, 4)];
    XCTAssertLessThan(presentation.opacity, 0.5);
    XCTAssertGreaterThan(presentation.translationY, 4.0);
    XCTAssertLessThan(presentation.scale, 0.85);

    XCTAssertNil([layoutManager presentationForAnimationRange:NSMakeRange(20, 1)]);
}

- (void)testEnumeratorsHonorStopWithMultipleItems
{
    CGSize firstAttachmentSize = CGSizeMake(12, 10);
    CGSize secondAttachmentSize = CGSizeMake(14, 11);
    auto firstAttachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &firstAttachmentSize);
    auto secondAttachment = Valdi::makeShared<SCValdiTestInlineAttachment>(1, &secondAttachmentSize);

    Valdi::TextAttributeValue::Parts parts;
    auto &firstStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("a"));
    firstStyle.onLayout = SCValdiTestNoopFunction();
    firstStyle.animationTransform = Valdi::TextAnimationTransform{};
    firstStyle.outerOutlineColor = Valdi::Color(10, 20, 30, 255);
    firstStyle.outerOutlineWidth = 2;
    SCValdiTestAppendInlineAttachmentPart(parts, firstAttachment);

    auto &secondStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("b"));
    secondStyle.onLayout = SCValdiTestNoopFunction();
    secondStyle.animationTransform = Valdi::TextAnimationTransform{};
    secondStyle.outerOutlineColor = Valdi::Color(40, 50, 60, 255);
    secondStyle.outerOutlineWidth = 3;
    SCValdiTestAppendInlineAttachmentPart(parts, secondAttachment);

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts), @{ NSFontAttributeName: [UIFont systemFontOfSize:20] }, nil);

    __block NSUInteger onLayoutCount = 0;
    [processedText enumerateOnLayoutCallbacksUsingBlock:^(id<SCValdiFunction> callback, NSRange range, BOOL *stop) {
        XCTAssertNotNil(callback);
        XCTAssertTrue(NSEqualRanges(range, NSMakeRange(0, 1)));
        onLayoutCount++;
        *stop = YES;
    }];
    XCTAssertEqual(onLayoutCount, 1U);

    __block NSUInteger inlineAttachmentCount = 0;
    [processedText enumerateInlineViewAttachmentsUsingBlock:^(SCValdiInlineViewAttachmentInfo *attachment,
                                                              NSRange range,
                                                              BOOL *stop) {
        XCTAssertEqual(attachment.childIndex, 0);
        XCTAssertTrue(NSEqualRanges(range, NSMakeRange(1, 1)));
        inlineAttachmentCount++;
        *stop = YES;
    }];
    XCTAssertEqual(inlineAttachmentCount, 1U);

    __block NSUInteger animationCount = 0;
    [processedText enumerateAnimationTransformsUsingBlock:^(SCValdiTextAnimationTransform *animationTransform,
                                                            NSRange range,
                                                            BOOL *stop) {
        XCTAssertNotNil(animationTransform);
        XCTAssertTrue(NSEqualRanges(range, NSMakeRange(0, 1)));
        animationCount++;
        *stop = YES;
    }];
    XCTAssertEqual(animationCount, 1U);

    __block NSUInteger outlineCount = 0;
    [processedText enumerateOuterOutlinesUsingBlock:^(UIColor *color, CGFloat width, NSRange range, BOOL *stop) {
        SCValdiAssertColorEqual(color, UIColorFromValdiAttributeValue(0x0A141EFF));
        XCTAssertEqualWithAccuracy(width, 2, 0.001);
        XCTAssertTrue(NSEqualRanges(range, NSMakeRange(0, 1)));
        outlineCount++;
        *stop = YES;
    }];
    XCTAssertEqual(outlineCount, 1U);
}

- (void)testClampUpdatesDerivedApiState
{
    CGSize attachmentSize = CGSizeMake(12, 10);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &attachmentSize);

    Valdi::TextAttributeValue::Parts parts;
    auto &onLayoutStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("a"));
    onLayoutStyle.onLayout = SCValdiTestNoopFunction();
    SCValdiTestAppendInlineAttachmentPart(parts, attachment);
    auto &animationStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("b"));
    animationStyle.animationTransform = Valdi::TextAnimationTransform{};
    auto &outlineStyle = SCValdiTestAppendTextPart(parts, STRING_LITERAL("c"));
    outlineStyle.outerOutlineColor = Valdi::Color(10, 20, 30, 255);
    outlineStyle.outerOutlineWidth = 2;

    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithParts(std::move(parts), @{ NSFontAttributeName: [UIFont systemFontOfSize:20] }, nil);
    XCTAssertTrue(processedText.hasOnLayout);
    XCTAssertTrue(processedText.hasInlineViewAttachment);
    XCTAssertTrue(processedText.hasAnimationTransform);
    XCTAssertTrue(processedText.hasOuterOutline);
    XCTAssertEqual(processedText.animationTransformsCount, 1U);

    BOOL didChange = NO;
    [processedText clampToCharacterLimit:1 ignoreNewlines:NO didChange:&didChange];
    XCTAssertTrue(didChange);
    XCTAssertEqualObjects(processedText.attributedString.string, @"a");
    XCTAssertTrue(processedText.hasOnLayout);
    XCTAssertFalse(processedText.hasInlineViewAttachment);
    XCTAssertFalse(processedText.hasAnimationTransform);
    XCTAssertFalse(processedText.hasOuterOutline);
    XCTAssertEqual(processedText.animationTransformsCount, 0U);
    XCTAssertNil([processedText inlineViewAttachmentAtIndex:0 effectiveRange:NULL]);

    didChange = YES;
    [processedText clampToCharacterLimit:10 ignoreNewlines:NO didChange:&didChange];
    XCTAssertFalse(didChange);
}

- (void)testInlineViewAttachmentRejectsChildIndexAboveMaximum
{
    CGSize attachmentSize = CGSizeMake(12, 10);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(64 * 1024, &attachmentSize);

    XCTAssertThrowsSpecificNamed(SCValdiProcessedTextWithInlineAttachments({ attachment }, [UIFont systemFontOfSize:20]),
                                 NSException,
                                 NSInvalidArgumentException);
}

- (void)testProcessedTextClampingRemapsCustomUnderlineRanges
{
    NSMutableAttributedString *attributedString =
        [[NSMutableAttributedString alloc] initWithString:@"a\nbcd"
                                               attributes:@{
                                                   NSFontAttributeName: [UIFont systemFontOfSize:20],
                                                   NSForegroundColorAttributeName: UIColor.blackColor,
                                               }];
    [attributedString addAttribute:NSUnderlineStyleAttributeName
                             value:@(NSUnderlineStyleSingle)
                             range:NSMakeRange(2, 1)];

    SCValdiProcessedTextConfiguration *configuration = [SCValdiProcessedTextConfiguration new];
    configuration.customUnderlineStyle = [[SCValdiCustomUnderlineStyle alloc] initWithHeight:1
                                                                                     onWidth:0
                                                                                    offWidth:0
                                                                                      offset:0];
    configuration.customUnderlineMode = SCValdiProcessedTextCustomUnderlineModeReplaceNativeUnderlineWithColorAttribute;
    configuration.customUnderlineColorAttributeName = kSCValdiTextViewCustomUnderlineColorAttribute;

    SCValdiProcessedText *processedText =
        [SCValdiProcessedText processedTextWithAttributeValue:attributedString
                                                   attributes:nil
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:configuration];

    BOOL didChange = NO;
    [processedText clampToCharacterLimit:3
                          ignoreNewlines:YES
                               didChange:&didChange];
    SCValdiProcessedText *clampedText = processedText;
    XCTAssertTrue(didChange);
    XCTAssertEqualObjects(clampedText.attributedString.string, @"ab");
    XCTAssertTrue(clampedText.hasCustomUnderline);

    NSRange customUnderlineRange = NSMakeRange(NSNotFound, 0);
    XCTAssertNotNil([clampedText.attributedString attribute:kSCValdiTextViewCustomUnderlineColorAttribute
                                                    atIndex:1
                                     longestEffectiveRange:&customUnderlineRange
                                                    inRange:NSMakeRange(0, clampedText.attributedString.length)]);
    XCTAssertTrue(NSEqualRanges(customUnderlineRange, NSMakeRange(1, 1)));

    processedText =
        [SCValdiProcessedText processedTextWithAttributeValue:attributedString
                                                   attributes:nil
                                                isRightToLeft:NO
                                                  fontManager:nil
                                              traitCollection:nil
                                                configuration:configuration];
    [processedText clampToCharacterLimit:2
                          ignoreNewlines:YES
                               didChange:NULL];
    clampedText = processedText;
    XCTAssertEqualObjects(clampedText.attributedString.string, @"a");
    XCTAssertFalse(clampedText.hasCustomUnderline);
}

- (void)testInlineViewAttachmentSizeUpdatesTextAttachmentBounds
{
    CGSize attachmentSize = CGSizeMake(18, 12);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &attachmentSize);
    attachment->setVerticalAlignment(Valdi::InlineViewVerticalAlignment::Top);

    UIFont *font = [UIFont systemFontOfSize:20];
    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithInlineAttachments({ attachment }, font);

    XCTAssertTrue(processedText.hasInlineViewAttachment);

    NSRange inlineAttachmentRange = NSMakeRange(NSNotFound, 0);
    SCValdiInlineViewAttachmentInfo *inlineAttachment =
        [processedText inlineViewAttachmentAtIndex:1 effectiveRange:&inlineAttachmentRange];
    XCTAssertNotNil(inlineAttachment);
    XCTAssertEqual(inlineAttachment.childIndex, 0);
    XCTAssertEqual(inlineAttachment.verticalAlignment, SCValdiInlineViewVerticalAlignmentTop);
    XCTAssertTrue(NSEqualRanges(inlineAttachmentRange, NSMakeRange(1, 1)));

    NSTextAttachment *textAttachment = SCValdiTextAttachmentAtRange(processedText, inlineAttachmentRange);
    XCTAssertNotNil(textAttachment);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.origin.y, 0, 0.001);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.size.width, 18, 0.001);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.size.height, 12, 0.001);

    attachmentSize = CGSizeMake(28, 16);
    XCTAssertTrue([processedText updateInlineAttachments]);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.origin.y, 0, 0.001);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.size.width, 28, 0.001);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.size.height, 16, 0.001);
    XCTAssertFalse([processedText updateInlineAttachments]);
}

- (void)testBaselineInlineViewAttachmentRectBottomMatchesTextBaseline
{
    CGSize attachmentSize = CGSizeMake(18, 12);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &attachmentSize);
    attachment->setVerticalAlignment(Valdi::InlineViewVerticalAlignment::Baseline);

    UIFont *font = [UIFont systemFontOfSize:20];
    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithInlineAttachments({ attachment }, font);

    NSRange inlineAttachmentRange = NSMakeRange(NSNotFound, 0);
    SCValdiInlineViewAttachmentInfo *inlineAttachment =
        [processedText inlineViewAttachmentAtIndex:1 effectiveRange:&inlineAttachmentRange];
    XCTAssertNotNil(inlineAttachment);
    XCTAssertEqual(inlineAttachment.verticalAlignment, SCValdiInlineViewVerticalAlignmentBaseline);

    NSTextAttachment *textAttachment = SCValdiTextAttachmentAtRange(processedText, inlineAttachmentRange);
    XCTAssertNotNil(textAttachment);
    XCTAssertEqualWithAccuracy(textAttachment.bounds.origin.y, 0, 0.001);

    NSLayoutManager *layoutManager = [NSLayoutManager new];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithSize:CGSizeMake(400, CGFLOAT_MAX)];
    __unused NSTextStorage *textStorage =
        SCValdiConfigureLayoutManagerForProcessedText(processedText, layoutManager, textContainer);

    CGRect attachmentRect = [processedText rectForInlineViewAttachment:inlineAttachment
                                                          layoutManager:layoutManager
                                                          textContainer:textContainer];
    XCTAssertFalse(CGRectIsNull(attachmentRect));

    NSRange glyphRange = [layoutManager glyphRangeForCharacterRange:inlineAttachmentRange actualCharacterRange:nil];
    CGRect lineRect = [layoutManager lineFragmentRectForGlyphAtIndex:glyphRange.location effectiveRange:nil];
    CGPoint glyphLocation = [layoutManager locationForGlyphAtIndex:glyphRange.location];
    CGFloat baselineY = CGRectGetMinY(lineRect) + glyphLocation.y;
    XCTAssertEqualWithAccuracy(CGRectGetMaxY(attachmentRect), baselineY, 0.001);
}

- (void)testInlineViewAttachmentUpdatesDoNotMutateParagraphLineHeight
{
    CGSize attachmentSize = CGSizeMake(156, 42);
    auto attachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &attachmentSize);
    attachment->setVerticalAlignment(Valdi::InlineViewVerticalAlignment::Center);

    Valdi::TextAttributeValue::Parts parts;
    SCValdiTestAppendTextPart(parts, STRING_LITERAL("A label can host a stateful inline child: "));
    SCValdiTestAppendInlineAttachmentPart(parts, attachment);
    SCValdiTestAppendTextPart(parts, STRING_LITERAL(" and the surrounding text changes."));

    UIFont *font = [UIFont systemFontOfSize:19];
    NSDictionary<NSAttributedStringKey, id> *attributes = @{
        NSFontAttributeName: font,
        SCValdiLineHeightMultipleAttributeName: @1.45,
    };
    SCValdiProcessedText *processedText = SCValdiProcessedTextWithParts(std::move(parts), attributes, nil);

    CGFloat baseLineHeight = font.pointSize * 1.45;
    NSParagraphStyle *initialParagraphStyle =
        [processedText.attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil];
    XCTAssertEqualWithAccuracy(initialParagraphStyle.minimumLineHeight, baseLineHeight, 0.001);
    XCTAssertEqualWithAccuracy(initialParagraphStyle.maximumLineHeight, baseLineHeight, 0.001);

    attachmentSize = CGSizeMake(82, 26);
    XCTAssertTrue([processedText updateInlineAttachments]);
    NSParagraphStyle *updatedParagraphStyle =
        [processedText.attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil];
    XCTAssertEqualWithAccuracy(updatedParagraphStyle.minimumLineHeight, baseLineHeight, 0.001);
    XCTAssertEqualWithAccuracy(updatedParagraphStyle.maximumLineHeight, baseLineHeight, 0.001);
}

- (void)testApplyInlineTextChildFramesUsesContainerSubviewsByChildIndex
{
    CGSize firstAttachmentSize = CGSizeMake(10, 8);
    CGSize secondAttachmentSize = CGSizeMake(24, 14);
    auto firstAttachment = Valdi::makeShared<SCValdiTestInlineAttachment>(2, &firstAttachmentSize);
    auto secondAttachment = Valdi::makeShared<SCValdiTestInlineAttachment>(0, &secondAttachmentSize);

    UIFont *font = [UIFont systemFontOfSize:20];
    SCValdiProcessedText *processedText =
        SCValdiProcessedTextWithInlineAttachments({ firstAttachment, secondAttachment }, font);

    NSLayoutManager *layoutManager = [NSLayoutManager new];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithSize:CGSizeMake(400, CGFLOAT_MAX)];
    __unused NSTextStorage *textStorage =
        SCValdiConfigureLayoutManagerForProcessedText(processedText, layoutManager, textContainer);

    __block SCValdiInlineViewAttachmentInfo *firstInlineAttachment = nil;
    __block SCValdiInlineViewAttachmentInfo *secondInlineAttachment = nil;
    [processedText enumerateInlineViewAttachmentsUsingBlock:^(SCValdiInlineViewAttachmentInfo *attachment,
                                                              NSRange range,
                                                              BOOL *stop) {
        (void)range;
        (void)stop;
        if (attachment.childIndex == 2) {
            firstInlineAttachment = attachment;
        } else if (attachment.childIndex == 0) {
            secondInlineAttachment = attachment;
        }
    }];
    XCTAssertNotNil(firstInlineAttachment);
    XCTAssertNotNil(secondInlineAttachment);

    UIView *containerView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 100)];
    UIView *childZero = [UIView new];
    UIView *childOne = [UIView new];
    UIView *childTwo = [UIView new];
    SCValdiProcessedTextTestViewNode *childZeroViewNode =
        [[SCValdiProcessedTextTestViewNode alloc] initWithView:childZero];
    SCValdiProcessedTextTestViewNode *childOneViewNode =
        [[SCValdiProcessedTextTestViewNode alloc] initWithView:childOne];
    SCValdiProcessedTextTestViewNode *childTwoViewNode =
        [[SCValdiProcessedTextTestViewNode alloc] initWithView:childTwo];
    childZero.valdiViewNode = childZeroViewNode;
    childOne.valdiViewNode = childOneViewNode;
    childTwo.valdiViewNode = childTwoViewNode;
    childOne.frame = CGRectMake(30, 40, 50, 60);
    [containerView addSubview:childZero];
    [containerView addSubview:childOne];
    [containerView addSubview:childTwo];

    CGPoint originOffset = CGPointMake(7, 11);
    NSRange childZeroRange = NSMakeRange(NSNotFound, 0);
    NSRange childTwoRange = NSMakeRange(NSNotFound, 0);
    [processedText inlineViewAttachmentForViewIndex:0 effectiveRange:&childZeroRange];
    [processedText inlineViewAttachmentForViewIndex:2 effectiveRange:&childTwoRange];
    SCValdiApplyInlineTextChildFrames(processedText, layoutManager, textContainer, originOffset, containerView);
    XCTAssertNil(childZeroViewNode.attributeValues[@"opacity"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"translationY"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"scaleX"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"scaleY"]);

    SCValdiApplyInlineTextChildAnimations(
        processedText,
        containerView,
        ^SCValdiTextAnimationPresentation *(NSRange range) {
            if (NSEqualRanges(range, childZeroRange)) {
                return [[SCValdiTextAnimationPresentation alloc] initWithTranslationY:6.0
                                                                                scale:0.8
                                                                              opacity:0.4];
            }
            if (NSEqualRanges(range, childTwoRange)) {
                return [[SCValdiTextAnimationPresentation alloc] initWithTranslationY:0.0
                                                                                scale:1.0
                                                                              opacity:0.7];
            }
            return nil;
        });

    CGRect expectedChildZeroFrame = [processedText rectForInlineViewAttachment:secondInlineAttachment
                                                                 layoutManager:layoutManager
                                                                 textContainer:textContainer];
    expectedChildZeroFrame.origin.x += originOffset.x;
    expectedChildZeroFrame.origin.y += originOffset.y;
    SCValdiViewLayout expectedChildZeroLayout =
        SCValdiMakeViewLayout(expectedChildZeroFrame.origin.x,
                              expectedChildZeroFrame.origin.y,
                              expectedChildZeroFrame.size.width,
                              expectedChildZeroFrame.size.height);
    XCTAssertTrue(CGPointEqualToPoint(childZero.center, expectedChildZeroLayout.center));
    XCTAssertTrue(CGSizeEqualToSize(childZero.bounds.size, expectedChildZeroLayout.size));
    XCTAssertTrue(CGRectEqualToRect(childOne.frame, CGRectZero));
    XCTAssertEqualWithAccuracy([childZeroViewNode.attributeValues[@"opacity"] doubleValue], 0.4, 0.001);
    XCTAssertEqualWithAccuracy([childZeroViewNode.attributeValues[@"translationY"] doubleValue], 6.0, 0.001);
    XCTAssertEqualWithAccuracy([childZeroViewNode.attributeValues[@"scaleX"] doubleValue], 0.8, 0.001);
    XCTAssertEqualWithAccuracy([childZeroViewNode.attributeValues[@"scaleY"] doubleValue], 0.8, 0.001);
    XCTAssertNil(childOneViewNode.attributeValues[@"opacity"]);
    XCTAssertNil(childOneViewNode.attributeValues[@"translationY"]);
    XCTAssertNil(childOneViewNode.attributeValues[@"scaleX"]);
    XCTAssertNil(childOneViewNode.attributeValues[@"scaleY"]);
    XCTAssertTrue([childOneViewNode.removedAttributeNames containsObject:@"opacity"]);
    XCTAssertTrue([childOneViewNode.removedAttributeNames containsObject:@"translationY"]);
    XCTAssertTrue([childOneViewNode.removedAttributeNames containsObject:@"scaleX"]);
    XCTAssertTrue([childOneViewNode.removedAttributeNames containsObject:@"scaleY"]);

    CGRect expectedChildTwoFrame = [processedText rectForInlineViewAttachment:firstInlineAttachment
                                                                layoutManager:layoutManager
                                                                textContainer:textContainer];
    expectedChildTwoFrame.origin.x += originOffset.x;
    expectedChildTwoFrame.origin.y += originOffset.y;
    SCValdiViewLayout expectedChildTwoLayout =
        SCValdiMakeViewLayout(expectedChildTwoFrame.origin.x,
                              expectedChildTwoFrame.origin.y,
                              expectedChildTwoFrame.size.width,
                              expectedChildTwoFrame.size.height);
    XCTAssertTrue(CGPointEqualToPoint(childTwo.center, expectedChildTwoLayout.center));
    XCTAssertTrue(CGSizeEqualToSize(childTwo.bounds.size, expectedChildTwoLayout.size));
    XCTAssertEqualWithAccuracy([childTwoViewNode.attributeValues[@"opacity"] doubleValue], 0.7, 0.001);
    XCTAssertNil(childTwoViewNode.attributeValues[@"translationY"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"scaleX"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"scaleY"]);
    XCTAssertTrue([childTwoViewNode.removedAttributeNames containsObject:@"translationY"]);
    XCTAssertTrue([childTwoViewNode.removedAttributeNames containsObject:@"scaleX"]);
    XCTAssertTrue([childTwoViewNode.removedAttributeNames containsObject:@"scaleY"]);

    SCValdiInlineViewAttachmentInfo *missingChildOneAttachment =
        [[SCValdiInlineViewAttachmentInfo alloc] initWithChildIndex:1
                                                  verticalAlignment:SCValdiInlineViewVerticalAlignmentCenter
                                                        sizeProvider:^CGSize {
            return CGSizeMake(4, 4);
        }];
    XCTAssertTrue(CGRectIsNull([processedText rectForInlineViewAttachment:missingChildOneAttachment
                                                            layoutManager:layoutManager
                                                            textContainer:textContainer]));

    Valdi::TextAttributeValue::Parts plainParts;
    SCValdiTestAppendTextPart(plainParts, STRING_LITERAL("plain"));
    SCValdiProcessedText *plainProcessedText =
        SCValdiProcessedTextWithParts(std::move(plainParts), @{ NSFontAttributeName: font }, nil);
    SCValdiApplyInlineTextChildFrames(plainProcessedText, layoutManager, textContainer, originOffset, containerView);
    SCValdiApplyInlineTextChildAnimations(plainProcessedText,
                                          containerView,
                                          ^SCValdiTextAnimationPresentation *(NSRange range) {
                                              (void)range;
                                              return nil;
                                          });

    XCTAssertTrue(CGRectEqualToRect(childZero.frame, CGRectZero));
    XCTAssertTrue(CGRectEqualToRect(childOne.frame, CGRectZero));
    XCTAssertTrue(CGRectEqualToRect(childTwo.frame, CGRectZero));
    XCTAssertNil(childZeroViewNode.attributeValues[@"opacity"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"translationY"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"scaleX"]);
    XCTAssertNil(childZeroViewNode.attributeValues[@"scaleY"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"opacity"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"translationY"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"scaleX"]);
    XCTAssertNil(childTwoViewNode.attributeValues[@"scaleY"]);
}

- (void)testTextViewsManageChildFramesAndProvideStableChildContainers
{
    XCTAssertTrue([SCValdiLabel valdi_managesChildFrames]);
    XCTAssertTrue([SCValdiTextView valdi_managesChildFrames]);
    XCTAssertFalse([UIView valdi_managesChildFrames]);

    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 100, 40)];
    id<SCValdiContentViewProviding> labelContentProvider = (id<SCValdiContentViewProviding>)label;
    UIView *labelContainer = [labelContentProvider contentViewForInsertingValdiChildren];
    XCTAssertNotNil(labelContainer);
    XCTAssertEqual(labelContainer.superview, label);
    XCTAssertEqual([labelContentProvider contentViewForInsertingValdiChildren], labelContainer);

    SCValdiTextView *textView = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 100, 40)];
    id<SCValdiContentViewProviding> textViewContentProvider = (id<SCValdiContentViewProviding>)textView;
    UIView *textViewContainer = [textViewContentProvider contentViewForInsertingValdiChildren];
    XCTAssertNotNil(textViewContainer);
    XCTAssertEqual(textViewContainer.superview, textView);
    XCTAssertEqual([textViewContentProvider contentViewForInsertingValdiChildren], textViewContainer);
}

- (void)testTextViewContentInsetUpdateDoesNotReenterFromContentSizeChanges
{
    SCValdiTextView *textView = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 120, 80)];
    textView.textValue = @"A text view with enough content to update its content size while vertical gravity is applied.";
    textView.needAttributedTextUpdate = YES;

    [textView setNeedsLayout];
    [textView layoutIfNeeded];
    [textView setNeedsLayout];
    [textView layoutIfNeeded];
}

@end
