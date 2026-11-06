import DjinniSupportCxx
import Foundation

// `outcome<T, E>` is `Result<T, E>`, so its error type must conform to `Error`: records and enums
// get that conformance from `deriving(error)`.

// `outcome<T>` is marshalled as a composite value
// 1. If the `outcome` object contains a successful value, then it's the [0]
//    element in the composite value
// 2. If the `outcome` object contains an error value, then element [0] is void,
//    and element [1] is the error value.
public enum OutcomeMarshaller<Res: Marshaller, Err: Marshaller>: Marshaller
  where Err.SwiftType: Error {
    public typealias SwiftType = Result<Res.SwiftType, Err.SwiftType>
    public static func fromCpp(_ v: djinni.swift.AnyValue) -> SwiftType {
        withUnsafePointer(to: v) { p in
            var r = djinni.swift.getMember(p, 0)
            if (!djinni.swift.isVoidValue(&r)) {
                return .success(Res.fromCpp(r))
            } else {
                let e = djinni.swift.getMember(p, 1)
                return .failure(Err.fromCpp(e))
            }
        }
    }
    public static func toCpp(_ s: SwiftType) -> djinni.swift.AnyValue {
        var comp = djinni.swift.makeCompositeValue()
        switch s {
        case .success(let r):
            djinni.swift.addMember(&comp, Res.toCpp(r))
        case .failure(let e):
            djinni.swift.addMember(&comp, djinni.swift.makeVoidValue())
            djinni.swift.addMember(&comp, Err.toCpp(e))
        }
        return comp
    }
}
