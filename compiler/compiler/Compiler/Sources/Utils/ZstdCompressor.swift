//
//  ZstdCompressor.swift
//  Compiler
//
//  Created by Simon Corsin on 3/8/19.
//

import Foundation
import Zstd

class ZstdCompressor {

    private static let compressionLevel: Int32 = 19

    class func compress(data: Data) throws -> Data {
        guard let stream = ZSTD_createCStream() else {
            throw CompilerError("Failed to create compression stream")
        }
        defer {
            ZSTD_freeCStream(stream)
        }

        let initResult = ZSTD_initCStream(stream, ZstdCompressor.compressionLevel)
        guard ZSTD_isError(initResult) == 0 else {
            throw CompilerError("zstd: \(String(cString: ZSTD_getErrorName(initResult)))")
        }

        let pledgeResult = ZSTD_CCtx_setPledgedSrcSize(stream, UInt64(data.count))
        guard ZSTD_isError(pledgeResult) == 0 else {
            throw CompilerError("zstd: \(String(cString: ZSTD_getErrorName(pledgeResult)))")
        }

        var output = Data()

        try data.withUnsafePointer { (inputBytes: UnsafePointer<UInt8>) in
            var buffer = Data()
            let bufferLength = ZSTD_CStreamOutSize()
            buffer.reserveCapacity(bufferLength)
            for _ in 0..<bufferLength {
                buffer.append(0)
            }

            var inBuffer = ZSTD_inBuffer()
            inBuffer.pos = 0
            inBuffer.src = UnsafeRawPointer(inputBytes)
            inBuffer.size = data.count

            while inBuffer.pos < inBuffer.size {
                let written: Int = try buffer.withUnsafeMutablePointer { (bufferBytes: UnsafeMutablePointer<UInt8>) in
                    var outBuffer = ZSTD_outBuffer()
                    outBuffer.pos = 0
                    outBuffer.dst = UnsafeMutableRawPointer(bufferBytes)
                    outBuffer.size = bufferLength

                    let result = ZSTD_compressStream(stream, &outBuffer, &inBuffer)
                    guard ZSTD_isError(result) == 0 else {
                        throw CompilerError("zstd: \(String(cString: ZSTD_getErrorName(result)))")
                    }

                    return outBuffer.pos
                }

                output.append(buffer[0..<written])
            }

            let written: Int = try buffer.withUnsafeMutablePointer { (bufferBytes: UnsafeMutablePointer<UInt8>) in
                var outBuffer = ZSTD_outBuffer()
                outBuffer.pos = 0
                outBuffer.dst = UnsafeMutableRawPointer(bufferBytes)
                outBuffer.size = bufferLength

                let remainingToFlush = ZSTD_endStream(stream, &outBuffer)
                guard remainingToFlush == 0 else {
                    throw CompilerError("zstd did return data to flush")
                }

                return outBuffer.pos
            }

            output.append(buffer[0..<written])
        }

        return output
    }

    class func decompress(data: Data) throws -> Data {
        guard let dstream = ZSTD_createDStream() else {
            throw CompilerError("Failed to create decompression stream")
        }
        defer {
            ZSTD_freeDStream(dstream)
        }

        let bufferSize = ZSTD_DStreamOutSize()

        var buffer = Data()
        buffer.reserveCapacity(bufferSize)

        /* In more complex scenarios, a file may consist of multiple appended frames (ex : pzstd).
         *  The following example decompresses only the first frame.
         *  It is compatible with other provided streaming examples */
        let initResult = ZSTD_initDStream(dstream)
        guard ZSTD_isError(initResult)  == 0 else {
            throw CompilerError("zstd: \(String(cString: ZSTD_getErrorName(initResult)))")
        }

        var output = Data()
        output.reserveCapacity(data.count * 4)

        try buffer.withUnsafeMutablePointer { (bufferBytes: UnsafeMutablePointer<UInt8>) in
            try data.withUnsafePointer { (inputBytes: UnsafePointer<UInt8>) in
                var inBuffer = ZSTD_inBuffer()
                inBuffer.src = UnsafeRawPointer(inputBytes)
                inBuffer.size = data.count
                inBuffer.pos = 0

                var outBuffer = ZSTD_outBuffer()
                outBuffer.dst = UnsafeMutableRawPointer(bufferBytes)
                outBuffer.pos = 0
                outBuffer.size = bufferSize

                while inBuffer.pos < data.count {
                    let result = ZSTD_decompressStream(dstream, &outBuffer, &inBuffer)
                    guard ZSTD_isError(result) == 0 else {
                        throw CompilerError("zstd: \(String(cString: ZSTD_getErrorName(result)))")
                    }

                    let written = outBuffer.pos

                    output.append(bufferBytes, count: written)
                    outBuffer.pos = 0
                }
            }
        }

        return output
    }
    
    class func isZstdCompressed(data: Data) -> Bool {
        guard data.count >= MemoryLayout<UInt32>.size else {
            return false
        }

        let magic = data.withUnsafeBytes { rawBytes in
            rawBytes.load(as: UInt32.self)
        }
        
        return magic == ZSTD_MAGICNUMBER
    }
}
