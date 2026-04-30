/// Forked from Saber's `MainActorLazy`. Uses `@ValdiActor` for isolation instead of `@MainActor`.
///
/// Used to defer the initialization of a type until the wrapped value is accessed.
/// Access is constrained to `@ValdiActor`, so the state is not protected with a lock.
public final class ValdiLazy<Value>: Sendable where Value: Sendable {

    @ValdiActor
    private var initializer: (@Sendable () -> Value)?

    @ValdiActor
    private var initialized: Value?

    // MARK: Initialization

    /// Initializes a new ``ValdiLazy`` instance.
    ///
    /// - Parameters:
    ///   - initializer: A closure that returns the value to be lazily instantiated.
    public nonisolated init(initializer: @escaping @Sendable () -> Value) {
        self.initializer = initializer
    }

    // MARK: Interface

    /// The lazily-instantiated value.
    ///
    /// The first access triggers the execution of the initialization closure.
    /// Subsequent accesses return the cached value.
    @ValdiActor
    public var value: Value {
        if let value = self.initialized {
            return value
        }

        guard let initializer = self.initializer else {
            preconditionFailure("ValdiLazy accessed before initialization and initializer is nil")
        }

        let value = initializer()
        self.initialized = value
        self.initializer = nil
        return value
    }
}
