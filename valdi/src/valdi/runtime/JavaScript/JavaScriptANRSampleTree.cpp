#include "valdi/runtime/JavaScript/JavaScriptANRSampleTree.hpp"

#include <algorithm>

namespace Valdi {

namespace {

using Duration = std::chrono::steady_clock::duration;

struct StackNode {
    StringBox label;
    Duration time{};
    // Time sampled in this scope but outside all of its child scopes.
    Duration ownTime{};
    std::vector<StackNode> children;

    StackNode& child(const StringBox& childLabel) {
        for (auto& existing : children) {
            if (existing.label == childLabel) {
                return existing;
            }
        }
        auto& added = children.emplace_back();
        added.label = childLabel;
        return added;
    }
};

// A branch below this share of the sampled time is one or two samples at the default rate, which is noise.
bool isSignificant(Duration time, Duration total) {
    return time * 5 >= total;
}

bool isBusier(const JavaScriptANRSampleTreeNode& lhs, const JavaScriptANRSampleTreeNode& rhs) {
    if (lhs.time != rhs.time) {
        return lhs.time > rhs.time;
    }
    // Ties go to the smaller label so the report does not depend on sampling order.
    auto lhsLabel = lhs.labels.empty() ? std::string_view() : lhs.labels.front().toStringView();
    auto rhsLabel = rhs.labels.empty() ? std::string_view() : rhs.labels.front().toStringView();
    return lhsLabel < rhsLabel;
}

std::vector<const StackNode*> significantChildren(const StackNode& node, Duration total) {
    std::vector<const StackNode*> result;
    for (const auto& child : node.children) {
        if (isSignificant(child.time, total)) {
            result.emplace_back(&child);
        }
    }
    return result;
}

std::vector<JavaScriptANRSampleTreeNode> makeBranches(const StackNode& parent, Duration total) {
    std::vector<JavaScriptANRSampleTreeNode> branches;
    for (const auto* child : significantChildren(parent, total)) {
        auto& branch = branches.emplace_back();
        branch.time = child->time;
        branch.labels.emplace_back(child->label);
        // A frame that leads into one frame for nearly all of its time, like the runtime.trace span around a
        // render, is printed on the same line, with the outer frame's time.
        const auto* node = child;
        while (true) {
            auto next = significantChildren(*node, total);
            if (next.size() != 1 || next.front()->time * 10 < node->time * 9) {
                break;
            }
            node = next.front();
            branch.labels.emplace_back(node->label);
        }
        branch.children = makeBranches(*node, total);
    }
    std::sort(branches.begin(), branches.end(), isBusier);
    return branches;
}

std::string toMilliseconds(Duration duration) {
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

struct TreeLine {
    std::string time;
    size_t depth;
    std::string labels;
};

void collectLines(const std::vector<JavaScriptANRSampleTreeNode>& branches,
                  size_t depth,
                  std::vector<TreeLine>& lines) {
    for (const auto& branch : branches) {
        if (lines.size() == kMaxSampleTreeLines) {
            return;
        }
        std::string labels = branch.labels.empty() ? "unattributed" : "";
        for (size_t i = 0; i < branch.labels.size(); ++i) {
            labels += (i == 0 ? "" : " -> ") + branch.labels[i].slowToString();
        }
        lines.push_back(TreeLine{toMilliseconds(branch.time), depth, std::move(labels)});
        collectLines(branch.children, depth + 1, lines);
    }
}

} // namespace

JavaScriptANRSampleTree buildSampleTree(const std::vector<JavaScriptANRSampledStack>& stacks) {
    StackNode root;
    for (const auto& stack : stacks) {
        root.time += stack.time;
        auto* node = &root;
        for (const auto& label : stack.labels) {
            node = &node->child(label);
            node->time += stack.time;
        }
        node->ownTime += stack.time;
    }

    JavaScriptANRSampleTree tree;
    tree.sampledTime = root.time;
    tree.branches = makeBranches(root, root.time);
    if (root.time.count() > 0 && isSignificant(root.ownTime, root.time)) {
        auto& unattributed = tree.branches.emplace_back();
        unattributed.time = root.ownTime;
        std::sort(tree.branches.begin(), tree.branches.end(), isBusier);
    }
    return tree;
}

std::string formatSampleTree(const JavaScriptANRSampleTree& tree, Duration anrWindow) {
    std::vector<TreeLine> lines;
    collectLines(tree.branches, 0, lines);

    // Whole milliseconds, like the frame durations, so the text stays sliceable with LIKE patterns.
    std::string result =
        "\nSampled " + toMilliseconds(tree.sampledTime) + " ms of the " + toMilliseconds(anrWindow) + " ms ANR window";
    if (lines.empty()) {
        return result + "\n  no branch held 20% of it";
    }
    size_t timeWidth = 0;
    for (const auto& line : lines) {
        timeWidth = std::max(timeWidth, line.time.size());
    }
    for (const auto& line : lines) {
        result += "\n  " + std::string(timeWidth - line.time.size(), ' ') + line.time + " ms  " +
                  std::string(line.depth * 2, ' ') + line.labels;
    }
    return result;
}

} // namespace Valdi
