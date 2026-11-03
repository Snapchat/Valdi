#include "valdi/terminal/Modules/TerminalBackendModule.hpp"

#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextAutoDestroy.hpp"
#include "valdi/runtime/Context/ContextManager.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/terminal/Host/TerminalRoot.hpp"
#include "valdi/terminal/Sink/MemorySink.hpp"
#include "valdi/terminal/Sink/TtySink.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithMethod.hpp"

#include <algorithm>
#include <unistd.h>

namespace Valdi {

namespace {

Value makeString(const std::string& text) {
    return Value(StringCache::getGlobal().makeString(std::string_view(text)));
}

Value frameToValue(const Terminal::FrameSnapshot& frame) {
    auto lines = ValueArray::make(frame.plainLines.size());
    for (size_t i = 0; i < frame.plainLines.size(); i++) {
        lines->emplace(i, makeString(frame.plainLines[i]));
    }
    return Value()
        .setMapValue("lines", Value(lines))
        .setMapValue("ansi", makeString(frame.ansi))
        .setMapValue("columns", Value(static_cast<int32_t>(frame.columns)))
        .setMapValue("height", Value(static_cast<int32_t>(frame.height)));
}

Value makeCancelable(Function<void()> cancel) {
    return Value().setMapValue(
        "cancel",
        Value(makeShared<ValueFunctionWithCallable>(
            [cancel = std::move(cancel)](const ValueFunctionCallContext& /*callContext*/) -> Value {
                cancel();
                return Value();
            })));
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

} // namespace

/**
 * Keeps the terminal root and its context alive for JS. Dropping the last reference destroys the
 * context, like the managed contexts of the SnapDrawing module.
 */
class TerminalRootHandle : public ContextAutoDestroy {
public:
    TerminalRootHandle(Runtime* runtime,
                       const Ref<ViewNodeTree>& viewNodeTree,
                       ContextId contextId,
                       ContextManager& contextManager,
                       const Ref<TerminalRoot>& root,
                       const Ref<Terminal::ITerminalSink>& sink,
                       const Ref<Terminal::MemorySink>& memorySink)
        : ContextAutoDestroy(contextId, contextManager),
          _runtime(weakRef(runtime)),
          _viewNodeTree(viewNodeTree),
          _root(root),
          _sink(sink),
          _memorySink(memorySink) {}

    ~TerminalRootHandle() override {
        destroy();
    }

    void destroy() {
        if (_destroyed.exchange(true)) {
            return;
        }
        _root->destroy();
        auto runtime = _runtime.lock();
        if (runtime != nullptr) {
            ContextAutoDestroy::destroy();
            runtime->destroyViewNodeTree(*_viewNodeTree);
        }
    }

    /** Paints a last time so non-interactive output gets its single frame, then tears down. */
    void finishAndDestroy() {
        if (!_destroyed) {
            _root->finish();
        }
        destroy();
    }

    const Ref<TerminalRoot>& getRoot() const {
        return _root;
    }

    const Ref<Terminal::MemorySink>& getMemorySink() const {
        return _memorySink;
    }

private:
    std::atomic<bool> _destroyed{false};
    Weak<Runtime> _runtime;
    Ref<ViewNodeTree> _viewNodeTree;
    Ref<TerminalRoot> _root;
    Ref<Terminal::ITerminalSink> _sink;
    Ref<Terminal::MemorySink> _memorySink;
};

static Ref<TerminalRootHandle> getHandle(const ValueFunctionCallContext& callContext) {
    return callContext.getParameter(0).checkedToValdiObject<TerminalRootHandle>(callContext.getExceptionTracker());
}

TerminalBackendModule::TerminalBackendModule(Ref<TerminalRuntime> terminalRuntime)
    : _terminalRuntime(std::move(terminalRuntime)) {}

TerminalBackendModule::~TerminalBackendModule() = default;

StringBox TerminalBackendModule::getModulePath() {
    return STRING_LITERAL("valdi_tui/src/TerminalBackendBridge");
}

Value TerminalBackendModule::loadModule() {
    Value out;

    ValueFunctionMethodBinder<TerminalBackendModule> binder(this, out);
    binder.bind("createRoot", &TerminalBackendModule::createRoot);
    binder.bind("destroyRoot", &TerminalBackendModule::destroyRoot);
    binder.bind("observeFrame", &TerminalBackendModule::observeFrame);
    binder.bind("setSize", &TerminalBackendModule::setSize);
    binder.bind("feedInput", &TerminalBackendModule::feedInput);
    binder.bind("writeAbove", &TerminalBackendModule::writeAbove);
    binder.bind("clearRoot", &TerminalBackendModule::clearRoot);
    binder.bind("getOutput", &TerminalBackendModule::getOutput);
    binder.bind("getLastFrame", &TerminalBackendModule::getLastFrame);
    binder.bind("getSize", &TerminalBackendModule::getSize);
    binder.bind("isTTY", &TerminalBackendModule::isTTY);
    binder.bind("observeInput", &TerminalBackendModule::observeInput);
    binder.bind("observeResize", &TerminalBackendModule::observeResize);
    return out;
}

Value TerminalBackendModule::createRoot(const ValueFunctionCallContext& callContext) {
    auto context = Context::current();
    if (context == nullptr || context->getRuntime() == nullptr) {
        callContext.getExceptionTracker().onError("Need a Context with an attached Runtime");
        return Value();
    }
    auto* runtime = context->getRuntime();
    TerminalRuntime::configureRuntime(*runtime);

    const auto options = callContext.getParameter(0);
    const auto sinkKind = options.getMapValue("sink");
    const bool useMemory = sinkKind.isString() && sinkKind.toStringBox() == "memory";
    const auto columns = options.getMapValue("columns");
    const auto rows = options.getMapValue("rows");

    Ref<Terminal::ITerminalSink> sink;
    Ref<Terminal::MemorySink> memorySink;
    if (useMemory) {
        memorySink = makeShared<Terminal::MemorySink>();
        const auto isTTY = options.getMapValue("isTTY");
        if (isTTY.isBool()) {
            memorySink->setIsTTY(isTTY.toBool());
        }
        const auto colorLevel = options.getMapValue("colorLevel");
        if (colorLevel.isNumber()) {
            memorySink->setColorLevel(static_cast<Terminal::ColorLevel>(std::clamp(colorLevel.toInt(), 0, 3)));
        }
        if (columns.isNumber() || rows.isNumber()) {
            memorySink->setSize(columns.isNumber() ? columns.toInt() : memorySink->columns(),
                                rows.isNumber() ? rows.toInt() : memorySink->rows());
        }
        sink = memorySink;
    } else {
        sink = Terminal::TtySink::makeForStdout();
    }

    TerminalRootOptions rootOptions;
    const auto fitTerminal = options.getMapValue("fitTerminal");
    rootOptions.fitTerminal = fitTerminal.isBool() && fitTerminal.toBool();
    const auto debug = options.getMapValue("debug");
    rootOptions.debug = debug.isBool() && debug.toBool();
    if (columns.isNumber()) {
        rootOptions.columns = columns.toInt();
    }
    if (rows.isNumber()) {
        rootOptions.rows = rows.toInt();
    }

    auto viewManagerContext = _terminalRuntime->getViewManagerContext();
    if (viewManagerContext == nullptr) {
        callContext.getExceptionTracker().onError("The runtime manager is no longer available");
        return Value();
    }

    auto newContext = runtime->getContextManager().createContext(nullptr, viewManagerContext, /* deferRender */ false);
    auto viewNodeTree = runtime->createViewNodeTree(newContext, ViewNodeTreeThreadAffinity::ANY);
    auto root = makeShared<TerminalRoot>(viewNodeTree, sink, std::move(rootOptions));
    root->attach();

    auto handle = makeShared<TerminalRootHandle>(
        runtime, viewNodeTree, newContext->getContextId(), runtime->getContextManager(), root, sink, memorySink);

    return Value()
        .setMapValue("contextId", Value(static_cast<int32_t>(newContext->getContextId())))
        .setMapValue("native", Value(handle));
}

Value TerminalBackendModule::destroyRoot(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    if (handle != nullptr) {
        handle->finishAndDestroy();
    }
    return Value();
}

Value TerminalBackendModule::observeFrame(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    auto observer = callContext.getParameterAsFunction(1);
    if (handle == nullptr || !callContext.getExceptionTracker()) {
        return Value();
    }

    // Frames are painted on the main thread; the flag hops the call onto the JS queue.
    const auto id = handle->getRoot()->addFrameObserver([observer](const Terminal::FrameSnapshot& frame) {
        (*observer)(ValueFunctionFlagsNeverCallSync, {frameToValue(frame)});
    });

    return makeCancelable([weakHandle = weakRef(handle.get()), id]() {
        auto handle = weakHandle.lock();
        if (handle != nullptr) {
            handle->getRoot()->removeFrameObserver(id);
        }
    });
}

Value TerminalBackendModule::setSize(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    const auto columns = callContext.getParameterAsInt(1);
    const auto rows = callContext.getParameterAsInt(2);
    if (handle == nullptr || !callContext.getExceptionTracker()) {
        return Value();
    }
    if (handle->getMemorySink() != nullptr) {
        handle->getMemorySink()->setSize(columns, rows);
    }
    handle->getRoot()->setSize(columns, rows);
    dispatchResize(columns, rows);
    return Value();
}

Value TerminalBackendModule::feedInput(const ValueFunctionCallContext& callContext) {
    const auto text = callContext.getParameterAsString(1);
    if (!callContext.getExceptionTracker()) {
        return Value();
    }
    dispatchInput(text);
    return Value();
}

Value TerminalBackendModule::writeAbove(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    const auto text = callContext.getParameterAsString(1);
    if (handle == nullptr || !callContext.getExceptionTracker()) {
        return Value();
    }
    handle->getRoot()->writeAbove(splitLines(text.toStringView()));
    return Value();
}

Value TerminalBackendModule::clearRoot(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    if (handle != nullptr) {
        handle->getRoot()->clear();
    }
    return Value();
}

Value TerminalBackendModule::getOutput(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    if (handle == nullptr || handle->getMemorySink() == nullptr) {
        return makeString("");
    }
    return makeString(handle->getMemorySink()->output());
}

Value TerminalBackendModule::getLastFrame(const ValueFunctionCallContext& callContext) {
    auto handle = getHandle(callContext);
    if (handle == nullptr) {
        return Value();
    }
    auto frame = handle->getRoot()->getLastFrame();
    if (frame.columns == 0) {
        return Value();
    }
    return frameToValue(frame);
}

Value TerminalBackendModule::getSize(const ValueFunctionCallContext& /*callContext*/) {
    auto sink = Terminal::TtySink::makeForStdout();
    if (!sink->isTTY() || sink->columns() <= 0 || sink->rows() <= 0) {
        return Value::undefined();
    }
    auto size = ValueArray::make(2);
    size->emplace(0, Value(static_cast<int32_t>(sink->columns())));
    size->emplace(1, Value(static_cast<int32_t>(sink->rows())));
    return Value(size);
}

Value TerminalBackendModule::isTTY(const ValueFunctionCallContext& callContext) {
    const auto fd = callContext.getParameterAsInt(0);
    if (!callContext.getExceptionTracker()) {
        return Value();
    }
    return Value(::isatty(fd) != 0);
}

Value TerminalBackendModule::addObserver(std::map<int, Ref<ValueFunction>>& observers,
                                         const Ref<ValueFunction>& observer) {
    int id;
    {
        std::lock_guard<Mutex> guard(_mutex);
        id = ++_nextObserverId;
        observers.emplace(id, observer);
    }
    return makeCancelable([weakSelf = weakRef(this), &observers, id]() {
        auto self = weakSelf.lock();
        if (self != nullptr) {
            std::lock_guard<Mutex> guard(self->_mutex);
            observers.erase(id);
        }
    });
}

Value TerminalBackendModule::observeInput(const ValueFunctionCallContext& callContext) {
    auto observer = callContext.getParameterAsFunction(0);
    if (!callContext.getExceptionTracker()) {
        return Value();
    }
    return addObserver(_inputObservers, observer);
}

Value TerminalBackendModule::observeResize(const ValueFunctionCallContext& callContext) {
    auto observer = callContext.getParameterAsFunction(0);
    if (!callContext.getExceptionTracker()) {
        return Value();
    }
    return addObserver(_resizeObservers, observer);
}

void TerminalBackendModule::dispatchInput(const StringBox& text) {
    std::vector<Ref<ValueFunction>> observers;
    {
        std::lock_guard<Mutex> guard(_mutex);
        for (const auto& entry : _inputObservers) {
            observers.emplace_back(entry.second);
        }
    }
    for (const auto& observer : observers) {
        (*observer)(ValueFunctionFlagsNeverCallSync, {Value(text)});
    }
}

void TerminalBackendModule::dispatchResize(int columns, int rows) {
    std::vector<Ref<ValueFunction>> observers;
    {
        std::lock_guard<Mutex> guard(_mutex);
        for (const auto& entry : _resizeObservers) {
            observers.emplace_back(entry.second);
        }
    }
    for (const auto& observer : observers) {
        (*observer)(ValueFunctionFlagsNeverCallSync,
                    {Value(static_cast<int32_t>(columns)), Value(static_cast<int32_t>(rows))});
    }
}

} // namespace Valdi
