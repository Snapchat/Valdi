#pragma once

#include <atomic>
#include <cstdint>

namespace Valdi {

/**
 Outcome of the most recent ObjectValueMarshaller::marshallInterface call on the
 current thread. Used to attribute a null observed by the platform after pushing
 an interface object (e.g. a platform service) to the exact code path that
 produced it.
 */
enum class InterfaceMarshallOutcome : int32_t {
    /** No marshallInterface call happened since the last reset. */
    None = 0,
    /** An existing proxy was found in the object attachments and reused. */
    ReusedExistingProxy = 1,
    /** A new proxy was created and stored. */
    CreatedNewProxy = 2,
    /** Looking up the object attachments raised an exception. */
    AttachmentsLookupFailed = -1,
    /** Storing new object attachments raised an exception. */
    AttachmentsStoreFailed = -2,
    /** Marshalling the backing typed object raised an exception. */
    TypedObjectMarshallFailed = -3,
    /** Creating the proxy object raised an exception. */
    ProxyCreationFailed = -4,
    /** Registering the proxy id in the object store raised an exception. */
    ObjectStoreSetFailed = -5,
};

/**
 Thread-local slot holding the last InterfaceMarshallOutcome. Nested
 marshallInterface calls overwrite it in completion order, so after an outermost
 push the slot reflects the outermost call.
 */
inline InterfaceMarshallOutcome& lastInterfaceMarshallOutcomeSlot() {
    static thread_local InterfaceMarshallOutcome outcome = InterfaceMarshallOutcome::None;
    return outcome;
}

/**
 Process-global gate for outcome recording, disabled by default so the marshall
 hot path performs no thread-local writes unless the platform opts in.
 */
inline std::atomic<bool>& interfaceMarshallDiagnosticsEnabledFlag() {
    static std::atomic<bool> enabled{false};
    return enabled;
}

inline void setInterfaceMarshallDiagnosticsEnabled(bool enabled) {
    interfaceMarshallDiagnosticsEnabledFlag().store(enabled, std::memory_order_relaxed);
}

inline void setLastInterfaceMarshallOutcome(InterfaceMarshallOutcome outcome) {
    if (!interfaceMarshallDiagnosticsEnabledFlag().load(std::memory_order_relaxed)) {
        return;
    }
    lastInterfaceMarshallOutcomeSlot() = outcome;
}

/** Returns the last outcome and resets the slot to none. */
inline int32_t getAndResetLastInterfaceMarshallOutcome() {
    auto& slot = lastInterfaceMarshallOutcomeSlot();
    auto outcome = static_cast<int32_t>(slot);
    slot = InterfaceMarshallOutcome::None;
    return outcome;
}

} // namespace Valdi
