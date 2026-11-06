import DjinniSupportCxx
import Foundation

// Common interface for all Swift type marshallers
// The C++ type is always djinni.swift.AnyValue
public protocol Marshaller {
    associatedtype SwiftType
    static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType
    static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue
}
// Djinni generated enums are always backed by Int32
public enum EnumMarshaller<T: RawRepresentable>: Marshaller where T.RawValue == Int32 {
    public typealias SwiftType = T
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return SwiftType(rawValue: djinni.swift.I32.toCpp(v))!
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.I32.fromCpp(s.rawValue)
    }
}
// All integer types smaller than 32-bit are handled by this
public enum SmallIntMarshaller<T: BinaryInteger>: Marshaller {
    public typealias SwiftType = T
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return SwiftType(exactly: djinni.swift.I32.toCpp(v))!
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.I32.fromCpp(Int32(s))
    }
}
// 64-bit integer
public enum I64Marshaller: Marshaller {
    public typealias SwiftType = Int64
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return SwiftType(djinni.swift.I64.toCpp(v))
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        // int64_t is `long` (Swift's Int) on Android and `long long` (Int64) on Apple.
        return djinni.swift.I64.fromCpp(.init(s))
    }
}
// Both float and double are marshalled to double values
public enum FloatMarshaller<T: BinaryFloatingPoint>: Marshaller {
    public typealias SwiftType = T
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return T(djinni.swift.F64.toCpp(v))
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.F64.fromCpp(Double(s))
    }
}
// Bool is marshalled as Int32 (0 or 1)
public enum BoolMarshaller: Marshaller {
    public typealias SwiftType = Bool
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return djinni.swift.I32.toCpp(v) != 0
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.I32.fromCpp(s ? 1 : 0)
    }
}
// Aliases for number types
public typealias I8Marshaller = SmallIntMarshaller<Int8>
public typealias I16Marshaller = SmallIntMarshaller<Int16>
public typealias I32Marshaller = SmallIntMarshaller<Int32>
public typealias F32Marshaller = FloatMarshaller<Float>
public typealias F64Marshaller = FloatMarshaller<Double>

public enum StringMarshaller: Marshaller {
    public typealias SwiftType = String
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let range = djinni.swift.getBinaryRange(v)
        if (range.size > 0) {
            return String(data: Data(bytes: range.bytes, count: range.size), encoding: String.Encoding.utf8) ?? "invalid"
        } else {
            return String()
        }
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        let bytes = s.utf8CString
        return bytes.withUnsafeBufferPointer { ptr in
            return djinni.swift.makeStringValue(UnsafeMutableRawPointer(mutating: ptr.baseAddress!),
                                                ptr.count - 1) // minus terminating \0
        }
    }
}

public enum BinaryMarshaller: Marshaller {
    public typealias SwiftType = Data
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let range = djinni.swift.getBinaryRange(v)
        if (range.size > 0) {
            return Data(bytes: range.bytes, count: range.size)
        } else {
            return Data()
        }
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return s.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in
            return djinni.swift.makeBinaryValue(UnsafeMutableRawPointer(mutating: ptr.baseAddress!),
                                                ptr.count)
        }
    }
}

public enum DateMarshaller: Marshaller {
    public typealias SwiftType = Date
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        let millisecondsSinceEpoch = I64Marshaller.fromCpp(v)
        return Date(timeIntervalSince1970: Double(millisecondsSinceEpoch))
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return I64Marshaller.toCpp(Int64(s.timeIntervalSince1970))
    }
}

public enum Void_Marshaller: Marshaller {
    public typealias SwiftType = Void
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return ()
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        return djinni.swift.makeVoidValue()
    }
}

public enum OptionalMarshaller<T: Marshaller>: Marshaller {
    public typealias SwiftType = T.SwiftType?
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        return withUnsafePointer(to: v) { p in
            if (djinni.swift.isVoidValue(p)) {
                return nil
            }
            return T.fromCpp(v)
        }
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        if (s == nil) {
            return djinni.swift.makeVoidValue()
        } else {
            return T.toCpp(s!)
        }
    }
}

public enum ListMarshaller<T: Marshaller>: Marshaller {
    public typealias SwiftType = [T.SwiftType]
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        var array = SwiftType()
        withUnsafePointer(to: v) { p in
            let size = djinni.swift.getSize(p)
            for i in 0..<size {
                array.append(T.fromCpp(djinni.swift.getMember(p, i)))
            }
        }
        return array
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        var composite = djinni.swift.makeCompositeValue()
        for item in s {
            djinni.swift.addMember(&composite, T.toCpp(item))
        }
        return composite
    }
}

// Swift don't need to box primitive types in arrays so they are identical to list
public typealias ArrayMarshaller<T: Marshaller> = ListMarshaller<T>

public enum SetMarshaller<T: Marshaller>: Marshaller where T.SwiftType: Hashable {
    public typealias SwiftType = Set<T.SwiftType>
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        var s = SwiftType()
        withUnsafePointer(to: v) { p in
            let size = djinni.swift.getSize(p)
            for i in 0..<size {
                s.insert(T.fromCpp(djinni.swift.getMember(p, i)))
            }
        }
        return s
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        var composite = djinni.swift.makeCompositeValue()
        for item in s {
            djinni.swift.addMember(&composite, T.toCpp(item))
        }
        return composite
    }
}

public enum MapMarshaller<K: Marshaller, V: Marshaller>: Marshaller where K.SwiftType: Hashable {
    public typealias SwiftType = Dictionary<K.SwiftType, V.SwiftType>
    static public func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        var s = SwiftType()
        withUnsafePointer(to: v) { p in
            let size = djinni.swift.getSize(p)
            for i in stride(from: 0, to: size, by: 2) {
                s.updateValue(V.fromCpp(djinni.swift.getMember(p, i+1)),
                              forKey: K.fromCpp(djinni.swift.getMember(p, i)))
            }
        }
        return s
    }
    static public func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        var composite = djinni.swift.makeCompositeValue()
        for (k, v) in s {
            djinni.swift.addMember(&composite, K.toCpp(k))
            djinni.swift.addMember(&composite, V.toCpp(v))
        }
        return composite
    }
}
