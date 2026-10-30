//
//  JavaScriptErrorStackTrace.hpp
//  valdi
//
//  Created by Simon Corsin on 6/11/21.
//

#pragma once

#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"

#include <optional>

namespace Valdi {

/**
 A JSStackTraceProvider which provides its stacktrace through an Error object, and will automatically
 symbolicate the error when getStackTrace() is called. The symbolicator may also name the bundle of the
 innermost feature frame, exposed through getStackBundleName().
 */
class JavaScriptErrorStackTrace : public JSStackTraceProvider {
public:
    JavaScriptErrorStackTrace(IJavaScriptContext& jsContext,
                              const JSValue& jsValue,
                              JSExceptionTracker& exceptionTracker);
    ~JavaScriptErrorStackTrace() override;

    StringBox getStackTrace() override;
    StringBox getStackBundleName() override;

private:
    JSValueRefHolder _error;
    StringBox _stackTrace;
    StringBox _stackBundleName;
};

} // namespace Valdi
