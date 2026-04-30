//
//  TypeScriptAnnotation.swift
//  Compiler
//
//  Created by Simon Corsin on 12/5/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

// CharacterSet to use when trimming contents of code comments (e.g. leading \* treated like whitespace)
private let trimCharSet = CharacterSet.whitespacesAndNewlines.union(CharacterSet(charactersIn: "*"))

// Special user-friendly handling for quotes, including unicode quote characters, because
// we actually had a case where someone used a unicode quotation mark, Ticket: 223
private let asciiQuoteCharSet = CharacterSet(charactersIn: "\'\"")
private let unicodeQuoteCharSet = CharacterSet(charactersIn: "«»‘’‚‛”„‟〝〞＂")
private let quoteCharSet = asciiQuoteCharSet.union(unicodeQuoteCharSet)

// TODO: rename to `TypeScriptAnnotation`
struct ValdiTypeScriptAnnotation {

    let name: String
    let range: NSRange
    let parameters: [String: String]?
    let content: String

    init(name: String, parameters: [String: String]?, range: NSRange, content: String) {
        self.name = name
        self.parameters = parameters
        self.range = range
        self.content = content
    }

    private static let annotationRegex = try! NSRegularExpression(pattern: "@([A-z-]+) *(?:\\((?:\\{(.*?)\\})\\))?", options: [.dotMatchesLineSeparators])

    static func extractAnnotations(comments: TS.AST.Comments, fileContent: String) throws -> [ValdiTypeScriptAnnotation] {
        let joinedComments = comments.text
        let matches = Self.annotationRegex.matches(in: joinedComments, options: [], range: joinedComments.nsrange)

        if matches.isEmpty {
             return []
        }

        let commentsRange = NSRange(location: comments.start, length: comments.end - comments.start)

        let nsString = comments.text as NSString
        var annotations = [ValdiTypeScriptAnnotation]()

        for match in matches {
            let annotationNameRange = match.range(at: 1)
            let annotationName = nsString.substring(with: annotationNameRange)
            // Make sure we aren't parsing jsDocs tags as annotations.
            if JSDocs.allSymbols.contains(annotationName) {
                continue
            }

            let annotationRange = match.range(at: 0)
            let annotationContent = nsString.substring(with: annotationRange)

            let totalRange = NSRange(location: annotationNameRange.lowerBound + commentsRange.lowerBound, length: annotationNameRange.length)

            var parameters: [String: String]?

            let parametersRange = match.range(at: 2)
            if parametersRange.location != NSNotFound {
                let rangeInFile = NSRange(location: commentsRange.location + parametersRange.location, length: parametersRange.length)
                let parametersString = nsString.substring(with: parametersRange)
                var foundParameters = [String: String]()
                for property in parametersString.split(separator: ",") {
                    let (key, value) = try parseAnnotationKeyValue(property: property,
                                                                   rangeInFile: rangeInFile,
                                                                   fileContent: fileContent)
                    foundParameters[key] = value
                }
                parameters = foundParameters
            }

            annotations.append(ValdiTypeScriptAnnotation(name: annotationName, parameters: parameters, range: totalRange, content: annotationContent))
        }

        return annotations
    }

    private static func parseAnnotationKeyValue(property: Substring, rangeInFile: NSRange, fileContent: String) throws -> (key: String, value: String) {
        guard let separatorIndex = property.firstIndex(of: ":") else {
            try throwAnnotationError(message: "Cannot parse annotation - missing column separator",
                                     range: rangeInFile,
                                     inDocument: fileContent)
        }

        let key = String(property[..<separatorIndex].trimmingCharacters(in: trimCharSet).unquote)
        let value = String(property[property.index(separatorIndex, offsetBy: 1)...].trimmingCharacters(in: trimCharSet).unquote)

        let keyOrValueHasQuotes = [key, value].contains(where: { $0.unicodeScalars.contains(where: quoteCharSet.contains) })
        if keyOrValueHasQuotes {
            try throwAnnotationError(message: "Cannot parse annotation - maybe missing/extra/invalid quote character (\' or \")?",
                                     range: rangeInFile,
                                     inDocument: fileContent)
        }

        return (key, value)
    }

    private static func throwAnnotationError(message: String, range: NSRange, inDocument: String) throws -> Never {
        throw CompilerError(type: "Annotation error", message: message, range: range, inDocument: inDocument)
    }
}
