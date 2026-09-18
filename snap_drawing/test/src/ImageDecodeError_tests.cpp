#include <cstring>
#include <gtest/gtest.h>
#include <string>

#include "snap_drawing/cpp/Utils/BytesUtils.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"

namespace snap::drawing {

static Valdi::BytesView bytesFrom(const char* value, size_t size) {
    return Valdi::BytesView(nullptr, reinterpret_cast<const Valdi::Byte*>(value), size);
}

static Valdi::BytesView bytesFromString(const char* value) {
    return bytesFrom(value, strlen(value));
}

static std::string errorFor(const Valdi::BytesView& payload) {
    auto image = Image::make(payload);
    EXPECT_FALSE(image);
    return image.error().toString();
}

static std::string errorFor(const char* payload) {
    return errorFor(bytesFromString(payload));
}

TEST(ImageDecodeError, reportsLengthAndMagicBytesForUnknownBinary) {
    const char payload[] = "\x89not-a-known-container-at-all";
    const auto message = errorFor(payload);

    EXPECT_NE(std::string::npos, message.find("Unable to decode image")) << message;
    EXPECT_NE(std::string::npos, message.find("bytes=" + std::to_string(strlen(payload)))) << message;
    EXPECT_NE(std::string::npos, message.find("magic=89")) << message;
}

TEST(ImageDecodeError, capsMagicBytesAtTwelve) {
    const auto message = errorFor("0123456789abcdefghijklmnop");

    EXPECT_NE(std::string::npos, message.find("magic=303132333435363738396162")) << message;
}

TEST(ImageDecodeError, classifiesHtmlBody) {
    EXPECT_NE(std::string::npos,
              errorFor("<!DOCTYPE html><html><body>blocked</body></html>").find("Non-image HTML body"));
    EXPECT_NE(std::string::npos, errorFor("<html><body>blocked</body></html>").find("Non-image HTML body"));
    EXPECT_NE(std::string::npos, errorFor("  \n<!doctype HTML><html></html>").find("Non-image HTML body"));
}

TEST(ImageDecodeError, classifiesHttpResponseBody) {
    const auto message = errorFor("HTTP/1.1 403 Forbidden\r\n\r\n");

    EXPECT_NE(std::string::npos, message.find("Non-image HTTP response body")) << message;
}

TEST(ImageDecodeError, classifiesJsonBody) {
    const auto message = errorFor("{\"error\":\"unavailable\"}");

    EXPECT_NE(std::string::npos, message.find("Non-image JSON body")) << message;
}

TEST(ImageDecodeError, keepsFallbackLabelForTruncatedPng) {
    const char truncatedPng[] = "\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR";
    const auto size = sizeof(truncatedPng) - 1;
    const auto message = errorFor(bytesFrom(truncatedPng, size));

    EXPECT_NE(std::string::npos, message.find("Unable to decode image")) << message;
    EXPECT_NE(std::string::npos, message.find("bytes=" + std::to_string(size))) << message;
    EXPECT_NE(std::string::npos, message.find("magic=89504e470d0a1a0a0000000d")) << message;
}

TEST(ImageDecodeError, doesNotClassifyXmlDeclarationAsHtml) {
    auto image = Image::make(bytesFromString("<?xml version=\"1.0\"?><svg"));

    ASSERT_FALSE(image);
    EXPECT_EQ(std::string::npos, image.error().toString().find("Non-image HTML body"));
}

TEST(ImageDecodeError, describesEmptyPayloadWithNullData) {
    const auto message = describeUndecodablePayload(Valdi::BytesView(), "Unable to decode image");

    EXPECT_NE(std::string::npos, message.find("Empty payload")) << message;
    EXPECT_NE(std::string::npos, message.find("bytes=0")) << message;
}

TEST(ImageDecodeError, describesEmptyPayloadWithNonNullData) {
    const char storage[] = "unused";
    const auto message = describeUndecodablePayload(bytesFrom(storage, 0), "Unable to decode image");

    EXPECT_NE(std::string::npos, message.find("Empty payload")) << message;
    EXPECT_NE(std::string::npos, message.find("bytes=0 magic=)")) << message;
}

TEST(ImageDecodeError, classifiesEmptyPayloadThroughImageMake) {
    const char storage[] = "unused";
    const auto message = errorFor(bytesFrom(storage, 0));

    EXPECT_NE(std::string::npos, message.find("Empty payload")) << message;
}

TEST(PayloadDescription, reportsByteCountAndLowercaseHex) {
    EXPECT_EQ("bytes=2 magic=abcd", describePayloadBytes(bytesFromString("\xAB\xCD")));
}

TEST(PayloadDescription, capsMagicAtTwelveBytes) {
    EXPECT_EQ("bytes=20 magic=303132333435363738396162", describePayloadBytes(bytesFromString("0123456789abcdefghij")));
}

TEST(PayloadDescription, reportsShortPayloadInFull) {
    EXPECT_EQ("bytes=3 magic=414243", describePayloadBytes(bytesFromString("ABC")));
}

TEST(PayloadDescription, reportsEmptyPayloadWithNoMagic) {
    EXPECT_EQ("bytes=0 magic=", describePayloadBytes(Valdi::BytesView()));

    const char storage[] = "unused";
    EXPECT_EQ("bytes=0 magic=", describePayloadBytes(bytesFrom(storage, 0)));
}

} // namespace snap::drawing
