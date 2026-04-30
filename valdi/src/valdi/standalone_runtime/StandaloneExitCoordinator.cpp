//
//  StandaloneExitCoordinator.cpp
//  valdi-standalone_runtime
//
//  Created by Simon Corsin on 10/16/19.
//

#include "valdi/standalone_runtime/StandaloneExitCoordinator.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

class JavaScriptQueueListener : public IQueueListener {
public:
    explicit JavaScriptQueueListener(Weak<StandaloneExitCoordinator> exitCoordinator)
        : _exitCoordinator(std::move(exitCoordinator)) {}
    ~JavaScriptQueueListener() override = default;

    void onQueueEmpty() override {
        auto exitCoordinator = _exitCoordinator.lock();
        if (exitCoordinator != nullptr) {
            exitCoordinator->onJsQueueEmpty(true);
        }
    }

    void onQueueNonEmpty() override {
        auto exitCoordinator = _exitCoordinator.lock();
        if (exitCoordinator != nullptr) {
            exitCoordinator->onJsQueueEmpty(false);
        }
    }

private:
    Weak<StandaloneExitCoordinator> _exitCoordinator;
};

class MainQueueListener : public IQueueListener {
public:
    explicit MainQueueListener(Weak<StandaloneExitCoordinator> exitCoordinator)
        : _exitCoordinator(std::move(exitCoordinator)) {}
    ~MainQueueListener() override = default;

    void onQueueEmpty() override {
        auto exitCoordinator = _exitCoordinator.lock();
        if (exitCoordinator != nullptr) {
            exitCoordinator->onMainQueueEmpty(true);
        }
    }

    void onQueueNonEmpty() override {
        auto exitCoordinator = _exitCoordinator.lock();
        if (exitCoordinator != nullptr) {
            exitCoordinator->onMainQueueEmpty(false);
        }
    }

private:
    Weak<StandaloneExitCoordinator> _exitCoordinator;
};

StandaloneExitCoordinator::StandaloneExitCoordinator(const Ref<DispatchQueue>& jsQueue, const Ref<TaskQueue>& mainQueue)
    : _jsQueue(jsQueue), _mainQueue(mainQueue) {
    _coordinatorQueue = DispatchQueue::create(STRING_LITERAL("Valdi Exit Coordinator"), ThreadQoSClassNormal);
}

StandaloneExitCoordinator::~StandaloneExitCoordinator() {
    _coordinatorQueue->fullTeardown();
}

void StandaloneExitCoordinator::postInit() {
    _jsQueue->setListener(Valdi::makeShared<JavaScriptQueueListener>(weakRef(this)));
    _mainQueue->setListener(Valdi::makeShared<MainQueueListener>(weakRef(this)));
}

void StandaloneExitCoordinator::flushUpdatesSync() {
    _coordinatorQueue->sync([]() {});
}

void StandaloneExitCoordinator::setEnabled(bool enabled) {
    auto weakSelf = weakRef(this);
    _coordinatorQueue->async([weakSelf, enabled]() {
        auto self = weakSelf.lock();
        if (self == nullptr) {
            return;
        }
        self->_enabled = enabled;
        self->exitIfNeeded();
    });
}

void StandaloneExitCoordinator::onJsQueueEmpty(bool empty) {
    auto weakSelf = weakRef(this);
    _coordinatorQueue->async([weakSelf, empty]() {
        auto self = weakSelf.lock();
        if (self == nullptr) {
            return;
        }
        self->_jsQueueEmpty = empty;
        self->exitIfNeeded();
    });
}

void StandaloneExitCoordinator::onMainQueueEmpty(bool empty) {
    auto weakSelf = weakRef(this);
    _coordinatorQueue->async([weakSelf, empty]() {
        auto self = weakSelf.lock();
        if (self == nullptr) {
            return;
        }
        self->_mainQueueEmpty = empty;
        self->exitIfNeeded();
    });
}

void StandaloneExitCoordinator::exitIfNeeded() {
    if (_enabled && _jsQueueEmpty && _mainQueueEmpty) {
        _enabled = false;
        _mainQueue->dispose();
    }
}

} // namespace Valdi
