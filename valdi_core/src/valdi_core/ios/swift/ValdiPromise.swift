import Foundation
import ValdiCoreCPP

// The Swift ValdiPromise class is a wrapper of a C++ Valdi::Promise pointer
public class ValdiPromise<T> : ValdiMarshallableObject {

    // This is a retained pointer that needs to be released
    fileprivate let valuePromise: SwiftValuePromisePtr;

    public init(_ valuePromise: SwiftValuePromisePtr) {
        self.valuePromise = valuePromise
    }

    // Release the retained pointer
    deinit {
        SwiftValdiMarshaller_ReleasePromise(valuePromise)
    }

    // The awaitable `value` property asynchronously returns the promise result
    public var value: T {
        get async throws {
            return try await withCheckedThrowingContinuation { continuation in
                withMarshaller { marshaller in
                    // Push a completion handler to the marshaller
                    let iCompletionHandler = marshaller.pushFunction { marshaller in
                        do {
                            let result: T = try marshaller.getGenericTypeParameter(0)
                            continuation.resume(returning: result)
                        } catch {
                            continuation.resume(throwing: error)
                        }
                        return true
                    }
                    // Attach the completion handler to the promise
                    SwiftValdiMarshaller_OnPromiseComplete(marshaller.marshallerCpp, valuePromise, iCompletionHandler)
                }
            }
        }
    }

    // Forward cancel request to the C++ Promise
    public func cancel() -> Void {
        SwiftValdiMarshaller_CancelPromise(valuePromise)
    }
    
    // ValdiConstructable
    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        guard let valuePromise = SwiftValdiMarshaller_GetPromise(marshaller.marshallerCpp, objectIndex) else {
            // A JS call skipped during runtime teardown yields 'undefined' instead of a Promise; throw rather than trap on the nil pointer.
            try marshaller.checkError()
            throw ValdiError.runtimeError(
                "Value at index \(objectIndex) is not a Promise; the JS call was likely skipped because the runtime is tearing down")
        }
        self.valuePromise = valuePromise
    }

    // ValdiMarshallable
    public func push(to marshaller: ValdiMarshaller) throws -> Int {
        return SwiftValdiMarshaller_PushPromise(marshaller.marshallerCpp, valuePromise)
    }
}

    // The Swift ValdiResolvablePromise class is backed by a C++ ResolvablePromise instance
public class ValdiResolvablePromise<T> : ValdiPromise<T> {

    public required init(from marshaller: ValdiMarshaller, at objectIndex: Int) throws {
        fatalError("This should never happen. TS Promise is always marshalled to ValdiPromise")
    }

    // Use a marhsaller to convert an empty ResolvablePromise into a retained pointer
    public init() {
        super.init(withMarshaller { marshaller in
                       let iPromise = SwiftValdiMarshaller_PushNewPromise(marshaller.marshallerCpp)
                       guard let valuePromise = SwiftValdiMarshaller_GetPromise(marshaller.marshallerCpp, iPromise) else {
                           fatalError("Failed to create ValdiResolvablePromise")
                       }
                       return valuePromise
                   })
    }

    // Push the result to the marshaller (turn it into a Valdi::Value)
    // Then forward the resolve request to the C++ ResolvablePromise (which can be resolved with a Value)
    public func resolve(result: T) throws {
        try withMarshaller { marshaller in
            let iResult = try marshaller.pushGenericTypeParameter(result)
            SwiftValdiMarshaller_ResolvePromise(marshaller.marshallerCpp, valuePromise, iResult)
        }
    }
    
    // Resolve the promise with an error
    // (Will throw in awaiting caller)
    public func resolve(errorMessage: String) throws {
        errorMessage.withCString { errorC in
            SwiftValdiMarshaller_RejectPromise(valuePromise, errorC)
        }
    }

    public func setCancelCallback(_ cb: @escaping ()->Void) {
        withMarshaller { marshaller in
            let iCancelCb = marshaller.pushFunction { _ in
                cb()
                return true
            }
            SwiftValdiMarshaller_SetCancelCallback(marshaller.marshallerCpp, valuePromise, iCancelCb)
        }
    }
}
