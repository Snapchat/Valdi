#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

TEST(JavaScriptANRAttribution, restoresEveryScopeAfterOverflow) {
    JavaScriptANRAttribution attribution;
    std::vector<JavaScriptANRAttribution::RestoreToken> tokens;
    std::vector<std::string> previousReports;

    for (size_t i = 0; i < 40; ++i) {
        previousReports.emplace_back(attribution.format());
        tokens.emplace_back(attribution.push(StringBox::fromString("frame" + std::to_string(i))));
    }

    auto report = attribution.format();
    EXPECT_NE(std::string::npos, report.find("[stuck-in: frame39]"));
    EXPECT_NE(std::string::npos, report.find("[attribution: frame0 -> ... (24 frames omitted) -> frame25"));
    EXPECT_EQ(std::string::npos, report.find(" -> frame24"));

    for (size_t i = tokens.size(); i > 0; --i) {
        attribution.pop(std::move(tokens[i - 1]));
        EXPECT_EQ(previousReports[i - 1], attribution.format());
    }
    EXPECT_TRUE(attribution.empty());
}

TEST(JavaScriptANRAttribution, snapshotsPreserveOriginWithoutRetainingUnboundedAncestry) {
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("Product.load"));
    auto parent = attribution;
    for (size_t i = 0; i < 1000; ++i) {
        auto child = attribution;
        child.push(StringBox::fromString("child" + std::to_string(i)));
        attribution = std::move(child);
    }

    auto report = attribution.format();
    EXPECT_EQ(" [stuck-in: Product.load]", parent.format());
    EXPECT_NE(std::string::npos, report.find("[stuck-in: child999]"));
    EXPECT_NE(std::string::npos, report.find("[attribution: Product.load -> ... (985 frames omitted) -> child985"));
    EXPECT_LT(report.size(), 1024u);
}

TEST(JavaScriptANRAttribution, boundsLabelBytesBeforeRetainingSnapshots) {
    JavaScriptANRAttribution attribution;
    auto label = StringBox::fromString(std::string(10000, 'x'));
    auto token = attribution.push(label);
    auto expected = std::string(125, 'x') + "...";
    EXPECT_EQ(" [stuck-in: " + expected + "]", attribution.format());
    attribution.pop(std::move(token));

    auto exact = StringBox::fromString(std::string(128, 'y'));
    attribution.push(exact);
    EXPECT_EQ(" [stuck-in: " + exact.slowToString() + "]", attribution.format());
}

TEST(JavaScriptANRAttribution, truncatesAtUtf8Boundaries) {
    JavaScriptANRAttribution attribution;
    auto prefix = std::string(124, 'x');
    attribution.push(StringBox::fromString(prefix + "\xE2\x82\xAC" + "extra"));
    EXPECT_EQ(" [stuck-in: " + prefix + "...]", attribution.format());
}

TEST(JavaScriptANRAttribution, emptyLabelsDoNotEraseParents) {
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("Product.load"));
    auto token = attribution.push(StringBox());
    EXPECT_EQ(" [stuck-in: Product.load]", attribution.format());
    attribution.pop(std::move(token));
    EXPECT_EQ(" [stuck-in: Product.load]", attribution.format());
}

TEST(JavaScriptANRAttribution, boundsReportSizeWithLongFramesAndOverflow) {
    JavaScriptANRAttribution attribution;
    for (size_t i = 0; i < 1000; ++i) {
        attribution.push(StringBox::fromString(std::string(10000, i % 2 == 0 ? 'x' : 'y')));
    }
    auto report = attribution.format();
    EXPECT_NE(std::string::npos, report.find("984 frames omitted"));
    EXPECT_LT(report.size(), 2300u);
}

} // namespace ValdiTest
