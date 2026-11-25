#include "valdi/runtime/JavaScript/JavaScriptANRSampleTree.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

/** One sampled stack: its time in ms, and its labels written "a -> b" ("" for no breadcrumb). */
using StackSpec = std::pair<int, const char*>;

std::vector<JavaScriptANRSampledStack> makeStacks(const std::vector<StackSpec>& specs) {
    std::vector<JavaScriptANRSampledStack> stacks;
    for (const auto& [ms, text] : specs) {
        auto& stack = stacks.emplace_back();
        stack.time = std::chrono::milliseconds(ms);
        std::string rest = text;
        while (!rest.empty()) {
            auto separator = rest.find(" -> ");
            stack.labels.emplace_back(StringBox::fromString(rest.substr(0, separator)));
            rest = separator == std::string::npos ? std::string() : rest.substr(separator + 4);
        }
    }
    return stacks;
}

std::string treeOf(const std::vector<StackSpec>& specs, int anrWindowMs = 5000) {
    return formatSampleTree(buildSampleTree(makeStacks(specs)), std::chrono::milliseconds(anrWindowMs));
}

} // namespace

TEST(JavaScriptANRSampleTree, showsTheLongScopeAndTheCallsThatHeldAFifth) {
    EXPECT_EQ("\nSampled 5000 ms of the 5000 ms ANR window"
              "\n  5000 ms  createRoot(Profile)"
              "\n  1400 ms    runtime.loadJsModule(profile/src/Actions)",
              treeOf({
                  {3500, "createRoot(Profile)"},
                  {1400, "createRoot(Profile) -> runtime.loadJsModule(profile/src/Actions)"},
                  {100, "createRoot(Profile) -> valdi_core/src/Strings.<r>.getLocalizedString"},
              }));
}

TEST(JavaScriptANRSampleTree, showsTheBacklogEvenWhenAnotherTaskRunsLast) {
    EXPECT_EQ("\nSampled 5000 ms of the 5000 ms ANR window"
              "\n  4050 ms  friendsObservable.subscribe.<p0>",
              treeOf({
                  {4050, "friendsObservable.subscribe.<p0>"},
                  {700, "runtime.scheduleWorkItem"},
                  {250, "renderComponent(SearchBody)"},
              }));
}

TEST(JavaScriptANRSampleTree, printsAFrameThatLeadsIntoOneFrameForNearlyAllOfItsTimeOnItsLine) {
    EXPECT_EQ("\nSampled 1000 ms of the 5000 ms ANR window"
              "\n  1000 ms  request"
              "\n   600 ms    runtime.trace -> render"
              "\n   400 ms    runtime.loadJsModule",
              treeOf({
                  {50, "request -> runtime.trace"},
                  {550, "request -> runtime.trace -> render"},
                  {400, "request -> runtime.loadJsModule"},
              }));
}

TEST(JavaScriptANRSampleTree, keepsAFrameOnItsOwnLineWhenItsOnlyBranchHoldsLessThanNinetyPercent) {
    EXPECT_EQ("\nSampled 1000 ms of the 5000 ms ANR window"
              "\n  1000 ms  request"
              "\n   800 ms    render",
              treeOf({{200, "request"}, {800, "request -> render"}}));
}

TEST(JavaScriptANRSampleTree, showsSplitWorkAsSiblingsBusiestFirst) {
    EXPECT_EQ("\nSampled 5000 ms of the 5000 ms ANR window"
              "\n  1700 ms  MixA"
              "\n  1650 ms  MixB"
              "\n  1650 ms  MixC",
              treeOf({{1650, "MixC"}, {1700, "MixA"}, {1650, "MixB"}}));
}

TEST(JavaScriptANRSampleTree, listsWorkOutsideAnyBreadcrumbAsUnattributed) {
    EXPECT_EQ("\nSampled 5000 ms of the 5000 ms ANR window"
              "\n  3500 ms  unattributed"
              "\n  1500 ms  renderComponent(Chat)",
              treeOf({{3500, ""}, {1500, "renderComponent(Chat)"}}));
}

TEST(JavaScriptANRSampleTree, saysSoWhenNoBranchHeldAFifth) {
    EXPECT_EQ("\nSampled 1000 ms of the 5000 ms ANR window"
              "\n  no branch held 20% of it",
              treeOf({{170, "A"}, {170, "B"}, {170, "C"}, {170, "D"}, {160, "E"}, {160, "F"}}));
}

TEST(JavaScriptANRSampleTree, breaksTiesOnTheLabel) {
    EXPECT_EQ("\nSampled 500 ms of the 5000 ms ANR window"
              "\n  250 ms  A"
              "\n  250 ms  B",
              treeOf({{250, "B"}, {250, "A"}}));
}

TEST(JavaScriptANRSampleTree, printsAtMostTheLineLimit) {
    JavaScriptANRSampleTree tree;
    tree.sampledTime = std::chrono::milliseconds(1000);
    for (size_t i = 0; i < kMaxSampleTreeLines + 2; ++i) {
        auto& branch = tree.branches.emplace_back();
        branch.labels.emplace_back(StringBox::fromString("scope" + std::to_string(i)));
        branch.time = std::chrono::milliseconds(200);
    }

    auto formatted = formatSampleTree(tree, std::chrono::milliseconds(5000));

    EXPECT_NE(std::string::npos, formatted.find("scope" + std::to_string(kMaxSampleTreeLines - 1)));
    EXPECT_EQ(std::string::npos, formatted.find("scope" + std::to_string(kMaxSampleTreeLines)));
}

namespace {

struct DeviceReport {
    const char* name;
    int anrWindowMs;
    std::vector<StackSpec> stacks;
    const char* expectedTree;
};

// Sampled stacks from ANR reports a Pixel 7 logged while cold-opening a public profile: 2 at a 150 ms threshold,
// and 28 at 300 ms with every core loaded. Feature labels are renamed; runtime and valdi_core labels and each
// stack's shape are as logged. Expected trees were computed separately from the logs.
const std::vector<DeviceReport>& deviceReports() {
    static const std::vector<DeviceReport> kReports = {
        // clang-format off
        {"report 1", 150,
         {
             {20, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call1"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module3)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module4)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call2"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module1)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module2)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/observable/fromSubscribable)"},
         },
         "\nSampled 100 ms of the 150 ms ANR window"
             "\n  100 ms  <anon>.networkClient.request.<p1>"
             "\n   60 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   30 ms      runtime.loadJsModule"
             "\n   20 ms    runtime.loadJsModule"},
        {"report 2", 150,
         {
             {30, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module3)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module5)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module6)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.withCallbackAttribution -> feature.call4"},
         },
         "\nSampled 100 ms of the 150 ms ANR window"
             "\n  100 ms  <anon>.networkClient.request.<p1>"
             "\n   60 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   20 ms      runtime.loadJsModule"
             "\n   20 ms    runtime.loadJsModule"},
        {"report 3", 300,
         {
             {30, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {20, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {11, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call6"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module18)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module20)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module21)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module8)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module9)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call2"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call5"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module10)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module11)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module12)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module13)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module14)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module15)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module16)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module17)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module19)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/observable/forkJoin)"},
         },
         "\nSampled 251 ms of the 300 ms ANR window"
             "\n  251 ms  <anon>.networkClient.request.<p1>"
             "\n  141 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n  100 ms      runtime.loadJsModule"
             "\n   80 ms    runtime.loadJsModule"},
        {"report 4", 300,
         {
             {40, ""},
             {23, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {22, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module22)"},
             {20, "runtime.trace -> createRoot.Profile"},
             {20, "runtime.trace -> createRoot.Profile -> feature.call3"},
             {12, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module22) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module23)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call11"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call7"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module26)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module29)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module30) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module31)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module32) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module33)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8 -> runtime.withCallbackAttribution -> feature.call9 -> feature.call10"},
             {10, "runtime.trace -> createRoot.Profile -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 247 ms of the 300 ms ANR window"
             "\n  207 ms  runtime.trace -> createRoot.Profile"
             "\n  127 ms    runtime.loadJsModule"},
        {"report 5", 300,
         {
             {51, "runtime.trace -> createRoot.Profile"},
             {30, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {20, "runtime.trace -> createRoot.Profile -> feature.call12"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module34)"},
             {13, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module35)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call11"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call3"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call6"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module42)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module38)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module40)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call13 -> runtime.withCallbackAttribution -> feature.call14 -> feature.call10"},
         },
         "\nSampled 254 ms of the 300 ms ANR window"
             "\n  254 ms  runtime.trace -> createRoot.Profile"
             "\n  133 ms    runtime.loadJsModule"},
        {"report 6", 300,
         {
             {50, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {30, "<anon>.networkClient.request.<p1> -> runtime.withCallbackAttribution -> feature.call16 -> runtime.withCallbackAttribution -> feature.call17 -> runtime.withCallbackAttribution -> feature.call18 -> feature.call10"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {11, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call19"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module18)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module3)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module45)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module47)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module48)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module49)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/index)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace"},
         },
         "\nSampled 251 ms of the 300 ms ANR window"
             "\n  251 ms  <anon>.networkClient.request.<p1>"
             "\n  160 ms    runtime.loadJsModule"},
        {"report 7", 300,
         {
             {251, ""},
         },
         "\nSampled 251 ms of the 300 ms ANR window"
             "\n  251 ms  unattributed"},
        {"report 8", 300,
         {
             {97, ""},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call7"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.configureColorPalette"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module53)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module32)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module40)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module51) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module52)"},
         },
         "\nSampled 257 ms of the 300 ms ANR window"
             "\n  160 ms  runtime.trace -> createRoot.Profile"
             "\n  140 ms    runtime.loadJsModule"
             "\n   97 ms  unattributed"},
        {"report 9", 300,
         {
             {112, ""},
             {23, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {11, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module54)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module55) -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module56)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module57)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/utils/PromiseUtils)"},
         },
         "\nSampled 257 ms of the 300 ms ANR window"
             "\n  145 ms  runtime.trace -> createRoot.Profile -> runtime.loadJsModule"
             "\n  112 ms  unattributed"},
        {"report 10", 300,
         {
             {102, ""},
             {30, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {12, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module53)"},
             {10, "runtime.trace -> createRoot.Profile"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.configureColorPalette"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module59) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module60)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module29)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module51) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module52)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module54)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module55) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module58)"},
         },
         "\nSampled 254 ms of the 300 ms ANR window"
             "\n  152 ms  runtime.trace -> createRoot.Profile -> runtime.loadJsModule"
             "\n  102 ms  unattributed"},
        {"report 11", 300,
         {
             {132, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43)"},
             {61, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module61)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.withCallbackAttribution -> feature.call16 -> runtime.withCallbackAttribution -> feature.call17 -> runtime.withCallbackAttribution -> feature.call18"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.withCallbackAttribution -> feature.call16 -> runtime.withCallbackAttribution -> feature.call17 -> runtime.withCallbackAttribution -> feature.call18 -> feature.call10"},
         },
         "\nSampled 253 ms of the 300 ms ANR window"
             "\n  253 ms  <anon>.networkClient.request.<p1>"
             "\n  213 ms    runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43)"
             "\n   61 ms      valdi_core/src/Strings.<r>.getLocalizedString"},
        {"report 12", 300,
         {
             {217, ""},
             {10, "feature.call1"},
             {10, "runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module62)"},
             {10, "runtime.trace -> renderComponent.FeedGrid"},
             {10, "runtime.trace -> renderComponent.FeedGrid -> runtime.loadJsModule"},
         },
         "\nSampled 257 ms of the 300 ms ANR window"
             "\n  217 ms  unattributed"},
        {"report 13", 300,
         {
             {50, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {41, "runtime.trace -> createRoot.Profile"},
             {40, ""},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module64)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module55) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module58)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module56) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module63)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call13 -> runtime.withCallbackAttribution -> feature.call14 -> feature.call10"},
         },
         "\nSampled 252 ms of the 300 ms ANR window"
             "\n  212 ms  runtime.trace -> createRoot.Profile"
             "\n  151 ms    runtime.loadJsModule"},
        {"report 14", 300,
         {
             {30, "<anon>.networkClient.request.<p1>"},
             {30, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module9)"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module18)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module3)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module65)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module66)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module69)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module70)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module71)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/index)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call2"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module2)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module67) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module68)"},
         },
         "\nSampled 250 ms of the 300 ms ANR window"
             "\n  250 ms  <anon>.networkClient.request.<p1>"
             "\n  160 ms    runtime.loadJsModule"
             "\n   50 ms    runtime.trace -> renderComponent.ProfileCore"},
        {"report 15", 300,
         {
             {30, ""},
             {24, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {20, "runtime.trace -> createRoot.Profile -> feature.call1"},
             {10, "runtime.trace -> createRoot.Profile"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module42)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module26)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module53)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module29)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module34)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module55) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module58)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module72)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module73)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module74)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module75)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module76)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call13 -> runtime.withCallbackAttribution -> feature.call14 -> feature.call10"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8"},
         },
         "\nSampled 254 ms of the 300 ms ANR window"
             "\n  224 ms  runtime.trace -> createRoot.Profile"
             "\n  174 ms    runtime.loadJsModule"},
        {"report 16", 300,
         {
             {59, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {30, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call20"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module21)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module45)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module71)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module77)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module78)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module80)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.submitRawRenderRequest -> runtime.submitRawRenderRequest.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call5"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module17)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module6)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module79)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/utils/NumberUtils)"},
         },
         "\nSampled 259 ms of the 300 ms ANR window"
             "\n  259 ms  <anon>.networkClient.request.<p1>"
             "\n  139 ms    runtime.loadJsModule"
             "\n   59 ms      runtime.loadJsModule(feature/src/Module7)"
             "\n   60 ms    runtime.trace -> renderComponent.ProfileCore"},
        {"report 17", 300,
         {
             {40, ""},
             {40, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {30, "runtime.trace -> createRoot.Profile"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28)"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.configureColorPalette"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module32) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module33)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module34)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module81)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module82)"},
         },
         "\nSampled 251 ms of the 300 ms ANR window"
             "\n  211 ms  runtime.trace -> createRoot.Profile"
             "\n  181 ms    runtime.loadJsModule"},
        {"report 18", 300,
         {
             {190, ""},
             {30, "runtime.trace -> renderComponent.FeedGrid"},
             {10, "runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module83)"},
             {10, "runtime.trace -> renderComponent.FeedContainer"},
             {10, "runtime.trace -> renderComponent.FeedGrid -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module84) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module85)"},
         },
         "\nSampled 250 ms of the 300 ms ANR window"
             "\n  190 ms  unattributed"
             "\n   50 ms  runtime.trace"},
        {"report 19", 300,
         {
             {41, "runtime.trace -> createRoot.Profile"},
             {31, "runtime.trace -> createRoot.Profile -> feature.call3"},
             {20, ""},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {12, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module26)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module59) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module60)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module29)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module51)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module86)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module87)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/Asset)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call13 -> runtime.withCallbackAttribution -> feature.call14 -> feature.call10"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8 -> runtime.withCallbackAttribution -> feature.call9 -> feature.call10"},
             {10, "runtime.trace -> createRoot.Profile -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 254 ms of the 300 ms ANR window"
             "\n  234 ms  runtime.trace -> createRoot.Profile"
             "\n  122 ms    runtime.loadJsModule"},
        {"report 20", 300,
         {
             {40, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule"},
             {39, "<anon>.networkClient.request.<p1>"},
             {26, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module88)"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module9)"},
             {14, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call21"},
             {13, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {11, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module89)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module90)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module92)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module14)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module19)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module91)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/utils/NumberUtils)"},
         },
         "\nSampled 263 ms of the 300 ms ANR window"
             "\n  263 ms  <anon>.networkClient.request.<p1>"
             "\n  121 ms    runtime.loadJsModule"
             "\n   90 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   66 ms      runtime.loadJsModule"},
        {"report 21", 300,
         {
             {73, "runtime.trace -> createRoot.Profile"},
             {60, ""},
             {10, "runtime.trace -> createRoot.Profile -> feature.call12"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call7"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module38)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module57)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module81)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module93)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8"},
         },
         "\nSampled 253 ms of the 300 ms ANR window"
             "\n  193 ms  runtime.trace -> createRoot.Profile"
             "\n   90 ms    runtime.loadJsModule"
             "\n   60 ms  unattributed"},
        {"report 22", 300,
         {
             {30, "runtime.trace -> createRoot.Profile"},
             {20, ""},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {13, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module22) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module23)"},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module87)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call11"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call12"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call3"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module42)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module26)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.configureColorPalette"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module96)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module35)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module64)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module86)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module94) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module95) -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/AssetCatalog)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module98)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8 -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module97)"},
         },
         "\nSampled 254 ms of the 300 ms ANR window"
             "\n  234 ms  runtime.trace -> createRoot.Profile"
             "\n  164 ms    runtime.loadJsModule"},
        {"report 23", 300,
         {
             {20, "<anon>.networkClient.request.<p1>"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call1"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call20"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module101)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module3)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module43)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module8)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module90)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module99)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call2"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module100)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module15)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module17)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module67) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module68)"},
         },
         "\nSampled 250 ms of the 300 ms ANR window"
             "\n  250 ms  <anon>.networkClient.request.<p1>"
             "\n  140 ms    runtime.loadJsModule"
             "\n   60 ms    runtime.trace -> renderComponent.ProfileCore"},
        {"report 24", 300,
         {
             {62, "runtime.trace -> createRoot.Profile"},
             {20, ""},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call3"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call7"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module103)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module104)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module102)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module35)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module37)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module64)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module55) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module58)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/Asset)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8 -> runtime.withCallbackAttribution -> feature.call9 -> feature.call10"},
             {10, "runtime.trace -> createRoot.Profile -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 252 ms of the 300 ms ANR window"
             "\n  232 ms  runtime.trace -> createRoot.Profile"
             "\n  120 ms    runtime.loadJsModule"},
        {"report 25", 300,
         {
             {32, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {20, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module106)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module70)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module9)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module99)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/index)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.submitRawRenderRequest"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call1"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.getAttributeId"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module1)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module100)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module105)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module15)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module16)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module19)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module5)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module88)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/observable/merge)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "<anon>.networkClient.request.<p1> -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 252 ms of the 300 ms ANR window"
             "\n  252 ms  <anon>.networkClient.request.<p1>"
             "\n  152 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   90 ms      runtime.loadJsModule"
             "\n   60 ms    runtime.loadJsModule"},
        {"report 26", 300,
         {
             {51, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {30, ""},
             {30, "runtime.trace -> createRoot.Profile"},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module39) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call1"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call15"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module107)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module109)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module110)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module96)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module108)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module56)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/operators/tap)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call13 -> runtime.withCallbackAttribution -> feature.call14 -> feature.call10"},
         },
         "\nSampled 252 ms of the 300 ms ANR window"
             "\n  222 ms  runtime.trace -> createRoot.Profile"
             "\n  162 ms    runtime.loadJsModule"
             "\n   51 ms      runtime.loadJsModule(feature/src/Module24)"},
        {"report 27", 300,
         {
             {71, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {30, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {20, "<anon>.networkClient.request.<p1> -> feature.call3"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call6"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module115)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46) -> runtime.loadModule"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module70)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module89)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module111)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module112)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module113)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module114)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module116)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module16)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> valdi_core/src/DeviceBridge.<r>.getDisplayHeight"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 251 ms of the 300 ms ANR window"
             "\n  251 ms  <anon>.networkClient.request.<p1>"
             "\n  151 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   60 ms      runtime.loadJsModule"
             "\n   70 ms    runtime.loadJsModule"},
        {"report 28", 300,
         {
             {82, ""},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule"},
             {20, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module117)"},
             {10, "runtime.trace -> createRoot.Profile"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call12"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module107)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module118)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module119)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module28) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module50)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> valdi_core/src/DeviceBridge.<r>.getDisplayTopInset"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module57)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module75)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_core/src/Asset)"},
             {10, "runtime.trace -> createRoot.Profile -> valdi_core/src/Strings.<r>.getLocalizedString"},
         },
         "\nSampled 253 ms of the 300 ms ANR window"
             "\n  171 ms  runtime.trace -> createRoot.Profile"
             "\n  141 ms    runtime.loadJsModule"
             "\n   82 ms  unattributed"},
        {"report 29", 300,
         {
             {61, "runtime.trace -> createRoot.Profile"},
             {31, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module24)"},
             {20, ""},
             {11, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module54)"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call12"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call22"},
             {10, "runtime.trace -> createRoot.Profile -> feature.call7"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module118)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module22) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module23)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module25) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module42)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module27) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module53)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module36) -> valdi_core/src/Strings.<r>.getLocalizedString"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module41)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/observable/combineLatest)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/operators/pairwise)"},
             {10, "runtime.trace -> createRoot.Profile -> runtime.withCallbackAttribution -> feature.call8 -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module120) -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module121)"},
         },
         "\nSampled 253 ms of the 300 ms ANR window"
             "\n  233 ms  runtime.trace -> createRoot.Profile"
             "\n  132 ms    runtime.loadJsModule"},
        {"report 30", 300,
         {
             {50, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore"},
             {20, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module7)"},
             {10, "<anon>.networkClient.request.<p1>"},
             {10, "<anon>.networkClient.request.<p1> -> feature.call6"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module122)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module123)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module44)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module46)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module66)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module9)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/index)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call2"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call23"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> feature.call24"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module105)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module14)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module15)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(feature/src/Module79)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> runtime.loadJsModule -> runtime.loadJsModule(valdi_rxjs/src/util/isObservable)"},
             {10, "<anon>.networkClient.request.<p1> -> runtime.trace -> renderComponent.ProfileCore -> valdi_core/src/DeviceBridge.<r>.getDisplayHeight"},
         },
         "\nSampled 250 ms of the 300 ms ANR window"
             "\n  250 ms  <anon>.networkClient.request.<p1>"
             "\n  140 ms    runtime.trace -> renderComponent.ProfileCore"
             "\n   50 ms      runtime.loadJsModule"
             "\n   90 ms    runtime.loadJsModule"},
        // clang-format on
    };
    return kReports;
}

} // namespace

TEST(JavaScriptANRSampleTree, buildsTheExpectedTreeForDeviceReports) {
    for (const auto& report : deviceReports()) {
        SCOPED_TRACE(report.name);
        EXPECT_EQ(report.expectedTree, treeOf(report.stacks, report.anrWindowMs));
    }
}

} // namespace ValdiTest
