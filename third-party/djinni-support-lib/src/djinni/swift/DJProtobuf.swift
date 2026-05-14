import DjinniSupportCxx
import SwiftProtobuf
import Foundation

public enum ProtobufMarshaller<T: SwiftProtobuf.Message>: Marshaller {
    public typealias SwiftType = T
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let range = djinni.swift.getBinaryRange(v)
        if (range.size > 0) {
            let bin = Data(bytesNoCopy:UnsafeMutableRawPointer(mutating: range.bytes), count:range.size, deallocator:.none)
            return try! T(serializedBytes: bin)
        } else {
            return T()
        }
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        let bin: Data = try! s.serializedData()
        return BinaryMarshaller.toCpp(bin)
    }
}
