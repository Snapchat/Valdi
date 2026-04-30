//  Error code constants for Valdi runtime error reporting.

#pragma once

#include <cstdint>

namespace Valdi {
namespace ErrorCodes {

/**
 * Error codes for Valdi/Composer errors.
 * These are the canonical error code definitions.
 */
namespace Composer {
    
    /** Generic uncaught JavaScript error */
    static constexpr int32_t UNCAUGHT_ERROR = 1;
    
    /** Bridge call exception */
    static constexpr int32_t BRIDGE_CALL_EXCEPTION = 2;
    
    /** Cannot unwrap JS value reference as it was disposed */
    static constexpr int32_t CANNOT_UNWRAP_DISPOSED_JS_VALUE = 3;
    
    /** Network failure */
    static constexpr int32_t NETWORK_FAILURE = 4;
    
    /** Value is not an object */
    static constexpr int32_t NOT_AN_OBJECT = 5;
    
    /** Unable to fetch auth token */
    static constexpr int32_t UNABLE_TO_FETCH_AUTH_TOKEN = 6;
    
    /** Operation could not be completed */
    static constexpr int32_t OPERATION_COULD_NOT_BE_COMPLETED = 7;
    
    /** Cronet error */
    static constexpr int32_t CRONET_ERROR = 8;
    
    /** Application not responding (ANR) */
    static constexpr int32_t APPLICATION_NOT_RESPONDING = 9;
    
} // namespace Composer

} // namespace ErrorCodes
} // namespace Valdi

