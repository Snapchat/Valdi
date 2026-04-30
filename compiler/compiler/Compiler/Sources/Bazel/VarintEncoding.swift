// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

/// Utilities for encoding and decoding varint (variable-length integer) format
/// as used in Protocol Buffers for length-prefixed messages.
enum VarintEncoding {
    
    enum Error: Swift.Error, CustomStringConvertible {
        case invalidVarint
        case bufferTooShort
        case valueOverflow
        
        var description: String {
            switch self {
            case .invalidVarint:
                return "Invalid varint encoding"
            case .bufferTooShort:
                return "Buffer too short to contain complete varint"
            case .valueOverflow:
                return "Varint value overflow (exceeds Int max)"
            }
        }
    }
    
    struct ReadResult {
        let value: Int
        let bytesRead: Int
    }
    
    static func readVarint(from data: Data) throws -> ReadResult {
        guard !data.isEmpty else {
            throw Error.bufferTooShort
        }
        
        var value: Int = 0
        var shift: Int = 0
        var bytesRead = 0
        
        for byte in data {
            bytesRead += 1
            
            // Each byte contributes 7 bits to the value
            let byteValue = Int(byte & 0x7F)
            
            // Check for overflow before shifting
            if shift >= Int.bitWidth - 7 {
                throw Error.valueOverflow
            }
            
            value |= byteValue << shift
            shift += 7
            
            // If high bit is not set, this is the last byte
            if (byte & 0x80) == 0 {
                return ReadResult(value: value, bytesRead: bytesRead)
            }
            
            // Varint should not exceed 5 bytes for 32-bit values, 10 for 64-bit
            if bytesRead > 10 {
                throw Error.invalidVarint
            }
        }
        
        // If we got here, we ran out of bytes without finding the end marker
        throw Error.bufferTooShort
    }

    static func writeVarint(_ value: Int) -> Data {
        guard value >= 0 else {
            // For simplicity, we don't support negative values
            // Protocol Buffer varint encoding for negative numbers uses ZigZag encoding
            return Data()
        }

        var data = Data()
        var remainingValue = value

        while remainingValue > 0x7F {
            // Write 7 bits with continuation bit set
            data.append(UInt8((remainingValue & 0x7F) | 0x80))
            remainingValue >>= 7
        }

        // Write final byte without continuation bit
        data.append(UInt8(remainingValue & 0x7F))

        return data
    }
}
