//
//  URL+Navigation.swift
//  Compiler
//
//  Created by Simon Corsin on 4/29/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

struct RelativePath: CustomStringConvertible, CustomDebugStringConvertible, Hashable, Equatable, Comparable {
    let components: [String]

    init(from: URL, to: URL) {
        let components = from.pathComponents.replacingPrefix(["/", "private"], replacement: ["/"])
        let otherComponents = to.pathComponents.replacingPrefix(["/", "private"], replacement: ["/"])
        self.init(from: components, to: otherComponents)
    }

    init(from components: [String], to otherComponents: [String]) {
        var matchingIndex = -1
        var index = 0
        while index < components.count && index < otherComponents.count {
            if components[index] == otherComponents[index] {
                matchingIndex = index
            } else {
                break
            }
            index += 1
        }

        let dotDots = Array(repeating: "..", count: components.count - matchingIndex - 1)
        let componentsRest = otherComponents.dropFirst(matchingIndex + 1)
        self.components = dotDots + componentsRest
    }

    init(url: URL) {
        self.components = url.pathComponents
    }

    init(components: [String]) {
        self.components = components
    }

    var description: String {
        if components.isEmpty {
            return "."
        } else {
            return components.joined(separator: "/")
        }
    }

    var debugDescription: String {
        return description
    }

    static func <(lhs: RelativePath, rhs: RelativePath) -> Bool {
        if lhs.components.count < rhs.components.count {
            return true
        } else if lhs.components.count > rhs.components.count {
            return false
        }

        for i in 0..<lhs.components.count {
            if lhs.components[i] < rhs.components[i] {
                return true
            } else if lhs.components[i] > rhs.components[i] {
                return false
            }
        }

        return false
    }
}

extension URL {

    // NOTE: Only resolves symlinks if the file at `path` exists.
    // See: https://developer.apple.com/documentation/foundation/nsurl/1415965-resolvingsymlinksinpath
    func resolving(path: String, isDirectory: Bool = false) -> URL {
        var newPath = path

        if newPath.first == "~" {
            newPath = "\(NSHomeDirectory())\(newPath[1..<newPath.count])"
        }

        return URL(fileURLWithPath: newPath, isDirectory: isDirectory, relativeTo: self).standardizedFileURL.absoluteURL
    }

    func relativePath(from url: URL) -> String {
        return url.relativePath(to: self)
    }

    func relativePath(to url: URL) -> String {
        return RelativePath(from: self, to: url).description
    }

    private static func getCommonPathComponent(index: Int, allPathComponents: [[String]]) -> String? {
        var commonPathComponent: String? = nil

        for pathComponents in allPathComponents {
            let pathComponent = pathComponents[index]
            if let commonPathComponent {
                if commonPathComponent != pathComponent {
                    return nil
                }
            } else {
                commonPathComponent = pathComponent
            }
        }
        return commonPathComponent
    }

    static func commonFileURL(in urls: [URL]) -> URL {
        var commonComponents = [String]()
        let allPathComponents = urls.map { $0.pathComponents }

        let lowestNumberOfPathComponents = allPathComponents.reduce(Int.max) { partialResult, components in
            return min(partialResult, components.count)
        }

        for i in 0..<lowestNumberOfPathComponents {
            if let commonPathComponent = getCommonPathComponent(index: i, allPathComponents: allPathComponents) {
                commonComponents.append(commonPathComponent)
            } else {
                break
            }
        }

        return URL(fileURLWithPath: commonComponents.joined(separator: "/"))
    }

}
