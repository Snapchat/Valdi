import Foundation

/// A global actor for isolating Valdi runtime operations.
@globalActor
public enum ValdiActor {
    public static let shared = ActorType()

    public actor ActorType {
        private static let _executor = _ValdiQueueExecutor(label: "com.snap.valdi.actor")

        nonisolated public var unownedExecutor: UnownedSerialExecutor {
            Self._executor.asUnownedSerialExecutor()
        }
    }
}

private final class _ValdiQueueExecutor: SerialExecutor {
    private let queue: DispatchQueue

    init(label: String) {
        self.queue = DispatchQueue(label: label)
    }

    nonisolated func asUnownedSerialExecutor() -> UnownedSerialExecutor {
        UnownedSerialExecutor(ordinary: self)
    }

    nonisolated func enqueue(_ job: UnownedJob) {
        let executor = asUnownedSerialExecutor()
        queue.async {
            job.runSynchronously(on: executor)
        }
    }

    @available(iOS 17.0, *)
    nonisolated func enqueue(_ job: consuming ExecutorJob) {
        enqueue(UnownedJob(job))
    }

    @available(iOS 17.0, *)
    nonisolated func isSameExclusiveExecutionContext(other: _ValdiQueueExecutor) -> Bool {
        self === other
    }
}
