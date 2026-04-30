//
//  ExceptionTracker.cpp
//  valdi_core-ios
//
//  Created by Simon Corsin on 2/23/23.
//

#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Context/ContextBase.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

ExceptionTracker::~ExceptionTracker() = default;

void ExceptionTracker::setEmpty(bool isEmpty) {
    _isEmpty = isEmpty;
}

void ExceptionTracker::onError(const Error& error) {
    onError(Error(error));
}

void ExceptionTracker::onError(Error&& error) {
    if (_isEmpty) {
        _isEmpty = false;
        onSetError(std::move(error));
    } else {
        onSetError(getError().rethrow(error));
    }
}

void ExceptionTracker::onError(std::string_view errorMessage) {
    onError(Error(StringCache::getGlobal().makeString(errorMessage)));
}

void ExceptionTracker::onError(const char* errorMessage) {
    onError(Error(errorMessage));
}

Error ExceptionTracker::getError() const {
    if (_isEmpty) {
        return Error("No Error");
    }
    return onGetError();
}

Error ExceptionTracker::extractError() {
    auto error = getError();
    clearError();
    return error;
}

void ExceptionTracker::clearError() {
    if (!_isEmpty) {
        onClearError();
        _isEmpty = true;
    }
}

void ExceptionTracker::assertEmpty() const {
    if (!_isEmpty) {
        auto message = getError().toString();
        auto* context = ContextBase::current();
        if (context != nullptr) {
            const auto& attribution = context->getAttribution();
            if (!attribution.moduleName.isEmpty() || !attribution.owner.isEmpty()) {
                message += "\n[valdi_attribution] module=";
                message += attribution.moduleName.toStringView();
                message += " owner=";
                message += attribution.owner.toStringView();
            }
        }
        SC_ASSERT_FAIL(message.c_str());
    }
}

void ExceptionTracker::assertNotEmpty() const {
    SC_ASSERT(!_isEmpty);
}

SimpleExceptionTracker::SimpleExceptionTracker() = default;
SimpleExceptionTracker::~SimpleExceptionTracker() {
    assertEmpty();
}

Error SimpleExceptionTracker::onGetError() const {
    SC_ASSERT(_error.has_value());
    return _error.value();
}

void SimpleExceptionTracker::onClearError() {
    _error = std::nullopt;
}

void SimpleExceptionTracker::onSetError(Error&& error) {
    _error = std::make_optional<Error>(std::move(error));
}

} // namespace Valdi
