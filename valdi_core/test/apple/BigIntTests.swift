import XCTest
import ValdiCoreSwift
import valdi_core

final class BigIntTests: XCTestCase {
    func testBigIntTypedAndUntypedRoundTrip() throws {
        let integer = try SCValdiBigInt(string: "18446744073709551616")
        let marshaller = ValdiMarshaller()
        let index = marshaller.pushBigInt(integer)
        XCTAssertFalse(integer.toInt64().success)
        XCTAssertEqual(integer.toInt64().value, 0)
        XCTAssertEqual(try marshaller.getBigInt(index).toString(), "18446744073709551616")
        XCTAssertEqual((try marshaller.getUntyped(index) as? SCValdiBigInt)?.toString(), integer.toString())
        let untypedIndex = try marshaller.pushUntyped(integer)
        XCTAssertTrue(marshaller.isBigInt(untypedIndex))
        let optionalIndex = marshaller.pushOptionalBigInt(nil)
        XCTAssertNil(try marshaller.getOptionalBigInt(optionalIndex))
    }

    func testExactInt64Conversion() {
        for value in [Int64.min, 0, Int64.max] {
            let integer = SCValdiBigInt(int64: value)
            let result = integer.toInt64()
            XCTAssertTrue(result.success)
            XCTAssertEqual(result.value, value)
        }
    }

    func testGetBigIntThrowsSwiftErrors() {
        let marshaller = ValdiMarshaller()

        func assertReadError(at index: Int, contains message: String) {
            do {
                _ = try marshaller.getBigInt(index)
                XCTFail("Expected a Swift marshalling error")
            } catch ValdiError.runtimeError(let reason) {
                XCTAssertTrue(reason.contains(message), reason)
            } catch {
                XCTFail("Unexpected error: \(error)")
            }
        }

        assertReadError(at: 5, contains: "Out of bounds index 5")
        let stringIndex = marshaller.pushString("42")
        assertReadError(at: stringIndex, contains: "Cannot convert type 'interned string' to type 'bigint'")

        let integerIndex = marshaller.pushBigInt(SCValdiBigInt(int64: 42))
        marshaller.setError("pending error")
        assertReadError(at: integerIndex, contains: "pending error")
        XCTAssertEqual(try marshaller.getBigInt(integerIndex).toString(), "42")
    }

}
