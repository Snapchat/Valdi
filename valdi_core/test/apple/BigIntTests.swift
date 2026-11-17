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

}
