//
//  JSCoreUtils.cpp
//  ValdiIOS
//
//  Created by Simon Corsin on 5/30/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/jscore/JSCoreUtils.hpp"

#include "valdi/jscore/JSCoreCustomClasses.hpp"
#include "valdi/jscore/JavaScriptCoreContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include <algorithm>
#include <sstream>

namespace ValdiJSCore {

JSObjectRef JSCoreRef::asObjectRefOrThrow(Valdi::IJavaScriptContext& jsContext,
                                          Valdi::JSExceptionTracker& exceptionTracker) const {
    auto objectRef = asObjectRef();
    if (objectRef == nullptr) {
        exceptionTracker.onError(Valdi::Error("Value is not an object"));
    }

    return objectRef;
}

std::pair<const char*, size_t> jsStringToUTF8(JSStringRef stringRef) {
    if (stringRef == nullptr) {
        return std::make_pair<const char*, size_t>(nullptr, 0);
    }

    auto maxUtf8Size = JSStringGetMaximumUTF8CStringSize(stringRef);

    thread_local static std::vector<char> tBuffer;
    auto& buffer = tBuffer;

    if (buffer.size() < maxUtf8Size) {
        buffer.resize(std::max(maxUtf8Size * 2, static_cast<size_t>(1024)));
    }

    auto written = JSStringGetUTF8CString(stringRef, buffer.data(), maxUtf8Size);
    if (written == 0) {
        return std::make_pair<const char*, size_t>(nullptr, 0);
    } else {
        return std::make_pair(buffer.data(), written - 1);
    }
}

Valdi::StringBox jsStringtoStdString(JSStringRef stringRef) {
    auto pair = jsStringToUTF8(stringRef);
    return Valdi::StringCache::getGlobal().makeString(pair.first, pair.second);
}

} // namespace ValdiJSCore
