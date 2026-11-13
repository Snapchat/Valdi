#include "valdi/terminal/Classes/TerminalAttributeMacros.hpp"

#include "valdi/runtime/Views/ViewAttributeHandlerDelegate.hpp"

namespace Valdi::TerminalAttributes {

namespace {

class TerminalAttributeDelegate : public ViewAttributeHandlerDelegate {
public:
    TerminalAttributeDelegate(ViewApplier&& applier, ViewResetter&& resetter)
        : _applier(std::move(applier)), _resetter(std::move(resetter)) {}

protected:
    Result<Void> onViewApply(const Ref<View>& view,
                             const StringBox& /*name*/,
                             const Value& value,
                             const Ref<Animator>& /*animator*/) override {
        auto terminalView = TerminalView::unwrap(view);
        if (terminalView == nullptr) {
            return Void();
        }
        auto result = _applier(*terminalView, value);
        terminalView->markDirty();
        return result;
    }

    void onViewReset(const Ref<View>& view, const StringBox& /*name*/, const Ref<Animator>& /*animator*/) override {
        auto terminalView = TerminalView::unwrap(view);
        if (terminalView == nullptr) {
            return;
        }
        _resetter(*terminalView);
        terminalView->markDirty();
    }

private:
    ViewApplier _applier;
    ViewResetter _resetter;
};

class IgnoringDelegate : public ViewAttributeHandlerDelegate {
protected:
    Result<Void> onViewApply(const Ref<View>& /*view*/,
                             const StringBox& /*name*/,
                             const Value& /*value*/,
                             const Ref<Animator>& /*animator*/) override {
        return Void();
    }

    void onViewReset(const Ref<View>& /*view*/, const StringBox& /*name*/, const Ref<Animator>& /*animator*/) override {
    }
};

} // namespace

Ref<AttributeHandlerDelegate> makeDelegate(ViewApplier&& applier, ViewResetter&& resetter) {
    return makeShared<TerminalAttributeDelegate>(std::move(applier), std::move(resetter));
}

Ref<AttributeHandlerDelegate> makeIgnoringDelegate() {
    return makeShared<IgnoringDelegate>();
}

} // namespace Valdi::TerminalAttributes
