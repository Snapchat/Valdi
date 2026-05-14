// Copyright © 2024 Snap, Inc. All rights reserved.

import Foundation
import SwiftProtobuf

/// Format for worker protocol messages
enum WorkerProtocolFormat: String {
    case json
    case protobuf
    
    static func fromCommandLineArguments(_ args: [String]) -> WorkerProtocolFormat {
        // Check for explicit format flags
        if args.contains("--persistent_worker_protocol=json") || args.contains("--json") {
            return .json
        }
        return .protobuf
    }
}

/// Unified internal representation of a work request
struct ParsedWorkRequest {
    let arguments: [String]
    let inputs: [ParsedInput]?
    let requestId: Int
    let cancel: Bool
    let verbosity: Int
    let sandboxDir: String?
    
    struct ParsedInput {
        let path: String
        let digest: String
    }
}

/// Unified internal representation of a work response
struct ParsedWorkResponse {
    let exitCode: Int
    let output: String
    let requestId: Int
    let wasCancelled: Bool
}

/// Protocol for adapting between wire format and internal representation
protocol WorkerProtocolAdapter {
    func parseWorkRequest(from data: Data) throws -> ParsedWorkRequest
    func serializeWorkResponse(_ response: ParsedWorkResponse) throws -> Data
    func serializeWorkResponseForWire(_ response: ParsedWorkResponse) throws -> Data
}

/// JSON protocol adapter
class JSONProtocolAdapter: WorkerProtocolAdapter {
    
    func parseWorkRequest(from data: Data) throws -> ParsedWorkRequest {
        let jsonRequest = try BazelWorkerProtocol.WorkRequest.fromJSON(data, keyDecodingStrategy: .useDefaultKeys)
        
        return ParsedWorkRequest(
            arguments: jsonRequest.arguments,
            inputs: jsonRequest.inputs?.map { input in
                ParsedWorkRequest.ParsedInput(path: input.path, digest: input.digest)
            },
            requestId: jsonRequest.requestId ?? 0,
            cancel: jsonRequest.cancel ?? false,
            verbosity: jsonRequest.verbosity ?? 0,
            sandboxDir: jsonRequest.sandboxDir
        )
    }
    
    func serializeWorkResponse(_ response: ParsedWorkResponse) throws -> Data {
        let jsonResponse = BazelWorkerProtocol.WorkResponse(
            exitCode: response.exitCode,
            output: response.output,
            requestId: response.requestId,
            wasCancelled: response.wasCancelled
        )
        
        return try jsonResponse.toJSON(keyEncodingStrategy: .useDefaultKeys)
    }
    
    func serializeWorkResponseForWire(_ response: ParsedWorkResponse) throws -> Data {
        var data = try serializeWorkResponse(response)
        // JSON responses are newline-delimited
        data.append(UInt8(0x0A))
        return data
    }
}

/// Protocol Buffer adapter
class ProtobufProtocolAdapter: WorkerProtocolAdapter {
    
    func parseWorkRequest(from data: Data) throws -> ParsedWorkRequest {
        let protoRequest = try Blaze_Worker_WorkRequest(serializedData: data)
        
        return ParsedWorkRequest(
            arguments: protoRequest.arguments,
            inputs: protoRequest.inputs.isEmpty ? nil : protoRequest.inputs.map { input in
                ParsedWorkRequest.ParsedInput(
                    path: input.path,
                    digest: input.digest.base64EncodedString()
                )
            },
            requestId: Int(protoRequest.requestID),
            cancel: protoRequest.cancel,
            verbosity: Int(protoRequest.verbosity),
            sandboxDir: protoRequest.sandboxDir.isEmpty ? nil : protoRequest.sandboxDir
        )
    }
    
    func serializeWorkResponse(_ response: ParsedWorkResponse) throws -> Data {
        var protoResponse = Blaze_Worker_WorkResponse()
        protoResponse.exitCode = Int32(response.exitCode)
        protoResponse.output = response.output
        protoResponse.requestID = Int32(response.requestId)
        protoResponse.wasCancelled = response.wasCancelled
        
        return try protoResponse.serializedData()
    }
    
    func serializeWorkResponseForWire(_ response: ParsedWorkResponse) throws -> Data {
        let messageData = try serializeWorkResponse(response)
        // Protobuf responses are length-delimited with varint prefix
        var wireData = VarintEncoding.writeVarint(messageData.count)
        wireData.append(messageData)
        return wireData
    }
}

/// Factory for creating the appropriate adapter
class WorkerProtocolAdapterFactory {
    static func createAdapter(for format: WorkerProtocolFormat) -> WorkerProtocolAdapter {
        switch format {
        case .json:
            return JSONProtocolAdapter()
        case .protobuf:
            return ProtobufProtocolAdapter()
        }
    }
}
