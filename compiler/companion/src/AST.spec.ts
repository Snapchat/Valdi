import 'ts-jest';
import {
  AST,
  dumpInterface,
  dumpRootNodes,
  getImportStringLiteralsFromSourceFile,
  isNonModuleWithAmbiantDeclarations,
} from './AST';
import { findNode, getNodeComments } from './TSUtils';
import * as ts from 'typescript';
import { Project } from './project/Project';

interface CompiledFile {
  sourceFile: ts.SourceFile;
  typeChecker: ts.TypeChecker;
}

function compile(str: string): CompiledFile {
  const project = new Project('/', {});
  const sourceFile = project.createSourceFile('File.ts', str);
  return {
    sourceFile: sourceFile,
    typeChecker: project.typeChecker,
  };
}

describe('AST', () => {
  it('can find types to dump', () => {
    const result = compile(
      `class Component<VM> {

      }

      // @ExportProxy({ios: 'SCTCallGridParticipant', android: 'com.snap.talk.SCTCallGridParticipant'})
      interface Participant {
        displayName: string;
        color: string;
        hasVideo: boolean;
      }

      /* @ViewModel
      @ExportModel({ios: 'SCTCallGridViewModel', android: 'com.snap.talk.CallGridViewModel'}) */
      interface CallGridViewModel {
        participants: Participant[];
        foo: number;
        bar: string;
      }

      /**
       * @Component
       * @ExportModel({ios: 'SCTCallGrid, android: 'com.snap.talk.CallGrid'})
       */
      export class CallGrid extends Component<CallGridViewModel> {
        // @Action
        someAction() {

        }

        onRender() {
          <layout></layout>;
        }
      }
    `,
    );

    const dumpedNodes = dumpRootNodes(result.sourceFile, {
      typeChecker: result.typeChecker,
      references: [],
    });

    expect(dumpedNodes.length).toEqual(3);

    const firstMemberOfComponent = dumpedNodes[2]!.interface!.members[0];
    expect(firstMemberOfComponent.name).toEqual('someAction');
    expect(firstMemberOfComponent.leadingComments?.text).toContain('@Action');
  });

  // `dumpRootNodes` decides whether a file is an @ExportModule module by inspecting only
  // rootNodes[0] (the first top-level node). So the same @ExportModule annotation changes the
  // dumped output depending on where in the file it appears — a whole-file scan would not.
  // This pins that first-node-only behavior.
  it('only treats @ExportModule as file-wide when it leads the first root node', () => {
    const interfaceNames = (nodes: ReturnType<typeof dumpRootNodes>): string[] =>
      nodes.map((node) => node.interface?.name).filter((name): name is string => !!name);

    // @ExportModule leads the first root node -> the whole file is an export module, so the plain
    // exported interface (no annotation of its own) is dumped alongside the annotated one.
    const onFirst = compile(
      `
      /** @ExportModule */
      export interface Annotated {
        a: string;
      }

      export interface Plain {
        b: string;
      }
    `,
    );
    const namesOnFirst = interfaceNames(
      dumpRootNodes(onFirst.sourceFile, { typeChecker: onFirst.typeChecker, references: [] }),
    );
    expect(namesOnFirst).toContain('Annotated');
    expect(namesOnFirst).toContain('Plain');

    // Same declarations, but @ExportModule now leads the second root node. dumpRootNodes only
    // checks rootNodes[0], so it no longer treats the file as an export module and the plain
    // exported interface is dropped — purely because of where the annotation appears.
    const onSecond = compile(
      `
      export interface Plain {
        b: string;
      }

      /** @ExportModule */
      export interface Annotated {
        a: string;
      }
    `,
    );
    const namesOnSecond = interfaceNames(
      dumpRootNodes(onSecond.sourceFile, { typeChecker: onSecond.typeChecker, references: [] }),
    );
    expect(namesOnSecond).not.toContain('Plain');
  });

  it('can dump enums', () => {
    const result = compile(
      `
      // @ExportEnum
      enum MyEnum {
        // This is a String
        aString = 'Hello',
        /**
         * This is a number
         */
        aNumber = 42,
      }
    `,
    );

    const dumpedNodes = dumpRootNodes(result.sourceFile, {
      typeChecker: result.typeChecker,
      references: [],
    });

    expect(dumpedNodes.length).toEqual(1);

    const enumNode = dumpedNodes[0];
    expect(enumNode.leadingComments?.text).toContain('@ExportEnum');
    expect(enumNode.enum).toBeTruthy();
    expect(enumNode.enum!.name).toEqual('MyEnum');
    expect(enumNode.enum!.members?.length).toEqual(2);

    const firstEnumMember = enumNode.enum!.members[0]!;
    expect(firstEnumMember.name).toEqual('aString');
    expect(firstEnumMember.leadingComments?.text).toContain('This is a String');
    expect(firstEnumMember.stringValue).toEqual('Hello');

    const secondEnumMember = enumNode.enum!.members[1]!;
    expect(secondEnumMember.name).toEqual('aNumber');
    expect(secondEnumMember.numberValue).toContain('42');
    expect(secondEnumMember.leadingComments?.text).toContain('This is a number');
  });

  it('can dump simple interface', () => {
    const result = compile(
      `
    export interface GroupRecipient {
      recipientType: number;
      groupId: string;
      displayName?: string;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'GroupRecipient',
      start: 0,
      end: 119,
      typeParameters: undefined,
      members: [
        {
          name: 'recipientType',
          isOptional: false,
          leadingComments: undefined,
          start: 40,
          end: 62,
          type: { name: 'number' },
        },
        {
          name: 'groupId',
          isOptional: false,
          leadingComments: undefined,
          start: 69,
          end: 85,
          type: { name: 'string' },
        },
        {
          name: 'displayName',
          isOptional: true,
          leadingComments: undefined,
          start: 92,
          end: 113,
          type: { name: 'string' },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with simple arrays', () => {
    const result = compile(
      `
    export interface SomeInterface {
      someNumbers: number[];
      someStrings: string[];
      someOptionalNumbers?: number[];
      someOptionalStrings?: string[];
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'SomeInterface',
      start: 0,
      end: expect.anything(),
      typeParameters: undefined,
      members: [
        {
          name: 'someNumbers',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: {
              name: 'number',
            },
          },
        },
        {
          name: 'someStrings',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: {
              name: 'string',
            },
          },
        },
        {
          name: 'someOptionalNumbers',
          isOptional: true,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: { name: 'number' },
          },
        },
        {
          name: 'someOptionalStrings',
          isOptional: true,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: { name: 'string' },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with generic arrays', () => {
    const result = compile(
      `
    export interface SomeInterface {
      someNumbers: Array<number>;
      someStrings: Array<string>;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'SomeInterface',
      start: expect.anything(),
      end: expect.anything(),
      typeParameters: undefined,
      members: [
        {
          name: 'someNumbers',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: {
              name: 'number',
            },
          },
        },
        {
          name: 'someStrings',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'array',
            array: {
              name: 'string',
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);

    expect(references.references).toEqual([]);
  });

  it('can dump interface with functions', () => {
    const result = compile(
      `
    export interface MyInterface {
      aLambda: (param1: number) => string;
      aFunction(str: string): number;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 117,
      typeParameters: undefined,
      members: [
        {
          name: 'aLambda',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 73,
          type: {
            name: 'function',
            function: {
              parameters: [
                {
                  start: 47,
                  end: 61,
                  isOptional: false,
                  name: 'param1',
                  type: {
                    name: 'number',
                  },
                },
              ],
              returnValue: {
                name: 'string',
              },
            },
          },
        },
        {
          name: 'aFunction',
          isOptional: false,
          leadingComments: undefined,
          start: 80,
          end: 111,
          type: {
            name: 'function',
            function: {
              parameters: [
                {
                  name: 'str',
                  isOptional: false,
                  start: 90,
                  end: 101,
                  type: {
                    name: 'string',
                  },
                },
              ],
              returnValue: {
                name: 'number',
              },
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with nested lambdas', () => {
    const result = compile(
      `
    export interface MyInterface {
      aComplexLambda: (param1: (nested: number) => void, param2: (((() => void))),) => () => void;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 135,
      typeParameters: undefined,
      members: [
        {
          name: 'aComplexLambda',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 129,
          type: {
            name: 'function',
            function: {
              parameters: [
                {
                  start: 54,
                  end: 86,
                  isOptional: false,
                  name: 'param1',
                  type: {
                    name: 'function',
                    function: {
                      parameters: [
                        {
                          name: 'nested',
                          start: 63,
                          end: 77,
                          isOptional: false,
                          type: {
                            name: 'number',
                          },
                        },
                      ],
                      returnValue: {
                        name: 'void',
                      },
                    },
                  },
                },
                {
                  start: 88,
                  end: 112,
                  isOptional: false,
                  name: 'param2',
                  type: {
                    name: 'function',
                    function: {
                      parameters: [],
                      returnValue: {
                        name: 'void',
                      },
                    },
                  },
                },
              ],
              returnValue: {
                name: 'function',
                function: {
                  parameters: [],
                  returnValue: {
                    name: 'void',
                  },
                },
              },
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump comments on types', () => {
    const result = compile(
      `
    export interface MyInterface {
      fetch(cb: /* Parameter comment */ () => void): /* Return comment */ () => void;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 122,
      typeParameters: undefined,
      members: [
        {
          name: 'fetch',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 116,
          type: {
            name: 'function',
            function: {
              parameters: [
                {
                  start: 43,
                  end: 81,
                  isOptional: false,
                  name: 'cb',
                  type: {
                    name: 'function',
                    leadingComments: {
                      text: '/* Parameter comment */',
                      start: 47,
                      end: 70,
                    },
                    function: {
                      parameters: [],
                      returnValue: {
                        name: 'void',
                      },
                    },
                  },
                },
              ],
              returnValue: {
                name: 'function',
                leadingComments: { text: '/* Return comment */', start: 84, end: 104 },
                function: {
                  parameters: [],
                  returnValue: {
                    name: 'void',
                  },
                },
              },
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with lambda returning nullable type', () => {
    const result = compile(
      `
    export interface MyInterface {
      anOptionalLambda?: () => string | undefined;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 87,
      members: [
        {
          name: 'anOptionalLambda',
          isOptional: true,
          leadingComments: undefined,
          start: 37,
          end: 81,
          type: {
            name: 'function',
            function: {
              parameters: [],
              returnValue: {
                name: 'union',
                unions: [{ name: 'string' }, { name: 'undefined' }],
              },
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with union', () => {
    const result = compile(
      `
    export interface MyInterface {
      unionType: number | string | undefined;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 82,
      members: [
        {
          name: 'unionType',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 76,
          type: { name: 'union', unions: [{ name: 'number' }, { name: 'string' }, { name: 'undefined' }] },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with related types', () => {
    const result = compile(
      `
    export interface MyInterface {
      value: Interface2;
    }

    interface Interface2 {

    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 61,
      members: [
        {
          name: 'value',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 55,
          type: {
            name: 'reference',
            typeReferenceIndex: 0,
            isTypeParameter: false,
            typeArguments: undefined,
          },
        },
      ],
      typeParameters: undefined,
    };

    expect(output).toEqual(expectedOutput);

    const expectedFileName = '/File.ts';
    expect(references.references).toEqual([{ name: 'Interface2', fileName: expectedFileName }]);
  });

  it('can dump interface with related enum', () => {
    const result = compile(
      `
    export interface MyInterface {
      value: MyEnum;
    }

    const enum MyEnum {

    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 57,
      members: [
        {
          name: 'value',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 51,
          type: {
            name: 'reference',
            typeReferenceIndex: 0,
            isTypeParameter: false,
            typeArguments: undefined,
          },
        },
      ],
      typeParameters: undefined,
    };

    expect(output).toEqual(expectedOutput);

    const expectedFileName = '/File.ts';
    expect(references.references).toEqual([{ name: 'MyEnum', fileName: expectedFileName }]);
  });

  it('can dump interface with enum value', () => {
    const result = compile(
      `
    export interface MyInterface {
      value: MyEnum.HEY;
    }

    const enum MyEnum {
      HEY = 1,
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 61,
      members: [
        {
          name: 'value',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 55,
          type: {
            name: 'reference',
            typeReferenceIndex: 0,
            isTypeParameter: false,
            typeArguments: undefined,
          },
        },
      ],
      typeParameters: undefined,
    };

    expect(output).toEqual(expectedOutput);

    const expectedFileName = '/File.ts';
    expect(references.references).toEqual([{ name: 'MyEnum', fileName: expectedFileName }]);
  });

  it('can dump interface with null', () => {
    const result = compile(
      `
    export interface MyInterface {
      aFunction(str: string | null): number;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: 0,
      end: 81,
      members: [
        {
          name: 'aFunction',
          isOptional: false,
          leadingComments: undefined,
          start: 37,
          end: 75,
          type: {
            name: 'function',
            function: {
              parameters: [
                {
                  name: 'str',
                  isOptional: false,
                  start: 47,
                  end: 65,
                  type: {
                    name: 'union',
                    unions: [{ name: 'string' }, { name: 'null' }],
                  },
                },
              ],
              returnValue: {
                name: 'number',
              },
            },
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump interface with generic type parameters', () => {
    const result = compile(
      `
    export interface MyInterface<T> {
      value: T;
    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: expect.anything(),
      end: expect.anything(),
      members: [
        {
          name: 'value',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'reference',
            typeReferenceIndex: 0,
            isTypeParameter: true,
            typeArguments: undefined,
          },
        },
      ],
      typeParameters: [
        {
          start: expect.anything(),
          end: expect.anything(),
          name: 'T',
        },
      ],
    };

    expect(output).toEqual(expectedOutput);

    const expectedFileName = '/File.ts';
    expect(references.references).toEqual([
      {
        fileName: expectedFileName,
        name: 'T',
      },
    ]);
  });

  it('can dump interface with generic property', () => {
    const result = compile(
      `
    export interface MyInterface {
      value: GenericContainer<ReferenceInterface>;
    }

    interface GenericContainer<T> {
      boxedValue: T
    }

    interface ReferenceInterface {

    }
    `.trim(),
    );

    const interfaceNode = findNode(result.sourceFile, 0, (node) => ts.isInterfaceDeclaration(node));
    expect(interfaceNode).toBeDefined();

    const references: AST.TypeReferences = {
      typeChecker: result.typeChecker,
      references: [],
    };

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, references);

    const expectedOutput: AST.Interface = {
      name: 'MyInterface',
      start: expect.anything(),
      end: expect.anything(),
      members: [
        {
          name: 'value',
          isOptional: false,
          leadingComments: undefined,
          start: expect.anything(),
          end: expect.anything(),
          type: {
            name: 'reference',
            typeReferenceIndex: 1,
            isTypeParameter: false,
            typeArguments: [
              {
                type: {
                  name: 'reference',
                  isTypeParameter: false,
                  typeArguments: undefined,
                  typeReferenceIndex: 0,
                },
              },
            ],
          },
        },
      ],
      typeParameters: undefined,
    };

    expect(output).toEqual(expectedOutput);

    const expectedFileName = '/File.ts';
    expect(references.references).toEqual([
      {
        name: 'ReferenceInterface',
        fileName: expectedFileName,
      },
      {
        name: 'GenericContainer',
        fileName: expectedFileName,
      },
    ]);
  });

  it('can dump interface with super type', () => {
    const result = compile(
      `
    export interface Base {}
    export interface Parent extends Base {}
    `.trim(),
    );

    const interfaceNode = findNode(
      result.sourceFile,
      undefined,
      (node) => ts.isInterfaceDeclaration(node) && node.name.getText() === 'Parent',
    );
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'Parent',
      start: 29,
      end: 68,
      typeParameters: undefined,
      members: [],
      supertypes: [
        {
          end: 65,
          isImplements: false,
          start: 61,
          type: {
            isTypeParameter: false,
            leadingComments: undefined,
            name: 'reference',
            typeArguments: undefined,
            typeReferenceIndex: 0,
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can dump module', () => {
    const result = compile(
      `
// @ExportModule

 export function function1(): void;
 export const VARIABLE_2: number;
    `.trim(),
    );

    const dumpedNodes = dumpRootNodes(result.sourceFile, {
      typeChecker: result.typeChecker,
      references: [],
    });

    expect(dumpedNodes.length).toEqual(2);
    expect(dumpedNodes[0].function?.name).toBe('function1');
    expect(dumpedNodes[1].variable).toEqual({
      start: 55,
      end: 87,
      name: 'VARIABLE_2',
      type: {
        name: 'number',
      },
    });
  });

  it('can dump interface with complex type references', () => {
    const result = compile(
      `
    export interface ViewModelBase {
      propA: boolean;
    }
    export type ViewModel = ViewModelBase & {
      propB: string;
    };

    export interface Base<T> {}
    export interface Parent extends Base<ViewModel> {}
    `.trim(),
    );

    const interfaceNode = findNode(
      result.sourceFile,
      undefined,
      (node) => ts.isInterfaceDeclaration(node) && node.name.getText() === 'Parent',
    );
    expect(interfaceNode).toBeDefined();

    const output = dumpInterface(interfaceNode as ts.InterfaceDeclaration, {
      typeChecker: result.typeChecker,
      references: [],
    });

    const expectedOutput: AST.Interface = {
      name: 'Parent',
      start: 172,
      end: 222,
      typeParameters: undefined,
      members: [],
      supertypes: [
        {
          end: 219,
          isImplements: false,
          start: 204,
          type: {
            isTypeParameter: false,
            leadingComments: undefined,
            name: 'reference',
            typeArguments: [
              {
                type: {
                  name: 'intersection',
                  leadingComments: undefined,
                  intersections: [
                    {
                      isTypeParameter: false,
                      leadingComments: undefined,
                      name: 'reference',
                      typeReferenceIndex: 0,
                      typeArguments: undefined,
                    },
                    {
                      name: 'reference',
                      typeReferenceIndex: 1,
                      isTypeParameter: false,
                      leadingComments: undefined,
                      typeArguments: undefined,
                    },
                  ],
                },
              },
            ],
            typeReferenceIndex: 2,
          },
        },
      ],
    };

    expect(output).toEqual(expectedOutput);
  });

  it('can resolve import paths', () => {
    const result = compile(
      `
      import resources from './res';
      import { Number } from './Number';
      import 'ts-jest';

    `.trim(),
    );

    const imports = getImportStringLiteralsFromSourceFile(result.sourceFile)
      .map((st) => st.text)
      .sort();

    expect(imports).toEqual(['./Number', './res', 'ts-jest']);
  });

  it('can detect non module files with ambiant declaration', () => {
    let result = compile(`
    import resources from './res';
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeFalsy();

    result = compile(`
    import resources from './res';

    declare function myFn(): void {}
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeFalsy();

    result = compile(`
    import resources from './res';

    declare module "mod" {
      interface Test {}
    }
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeTruthy();

    result = compile(`
    import resources from './res';

    declare module "mod" {
      interface Test {}
    }

    export const VALUE = 42;
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeTruthy();

    result = compile(`
    import resources from './res';

    declare module "mod" {
      interface Test {}
    }

    export { resources };
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeTruthy();

    result = compile(`
    import resources from './res';

    declare module "mod" {
      interface Test {}
    }

    export = resources;
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeTruthy();

    result = compile(`
    interface MyInterface {}
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeTruthy();

    result = compile(`
    export interface MyInterface {}
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeFalsy();

    result = compile(`
    declare namespace moment {}
    export { moment };
    `);

    expect(isNonModuleWithAmbiantDeclarations(result.sourceFile)).toBeFalsy();
  });
});
