// Copyright © 2026 Snap, Inc. All rights reserved.

import Foundation

extension RustType {
    func pushStatement(
        value: String,
        to marshaller: String,
        asReturnValue: Bool,
        coreModule: String
    ) -> String {
        switch self {
        case .void:
            return "\(marshaller).push_undefined();"
        case .bytes:
            return asReturnValue
                ? "\(value).push_owned_to(\(marshaller))?;"
                : "\(coreModule)::ValdiPush::push_to(&\(value), \(marshaller))?;"
        case .nullable(let inner):
            let innerPush = inner.pushStatement(
                value: "inner",
                to: marshaller,
                asReturnValue: asReturnValue,
                coreModule: coreModule)
            // The Promise trampoline reserves native undefined for a skipped call.
            // An explicit native null still becomes JS undefined for an optional return.
            let absentValue: String
            if asReturnValue, case .future = inner {
                absentValue = "push_null"
            } else {
                absentValue = "push_undefined"
            }
            return "match \(value) { Some(inner) => { \(innerPush) }, None => \(marshaller).\(absentValue)() };"
        default:
            return "\(coreModule)::ValdiPush::push_to(&\(value), \(marshaller))?;"
        }
    }

    func deserializeExpression(from value: String, borrowed: Bool, coreModule: String) -> String {
        switch self {
        case .void:
            return "Ok(())"
        case .string:
            return borrowed ? "\(value).as_str()" : "\(value).as_string()"
        case .nullable(let inner):
            let innerExpression = inner.deserializeExpression(
                from: "inner",
                borrowed: borrowed,
                coreModule: coreModule)
            return "\(coreModule)::deserialize_optional(\(value), |inner| \(innerExpression))"
        default:
            let type = borrowed ? rustBorrowedType : rustOwnedType
            return "<\(type) as \(coreModule)::ValdiDeserialize>::from_valdi(\(value))"
        }
    }

    func deserializeBinding(
        named name: String, from value: String, borrowed: Bool, coreModule: String
    ) -> String {
        if borrowed {
            switch self {
            case .string:
                return "let \(name)_storage = \(value).as_str()?;\nlet \(name) = \(name)_storage.as_ref();"
            case .nullable(.string):
                return
                    "let \(name)_storage = \(coreModule)::deserialize_optional(\(value), |inner| inner.as_str())?;\nlet \(name) = \(name)_storage.as_deref();"
            default:
                break
            }
        }
        return
            "let \(name) = \(deserializeExpression(from: value, borrowed: borrowed, coreModule: coreModule))?;"
    }

    func serializeExpression(from value: String, coreModule: String) -> String {
        switch self {
        case .void:
            return "\(coreModule)::Value::undefined()"
        case .nullable(let inner):
            let innerExpression = inner.serializeExpression(from: "inner", coreModule: coreModule)
            return
                "match \(value) { Some(inner) => \(innerExpression), None => \(coreModule)::Value::undefined() }"
        default:
            return "\(coreModule)::ValdiSerialize::to_valdi(\(value))"
        }
    }

}
