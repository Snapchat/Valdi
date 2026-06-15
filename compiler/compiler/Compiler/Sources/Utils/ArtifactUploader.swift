//
//  ArtifactUploader.swift
//  Compiler
//
//  Created by Simon Corsin on 8/8/19.
//

import Foundation
import SwiftProtobuf
import Chalk

struct ArtifactInfo: Codable {
    let url: URL
    let sha256Digest: String
}

/// Upload artifacts to a remote endpoint
class ArtifactUploader {
    private let moduleBaseUploadURL: URL
    private let companionExecutable: CompanionExecutable

    private var pendingPreparedArtifact = Synchronized(data: [String: File]())

    init(moduleBaseUploadURL: URL, companionExecutable: CompanionExecutable) {
        self.moduleBaseUploadURL = moduleBaseUploadURL
        self.companionExecutable = companionExecutable
    }

    func makePreparedUploadArtifact() throws -> File {
        let allItems = pendingPreparedArtifact.data { data in
            return data
        }

        let builder = ValdiModuleBuilder(items: allItems.map { ZippableItem(file: $0.value, path: $0.key) })
        builder.compress = false
        return .data(try builder.build())
    }

    func appendToPreparedArtifact(artifactName: String, artifactData: Data, sha256: String) -> Promise<ArtifactInfo> {
        let outputFilename = "\(artifactName)-\(sha256)"

        pendingPreparedArtifact.data { dict in
            dict[outputFilename] = .data(artifactData)
        }

        let resultURL = moduleBaseUploadURL.appendingPathComponent(outputFilename)
        let artifactInfo = ArtifactInfo(url: resultURL, sha256Digest: sha256)
        return Promise(data: artifactInfo)
    }

    func upload(artifactName: String, artifactData: Data, sha256: String) -> Promise<ArtifactInfo> {
        let key = sha256
        return companionExecutable.uploadArtifact(artifactName: artifactName, artifactData: artifactData, sha256: sha256)
    }

    static func uploadFiles(fileManager: ValdiFileManager, paths: [String], baseURL: URL, out: String?, moduleBaseUploadURL: URL?, companionExectuable: CompanionExecutable) throws {
        guard let moduleBaseUploadURL = moduleBaseUploadURL else {
            throw CompilerError("--upload-base-url must be provided when using the --upload-module utility command")
        }

        let outputURL = try CLIUtils.getOutputURLs(commandName: "--upload-module", baseUrl: baseURL, out: out)

        for inputPath in paths {
            let inputURL = baseURL.resolving(path: inputPath)

            let fileData = try File.url(inputURL).readData()

            let artifactUploader = ArtifactUploader(moduleBaseUploadURL: moduleBaseUploadURL, companionExecutable: companionExectuable)

            let artifactName = inputURL.lastPathComponent
            let sha256Digest = try fileData.generateSHA256Digest()
            let artifactInfo = try artifactUploader.upload(artifactName: artifactName, artifactData: fileData, sha256: sha256Digest).waitForData()

            let outputFileContent = [artifactInfo.url.absoluteString, artifactInfo.sha256Digest].joined(separator: "\n")
            try fileManager.save(data: outputFileContent.utf8Data(), to: outputURL)
        }
    }

}
