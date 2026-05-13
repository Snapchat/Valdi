//
//  V8JavaScriptContextFactory.hpp
//  valdi-v8
//
//  Created by Ramzy Jaber on 6/13/23.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"

namespace Valdi::V8 {

class V8JavaScriptContextFactory : public IJavaScriptBridge {
public:
    const char* getName() override {
        return "V8";
    }

    Ref<IJavaScriptContext> createJsContext(JavaScriptTaskScheduler* taskScheduler, ILogger& logger) override;

    BytesView dumpHeap(std::span<IJavaScriptContext*> jsContexts, JSExceptionTracker& exceptionTracker) final;
};

} // namespace Valdi::V8
