//
//  main.swift
//  Compiler
//
//  Created by Simon Corsin on 4/5/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation
import Backtrace

func isCompilingUnderBazel() -> Bool {
    return CommandLine.arguments.contains { $0 == "--bazel" }
}

func shouldRunAsPersistentWorker() -> Bool {
    return CommandLine.arguments.contains { $0 == "--persistent_worker" }
}

func getArgFilePath() -> String? {
    let arguments = CommandLine.arguments
    guard arguments.count == 2 else { return nil }
    let arg = arguments[1]
    let prefixToCheck = "@"
    guard arg.hasPrefix(prefixToCheck) else { return nil }

    return String(arg[prefixToCheck.endIndex..<arg.endIndex])
}

func main() {
    // Installing Backtrace's signal handlers for sane-ish stack traces when crashing on Linux
    // (this is a no-op on macOS)
    Backtrace.install(signals: [SIGILL, SIGSEGV, SIGBUS, SIGFPE, SIGQUIT])

    // Ignore SIGPIPE signals, they are very difficult to handle properly with the Foundation APIs
    signal(SIGPIPE, SIG_IGN)

    SentryClient.setTag(value: String(isCompilingUnderBazel()), key: "bazel_build")

    if shouldRunAsPersistentWorker() {
        let logger = Logger(output: FileHandleLoggerOutput(stdout: FileHandle.standardError, stderr: FileHandle.standardError))
        let bazelPersistentWorker = BazelPersistentWorker(stdin: FileHandle.standardInput, stdout: FileHandle.standardOutput, logger: logger)
        let companionExecutableProvider = CompanionExecutableProvider(logger: logger)
        bazelPersistentWorker.run { request in
            let arguments: ValdiCompilerArguments
            do {
                arguments = try ValdiCompilerArguments.parse(request.arguments)
            } catch let error {
                return BazelPersistentWorker.Response(result: Promise(error: error))
            }

            let runner = ValdiCompilerRunner(logger: request.logger, 
                                                arguments: arguments,
                                                companionExecutableProvider: companionExecutableProvider,
                                                workingDirectoryPath: request.workingDirectory,
                                                runningAsWorker: true)
            if runner.run() {
                return BazelPersistentWorker.Response(result: Promise(data: ()))
            } else {
                return BazelPersistentWorker.Response(result: Promise(error: CompilerError("Compilation failed")))
            }
        }
    } else {
        if let argFilePath = getArgFilePath() {
            let argFile: String
            do {
                argFile = try String(contentsOfFile: argFilePath)
            } catch let error {
                print("Failed to read arg file: \(error.legibleLocalizedDescription)")
                _exit(1)
            }

            let resolvedArguments = argFile.components(separatedBy: "\n").filter { !$0.isEmpty }
            ValdiCompilerArguments.main(resolvedArguments)
        } else {
            // Sentry doesn't really support running on multiple instances of a program at once.
            // It reads and writes crash data into the same file
            if !isCompilingUnderBazel() {
                let arguments = Array(CommandLine.arguments.dropFirst())
                if let parsedArgs = try? ValdiCompilerArguments.parseAsRoot(arguments) as? ValdiCompilerArguments,
                   let sentryDsn = parsedArgs.sentryDsn {
                    SentryClient.start(dsn: sentryDsn,
                                    tracesSampleRate: 0.25,
                                    username: NSUserName())
                }
            }

            ValdiCompilerArguments.main()
        }

    }
}
main()
