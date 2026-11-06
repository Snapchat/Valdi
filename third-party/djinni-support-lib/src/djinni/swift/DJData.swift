import DjinniSupportCxx
import Foundation

// DataView is a borrowed view of bytes, valid for the call: it's `UnsafeRawBufferPointer` in Swift.
// From C++, the pointer is C++'s; from Swift, the caller keeps the bytes alive for as long as C++ can
// read them (as a `DataView` requires on the C++ side).
public enum DataViewMarshaller: Marshaller {
    public typealias SwiftType = UnsafeRawBufferPointer
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let range = djinni.swift.getBinaryRange(v)
        guard let bytes = range.bytes, range.size > 0 else {
            return SwiftType(start: nil, count: 0)
        }
        return SwiftType(start: bytes, count: range.size)
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.makeRangeValue(s.baseAddress, s.count)
    }
}

/// A djinni `DataRef` in Swift: a reference-counted buffer shared with C++, so it crosses either way
/// without copying. It owns a C++ `DataRef`, whose bytes stay put for as long as it's alive.
/// (`Data` can't be handed to C++ safely: its small-buffer storage has no stable address.)
public final class DJDataRef: @unchecked Sendable {
    // A heap-held C++ DataRef (djinni::swift::makeDataRef / DataRefAdaptor::fromCpp).
    fileprivate let holder: UnsafeRawPointer

    fileprivate init(holder: UnsafeRawPointer) {
        self.holder = holder
    }

    /// A new buffer holding a copy of `bytes`.
    public convenience init(copying bytes: UnsafeRawBufferPointer) {
        self.init(holder: djinni.swift.makeDataRef(bytes.baseAddress, bytes.count)!)
    }

    /// A new buffer holding a copy of `data`.
    public convenience init(_ data: Data) {
        let holder = data.withUnsafeBytes { djinni.swift.makeDataRef($0.baseAddress, $0.count)! }
        self.init(holder: holder)
    }

    deinit {
        djinni.swift.releaseDataRef(holder)
    }

    /// The bytes, valid while this `DJDataRef` is alive.
    public var bytes: UnsafeRawBufferPointer {
        UnsafeRawBufferPointer(start: djinni.swift.dataRefBytes(holder), count: djinni.swift.dataRefSize(holder))
    }
}

extension DJDataRef {
    /// A copy of the bytes.
    public var data: Data {
        Data(bytes)
    }
}

// By content, as records deriving eq / ord compare their fields.
extension DJDataRef: Hashable {
    public static func == (lhs: DJDataRef, rhs: DJDataRef) -> Bool {
        lhs === rhs || lhs.bytes.elementsEqual(rhs.bytes)
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(bytes: bytes)
    }
}

public enum DataRefMarshaller: Marshaller {
    public typealias SwiftType = DJDataRef
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        // Takes over the heap-held DataRef C++ hands over.
        return DJDataRef(holder: djinni.swift.getBinaryRange(v).bytes!)
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        // A new holder sharing the buffer, which C++ takes over: `s` can go away before C++ reads it.
        return djinni.swift.makeRangeValue(djinni.swift.retainDataRef(s.holder), 0)
    }
}
