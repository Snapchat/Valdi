// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustReferencedTypeGenerator {
    private init() {}

    private static func collectTypes<T>(
        in types: [RustType],
        select: (RustType) -> (String, T)?
    ) -> [T] {
        var values: [T] = []
        var seen: Set<String> = []

        func visit(_ type: RustType) {
            switch type {
            case .callback(let callback):
                callback.parameters.forEach { visit($0.type) }
                visit(callback.returnType)
            case .future(let future):
                visit(future.value)
            case .valueArray(let element), .nullable(let element):
                visit(element)
            default:
                break
            }
            if let (name, value) = select(type), seen.insert(name).inserted {
                values.append(value)
            }
        }

        types.forEach(visit)
        return values
    }

    static func referencedCallbacks(in types: [RustType]) -> [RustCallbackType] {
        collectTypes(in: types) { type in
            guard case .callback(let callback) = type else { return nil }
            return (callback.rustName, callback)
        }
    }

    static func referencedFutures(in types: [RustType]) -> [RustFutureType] {
        collectTypes(in: types) { type in
            guard case .future(let future) = type else { return nil }
            return (future.rustName, future)
        }
    }

    static func addDefinitions(requiredBy types: [RustType], to files: RustGeneratedFiles)
        -> RustGeneratedFiles
    {
        let futures = referencedFutures(in: types).map {
            RustFutureGenerator.generateDefinition(for: $0)
        }
        let callbacks = referencedCallbacks(in: types).map {
            RustCallbackGenerator.generateDefinition(for: $0)
        }
        return RustGeneratedFiles(
            rust: files.rust + futures.joined() + callbacks.joined(),
            cHeader: files.cHeader,
            registrationSymbols: files.registrationSymbols)
    }
}
