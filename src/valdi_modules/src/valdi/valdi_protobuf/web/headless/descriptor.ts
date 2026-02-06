/**
 * This file contains protobuf descriptor types copied from @protobuf-ts/plugin-framework.
 * We duplicate these types locally instead of importing from the package because:
 *
 * 1. @protobuf-ts/plugin-framework has a hard dependency on the TypeScript compiler package
 *    (typescript ^3.9), which is ~15MB and not browser-safe.
 *
 * 2. The plugin-framework is designed for building protoc plugins (code generation tools),
 *    not for browser runtime use. It pulls in TypeScript for AST manipulation.
 *
 * 3. When webpack bundles the code, it tries to include the entire TypeScript compiler,
 *    which fails with "Cannot find module" errors for Node.js-specific modules.
 *
 * 4. The descriptor types themselves only depend on @protobuf-ts/runtime (which IS
 *    browser-safe), so we can safely copy just the types we need.
 *
 * SOURCE: @protobuf-ts/plugin-framework v2.11.0
 *         node_modules/@protobuf-ts/plugin-framework/build/es2015/google/protobuf/descriptor.js
 *
 * NOTE: This is a minimal subset - only types used by DescriptorDatabase are included.
 * If you need additional descriptor types, copy from the source package.
 */

import { MessageType } from "@protobuf-ts/runtime";

// ============================================================================
// Type definitions (interfaces used by DescriptorDatabase)
// ============================================================================

export type DescriptorProto = {
  name?: string;
  field?: FieldDescriptorProto[];
  nestedType?: DescriptorProto[];
  enumType?: EnumDescriptorProto[];
  options?: MessageOptions;
};

export type EnumDescriptorProto = {
  name?: string;
  value: { name?: string; number?: number }[];
};

export type FieldDescriptorProto = {
  name?: string;
  number?: number;
  label?: FieldDescriptorProto_Label;
  type?: FieldDescriptorProto_Type;
  typeName?: string;
};

export type FileDescriptorProto = {
  name?: string;
  package?: string;
  messageType?: DescriptorProto[];
  enumType?: EnumDescriptorProto[];
};

export type FileDescriptorSet = {
  file?: FileDescriptorProto[];
};

export type MessageOptions = {
  mapEntry?: boolean;
};

// ============================================================================
// Enums (used for field type/label checking)
// ============================================================================

export enum FieldDescriptorProto_Type {
    UNSPECIFIED$ = 0,
    DOUBLE = 1,
    FLOAT = 2,
    INT64 = 3,
    UINT64 = 4,
    INT32 = 5,
    FIXED64 = 6,
    FIXED32 = 7,
    BOOL = 8,
    STRING = 9,
    GROUP = 10,
    MESSAGE = 11,
    BYTES = 12,
    UINT32 = 13,
    ENUM = 14,
    SFIXED32 = 15,
    SFIXED64 = 16,
    SINT32 = 17,
    SINT64 = 18,
}

export enum FieldDescriptorProto_Label {
    UNSPECIFIED$ = 0,
    OPTIONAL = 1,
    REQUIRED = 2,
    REPEATED = 3,
}

// ============================================================================
// MessageType instances (needed for binary decoding via fromBinary)
// ============================================================================

class FileDescriptorSet$Type extends MessageType<FileDescriptorSet> {
    constructor() {
        super("google.protobuf.FileDescriptorSet", [
            { no: 1, name: "file", kind: "message", repeat: 2, T: () => FileDescriptorProtoType }
        ]);
    }
}
export const FileDescriptorSetType = new FileDescriptorSet$Type();

class FileDescriptorProto$Type extends MessageType<FileDescriptorProto> {
    constructor() {
        super("google.protobuf.FileDescriptorProto", [
            { no: 1, name: "name", kind: "scalar", opt: true, T: 9 },
            { no: 2, name: "package", kind: "scalar", opt: true, T: 9 },
            { no: 4, name: "message_type", kind: "message", repeat: 2, T: () => DescriptorProtoType },
            { no: 5, name: "enum_type", kind: "message", repeat: 2, T: () => EnumDescriptorProtoType },
        ]);
    }
}
export const FileDescriptorProtoType = new FileDescriptorProto$Type();

class DescriptorProto$Type extends MessageType<DescriptorProto> {
    constructor() {
        super("google.protobuf.DescriptorProto", [
            { no: 1, name: "name", kind: "scalar", opt: true, T: 9 },
            { no: 2, name: "field", kind: "message", repeat: 2, T: () => FieldDescriptorProtoType },
            { no: 3, name: "nested_type", kind: "message", repeat: 2, T: () => DescriptorProtoType },
            { no: 4, name: "enum_type", kind: "message", repeat: 2, T: () => EnumDescriptorProtoType },
            { no: 7, name: "options", kind: "message", T: () => MessageOptionsType },
        ]);
    }
}
export const DescriptorProtoType = new DescriptorProto$Type();

class FieldDescriptorProto$Type extends MessageType<FieldDescriptorProto> {
    constructor() {
        super("google.protobuf.FieldDescriptorProto", [
            { no: 1, name: "name", kind: "scalar", opt: true, T: 9 },
            { no: 3, name: "number", kind: "scalar", opt: true, T: 5 },
            { no: 4, name: "label", kind: "enum", opt: true, T: () => ["google.protobuf.FieldDescriptorProto.Label", FieldDescriptorProto_Label, "LABEL_"] },
            { no: 5, name: "type", kind: "enum", opt: true, T: () => ["google.protobuf.FieldDescriptorProto.Type", FieldDescriptorProto_Type, "TYPE_"] },
            { no: 6, name: "type_name", kind: "scalar", opt: true, T: 9 },
        ]);
    }
}
export const FieldDescriptorProtoType = new FieldDescriptorProto$Type();

class EnumDescriptorProto$Type extends MessageType<EnumDescriptorProto> {
    constructor() {
        super("google.protobuf.EnumDescriptorProto", [
            { no: 1, name: "name", kind: "scalar", opt: true, T: 9 },
            { no: 2, name: "value", kind: "message", repeat: 2, T: () => EnumValueDescriptorProtoType },
        ]);
    }
}
export const EnumDescriptorProtoType = new EnumDescriptorProto$Type();

class EnumValueDescriptorProto$Type extends MessageType<{ name?: string; number?: number }> {
    constructor() {
        super("google.protobuf.EnumValueDescriptorProto", [
            { no: 1, name: "name", kind: "scalar", opt: true, T: 9 },
            { no: 2, name: "number", kind: "scalar", opt: true, T: 5 },
        ]);
    }
}
export const EnumValueDescriptorProtoType = new EnumValueDescriptorProto$Type();

class MessageOptions$Type extends MessageType<MessageOptions> {
    constructor() {
        super("google.protobuf.MessageOptions", [
            { no: 7, name: "map_entry", kind: "scalar", opt: true, T: 8 },
        ]);
    }
}
export const MessageOptionsType = new MessageOptions$Type();
