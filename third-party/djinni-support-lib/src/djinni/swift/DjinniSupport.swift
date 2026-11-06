import DjinniSupportCxx
import Foundation

// Define a C++ vtable like data structure for dispatching proxy methods to
// protocol methods.  Each entry in the table is function pointer that takes 3
// parameters
// - The object instance (that conforms to the protocol)
// - The parameter list
// - The return value (output parameter)
// The return value has to be an output parameter instead of the function's
// return type, otherwise the Swift compiler will crash (if this crash is fixed
// in a later version of the compiler, we may change this).
public typealias Vtbl<T> = [(T, UnsafePointer<djinni.swift.ParameterList>?, UnsafeMutablePointer<djinni.swift.AnyValue>?) throws -> Void]

// Type erased interface for ProtocolWrapperContext. We don't have the type
// parameter T for ProtocolWrapperContext inside dispatcherProtocalCall (called
// by C++ code)
protocol GenericProtocolWrapperContext: AnyObject {
    func dispatch(idx: Int32, params: UnsafePointer<djinni.swift.ParameterList>?, ret: UnsafeMutablePointer<djinni.swift.AnyValue>?) -> Void
    func getInst() -> AnyObject
    /// The SwiftProxyCache entry of this context's proxy.
    var proxyKey: SwiftProxyKey { get }
}

// A Swift object's proxy for one djinni interface: an object implementing several interfaces has a
// proxy (a C++ object of a different class) for each.
struct SwiftProxyKey: Hashable {
    let object: UnsafeMutableRawPointer
    let interface: ObjectIdentifier

    init<I>(_ object: I) {
        self.object = Unmanaged.passUnretained(object as AnyObject).toOpaque()
        self.interface = ObjectIdentifier(I.self)
    }
}

// The bridge between C++ caller and Swift protocol. We store
// - The object instance (that conforms to our protocol)
// - The dispatch table for each callable method in the protocol
final class ProtocolWrapperContext<T>: GenericProtocolWrapperContext {
    var inst: T
    var vtbl: Vtbl<T>
    init(inst: T, vtbl: Vtbl<T>) {
        self.inst = inst
        self.vtbl = vtbl
    }
    func dispatch(idx: Int32, params: UnsafePointer<djinni.swift.ParameterList>?, ret: UnsafeMutablePointer<djinni.swift.AnyValue>?) -> Void {
        // No Swift error will cross the language boundary. They are converted
        // to `ErrorValue`s which will be translated into C++ exceptions on the
        // other side.
        do {
            try vtbl[Int(idx)](inst, params, ret)
        } catch let error as DjinniError {
            djinni.swift.setErrorValue(ret, error.wrapped)
        } catch {
            djinni.swift.setErrorMessage(ret, std.string(String(describing: error)))
        }
    }
    func getInst() -> AnyObject {
        return inst as AnyObject
    }
    var proxyKey: SwiftProxyKey {
        return SwiftProxyKey(inst)
    }
}

// This function has a C++ friendly signature. It is passed to C++ and will be
// used by proxy objects to dispatch protocol calls.
public func dispatcherProtocalCall(
  ptr: UnsafeMutableRawPointer?, // The Swift protocol wrapper object as an opaque pointer
  idx: Int32,                    // The method index (starting from 0)
  params: UnsafePointer<djinni.swift.ParameterList>?, // Input parameters from C++ caller
  ret: UnsafeMutablePointer<djinni.swift.AnyValue>?)  // Return value that will be passed back to C++
  -> Void {
    guard let pctx = ptr else { return }
    let ctx = Unmanaged<AnyObject>.fromOpaque(pctx).takeUnretainedValue() as! GenericProtocolWrapperContext
    if (idx >= 0) {
        // Dynamic dispatch on the vtbl.  We use `takeUnretainedValue` here
        // because we want to keep the wrapper object alive inside the C++ side
        // proxy (inherits ProtocolWrapper).
        ctx.dispatch(idx: idx, params: params, ret: ret)
    } else {
        // If the index is negative, release and destroy the context.  We do
        // this when the C++ side proxy (ProtocolWrapper) is destroyed.
        SwiftProxyCache.shared.mapPtrToProxy.removeValue(forKey: ctx.proxyKey)
    }
}

// Base class for a Swift callable proxy of a C++ object
open class CppProxy {
    // Stores a C++ interface. A C++ interface value is a double pointer:
    // 1. A shared_ptr<> that keeps the C++ implementation object alive
    // 2. A shared_ptr<> to a ProtocolWrapper that facilitates dispatching
    public var inst: djinni.swift.AnyValue
    public init(inst: djinni.swift.AnyValue) {
        self.inst = inst
    }
    deinit {
        // Remove the C++ proxy pointer from the proxy cache
        withUnsafePointer(to: inst) { p in
            let info = djinni.swift.getInterfaceInfo(p)
            CppProxyCache.shared.mapPtrToProxy.removeValue(forKey: info.cppPointer)
        }
    }
}

class CppProxyCache {
    static let shared = CppProxyCache()
    // C++ objects in swift.
    // Key:   raw C++ implementation object pointer
    // Value: Swift callable proxy converted to an *Unretained* (weak) opaque pointer
    var mapPtrToProxy: [UnsafeRawPointer: UnsafeMutableRawPointer] = [:]
}

class SwiftProxyCache {
    static let shared = SwiftProxyCache()
    // Swift objects in c++
    // Key:    Swift implementation object (as an *Unretained* opaque pointer) and the interface
    //         the proxy implements
    // Value : Weak proxy, no ownership, but can be converted to a C++ callable strong proxy
    var mapPtrToProxy: [SwiftProxyKey: djinni.swift.WeakSwiftProxy] = [:]
}

// 1. Object is a an existing cpp proxy  : return cpp proxy
// 2. Object is a an existing swift proxy: unwrap the original swift object
// 3. Need to create a new proxy         : call newProxyFunc
public func cppInterfaceToSwift<I>(_ c: djinni.swift.AnyValue,
                                   _ newProxyFunc: ()->I) -> I {
    return withUnsafePointer(to: c) { p in
        let info = djinni.swift.getInterfaceInfo(p)
        // 1. Check the CppProxyCache
        if let s = CppProxyCache.shared.mapPtrToProxy[info.cppPointer] {
            return Unmanaged<AnyObject>.fromOpaque(s).takeUnretainedValue() as! I
        }
        // 2. Check if c++ ptr exists in SwiftProxyCache
        if let pctx = info.ctxPointer {
            let ctx = Unmanaged<AnyObject>.fromOpaque(pctx).takeUnretainedValue() as! GenericProtocolWrapperContext
            return ctx.getInst() as! I
        }
        // 3. Create new proxy and store unretained (weak) pointer in CppProxyCache
        let newProxy = newProxyFunc()
        CppProxyCache.shared.mapPtrToProxy[info.cppPointer] = Unmanaged.passUnretained(newProxy as AnyObject).toOpaque()
        return newProxy
    }
}

// 1. object is a an existing cpp proxy  : unwrap swift proxy
// 2. object is a an existing swift proxy: return existing
// 3. need to create a new proxy         : call newProxyFunc
public func swiftInterfaceToCpp<I>(_ s: I,
                                   _ newProxyFunc: ()->djinni.swift.AnyValue) -> djinni.swift.AnyValue {
    // 1. Try cast to CppProxy and unwrap
    if let cppproxy = s as? CppProxy {
        return cppproxy.inst
    }
    // 2. Check swift proxy cache
    let key = SwiftProxyKey(s)
    if let weakProxy = SwiftProxyCache.shared.mapPtrToProxy[key] {
        return djinni.swift.strongify(weakProxy)
    }
    // 3. Create new proxy and store weak reference in SwiftProxyCache
    let newProxy = newProxyFunc()
    SwiftProxyCache.shared.mapPtrToProxy[key] = djinni.swift.weakify(newProxy)
    return newProxy
}

// Shortcut function to create a Swift protocol wrapper context and return its
// *Retained* pointer. The returned pointer is owned by the C++ side
// ProtocolWrapper, and released in ProtocolWrapper::~ProtocolWrapper() by
// sending a dispatch request with index -1.
public func ctxPtr<I> (_ s: I, _ vtbl: Vtbl<I>) -> UnsafeMutableRawPointer {
    let ctx = ProtocolWrapperContext(inst: s, vtbl: vtbl)
    return Unmanaged.passRetained(ctx).toOpaque()
}

// Wraps C++ ErrorValue as a Swift throwable Error
public class DjinniError: Error {
    var wrapped: djinni.swift.ErrorValue
    init(_ wrapped: djinni.swift.ErrorValue) {
        self.wrapped = wrapped
    }
    public init(_ msg: String) {
        self.wrapped = djinni.swift.ErrorValue(std.string(msg))
    }
    public var errorMessage: String { return String(wrapped.msg) }
}

// Called by stubs to convert C++ exceptions stored as ErrorValue to Swift errors
public func handleCppErrors(_ ret: UnsafePointer<djinni.swift.AnyValue>) throws {
    if (djinni.swift.isError(ret)) {
        throw DjinniError(djinni.swift.getError(ret))
    }
}
