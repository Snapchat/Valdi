//
//  UnicodeModuleFactory.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/9/24.
//

#include "valdi/runtime/JavaScript/Modules/UnicodeModuleFactory.hpp"

#include "valdi/runtime/JavaScript/JSNativeClassBinder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"

#include "valdi/runtime/Text/Emoji.hpp"
#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include "hb.h"

namespace Valdi {

enum class TextEncoding : int32_t {
    UTF8 = 1,
    UTF16 = 2,
    UTF32 = 3,
};

UnicodeModuleFactory::UnicodeModuleFactory() = default;
UnicodeModuleFactory::~UnicodeModuleFactory() = default;

StringBox UnicodeModuleFactory::getModulePath() const {
    return STRING_LITERAL("coreutils/src/unicode/UnicodeNative");
}

static constexpr int32_t kCharacterFlagControl = 1 << 0;
static constexpr int32_t kCharacterFlagFormat = 1 << 1;
static constexpr int32_t kCharacterFlagUnassigned = 1 << 2;
static constexpr int32_t kCharacterFlagPrivateUse = 1 << 3;
static constexpr int32_t kCharacterFlagSurrogate = 1 << 4;
static constexpr int32_t kCharacterFlagLowercaseLetter = 1 << 5;
static constexpr int32_t kCharacterFlagModifierLetter = 1 << 6;
static constexpr int32_t kCharacterFlagOtherLetter = 1 << 7;
static constexpr int32_t kCharacterFlagTitlecaseLetter = 1 << 8;
static constexpr int32_t kCharacterFlagUperrcaseLetter = 1 << 9;
static constexpr int32_t kCharacterFlagSpacingMark = 1 << 10;
static constexpr int32_t kCharacterFlagEnclosingMark = 1 << 11;
static constexpr int32_t kCharacterFlagNonSpacingMark = 1 << 12;
static constexpr int32_t kCharacterFlagDecimalNumber = 1 << 13;
static constexpr int32_t kCharacterFlagLetterNumber = 1 << 14;
static constexpr int32_t kCharacterFlagOtherNumber = 1 << 15;
static constexpr int32_t kCharacterFlagConnectPunctuation = 1 << 16;
static constexpr int32_t kCharacterFlagDashPunctuation = 1 << 17;
static constexpr int32_t kCharacterFlagClosePunctuation = 1 << 18;
static constexpr int32_t kCharacterFlagFinalPunctuation = 1 << 19;
static constexpr int32_t kCharacterFlagInitialPunctuation = 1 << 20;
static constexpr int32_t kCharacterFlagOtherPunctuation = 1 << 21;
static constexpr int32_t kCharacterFlagOpenPunctuation = 1 << 22;
static constexpr int32_t kCharacterFlagCurrencySymbol = 1 << 23;
static constexpr int32_t kCharacterFlagModifierSymbol = 1 << 24;
static constexpr int32_t kCharacterFlagMathSymbol = 1 << 25;
static constexpr int32_t kCharacterFlagOtherSymbol = 1 << 26;
static constexpr int32_t kCharacterFlagLineSeparator = 1 << 27;
static constexpr int32_t kCharacterFlagParagraphSeparator = 1 << 28;
static constexpr int32_t kCharacterFlagSpaceSeparator = 1 << 29;
static constexpr int32_t kCharacterFlagEmoji = 1 << 30;
static constexpr int32_t kCharacterFromGraphemeCluster = 1 << 31;

static constexpr int32_t kCharacterFlagMaskMarks =
    kCharacterFlagSpacingMark | kCharacterFlagEnclosingMark | kCharacterFlagNonSpacingMark;

static hb_unicode_funcs_t* getUnicodeFuncs() {
    static auto* kUnicodeFuncs = hb_unicode_funcs_get_default();
    return kUnicodeFuncs;
}

inline static int32_t getUnicodeFlag(hb_unicode_funcs_t* unicodeFuncs, uint32_t c) {
    switch (hb_unicode_general_category(unicodeFuncs, c)) {
        case HB_UNICODE_GENERAL_CATEGORY_CONTROL: /* Cc */
            return kCharacterFlagControl;
        case HB_UNICODE_GENERAL_CATEGORY_FORMAT: /* Cf */
            return kCharacterFlagFormat;
        case HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED: /* Cn */
            return kCharacterFlagUnassigned;
        case HB_UNICODE_GENERAL_CATEGORY_PRIVATE_USE: /* Co */
            return kCharacterFlagPrivateUse;
        case HB_UNICODE_GENERAL_CATEGORY_SURROGATE: /* Cs */
            return kCharacterFlagSurrogate;
        case HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER: /* Ll */
            return kCharacterFlagLowercaseLetter;
        case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER: /* Lm */
            return kCharacterFlagModifierLetter;
        case HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER: /* Lo */
            return kCharacterFlagOtherLetter;
        case HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER: /* Lt */
            return kCharacterFlagTitlecaseLetter;
        case HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER: /* Lu */
            return kCharacterFlagUperrcaseLetter;
        case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK: /* Mc */
            return kCharacterFlagSpacingMark;
        case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK: /* Me */
            return kCharacterFlagEnclosingMark;
        case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK: /* Mn */
            return kCharacterFlagNonSpacingMark;
        case HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER: /* Nd */
            return kCharacterFlagDecimalNumber;
        case HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER: /* Nl */
            return kCharacterFlagLetterNumber;
        case HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER: /* No */
            return kCharacterFlagOtherNumber;
        case HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION: /* Pc */
            return kCharacterFlagConnectPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION: /* Pd */
            return kCharacterFlagDashPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION: /* Pe */
            return kCharacterFlagClosePunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION: /* Pf */
            return kCharacterFlagFinalPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION: /* Pi */
            return kCharacterFlagInitialPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION: /* Po */
            return kCharacterFlagOtherPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION: /* Ps */
            return kCharacterFlagOpenPunctuation;
        case HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL: /* Sc */
            return kCharacterFlagCurrencySymbol;
        case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL: /* Sk */
            return kCharacterFlagModifierSymbol;
        case HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL: /* Sm */
            return kCharacterFlagMathSymbol;
        case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL: /* So */
            return kCharacterFlagOtherSymbol;
        case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR: /* Zl */
            return kCharacterFlagLineSeparator;
        case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR: /* Zp */
            return kCharacterFlagParagraphSeparator;
        case HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR: /* Zs */
            return kCharacterFlagSpaceSeparator;
    }
}

static int32_t computeCharacterFlags(hb_unicode_funcs_t* unicodeFuncs, uint32_t c) {
    return getUnicodeFlag(unicodeFuncs, c);
}

inline static bool isCombiningChar(uint32_t codepoint) {
    // Combining marks range from U+0300 to U+036F (simplified example)
    return (codepoint >= 0x0300 && codepoint <= 0x036F);
}

inline static bool isRegionalIndicator(uint32_t codepoint) {
    // Regional indicators range from U+1F1E6 to U+1F1FF
    return (codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF);
}

inline static bool isZeroWidthJoiner(uint32_t codepoint) {
    return codepoint == 0x200D; // U+200D
}

inline static bool isEmojiModifier(uint32_t codepoint) {
    // Emoji modifiers range from U+1F3FB to U+1F3FF
    return (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF);
}

inline static bool isVariationSelector(uint32_t codepoint) {
    return (codepoint >= 0xFE00 && codepoint <= 0xFE0F) || (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
}

static bool isPartOfGraphemeCluster(uint32_t current, uint32_t previous, uint32_t previousFlags) {
    if (isCombiningChar(current)) {
        return true;
    }

    if (isZeroWidthJoiner(previous) || isZeroWidthJoiner(current)) {
        return true;
    }

    if (isRegionalIndicator(current) && isRegionalIndicator(previous)) {
        return true;
    }

    if (isEmojiModifier(current) && (previousFlags & kCharacterFlagEmoji) != 0) {
        return true;
    }

    if (isVariationSelector(current)) {
        return true;
    }

    return false;
}

static void resolveUnicodeSequences(const uint32_t* unicodePoints, uint32_t* flags, size_t size) {
    uint32_t previous = 0;
    uint32_t previousFlags = 0;

    const auto* itBegin = unicodePoints;
    const auto* it = itBegin;
    const auto* itEnd = itBegin + size;
    const auto* emojiTrie = getEmojiUnicodeTrie();

    while (it != itEnd) {
        auto current = *it;
        auto currentFlags = *flags;

        if (it != itBegin && isPartOfGraphemeCluster(current, previous, previousFlags)) {
            currentFlags |= kCharacterFromGraphemeCluster;
            *flags = currentFlags;
        }

        const auto* emojiSequence = emojiTrie->find(it, itEnd);

        if (emojiSequence != nullptr) {
            currentFlags |= kCharacterFlagEmoji;
            *flags = currentFlags;

            it++;
            flags++;

            for (size_t i = 1; i < emojiSequence->size(); i++) {
                currentFlags = *flags | (kCharacterFlagEmoji | kCharacterFromGraphemeCluster);
                *flags = currentFlags;

                it++;
                flags++;
            }
        } else {
            it++;
            flags++;
        }

        previous = current;
        previousFlags = currentFlags;
    }
}

struct ProcessedUnicodeString {
    Ref<ByteBuffer> buffer;
};

static ProcessedUnicodeString processUnicodeString(const uint32_t* data,
                                                   size_t length,
                                                   bool normalize,
                                                   bool includeCategorization) {
    auto* unicodeFuncs = getUnicodeFuncs();

    ProcessedUnicodeString output;
    output.buffer = makeShared<ByteBuffer>();

    if (normalize) {
        SmallVector<int32_t, 32> unicodePoints;
        SmallVector<int32_t, 32> flags;
        unicodePoints.reserve(length);
        flags.reserve(length);

        uint32_t prevCodepoint = 0;
        int32_t prevFlags = 0;
        bool hasPrevCodepoint = false;

        for (size_t i = 0; i < length; i++) {
            auto codepoint = data[i];
            int32_t currentFlags = computeCharacterFlags(unicodeFuncs, codepoint);

            if (hasPrevCodepoint) {
                uint32_t composed = 0;
                // If we have a mark, and that we succesfully composed our new unicode point
                if ((currentFlags & kCharacterFlagMaskMarks) != 0 &&
                    hb_unicode_compose(unicodeFuncs, prevCodepoint, codepoint, &composed)) {
                    unicodePoints.emplace_back(static_cast<int32_t>(composed));
                    flags.emplace_back(computeCharacterFlags(unicodeFuncs, composed));
                    hasPrevCodepoint = false;
                } else {
                    unicodePoints.emplace_back(static_cast<int32_t>(prevCodepoint));
                    flags.emplace_back(prevFlags);

                    prevCodepoint = codepoint;
                    prevFlags = currentFlags;
                }
            } else {
                prevCodepoint = codepoint;
                prevFlags = currentFlags;
                hasPrevCodepoint = true;
            }
        }

        if (hasPrevCodepoint) {
            unicodePoints.emplace_back(static_cast<int32_t>(prevCodepoint));
            flags.emplace_back(prevFlags);
        }

        output.buffer->reserve(unicodePoints.size() + flags.size());
        output.buffer->append(reinterpret_cast<const Byte*>(unicodePoints.data()),
                              reinterpret_cast<const Byte*>(unicodePoints.data() + unicodePoints.size()));
        output.buffer->append(reinterpret_cast<const Byte*>(flags.data()),
                              reinterpret_cast<const Byte*>(flags.data() + flags.size()));
    } else {
        output.buffer->reserve(length * 2 * sizeof(uint32_t));
        output.buffer->append(reinterpret_cast<const Byte*>(data), reinterpret_cast<const Byte*>(data + length));
        for (size_t i = 0; i < length; i++) {
            auto codepoint = data[i];
            int32_t currentFlags = includeCategorization ? computeCharacterFlags(unicodeFuncs, codepoint) : 0;
            output.buffer->append(reinterpret_cast<const Byte*>(&currentFlags),
                                  reinterpret_cast<const Byte*>(&currentFlags + 1));
        }
    }

    if (!output.buffer->empty() && includeCategorization) {
        // Resolve emoji sequences and compute grapheme clusters
        const auto* unicodePoints = reinterpret_cast<const uint32_t*>(output.buffer->data());
        auto* flags = reinterpret_cast<uint32_t*>(output.buffer->data() + (output.buffer->size() / 2));
        auto size = output.buffer->size() / 2 / sizeof(uint32_t);
        resolveUnicodeSequences(unicodePoints, flags, size);
    }

    return output;
}

JSValueRef UnicodeModuleFactory::strToCodepoints(const Ref<StaticString>& str,
                                                 bool normalize,
                                                 bool disableCategorization,
                                                 JSFunctionNativeCallContext& callContext) {
    auto utf32Storage = str->utf32Storage();

    auto processedString = processUnicodeString(
        reinterpret_cast<const uint32_t*>(utf32Storage.data), utf32Storage.length, normalize, !disableCategorization);

    auto bufferJs = callContext.getContext().newArrayBuffer(processedString.buffer->toBytesView(),
                                                            callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return bufferJs;
}

static JSValueRef newStringFromUtf32(JSFunctionNativeCallContext& callContext,
                                     const uint32_t* codepoints,
                                     size_t length) {
    auto utf16Length = countUtf32ToUtf16(codepoints, length);
    SmallVector<char16_t, 32> output;
    output.resize(utf16Length);

    utf32ToUtf16(codepoints, length, output.data(), output.size());

    return callContext.getContext().newStringUTF16(std::u16string_view(output.data(), output.size()),
                                                   callContext.getExceptionTracker());
}

JSValueRef UnicodeModuleFactory::codepointsToStr(JSValue codepoints,
                                                 bool normalize,
                                                 bool disableCategorization,
                                                 JSFunctionNativeCallContext& callContext) {
    static auto kStr = STRING_LITERAL("str");
    static auto kBuffer = STRING_LITERAL("buffer");

    auto arrayLength = jsArrayGetLength(callContext.getContext(), codepoints, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto flagsJs = callContext.getContext().newArray(arrayLength, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    SmallVector<uint32_t, 32> allCodepoints;
    allCodepoints.reserve(arrayLength);

    for (size_t i = 0; i < arrayLength; i++) {
        auto codepointJs =
            callContext.getContext().getObjectPropertyForIndex(codepoints, i, callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        auto codepoint = callContext.getContext().valueToInt(codepointJs.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        allCodepoints.emplace_back(static_cast<uint32_t>(codepoint));
    }

    auto processedString =
        processUnicodeString(allCodepoints.data(), allCodepoints.size(), normalize, !disableCategorization);

    auto bufferJs = callContext.getContext().newArrayBuffer(processedString.buffer->toBytesView(),
                                                            callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto jsStr = newStringFromUtf32(callContext,
                                    reinterpret_cast<const uint32_t*>(processedString.buffer->data()),
                                    processedString.buffer->size() / 2 / sizeof(uint32_t));
    CHECK_CALL_CONTEXT(callContext);

    auto bufferKey = callContext.getContext().getPropertyNameCached(kBuffer);
    auto strKey = callContext.getContext().getPropertyNameCached(kStr);

    auto output = callContext.getContext().newObject(callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    callContext.getContext().setObjectProperty(output.get(), strKey, jsStr.get(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    callContext.getContext().setObjectProperty(
        output.get(), bufferKey, bufferJs.get(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return output;
}

static JSValueRef throwInvalidEncoding(JSFunctionNativeCallContext& callContext) {
    return callContext.throwError(Error("Invalid encoding"));
}

JSValueRef UnicodeModuleFactory::encodeString(const Ref<StaticString>& str,
                                              int32_t encoding,
                                              JSFunctionNativeCallContext& callContext) {
    switch (static_cast<TextEncoding>(encoding)) {
        case TextEncoding::UTF8: {
            auto storage = str->utf8Storage();
            return callContext.getContext().newArrayBufferCopy(
                reinterpret_cast<const Byte*>(storage.data), storage.length, callContext.getExceptionTracker());
        }
        case TextEncoding::UTF16: {
            auto storage = str->utf16Storage();
            return callContext.getContext().newArrayBufferCopy(
                reinterpret_cast<const Byte*>(storage.data), storage.length * 2, callContext.getExceptionTracker());
        }
        case TextEncoding::UTF32: {
            auto storage = str->utf32Storage();
            return callContext.getContext().newArrayBufferCopy(
                reinterpret_cast<const Byte*>(storage.data), storage.length * 4, callContext.getExceptionTracker());
        }
        default:
            return throwInvalidEncoding(callContext);
    }
}

JSValueRef UnicodeModuleFactory::decodeIntoString(const JSTypedArray& buffer,
                                                  int32_t encoding,
                                                  JSFunctionNativeCallContext& callContext) {
    switch (static_cast<TextEncoding>(encoding)) {
        case TextEncoding::UTF8:
            return callContext.getContext().newStringUTF8(
                std::string_view(reinterpret_cast<const char*>(buffer.data), buffer.length),
                callContext.getExceptionTracker());
        case TextEncoding::UTF16:
            return callContext.getContext().newStringUTF16(
                std::u16string_view(reinterpret_cast<const char16_t*>(buffer.data), buffer.length / 2),
                callContext.getExceptionTracker());
        case TextEncoding::UTF32: {
            auto storage = utf32ToUtf8(reinterpret_cast<const uint32_t*>(buffer.data), buffer.length / 4);
            return callContext.getContext().newStringUTF8(std::string_view(storage.first, storage.second),
                                                          callContext.getExceptionTracker());
        }
        default:
            return throwInvalidEncoding(callContext);
    }
}

JSValueRef UnicodeModuleFactory::loadModule(IJavaScriptContext& jsContext,
                                            const ReferenceInfoBuilder& /*referenceInfoBuilder*/,
                                            JSExceptionTracker& exceptionTracker) {
    auto definition =
        JSNativeClassBinder<UnicodeModuleFactory>("UnicodeNative")
            .bindClassMethod<&UnicodeModuleFactory::strToCodepoints>("strToCodepoints", true, true, true)
            .bindClassMethod<&UnicodeModuleFactory::codepointsToStr>("codepointsToStr", true, true, true)
            .bindClassMethod<&UnicodeModuleFactory::encodeString>("encodeString", true, true, true)
            .bindClassMethod<&UnicodeModuleFactory::decodeIntoString>("decodeIntoString", true, true, true)
            .extractClassDefinition();

    return jsContext.newNativeClass(nullptr, definition, exceptionTracker);
}

} // namespace Valdi
