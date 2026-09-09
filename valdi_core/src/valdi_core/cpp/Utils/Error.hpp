//
//  Error.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

/**
 Error code stamped on the error reported when a native->JS resolution/call is skipped because the
 owning JS runtime was disposed / torn down (makeJsThreadDispatchFunction early-returns on
 _isDisposed/!_running) — nothing ran and nothing threw. Distinguishing this from a genuine
 resolution failure lets a marshalling boundary degrade gracefully on teardown without masking a
 real bug. Distinct from ErrorCodes::Composer (1-9).
 */
static constexpr int32_t kResolutionSkippedDuringTeardownErrorCode = 100;

/**
 Error code stamped on the synthetic failure a canceled promise delivers to its callbacks when the
 producer does not settle it during cancellation (see ResolvablePromise::cancel). Lets Swift map the
 failure to a cancellation (NSErrorFromError carries getErrorCode() into kValdiErrorDomain) and lets
 JS filter cancellations without string-matching the message. Distinct from ErrorCodes::Composer (1-9)
 and kResolutionSkippedDuringTeardownErrorCode (100).
 */
static constexpr int32_t kPromiseCanceledErrorCode = 101;

struct ErrorStorage : public SimpleRefCountable {
    StringBox message;
    StringBox stackTrace;
    Ref<ErrorStorage> cause;
    int32_t errorCode;

    ErrorStorage(StringBox&& message, StringBox&& stackTrace, const Ref<ErrorStorage>& cause, int32_t errorCode = 0);
    ~ErrorStorage() override;

    bool operator==(const ErrorStorage& storage) const;

    size_t hash() const;
};

class Error {
public:
    Error();
    explicit Error(StringBox message);
    explicit Error(const std::string_view& message);
    explicit Error(const char* message);
    Error(StringBox message, StringBox stackTrace, const Error* cause);
    Error(StringBox message, int32_t errorCode);
    Error(Ref<ErrorStorage> storage);

    bool isEmpty() const;
    bool hasStack() const;

    std::string toString() const;
    StringBox toStringBox() const;

    /**
     Return the outer message in the Error hierarchy.
     */
    const StringBox& getMessage() const noexcept;

    /**
     Return the outer stack in the Error hierarchy.
     */
    const StringBox& getStack() const noexcept;

    std::optional<Error> getCause() const noexcept;

    /**
     Return the error code associated with this error.
     Returns 0 if no specific error code was set.
     */
    int32_t getErrorCode() const noexcept;

    bool operator==(const Error& other) const noexcept;
    bool operator!=(const Error& other) const noexcept;

    Error rethrow(const StringBox& message) const;
    Error rethrow(std::string_view message) const;
    Error rethrow(const Error& error) const;

    /**
     Return an error message that contains a merged message from all the causes, and contains the first resolved stack
     */
    Error flatten() const;

    size_t hash() const;

    const Ref<ErrorStorage>& getStorage() const;

private:
    Ref<ErrorStorage> _storage;

    std::string toString(bool includeCauses) const;
};

std::ostream& operator<<(std::ostream& os, const Error& error);

} // namespace Valdi
