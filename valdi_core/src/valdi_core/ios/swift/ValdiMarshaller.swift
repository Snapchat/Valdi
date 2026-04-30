import Foundation
import valdi_core
import ValdiCoreCPP

public func withMarshaller<T>(_ body: (ValdiMarshaller) throws -> T) rethrows -> T {
    let marshaller: ValdiMarshaller = ValdiMarshaller()
    return try body(marshaller)
}

public enum ValdiError: Error, LocalizedError {
    case runtimeError(String)
    case functionCallFailed(String)
    case invalidEnumValue(String, String)

    public var errorDescription: String? {
        switch self {
        case .runtimeError(let message):
            return message
        case .functionCallFailed(let function):
            return "Unknown exception calling \(function)"
        case .invalidEnumValue(let type, let value):
            return "Invalid enum type: \(type) value: \(value)"
        }
    }
}

public class ValdiFunction {
    let callable: ((ValdiMarshaller) -> Bool)?
    let functionRef: SwiftValueFunctionPtr?
    public init (callable: @escaping (ValdiMarshaller) -> Bool) {
        self.callable = callable
        self.functionRef = nil 
    }

    // Initializes a ValdiFunction with a function reference.
    // This will take ownership of the function reference and release it when deinitialized.
    init (functionRef: SwiftValueFunctionPtr) {
        self.callable = nil
        self.functionRef = functionRef
    }

    deinit {
        if let functionRef = functionRef {
            SwiftValdiMarshaller_ReleaseFunction(functionRef)
        }
    }

    // Calls the callable or functionRef that this was initialized with.
    // Returns true on success. Returns false if the function failed with an error on the marshaller.
    public func perform(with marshaller: ValdiMarshaller, sync: Bool) -> Bool {
        if let callable {
            return callable(marshaller)
        } else if let functionRef {
            return SwiftValdiMarshaller_CallFunction(functionRef, marshaller.marshallerCpp, sync)
        } else {
            marshaller.setError("ValdiFunction was not initialized with a callable or functionRef")
            return false
        }
    }
}

fileprivate func cfStringToString(_ cfString: Unmanaged<CFString>) -> String {
    return cfString.takeUnretainedValue() as String
}

/*
 * This class exists to convert Swift data types to the internal Valdi::Value()
 * data types so it can be passed to other languages.
 * The marshaller can be viewed as a stack that values are placed into.
 * Operations (Like setting an object's property) are performed by using the data
 * at the top of the stack.
 */ 
open class ValdiMarshaller {
    public var marshallerCpp: SwiftValdiMarshallerRef
    let ownsMarshaller: Bool
    public init() {
        marshallerCpp = SwiftValdiMarshaller_Create()
        ownsMarshaller = true 
    }

    public init(_ marshallerCpp: SwiftValdiMarshallerRef) {
        self.marshallerCpp = marshallerCpp
        self.ownsMarshaller = false
    }

    deinit {
        if ownsMarshaller {
            SwiftValdiMarshaller_Destroy(marshallerCpp)
        }
    }

    public func size() -> Int {
        return SwiftValdiMarshaller_Size(marshallerCpp)
    }

    public func pop() {
        SwiftValdiMarshaller_Pop(marshallerCpp)
    }

    public func pop(count: Int) {
        SwiftValdiMarshaller_PopCount(marshallerCpp, count)
    }

    public func checkError() throws {
        if SwiftValdiMarshaller_CheckError(marshallerCpp) {
            guard let cfString = SwiftValdiMarshaller_GetError(marshallerCpp) else {
                throw ValdiError.runtimeError("ValdiMarshaller.checkError unknown error")
            }
            let error = cfStringToString(cfString)
            throw ValdiError.runtimeError(error)
        }
    }

    public func setError(_ error: String) {
        return error.withCString { errorC in
            SwiftValdiMarshaller_SetError(marshallerCpp, errorC)
        }
    }

    /// Converts the value to a string.
    /// - Parameter: index of object to convert
    /// - Parameter: indent. If the string should be indented
    /// - Returns: The string representation of the object
    /// - Throws: ValdiError.runtimeError if index is invalid
    public func toString(index: Int, indent: Bool) throws -> String {
        guard let cfString = SwiftValdiMarshaller_ToString(marshallerCpp, index, indent) else {
            try checkError()
            throw ValdiError.runtimeError("ValdiMarshaller.toString unknown error")
        }
        return cfStringToString(cfString)
    }

    public func pushFunction(_ value: ValdiFunction) -> Int {
        return SwiftValdiMarshaller_PushFunction(marshallerCpp, Unmanaged.passUnretained(value).toOpaque())
    }

    public func pushFunction(_ value: @escaping (ValdiMarshaller) -> Bool) -> Int {
        return pushFunction(ValdiFunction(callable: value))
    }
     
    /// Pushes a <type> onto the stack.
    /// - Parameter: initialCapacity
    /// - Returns: The index of the object
    public func pushMap(initialCapacity: Int) -> Int {
        return SwiftValdiMarshaller_PushMap(marshallerCpp, initialCapacity)
    }
    public func pushArray(capacity: Int) -> Int {
        return SwiftValdiMarshaller_PushArray(marshallerCpp, capacity)
    }
    public func pushDouble(_ value: Double) -> Int {
        return SwiftValdiMarshaller_PushDouble(marshallerCpp, value)
    }
    public func pushLong(_ value: Int64) -> Int {
        return SwiftValdiMarshaller_PushLong(marshallerCpp, value)
    }
    public func pushBool(_ value: Bool) -> Int {
        return SwiftValdiMarshaller_PushBool(marshallerCpp, value)
    }
    public func pushInt(_ value: Int32) -> Int {
        return SwiftValdiMarshaller_PushInt(marshallerCpp, value)
    }
    public func pushString(_ value: String) -> Int {
        return value.utf8.withContiguousStorageIfAvailable { ptr in
            ptr.withMemoryRebound(to: CChar.self) { ptr in
                let strview = SwiftStringView(buf: ptr.baseAddress, size: ptr.count)
                return SwiftValdiMarshaller_PushString(marshallerCpp, strview)
            }
        } ?? {
            let bytes = value.utf8CString
            return bytes.withUnsafeBufferPointer { ptr in
                let strview = SwiftStringView(buf: ptr.baseAddress, size: ptr.count - 1)
                return SwiftValdiMarshaller_PushString(marshallerCpp, strview)
            }
        }()
    }
    public func pushData(_ value: Data) -> Int {
        return value.withUnsafeBytes { bytes in
            return SwiftValdiMarshaller_PushData(marshallerCpp, bytes.baseAddress?.assumingMemoryBound(to: UInt8.self), value.count)
        }
    }
    public func pushUndefined() -> Int {
        return SwiftValdiMarshaller_PushUndefined(marshallerCpp)
    }
    public func pushNull() -> Int {
        return SwiftValdiMarshaller_PushNull(marshallerCpp)
    }

    public func pushEnum(_ value: any ValdiMarshallableIntEnum) -> Int {
        return pushInt(value.rawValue)
    }

    public func pushEnum(_ value: any ValdiMarshallableStringEnum) -> Int {
        return pushString(value.rawValue)
    }

     /// Pushes an empty typed Object of the given type onto the stack.
     /// - Parameter: Class name of the object
     /// - Returns: The index of the object
    public func pushObject(className: String) throws -> Int {
        let registryCpp = ValdiMarshallableObjectRegistry.shared.registryCpp
        let index = SwiftValdiMarshaller_PushObject(marshallerCpp, registryCpp, className.toCString())
        if index < 0 {
            throw ValdiError.runtimeError("Class schema \(className) not found")
        }
        return index
    }

    /// Pushes an instance of a typed object onto the stack.
    public func pushObject(_ object: ValdiMarshallable) throws -> Int {
        return try object.push(to: self)
    }

    public func pushObjCObject(_ object: AnyObject) throws -> Int {
        guard let valdiObject = object as? SCValdiMarshallable else {
            throw ValdiError.runtimeError("pushObjCObject object must be a SCValdiMarshallableObject")
        }
        return try valdiObject.push?(to: OpaquePointer(marshallerCpp)) ?? {
            throw ValdiError.runtimeError("pushToValdiMarshaller is nil")
        }()
    }

    /// Pushes an instance of a generic object onto the stack.
    public func pushGenericObject(_ object: ValdiMarshallable) throws -> Int {
        return try pushObject(object)
    }

     /// Pushes an proxy typed object onto the stack.
     /// - Parameter: The swift object to push
     /// - Parameter: doPush closure that pushes a typed object onto the stack.
     /// - Returns: The index of the object
    public func pushProxy(object: AnyObject, doPush: () throws -> Void) throws -> Int {
        let pointer = Unmanaged.passUnretained(object).toOpaque()
        try doPush()
        return SwiftValdiMarshaller_PushProxyObject(marshallerCpp, pointer)
    }

    public func pushInterface(_ object: ValdiMarshallableInterface) throws -> Int {
        return try object.push(to: self)
    }

    public func pushPromise<T>(_ object: ValdiPromise<T>) throws -> Int {
        try object.push(to: self)
    }

    public func push(_ object: ValdiMarshallable) throws -> Int {
        return try object.push(to: self)
    }

    /// Pushes a map iterator onto the stack, storing its state. Use `pushMapIteratorNext` to advance.
    /// - Parameter: Index of map to iterate.
    /// - Returns: Index of object.
    /// - Throws: If mapIndex is invalid.
    public func pushMapIterator(_ mapIndex: Int) throws -> Int {
        let objIndex = SwiftValdiMarshaller_PushMapIterator(marshallerCpp, mapIndex)
        try checkError()
        return objIndex
    }

    /// Places the iterator's current key, then value on top of the stack and advances the iterator.
    /// The Value ends up on top of the stack, and the key is below it.
    /// - Parameter: Index of map iterator.
    /// - Throws: If the index is invalid.
    public func pushMapIteratorNext(_ iteratorIndex: Int) throws -> Bool {
        let success = SwiftValdiMarshaller_PushMapIteratorNext(marshallerCpp, iteratorIndex)
        try checkError()
        return success
    }

    /// Uses the item returned from putItem and sets it as the property of the map at mapIndex.
    /// - Parameter: property name to set
    /// - Parameter: mapIndex of map to set property on
    /// - Parameter: putItem closure that must push one item onto the stack.
    /// - Throws: On invalid map index or if putItem didn't push an item.
    public func putMapProperty(_ property: String, _ mapIndex: Int, putItem: () throws -> Int) throws {
        _ = try putItem()
        try moveTopItemIntoMap(property, mapIndex: mapIndex)
    }

    /// Returns the value at the given index as Swift <type>
    /// - Parameter: index of value
    /// - Returns: value as <type>
    /// - Throws: If index is invalid or wrong type
    public func getBool(_ index: Int) throws -> Bool {
        let value = SwiftValdiMarshaller_GetBool(marshallerCpp, index)
        try checkError()
        return value
    }
    public func getInt(_ index: Int) throws -> Int32 {
        let value = SwiftValdiMarshaller_GetInt(marshallerCpp, index)
        try checkError()
        return value
    }
    public func getLong(_ index: Int) throws -> Int64 {
        let value = SwiftValdiMarshaller_GetLong(marshallerCpp, index)
        try checkError()
        return value
    }
    public func getDouble(_ index: Int) throws -> Double {
        let value = SwiftValdiMarshaller_GetDouble(marshallerCpp, index)
        try checkError()
        return value
    }
    public func getString(_ index: Int) throws -> String {
        guard let strbox = SwiftValdiMarshaller_GetString(marshallerCpp, index) else {
            // SwiftValdiMarshaller_GetString returns nullptr for empty string
            return ""
        }
        defer { SwiftValdiMarshaller_ReleaseStringBox(strbox) }
        let strview = SwiftValdiMarshaller_StringViewFromStringBox(strbox)
        if strview.size == 0 {
            return ""
        }
        let data = Data(bytesNoCopy: UnsafeMutableRawPointer(mutating:strview.buf), count: strview.size, deallocator:.none)
        guard let str = String(data:data, encoding:.utf8) else {
            throw ValdiError.runtimeError("failed to encode string into utf8")
        }
        return str
    }
    public func getData(_ index: Int) throws -> Data {
        guard let arrayPointer = SwiftValdiMarshaller_GetData(marshallerCpp, index) else {
            try checkError()
            return Data()
        }
        let arraySize = SwiftValdiMarshaller_GetDataSize(marshallerCpp, index)
        try checkError()
        return Data(bytes: arrayPointer, count: arraySize)
    }

    public func getEnum<T: ValdiMarshallableEnum>(_ index: Int) throws -> T {
        return try T(from: self, at: index)
    }

    public func getFunction(_ index: Int) throws -> ValdiFunction {
        guard let functionRef = SwiftValdiMarshaller_GetFunction(marshallerCpp, index) else {
            throw ValdiError.runtimeError("Function not found")
        }
        try checkError()
        return ValdiFunction(functionRef: functionRef)
    }

    public func getPromise<T>(_ index: Int) throws -> ValdiPromise<T> {
        return try ValdiPromise<T>(from: self, at: index)
    }

    public func getObject<T: ValdiMarshallableObject>(_ index: Int) throws -> T {
        return try T(from: self, at: index)
    }

    public func get<T: ValdiConstructable>(_ index: Int) throws -> T {
        return try T(from: self, at: index)
    }

    /// Returns the underlying Swift object from a proxy object.
    /// - Parameter: index of proxy object
    public func getObject<T>(_ index: Int) throws -> T {
        guard let pointer = SwiftValdiMarshaller_GetProxyObject(marshallerCpp, index) else {
            throw ValdiError.runtimeError("Proxy object not found")
        }
        try checkError()
        if let instance = Unmanaged<AnyObject>.fromOpaque(pointer).takeUnretainedValue() as? T {
            return instance
        } else {
            throw ValdiError.runtimeError("Proxy object is not of type \(T.self)")
        }
    }

    public func getObjCObject<T>(_ index: Int) throws -> T {
        if let objcType = T.self as? SCValdiMarshallable.Type {
            if let instance = SCValdiMarshallableObjectUnmarshall(OpaquePointer(marshallerCpp), index, objcType) as? T {
                return instance               
            } else {
                throw ValdiError.runtimeError("Proxy object is not of type \(T.self)")
            }
        }
        else {
            throw ValdiError.runtimeError("Not a marshallable object \(T.self)")
        }
    }

    public func getGenericObject<T>(_ index: Int) throws -> T {
        return try getObject(index)
    }

    public func getGenericTypeParameter<T>(_ index: Int) throws -> T {
        switch T.self {
        case is Bool.Type:
            return try getBool(index) as! T
        case is Int32.Type:
            return try getInt(index) as! T
        case is Int64.Type:
            return try getLong(index) as! T
        case is Double.Type:
            return try getDouble(index) as! T
        case is String.Type:
            return try getString(index) as! T
        case is Data.Type:
            return try getData(index) as! T
        case let enumType as any ValdiMarshallableEnum.Type:
            return try enumType.init(from: self, at: index) as! T
        case let marshallableType as ValdiMarshallableObject.Type:
            return try marshallableType.init(from: self, at: index) as! T
        case let objcType as SCValdiMarshallable.Type:
            return SCValdiMarshallableObjectUnmarshall(OpaquePointer(marshallerCpp), index, objcType) as! T
        default:
            throw ValdiError.runtimeError("getGenericTypeParameter<\(String(describing: T.self))>(): Type is not marshallable")
        }
    }
    // primitive getters
    public func getTypedObjectPropertyBool(_ objectIndex: Int, _ propertyIndex: Int) throws -> Bool {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getBool)
    }
    public func getTypedObjectPropertyInt(_ objectIndex: Int, _ propertyIndex: Int) throws -> Int32 {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getInt)
    }
    public func getTypedObjectPropertyLong(_ objectIndex: Int, _ propertyIndex: Int) throws -> Int64 {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getLong)
    }
    public func getTypedObjectPropertyDouble(_ objectIndex: Int, _ propertyIndex: Int) throws -> Double {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getDouble)
    }
    public func getTypedObjectPropertyString(_ objectIndex: Int, _ propertyIndex: Int) throws -> String {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getString)
    }
    public func getTypedObjectPropertyData(_ objectIndex: Int, _ propertyIndex: Int) throws -> Data {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getData)
    }
    public func getTypedObjectPropertyOptionalBool(_ objectIndex: Int, _ propertyIndex: Int) throws -> Bool? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalBool)
    }
    public func getTypedObjectPropertyOptionalInt(_ objectIndex: Int, _ propertyIndex: Int) throws -> Int32? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalInt)
    }
    public func getTypedObjectPropertyOptionalLong(_ objectIndex: Int, _ propertyIndex: Int) throws -> Int64? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalLong)
    }
    public func getTypedObjectPropertyOptionalDouble(_ objectIndex: Int, _ propertyIndex: Int) throws -> Double? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalDouble)
    }
    public func getTypedObjectPropertyOptionalString(_ objectIndex: Int, _ propertyIndex: Int) throws -> String? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalString)
    }
    public func getTypedObjectPropertyOptionalData(_ objectIndex: Int, _ propertyIndex: Int) throws -> Data? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalData)
    }
    //enum getters
    public func getTypedObjectPropertyEnum<T: ValdiMarshallableEnum>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getEnum)
    }
    public func getTypedObjectPropertyOptionalEnum<T: ValdiMarshallableEnum>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalEnum)
    }
    // object getters
    public func getTypedObjectPropertyObject<T: ValdiMarshallableObject>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getObject)
    }
    public func getTypedObjectPropertyOptionalObject<T: ValdiMarshallableObject>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalObject)
    }
    public func getTypedObjectPropertyObject<T>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getObject)
    }
    public func getTypedObjectPropertyOptionalObject<T>(_ objectIndex: Int, _ propertyIndex: Int) throws -> T? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalObject)
    }
    // Function getters
    public func getTypedObjectPropertyFunction(_ objectIndex: Int, _ propertyIndex: Int) throws -> ValdiFunction {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getFunction)
    }
    public func getTypedObjectPropertyOptionalFunction(_ objectIndex: Int, _ propertyIndex: Int) throws -> ValdiFunction? {
        return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: getOptionalFunction)
    }

    // primitive setters
    public func setTypedObjectPropertyBool(_ objectIndex: Int, _ propertyIndex: Int, _ value: Bool) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushBool(value) }
    }
    public func setTypedObjectPropertyInt(_ objectIndex: Int, _ propertyIndex: Int, _ value: Int32) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushInt(value) }
    }
    public func setTypedObjectPropertyLong(_ objectIndex: Int, _ propertyIndex: Int, _ value: Int64) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushLong(value) }
    }
    public func setTypedObjectPropertyDouble(_ objectIndex: Int, _ propertyIndex: Int, _ value: Double) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushDouble(value) }
    }
    public func setTypedObjectPropertyString(_ objectIndex: Int, _ propertyIndex: Int, _ value: String) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushString(value) }
    }
    public func setTypedObjectPropertyData(_ objectIndex: Int, _ propertyIndex: Int, _ value: Data) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushData(value) }
    }
    public func setTypedObjectPropertyOptionalBool(_ objectIndex: Int, _ propertyIndex: Int, _ value: Bool?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalBool(value) }
    }
    public func setTypedObjectPropertyOptionalInt(_ objectIndex: Int, _ propertyIndex: Int, _ value: Int32?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalInt(value) }
    }
    public func setTypedObjectPropertyOptionalLong(_ objectIndex: Int, _ propertyIndex: Int, _ value: Int64?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalLong(value) }
    }
    public func setTypedObjectPropertyOptionalDouble(_ objectIndex: Int, _ propertyIndex: Int, _ value: Double?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalDouble(value) }
    }
    public func setTypedObjectPropertyOptionalString(_ objectIndex: Int, _ propertyIndex: Int, _ value: String?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalString(value) }
    }
    public func setTypedObjectPropertyOptionalData(_ objectIndex: Int, _ propertyIndex: Int, _ value: Data?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalData(value) }
    }
    // enum setters
    public func setTypedObjectPropertyEnum(_ objectIndex: Int, _ propertyIndex: Int, _ value: any ValdiMarshallableIntEnum) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushEnum(value) }
    }
    public func setTypedObjectPropertyOptionalEnum(_ objectIndex: Int, _ propertyIndex: Int, _ value: (any ValdiMarshallableIntEnum)?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalEnum(value) }
    }
    public func setTypedObjectPropertyEnum(_ objectIndex: Int, _ propertyIndex: Int, _ value: any ValdiMarshallableStringEnum) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushEnum(value) }
    }
    public func setTypedObjectPropertyOptionalEnum(_ objectIndex: Int, _ propertyIndex: Int, _ value: (any ValdiMarshallableStringEnum)?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = pushOptionalEnum(value) }
    }
    // object setters
    public func setTypedObjectPropertyObject(_ objectIndex: Int, _ propertyIndex: Int, _ value: any ValdiMarshallableObject) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushObject(value) }
    }
    public func setTypedObjectPropertyOptionalObject(_ objectIndex: Int, _ propertyIndex: Int, _ value: (any ValdiMarshallableObject)?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushOptionalObject(value) }
    }
    public func setTypedObjectPropertyObject(_ objectIndex: Int, _ propertyIndex: Int, _ value: any ValdiMarshallableInterface) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushObject(value) }
    }
    public func setTypedObjectPropertyOptionalObject(_ objectIndex: Int, _ propertyIndex: Int, _ value: (any ValdiMarshallableInterface)?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushOptionalObject(value) }
    }
    // Function setters
    public func setTypedObjectPropertyFunction(_ objectIndex: Int, _ propertyIndex: Int, _ value: @escaping ((ValdiMarshaller) -> Bool)) throws -> Void {        
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushFunction(value) }
    }
    public func setTypedObjectPropertyOptionalFunction(_ objectIndex: Int, _ propertyIndex: Int, _ value: ((ValdiMarshaller) -> Bool)?) throws -> Void {
        return try setTypedObjectProperty(objectIndex, propertyIndex) { _ = try pushOptionalFunction(value) }
    }

    public func pushGenericTypeParameter<T>(_ value: T) throws -> Int {
        switch value {
        case let boolValue as Bool:
            return pushBool(boolValue)
        case let intValue as Int32:
            return pushInt(intValue)
        case let longValue as Int64:
            return pushLong(longValue)
        case let doubleValue as Double:
            return pushDouble(doubleValue)
        case let stringValue as String:
            return pushString(stringValue)
        case let dataValue as Data:
            return pushData(dataValue)
        case let intEnumValue as any ValdiMarshallableIntEnum:
            return pushEnum(intEnumValue)
        case let strEnumValue as any ValdiMarshallableStringEnum:
            return pushEnum(strEnumValue)
        case let objValue as ValdiMarshallableObject:
            return try objValue.push(to:self)
        case let objcValue as SCValdiMarshallable:
            return try pushObjCObject(objcValue)
        default:
            throw ValdiError.runtimeError("pushGenericTypeParameter<\(String(describing: T.self))>(): Object is not marshallable")
        }
    }

    /// Unwraps a proxy object, pushing the Value typed object onto the stack.
    /// - Parameter: index of proxy object
    /// - Returns: The index of the unwrapped object
    public func unwrapProxy(_ index: Int) throws -> Int {
        let index = SwiftValdiMarshaller_UnwrapProxyObject(marshallerCpp, index)
        try checkError()
        return index;
    }

    /// Returns the value at the given index as a Swift map.
    /// - Parameter: index of value
    /// - Parameter: itemFactory closure that converts the top value of the stack to type T
    /// - Returns: Map value
    /// - Throws: If index is invalid or wrong type 
    public func getMap<T>(_ mapIndex: Int, itemFactory: (Int) throws -> T) throws -> [String:T] {
        var dictionary: [String:T] = [:]
        let iteratorIndex = try pushMapIterator(mapIndex)
        while (try pushMapIteratorNext(iteratorIndex)) {
            let key = try getString(-2)
            let value = try itemFactory(-1)
            dictionary[key] = value
            pop(count: 2)
        }
        pop()
        return dictionary
    }

    /// Pushes a Swift map onto the stack.
    /// - Parameter: map to push
    /// - Parameter: itemMarshaller closure that pushes value of type T onto the stack and returns its index
    /// - Returns: Index of the map
    public func pushMap<T>(_ map: [String: T], itemMarshaller: (T) throws -> Int) throws -> Int {
        let mapIndex = pushMap(initialCapacity: map.count)
        for (key, value) in map {
            try putMapProperty(key, mapIndex) {
                try itemMarshaller(value)
            }
        }
        return mapIndex
    }

    /// Pushes a map of untyped values onto the stack.
    /// - Parameter: index of value
    /// - Throws: If a value cant be pushed onto the stack
    public func pushUntypedMap(_ map: [String: Any?]) throws -> Int {
        let objectIndex = try pushMap(map) {
            return try pushUntyped($0)
        }
        return objectIndex
    }

    /// Sets an array's item as the top value on the stack.
    /// - Parameter: Index of the array 
    /// - Parameter: index of item to set
    /// - Throws: If index or arrayIndex is invalid
    public func setArrayItem(_ index: Int, _ arrayIndex: Int) throws {
        SwiftValdiMarshaller_SetArrayItem(marshallerCpp, index, arrayIndex)
        try checkError()
    }

    /// Returns the size of the array at the given index.
    /// - Parameter: Index of the array 
    /// - Returns: size of array
    /// - Throws: If index is invalid
    public func getArraySize(_ index: Int) throws -> Int {
        let arraySize = SwiftValdiMarshaller_GetArraySize(marshallerCpp, index)
        try checkError()
        return arraySize
    }

    /// Places the array item at the given index on top of the stack.
    /// - Parameter: Index of the array 
    /// - Parameter: Index of item in the array
    /// - Returns: size of array
    /// - Throws: If index or arrayIndex is invalid
    public func putArrayItemOnTop(_ index: Int, _ arrayIndex: Int) throws {
        SwiftValdiMarshaller_GetArrayItem(marshallerCpp, index, arrayIndex)
        try checkError()
    }

    /// Get the array at the given index as a Swift typed array.
    /// - Parameter: Index of the array
    /// - Parameter: itemFactory closure that converts the top value of the stack to type T
    /// - Returns: Array of type [T]
    /// - Throws: If index is invalid or wrong type
    public func getArray<T>(_ index: Int, itemFactory: (Int) throws -> T) throws -> [T] {
        let arraySize = try getArraySize(index)
        guard arraySize > 0 else { return [] }

        var array: [T] = []
        for i in 0..<arraySize {
            try putArrayItemOnTop(index, i)
            array.append(try itemFactory(-1))
            pop()
        }
        return array
    }

    /// Pushes a Swift array onto the stack.
    /// - Parameter: list to push
    /// - Parameter: itemMarshaller closure that pushes value of type T onto the stack and returns its index
    /// - Returns: Index of the array
    public func pushArray<T>(_ list: [T], itemMarshaller: (T) throws -> Int) throws -> Int {
        let objectIndex = pushArray(capacity: list.count)
        for (i, item) in list.enumerated() {
            _ = try itemMarshaller(item)
            try setArrayItem(objectIndex, i)
        }
        return objectIndex
    }

    /// Moves the top item into the map at the given index.
    /// - Parameter: property name to set
    /// - Parameter: mapIndex of map to set property on
    /// - Throws: mapIndex is invalid
    private func moveTopItemIntoMap(_ property: String, mapIndex: Int) throws {
        property.withCString { propertyC in
            SwiftValdiMarshaller_PutMapProperty(marshallerCpp, propertyC, mapIndex)
        }
        try checkError()
    }

    /// Moves the typed object property to the top of the stack.
    /// - Parameter: objectIndex of object
    /// - Parameter: propertyIndex of property
    /// - Throws: If objectIndex or propertyIndex is invalid
    private func moveTypedObjectPropertyToTop(_ objectIndex: Int, _ propertyIndex: Int) throws {
        SwiftValdiMarshaller_GetTypedObjectProperty(marshallerCpp, objectIndex, propertyIndex)
        try checkError()
    }

    /// Moves the map property to the top of the stack.
    /// - Parameter: mapIndex of map
    /// - Parameter: propertyName of property
    /// - Throws: If mapIndex or propertyName is invalid
    private func moveMapPropertyToTop(_ mapIndex: Int, _ propertyName: String) throws {
        if !propertyName.withCString ({ SwiftValdiMarshaller_GetMapProperty(marshallerCpp, $0, mapIndex) }) {
            throw ValdiError.runtimeError("Couldn't move map property to top of stack")
        }
        try checkError()
    }
    
    /// Sets the typed object property at the given index.
    /// - Parameter: objectIndex of object
    /// - Parameter: propertyIndex of property
    /// - Parameter: doPush closure that pushes the value of the property onto the stack
    /// - Throws: If objectIndex or propertyIndex is invalid
    public func setTypedObjectProperty(_ objectIndex: Int, _ propertyIndex: Int, doPush: () throws -> Void) throws {
        try doPush()
        defer { pop() }
        SwiftValdiMarshaller_SetTypedObjectProperty(marshallerCpp, objectIndex, propertyIndex)
        try checkError()
    }

    /// Gets the typed object property at the given index.
    /// - Parameter: objectIndex of object
    /// - Parameter: propertyIndex of property
    /// - Parameter: doGet closure that converts the top value of the stack to type T
    /// - Returns: Value of property as type T
    /// - Throws: Invalid object index or property index and types
    public func getTypedObjectProperty<T>(_ objectIndex: Int, _ propertyIndex: Int, doGet: (_ valueIndex: Int) throws -> T) throws -> T {
        try moveTypedObjectPropertyToTop(objectIndex, propertyIndex)
        defer { pop() }
        return try doGet(-1)
    }

    /// Gets the map property at the given index.
    /// - Parameter: mapIndex of map
    /// - Parameter: propertyName of property
    /// - Parameter: doGet closure that converts the top value of the stack to type T
    /// - Returns: Value of property as type T
    /// - Throws: Invalid map index or property name and types
    public func getMapProperty<T>(_ mapIndex: Int, _ propertyName: String, doGet: (_ valueIndex: Int) throws -> T) throws -> T {
        try moveMapPropertyToTop(mapIndex, propertyName)
        defer { pop() }
        return try doGet(-1)
    }

    /// Convenience method to get a typed object or map property.
    /// - Parameter: objectIndex of object. Can be a typed object or a map.
    /// - Parameter: propertyIndex of property. Only used if objectIndex is a typed object.
    /// - Parameter: propertyName of property. Only used if objectIndex is a map.
    /// - Parameter: doGet closure that converts the top value of the stack to type T
    /// - Returns: Value of property as type T
    /// - Throws: Invalid object index or property index and types
    public func getTypedObjectOrMapProperty<T>(_ objectIndex: Int, _ propertyIndex: Int, _ propertyName: String, doGet: (_ valueIndex: Int) throws -> T) throws -> T {
        if isTypedObject(objectIndex) {
            return try getTypedObjectProperty(objectIndex, propertyIndex, doGet: doGet)
        } else if isMap(objectIndex) {
            return try getMapProperty(objectIndex, propertyName, doGet: doGet)
        }
        throw ValdiError.runtimeError("Property is neither a typed object nor a map")
    }

    /// Gets the value at the given index as a Swift optional <type>
    /// - Parameter: index of value
    /// - Returns: value as <type> or nil
    /// - Throws: If index is invalid or wrong type
    public func getOptionalDouble(_ index: Int) throws -> Double? {
        return try getOptional(index) { try getDouble($0) }
    }
    public func getOptionalBool(_ index: Int) throws -> Bool? {
        return try getOptional(index) { try getBool($0) }
    }
    public func getOptionalLong(_ index: Int) throws -> Int64? {
        return try getOptional(index) { try getLong($0) }
    }
    public func getOptionalInt(_ index: Int) throws -> Int32? {
        return try getOptional(index) { try getInt($0) }
    }
    public func getOptionalString(_ index: Int) throws -> String? {
        return try getOptional(index) { try getString($0) }
    }
    public func getOptionalFunction(_ index: Int) throws -> ValdiFunction? {
        return try getOptional(index) { try getFunction($0) }
    }
    public func getOptionalEnum<T: ValdiMarshallableEnum>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getEnum($0) }
    }
    public func getOptionalObject<T: ValdiMarshallableObject>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getObject($0) }
    }
    public func getOptionalObject<T>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getObject($0) }
    }
    public func getOptionalGenericObject<T>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getGenericObject($0) }
    }
    public func getOptionalGenericTypeParameter<T>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getGenericTypeParameter($0) }
    }
    public func getOptionalArray<T>(_ index: Int, itemFactory: (Int) throws -> T) throws -> [T]? {
        return try getOptional(index) { try getArray($0, itemFactory: itemFactory) }
    }
    public func getOptionalMap<T>(_ index: Int, itemFactory: (Int) throws -> T) throws -> [String:T]? {
        return try getOptional(index) { try getMap($0, itemFactory: itemFactory) }
    }
    public func getOptionalData(_ index: Int) throws -> Data? {
        return try getOptional(index) { try getData($0) }
    }
    public func getOptionalPromise<T>(_ index: Int) throws -> ValdiPromise<T>? {
        return try getOptional(index) { try getPromise($0) }
    }
    public func getOptionalObjCObject<T>(_ index: Int) throws -> T? {
        return try getOptional(index) { try getObjCObject($0) }
    }

    /// Pushes a Swift optional value onto the stack.
    /// - Parameter: value to push
    /// - Returns: Index of the value
    public func pushOptionalDouble(_ value: Double?) -> Int {
        return pushOptional(value) { pushDouble($0) }
    }
    public func pushOptionalBool(_ value: Bool?) -> Int {
        return pushOptional(value) { pushBool($0) }
    }
    public func pushOptionalLong(_ value: Int64?) -> Int {
        return pushOptional(value) { pushLong($0) }
    }
    public func pushOptionalInt(_ value: Int32?) -> Int {
        return pushOptional(value) { pushInt($0) }
    }
    public func pushOptionalString(_ value: String?) -> Int {
        return pushOptional(value) { pushString($0) }
    }
    public func pushOptionalFunction(_ value: ValdiFunction?) -> Int {
        return pushOptional(value) { pushFunction($0) }
    }
    public func pushOptionalFunction(_ value: ((ValdiMarshaller) -> Bool)?) -> Int {
        return pushOptional(value) { pushFunction($0) }
    }
    public func pushOptionalEnum(_ value: (any ValdiMarshallableIntEnum)?) -> Int {
        return pushOptional(value) { pushEnum($0) }
    }
    public func pushOptionalEnum(_ value: (any ValdiMarshallableStringEnum)?) -> Int {
        return pushOptional(value) { pushEnum($0) }
    }
    public func pushOptionalObject(_ value: (any ValdiMarshallable)?) throws -> Int {
        return try pushOptional(value) { try pushObject($0) }
    }
    public func pushOptionalGenericObject(_ value: ValdiMarshallable?) throws -> Int {
        return try pushOptional(value) { try pushGenericObject($0) }
    }
    public func pushOptionalGenericTypeParameter<T>(_ value: T?) throws -> Int {
        return try pushOptional(value) { try pushGenericTypeParameter($0) }
    }
    public func pushOptionalMap<T>(_ map: [String: T]?, itemMarshaller: (T) throws -> Int) throws -> Int {
        return try pushOptional(map) { try pushMap($0, itemMarshaller: itemMarshaller) }
    }
    public func pushOptionalArray<T>(_ list: [T]?, itemMarshaller: (T) throws -> Int) throws -> Int {
        return try pushOptional(list) { try pushArray($0, itemMarshaller: itemMarshaller) }
    }
    public func pushOptionalData(_ value: Data?) -> Int {
        return pushOptional(value) { pushData($0) }
    }
    public func pushOptionalPromise<T>(_ value: ValdiPromise<T>?) throws -> Int {
        return try pushOptional(value) { try pushPromise($0) }
    }
    public func pushOptionalObjCObject(_ value: AnyObject?) throws -> Int {
        return try pushOptional(value) { try pushObjCObject($0) }
    }

    /// Gets the value at the given index as a Swift optional <type>
    /// - Parameter: index of value
    /// - Parameter: getItem closure that converts the top value of the stack to type T. Only called if the value exists.
    /// - Returns: value as <type> or nil
    /// - Throws: If index is invalid or wrong type
    public func getOptional<T>(_ index: Int, getItem: (Int) throws -> T) rethrows -> T? {
        if isNullOrUndefined(index) {
            return nil
        }
        return try getItem(index)
    }

    /// Pushes a Swift optional value onto the stack.
    /// - Parameter: optValue to push
    /// - Parameter: pushItem closure that pushes value of type T onto the stack and returns its index. Only called if the value exists.
    public func pushOptional<T>(_ optValue: T?, pushItem: (T) throws -> Int) rethrows -> Int {
        if let value = optValue {
            return try pushItem(value)
        }
        return pushNull()
    }

    /// Returns the type of the value at the given index.
    /// - Parameter: index of value.
    public func isNullOrUndefined(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsNullOrUndefined(marshallerCpp, index)
    }
    public func isBool(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsBool(marshallerCpp, index)
    }
    public func isInt(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsInt(marshallerCpp, index)
    }
    public func isLong(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsLong(marshallerCpp, index)
    }
    public func isDouble(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsDouble(marshallerCpp, index)
    }
    public func isString(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsString(marshallerCpp, index)
    }
    public func isMap(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsMap(marshallerCpp, index)
    }
    public func isArray(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsArray(marshallerCpp, index)
    }
    public func isTypedObject(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsTypedObject(marshallerCpp, index)
    }
    public func isError(_ index: Int) -> Bool {
        return SwiftValdiMarshaller_IsError(marshallerCpp, index)
    }

    public func getOptionalUntyped(_ index: Int) throws -> Any? {
        return try getUntyped(index)
    }

    public func getUntyped(_ index: Int) throws -> Any? {
        if isNullOrUndefined(index) {
            return nil
        } else if isBool(index) {
            return try getBool(index) as Any
        } else if isInt(index) {
            return try getInt(index) as Any
        } else if isLong(index) {
            return try getLong(index) as Any
        } else if isDouble(index) {
            return try getDouble(index) as Any
        } else if isString(index) {
            return try getString(index) as Any
        } else if isArray(index) {
            let array : [Any?] = try getArray(index) {
                try getUntyped($0)
            }
            return array
        } else if isMap(index) {
            let map : [String: Any?] = try getMap(index) {
                try getUntyped($0)
            }
            return map
        } else if isTypedObject(index) {
            let obj : Any = try getObject(index)
            return obj
        }
        throw ValdiError.runtimeError("Retreiving unknown type in marshaller")
    }

    public func pushOptionalUntyped(_ value: Any?) throws -> Int {
        return try pushUntyped(value)
    }

    /// Pushes an untyped value onto the stack.
    /// - Parameter: value to push
    /// - Returns: Index of the value
    /// - Throws: If the value is not a supported type
    public func pushUntyped(_ value: Any?) throws -> Int {
        switch value {
        case nil:
            return pushNull()
            
        case let boolValue as Bool:
            return pushBool(boolValue)
            
        case let int32Value as Int32:
            return pushInt(int32Value)
            
        case let int64Value as Int64:
            return pushLong(int64Value)
            
        case let intValue as Int:
            // Safely converts Int to Int64 instead of force-casting
            return pushLong(Int64(intValue))
            
        case let doubleValue as Double:
            return pushDouble(doubleValue)
            
        case let stringValue as String:
            return pushString(stringValue)
            
        case let dataValue as Data:
            return pushData(dataValue)
            
        case let mapValue as [String: Any?]:
            // Casts map values to Optional to match your original signature
            return try pushUntypedMap(mapValue)
            
        case let arrayValue as [Any?]:
            return try pushArray(arrayValue) { try pushUntyped($0) }
            
        case let marshallableObj as ValdiMarshallableObject:
            return try marshallableObj.push(to: self)
            
        default:
            // Added the type to the error message to make debugging easier
            let typeName = type(of: value)
            throw ValdiError.runtimeError("Pushing unsupported type to marshaller: \(typeName)")
        }
    }
}

public func createStringEnum<T: ValdiMarshallableStringEnum>(from marshaller: ValdiMarshaller, at objectIndex: Int) throws -> T {
    let value = try marshaller.getString(objectIndex)
    guard let enumValue = T(rawValue: value) else {
        throw ValdiError.invalidEnumValue("\(String(describing: T.self))", String(value))
    }
    return enumValue
}

public func createIntEnum<T: ValdiMarshallableIntEnum>(from marshaller: ValdiMarshaller, at objectIndex: Int) throws -> T {
    let value = try marshaller.getInt(objectIndex)
    guard let enumValue = T(rawValue: value) else {
        throw ValdiError.invalidEnumValue("\(String(describing: T.self))", String(value))
    }
    return enumValue
}
