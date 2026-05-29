//
//  File.swift
//  
//
//  Created by Simon Corsin on 8/2/22.
//

import Foundation

class ImageToolbox {

    private let toolboxExecutable: ToolboxExecutable

    init(toolboxExecutable: ToolboxExecutable) {
        self.toolboxExecutable = toolboxExecutable
    }

    func convert(inputPath: String, outputPath: String, outputWidth: Int, outputHeight: Int, qualityRatio: Double?) throws {
        try toolboxExecutable.convertImage(inputFilePath: inputPath,
                                           outputFilePath: outputPath,
                                           outputWidth: outputWidth,
                                           outputHeight: outputHeight,
                                           qualityRatio: qualityRatio)
    }

    func getInfo(inputPath: String) throws -> ToolboxExecutable.ImageInfoOutput {
        return try toolboxExecutable.getImageInfo(inputFilePath: inputPath)
    }

    func getVersion() throws -> String {
        return try toolboxExecutable.getVersionString()
    }

    func optimizePNG(inputPath: String) throws {
        _ = try toolboxExecutable.pngquant(arguments: [
            "-i",
            inputPath
        ])
    }
}
