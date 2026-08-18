//
//  HotReloadLifecycleReporter.swift
//  Compiler
//

import Foundation

private struct HotReloadLifecycleEvent: Encodable {
    let source = "valdi_hotreload"
    let event: String
    let time: String
    let changedFileCount: Int
}

final class HotReloadLifecycleReporter {
    typealias Output = (String) -> Void

    private let output: Output
    private let errorOutput: Output

    init(output: @escaping Output,
         errorOutput: @escaping Output) {
        self.output = output
        self.errorOutput = errorOutput
    }

    static func standardOutput(enabled: Bool) -> HotReloadLifecycleReporter? {
        guard enabled else {
            return nil
        }

        return HotReloadLifecycleReporter(
            output: { line in
                FileHandle.standardOutput.write(Data("\(line)\n".utf8))
            },
            errorOutput: { line in
                FileHandle.standardError.write(Data("\(line)\n".utf8))
            })
    }

    func recompilationSucceeded(changedFileCount: Int) {
        let payload = HotReloadLifecycleEvent(event: "recompilation_succeeded",
                                              time: ISO8601DateFormatter().string(from: Date()),
                                              changedFileCount: changedFileCount)
        emit(payload)
    }

    private func emit(_ payload: HotReloadLifecycleEvent) {
        do {
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
            output(String(decoding: try encoder.encode(payload), as: UTF8.self))
        } catch {
            errorOutput("Failed to encode Valdi hot reload recompilation event: \(error)")
        }
    }
}
