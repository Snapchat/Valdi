import {
  DescriptorProto,
  EnumDescriptorProto,
  FieldDescriptorProto,
  FieldDescriptorProto_Label as FieldLabel,
  FieldDescriptorProto_Type as FieldType,
  FileDescriptorProto,
  FileDescriptorSet,
  FileDescriptorSetType,
} from './descriptor';
import { FullyQualifiedName } from './FullyQualifiedName';
import {
  MessageType,
  ScalarType,
  RepeatType,
  PartialFieldInfo,
  IMessageType,
  EnumInfo,
} from '@protobuf-ts/runtime';

export interface RegisteredDescriptor {
  name: FullyQualifiedName;
  isEnum: boolean;
}

interface MapType {
  keyType: ScalarType;
  valueType: string;
  valueKind: 'scalar' | 'enum' | 'message';
  valueScalarType?: ScalarType;
}

interface PendingField {
  fieldInfo: PartialFieldInfo;
  typeName?: string; // For message/enum fields, the type name to resolve later
}

interface PendingMessageType {
  typeName: string;
  fields: PendingField[];
  messageType?: MessageType<any>;
}

export class DescriptorDatabase {
  private allDescriptors: RegisteredDescriptor[] = [];
  private messageTypes: Map<string, MessageType<any>> = new Map();
  private enumInfos: Map<string, EnumInfo> = new Map();
  private pendingMessageTypes: Map<string, PendingMessageType> = new Map();
  private mapTypeByName: Map<string, MapType> = new Map();

  constructor() {}

  /**
   * Resolves all pending message types by creating MessageType instances.
   * Must be called after all file descriptors have been added.
   */
  resolve(): void {
    // Create MessageType instances for all pending types
    for (const [typeName, pending] of this.pendingMessageTypes) {
      if (!pending.messageType) {
        this.createMessageType(typeName, pending);
      }
    }
    this.pendingMessageTypes.clear();
  }

  private createMessageType(typeName: string, pending: PendingMessageType): MessageType<any> {
    // Check if already created (to handle circular references)
    if (pending.messageType) {
      return pending.messageType;
    }

    // Resolve fields
    const resolvedFields: PartialFieldInfo[] = pending.fields.map(pf => {
      const info = pf.fieldInfo as any;
      
      if (info.kind === 'message' && pf.typeName) {
        // Resolve message type reference
        info.T = () => this.getMessageType(pf.typeName!);
      } else if (info.kind === 'enum' && pf.typeName) {
        // Resolve enum type reference
        info.T = () => this.getEnumInfo(pf.typeName!);
      } else if (info.kind === 'map' && pf.typeName) {
        // Resolve map value type if it's a message or enum
        const mapType = this.mapTypeByName.get(pf.typeName);
        if (mapType && info.V) {
          if (info.V.kind === 'message') {
            info.V.T = () => this.getMessageType(mapType.valueType);
          } else if (info.V.kind === 'enum') {
            info.V.T = () => this.getEnumInfo(mapType.valueType);
          }
        }
      }
      
      return info as PartialFieldInfo;
    });

    // Create the MessageType
    const messageType = new MessageType<any>(typeName, resolvedFields);
    pending.messageType = messageType;
    this.messageTypes.set(typeName, messageType);
    
    return messageType;
  }

  getMessageType(typeName: string): IMessageType<any> {
    const existing = this.messageTypes.get(typeName);
    if (existing) {
      return existing;
    }

    // Check if it's pending
    const pending = this.pendingMessageTypes.get(typeName);
    if (pending) {
      return this.createMessageType(typeName, pending);
    }

    throw new Error(`Unknown message type: ${typeName}`);
  }

  getEnumInfo(typeName: string): EnumInfo {
    const info = this.enumInfos.get(typeName);
    if (!info) {
      throw new Error(`Unknown enum type: ${typeName}`);
    }
    return info;
  }

  getAllTypeNames(): FullyQualifiedName[] {
    return this.allDescriptors.map(d => d.name);
  }

  getAllMessageDescriptorTypeNames(): FullyQualifiedName[] {
    return this.allDescriptors.filter(d => !d.isEnum).map(d => d.name);
  }

  getAllDescriptors(): readonly RegisteredDescriptor[] {
    return this.allDescriptors;
  }

  addFileDescriptor(fileDescriptor: FileDescriptorProto): void {
    const packageName = fileDescriptor.package;
    const currentPackage = packageName ? FullyQualifiedName.fromString(packageName) : undefined;

    if (fileDescriptor.messageType) {
      for (const messageType of fileDescriptor.messageType) {
        this.addDescriptor(currentPackage, messageType);
      }
    }

    if (fileDescriptor.enumType) {
      for (const enumType of fileDescriptor.enumType) {
        this.addEnumDescriptor(currentPackage, enumType);
      }
    }
  }

  private toTypeNameString(typeName: string | undefined): string {
    if (!typeName) {
      throw new Error('Expected type name');
    }
    return typeName.startsWith('.') ? typeName.substring(1) : typeName;
  }

  private fieldTypeToScalarType(fieldType: FieldType): ScalarType | undefined {
    switch (fieldType) {
      case FieldType.DOUBLE:
        return ScalarType.DOUBLE;
      case FieldType.FLOAT:
        return ScalarType.FLOAT;
      case FieldType.INT64:
        return ScalarType.INT64;
      case FieldType.UINT64:
        return ScalarType.UINT64;
      case FieldType.INT32:
        return ScalarType.INT32;
      case FieldType.FIXED64:
        return ScalarType.FIXED64;
      case FieldType.FIXED32:
        return ScalarType.FIXED32;
      case FieldType.BOOL:
        return ScalarType.BOOL;
      case FieldType.STRING:
        return ScalarType.STRING;
      case FieldType.BYTES:
        return ScalarType.BYTES;
      case FieldType.UINT32:
        return ScalarType.UINT32;
      case FieldType.SFIXED32:
        return ScalarType.SFIXED32;
      case FieldType.SFIXED64:
        return ScalarType.SFIXED64;
      case FieldType.SINT32:
        return ScalarType.SINT32;
      case FieldType.SINT64:
        return ScalarType.SINT64;
      default:
        return undefined; // MESSAGE, ENUM, GROUP
    }
  }

  private toFieldInfo(fieldDescriptor: FieldDescriptorProto): PendingField {
    const isRepeated = fieldDescriptor.label === FieldLabel.REPEATED;
    const fieldType = fieldDescriptor.type ?? FieldType.UNSPECIFIED$;
    const scalarType = this.fieldTypeToScalarType(fieldType);

    if (scalarType !== undefined) {
      // Scalar field
      return {
        fieldInfo: {
          kind: 'scalar',
          no: fieldDescriptor.number,
          name: fieldDescriptor.name,
          T: scalarType,
          repeat: isRepeated ? RepeatType.PACKED : RepeatType.NO,
        } as PartialFieldInfo,
      };
    } else if (fieldType === FieldType.MESSAGE) {
      const typeName = this.toTypeNameString(fieldDescriptor.typeName);
      
      // Check if this is a map field
      const mapType = this.mapTypeByName.get(typeName);
      if (mapType) {
        // Map field
        const mapValue: any = mapType.valueKind === 'scalar'
          ? { kind: 'scalar', T: mapType.valueScalarType! }
          : mapType.valueKind === 'message'
            ? { kind: 'message', T: () => this.getMessageType(mapType.valueType) }
            : { kind: 'enum', T: () => this.getEnumInfo(mapType.valueType) };

        return {
          fieldInfo: {
            kind: 'map',
            no: fieldDescriptor.number,
            name: fieldDescriptor.name,
            K: mapType.keyType,
            V: mapValue,
          } as PartialFieldInfo,
          typeName,
        };
      }

      // Regular message field
      return {
        fieldInfo: {
          kind: 'message',
          no: fieldDescriptor.number,
          name: fieldDescriptor.name,
          T: () => { throw new Error('Not resolved yet'); }, // Placeholder
          repeat: isRepeated ? RepeatType.UNPACKED : RepeatType.NO,
        } as PartialFieldInfo,
        typeName,
      };
    } else if (fieldType === FieldType.ENUM) {
      const typeName = this.toTypeNameString(fieldDescriptor.typeName);
      return {
        fieldInfo: {
          kind: 'enum',
          no: fieldDescriptor.number,
          name: fieldDescriptor.name,
          T: () => { throw new Error('Not resolved yet'); }, // Placeholder
          repeat: isRepeated ? RepeatType.PACKED : RepeatType.NO,
        } as PartialFieldInfo,
        typeName,
      };
    } else if (fieldType === FieldType.GROUP) {
      throw new Error('Groups are not supported');
    }

    throw new Error(`Unknown field type: ${fieldType}`);
  }

  private createMapType(descriptorProto: DescriptorProto): MapType {
    let keyType: ScalarType | undefined;
    let valueType: string | undefined;
    let valueKind: 'scalar' | 'enum' | 'message' = 'scalar';
    let valueScalarType: ScalarType | undefined;

    if (descriptorProto.field) {
      for (const field of descriptorProto.field) {
        const fType = field.type ?? FieldType.UNSPECIFIED$;
        if (field.name === 'key') {
          keyType = this.fieldTypeToScalarType(fType);
        } else if (field.name === 'value') {
          valueScalarType = this.fieldTypeToScalarType(fType);
          if (valueScalarType !== undefined) {
            valueKind = 'scalar';
            valueType = '';
          } else if (fType === FieldType.MESSAGE) {
            valueKind = 'message';
            valueType = this.toTypeNameString(field.typeName);
          } else if (fType === FieldType.ENUM) {
            valueKind = 'enum';
            valueType = this.toTypeNameString(field.typeName);
          }
        }
      }
    }

    if (keyType === undefined || valueType === undefined) {
      throw new Error(`Could not resolve map type in: ${JSON.stringify(descriptorProto, null, 2)}`);
    }

    return { keyType, valueType, valueKind, valueScalarType };
  }

  private rebuildFullyQualifiedNameFromFull(fullName: string): FullyQualifiedName {
    const parts = fullName.split('.');
    let parent: FullyQualifiedName | undefined = undefined;
    let full = '';

    for (let i = 0; i < parts.length - 1; i++) {
      full = i === 0 ? parts[i] : `${full}.${parts[i]}`;
      parent = {
        symbolName: parts[i],
        fullName: full,
        parent: parent,
      };
    }

    return {
      symbolName: parts[parts.length - 1],
      fullName,
      parent,
    };
  }

  private addDescriptor(
    parentPackage: FullyQualifiedName | undefined,
    descriptorProto: DescriptorProto,
  ): void {
    const name = descriptorProto.name ?? '';
    if (!name) return; // Skip descriptors without names
    const childPackage = new FullyQualifiedName(parentPackage, name);
    const typeName = childPackage.fullName;

    // Check if this is a map entry type
    if (descriptorProto.options?.mapEntry) {
      const mapType = this.createMapType(descriptorProto);
      this.mapTypeByName.set(typeName, mapType);
      return;
    }

    // Process nested types FIRST so map entries are registered before we process fields
    if (descriptorProto.nestedType) {
      for (const nestedType of descriptorProto.nestedType) {
        this.addDescriptor(childPackage, nestedType);
      }
    }

    // Now collect fields (after nested types are registered)
    const pendingFields: PendingField[] = [];
    if (descriptorProto.field) {
      for (const fieldDescriptor of descriptorProto.field) {
        pendingFields.push(this.toFieldInfo(fieldDescriptor));
      }
    }

    // Register as pending
    this.pendingMessageTypes.set(typeName, {
      typeName,
      fields: pendingFields,
    });

    const fixedFQN = this.rebuildFullyQualifiedNameFromFull(typeName);
    this.allDescriptors.push({ name: fixedFQN, isEnum: false });

    if (descriptorProto.enumType) {
      for (const nestedEnum of descriptorProto.enumType) {
        this.addEnumDescriptor(childPackage, nestedEnum);
      }
    }
  }

  private addEnumDescriptor(
    parentPackage: FullyQualifiedName | undefined,
    enumDescriptor: EnumDescriptorProto,
  ): void {
    const name = enumDescriptor.name ?? '';
    if (!name) return; // Skip enum descriptors without names
    const values = enumDescriptor.value;
    const childPackage = new FullyQualifiedName(parentPackage, name);
    const typeName = childPackage.fullName;

    // Create enum object (bidirectional mapping)
    const enumObj: { [key: number]: string; [k: string]: number | string } = {};
    for (const value of values) {
      const valueName = value.name ?? '';
      const valueNumber = value.number ?? 0;
      if (!valueName) continue;
      enumObj[valueName] = valueNumber;
      enumObj[valueNumber] = valueName;
    }

    // Create EnumInfo tuple: [typeName, enumObject]
    const enumInfo: EnumInfo = [typeName, enumObj];
    this.enumInfos.set(typeName, enumInfo);

    this.allDescriptors.push({ name: childPackage, isEnum: true });
  }

  /**
   * Validates that a string contains only valid package name characters.
   */
  private isValidPackageName(name: string): boolean {
    return /^[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z_][a-zA-Z0-9_]*)*$/.test(name);
  }

  getDescriptorSetFromBuffer(buffer: Uint8Array): Uint8Array {
    if (buffer.length >= 8) {
      const tagBytes = buffer.subarray(0, 8);
      const tag = String.fromCharCode(
        tagBytes[0], tagBytes[1], tagBytes[2], tagBytes[3],
        tagBytes[4], tagBytes[5], tagBytes[6], tagBytes[7]
      );
      if (tag === 'VALDIPRO') {
        const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        const indexSize = view.getUint32(8, true);
        return buffer.subarray(12 + indexSize);
      }
    }
    return buffer;
  }

  addFileDescriptorSet(buffer: Uint8Array): void {
    const fileDescriptorSet = FileDescriptorSetType.fromBinary(this.getDescriptorSetFromBuffer(buffer));

    if (fileDescriptorSet.file) {
      for (const file of fileDescriptorSet.file) {
        if (file.package && !this.isValidPackageName(file.package)) {
          continue;
        }
        if (file.name && /[\x00-\x1F]/.test(file.name)) {
          continue;
        }
        this.addFileDescriptor(file);
      }
    }
  }
}
