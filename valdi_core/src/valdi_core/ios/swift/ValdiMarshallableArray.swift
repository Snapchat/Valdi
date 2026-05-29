/// Marker for `Array` types so `getGenericTypeParameter<T>` can dispatch
/// array unmarshalling without knowing the element type statically.
/// All `Array<Element>` conform automatically; the init recurses through
/// `getGenericTypeParameter` so element type can be anything the marshaller
/// already supports (primitives, enums, `ValdiMarshallableObject`, nested
/// arrays).
public protocol ValdiMarshallableArray {
    init(from marshaller: ValdiMarshaller, at index: Int) throws
}

extension Array: ValdiMarshallableArray {
    public init(from marshaller: ValdiMarshaller, at index: Int) throws {
        self = try marshaller.getArray(index) { itemIndex -> Element in
            try marshaller.getGenericTypeParameter(itemIndex)
        }
    }
}
