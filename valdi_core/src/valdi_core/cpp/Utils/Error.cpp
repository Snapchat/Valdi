//
//  Error.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#include "valdi_core/cpp/Utils/Error.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <boost/functional/hash.hpp>

namespace Valdi {

ErrorStorage::ErrorStorage(StringBox&& message, StringBox&& stackTrace, const Ref<ErrorStorage>& cause, int32_t errorCode)
    : message(std::move(message)), stackTrace(std::move(stackTrace)), cause(cause), errorCode(errorCode) {}

ErrorStorage::~ErrorStorage() = default;

bool ErrorStorage::operator==(const ErrorStorage& storage) const {
    if (message != storage.message || stackTrace != storage.stackTrace) {
        return false;
    }
    if (cause != nullptr && storage.cause != nullptr) {
        return *cause == *storage.cause;
    }

    return (cause == nullptr) == (storage.cause == nullptr);
}

size_t ErrorStorage::hash() const {
    size_t hash = 0;
    boost::hash_combine(hash, message.hash());
    boost::hash_combine(hash, stackTrace.hash());
    if (cause != nullptr) {
        boost::hash_combine(hash, cause->hash());
    }

    return hash;
}

Error::Error() = default;
Error::Error(StringBox message) : Error(std::move(message), StringBox(), nullptr) {}
Error::Error(const std::string_view& message) : Error(StringCache::getGlobal().makeString(message)) {}
Error::Error(const char* message) : Error(STRING_LITERAL(message), StringBox(), nullptr) {}
Error::Error(StringBox message, StringBox stackTrace, const Error* cause)
    : _storage(makeShared<ErrorStorage>(
          std::move(message), std::move(stackTrace), cause != nullptr ? cause->_storage : nullptr, 0)) {}
Error::Error(StringBox message, int32_t errorCode)
    : _storage(makeShared<ErrorStorage>(std::move(message), StringBox(), nullptr, errorCode)) {}
Error::Error(Ref<ErrorStorage> storage) : _storage(std::move(storage)) {}

bool Error::hasStack() const {
    auto current = _storage;
    while (current != nullptr) {
        if (!current->stackTrace.isEmpty()) {
            return true;
        }
        current = current->cause;
    }

    return false;
}

std::optional<Error> Error::getCause() const noexcept {
    if (_storage == nullptr || _storage->cause == nullptr) {
        return std::nullopt;
    }

    return Error(_storage->cause);
}

const StringBox& Error::getMessage() const noexcept {
    return _storage != nullptr ? _storage->message : StringBox::emptyString();
}

const StringBox& Error::getStack() const noexcept {
    return _storage != nullptr ? _storage->stackTrace : StringBox::emptyString();
}

StringBox Error::toStringBox() const {
    return StringCache::getGlobal().makeString(toString());
}

std::string Error::toString() const {
    return toString(true);
}

std::string Error::toString(bool includeCauses) const {
    if (_storage == nullptr) {
        return "Empty Error";
    }

    std::string output;

    auto current = _storage;
    while (current != nullptr) {
        if (!output.empty()) {
            output += "\n[caused by]: ";
        }
        output += current->message.toStringView();
        if (includeCauses && !current->stackTrace.isEmpty()) {
            output += "\nStacktrace:\n";
            output += current->stackTrace.toStringView();
        }

        current = current->cause;
    }

    return output;
}

Error Error::rethrow(const StringBox& message) const {
    return Error(message, StringBox(), this);
}

Error Error::rethrow(std::string_view messagePrefix) const {
    return Error(StringCache::getGlobal().makeString(messagePrefix), StringBox(), this);
}

Error Error::rethrow(const Error& error) const {
    if (_storage == nullptr) {
        return error;
    }
    if (error._storage == nullptr) {
        return *this;
    }

    // Attempt to reconcile error objects

    if (error.getCause()) {
        // If our message have a cause, we build a new message that will contain
        // all the causes, and use use as a cause
        return Error(StringCache::getGlobal().makeString(error.toString()), StringBox(), this);
    } else {
        return Error(error.getMessage(), error.getStack(), this);
    }
}

Error Error::flatten() const {
    if (_storage == nullptr || _storage->cause == nullptr) {
        return *this;
    }

    auto message = toString(false);
    StringBox stacktrace;

    auto current = _storage;
    while (current != nullptr && stacktrace.isEmpty()) {
        stacktrace = current->stackTrace;
        current = current->cause;
    }

    return Error(StringCache::getGlobal().makeString(std::move(message)), std::move(stacktrace), nullptr);
}

bool Error::operator==(const Error& other) const noexcept {
    if (_storage != nullptr && other._storage != nullptr) {
        return *_storage == *other._storage;
    }
    return (_storage == nullptr) == (other._storage == nullptr);
}

bool Error::operator!=(const Error& other) const noexcept {
    return !(*this == other);
}

bool Error::isEmpty() const {
    return _storage == nullptr;
}

int32_t Error::getErrorCode() const noexcept {
    return _storage != nullptr ? _storage->errorCode : 0;
}

size_t Error::hash() const {
    return _storage != nullptr ? _storage->hash() : 0;
}

const Ref<ErrorStorage>& Error::getStorage() const {
    return _storage;
}

std::ostream& operator<<(std::ostream& os, const Error& error) {
    return os << error.toString();
}

} // namespace Valdi
