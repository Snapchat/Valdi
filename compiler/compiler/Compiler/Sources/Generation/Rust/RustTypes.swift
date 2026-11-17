// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

struct RustCallbackParameter {
    let name: String
    let type: RustType
}

struct RustCallbackType {
    let rustName: String
    let parameters: [RustCallbackParameter]
    let returnType: RustType
}

struct RustFutureType {
    let rustName: String
    let value: RustType
}

indirect enum RustType {
    case void
    case bool
    case double
    case long
    case string
    case bytes
    case enumType(String)
    case genericParameter(String)
    case model(rust: String, borrowed: String)
    case handle(rust: String, borrowed: String)
    case callback(RustCallbackType)
    case valueArray(RustType)
    case future(RustFutureType)
    case nullable(RustType)

    var rustOwnedType: String {
        switch self {
        case .void: return "()"
        case .bool: return "bool"
        case .double: return "f64"
        case .long: return "i64"
        case .string: return "String"
        case .bytes: return "valdi_core_rust::BytesView"
        case .enumType(let rust): return rust
        case .genericParameter(let name): return name
        case .model(let rust, _): return rust
        case .handle(let rust, _): return rust
        case .callback(let callback): return callback.rustName
        case .valueArray(let element): return "valdi_core_rust::Array<\(element.rustOwnedType)>"
        case .future(let future): return future.rustName
        case .nullable(let inner): return "Option<\(inner.rustOwnedType)>"
        }
    }

    var rustBorrowedType: String {
        switch self {
        case .string: return "&str"
        case .bytes: return "valdi_core_rust::BytesRef<'_>"
        case .model(_, let borrowed): return borrowed
        case .handle(_, let borrowed): return borrowed
        case .genericParameter(let name): return name
        case .callback(let callback): return "\(callback.rustName)Ref<'_>"
        case .valueArray(let element):
            return "valdi_core_rust::ArrayRef<'_, \(element.rustNestedBorrowedType)>"
        case .future(let future): return "\(future.rustName)Ref<'_>"
        case .nullable(let inner): return "Option<\(inner.rustBorrowedType)>"
        default: return rustOwnedType
        }
    }

    var rustNestedBorrowedType: String {
        switch self {
        case .string:
            return "valdi_core_rust::StringRef<'_>"
        default:
            return rustBorrowedType
        }
    }
}

struct RustResolvedProperty {
    let property: ValdiModelProperty
    let rustName: String
}

struct RustResolvedMethod {
    let property: RustResolvedProperty
    let parameters: [(name: String, type: RustType)]
    let returnType: RustType
}

struct RustGeneratedFiles {
    let rust: String
    let cHeader: String
    let registrationSymbols: [String]
}
