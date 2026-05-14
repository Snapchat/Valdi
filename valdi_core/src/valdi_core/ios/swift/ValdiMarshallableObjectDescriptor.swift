import Foundation
import ValdiCoreCPP

public typealias CString = UnsafePointer<CChar>?
public typealias CArray<T> = UnsafePointer<T>?
// Code for copying Swift collections to C arrays copies from here:
// https://forums.swift.org/t/create-c-string-array-from-swift/31149/2
extension Collection {
    /// Creates an UnsafeMutableBufferPointer with enough space to hold the elements of self.
    public func unsafeCopy() -> UnsafeMutableBufferPointer<Self.Element> {
        let copy = UnsafeMutableBufferPointer<Self.Element>.allocate(capacity: self.underestimatedCount)
        _ = copy.initialize(from: self)
        return copy
    }

    public func toCArray() -> CArray<Self.Element> {
        return UnsafePointer(self.unsafeCopy().baseAddress)
    }
}

extension String {
    /// Create UnsafeMutableBufferPointer holding a null-terminated UTF8 copy of the string
    public func unsafeUTF8Copy() -> UnsafeMutableBufferPointer<CChar> { self.utf8CString.unsafeCopy() }
    public func toCString() -> CString { UnsafePointer(self.unsafeUTF8Copy().baseAddress)}
}

func deallocateCArray<T>(_ array: CArray<T>) {
    let mutablePointer = UnsafeMutablePointer(mutating: array)
    mutablePointer?.deallocate()
}

func deallocateCString(_ cString: CString) {
    let mutablePointer = UnsafeMutablePointer(mutating: cString)
    mutablePointer?.deallocate()
}

public struct ValdiMarshallableIdentifier {
    var name: String
    var registerFunc: (() -> Void)?
    public init(name: String, registerFunc: (() -> Void)?) {
        self.name = name
        self.registerFunc = registerFunc
    }
}

open class ValdiMarshallableObjectDescriptor {
    private var fields: [ValdiMarshallableObjectFieldDescriptor]
    private var type: ValdiMarshallableObjectType
    private var cDescriptor: SwiftValdiMarshallableObjectRegistry_ObjectDescriptor?
    public var identifiers: [ValdiMarshallableIdentifier] = []

    public init(_ fields: [ValdiMarshallableObjectFieldDescriptor],
                identifiers: [ValdiMarshallableIdentifier]?,
                type: ValdiMarshallableObjectType) {
        self.fields = fields
        self.identifiers = identifiers ?? []
        self.type = type
    }
    deinit {
        deallocCDescriptor()
    }

    public func getOrCreateCDescriptor() -> SwiftValdiMarshallableObjectRegistry_ObjectDescriptor {
        guard let cDescriptor = self.cDescriptor else {
            let cDescriptor = allocCDescriptor()
            self.cDescriptor = cDescriptor
            return cDescriptor
        }
        return cDescriptor
    }

    private func allocCDescriptor() -> SwiftValdiMarshallableObjectRegistry_ObjectDescriptor {
        // Build field descriptors array with a terminator that has null name and type
        var cFieldDescs = self.fields.map {
            SwiftValdiMarshallableObjectRegistry_FieldDescriptor(
                name: $0.name?.toCString(),
                type: $0.type?.toCString()
            )
        }
        cFieldDescs.append(SwiftValdiMarshallableObjectRegistry_FieldDescriptor(name: nil, type: nil))

        // Build identifiers array with a null terminator
        var cIdentifiers : [CString] = self.identifiers.map { $0.name.toCString() }
        cIdentifiers.append(nil)

        return SwiftValdiMarshallableObjectRegistry_ObjectDescriptor(
            fields: cFieldDescs.toCArray(),
            identifiers: cIdentifiers.toCArray(),
            objectType: getCType()
        )
    }

    private func deallocCDescriptor() {
        guard let descriptor = self.cDescriptor else {
            return
        }
        var field = descriptor.fields
        while let currentField = field?.pointee {
            guard currentField.name != nil else {
                break
            }
            deallocateCString(currentField.name)
            deallocateCString(currentField.type)
            field = field?.advanced(by: 1)
        }

        var identifier = descriptor.identifiers
        while identifier?.pointee != nil {
            deallocateCString(identifier?.pointee)
            identifier = identifier?.advanced(by: 1)
        }
        deallocateCArray(descriptor.fields)
        deallocateCArray(descriptor.identifiers)
    }

    private func getCType() -> SwiftValdiMarshallableObjectRegistry_ObjectType {
        switch self.type {
        case .Class:
            return SwiftValdiMarshallableObjectRegistry_ObjectType_Class
        case .Interface:
            return SwiftValdiMarshallableObjectRegistry_ObjectType_Protocol
        case .Function:
            return SwiftValdiMarshallableObjectRegistry_ObjectType_Function
        case .Untyped:
            return SwiftValdiMarshallableObjectRegistry_ObjectType_Untyped
        }
    }
}

open class ValdiMarshallableObjectFieldDescriptor {
    var name: String?
    var type: String?
    public init(name: String?, type: String?) {
        self.name = name
        self.type = type
        if (name == nil) != (type == nil) {
            fatalError("Both name and type must be set or unset")
        }
    }
}

//define enum named ValdiMarshallableObjectType
public enum ValdiMarshallableObjectType {
    case Class
    case Interface
    case Function
    case Untyped
}