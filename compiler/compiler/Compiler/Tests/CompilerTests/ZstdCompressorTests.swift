import XCTest
import Foundation
import Zstd
@testable import Compiler

final class ZstdCompressorTests: XCTestCase {

    func testCompressedFrameContainsContentSize() throws {
        let original = Data("Hello, this is a test payload for zstd frame content size verification.".utf8)

        let compressed = try ZstdCompressor.compress(data: original)

        let contentSize = compressed.withUnsafeBytes { rawBytes in
            ZSTD_getFrameContentSize(rawBytes.baseAddress!, compressed.count)
        }

        XCTAssertNotEqual(contentSize, ZSTD_CONTENTSIZE_UNKNOWN,
                          "Compressed frame must contain a content size field")
        XCTAssertNotEqual(contentSize, ZSTD_CONTENTSIZE_ERROR,
                          "ZSTD_getFrameContentSize returned an error")
        XCTAssertEqual(UInt64(original.count), contentSize,
                       "Frame content size must match original data size")
    }

    func testCompressedFrameIsSingleFrame() throws {
        let original = Data("Single frame verification payload.".utf8)

        let compressed = try ZstdCompressor.compress(data: original)

        let firstFrameSize = compressed.withUnsafeBytes { rawBytes in
            ZSTD_findFrameCompressedSize(rawBytes.baseAddress!, compressed.count)
        }

        XCTAssertFalse(ZSTD_isError(firstFrameSize) != 0,
                       "ZSTD_findFrameCompressedSize should not error")
        XCTAssertEqual(firstFrameSize, compressed.count,
                       "Compressed output must be exactly one frame")
    }

    func testRoundTripPreservesData() throws {
        let original = Data("Round-trip integrity test: the quick brown fox jumps over the lazy dog.".utf8)

        let compressed = try ZstdCompressor.compress(data: original)
        let decompressed = try ZstdCompressor.decompress(data: compressed)

        XCTAssertEqual(original, decompressed)
    }

    func testEmptyDataRoundTrip() throws {
        let original = Data()

        let compressed = try ZstdCompressor.compress(data: original)

        let contentSize = compressed.withUnsafeBytes { rawBytes in
            ZSTD_getFrameContentSize(rawBytes.baseAddress!, compressed.count)
        }

        XCTAssertEqual(UInt64(0), contentSize,
                       "Empty input must report content size of 0")

        let decompressed = try ZstdCompressor.decompress(data: compressed)
        XCTAssertEqual(original, decompressed)
    }

    func testLargeDataContainsContentSize() throws {
        let original = Data(repeating: 0xAB, count: 512 * 1024)

        let compressed = try ZstdCompressor.compress(data: original)

        let contentSize = compressed.withUnsafeBytes { rawBytes in
            ZSTD_getFrameContentSize(rawBytes.baseAddress!, compressed.count)
        }

        XCTAssertEqual(UInt64(original.count), contentSize,
                       "Large payload frame must contain correct content size")

        let decompressed = try ZstdCompressor.decompress(data: compressed)
        XCTAssertEqual(original, decompressed)
    }
}
