//
//  HotReloadLifecycleReporter.swift
//  Compiler
//

import Foundation

private struct HotReloadLifecycleEvent: Encodable {
    let source = "valdi_hotreload"
    let event: String
    let target: String
    let port: Int?
    let time: String
    let clientId: Int?
    let applicationId: String?
    let platform: String?
    let resourceCount: Int?
    let changedFileCount: Int?
}

final class HotReloadLifecycleReporter {
    typealias Output = (String) -> Void

    private let target: String
    private let port: Int?
    private let output: Output
    private let errorOutput: Output
    // Emitted from multiple queues (DaemonService connection queue, AutoRecompiler main
    // queue); serialize writes so concurrent lines are not interleaved.
    private let writeLock = NSLock()

    init(target: String,
         port: Int?,
         output: @escaping Output,
         errorOutput: @escaping Output) {
        self.target = target
        self.port = port
        self.output = output
        self.errorOutput = errorOutput
    }

    static func standardOutput(enabled: Bool,
                               target: String,
                               port: Int?) -> HotReloadLifecycleReporter? {
        guard enabled else {
            return nil
        }

        return HotReloadLifecycleReporter(
            target: target,
            port: port,
            output: { line in
                FileHandle.standardOutput.write(Data("\(line)\n".utf8))
            },
            errorOutput: { line in
                FileHandle.standardError.write(Data("\(line)\n".utf8))
            })
    }

    func targetConnected(clientId: Int, applicationId: String?, platform: String?) {
        emit(event: "target_connected",
             clientId: clientId,
             applicationId: applicationId,
             platform: platform,
             resourceCount: nil,
             changedFileCount: nil)
    }

    func resourcesSent(clientId: Int,
                       applicationId: String?,
                       platform: String?,
                       resourceCount: Int) {
        emit(event: "resources_sent",
             clientId: clientId,
             applicationId: applicationId,
             platform: platform,
             resourceCount: resourceCount,
             changedFileCount: nil)
    }

    func recompilationSucceeded(changedFileCount: Int) {
        emit(event: "recompilation_succeeded",
             clientId: nil,
             applicationId: nil,
             platform: nil,
             resourceCount: nil,
             changedFileCount: changedFileCount)
    }

    private func emit(event: String,
                      clientId: Int?,
                      applicationId: String?,
                      platform: String?,
                      resourceCount: Int?,
                      changedFileCount: Int?) {
        let payload = HotReloadLifecycleEvent(event: event,
                                              target: target,
                                              port: port,
                                              time: ISO8601DateFormatter().string(from: Date()),
                                              clientId: clientId,
                                              applicationId: applicationId,
                                              platform: platform,
                                              resourceCount: resourceCount,
                                              changedFileCount: changedFileCount)
        do {
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
            let line = String(decoding: try encoder.encode(payload), as: UTF8.self)
            writeLock.lock()
            defer { writeLock.unlock() }
            output(line)
        } catch {
            writeLock.lock()
            defer { writeLock.unlock() }
            errorOutput("Failed to encode Valdi hot reload lifecycle event '\(event)': \(error)")
        }
    }
}
