import XCTest
import ValdiCoreSwift

/**
 The Swift↔Valdi half of the cancellation matrix from PR #48811, as tests rather than a manual pass.

 `ValdiPromise.value` suspends on a `withCheckedThrowingContinuation`, so the failure these guard
 against is an awaiter that never resumes: under drop-on-cancel (#117404) the continuation was
 abandoned and the caller hung forever. Each test fails by timing out, which is precisely that
 symptom — so "resumed at all" is the load-bearing assertion, not the shape of what came back.

 They deliberately do not assert that a failed promise *throws*. `ValdiPromise.value` reads the
 completion's parameter with `getGenericTypeParameter`, which for a string-coercible `T` stringifies
 an error `Value` into a successful result, so a rejected promise resumes with the error's message as
 its value rather than throwing. That is a pre-existing defect in the Swift promise layer, unrelated
 to cancellation, and asserting the correct behaviour here would just leave a red test behind.
 `settledDescription` therefore accepts either shape.

 The Obj-C legs of the same matrix live in `SCValdiPromiseBridgeTests`, including a bridged
 Obj-C→C++ chain, and those do assert the error and its code. The legs that additionally round-trip
 through the JS runtime are not covered: this target has only `MockValdiRuntime`.
 */
class ValdiPromiseCancelTests: XCTestCase {

    /// Awaits `promise`, returning a description of however it settled — value or thrown error.
    private func settledDescription<T>(of promise: ValdiPromise<T>) async -> String {
        do {
            return "\(try await promise.value)"
        } catch {
            return "\(error)"
        }
    }

    /// Swift promise, awaited in Swift, canceled in Swift.
    func testCancelUnblocksSwiftAwaiter() throws {
        let expectation = self.expectation(description: "awaiter resumed")
        let promise = ValdiResolvablePromise<String>()

        Task {
            _ = await self.settledDescription(of: promise)
            expectation.fulfill()
        }

        // Let the await register its completion handler first, so this exercises the
        // pending-callback path rather than the already-canceled one covered below.
        Task {
            try await Task.sleep(nanoseconds: 50_000_000)
            promise.cancel()
        }

        waitForExpectations(timeout: 2.0, handler: nil)
    }

    /// Swift promise marshalled out to Valdi and back, then canceled in Swift. The awaiter holds the
    /// marshalled promise, so this covers cancel propagating across the marshalling boundary.
    func testCancelUnblocksAwaiterOnAMarshalledPromise() throws {
        let expectation = self.expectation(description: "awaiter resumed")
        let promise = ValdiResolvablePromise<String>()
        let marshalled: ValdiPromise<String> = try withMarshaller { marshaller in
            let objectIndex = try marshaller.push(promise)
            return try marshaller.getPromise(objectIndex)
        }

        Task {
            _ = await self.settledDescription(of: marshalled)
            expectation.fulfill()
        }

        Task {
            try await Task.sleep(nanoseconds: 50_000_000)
            promise.cancel()
        }

        waitForExpectations(timeout: 2.0, handler: nil)
    }

    /// When the producer answers the cancel by settling, the awaiter must receive that real result
    /// rather than the synthetic canceled error. This is the #48811 behaviour that #117404 removed.
    func testCancelDeliversTheProducerResultWhenItSettles() throws {
        let expectation = self.expectation(description: "awaiter received the producer's result")
        let promise = ValdiResolvablePromise<String>()
        promise.setCancelCallback {
            try? promise.resolve(errorMessage: "upstream failure")
        }

        Task {
            let settled = await self.settledDescription(of: promise)
            XCTAssertTrue(settled.contains("upstream failure"),
                          "cancel must not mask the producer's real result, got: \(settled)")
            XCTAssertFalse(settled.contains("Promise canceled"),
                           "the synthetic canceled error should have been superseded, got: \(settled)")
            expectation.fulfill()
        }

        Task {
            try await Task.sleep(nanoseconds: 50_000_000)
            promise.cancel()
        }

        waitForExpectations(timeout: 2.0, handler: nil)
    }

    /// A producer that settles asynchronously after cancel must not reach the continuation a second
    /// time. Swift traps on a double `resume`, so a regression here aborts the process rather than
    /// failing an assertion.
    func testResolveAfterCancelDoesNotResumeTheContinuationTwice() throws {
        let expectation = self.expectation(description: "awaiter resumed once")
        let promise = ValdiResolvablePromise<String>()

        Task {
            _ = await self.settledDescription(of: promise)
            expectation.fulfill()
        }

        Task {
            try await Task.sleep(nanoseconds: 50_000_000)
            promise.cancel()
            // Dropped by the promise; were it forwarded, the continuation would trap.
            try? promise.resolve(result: "late")
        }

        waitForExpectations(timeout: 2.0, handler: nil)
    }

    /// Awaiting a promise that was already canceled must resolve promptly from the recorded result
    /// instead of hanging — the late-registrant case.
    func testAwaitAfterCancelDoesNotHang() throws {
        let expectation = self.expectation(description: "late awaiter resumed")
        let promise = ValdiResolvablePromise<String>()
        promise.cancel()

        Task {
            _ = await self.settledDescription(of: promise)
            expectation.fulfill()
        }

        waitForExpectations(timeout: 2.0, handler: nil)
    }
}
