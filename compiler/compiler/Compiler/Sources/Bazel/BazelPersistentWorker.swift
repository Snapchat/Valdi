// Copyright © 2024 Snap, Inc. All rights reserved.

import Foundation

class BazelPersistentWorker {

    struct Request {
        let arguments: [String]
        let logger: ILogger
        let cancelableToken: StateCancelable
        let workingDirectory: String
    }

    struct Response {
        let result: Promise<Void>
    }

    typealias StartHandler = (Request) -> Response

    private struct CurrentRequest {
        let cancelableToken: StateCancelable
    }

    private let stdin: FileHandleReader
    private let stdout: FileHandle
    private let workQueue = DispatchQueue(label: "com.snap.valdi.compiler.BazelWorkQueue", qos: .userInitiated)
    private let logger: ILogger
    private let format: WorkerProtocolFormat
    private let adapter: WorkerProtocolAdapter
    private var requestById = [Int: CurrentRequest]()
    private var buffer = Data()
    private var readSource: DispatchSourceRead?

    init(stdin: FileHandle, stdout: FileHandle, logger: ILogger, format: WorkerProtocolFormat = .protobuf) {
        self.stdin = FileHandleReader(fileHandle: stdin, dispatchQueue: DispatchQueue.main)
        self.stdout = stdout
        self.logger = logger
        self.format = format
        self.adapter = WorkerProtocolAdapterFactory.createAdapter(for: format)
    }

    func run(startHandler: @escaping StartHandler) {
        self.stdin.onDidReceiveData = { [weak self] reader in
            guard let self else { return }
            while self.consumeNextRequest(startHandler: startHandler) {}
        }
        RunLoop.main.run()
    }

    private func submitResponse(exitCode: Int, output: String, requestId: Int, wasCancelled: Bool) {
        let response = ParsedWorkResponse(
            exitCode: exitCode,
            output: output,
            requestId: requestId,
            wasCancelled: wasCancelled
        )

        do {
            let wireData = try adapter.serializeWorkResponseForWire(response)
            try self.stdout.write(contentsOf: wireData)
        } catch let error {
            logger.error("Failed to write Bazel response: \(error.legibleLocalizedDescription)")
            logger.flush()
            _exit(1)
        }
    }

    private func onRequestCompleted(result: Result<Void, Error>, logOutput: BufferLoggerOutput, requestId: Int) {
        guard requestById.removeValue(forKey: requestId) != nil else {
            return
        }

        var logData = logOutput.data
        var exitCode: Int32

        switch result {
        case .success:
            exitCode = EXIT_SUCCESS
        case .failure(let error):
            exitCode = EXIT_FAILURE
            if let errorData = try? error.legibleLocalizedDescription.utf8Data() {
                logData.append(errorData)
            }
        }
        submitResponse(exitCode: Int(exitCode), output: String(data: logData, encoding: .utf8) ?? "", requestId: requestId, wasCancelled: false)
    }

    private func process(data: Data, startHandler: @escaping StartHandler) {
        do {
            let workRequest = try adapter.parseWorkRequest(from: data)
            let requestId = workRequest.requestId
            
            if workRequest.cancel {

                if let cancelToken = self.requestById.removeValue(forKey: requestId) {
                    cancelToken.cancelableToken.cancel()
                    self.submitResponse(exitCode: 0, output: "", requestId: requestId, wasCancelled: true)
                }
                return
            }

            let cancelableToken = StateCancelable()

            self.requestById[requestId] = CurrentRequest(cancelableToken: cancelableToken)

            workQueue.async {
                let logOutput = BufferLoggerOutput()
                let logger = Logger(output: logOutput)
                let response = startHandler(Request(
                    arguments: workRequest.arguments,
                    logger: logger,
                    cancelableToken: cancelableToken,
                    workingDirectory: workRequest.sandboxDir ?? FileManager.default.currentDirectoryPath))
                response.result.onComplete { result in
                    logger.flush()
                    DispatchQueue.main.async {
                        self.onRequestCompleted(result: result, logOutput: logOutput, requestId: requestId)
                    }
                }
            }
        } catch let error {
            let inputData = String(data: data, encoding: .utf8) ?? ""
            logger.error("Failed to process Bazel request: \(error.legibleLocalizedDescription) (request content: \(inputData)")
            logger.flush()
            _exit(1)
        }
    }

    private func consumeNextRequest(startHandler: @escaping StartHandler) -> Bool {
        switch format {
        case .json:
            return consumeNextJSONRequest(startHandler: startHandler)
        case .protobuf:
            return consumeNextProtobufRequest(startHandler: startHandler)
        }
    }

    private func consumeNextProtobufRequest(startHandler: @escaping StartHandler) -> Bool {
        let bufferData = stdin.content
        
        // Need at least 1 byte for varint
        guard !bufferData.isEmpty else {
            return false
        }
        
        let varintResult: VarintEncoding.ReadResult
        do {
            varintResult = try VarintEncoding.readVarint(from: bufferData)
        } catch VarintEncoding.Error.bufferTooShort {
            return false
        } catch {
            logger.error("Failed to parse varint length prefix: \(error.legibleLocalizedDescription)")
            logger.flush()
            _exit(1)
        }
        
        let messageLength = varintResult.value
        let varintBytes = varintResult.bytesRead
        let totalBytesNeeded = varintBytes + messageLength
        
        guard bufferData.count >= totalBytesNeeded else {
            return false
        }

        let messageData = bufferData.subdata(in: varintBytes..<totalBytesNeeded)
        stdin.trimContent(by: totalBytesNeeded)
        process(data: messageData, startHandler: startHandler)
        return true
    }

    private func consumeNextJSONRequest(startHandler: @escaping StartHandler) -> Bool {
        let bufferData = stdin.content
        
        // JSON messages are newline-delimited
        guard !bufferData.isEmpty else {
            return false
        }
        
        guard let newlineIndex = bufferData.firstIndex(of: 0x0A) else {
            return false
        }
        
        let messageData = bufferData.subdata(in: 0..<newlineIndex)
        stdin.trimContent(by: newlineIndex + 1)
        
        process(data: messageData, startHandler: startHandler)
        return true
    }

}
