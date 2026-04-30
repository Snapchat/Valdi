#pragma once

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodePath.hpp"
#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi_core/CompositeAttributePart.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/LazyValueConvertible.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include "valdi/standalone_runtime/StandaloneGlobalScrollPerfLoggerBridgeModuleFactory.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"

#include "utils/time/StopWatch.hpp"

#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace ValdiTest {

class DummyView {
public:
    DummyView(const char* className);
    DummyView(Valdi::Ref<Valdi::StandaloneView> impl);

    Valdi::Value getAttribute(const char* attribute);

    Valdi::Ref<Valdi::LoadedAsset> getLoadedAsset() const;

    DummyView getChild(size_t index);

    DummyView& addChild(DummyView child);

    size_t getChildrenCount() const;

    template<typename T>
    DummyView& addAttribute(const char* name, T value) {
        _impl->setAttribute(STRING_LITERAL(name), Valdi::Value(value));

        return *this;
    }

    DummyView& addAttribute(const char* name, const char* value);

    Valdi::StandaloneView& getImpl() const;

    const Valdi::Ref<Valdi::StandaloneView>& getSharedImpl() const;

    void setTag(const char* tag);

    std::string getTag();

    std::string_view getClassName();

    DummyView clone() const;

    bool isRightToLeft() const;

private:
    Valdi::Ref<Valdi::StandaloneView> _impl;
};

std::ostream& operator<<(std::ostream& os, const DummyView& dummyView);

bool operator!=(const DummyView& left, const DummyView& right);

bool operator==(const DummyView& left, const DummyView& right);

bool operator!=(const DummyView& left, const DummyView& right);

template<typename T>
T unwrapResult(Valdi::Result<T> result) {
    if (!result) {
        throw Valdi::Exception(result.error().toStringBox());
    }
    return result.moveValue();
}

DummyView getDummyView(const Valdi::Ref<Valdi::View>& view);

DummyView getRootView(Valdi::SharedViewNodeTree& viewNodeTree);

Valdi::ViewNodePath parseNodePath(std::string_view nodePath);

DummyView getViewForId(const Valdi::SharedViewNodeTree& viewNodeTree, std::string id);
Valdi::Ref<Valdi::ViewNode> getViewNodeForId(const Valdi::SharedViewNodeTree& viewNodeTree, std::string id);

Valdi::Path resolveTestPath(const std::string& path);

Valdi::Path resolveOpenSourceTestPath(const std::string& path);

Valdi::Result<Valdi::BytesView> loadFileFromAbsolutePath(const Valdi::Path& absolutePath);

Valdi::Result<Valdi::BytesView> loadResourceFromDisk(const std::string& path);

Valdi::FlatMap<Valdi::Path, Valdi::BytesView> readDirectory(const std::string& path);

std::string getModuleSearchDirectoryPath();

std::string getOpenSourceModuleSearchDirectoryPath();

struct MainQueue : public Valdi::StandaloneMainQueue {
    int numberOfExecutedTasks = 0;

    template<typename Fun>
    void runUntilTrue(Fun&& func) {
        runUntilTrue("Waiting until condition becomes true", func);
    }

    template<typename Fun>
    void runUntilTrue(const char* desc, Fun&& func) {
        while (!func()) {
            // Delay was 1 second, which caused intermittent test failures
            const auto delay = std::chrono::seconds(10);
            auto success = runNextTask(std::chrono::steady_clock::now() + delay);
            if (!success) {
                throw Valdi::Exception(fmt::format("Condition '{}' timed out", desc));
            }
            numberOfExecutedTasks++;
        }
    }

    void runOnce();

    void runOnce(const char* desc);

    void flush();

    void stop();
};

struct RuntimeListener : public Valdi::IRuntimeListener {
    int numberOfRenders = 0;
    int numberOfLayoutBecameDirty = 0;
    bool layoutTreeAutomatically = true;

    void onContextCreated(Valdi::Runtime& runtime, const Valdi::SharedContext& context) override;

    void onContextDestroyed(Valdi::Runtime& runtime, Valdi::Context& context) override;

    void onContextRendered(Valdi::Runtime& runtime, const Valdi::SharedContext& context) override;
    void onContextLayoutBecameDirty(Valdi::Runtime& runtime, const Valdi::SharedContext& context) override;

    void onAllContextsDestroyed(Valdi::Runtime& runtime) override;

private:
    void setLayoutSpecsToContextIfNeeded(Valdi::Runtime& runtime, const Valdi::SharedContext& context);
};

template<typename Func>
void forEachViewNode(Valdi::ViewNode* viewNode, Func&& func) {
    func(viewNode);

    for (auto child : *viewNode) {
        forEachViewNode(child, func);
    }
}

template<typename Func>
void forEachViewNode(const Valdi::SharedViewNode& viewNode, Func&& func) {
    forEachViewNode(viewNode.get(), func);
}

std::vector<Valdi::ViewNode*> findViewNodesWithId(const Valdi::SharedViewNode& viewNode, const char* nodeId);

void assertHasNoView(Valdi::ViewNode* viewNode);

void assertHasNoView(const Valdi::SharedViewNode& viewNode);

void assertAllHasView(Valdi::ViewNode* viewNode);

void assertAllHasView(const Valdi::SharedViewNode& viewNode);

int getNumberOfPooledViews(const Valdi::StringBox& viewClassName, const Valdi::ViewPoolsStats& stats);

template<typename T>
T waitForFuture(std::future<T>& future) {
    auto status = future.wait_for(std::chrono::seconds(1));
    if (status != std::future_status::ready) {
        throw Valdi::Exception("Timed out while waiting for future to become ready");
    }
    return future.get();
}

void runParallel(size_t threadsCount, int64_t durationSeconds, Valdi::DispatchFunction run);

bool runTypeScriptTests(std::string_view modulePath,
                        Valdi::IJavaScriptBridge* jsBridge,
                        std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> moduleFactoriesProvider);

} // namespace ValdiTest
