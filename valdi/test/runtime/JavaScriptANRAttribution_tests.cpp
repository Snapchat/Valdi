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

TEST(JavaScriptANRAttribution, reportsHowLongEachFrameHasBeenOpen) {
    JavaScriptANRAttribution::TimePoint start(std::chrono::seconds(100));
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("createRoot(Profile)"), start);
    attribution.push(StringBox::fromCString("runtime.loadJsModule(profile/src/Actions)"),
                     start + std::chrono::milliseconds(4826));
    attribution.push(StringBox::fromCString("Strings.getLocalizedString"), start + std::chrono::milliseconds(4867));

    EXPECT_EQ(" [stuck-in: Strings.getLocalizedString] [attribution: createRoot(Profile) (4870 ms) -> "
              "runtime.loadJsModule(profile/src/Actions) (44 ms) -> Strings.getLocalizedString (3 ms)]",
              attribution.format(start + std::chrono::milliseconds(4870)));
}

TEST(JavaScriptANRAttribution, reportsTheDurationOfALoneFrame) {
    JavaScriptANRAttribution::TimePoint start(std::chrono::seconds(100));
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("renderComponent(SearchBody)"), start);

    EXPECT_EQ(" [stuck-in: renderComponent(SearchBody)] [attribution: renderComponent(SearchBody) (180 ms)]",
              attribution.format(start + std::chrono::milliseconds(180)));
    EXPECT_EQ(" [stuck-in: renderComponent(SearchBody)]", attribution.format());
}

TEST(JavaScriptANRAttribution, omitsDurationsForFramesWithoutStartTimes) {
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("A"));
    attribution.push(StringBox::fromCString("B"));

    EXPECT_EQ(" [stuck-in: B] [attribution: A -> B]",
              attribution.format(JavaScriptANRAttribution::TimePoint(std::chrono::seconds(100))));
}

TEST(JavaScriptANRAttribution, restampsInheritedFrames) {
    JavaScriptANRAttribution::TimePoint dispatchTime(std::chrono::seconds(100));
    JavaScriptANRAttribution attribution;
    attribution.push(StringBox::fromCString("A"), dispatchTime);
    attribution.push(StringBox::fromCString("B"), dispatchTime);

    auto taskStart = dispatchTime + std::chrono::seconds(4);
    attribution.setStartTime(taskStart);

    EXPECT_EQ(" [stuck-in: B] [attribution: A (20 ms) -> B (20 ms)]",
              attribution.format(taskStart + std::chrono::milliseconds(20)));
}

TEST(JavaScriptANRAttribution, keepsStartTimesThroughOverflow) {
    JavaScriptANRAttribution::TimePoint start(std::chrono::seconds(100));
    JavaScriptANRAttribution attribution;
    std::vector<JavaScriptANRAttribution::RestoreToken> tokens;
    for (int i = 0; i < 20; ++i) {
        tokens.emplace_back(
            attribution.push(StringBox::fromString("frame" + std::to_string(i)), start + std::chrono::milliseconds(i)));
    }
    // Popping the overflowing frames brings back the displaced ones with their own start times.
    for (int i = 19; i >= 16; --i) {
        attribution.pop(std::move(tokens[i]));
    }

    auto report = attribution.format(start + std::chrono::milliseconds(100));
    EXPECT_NE(std::string::npos, report.find("[stuck-in: frame15]"));
    EXPECT_NE(std::string::npos, report.find("[attribution: frame0 (100 ms) -> frame1 (99 ms) -> frame2 (98 ms)"));
    EXPECT_EQ(std::string::npos, report.find("omitted"));
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
