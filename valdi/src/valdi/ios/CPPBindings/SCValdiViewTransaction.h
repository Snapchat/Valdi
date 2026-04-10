//
//  SCValdiViewTransaction.h
//  valdi-ios
//
//  Created by Simon Corsin on 7/29/24.
//

#ifdef __cplusplus

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi/runtime/Interfaces/IViewTransaction.hpp"

#include <vector>

namespace ValdiIOS {

class ViewTransaction : public Valdi::IViewTransaction {
public:
    ViewTransaction();
    ~ViewTransaction() override;

    void flush(bool sync) override;

    void willUpdateRootView(const Valdi::Ref<Valdi::View>& view) override;

    void didUpdateRootView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty) override;

    void moveViewToTree(const Valdi::Ref<Valdi::View>& view,
                        Valdi::ViewNodeTree* viewNodeTree,
                        Valdi::ViewNode* viewNode) override;

    void insertChildView(const Valdi::Ref<Valdi::View>& view,
                         const Valdi::Ref<Valdi::View>& childView,
                         int index,
                         const Valdi::Ref<Valdi::Animator>& animator) override;

    void removeViewFromParent(const Valdi::Ref<Valdi::View>& view,
                              const Valdi::Ref<Valdi::Animator>& animator,
                              bool shouldClearViewNode) override;

    void invalidateViewLayout(const Valdi::Ref<Valdi::View>& view) override;

    void setViewFrame(const Valdi::Ref<Valdi::View>& view,
                      const Valdi::Frame& newFrame,
                      bool isRightToLeft,
                      const Valdi::Ref<Valdi::Animator>& animator) override;

    void setViewScrollSpecs(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Point& directionDependentContentOffset,
                            const Valdi::Size& contentSize,
                            bool animated) override;

    void setViewLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                            bool shouldDrawFlipped) override;

    void layoutView(const Valdi::Ref<Valdi::View>& view) override;

    void cancelAllViewAnimations(const Valdi::Ref<Valdi::View>& view) override;

    void willEnqueueViewToPool(const Valdi::Ref<Valdi::View>& view,
                               Valdi::Function<void(Valdi::View&)> onEnqueue) override;

    void snapshotView(const Valdi::Ref<Valdi::View>& view,
                      Valdi::Function<void(Valdi::Result<Valdi::BytesView>)> cb) override;

    void flushAnimator(const Valdi::Ref<Valdi::Animator>& animator, const Valdi::Value& completionCallback) override;

    void cancelAnimator(const Valdi::Ref<Valdi::Animator>& animator) override;

    void scheduleOnNextDraw(const Valdi::Ref<Valdi::View>& rootView, Valdi::DispatchFunction callback) override;

    void executeInTransactionThread(Valdi::DispatchFunction executeFn) override;

private:
    std::vector<Valdi::DispatchFunction> _pendingOnNextDrawCallbacks;
};

} // namespace ValdiIOS

#endif
