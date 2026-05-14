// swift-tools-version:5.6
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "Compiler",
    platforms: [
        .macOS(.v11),
    ],
    dependencies: [
        .package(path: "Vendors/BlueSocket"),
        .package(path: "Vendors/SwiftCSSParser"),
        .package(url: "https://github.com/drmohundro/SWXMLHash.git", from: "7.0.0"),
        .package(url: "https://github.com/apple/swift-protobuf.git", from: "1.25.2"),
        .package(path: "Vendors/Zstd"),
        .package(path: "Vendors/Clibsass"),
        .package(url: "https://github.com/mxcl/LegibleError.git", from: "1.0.6"),
        .package(url: "https://github.com/mxcl/Chalk.git", from: "0.5.0"),
        .package(url: "https://github.com/jpsim/Yams.git", from: "5.0.6"),
        .package(url: "https://github.com/swift-server/swift-backtrace.git", from: "1.3.3"),
        .package(url: "https://github.com/getsentry/sentry-cocoa", from: "8.22.4"),
        .package(url: "https://github.com/apple/swift-argument-parser", from: "1.3.1"),
        .package(url: "https://github.com/scinfu/SwiftSoup.git", from: "2.6.0"),
        .package(url: "https://github.com/apple/swift-crypto.git", from: "3.1.0"),
        .package(url: "https://github.com/marmelroy/Zip.git", .upToNextMinor(from: "2.1.0")),
        .package(
          url: "https://github.com/apple/swift-collections.git", .upToNextMinor(from: "1.1.0")  ),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages which this package depends on.
        .executableTarget(
            name: "Compiler",
            dependencies: [
                .product(name: "Backtrace", package: "swift-backtrace"),
                "BlueSocket",
                "SwiftCSSParser",
                "SWXMLHash",
                .product(name: "SwiftProtobuf", package: "swift-protobuf"),
                "Clibsass",
                "Zstd",
                "LegibleError",
                "Chalk",
                "SwiftSoup",
                "Yams",
                "Zip",
                .product(name: "Crypto", package: "swift-crypto"),
                .product(name: "Sentry", package: "sentry-cocoa", condition: .when(platforms: [.macOS])),
                .product(name: "ArgumentParser", package: "swift-argument-parser"),
                .product(name: "Collections", package: "swift-collections"),
            ],
            path: "Sources"
        ),
        .testTarget(
            name: "CompilerTests",
            dependencies: ["Compiler", "SwiftSoup", "Zstd"]
        ),
    ]
)
