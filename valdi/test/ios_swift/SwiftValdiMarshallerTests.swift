import XCTest
import ValdiCoreSwift

protocol ValdiRegisterable {
    static func getDescriptor() -> ValdiMarshallableObjectDescriptor
}

func registerClass<T: ValdiRegisterable>(_ cls: T.Type) {
    ValdiMarshallableObjectRegistry.shared.register(className: String(describing: cls),
                                                       objectDescriptor: T.getDescriptor())
}

func registerProxy<T: ValdiRegisterable>(_ name: String, _ cls: T.Type) {
    ValdiMarshallableObjectRegistry.shared.register(className: name,
                                                       objectDescriptor: T.getDescriptor())
}

class ValdiMarshallerTestDoubleObject : ValdiMarshallableObject, ValdiRegisterable {
    var aDouble: Double = 0
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aDouble = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aDouble") {
            try marshaller.getDouble($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        if (objectIndex >= 0) {
            try marshaller.setTypedObjectProperty(objectIndex, 0) {
                _ = marshaller.pushDouble(self.aDouble)
            }
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aDouble", type: "d"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

public class ValdiMarshallerTestBoolObject : ValdiMarshallableObject, ValdiRegisterable {
    var aBool: Bool = false
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aBool = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aBool") {
            try marshaller.getBool($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushBool(self.aBool)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aBool", type: "b"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

public class ValdiMarshallerTestLongObject : ValdiMarshallableObject, ValdiRegisterable {
    var aLong: Int64 = 0
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aLong = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aLong") {
            try marshaller.getLong($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushLong(self.aLong)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aLong", type: "l"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

public class ValdiMarshallerTestIntObject : ValdiMarshallableObject, ValdiRegisterable {
    var aInt: Int32 = 0
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aInt = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aInt") {
            try marshaller.getInt($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushInt(self.aInt)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aInt", type: "i"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}
class ValdiMarshallerTestOptionalDoubleObject : ValdiMarshallableObject, ValdiRegisterable {
    var anOptionalDouble: Double?
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anOptionalDouble = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anOptionalDouble") {
            try marshaller.getOptionalDouble($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushOptionalDouble(self.anOptionalDouble)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "anOptionalDouble", type: "d@?"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestOptionalBoolObject : ValdiMarshallableObject, ValdiRegisterable {
    var anOptionalBool: Bool?
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anOptionalBool = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anOptionalBool") {
            try marshaller.getOptionalBool($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushOptionalBool(self.anOptionalBool)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "anOptionalBool", type: "b@?"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestOptionalLongObject : ValdiMarshallableObject, ValdiRegisterable {
    var anOptionalLong: Int64?
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anOptionalLong = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anOptionalLong") {
            try marshaller.getOptionalLong($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushOptionalLong(self.anOptionalLong)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "anOptionalLong", type: "l@?"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestOptionalIntObject : ValdiMarshallableObject, ValdiRegisterable {
    var anOptionalInt: Int32?
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anOptionalInt = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anOptionalInt") {
            try marshaller.getOptionalInt($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing: type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushOptionalInt(self.anOptionalInt)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "anOptionalInt", type: "i@?"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestStringObject : ValdiMarshallableObject, ValdiRegisterable {
    var aString: String = ""
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aString = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aString") {
            try marshaller.getString($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushString(self.aString)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aString", type: "s"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestDataObject : ValdiMarshallableObject, ValdiRegisterable {
    var bytes: Data = Data()
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.bytes = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "bytes") {
            try marshaller.getData($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushData(self.bytes)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "bytes", type: "t"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestMapObject : ValdiMarshallableObject, ValdiRegisterable {
    var aMap: [String: Any] = [:]
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aMap = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aMap") {
            try marshaller.getMap($0) {
                try marshaller.getString($0)
            }
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = try marshaller.pushMap(self.aMap) { mapValue in
                try marshaller.pushUntyped(mapValue)
            }
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aMap", type: "m<s, u>"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestArrayObject : ValdiMarshallableObject, ValdiRegisterable {
    var anArray: [String] = []
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anArray = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anArray") {
            try marshaller.getArray($0) {
                try marshaller.getString($0)
            }
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = try marshaller.pushArray(self.anArray) {
                marshaller.pushString($0)
            }
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "anArray", type: "a<s>"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestFunctionObject : ValdiMarshallableObject, ValdiRegisterable {
    var aFunction: (Double) throws -> String = { _ in return "" }
    init() {
    }

    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aFunction = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aFunction") { self.create_aFunction_from_bridgeFunction(using: try marshaller.getFunction($0)) }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushFunction(self.create_aFunction_bridgeFunction(using: self.aFunction))
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aFunction", type: "f(d): s"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Class)
    }

    private func create_aFunction_from_bridgeFunction(using functionImpl: ValdiFunction) -> ((Double) throws -> String) {
        return { (arg0: Double) throws -> String in
            let marshaller = ValdiMarshaller()
            _ = marshaller.pushDouble(arg0)
            if functionImpl.perform(with: marshaller, sync: true) {
                return try marshaller.getString(-1)
            } else {
                try marshaller.checkError()
                throw ValdiError.functionCallFailed("aFunction")
            }
        }
    }

    private func create_aFunction_bridgeFunction(using functionImpl: @escaping ((Double) throws -> String)) -> ValdiFunction {
        return ValdiFunction(callable: { (marshaller: ValdiMarshaller) in
            do {
                let arg = try marshaller.getDouble(-1)
                let result = try functionImpl(arg)
                _ = marshaller.pushString(result)
                return true
            } catch let error {
                marshaller.setError(error.localizedDescription)
                return false
            }
        })
    }

}

class ValdiMarshallerTestParentObject : ValdiMarshallableObject, ValdiRegisterable {
    var aValdiObject: ValdiMarshallerTestDoubleObject = ValdiMarshallerTestDoubleObject()
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aValdiObject = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aValdiObject") {
            try ValdiMarshallerTestDoubleObject(from: marshaller, at: $0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = try self.aValdiObject.push(to: marshaller)
        }
        return objectIndex
    }

    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "aValdiObject", type: "r:'[0]'"),
        ]
        let identifiers: [ValdiMarshallableIdentifier] = [
          ValdiMarshallableIdentifier(name: "ValdiMarshallerTestDoubleObject", registerFunc: {registerClass(ValdiMarshallerTestDoubleObject.self)})
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: identifiers, type: ValdiMarshallableObjectType.Class)
    }
}

enum ValdiMarshallerTestIntEnum : Int32, ValdiMarshallableIntEnum {
    case EnumZero = 0
    case EnumOne = 1

    public init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        let value = try marshaller.getInt(objectIndex)
        guard let enumValue = ValdiMarshallerTestIntEnum(rawValue: value) else {
            throw ValdiError.invalidEnumValue("ValdiMarshallerTestIntEnum", String(value))
        }
        self = enumValue
    }
}

class ValdiMarshallerTestIntEnumObject : ValdiMarshallableObject, ValdiRegisterable {
    var anIntEnum: ValdiMarshallerTestIntEnum = .EnumZero
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anIntEnum = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anIntEnum") {
            try marshaller.getEnum($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushEnum(self.anIntEnum)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
          ValdiMarshallableObjectFieldDescriptor(name: "anIntEnum", type: "r<e>:'[0]'"),
        ]
        let identifiers: [ValdiMarshallableIdentifier] = [
          ValdiMarshallableIdentifier(name: "ValdiMarshallerTestIntEnum", registerFunc: {})
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: identifiers, type: ValdiMarshallableObjectType.Class)
    }
}

class ValdiMarshallerTestOptionalIntEnumObject : ValdiMarshallableObject, ValdiRegisterable {
    var anOptionalIntEnum: ValdiMarshallerTestIntEnum?
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.anOptionalIntEnum = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "anOptionalIntEnum") {
            try marshaller.getOptionalEnum($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushOptionalEnum(self.anOptionalIntEnum)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
          ValdiMarshallableObjectFieldDescriptor(name: "anOptionalIntEnum", type: "r@?<e>:'[0]'"),
        ]
        let identifiers: [ValdiMarshallableIdentifier] = [
          ValdiMarshallableIdentifier(name: "ValdiMarshallerTestIntEnum", registerFunc: {})
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: identifiers, type: ValdiMarshallableObjectType.Class)
    }
}

enum ValdiMarshallerTestStringEnum : String, ValdiMarshallableStringEnum {
    case EnumStringHello = "Hello"
    case EnumStringWorld = "World"

    public init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        let value = try marshaller.getString(objectIndex)
        guard let enumValue = ValdiMarshallerTestStringEnum(rawValue: value) else {
            throw ValdiError.invalidEnumValue("ValdiMarshallerTestStringEnum", String(value))
        }
        self = enumValue
    }
}

class ValdiMarshallerTestStringEnumObject : ValdiMarshallableObject, ValdiRegisterable {
    var aStringEnum: ValdiMarshallerTestStringEnum = .EnumStringHello
    init() {}
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        self.aStringEnum = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aStringEnum") {
            try marshaller.getEnum($0)
        }
    }
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        let objectIndex = try marshaller.pushObject(className: String(describing:type(of:self)))
        try marshaller.setTypedObjectProperty(objectIndex, 0) {
            _ = marshaller.pushEnum(self.aStringEnum)
        }
        return objectIndex
    }
    public class func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
          ValdiMarshallableObjectFieldDescriptor(name: "aStringEnum", type: "r<e>:'[0]'"),
        ]
        let identifiers: [ValdiMarshallableIdentifier] = [
          ValdiMarshallableIdentifier(name: "ValdiMarshallerTestStringEnum", registerFunc: {})
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: identifiers, type: ValdiMarshallableObjectType.Class)
    }
}

protocol ICalculator : ValdiMarshallableInterface {
    func add(lhs: Double) throws -> Double
    func multiply(lhs: Double) throws -> Double
}

extension ICalculator {
    private func call_add(_ marshaller: ValdiMarshaller) -> Bool {
        do {
            let lhs = try marshaller.getDouble(0)
            let result = try add(lhs: lhs)
            _ = marshaller.pushDouble(result)
            return true
        } catch let error {
            marshaller.setError(error.localizedDescription)
            return false
        }
    }

    private func call_multiply(_ marshaller: ValdiMarshaller) -> Bool {
        do {
            let lhs = try marshaller.getDouble(0)
            let result = try multiply(lhs: lhs)
            _ = marshaller.pushDouble(result)
            return true
        } catch let error {
            marshaller.setError(error.localizedDescription)
            return false
        }
    }

    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        try marshaller.pushProxy(object: self) {
            let objectIndex = try marshaller.pushObject(className: "ICalculator")
            try marshaller.setTypedObjectProperty(objectIndex, 0) {
                _ = marshaller.pushFunction(self.call_add)
            }
            try marshaller.setTypedObjectProperty(objectIndex, 1) {
                _ = marshaller.pushFunction(self.call_multiply)
            }
        }
    }

    public static func getDescriptor() -> ValdiMarshallableObjectDescriptor {
        let fields: [ValdiMarshallableObjectFieldDescriptor] =
        [
            ValdiMarshallableObjectFieldDescriptor(name: "add", type: "f(d): d"),
            ValdiMarshallableObjectFieldDescriptor(name: "multiply", type: "f(d): d"),
        ]
        return ValdiMarshallableObjectDescriptor(fields, identifiers: nil, type: ValdiMarshallableObjectType.Interface)
    }
}

class  ICalculator_Proxy : ICalculator, ValdiMarshallableObject, ValdiRegisterable {
    private var _add : (Double)  throws -> Double
    private var _multiply : (Double)  throws -> Double
    func add(lhs: Double) throws -> Double {
        return try _add(lhs)
    }
    func multiply(lhs: Double) throws -> Double {
        return try _multiply(lhs)
    }
    public required init(from marshaller: ValdiMarshaller, at proxyIndex: Int) throws {
        let objectIndex = try marshaller.unwrapProxy(proxyIndex)
        let add_internal = try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "add") {
            try marshaller.getFunction($0)
        }
        self._add = { (lhs: Double) throws -> Double in
            let marshaller = ValdiMarshaller()
            _ = marshaller.pushDouble(lhs)
            if add_internal.perform(with: marshaller, sync: true) {
                return try marshaller.getDouble(-1)
            } else {
                try marshaller.checkError()
                throw ValdiError.functionCallFailed("add")
            }
        }
        let multiply_internal = try marshaller.getTypedObjectOrMapProperty(objectIndex, 1, "multiply") {
            try marshaller.getFunction($0)
        }
        self._multiply = { (lhs: Double) throws -> Double in
            let marshaller = ValdiMarshaller()
            _ = marshaller.pushDouble(lhs)
            if multiply_internal.perform(with: marshaller, sync: true) {
                return try marshaller.getDouble(-1)
            } else {
                try marshaller.checkError()
                throw ValdiError.functionCallFailed("multiply")
            }
        }
        marshaller.pop()
    }
}


class ValdiMarshallerTestInterfaceObject : ICalculator, ValdiRegisterable {
    public var rhs : Double = 0
    func add(lhs: Double) throws -> Double {
        return lhs + rhs
    }
    func multiply(lhs: Double) throws -> Double {
        return lhs * rhs
    }
    // Used by the test case to verify that the object was deallocated
    public var onDeinit: (() -> Void)?
    deinit {
        onDeinit?()
    }
}

class SwiftValdiMarshallerTests : XCTestCase {
    override func setUpWithError() throws {
        registerClass(ValdiMarshallerTestDoubleObject.self)
        registerClass(ValdiMarshallerTestBoolObject.self)
        registerClass(ValdiMarshallerTestLongObject.self)
        registerClass(ValdiMarshallerTestIntObject.self)
        registerClass(ValdiMarshallerTestOptionalDoubleObject.self)
        registerClass(ValdiMarshallerTestOptionalBoolObject.self)
        registerClass(ValdiMarshallerTestOptionalLongObject.self)
        registerClass(ValdiMarshallerTestOptionalIntObject.self)
        registerClass(ValdiMarshallerTestStringObject.self)
        registerClass(ValdiMarshallerTestDataObject.self)
        registerClass(ValdiMarshallerTestMapObject.self)
        registerClass(ValdiMarshallerTestArrayObject.self)
        registerClass(ValdiMarshallerTestFunctionObject.self)
        registerClass(ValdiMarshallerTestParentObject.self)
        registerClass(ValdiMarshallerTestIntEnumObject.self)
        registerClass(ValdiMarshallerTestOptionalIntEnumObject.self)
        registerClass(ValdiMarshallerTestStringEnumObject.self)
        registerProxy("ICalculator", ICalculator_Proxy.self)
        registerClass(ValdiMarshallerTestInterfaceObject.self)
        super.setUp()
    }
    override func tearDownWithError() throws {
        super.tearDown()
    }

    func testCanMarshallDouble() throws {
        let testObject : ValdiMarshallerTestDoubleObject = ValdiMarshallerTestDoubleObject()
        testObject.aDouble = 42.069
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing: type(of: testObject))): { aDouble: 42.069000} >", result)
    }

    func testCanUnmarshallDouble() throws {
        let object : ValdiMarshallerTestDoubleObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aDouble", idx) {
                marshaller.pushDouble(42.0)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(42.0, object.aDouble)
    }

    func testCanMarshallBool() throws {
        let testObject : ValdiMarshallerTestBoolObject = ValdiMarshallerTestBoolObject()
        testObject.aBool = true
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing: type(of: testObject))): { aBool: true} >", result)
    }

    func testCanUnmarshallBool() throws {
        let object : ValdiMarshallerTestBoolObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aBool", idx) {
                marshaller.pushBool(true)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(true, object.aBool)
    }

    func testCanMarshallLong() throws {
        let testObject : ValdiMarshallerTestLongObject = ValdiMarshallerTestLongObject()
        testObject.aLong = 420690420690420690
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing: type(of: testObject))): { aLong: 420690420690420690} >", result)
    }

    func testCanUnmarshallLong() throws {
        let object : ValdiMarshallerTestLongObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aLong", idx) {
                marshaller.pushLong(420690420690420690)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(420690420690420690, object.aLong)
    }

    func testCanMarshallInt() throws {
        let testObject : ValdiMarshallerTestIntObject = ValdiMarshallerTestIntObject()
        testObject.aInt = 42069
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing: type(of: testObject))): { aInt: 42069} >", result)
    }

    func testCanUnmarshallInt() throws {
        let object : ValdiMarshallerTestIntObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aInt", idx) {
                marshaller.pushInt(42069)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(42069, object.aInt)
    }

    func testCanMarshallOptionalDouble() throws {
        let testObject : ValdiMarshallerTestOptionalDoubleObject = ValdiMarshallerTestOptionalDoubleObject()
        testObject.anOptionalDouble = 42.069
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalDouble: 42.069000} >", result)
        testObject.anOptionalDouble = nil
        let nilResult = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalDouble: <null>} >", nilResult)
    }

    func testCanUnmarshallOptionalDouble() throws {
        let object : ValdiMarshallerTestOptionalDoubleObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalDouble", idx) {
                marshaller.pushDouble(42.069)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(42.069, object.anOptionalDouble)
        let nilObject : ValdiMarshallerTestOptionalDoubleObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalDouble", idx) {
                marshaller.pushUndefined()
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertNil(nilObject.anOptionalDouble)
    }

    func testCanMarshallOptionalBool() throws {
        let testObject : ValdiMarshallerTestOptionalBoolObject = ValdiMarshallerTestOptionalBoolObject()
        testObject.anOptionalBool = true
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalBool: true} >", result)
        testObject.anOptionalBool = nil
        let nilResult = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalBool: <null>} >", nilResult)
    }

    func testCanUnmarshallOptionalBool() throws {
        let object : ValdiMarshallerTestOptionalBoolObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalBool", idx) {
                marshaller.pushBool(true)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(true, object.anOptionalBool)
        let nilObject : ValdiMarshallerTestOptionalBoolObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalBool", idx) {
                marshaller.pushUndefined()
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertNil(nilObject.anOptionalBool)
    }

    func testCanMarshallOptionalLong() throws {
        let testObject : ValdiMarshallerTestOptionalLongObject = ValdiMarshallerTestOptionalLongObject()
        testObject.anOptionalLong = 420690420690420690
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalLong: 420690420690420690} >", result)
        testObject.anOptionalLong = nil
        let nilResult = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalLong: <null>} >", nilResult)
    }

    func testCanUnmarshallOptionalLong() throws {
        let object : ValdiMarshallerTestOptionalLongObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalLong", idx) {
                marshaller.pushLong(420690420690420690)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(420690420690420690, object.anOptionalLong)
        let nilObject : ValdiMarshallerTestOptionalLongObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalLong", idx) {
                marshaller.pushUndefined()
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertNil(nilObject.anOptionalLong)
    }

    func testCanMarshallOptionalInt() throws {
        let testObject : ValdiMarshallerTestOptionalIntObject = ValdiMarshallerTestOptionalIntObject()
        testObject.anOptionalInt = 42069
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalInt: 42069} >", result)
        testObject.anOptionalInt = nil
        let nilResult = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalInt: <null>} >", nilResult)
    }

    func testCanUnmarshallOptionalInt() throws {
        let object : ValdiMarshallerTestOptionalIntObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalInt", idx) {
                marshaller.pushInt(42069)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(42069, object.anOptionalInt)
        let nilObject : ValdiMarshallerTestOptionalIntObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalInt", idx) {
                marshaller.pushUndefined()
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertNil(nilObject.anOptionalInt)
    }

    func testCanMarshallString() throws {
        let testObject : ValdiMarshallerTestStringObject = ValdiMarshallerTestStringObject()
        testObject.aString = "Hello, World!"
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { aString: Hello, World!} >", result)
    }

    func testCanUnmarshallString() throws {
        let object : ValdiMarshallerTestStringObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aString", idx) {
                marshaller.pushString("Hello, World!")
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual("Hello, World!", object.aString)
   }

    func testCanMarshallData() throws {
        let testObject : ValdiMarshallerTestDataObject = ValdiMarshallerTestDataObject()
        testObject.bytes = Data("hello".utf8)
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { bytes: <TypedArray: type=Uint8Array size=5>} >", result)
    }

    func testCanUnmarshallBytes() throws {
        let object : ValdiMarshallerTestDataObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("bytes", idx) {
                marshaller.pushData(Data("hello".utf8))
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual("hello", String(data: object.bytes as Data, encoding: .utf8))
    }

    func testCanMarshallMap() throws {
        let testObject : ValdiMarshallerTestMapObject = ValdiMarshallerTestMapObject()
        testObject.aMap["hello"] =  "world"
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            XCTAssertEqual(marshaller.size(), 1)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { aMap: { hello: world} } >", result)
    }

    func testCanUnmarshallMap() throws {
        let object : ValdiMarshallerTestMapObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aMap", idx) {
                try marshaller.pushUntyped(["hello": "world"])
            }
            XCTAssertEqual(marshaller.size(), 1)
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual("world", object.aMap["hello"] as! String)
    }

    func testCanMarshallArray() throws {
        let testObject : ValdiMarshallerTestArrayObject = ValdiMarshallerTestArrayObject()
        testObject.anArray = ["hello", "world"]
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            XCTAssertEqual(marshaller.size(), 1)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anArray: [ hello, world ]} >", result)
    }

    func testCanUnmarshallArray() throws {
        let object : ValdiMarshallerTestArrayObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anArray", idx) {
                let arrayIndex = marshaller.pushArray(capacity: 2)
                _ = marshaller.pushString("hello")
                try marshaller.setArrayItem(arrayIndex, 0)
                _ = marshaller.pushString("world")
                try marshaller.setArrayItem(arrayIndex, 1)
                return arrayIndex
            }
            XCTAssertEqual(marshaller.size(), 1)
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(["hello", "world"], object.anArray)
    }

    func testCanMarshallFunction() throws {
        var funcArg : Double = 0
        let testObject : ValdiMarshallerTestFunctionObject = ValdiMarshallerTestFunctionObject()
        testObject.aFunction = { arg in
            funcArg = arg
            return String(arg)
        }
        let strResult = try testObject.aFunction(42.069)
        XCTAssertEqual(42.069, funcArg)
        XCTAssertEqual("42.069", strResult)

        let objectFunction = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            let marshalledResult = try marshaller.toString(index: objectIndex, indent: false)
            XCTAssertTrue(marshalledResult.contains("<typedObject \(String(describing:type(of: testObject))): { aFunction: <function SwiftFunction"))
            return try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aFunction") {
                try marshaller.getFunction($0)
            }
        }
        funcArg = 0
        let callResult = try withMarshaller { marshaller in
            _ = marshaller.pushDouble(42.069)
            let success = objectFunction.perform(with: marshaller, sync: true)
            XCTAssertTrue(success)
            return try marshaller.getString(-1)
        }
        XCTAssertEqual(42.069, funcArg)
        XCTAssertEqual("42.069", callResult)
    }

    func testCanUnmarshallFunction() throws {
        let object : ValdiMarshallerTestFunctionObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aFunction", idx) {
                return marshaller.pushFunction { marshaller in
                    do {
                        let arg = try marshaller.getDouble(-1)
                        _ = marshaller.pushString(String(arg))
                        return true
                    } catch let error {
                        marshaller.setError(error.localizedDescription)
                        return false
                    }
                }
            }
            return try marshaller.getObject(idx)
        }
        let result = try object.aFunction(42.069)
        XCTAssertEqual("42.069", result)
    }

    func testCanMarshallFunctionThrowErrors() throws {
        let testObject : ValdiMarshallerTestFunctionObject = ValdiMarshallerTestFunctionObject()
        testObject.aFunction = { (arg: Double) throws -> String in
            throw ValdiError.runtimeError("Test error")
        }
        do {
            let result = try testObject.aFunction(42.069)
            XCTAssertTrue(false, "Expected error. Got \(result)")
        } catch let error {
            XCTAssertEqual("Test error", error.localizedDescription)
        }

        let objectFunction = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            let marshalledResult = try marshaller.toString(index: objectIndex, indent: false)
            XCTAssertTrue(marshalledResult.contains("<typedObject \(String(describing:type(of: testObject))): { aFunction: <function SwiftFunction"))
            return try marshaller.getTypedObjectOrMapProperty(objectIndex, 0, "aFunction") {
                try marshaller.getFunction($0)
            }
        }
        withMarshaller { marshaller in
            _ = marshaller.pushDouble(42.069)
            let success = objectFunction.perform(with: marshaller, sync: true)
            XCTAssertTrue(!success)
            do {
                try marshaller.checkError()
                XCTAssertTrue(false, "Expected error")
            } catch let error {
                XCTAssertEqual("Test error", error.localizedDescription)
            }
        }
    }

    func testCanUnmarshallFunctionThrowErrors() throws {
        let object : ValdiMarshallerTestFunctionObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aFunction", idx) {
                return marshaller.pushFunction { marshaller in
                    do {
                        throw ValdiError.runtimeError("Test error")
                    } catch let error {
                        marshaller.setError(error.localizedDescription)
                        return false
                    }
                }
            }
            return try marshaller.getObject(idx)
        }
        do {
            let result = try object.aFunction(42.069)
            XCTAssertTrue(false, "Expected error. Got \(result)")
        } catch let error {
            XCTAssertEqual("Test error", error.localizedDescription)
        }
    }

    func testCanMarshallParentObject() throws {
        let testObject : ValdiMarshallerTestParentObject = ValdiMarshallerTestParentObject()
        testObject.aValdiObject.aDouble = 42.069
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { aValdiObject: <typedObject \(String(describing:type(of: testObject.aValdiObject))): { aDouble: 42.069000} >} >", result)
    }

    func testCanUnmarshallParentObject() throws {
        let object : ValdiMarshallerTestParentObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aValdiObject", idx) {
                let childIdx = marshaller.pushMap(initialCapacity: 1)
                try marshaller.putMapProperty("aDouble", childIdx) {
                    let ret = marshaller.pushDouble(42.0)
                    return ret
                }
                return childIdx
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(42.0, object.aValdiObject.aDouble)
    }

    func testCanMarshallIntEnum() throws {
        let testObject : ValdiMarshallerTestIntEnumObject = ValdiMarshallerTestIntEnumObject()
        testObject.anIntEnum = .EnumOne
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anIntEnum: 1} >", result)
    }

    func testCanUnmarshallIntEnum() throws {
        let object : ValdiMarshallerTestIntEnumObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anIntEnum", idx) {
                marshaller.pushInt(1)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(.EnumOne, object.anIntEnum)
    }

    func testCanMarshallOptionalIntEnum() throws {
        let testObject : ValdiMarshallerTestOptionalIntEnumObject = ValdiMarshallerTestOptionalIntEnumObject()
        testObject.anOptionalIntEnum = .EnumOne
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalIntEnum: 1} >", result)
        testObject.anOptionalIntEnum = nil
        let nilResult = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { anOptionalIntEnum: <null>} >", nilResult)
    }

    func testCanUnmarshallOptionalIntEnum() throws {
        let object : ValdiMarshallerTestOptionalIntEnumObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalIntEnum", idx) {
                marshaller.pushInt(1)
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(.EnumOne, object.anOptionalIntEnum)
        let nilObject : ValdiMarshallerTestOptionalIntEnumObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("anOptionalIntEnum", idx) {
                marshaller.pushUndefined()
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertNil(nilObject.anOptionalIntEnum)
    }

    func testUnmarshallInvalidIntEnumThrows() throws {
        do {
            let _ : ValdiMarshallerTestIntEnumObject = try withMarshaller { marshaller in
                let idx = marshaller.pushMap(initialCapacity: 1)
                try marshaller.putMapProperty("anIntEnum", idx) {
                    marshaller.pushInt(42)
                }
                return try marshaller.getObject(idx)
            }
            XCTAssertTrue(false, "Expected error")
        } catch let error {
            XCTAssertEqual("Invalid enum type: ValdiMarshallerTestIntEnum value: 42", error.localizedDescription)
        }
    }

    func testCanMarshallStringEnum() throws {
        let testObject : ValdiMarshallerTestStringEnumObject = ValdiMarshallerTestStringEnumObject()
        testObject.aStringEnum = .EnumStringWorld
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testObject)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertEqual("<typedObject \(String(describing:type(of: testObject))): { aStringEnum: World} >", result)
    }

    func testCanUnmarshallStringEnum() throws {
        let object : ValdiMarshallerTestStringEnumObject = try withMarshaller { marshaller in
            let idx = marshaller.pushMap(initialCapacity: 1)
            try marshaller.putMapProperty("aStringEnum", idx) {
                marshaller.pushString("World")
            }
            return try marshaller.getObject(idx)
        }
        XCTAssertEqual(.EnumStringWorld, object.aStringEnum)
    }

    func testUnmarshallInvalidStringEnumThrows() throws {
        do {
            let _ : ValdiMarshallerTestStringEnumObject = try withMarshaller { marshaller in
                let idx = marshaller.pushMap(initialCapacity: 1)
                try marshaller.putMapProperty("aStringEnum", idx) {
                    marshaller.pushString("INVALID")
                }
                return try marshaller.getObject(idx)
            }
            XCTAssertTrue(false, "Expected error")
        } catch let error {
            XCTAssertEqual("Invalid enum type: ValdiMarshallerTestStringEnum value: INVALID", error.localizedDescription)
        }
    }

    func testCanMarshallInterface() throws {
        let expectation = self.expectation(description: "Object should be deallocated")
        do {
            let testObject = ValdiMarshallerTestInterfaceObject()
            testObject.rhs = 2.0
            testObject.onDeinit = {
                expectation.fulfill()
            }
            XCTAssertEqual(42.0, try testObject.add(lhs: 40.0))
            XCTAssertEqual(42.0, try testObject.multiply(lhs: 21.0))
            let result = try withMarshaller { marshaller in
                let objectIndex = try marshaller.push(testObject)
                let instance : ICalculator = try marshaller.getObject(objectIndex)
                XCTAssertTrue(instance === testObject)
                testObject.rhs = 3.0
                XCTAssertEqual(5.0, try instance.add(lhs: 2.0))
                XCTAssertEqual(6.0, try instance.multiply(lhs: 2.0))
                return try marshaller.toString(index: objectIndex, indent: false)
            }
            XCTAssertTrue(result.contains("<proxyObject Swift Proxy on ICalculator"))
            XCTAssertTrue(result.contains("add: <function SwiftFunction"))
            XCTAssertTrue(result.contains("multiply: <function SwiftFunction"))
        }
        waitForExpectations(timeout: 1.0, handler: nil)
    }

    // Simulate a remote proxy object by marshalling the interface implementation and
    // unmarshalling it as a <interface>_Proxy object
    func testCanUnmarshallInterface() throws {
        let expectation = self.expectation(description: "Object should be deallocated")
        do {
            let testObject = ValdiMarshallerTestInterfaceObject()
            testObject.onDeinit = {
                expectation.fulfill()
            }
            testObject.rhs = 2.0
            let unmarshalledTestObject : ICalculator_Proxy = try withMarshaller { marshaller in
                let idx = try marshaller.push(testObject)
                return try marshaller.getObject(idx)
            }
            XCTAssertFalse(unmarshalledTestObject === testObject)
            XCTAssertEqual(42.0, try unmarshalledTestObject.add(lhs: 40.0))
            XCTAssertEqual(42.0, try unmarshalledTestObject.multiply(lhs: 21.0))
        }
        waitForExpectations(timeout: 1.0, handler: nil)
    }

    func testCanMarshallPromise() throws {
        let testPromise = ValdiResolvablePromise<String>()
        let result = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testPromise)
            return try marshaller.toString(index: objectIndex, indent: false)
        }
        XCTAssertTrue(result.contains("<Promise@"))
    }
    func testCanUnmarshallPromise() throws {
        let expectation = self.expectation(description: "Promise fulfilled")
        let testPromise = ValdiResolvablePromise<String>()
        let resultPromise = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(testPromise)
            let result: ValdiPromise<String> = try marshaller.getPromise(objectIndex)
            return result
        }
        Task {
            try testPromise.resolve(result:"abc")
        }
        Task {
            let result = try await resultPromise.value
            XCTAssertEqual("abc", result)
            expectation.fulfill()
        }
        waitForExpectations(timeout: 1.0, handler: nil)
    }

    // A JS call skipped during runtime teardown yields 'undefined' with a clean exception
    // tracker; unmarshalling it as a Promise must throw a catchable ValdiError, not trap
    // (COMPOSER-6242).
    func testUnmarshallingNonPromiseValueThrowsInsteadOfTrapping() throws {
        try withMarshaller { marshaller in
            let undefinedIndex = marshaller.pushUndefined()
            XCTAssertThrowsError(try marshaller.getPromise(undefinedIndex) as ValdiPromise<String>) { error in
                XCTAssertTrue(error is ValdiError)
            }
            let intIndex = marshaller.pushInt(42)
            XCTAssertThrowsError(try marshaller.getPromise(intIndex) as ValdiPromise<String>) { error in
                XCTAssertTrue(error is ValdiError)
            }
        }
    }

    func testStringMarshallingPerformance() throws {
        self.measure {
            do {
                let repeatedString = String(repeating: "Hello World", count: 10000)
                try withMarshaller { marshaller in
                    for _ in 1...1000 {
                        let j = marshaller.pushString(repeatedString)
                        let _ = try marshaller.getString(j)
                    }
                }
            }
            catch {
                XCTFail("Unexpected error: \(error)")
            }
        }
    }
}
