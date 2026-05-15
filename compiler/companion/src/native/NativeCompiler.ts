import * as ts from 'typescript';
import * as NativeHelper from './NativeHelper';
import { NativeCompilerModuleBuilder } from './builder/NativeCompilerBuilder';
import {
  INativeCompilerBlockBuilder,
  INativeCompilerJumpTargetBuilder,
  IVariableDelegate,
  NativeCompilerBuilderAtomID,
  NativeCompilerBuilderBranchType,
  NativeCompilerBuilderJumpTargetID,
  NativeCompilerBuilderVariableID,
  NativeCompilerBuilderVariableRef,
  NativeCompilerBuilderVariableType,
  VariableRefScope,
} from './builder/INativeCompilerBuilder';
import { CompilerNativeCEmitter } from './emitter/CompilerNativeCEmitter';
import { emitterWalk, NativeCompilerEmitterOutput } from './emitter/INativeCompilerEmitter';
import { NativeCompilerIR } from './builder/NativeCompilerBuilderIR';
import { NamePath } from './utils/NamePath';
import { AssignmentTracker } from './utils/AssignmentTracker';
import { NativeCompilerOptions } from './NativeCompilerOptions';
import { Lazy } from '../utils/Lazy';
import { isSuperCall, statementHasSuperCall, getSuperCallFromStatement } from '../TSUtils';
import { NameAllocator } from './utils/NameAllocator';
import { EmitResolver, getEmitResolver } from '../utils/EmitResolver';

interface NativeCompilerVariableIDWithParent {
  variable: NativeCompilerBuilderVariableID;
  parentVariable: NativeCompilerBuilderVariableID | undefined;
}

interface NativeCompilerVariableIDValueDelegate {
  getValue: () => NativeCompilerVariableIDWithParent;
  setValue: (value: NativeCompilerBuilderVariableID) => void;
}

interface BindingElementVariable {
  propertyName: NativeCompilerBuilderAtomID | undefined;
  variableRef: NativeCompilerBuilderVariableRef;
  resolveValue?: (input: NativeCompilerBuilderVariableID) => NativeCompilerBuilderVariableID;
  isRest: boolean;
}

interface AssignmentElementVariable {
  variable: NativeCompilerBuilderVariableID;
  defaultValue?: (input: NativeCompilerBuilderVariableID) => NativeCompilerBuilderVariableID;
  isRest: boolean;
}

interface CompilationPlan {
  functionDeclarationInsertionIndex: number;
  readonly statementsToProcess: ts.Statement[];
}

function makeValidModuleName(name: string) {
  if (name.match(/^[^a-z_]/gi) !== null) {
    name += '_' + name;
  }
  return name.replace(/[^a-z0-9_]/gi, '_');
}

const CONSTRUCTOR_NAME = '__ctor__';

class NativeCompilerContext {
  constructor(
    readonly namePath: NamePath,
    readonly assignmentTracker: AssignmentTracker | undefined,
    readonly emitResolver: EmitResolver | undefined,
    readonly isGenerator: boolean,
    readonly isAsync: boolean,
  ) {}

  optionalChainExit?: NativeCompilerBuilderJumpTargetID = undefined;
  optionalChainResult?: NativeCompilerBuilderVariableID = undefined;

  withNamePath(namePath: NamePath, isGenerator?: boolean, isAsync?: boolean): NativeCompilerContext {
    if (isGenerator === undefined) {
      isGenerator = this.isGenerator;
    }
    if (isAsync === undefined) {
      isAsync = this.isAsync;
    }
    if (namePath === this.namePath && isGenerator === this.isGenerator && isAsync === this.isAsync) {
      return this;
    }
    return new NativeCompilerContext(namePath, this.assignmentTracker, this.emitResolver, isGenerator, isAsync);
  }

  withAssignmentTracker(assignmentTracker: AssignmentTracker | undefined): NativeCompilerContext {
    if (assignmentTracker === this.assignmentTracker) {
      return this;
    }

    return new NativeCompilerContext(
      this.namePath,
      assignmentTracker,
      this.emitResolver,
      this.isGenerator,
      this.isAsync,
    );
  }
}

const MAX_DEBUG_INFO_LENGTH = 50;

type DeferredStatementCb = (context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => void;
type EnqueueDeferredStatementFunc = (cb: DeferredStatementCb) => void;

export class NativeCompiler {
  private moduleName: string;
  private moduleBuilder: NativeCompilerModuleBuilder;
  private allocatedFunctionNames = new NameAllocator();
  private allocatedVariableRefNames = new NameAllocator();
  private moduleVariableNameByPosition: { [key: number]: string } = {};
  private loadedModules = new Set<string>();
  private lastProcessingDeclaration: ts.Node | undefined = undefined;

  constructor(
    readonly sourceFile: ts.SourceFile,
    readonly filePath: string,
    readonly modulePath: string,
    readonly typeChecker: ts.TypeChecker,
    readonly options: NativeCompilerOptions,
  ) {
    this.moduleName = makeValidModuleName(modulePath);
    this.moduleBuilder = new NativeCompilerModuleBuilder(this.moduleName, modulePath);
  }

  private allocateFunctionName(nameInitial: string): string {
    return this.allocatedFunctionNames.allocate(nameInitial);
  }

  private getFunctionName(namePath: NamePath): string {
    return this.allocateFunctionName(namePath.toString());
  }

  private allocateUniqueVariableRefName(name: string): string {
    return this.allocatedVariableRefNames.allocate(name);
  }

  private onError(node: ts.Node, message: string): never {
    NativeHelper.onError(this.filePath, this.sourceFile, node, message);
  }

  private processBindingElement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.BindingElement,
    includePropertyName: boolean,
  ): BindingElementVariable {
    if (ts.isIdentifier(node.name)) {
      const isRest = !!node.dotDotDotToken;
      const variableName = node.name;
      let propertyName: NativeCompilerBuilderAtomID | undefined;
      if (includePropertyName) {
        if (node.propertyName) {
          const resolvedPropertyName = this.processPropertyName(context, builder, node.propertyName);
          if (!(resolvedPropertyName instanceof NativeCompilerBuilderAtomID)) {
            this.onError(node, 'alias in binding element can only be identifiers');
          }
          propertyName = resolvedPropertyName;
        } else {
          propertyName = this.processIdentifierAsAtom(builder, variableName);
        }
      }

      let resolveValue: ((input: NativeCompilerBuilderVariableID) => NativeCompilerBuilderVariableID) | undefined;

      if (node.initializer) {
        const initializer = node.initializer;
        resolveValue = (input: NativeCompilerBuilderVariableID) => {
          return this.processDeclarationWithDefaultInitializer(context, builder, initializer, input);
        };
      }

      const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, variableName);

      return { propertyName, variableRef, resolveValue, isRest };
    } else {
      this.onError(node.name, 'Unsupported destructuring binding');
    }
  }

  private processBindingName(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    name: ts.BindingName,
    declaration: ts.Declaration | undefined,
    buildInitialValue: (context: NativeCompilerContext) => NativeCompilerBuilderVariableID,
  ): void {
    if (ts.isIdentifier(name)) {
      const variableName = name.text;
      const newNamePath = context.namePath.appending(variableName);

      const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, name);

      const initializerVariable = buildInitialValue(context.withNamePath(newNamePath));
      if (initializerVariable) {
        builder.buildSetVariableRef(variableRef, initializerVariable);
      }
      if (declaration) {
        this.appendExportedDeclarationIfNeeded(context, builder, declaration, name, initializerVariable);
      }
    } else if (ts.isObjectBindingPattern(name)) {
      const objectBindingPattern: ts.ObjectBindingPattern = name;
      const bindingElements: BindingElementVariable[] = [];

      const initializerVariable = buildInitialValue(context.withNamePath(context.namePath.appending('__destruct__')));

      for (const element of objectBindingPattern.elements) {
        if (ts.isIdentifier(element.name)) {
          bindingElements.push(this.processBindingElement(context, builder, element, true));
        } else if (element.propertyName && ts.isIdentifier(element.propertyName)) {
          this.processBindingName(context, builder, element.name, undefined, (ctx) => {
            return builder.buildGetProperty(
              initializerVariable,
              this.processIdentifierAsAtom(builder, element.propertyName! as ts.Identifier),
              initializerVariable,
            );
          });
        }
      }

      for (const bindingElement of bindingElements) {
        let propertyValue: NativeCompilerBuilderVariableID;
        if (bindingElement.isRest) {
          // Gather referenced properties that shouldn't be included
          const propertiesToIgnore: NativeCompilerBuilderAtomID[] = [];
          for (const otherBindingElement of bindingElements) {
            if (otherBindingElement === bindingElement) {
              break;
            }
            propertiesToIgnore.push(otherBindingElement.propertyName!);
          }

          propertyValue = builder.buildNewObject();
          builder.buildCopyPropertiesFrom(propertyValue, initializerVariable, propertiesToIgnore);
        } else {
          propertyValue = builder.buildGetProperty(
            initializerVariable,
            bindingElement.propertyName!,
            initializerVariable,
          );
        }

        if (bindingElement.resolveValue) {
          builder.buildSetVariableRef(bindingElement.variableRef, bindingElement.resolveValue(propertyValue));
        } else {
          builder.buildSetVariableRef(bindingElement.variableRef, propertyValue);
        }
      }
    } else if (ts.isArrayBindingPattern(name)) {
      const arrayBindingPattern: ts.ArrayBindingPattern = name;
      const allVariableRefs: [number, BindingElementVariable][] = [];

      const initializerVariable = buildInitialValue(context.withNamePath(context.namePath.appending('__destruct__')));

      let elementIndex = 0;
      for (const element of arrayBindingPattern.elements) {
        const currentElementIndex = elementIndex++;

        if (ts.isOmittedExpression(element)) {
          continue;
        }

        if (ts.isArrayBindingPattern(element.name) || ts.isObjectBindingPattern(element.name)) {
          const isRest = !!element.dotDotDotToken;
          if (isRest) {
            this.processBindingName(context, builder, element.name, undefined, (ctx) => {
              const sliceMethod = builder.buildGetProperty(
                initializerVariable,
                this.processIdentifierStringAsAtom(builder, 'slice'),
                initializerVariable,
              );
              return builder.buildFunctionInvocation(
                sliceMethod,
                [builder.buildLiteralInteger(currentElementIndex.toString())],
                initializerVariable,
              );
            });
          } else {
            this.processBindingName(context, builder, element.name, undefined, (ctx) => {
              return builder.buildGetPropertyValue(
                initializerVariable,
                builder.buildLiteralInteger(currentElementIndex.toString()),
                initializerVariable,
              );
            });
          }
        } else {
          const variableRef = this.processBindingElement(context, builder, element, false);
          allVariableRefs.push([currentElementIndex, variableRef]);
        }
      }

      for (const [elementIndex, bindingElement] of allVariableRefs) {
        let propertyValue: NativeCompilerBuilderVariableID;

        if (bindingElement.isRest) {
          // Call object.slice(elementIndex)
          const sliceMethod = builder.buildGetProperty(
            initializerVariable,
            this.processIdentifierStringAsAtom(builder, 'slice'),
            initializerVariable,
          );

          propertyValue = builder.buildFunctionInvocation(
            sliceMethod,
            [builder.buildLiteralInteger(elementIndex.toString())],
            initializerVariable,
          );
        } else {
          propertyValue = builder.buildGetPropertyValue(
            initializerVariable,
            builder.buildLiteralInteger(elementIndex.toString()),
            initializerVariable,
          );
        }

        if (bindingElement.resolveValue) {
          builder.buildSetVariableRef(bindingElement.variableRef, bindingElement.resolveValue(propertyValue));
        } else {
          builder.buildSetVariableRef(bindingElement.variableRef, propertyValue);
        }
      }
    } else {
      this.onError(name, 'Unsupported variable declaration type');
    }
  }

  private processVariableDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.VariableDeclaration,
    buildInitialValue: (context: NativeCompilerContext) => NativeCompilerBuilderVariableID,
  ): void {
    this.appendNodeDebugInfo(builder, node);

    this.processBindingName(context, builder, node.name, node, buildInitialValue);
  }

  private registerAndGetStringLiteralValue(
    builder: INativeCompilerBlockBuilder,
    text: string,
  ): NativeCompilerBuilderVariableID {
    const constantIndex = this.moduleBuilder.registerStringConstant(text);
    return builder.buildGetModuleConst(constantIndex);
  }

  private processStringLiteral(
    builder: INativeCompilerBlockBuilder,
    node: ts.StringLiteral,
  ): NativeCompilerBuilderVariableID {
    return this.registerAndGetStringLiteralValue(builder, node.text);
  }

  private processNumericalLiteral(
    builder: INativeCompilerBlockBuilder,
    node: ts.NumericLiteral,
  ): NativeCompilerBuilderVariableID {
    const numString = node.text;
    const numericalStringType = NativeHelper.getNumericalStringType(numString);

    switch (numericalStringType) {
      case NativeHelper.NumberType.Integer:
        return builder.buildLiteralInteger(numString);
      case NativeHelper.NumberType.Long:
        return builder.buildLiteralLong(numString);
      case NativeHelper.NumberType.Float:
        return builder.buildLiteralDouble(numString);
    }
  }

  private processPropertyAccessExpressionForCallExpressionEpilogue(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    expressionVariable: NativeCompilerBuilderVariableID,
    nameVariable: NativeCompilerBuilderAtomID,
    variableToUseAsThis: NativeCompilerBuilderVariableID,
    buildCall: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
      callee: NativeCompilerVariableIDWithParent,
    ) => NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    const resolvedVariable = builder.buildGetProperty(expressionVariable, nameVariable, variableToUseAsThis);
    const callee: NativeCompilerVariableIDWithParent = {
      variable: resolvedVariable,
      parentVariable: variableToUseAsThis,
    };
    return buildCall(context, builder, callee);
  }

  processPropertyAccessExpressionForCallExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAccessExpression,
    buildCall: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
      callee: NativeCompilerVariableIDWithParent,
    ) => NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    return this.doProcessPropertyAccessExpression(
      context,
      builder,
      node,
      (context, builder, expressionVariable, nameVariable) => {
        const variableToUseAsThis = this.resolveVariableToUseAsThis(context, builder, expressionVariable, node);
        return this.processPropertyAccessExpressionForCallExpressionEpilogue(
          context,
          builder,
          expressionVariable,
          nameVariable,
          variableToUseAsThis,
          buildCall,
        );
      },
    );
  }

  private resolveVariableToUseAsThis(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    variableID: NativeCompilerBuilderVariableID,
    node: ts.Node,
  ): NativeCompilerBuilderVariableID {
    if ((variableID.type & NativeCompilerBuilderVariableType.Super) != 0) {
      // The variable is a 'super' reference. The "thisObject" needs to be
      // the "this"
      return this.processThis(context, builder, node);
    }
    return variableID;
  }

  processPropertyAccessExpressionAsAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    propertyName: ts.PropertyName,
    expression: ts.Expression,
    variableID: NativeCompilerBuilderVariableID,
  ) {
    const nameVariable = this.processPropertyName(context, builder, propertyName);

    const newNamePath = context.namePath.appendingTSNode(propertyName);

    const expressionVariable = this.processExpression(context.withNamePath(newNamePath), builder, expression);

    this.doBuildSetProperty(context, builder, nameVariable, expressionVariable, variableID);
  }

  processElementAccessExpressionAsAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ElementAccessExpression,
    variableID: NativeCompilerBuilderVariableID,
  ) {
    const expressionVariable = this.processExpression(context, builder, node.expression);

    if (ts.isNumericLiteral(node.argumentExpression)) {
      const index = Number.parseInt(node.argumentExpression.text);
      builder.buildSetPropertyIndex(expressionVariable, index, variableID);
    } else {
      const nameVariable = this.processExpression(context, builder, node.argumentExpression);

      builder.buildSetPropertyValue(expressionVariable, nameVariable, variableID);
    }
  }

  private buildQuestionDotExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    expressionVariable: NativeCompilerBuilderVariableID,
    onTrue: (context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    const cond = builder.buildJumpTarget('value_condition');
    const condTrue = builder.buildJumpTarget('value_true');
    const condFalse = builder.buildJumpTarget('value_false');
    const condExit = builder.buildJumpTarget('value_exit');

    // use whole expression result and exit target if this is part of an optional chain
    const result: NativeCompilerBuilderVariableID = context.optionalChainResult ?? builder.buildAssignableUndefined();
    const exitTarget = context.optionalChainExit ?? condExit.target;

    const trueValue = onTrue(context, condTrue.builder);
    condTrue.builder.buildAssignment(result, trueValue);
    condTrue.builder.buildJump(condExit.target);

    condFalse.builder.buildAssignment(result, condFalse.builder.buildUndefined());
    condFalse.builder.buildJump(exitTarget);

    cond.builder.buildBranch(
      expressionVariable,
      NativeCompilerBuilderBranchType.NotUndefinedOrNull,
      condTrue.target,
      condFalse.target,
    );

    return result;
  }

  private doProcessPropertyAccessExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAccessExpression,
    doBuild: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
      expressionVariable: NativeCompilerBuilderVariableID,
      nameVariable: NativeCompilerBuilderAtomID,
    ) => NativeCompilerBuilderVariableID,
  ) {
    let nameVariable: NativeCompilerBuilderAtomID;
    if (ts.isIdentifier(node.name)) {
      nameVariable = this.processIdentifierAsAtom(builder, node.name);
    } else {
      this.onError(node.name, 'expecting identifier');
    }

    const newNamePath = context.namePath.appending(nameVariable.identifier);
    const parentExpressionContext = context.withNamePath(newNamePath);

    if (ts.isOptionalChain(node)) {
      // if this is part of an optional chain, share the whole expression's result
      const result = context.optionalChainResult ?? builder.buildAssignableUndefined();
      if (context.optionalChainExit) {
        // parent expression share the whole expression's exit and result
        parentExpressionContext.optionalChainExit = context.optionalChainExit;
        parentExpressionContext.optionalChainResult = context.optionalChainResult;
      } else {
        // start of a new optional chain, create the short circuit jump target
        const optionalChain = builder.buildJumpTarget('optional_chain');
        const optionalChainExit = builder.buildJumpTarget('optional_chain_exit');
        parentExpressionContext.optionalChainExit = optionalChainExit.target;
        parentExpressionContext.optionalChainResult = result;
        builder = optionalChain.builder;
      }
      // evaluate the parent expression with chain result and exit set
      const expressionVariable = this.processExpression(parentExpressionContext, builder, node.expression);
      if (context.assignmentTracker) {
        context.assignmentTracker.onGetProperty(expressionVariable, nameVariable);
      }
      if (node.questionDotToken) {
        builder.buildAssignment(
          result,
          this.buildQuestionDotExpression(context, builder, expressionVariable, (context, builder) => {
            return doBuild(context, builder, expressionVariable, nameVariable);
          }),
        );
        return result;
      } else {
        // this expression is not optional but it's part of an optional chain
        builder.buildAssignment(result, doBuild(context, builder, expressionVariable, nameVariable));
        return result;
      }
    } else {
      // not in optional chain
      const expressionVariable = this.processExpression(parentExpressionContext, builder, node.expression);
      if (context.assignmentTracker) {
        context.assignmentTracker.onGetProperty(expressionVariable, nameVariable);
      }
      return doBuild(context, builder, expressionVariable, nameVariable);
    }
  }

  private tryProcessEnumConstantValuePrologue(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAccessExpression | ts.ElementAccessExpression | ts.EnumMember,
  ): string | number | undefined {
    if (!context.emitResolver) {
      return undefined;
    }
    return context.emitResolver.getConstantValue(node);
  }

  private processEnumConstantValueEpilogue(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    value: string | number,
  ): NativeCompilerBuilderVariableID {
    if (typeof value === 'string') {
      return builder.buildLiteralString(value);
    } else {
      const numericalStringType = NativeHelper.getNumericalStringTypeFromNumber(value);
      switch (numericalStringType) {
        case NativeHelper.NumberType.Integer:
          return builder.buildLiteralInteger(value.toString());
        case NativeHelper.NumberType.Long:
          return builder.buildLiteralLong(value.toString());
        case NativeHelper.NumberType.Float:
          return builder.buildLiteralDouble(value.toString());
      }
    }
  }

  private tryProcessEnumConstantValue(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAccessExpression | ts.ElementAccessExpression | ts.EnumMember,
  ): NativeCompilerBuilderVariableID | undefined {
    const value = this.tryProcessEnumConstantValuePrologue(context, builder, node);
    if (value === undefined) {
      return undefined;
    }

    return this.processEnumConstantValueEpilogue(context, builder, value);
  }

  processPropertyAccessExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAccessExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    // try shortcut builtin constants (e.g. Math.PI)
    if (this.options.enableIntrinsics && ts.isIdentifier(node.expression)) {
      const intrinsicConstant = builder.getIntrinsicConstant(`${node.expression.text}.${node.name.text}`);
      if (typeof intrinsicConstant === 'number') {
        return builder.buildLiteralDouble(String(intrinsicConstant));
      } else if (typeof intrinsicConstant === 'string') {
        return builder.buildLiteralString(intrinsicConstant);
      }
    }

    const constantValue = this.tryProcessEnumConstantValue(context, builder, node);
    if (constantValue !== undefined) {
      return constantValue;
    }

    return this.doProcessPropertyAccessExpression(
      context,
      builder,
      node,
      (context, builder, expressionVariable, nameVariable) => {
        return builder.buildGetProperty(
          expressionVariable,
          nameVariable,
          this.resolveVariableToUseAsThis(context, builder, expressionVariable, node),
        );
      },
    );
  }

  processVariableDeclarationList(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.VariableDeclarationList,
  ) {
    node.declarations.forEach((d) => {
      this.processVariableDeclaration(context, builder, d, (context) => {
        const initializer = d.initializer;
        if (initializer) {
          return this.processExpression(context, builder, initializer);
        } else {
          return builder.buildUndefined();
        }
      });
    });
  }

  processVariableStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.VariableStatement,
  ) {
    this.processVariableDeclarationList(context, builder, node.declarationList);
  }

  processIdentifier(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Identifier,
  ): NativeCompilerBuilderVariableID {
    return this.processIdentifierAsValueDelegate(context, builder, node).getValue().variable;
  }

  processThis(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
  ): NativeCompilerBuilderVariableID {
    // the name 'this' is registered by buildFunction()
    const valueDelegate = this.resolveValueDelegate(context, builder, 'this', node);
    return valueDelegate.getValue().variable;
  }

  processNullKeyword(builder: INativeCompilerBlockBuilder, node: ts.Node): NativeCompilerBuilderVariableID {
    return builder.buildNull();
  }

  processUndefinedKeyword(builder: INativeCompilerBlockBuilder, node: ts.Node): NativeCompilerBuilderVariableID {
    return builder.buildUndefined();
  }

  procssTrueKeyword(buildr: INativeCompilerBlockBuilder, node: ts.Node): NativeCompilerBuilderVariableID {
    return buildr.buildLiteralBool(true);
  }

  processFalseKeyword(buildr: INativeCompilerBlockBuilder, node: ts.Node): NativeCompilerBuilderVariableID {
    return buildr.buildLiteralBool(false);
  }

  processSuperKeyword(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
  ): NativeCompilerBuilderVariableID {
    if (context.namePath.name === CONSTRUCTOR_NAME) {
      // We are in a constructor, the super keyword should reference the super constructor
      return builder.buildGetSuperConstructor();
    } else {
      return builder.buildGetSuper();
    }
  }

  processIdentifierAsAtom(builder: INativeCompilerBlockBuilder, node: ts.Identifier): NativeCompilerBuilderAtomID {
    return this.processIdentifierStringAsAtom(builder, node.text);
  }

  processIdentifierStringAsAtom(builder: INativeCompilerBlockBuilder, text: string): NativeCompilerBuilderAtomID {
    return this.moduleBuilder.registerAtom(text);
  }

  private resolveRegisteredVariableRefForIdentifierName(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
    name: string,
  ): NativeCompilerBuilderVariableRef {
    const variableRef = builder.resolveVariableRefOrDelegateInScope(name);
    if (!(variableRef instanceof NativeCompilerBuilderVariableRef)) {
      this.onError(node, 'Declaration was not registered');
    }

    return variableRef;
  }
  private resolveRegisteredVariableRefForIdentifier(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Identifier,
  ): NativeCompilerBuilderVariableRef {
    return this.resolveRegisteredVariableRefForIdentifierName(context, builder, node, node.text);
  }

  private getVariableRefNameForImportedModule(node: ts.Expression): string {
    const moduleVariableName = this.moduleVariableNameByPosition[node.pos];
    if (!moduleVariableName) {
      this.onError(node, 'Variable ref name not registered for import clause');
    }

    return moduleVariableName;
  }

  private createVariableRefNameForImportedModule(node: ts.Expression): string {
    const variableRefName = this.allocateUniqueVariableRefName('$imported_module');
    if (this.moduleVariableNameByPosition[node.pos]) {
      this.onError(node, 'Duplicated module variable name for import clause');
    }

    this.moduleVariableNameByPosition[node.pos] = variableRefName;

    return variableRefName;
  }

  private static makeValueDelegate(
    builder: INativeCompilerBlockBuilder,
    variableName: string,
    variableRefOrDelegate: NativeCompilerBuilderVariableRef | IVariableDelegate,
  ): NativeCompilerVariableIDValueDelegate {
    if (variableRefOrDelegate instanceof NativeCompilerBuilderVariableRef) {
      return {
        getValue: () => {
          return { variable: builder.buildLoadVariableRef(variableRefOrDelegate), parentVariable: undefined };
        },
        setValue: (value) => {
          builder.buildSetVariableRef(variableRefOrDelegate, value);
        },
      };
    } else {
      return {
        getValue: () => {
          return { variable: variableRefOrDelegate.onGetValue(builder), parentVariable: undefined };
        },
        setValue: (value) => {
          if (variableRefOrDelegate.onSetValue) {
            variableRefOrDelegate.onSetValue(builder, value);
          } else {
            throw new Error(`Variable from identifier '${variableName}' is read-only`);
          }
        },
      };
    }
  }

  private resolveValueDelegate(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    variableName: string,
    node: ts.Node,
  ): NativeCompilerVariableIDValueDelegate {
    if (variableName === 'undefined') {
      return {
        getValue: () => {
          return { variable: builder.buildUndefined(), parentVariable: undefined };
        },
        setValue: (value) => {
          this.onError(node, 'undefined cannot be set');
        },
      };
    }

    const resolvedVar = builder.resolveVariableRefOrDelegateInScope(variableName);
    if (resolvedVar !== undefined) {
      return NativeCompiler.makeValueDelegate(builder, variableName, resolvedVar);
    }

    return {
      getValue: () => {
        return this.getGlobalVariable(context, builder, variableName);
      },
      setValue: (value) => {
        const parentObject = builder.buildGlobal();
        const atomID = this.processIdentifierStringAsAtom(builder, variableName);
        builder.buildSetProperty(parentObject, atomID, value);
      },
    };
  }

  private processIdentifierAsValueDelegate(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Identifier,
  ): NativeCompilerVariableIDValueDelegate {
    return this.resolveValueDelegate(context, builder, node.text, node);
  }

  private getGlobalVariable(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    variableName: string,
  ): NativeCompilerVariableIDWithParent {
    const parentObject = builder.buildGlobal();
    const atomID = this.processIdentifierStringAsAtom(builder, variableName);

    if (context.assignmentTracker) {
      context.assignmentTracker.onGetProperty(parentObject, atomID);
    }

    return { variable: builder.buildGetProperty(parentObject, atomID, parentObject), parentVariable: parentObject };
  }

  private resolveNamePath(node: ts.Node, initialNamePath: NamePath): NamePath {
    if (ts.isIdentifier(node)) {
      return initialNamePath.appending(node.text);
    } else if (ts.isPropertyAccessExpression(node)) {
      const newNamePath = this.resolveNamePath(node.expression, initialNamePath);
      return this.resolveNamePath(node.name, newNamePath);
    } else {
      return initialNamePath;
    }
  }

  private processBinaryExpressionAsAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    operatorType: NativeCompilerIR.BinaryOperator,
    leftNode: ts.Node,
    leftVariable: Lazy<NativeCompilerBuilderVariableID>,
    rightVariable: NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    const resultVariable = builder.buildBinaryOp(operatorType, leftVariable.target, rightVariable);

    return this.processBinaryExpressionResultAsAssignment(context, builder, leftNode, leftVariable, resultVariable);
  }

  private processAssignmentElement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    name: ts.Identifier,
    initializer?: ts.Expression,
  ): AssignmentElementVariable {
    const resolvedVar = builder.resolveVariableInScope(name.text);
    if (!resolvedVar) {
      // the requested variable name must already exist
      this.onError(name, 'Unknown variable: ' + name.text);
    }
    return {
      variable: resolvedVar,
      defaultValue: initializer
        ? (input: NativeCompilerBuilderVariableID) =>
            this.buildConditionalVariable(
              context,
              builder,
              (context, builder) =>
                builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.EqualEqualEqual, input, builder.buildUndefined()),
              (context, builder) => this.processExpression(context, builder, initializer),
              (context, builder) => input,
            )
        : undefined,
      isRest: false,
    };
  }

  private processArrayAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    pattern: ts.ArrayLiteralExpression,
    resultVariable: NativeCompilerBuilderVariableID,
  ) {
    const allVariables: [number, AssignmentElementVariable][] = [];
    let elementIndex = 0;
    for (const element of pattern.elements) {
      const currentElementIndex = elementIndex++;
      if (ts.isOmittedExpression(element)) {
        continue;
      }
      if (ts.isIdentifier(element)) {
        // simple element
        allVariables.push([currentElementIndex, this.processAssignmentElement(context, builder, element)]);
      } else if (
        ts.isBinaryExpression(element) &&
        element.operatorToken.kind === ts.SyntaxKind.EqualsToken &&
        ts.isIdentifier(element.left)
      ) {
        // element with default
        allVariables.push([
          currentElementIndex,
          this.processAssignmentElement(context, builder, element.left, element.right),
        ]);
      } else if (ts.isObjectLiteralExpression(element)) {
        // object pattern in array
        const elementValue = builder.buildGetPropertyValue(
          resultVariable,
          builder.buildLiteralInteger(currentElementIndex.toString()),
          resultVariable,
        );
        this.processObjectAssignment(context, builder, element, elementValue);
      } else if (ts.isArrayLiteralExpression(element)) {
        // object pattern in array
        const elementValue = builder.buildGetPropertyValue(
          resultVariable,
          builder.buildLiteralInteger(currentElementIndex.toString()),
          resultVariable,
        );
        this.processArrayAssignment(context, builder, element, elementValue);
      } else if (ts.isSpreadElement(element) && ts.isIdentifier(element.expression)) {
        // ...rest
        const restElement: AssignmentElementVariable = {
          ...this.processAssignmentElement(context, builder, element.expression),
          isRest: true,
        };
        allVariables.push([currentElementIndex, restElement]);
      } else if (ts.isSpreadElement(element)) {
        // Call object.slice(elementIndex)
        const sliceMethod = builder.buildGetProperty(
          resultVariable,
          this.processIdentifierStringAsAtom(builder, 'slice'),
          resultVariable,
        );
        const elementValue = builder.buildFunctionInvocation(
          sliceMethod,
          [builder.buildLiteralInteger(currentElementIndex.toString())],
          resultVariable,
        );
        if (ts.isArrayLiteralExpression(element.expression)) {
          this.processArrayAssignment(context, builder, element.expression, elementValue);
        } else if (ts.isObjectLiteralExpression(element.expression)) {
          this.processObjectAssignment(context, builder, element.expression, elementValue);
        } else {
          this.onError(element, 'Unsupported spread assignment');
        }
      } else {
        this.onError(element, 'Unsupported assignment');
      }
    }
    for (const [elementIndex, element] of allVariables) {
      let elementValue: NativeCompilerBuilderVariableID;
      if (element.isRest) {
        // Call object.slice(elementIndex)
        const sliceMethod = builder.buildGetProperty(
          resultVariable,
          this.processIdentifierStringAsAtom(builder, 'slice'),
          resultVariable,
        );
        elementValue = builder.buildFunctionInvocation(
          sliceMethod,
          [builder.buildLiteralInteger(elementIndex.toString())],
          resultVariable,
        );
      } else {
        elementValue = builder.buildGetPropertyValue(
          resultVariable,
          builder.buildLiteralInteger(elementIndex.toString()),
          resultVariable,
        );
        if (element.defaultValue) {
          elementValue = element.defaultValue(elementValue);
        }
      }
      builder.buildAssignment(element.variable, elementValue);
    }
  }

  private processObjectAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    pattern: ts.ObjectLiteralExpression,
    resultVariable: NativeCompilerBuilderVariableID,
  ) {
    const allVariables: [ts.Identifier, AssignmentElementVariable][] = [];
    for (const element of pattern.properties) {
      if (ts.isShorthandPropertyAssignment(element)) {
        allVariables.push([
          element.name,
          this.processAssignmentElement(context, builder, element.name, element.objectAssignmentInitializer),
        ]);
      } else if (
        ts.isPropertyAssignment(element) &&
        ts.isIdentifier(element.name) &&
        ts.isIdentifier(element.initializer)
      ) {
        allVariables.push([
          element.name,
          this.processAssignmentElement(context, builder, element.initializer, undefined),
        ]);
      } else if (
        ts.isPropertyAssignment(element) &&
        ts.isIdentifier(element.name) &&
        ts.isBinaryExpression(element.initializer) &&
        ts.isIdentifier(element.initializer.left)
      ) {
        allVariables.push([
          element.name,
          this.processAssignmentElement(context, builder, element.initializer.left, element.initializer.right),
        ]);
      } else if (ts.isSpreadAssignment(element) && ts.isIdentifier(element.expression)) {
        // ...rest
        const restElement: AssignmentElementVariable = {
          ...this.processAssignmentElement(context, builder, element.expression),
          isRest: true,
        };
        allVariables.push([element.expression, restElement]);
      } else if (
        ts.isPropertyAssignment(element) &&
        ts.isIdentifier(element.name) &&
        ts.isObjectLiteralExpression(element.initializer)
      ) {
        // recursive assignment
        const propertyValue = builder.buildGetProperty(
          resultVariable,
          this.processIdentifierAsAtom(builder, element.name),
          resultVariable,
        );
        this.processObjectAssignment(context, builder, element.initializer, propertyValue);
      } else {
        this.onError(element, 'Unsupported assignment');
      }
    }
    const propertiesToIgnore: NativeCompilerBuilderAtomID[] = [];
    for (const [identifier, element] of allVariables) {
      let elementValue: NativeCompilerBuilderVariableID;
      if (element.isRest) {
        // ...rest can only be the last element so propertiesToIgnore should already have been populated
        elementValue = builder.buildNewObject();
        builder.buildCopyPropertiesFrom(elementValue, resultVariable, propertiesToIgnore);
      } else {
        elementValue = builder.buildGetProperty(
          resultVariable,
          this.processIdentifierAsAtom(builder, identifier),
          resultVariable,
        );
        if (element.defaultValue) {
          elementValue = element.defaultValue(elementValue);
        }
      }
      builder.buildAssignment(element.variable, elementValue);
      propertiesToIgnore.push(this.processIdentifierAsAtom(builder, identifier));
    }
  }

  private processBinaryExpressionResultAsAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    leftNode: ts.Node,
    leftVariable: Lazy<NativeCompilerBuilderVariableID>,
    resultVariable: NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    if (ts.isPropertyAccessExpression(leftNode)) {
      this.processPropertyAccessExpressionAsAssignment(
        context,
        builder,
        leftNode.name,
        leftNode.expression,
        resultVariable,
      );
    } else if (ts.isIdentifier(leftNode)) {
      const valueDelegate = this.processIdentifierAsValueDelegate(context, builder, leftNode);
      valueDelegate.setValue(resultVariable);
    } else if (ts.isElementAccessExpression(leftNode)) {
      this.processElementAccessExpressionAsAssignment(context, builder, leftNode, resultVariable);
    } else if (ts.isArrayLiteralExpression(leftNode)) {
      this.processArrayAssignment(context, builder, leftNode, resultVariable);
    } else if (ts.isObjectLiteralExpression(leftNode)) {
      this.processObjectAssignment(context, builder, leftNode, resultVariable);
    } else {
      this.onError(leftNode, 'Unsupported assignment');
    }
    return resultVariable;
  }

  processBinaryExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.BinaryExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    let rightContext = context.withNamePath(this.resolveNamePath(node.left, context.namePath));

    if (node.operatorToken.kind === ts.SyntaxKind.BarBarToken) {
      const result = builder.buildAssignableUndefined();

      const leftCondition = builder.buildJumpTarget('lhs_condition');
      const leftConditionTrue = builder.buildJumpTarget('lhs_true');
      const leftConditionFalse = builder.buildJumpTarget('lhs_false');
      const exitTarget = builder.buildJumpTarget('exit');

      const lhs = this.processExpression(context, leftCondition.builder, node.left);

      // If lhs is truthy, assign lhs to result
      leftConditionTrue.builder.buildAssignment(result, lhs);
      leftConditionTrue.builder.buildJump(exitTarget.target);

      // If lhs is falsy, assign rhs to result
      const rhs = this.processExpression(rightContext, leftConditionFalse.builder, node.right);
      leftConditionFalse.builder.buildAssignment(result, rhs);
      leftConditionFalse.builder.buildJump(exitTarget.target);

      leftCondition.builder.buildBranch(
        lhs,
        NativeCompilerBuilderBranchType.Truthy,
        leftConditionTrue.target,
        leftConditionFalse.target,
      );

      return result;
    } else if (node.operatorToken.kind === ts.SyntaxKind.AmpersandAmpersandToken) {
      const result = builder.buildAssignableUndefined();

      const leftCondition = builder.buildJumpTarget('lhs_condition');
      const leftConditionTrue = builder.buildJumpTarget('lhs_true');
      const leftConditionFalse = builder.buildJumpTarget('lhs_false');
      const exitTarget = builder.buildJumpTarget('exit');

      const lhs = this.processExpression(context, leftCondition.builder, node.left);

      // If lhs is truthy, assign rhs to result
      const rhs = this.processExpression(rightContext, leftConditionTrue.builder, node.right);
      leftConditionTrue.builder.buildAssignment(result, rhs);
      leftConditionTrue.builder.buildJump(exitTarget.target);

      // If lhs is falsy, assign lhs to result
      leftConditionFalse.builder.buildAssignment(result, lhs);
      leftConditionFalse.builder.buildJump(exitTarget.target);

      leftCondition.builder.buildBranch(
        lhs,
        NativeCompilerBuilderBranchType.Truthy,
        leftConditionTrue.target,
        leftConditionFalse.target,
      );

      return result;
    } else if (node.operatorToken.kind === ts.SyntaxKind.QuestionQuestionToken) {
      const result = builder.buildAssignableUndefined();

      const lhsValue = builder.buildJumpTarget('lhs_value');
      const lhsAssign = builder.buildJumpTarget('lhs_assign');
      const rhsValue = builder.buildJumpTarget('rhs_value');
      const rhsAssign = builder.buildJumpTarget('rhs_assign');
      const exitTarget = builder.buildJumpTarget('exit');

      const lhs = this.processExpression(context, lhsValue.builder, node.left);

      // lhs_assign will be evaluated if lhs is not null or undefined, and will assign the result
      lhsAssign.builder.buildAssignment(result, lhs);
      lhsAssign.builder.buildJump(exitTarget.target);

      // rhs_value will be evaluated if lhs_value is null or undefined
      const rhs = this.processExpression(rightContext, rhsValue.builder, node.right);
      rhsValue.builder.buildJump(rhsAssign.target);

      // rhs_assign will be evaluated right after rhs_value is evaluated, and will assign the result
      rhsAssign.builder.buildAssignment(result, rhs);
      rhsAssign.builder.buildJump(exitTarget.target);

      lhsValue.builder.buildBranch(
        lhs,
        NativeCompilerBuilderBranchType.NotUndefinedOrNull,
        lhsAssign.target,
        rhsValue.target,
      );

      return result;
    } else if (node.operatorToken.kind === ts.SyntaxKind.QuestionQuestionEqualsToken) {
      const lhsValue = builder.buildJumpTarget('lhs_value');
      const rhsValue = builder.buildJumpTarget('rhs_value');
      const rhsAssign = builder.buildJumpTarget('rhs_assign');
      const exitTarget = builder.buildJumpTarget('exit');

      const lhs = this.processExpression(context, lhsValue.builder, node.left);

      // rhs_value will be evaluated if lhs_value is null or undefined
      const rhs = this.processExpression(rightContext, rhsValue.builder, node.right);
      rhsValue.builder.buildJump(rhsAssign.target);

      // rhs_assign will be evaluated right after rhs_value is evaluated, and will assign to lhs
      rhsAssign.builder.buildAssignment(
        lhs,
        this.processBinaryExpressionResultAsAssignment(context, rhsAssign.builder, node.left, new Lazy(() => lhs), rhs),
      );
      rhsAssign.builder.buildJump(exitTarget.target);

      lhsValue.builder.buildBranch(
        lhs,
        NativeCompilerBuilderBranchType.NotUndefinedOrNull,
        exitTarget.target, // no assignment if lhs is not nullish
        rhsValue.target,
      );

      return lhs;
    } else if (node.operatorToken.kind === ts.SyntaxKind.BarBarEqualsToken) {
      const leftCondition = builder.buildJumpTarget('lhs_condition');
      const leftConditionFalse = builder.buildJumpTarget('lhs_false');
      const exitTarget = builder.buildJumpTarget('exit');

      const lhs = this.processExpression(context, leftCondition.builder, node.left);

      // If lhs is falsy, assign rhs to lhs
      const rhs = this.processExpression(rightContext, leftConditionFalse.builder, node.right);
      leftConditionFalse.builder.buildAssignment(
        lhs,
        this.processBinaryExpressionResultAsAssignment(
          context,
          leftConditionFalse.builder,
          node.left,
          new Lazy(() => lhs),
          rhs,
        ),
      );
      leftConditionFalse.builder.buildJump(exitTarget.target);

      leftCondition.builder.buildBranch(
        lhs,
        NativeCompilerBuilderBranchType.Truthy,
        exitTarget.target, // If lhs is truthy, return lhs
        leftConditionFalse.target,
      );

      return lhs;
    }

    const leftVariable = new Lazy(() => this.processExpression(context, builder, node.left));
    const rightVariable = this.processExpression(rightContext, builder, node.right);

    switch (node.operatorToken.kind) {
      case ts.SyntaxKind.AsteriskToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Mult, leftVariable.target, rightVariable);
      case ts.SyntaxKind.SlashToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Div, leftVariable.target, rightVariable);
      case ts.SyntaxKind.LessThanToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.LessThan, leftVariable.target, rightVariable);
      case ts.SyntaxKind.LessThanEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.LessThanOrEqual,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.LessThanLessThanEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.LessThanOrEqualEqual,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.GreaterThanToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.GreaterThan, leftVariable.target, rightVariable);
      case ts.SyntaxKind.GreaterThanEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.GreaterThanOrEqual,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.GreaterThanGreaterThanEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.GreaterThanOrEqualEqual,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.EqualsEqualsToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.EqualEqual, leftVariable.target, rightVariable);
      case ts.SyntaxKind.EqualsEqualsEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.EqualEqualEqual,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.ExclamationEqualsToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.DifferentThan, leftVariable.target, rightVariable);
      case ts.SyntaxKind.ExclamationEqualsEqualsToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.DifferentThanStrict,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.MinusToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Sub, leftVariable.target, rightVariable);
      case ts.SyntaxKind.PlusToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Add, leftVariable.target, rightVariable);
      case ts.SyntaxKind.GreaterThanGreaterThanToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.RightShift, leftVariable.target, rightVariable);
      case ts.SyntaxKind.GreaterThanGreaterThanGreaterThanToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.UnsignedRightShift,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.LessThanLessThanToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.LeftShift, leftVariable.target, rightVariable);
      case ts.SyntaxKind.BarToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.BitwiseOR, leftVariable.target, rightVariable);
      case ts.SyntaxKind.CaretToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.BitwiseXOR, leftVariable.target, rightVariable);
      case ts.SyntaxKind.AmpersandToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.BitwiseAND, leftVariable.target, rightVariable);
      case ts.SyntaxKind.PercentToken:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Modulo, leftVariable.target, rightVariable);
      case ts.SyntaxKind.AsteriskAsteriskToken:
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.Exponentiation,
          leftVariable.target,
          rightVariable,
        );
      case ts.SyntaxKind.InstanceOfKeyword:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.InstanceOf, leftVariable.target, rightVariable);
      case ts.SyntaxKind.CommaToken:
        leftVariable.loadIfNeeded();
        return rightVariable;
      case ts.SyntaxKind.InKeyword:
        return builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.In, leftVariable.target, rightVariable);
      /**
       * Start of binary assignment operators
       */
      case ts.SyntaxKind.EqualsToken:
        return this.processBinaryExpressionResultAsAssignment(context, builder, node.left, leftVariable, rightVariable);
      case ts.SyntaxKind.PlusEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.Add,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.MinusEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.Sub,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.AsteriskEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.Mult,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.SlashEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.Div,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.AmpersandEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.BitwiseAND,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.BarEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.BitwiseOR,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.CaretEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.BitwiseXOR,
          node.left,
          leftVariable,
          rightVariable,
        );
      case ts.SyntaxKind.GreaterThanGreaterThanGreaterThanEqualsToken:
        return this.processBinaryExpressionAsAssignment(
          context,
          builder,
          NativeCompilerIR.BinaryOperator.UnsignedRightShift,
          node.left,
          leftVariable,
          rightVariable,
        );
      default:
        this.onError(
          node,
          `Operator not supported in processBinaryExpression (operator id: ${node.operatorToken.kind})`,
        );
    }
  }

  processInterfaceDeclaration(builder: INativeCompilerBlockBuilder, node: ts.InterfaceDeclaration) {
    //NOOP
  }

  private processDeclarationWithDefaultInitializer(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Expression,
    inputVariable: NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    return this.buildConditionalVariable(
      context,
      builder,
      (context, builder) => {
        return builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.DifferentThanStrict,
          inputVariable,
          builder.buildUndefined(),
        );
      },
      (context, builder) => {
        return inputVariable;
      },
      (context, builder) => {
        return this.processExpression(context, builder, node);
      },
    );
  }

  processParameterDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ParameterDeclaration,
    parentNode: ts.Node,
    index: number,
  ): ((context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => void) | undefined {
    if (!ts.isIdentifier(node.name)) {
      if (ts.isParameterPropertyDeclaration(node, parentNode)) {
        this.onError(node, 'Parameter property declaration not yet supported');
      }

      this.processBindingName(context, builder, node.name, undefined, (context) => {
        if (node.dotDotDotToken) {
          return builder.buildGetFunctionArgumentsObject(index);
        } else {
          return builder.buildGetFunctionArg(index);
        }
      });

      return;
    }

    let paramVariable: NativeCompilerBuilderVariableID;
    if (node.dotDotDotToken) {
      paramVariable = builder.buildGetFunctionArgumentsObject(index);
    } else {
      paramVariable = builder.buildGetFunctionArg(index);
    }

    const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, node.name);
    let resolvedVariable: NativeCompilerBuilderVariableID;

    const defaultParameterValue = node.initializer;
    if (defaultParameterValue) {
      resolvedVariable = this.processDeclarationWithDefaultInitializer(
        context,
        builder,
        defaultParameterValue,
        paramVariable,
      );
    } else {
      resolvedVariable = paramVariable;
    }

    builder.buildSetVariableRef(variableRef, resolvedVariable);

    if (ts.isParameterPropertyDeclaration(node, parentNode)) {
      return (context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => {
        const thisVariable = this.processThis(context, builder, node);
        const propertyName = this.processIdentifierAsAtom(builder, node.name);

        this.doBuildSetProperty(
          context,
          builder,
          propertyName,
          thisVariable,
          builder.resolveVariableInScope(node.name.text)!,
        );
      };
    }
    return undefined;
  }

  private isThisParameter(node: ts.ParameterDeclaration): boolean {
    if (!ts.isIdentifier(node.name)) {
      return false;
    }

    return node.name.text === 'this';
  }

  private processParameterDeclarations(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    parameters: ts.NodeArray<ts.ParameterDeclaration>,
    parentNode: ts.Node,
    isArrowFunction: boolean,
    enqueueDeferredStatement: EnqueueDeferredStatementFunc | undefined,
  ) {
    if (!isArrowFunction) {
      builder.registerLazyVariableRef('arguments', VariableRefScope.Function, (builder) => {
        const parametersVariableRef = builder.buildVariableRef(
          'arguments',
          NativeCompilerBuilderVariableType.Object,
          VariableRefScope.Function,
        );
        const parametersVariable = builder.buildGetFunctionArgumentsObject(0);
        builder.buildSetVariableRef(parametersVariableRef, parametersVariable);
        return parametersVariableRef;
      });
    }

    let parametersToProcess: ts.NodeArray<ts.ParameterDeclaration> | ts.ParameterDeclaration[];

    if (parameters.length && this.isThisParameter(parameters[0])) {
      // Ignore 'this' parameter
      parametersToProcess = parameters.slice(1);
    } else {
      parametersToProcess = parameters;
    }

    for (const parameter of parametersToProcess) {
      this.prepareBindingName(builder, parameter.name, VariableRefScope.Function);
    }

    parametersToProcess.forEach((p, i) => {
      const deferredStatement = this.processParameterDeclaration(context, builder, p, parentNode, i);
      if (deferredStatement) {
        if (enqueueDeferredStatement) {
          enqueueDeferredStatement(deferredStatement);
        } else {
          deferredStatement(context, builder);
        }
      }
    });
  }

  private isAmbientDeclaration(node: ts.Node): boolean {
    const modifiers = ts.getCombinedModifierFlags(node as ts.Declaration);
    return (modifiers & ts.ModifierFlags.Ambient) != 0;
  }

  processFunctionBody(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    body: ts.FunctionBody | ts.Expression,
    isGenerator: boolean,
    isAsync: boolean,
  ) {
    if (isGenerator && isAsync) {
      const generator = this.processGenerator(context, builder, body, isGenerator, isAsync);
      const helper = this.getGlobalVariable(context, builder, '__tsn_async_generator_helper').variable;
      const asyncGenenerator = builder.buildFunctionInvocation(helper, [generator], builder.buildUndefined());
      builder.buildReturn(asyncGenenerator);
    } else if (isGenerator) {
      // generator: build an iterator on the closure and return it
      const generator = this.processGenerator(context, builder, body, isGenerator, isAsync);
      builder.buildReturn(generator);
    } else if (isAsync) {
      // convert the async function body into a generator
      // the `await` keyword will produce the same code as `yield`
      const generator = this.processGenerator(context, builder, body, isGenerator, isAsync);
      // the helper function is defined in "tsn/rtl/rtl/tsn.ts"
      // and is expected to be present in the global namespace
      const helper = this.getGlobalVariable(context, builder, '__tsn_async_helper').variable;
      // async function returns a promise from the helper
      const promise = builder.buildFunctionInvocation(helper, [generator], builder.buildUndefined());
      builder.buildReturn(promise);
    } else {
      // regular function: build function body
      if (ts.isBlock(body)) {
        this.processBlock(context, builder, body);
      } else {
        const returnValue = this.processExpression(context, builder, body);
        builder.buildReturn(returnValue);
      }
    }
  }

  processFunctionDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.FunctionDeclaration,
    isArrowFunction: boolean,
  ) {
    if (!node.body) {
      return;
    }

    this.appendNodeDebugInfo(builder, node);

    const functionVariableId = this.doProcessFunctionDeclaration(
      context,
      builder,
      node.name,
      undefined,
      false,
      false,
      false,
      undefined,
      (context, builder) => {
        this.processParameterDeclarations(context, builder, node.parameters, node, isArrowFunction, undefined);
        if (node.body) {
          const isGenerator = !!node.asteriskToken;
          const isAsync = !!node.modifiers?.some((m) => m.kind == ts.SyntaxKind.AsyncKeyword);
          this.processFunctionBody(context, builder, node.body, isGenerator, isAsync);
        }
        return node.parameters.length;
      },
    );

    this.appendExportedDeclarationIfNeeded(context, builder, node, node.name, functionVariableId);

    return functionVariableId;
  }

  private processGenerator(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    body: ts.FunctionBody | ts.Expression,
    isGenerator: boolean,
    isAsync: boolean,
  ) {
    const functionName = this.getFunctionName(context.namePath.appendingTSNode(undefined));
    // build the generator closure
    const functionBuilder = this.moduleBuilder.buildFunction(
      functionName,
      builder,
      true,
      true /* async are processed as generators at builder level */,
      false,
    );
    const functionContext = context.withNamePath(context.namePath, isGenerator, isAsync);
    if (ts.isBlock(body)) {
      this.processBlock(functionContext, functionBuilder.builder, body);
    } else {
      const returnValue = this.processExpression(functionContext, functionBuilder.builder, body);
      functionBuilder.builder.buildReturn(returnValue);
    }
    const closure = builder.buildNewArrowFunctionValue(functionName, 1, functionBuilder.closureArguments, true);
    return builder.buildGenerator(closure);
  }

  private doProcessFunctionDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    name: ts.Identifier | undefined,
    namePathPrefx: string | undefined,
    isMethod: boolean,
    isClass: boolean,
    isExpression: boolean,
    parentClass: NativeCompilerBuilderVariableID | undefined,
    doBuild: (context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => number,
  ): NativeCompilerBuilderVariableID {
    let newNamePath = context.namePath.appendingTSNode(name, namePathPrefx);
    const functionName = this.getFunctionName(newNamePath);

    let localFunctionName: ts.Identifier | undefined;
    let constructorName: NativeCompilerBuilderAtomID;
    if (name && ts.isIdentifier(name)) {
      localFunctionName = name;
      constructorName = this.moduleBuilder.registerAtom(localFunctionName.text);
    } else {
      constructorName = this.moduleBuilder.registerAtom('anonymous');
    }

    let variableRef: NativeCompilerBuilderVariableRef | undefined;
    if (localFunctionName && !isMethod) {
      if (isExpression) {
        builder = builder.buildSubBuilder(true);
        variableRef = builder.buildVariableRef(
          localFunctionName.text,
          NativeCompilerBuilderVariableType.Object,
          VariableRefScope.Block,
        );
      } else {
        variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, localFunctionName);
      }
    }

    const functionBuilder = this.moduleBuilder.buildFunction(functionName, builder, false, false, isClass);

    const argc = doBuild(context.withNamePath(newNamePath), functionBuilder.builder);

    let variable: NativeCompilerBuilderVariableID;

    if (isClass) {
      const resolvedParentClass = parentClass ?? builder.buildUndefined();
      variable = builder.buildNewClassValue(
        functionName,
        constructorName,
        argc,
        resolvedParentClass,
        functionBuilder.closureArguments,
      );
    } else {
      variable = builder.buildNewFunctionValue(functionName, constructorName, argc, functionBuilder.closureArguments);
    }

    if (variableRef) {
      builder.buildSetVariableRef(variableRef, variable);
    }

    return variable;
  }

  processFunctionExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.FunctionExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    let newNamePath = context.namePath.appendingTSNode(node.name);
    const functionName = this.getFunctionName(newNamePath);
    let constructorName: NativeCompilerBuilderAtomID;
    let variableRef: NativeCompilerBuilderVariableRef | undefined;
    if (node.name) {
      // If we have a name, we need to register a variable ref for our function expression scoped
      // to the inner function. The inner function will have access to the variable ref of itself
      builder = builder.buildSubBuilder(true);
      constructorName = this.processIdentifierAsAtom(builder, node.name);
      variableRef = builder.buildVariableRef(
        node.name.text,
        NativeCompilerBuilderVariableType.Object,
        VariableRefScope.Block,
      );
    } else {
      constructorName = this.moduleBuilder.registerAtom(newNamePath.name);
    }

    const functionBuilder = this.moduleBuilder.buildFunction(functionName, builder, false, false, false);
    const childContext = context.withNamePath(newNamePath);
    this.processParameterDeclarations(childContext, functionBuilder.builder, node.parameters, node, false, undefined);

    const isGenerator = !!node.asteriskToken;
    const isAsync = !!node.modifiers?.some((m) => m.kind == ts.SyntaxKind.AsyncKeyword);
    this.processFunctionBody(context, functionBuilder.builder, node.body, isGenerator, isAsync);

    const variableId = builder.buildNewFunctionValue(
      functionName,
      constructorName,
      node.parameters.length,
      functionBuilder.closureArguments,
    );

    if (variableRef) {
      builder.buildSetVariableRef(variableRef, variableId);
    }

    return variableId;
  }

  processArrowFunction(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ArrowFunction,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);
    const functionName = this.getFunctionName(context.namePath.appendingTSNode(undefined));

    const functionBuilder = this.moduleBuilder.buildFunction(functionName, builder, true, false, false);
    this.processParameterDeclarations(context, functionBuilder.builder, node.parameters, node, true, undefined);
    const isGenerator = !!node.asteriskToken;
    const isAsync = !!node.modifiers?.some((m) => m.kind == ts.SyntaxKind.AsyncKeyword);
    this.processFunctionBody(context, functionBuilder.builder, node.body, isGenerator, isAsync);
    return builder.buildNewArrowFunctionValue(
      functionName,
      node.parameters.length,
      functionBuilder.closureArguments,
      false,
    );
  }

  buildArrayWithElements(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    elements: ts.NodeArray<ts.Expression>,
  ): NativeCompilerBuilderVariableID {
    const arrayVariableID = builder.buildNewArray();

    if (elements.length > 0) {
      let index = builder.buildLiteralInteger('0');
      for (const element of elements) {
        if (ts.isSpreadElement(element)) {
          const spreadElement: ts.SpreadElement = element;
          const sourceObject = this.processExpression(context, builder, spreadElement.expression);
          const addedPropertiesVariableId = builder.buildCopyPropertiesFrom(arrayVariableID, sourceObject, undefined);
          index = builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Add, index, addedPropertiesVariableId);
        } else {
          const variable = this.processExpression(context, builder, element);
          builder.buildSetPropertyValue(arrayVariableID, index, variable);
          index = builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Inc, index);
        }
      }
    }

    return arrayVariableID;
  }

  processArrayLiteralExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ArrayLiteralExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);
    return this.buildArrayWithElements(context, builder, node.elements);
  }

  processNewExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.NewExpression,
  ): NativeCompilerBuilderVariableID {
    const variable = this.processExpression(context, builder, node.expression);
    this.appendNodeDebugInfo(builder, node);

    if (node.arguments && this.hasSpreadElement(node.arguments)) {
      const arrayArguments = this.buildArrayWithElements(context, builder, node.arguments);
      return builder.buildConstructorInvocationWithArgsArray(variable, variable, arrayArguments);
    } else {
      const inputArgs: NativeCompilerBuilderVariableID[] = [];

      if (node.arguments) {
        node.arguments.forEach((e) => {
          let variable = this.processExpression(context, builder, e);
          inputArgs.push(variable);
        });
      }
      return builder.buildConstructorInvocation(variable, variable, inputArgs);
    }
  }

  private buildConditionalVariable(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    buildConditionVariable: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
    ) => NativeCompilerBuilderVariableID,
    buildTrueResult: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
    ) => NativeCompilerBuilderVariableID,
    buildFalseResult: (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
    ) => NativeCompilerBuilderVariableID,
  ): NativeCompilerBuilderVariableID {
    const outputVariable = builder.buildAssignableUndefined();
    const condition = builder.buildJumpTarget('condition');
    const trueTarget = builder.buildJumpTarget('condition_true');
    const falseTarget = builder.buildJumpTarget('condition_false');
    const exit = builder.buildJumpTarget('exit');

    const conditionVariable = buildConditionVariable(context, condition.builder);

    const whenTrueVariable = buildTrueResult(context, trueTarget.builder);
    trueTarget.builder.buildAssignment(outputVariable, whenTrueVariable);
    trueTarget.builder.buildJump(exit.target);

    const whenFalseVariable = buildFalseResult(context, falseTarget.builder);
    falseTarget.builder.buildAssignment(outputVariable, whenFalseVariable);
    falseTarget.builder.buildJump(exit.target);

    condition.builder.buildBranch(
      conditionVariable,
      NativeCompilerBuilderBranchType.Truthy,
      trueTarget.target,
      falseTarget.target,
    );

    return outputVariable;
  }

  processConditionalExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ConditionalExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    return this.buildConditionalVariable(
      context,
      builder,
      (context, builder) => {
        return this.processExpression(context, builder, node.condition);
      },
      (context, builder) => {
        return this.processExpression(context, builder, node.whenTrue);
      },
      (context, builder) => {
        return this.processExpression(context, builder, node.whenFalse);
      },
    );
  }

  processAsExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.AsExpression,
  ): NativeCompilerBuilderVariableID {
    return this.processExpression(context, builder, node.expression);
  }

  processTypeOfExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.TypeOfExpression,
  ): NativeCompilerBuilderVariableID {
    const variable = this.processExpression(context, builder, node.expression);
    return builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.TypeOf, variable);
  }

  processRegularExpressionLiteral(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.RegularExpressionLiteral,
  ): NativeCompilerBuilderVariableID {
    const regexCtor = this.getGlobalVariable(context, builder, 'RegExp');

    const inputRegEx = node.text;

    if (inputRegEx[0] !== '/') {
      this.onError(node, 'Expecting RegEx literal to start with /');
    }

    let regExToString = inputRegEx.substring(1);
    let flags = '';
    for (;;) {
      if (!regExToString.length) {
        this.onError(node, 'Expecting RegEx literal to end with /');
      }

      const trailing = regExToString[regExToString.length - 1];
      regExToString = regExToString.substring(0, regExToString.length - 1);
      if (trailing === '/') {
        break;
      } else {
        flags = trailing + flags;
      }
    }

    regExToString = regExToString.replace(/\\/g, '\\\\');

    const literalString = this.registerAndGetStringLiteralValue(builder, regExToString);

    if (flags) {
      const flagsLiteral = this.registerAndGetStringLiteralValue(builder, flags);
      return builder.buildConstructorInvocation(regexCtor.variable, regexCtor.variable, [literalString, flagsLiteral]);
    } else {
      return builder.buildConstructorInvocation(regexCtor.variable, regexCtor.variable, [literalString]);
    }
  }

  processReturnStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ReturnStatement,
  ) {
    let returnVariableID: NativeCompilerBuilderVariableID;
    if (node.expression) {
      returnVariableID = this.processExpression(context, builder, node.expression);
    } else {
      returnVariableID = builder.buildUndefined();
    }
    builder.buildReturn(returnVariableID);
  }

  processThrowStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.ThrowStatement) {
    const exceptionObject = this.processExpression(context, builder, node.expression);
    builder.buildThrow(exceptionObject);
  }

  processBreakStatement(builder: INativeCompilerBlockBuilder, node: ts.BreakStatement) {
    const exitTarget = builder.resolveExitTargetInScope();
    if (exitTarget === undefined) {
      this.onError(node, 'Could not resolve exit target to process break statement');
    }

    builder.buildJump(exitTarget);
  }

  processTryStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.TryStatement) {
    const hasCatchClause = !!node.catchClause;
    const hasFinallyBlock = !!node.finallyBlock;
    const tryCatch = builder.buildTryCatch(hasCatchClause, hasFinallyBlock);

    this.processBlock(context, tryCatch.tryBuilder, node.tryBlock);

    if (hasCatchClause) {
      const catchBuilder = tryCatch.catchBuilder!;
      this.appendNodeDebugInfo(catchBuilder, node.catchClause);
      const exceptionVariable = catchBuilder.buildGetException();

      if (node.catchClause.variableDeclaration) {
        const variableDeclaration = node.catchClause.variableDeclaration;
        if (!ts.isIdentifier(variableDeclaration.name)) {
          this.onError(variableDeclaration.name, 'Parsers does not currently support statement type');
        }

        const variableName = variableDeclaration.name.text;

        catchBuilder.buildInitializedVariableRef(variableName, exceptionVariable, VariableRefScope.Block);
      }

      this.processBlock(context, catchBuilder, node.catchClause.block);
    }

    if (hasFinallyBlock) {
      this.processBlock(context, tryCatch.finallyBuilder!, node.finallyBlock);
    }
  }

  processSwitchStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.SwitchStatement,
  ) {
    this.appendNodeDebugInfo(builder, node);
    builder = builder.buildSubBuilder(true);
    const switchStartJumpTarget = builder.buildJumpTarget('switch_enter');
    const jumpTargets: INativeCompilerJumpTargetBuilder[] = [];
    let defaultClauseJumpTarget: INativeCompilerJumpTargetBuilder | undefined;

    let switchCaseIndex = 0;
    for (const switchClause of node.caseBlock.clauses) {
      if (ts.isDefaultClause(switchClause)) {
        defaultClauseJumpTarget = builder.buildJumpTarget('switch_clause_default');
      } else {
        jumpTargets.push(builder.buildJumpTarget(`switch_clause_${switchCaseIndex}`));
        switchCaseIndex++;
      }
    }

    const exitJumpTarget = builder.buildJumpTarget('switch_exit');

    this.appendNodeDebugInfo(switchStartJumpTarget.builder, node.expression);
    const lhs = this.processExpression(context, switchStartJumpTarget.builder, node.expression);

    switchCaseIndex = 0;

    for (const switchClause of node.caseBlock.clauses) {
      if (ts.isDefaultClause(switchClause)) {
        const jumpTarget = defaultClauseJumpTarget!;

        jumpTarget.builder.registerExitTargetInScope(exitJumpTarget.target);

        this.appendNodeDebugInfo(switchStartJumpTarget.builder, switchClause);

        this.processStatementsInBlock(context, jumpTarget.builder, switchClause.statements);
      } else {
        const jumpTarget = jumpTargets[switchCaseIndex];

        jumpTarget.builder.registerExitTargetInScope(exitJumpTarget.target);

        this.appendNodeDebugInfo(switchStartJumpTarget.builder, switchClause);

        const rhs = this.processExpression(context, switchStartJumpTarget.builder, switchClause.expression);
        const conditionVariable = switchStartJumpTarget.builder.buildBinaryOp(
          NativeCompilerIR.BinaryOperator.EqualEqualEqual,
          lhs,
          rhs,
        );

        this.processStatementsInBlock(context, jumpTarget.builder, switchClause.statements);

        switchStartJumpTarget.builder.buildBranch(
          conditionVariable,
          NativeCompilerBuilderBranchType.Truthy,
          jumpTarget.target,
          undefined,
        );

        switchCaseIndex++;
      }
    }

    if (defaultClauseJumpTarget) {
      switchStartJumpTarget.builder.buildJump(defaultClauseJumpTarget.target);
    } else {
      switchStartJumpTarget.builder.buildJump(exitJumpTarget.target);
    }
  }

  processStatementsInBlock(
    context: NativeCompilerContext,
    scopedBuilder: INativeCompilerBlockBuilder,
    statements: ts.NodeArray<ts.Statement> | ts.Statement[],
    cb?: (statement: ts.Statement) => void,
  ): void {
    const compilationPlan: CompilationPlan = {
      functionDeclarationInsertionIndex: 0,
      statementsToProcess: [],
    };
    // Process all the statements that require to emit variable refs
    for (const statement of statements) {
      this.prepareStatement(compilationPlan, scopedBuilder, statement);
    }

    for (const statement of compilationPlan.statementsToProcess) {
      this.processStatement(context, scopedBuilder, statement);
      cb?.(statement);
    }
  }

  processBlock(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.Block) {
    const subBuilder = builder.buildSubBuilder(true);

    this.processStatementsInBlock(context, subBuilder, node.statements);
  }

  processSourceFile(initFunctionName: string, node: ts.SourceFile) {
    const functionBuilder = this.moduleBuilder.buildFunction(initFunctionName, undefined, true, false, false);
    const builder = functionBuilder.builder;

    // require is passed as first parameter
    builder.registerLazyVariableRef('require', VariableRefScope.Function, (builder) =>
      builder.buildInitializedVariableRef('require', builder.buildGetFunctionArg(0), VariableRefScope.Function),
    );
    // module is passed as second parameter
    builder.registerLazyVariableRef('module', VariableRefScope.Function, (builder) =>
      builder.buildInitializedVariableRef('module', builder.buildGetFunctionArg(1), VariableRefScope.Function),
    );
    // exports is passed as third parameter
    const exportsVarRef = builder.buildInitializedVariableRef(
      'exports',
      builder.buildGetFunctionArg(2),
      VariableRefScope.Function,
    );
    builder.registerExportsVariableRef(exportsVarRef);

    let emitResolver: EmitResolver | undefined;
    if (this.filePath.endsWith('.ts') || this.filePath.endsWith('.tsx')) {
      emitResolver = getEmitResolver(this.typeChecker, node, undefined);
    }

    const context = new NativeCompilerContext(new NamePath(this.moduleName), undefined, emitResolver, false, false);

    this.processStatementsInBlock(context, builder, node.statements);
  }

  processObjectLiteralExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ObjectLiteralExpression,
  ) {
    this.appendNodeDebugInfo(builder, node);
    const objectVariableID = builder.buildNewObject();

    interface BatchedProp {
      propId: NativeCompilerBuilderAtomID | NativeCompilerBuilderVariableID;
      thisVar: NativeCompilerBuilderVariableID;
      valueVar: NativeCompilerBuilderVariableID;
    }

    let batchedProps: BatchedProp[] = [];
    const flush = () => {
      if (batchedProps.length > 1 && batchedProps.every((p) => p.propId instanceof NativeCompilerBuilderAtomID)) {
        const propNames = batchedProps.map((p) => p.propId as NativeCompilerBuilderAtomID);
        const propVals = batchedProps.map((p) => p.valueVar);
        builder.buildSetProperties(batchedProps[0].thisVar, propNames, propVals);
      } else {
        for (const prop of batchedProps) {
          this.doBuildSetProperty(context, builder, prop.propId, prop.thisVar, prop.valueVar);
        }
      }
      batchedProps = [];
    };

    for (const property of node.properties) {
      if (ts.isPropertyAssignment(property)) {
        const { propertyVariableID, propertyInitVariableID } = this.processPropertyAssignment(
          context,
          builder,
          property,
        );
        batchedProps.push({ propId: propertyVariableID, thisVar: objectVariableID, valueVar: propertyInitVariableID });
        if (!this.options.mergeSetProperties) {
          flush();
        }
      } else if (ts.isShorthandPropertyAssignment(property)) {
        const propertyVariableID = this.processPropertyName(context, builder, property.name);
        const propertyInitVariableID = this.processIdentifier(context, builder, property.name);
        batchedProps.push({ propId: propertyVariableID, thisVar: objectVariableID, valueVar: propertyInitVariableID });
        if (!this.options.mergeSetProperties) {
          flush();
        }
      } else if (ts.isSpreadAssignment(property)) {
        const spreadAssigment: ts.SpreadAssignment = property;
        const sourceObject = this.processExpression(context, builder, spreadAssigment.expression);
        flush();
        builder.buildCopyPropertiesFrom(objectVariableID, sourceObject, undefined);
      } else if (ts.isMethodDeclaration(property)) {
        const propertyName = this.processPropertyName(context, builder, property.name);

        const functionVariableID = this.doProcessFunctionDeclaration(
          context,
          builder,
          ts.isIdentifier(property.name) ? property.name : undefined,
          undefined,
          true,
          false,
          false,
          undefined,
          (context, builder) => {
            this.processParameterDeclarations(context, builder, property.parameters, property, false, undefined);
            const isGenerator = !!property.asteriskToken;
            const isAsync = !!property.modifiers?.some((m) => m.kind == ts.SyntaxKind.AsyncKeyword);
            if (property.body) {
              this.processFunctionBody(context, builder, property.body, isGenerator, isAsync);
            }
            return property.parameters.length;
          },
        );
        batchedProps.push({ propId: propertyName, thisVar: objectVariableID, valueVar: functionVariableID });
        if (!this.options.mergeSetProperties) {
          flush();
        }
      } else {
        this.onError(property, 'Failed to process literal object expression');
      }
    }
    flush();

    return objectVariableID;
  }

  processPropertyAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyAssignment,
  ) {
    const newNamePath = context.namePath.appendingTSNode(node.name);
    const propertyVariableID = this.processPropertyName(context, builder, node.name);
    const propertyInitVariableID = this.processExpression(context.withNamePath(newNamePath), builder, node.initializer);

    return { propertyVariableID: propertyVariableID, propertyInitVariableID: propertyInitVariableID };
  }

  processPropertyName(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyName,
  ): NativeCompilerBuilderAtomID | NativeCompilerBuilderVariableID {
    if (ts.isIdentifier(node)) {
      return this.processIdentifierAsAtom(builder, node);
    } else if (ts.isStringLiteralLike(node)) {
      return this.processStringLiteral(builder, node as ts.StringLiteral);
    } else if (ts.isComputedPropertyName(node)) {
      return this.processExpression(context, builder, node.expression);
    } else if (ts.isNumericLiteral(node)) {
      return this.processNumericalLiteral(builder, node);
    } else {
      this.onError(node, 'Failed to process property name');
    }
  }

  private appendSetPropertyIfNeeded(
    assignmentTracker: AssignmentTracker,
    builder: INativeCompilerBlockBuilder,
    variable: NativeCompilerBuilderVariableID,
  ) {
    const assignments = assignmentTracker.assignments;
    if (!assignments.length) {
      return;
    }

    const lastAssigment = assignments[assignments.length - 1];
    if (lastAssigment.property instanceof NativeCompilerBuilderAtomID) {
      builder.buildSetProperty(lastAssigment.object, lastAssigment.property, variable);
    } else {
      builder.buildSetPropertyValue(lastAssigment.object, lastAssigment.property, variable);
    }
  }

  processPrefixUnaryExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PrefixUnaryExpression,
  ) {
    this.appendNodeDebugInfo(builder, node);
    const assignmentTracker = new AssignmentTracker();
    const operandVariableID = this.processExpression(
      context.withAssignmentTracker(assignmentTracker),
      builder,
      node.operand,
    );
    if (node.operator == ts.SyntaxKind.MinusToken) {
      return builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Neg, operandVariableID);
    } else if (node.operator === ts.SyntaxKind.TildeToken) {
      return builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.BitwiseNot, operandVariableID);
    } else if (node.operator === ts.SyntaxKind.MinusMinusToken) {
      const resultVariable = builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Dec, operandVariableID);
      builder.buildAssignment(operandVariableID, resultVariable);
      this.appendSetPropertyIfNeeded(assignmentTracker, builder, resultVariable);
      return resultVariable;
    } else if (node.operator === ts.SyntaxKind.PlusPlusToken) {
      const resultVariable = builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Inc, operandVariableID);
      builder.buildAssignment(operandVariableID, resultVariable);
      this.appendSetPropertyIfNeeded(assignmentTracker, builder, resultVariable);
      return resultVariable;
    } else if (node.operator === ts.SyntaxKind.PlusToken) {
      return builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Plus, operandVariableID);
    } else if (node.operator === ts.SyntaxKind.ExclamationToken) {
      return builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.LogicalNot, operandVariableID);
    } else {
      this.onError(node, 'prefixUnary operator is not supported');
    }
  }

  private hasSpreadElement(args: ts.NodeArray<ts.Expression>): boolean {
    for (const arg of args) {
      if (ts.isSpreadElement(arg)) {
        return true;
      }
    }

    return false;
  }

  private doProcessCall(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
    fn: NativeCompilerVariableIDWithParent,
    args: ts.NodeArray<ts.Expression>,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    let retval;
    if (this.hasSpreadElement(args)) {
      const arrayArgument = this.buildArrayWithElements(context, builder, args);

      if (isSuperCall(node)) {
        retval = builder.buildConstructorInvocationWithArgsArray(fn.variable, builder.buildNewTarget(), arrayArgument);
        builder.buildAssignment(this.processThis(context, builder, node), retval);
      } else {
        retval = builder.buildFunctionInvocationWithArgsArray(
          fn.variable,
          arrayArgument,
          fn.parentVariable ?? builder.buildUndefined(),
        );
      }
    } else {
      if (isSuperCall(node)) {
        const inputArgs = args.map((arg) => this.processExpression(context, builder, arg));
        retval = builder.buildConstructorInvocation(fn.variable, builder.buildNewTarget(), inputArgs);
        builder.buildAssignment(this.processThis(context, builder, node), retval);
      } else {
        const inputArgs = args.map((arg) => this.processExpression(context, builder, arg));
        retval = builder.buildFunctionInvocation(fn.variable, inputArgs, fn.parentVariable ?? builder.buildUndefined());
      }
    }
    return retval;
  }

  private processCallExpressionEpilogue(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.CallExpression,
    callee: NativeCompilerVariableIDWithParent,
  ): NativeCompilerBuilderVariableID {
    if (node.questionDotToken) {
      return this.buildQuestionDotExpression(context, builder, callee.variable, (context, builder) => {
        return this.doProcessCall(context, builder, node, callee, node.arguments);
      });
    } else {
      return this.doProcessCall(context, builder, node, callee, node.arguments);
    }
  }

  processCallExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.CallExpression,
  ): NativeCompilerBuilderVariableID {
    if (node.expression.kind === ts.SyntaxKind.ImportKeyword) {
      // Import statement.
      if (node.arguments.length !== 1) {
        this.onError(node, 'Expected 1 argument for import expression');
      }

      // Import module
      const importedModuleVariable = this.doImportModule(context, builder, node.expression, node.arguments[0]);

      // Return result of Promise.resolve(<imported module>)
      const promiseCtor = this.getGlobalVariable(context, builder, 'Promise');
      const resolveFn = builder.buildGetProperty(
        promiseCtor.variable,
        this.processIdentifierStringAsAtom(builder, 'resolve'),
        promiseCtor.variable,
      );
      return builder.buildFunctionInvocation(resolveFn, [importedModuleVariable], promiseCtor.variable);
    }

    if (ts.isPropertyAccessExpression(node.expression)) {
      // try shortcut builtin functions (e.g. Math.pow)
      if (this.options.enableIntrinsics && ts.isIdentifier(node.expression.expression)) {
        const intrinsic = node.expression.expression.text + '.' + node.expression.name.text;
        if (builder.hasIntrinsicFunction(intrinsic)) {
          const args = node.arguments.map((a) => this.processExpression(context, builder, a));
          return builder.buildIntrinsicCall(intrinsic, args);
        }
      }

      return this.processPropertyAccessExpressionForCallExpression(
        context,
        builder,
        node.expression,
        (context, builder, callee) => {
          return this.processCallExpressionEpilogue(context, builder, node, callee);
        },
      );
    } else {
      const variable = this.processExpression(context, builder, node.expression);
      const callee: NativeCompilerVariableIDWithParent = {
        variable,
        parentVariable: this.resolveVariableToUseAsThis(context, builder, variable, node.expression),
      };

      return this.processCallExpressionEpilogue(context, builder, node, callee);
    }
  }

  processDeleteExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.DeleteExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);

    const expression = node.expression;
    if (ts.isElementAccessExpression(expression)) {
      let expressionVariable = this.processExpression(context, builder, expression.expression);
      let argumentExpressionVariable = this.processExpression(context, builder, expression.argumentExpression);

      return builder.buildDeletePropertyValue(expressionVariable, argumentExpressionVariable);
    } else if (ts.isPropertyAccessExpression(expression)) {
      return this.doProcessPropertyAccessExpression(
        context,
        builder,
        expression,
        (context, builder, expressionVariable, nameVariable) => {
          return builder.buildDeleteProperty(expressionVariable, nameVariable);
        },
      );
    } else {
      this.onError(expression, 'Expecting element or property access expression');
    }
  }

  processAwaitExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.AwaitExpression,
  ): NativeCompilerBuilderVariableID {
    let retVal = this.processExpression(context, builder, node.expression);
    if (context.isAsync && context.isGenerator) {
      const awaitRet = builder.buildNewObject();
      builder.buildSetProperty(awaitRet, this.moduleBuilder.registerAtom('await'), retVal);
      retVal = awaitRet;
    }
    return this.processSuspendResume(context, builder, retVal);
  }

  private processYieldStarExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.YieldExpression,
  ): NativeCompilerBuilderVariableID {
    if (node.expression === undefined) {
      this.onError(node, 'Expecting expression after yield*');
    }
    const helper = this.getGlobalVariable(
      context,
      builder,
      context.isAsync ? '__tsn_get_async_iterator' : '__tsn_get_iterator',
    ).variable;
    const builderLoop = builder.buildLoop();
    const iterator = builderLoop.initBuilder.buildFunctionInvocation(
      helper,
      [this.processExpression(context, builderLoop.initBuilder, node.expression)],
      builderLoop.initBuilder.buildUndefined(),
    );
    const nextFunc = builderLoop.condBuilder.buildGetProperty(
      iterator,
      this.moduleBuilder.registerAtom('next'),
      iterator,
    );
    let iteratorResult;
    if (context.isAsync) {
      const promise = builderLoop.condBuilder.buildFunctionInvocation(nextFunc, [], iterator);
      const awaitable = builderLoop.condBuilder.buildNewObject();
      builderLoop.condBuilder.buildSetProperty(awaitable, this.moduleBuilder.registerAtom('await'), promise);
      iteratorResult = this.processSuspendResume(context, builderLoop.condBuilder, awaitable);
    } else {
      iteratorResult = builderLoop.condBuilder.buildFunctionInvocation(nextFunc, [], iterator);
    }
    builderLoop.condBuilder.buildBranch(
      builderLoop.condBuilder.buildGetProperty(iteratorResult, this.moduleBuilder.registerAtom('done'), iteratorResult),
      NativeCompilerBuilderBranchType.Truthy,
      builderLoop.exitTarget,
      builderLoop.bodyJumpTargetBuilder.target,
    );
    const iteratorValue = builderLoop.bodyJumpTargetBuilder.builder.buildGetProperty(
      iteratorResult,
      this.moduleBuilder.registerAtom('value'),
      iteratorResult,
    );
    if (context.isAsync) {
      const yieldRet = builderLoop.bodyJumpTargetBuilder.builder.buildNewObject();
      builderLoop.bodyJumpTargetBuilder.builder.buildSetProperty(
        yieldRet,
        this.moduleBuilder.registerAtom('yield'),
        iteratorValue,
      );
      return this.processSuspendResume(context, builderLoop.bodyJumpTargetBuilder.builder, yieldRet);
    } else {
      return this.processSuspendResume(context, builderLoop.bodyJumpTargetBuilder.builder, iteratorValue);
    }
  }

  processYieldExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.YieldExpression,
  ): NativeCompilerBuilderVariableID {
    if (node.asteriskToken) {
      return this.processYieldStarExpression(context, builder, node);
    }
    let retVal = node.expression ? this.processExpression(context, builder, node.expression) : builder.buildUndefined();
    if (context.isAsync && context.isGenerator) {
      const yieldRet = builder.buildNewObject();
      builder.buildSetProperty(yieldRet, this.moduleBuilder.registerAtom('yield'), retVal);
      retVal = yieldRet;
    }
    return this.processSuspendResume(context, builder, retVal);
  }

  private processSuspendResume(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    retVal: NativeCompilerBuilderVariableID,
  ) {
    // build a suspend return
    const suspendBlock = builder.buildSubBuilder(false);
    const rp = builder.buildJumpTarget('resume_point');
    suspendBlock.buildSuspend(retVal, rp.target);
    // and resume with arg
    return builder.buildResume();
  }

  processJsxExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.JsxExpression,
  ): NativeCompilerBuilderVariableID {
    if (node.expression) {
      return this.processExpression(context, builder, node.expression);
    } else {
      return builder.buildUndefined();
    }
  }

  processExpressionStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ExpressionStatement,
  ) {
    if (ts.isBinaryExpression(node.expression)) {
      this.processBinaryExpression(context, builder, node.expression);
    } else {
      this.processExpression(context, builder, node.expression);
    }
  }

  processElementAccessExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ElementAccessExpression,
  ) {
    this.appendNodeDebugInfo(builder, node);

    const constantValue = this.tryProcessEnumConstantValue(context, builder, node);
    if (constantValue !== undefined) {
      return constantValue;
    }

    const doBuild = (
      context: NativeCompilerContext,
      builder: INativeCompilerBlockBuilder,
      expressionVariable: NativeCompilerBuilderVariableID,
    ) => {
      let argumentExpressionVariable = this.processExpression(context, builder, node.argumentExpression);
      if (context.assignmentTracker) {
        context.assignmentTracker.onGetPropertyValue(expressionVariable, argumentExpressionVariable);
      }
      return builder.buildGetPropertyValue(expressionVariable, argumentExpressionVariable, expressionVariable);
    };

    // use a separate context for the optional chain (call ctor directly to make sure we get a copy)
    const parentExpressionContext = new NativeCompilerContext(
      context.namePath,
      context.assignmentTracker,
      context.emitResolver,
      context.isGenerator,
      context.isAsync,
    );
    if (ts.isOptionalChain(node)) {
      const result = context.optionalChainResult ?? builder.buildAssignableUndefined();
      if (context.optionalChainExit) {
        // parent expression share the whole expression's exit and result
        parentExpressionContext.optionalChainExit = context.optionalChainExit;
        parentExpressionContext.optionalChainResult = context.optionalChainResult;
      } else {
        // start of a new optional chain, create the short circuit jump target
        const optionalChain = builder.buildJumpTarget('optional_chain');
        const optionalChainExit = builder.buildJumpTarget('optional_chain_exit');
        parentExpressionContext.optionalChainExit = optionalChainExit.target;
        parentExpressionContext.optionalChainResult = result;
        builder = optionalChain.builder;
      }
      let expressionVariable = this.processExpression(parentExpressionContext, builder, node.expression);
      parentExpressionContext.optionalChainExit = undefined;
      parentExpressionContext.optionalChainResult = undefined;

      if (node.questionDotToken) {
        return this.buildQuestionDotExpression(context, builder, expressionVariable, (context, builder) => {
          return doBuild(context, builder, expressionVariable);
        });
      } else {
        builder.buildAssignment(result, doBuild(context, builder, expressionVariable));
        return result;
      }
    } else {
      let expressionVariable = this.processExpression(context, builder, node.expression);
      return doBuild(context, builder, expressionVariable);
    }
  }

  processForStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.ForStatement) {
    this.appendNodeDebugInfo(builder, node);

    const builderLoop = builder.buildLoop();

    if (node.initializer) {
      if (ts.isVariableDeclarationList(node.initializer)) {
        this.prepareVariableDeclarationList(builderLoop.initBuilder, node.initializer);
        this.processVariableDeclarationList(context, builderLoop.initBuilder, node.initializer);
      } else {
        this.processExpression(context, builderLoop.initBuilder, node.initializer);
      }
    }

    if (node.condition) {
      let conditionVariable = this.processExpression(context, builderLoop.condBuilder, node.condition);
      builderLoop.condBuilder.buildBranch(
        conditionVariable,
        NativeCompilerBuilderBranchType.Truthy,
        builderLoop.bodyJumpTargetBuilder.target,
        builderLoop.exitTarget,
      );
    }

    this.processStatement(context, builderLoop.bodyJumpTargetBuilder.builder, node.statement);

    if (node.incrementor) {
      this.processExpression(context, builderLoop.bodyJumpTargetBuilder.builder, node.incrementor);
    }
  }

  processIfStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.IfStatement) {
    this.appendNodeDebugInfo(builder, node);
    const condition = builder.buildJumpTarget('condition');
    const conditionVariable = this.processExpression(context, condition.builder, node.expression);

    const trueTarget = builder.buildJumpTarget('true');
    const elseTarget = builder.buildJumpTarget('false');
    const exit = builder.buildJumpTarget('exit');

    this.processStatement(context, trueTarget.builder, node.thenStatement);

    if (node.elseStatement) {
      this.processStatement(context, elseTarget.builder, node.elseStatement);
    }

    condition.builder.buildBranch(
      conditionVariable,
      NativeCompilerBuilderBranchType.Truthy,
      trueTarget.target,
      elseTarget.target,
    );
    trueTarget.builder.buildJump(exit.target);
  }

  processWhileStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.WhileStatement) {
    this.appendNodeDebugInfo(builder, node);

    const builderLoop = builder.buildLoop();

    const conditionVariable = this.processExpression(context, builderLoop.condBuilder, node.expression);
    builderLoop.condBuilder.buildBranch(
      conditionVariable,
      NativeCompilerBuilderBranchType.Truthy,
      builderLoop.bodyJumpTargetBuilder.target,
      builderLoop.exitTarget,
    );

    this.processStatement(context, builderLoop.bodyJumpTargetBuilder.builder, node.statement);
  }

  processPostfixUnaryExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PostfixUnaryExpression,
  ) {
    this.appendNodeDebugInfo(builder, node);
    const assignmentTracker = new AssignmentTracker();
    const operandVariableID = this.processExpression(
      context.withAssignmentTracker(assignmentTracker),
      builder,
      node.operand,
    );
    const copiedVariable = builder.buildCopy(operandVariableID);
    let intermediate: NativeCompilerBuilderVariableID;

    if (node.operator == ts.SyntaxKind.PlusPlusToken) {
      intermediate = builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Inc, operandVariableID);
    } else if (node.operator === ts.SyntaxKind.MinusMinusToken) {
      intermediate = builder.buildUnaryOp(NativeCompilerIR.UnaryOperator.Dec, operandVariableID);
    } else {
      this.onError(node, 'prefixUnary operator is not supported');
    }
    builder.buildAssignment(operandVariableID, intermediate);
    this.appendSetPropertyIfNeeded(assignmentTracker, builder, intermediate);
    return copiedVariable;
  }

  processParenthesizedExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ParenthesizedExpression,
  ) {
    this.appendNodeDebugInfo(builder, node);
    return this.processExpression(context, builder, node.expression);
  }

  processTemplateLiteralToken(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.TemplateLiteralToken,
  ): NativeCompilerBuilderVariableID {
    if (node.rawText) {
      return this.registerAndGetStringLiteralValue(builder, node.rawText);
    } else {
      return this.registerAndGetStringLiteralValue(builder, '');
    }
  }

  processTemplateExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.TemplateExpression,
  ): NativeCompilerBuilderVariableID {
    this.appendNodeDebugInfo(builder, node);
    let stringVariableId = this.processTemplateLiteralToken(context, builder, node.head);

    for (const span of node.templateSpans) {
      const expressionVariable = this.processExpression(context, builder, span.expression);
      stringVariableId = builder.buildBinaryOp(
        NativeCompilerIR.BinaryOperator.Add,
        stringVariableId,
        expressionVariable,
      );

      if (span.literal.rawText) {
        const spanValue = this.processTemplateLiteralToken(context, builder, span.literal);
        stringVariableId = builder.buildBinaryOp(NativeCompilerIR.BinaryOperator.Add, stringVariableId, spanValue);
      }
    }

    return stringVariableId;
  }

  processNonNullExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.NonNullExpression,
  ): NativeCompilerBuilderVariableID {
    return this.processExpression(context, builder, node.expression);
  }

  processVoidExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.VoidExpression,
  ): NativeCompilerBuilderVariableID {
    this.processExpression(context, builder, node.expression);
    return builder.buildUndefined();
  }

  processTypeAssertion(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.TypeAssertion,
  ): NativeCompilerBuilderVariableID {
    return this.processExpression(context, builder, node.expression);
  }

  processExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Expression,
  ): NativeCompilerBuilderVariableID {
    if (node.kind === ts.SyntaxKind.NullKeyword) {
      return this.processNullKeyword(builder, node);
    } else if (node.kind === ts.SyntaxKind.UndefinedKeyword) {
      return this.processUndefinedKeyword(builder, node);
    } else if (node.kind === ts.SyntaxKind.FalseKeyword) {
      return this.processFalseKeyword(builder, node);
    } else if (node.kind === ts.SyntaxKind.TrueKeyword) {
      return this.procssTrueKeyword(builder, node);
    } else if (node.kind === ts.SyntaxKind.ThisKeyword) {
      return this.processThis(context, builder, node as ts.ThisExpression);
    } else if (node.kind === ts.SyntaxKind.SuperKeyword) {
      return this.processSuperKeyword(context, builder, node);
    } else if (ts.isIdentifier(node)) {
      return this.processIdentifier(context, builder, node);
    } else if (ts.isNumericLiteral(node)) {
      return this.processNumericalLiteral(builder, node);
    } else if (ts.isStringLiteral(node)) {
      return this.processStringLiteral(builder, node);
    } else if (ts.isPropertyAccessExpression(node)) {
      return this.processPropertyAccessExpression(context, builder, node);
    } else if (ts.isBinaryExpression(node)) {
      return this.processBinaryExpression(context, builder, node);
    } else if (ts.isObjectLiteralExpression(node)) {
      return this.processObjectLiteralExpression(context, builder, node);
    } else if (ts.isPrefixUnaryExpression(node)) {
      return this.processPrefixUnaryExpression(context, builder, node);
    } else if (ts.isCallExpression(node)) {
      return this.processCallExpression(context, builder, node);
    } else if (ts.isElementAccessExpression(node)) {
      return this.processElementAccessExpression(context, builder, node);
    } else if (ts.isPostfixUnaryExpression(node)) {
      return this.processPostfixUnaryExpression(context, builder, node);
    } else if (ts.isParenthesizedExpression(node)) {
      return this.processParenthesizedExpression(context, builder, node);
    } else if (ts.isArrowFunction(node)) {
      return this.processArrowFunction(context, builder, node);
    } else if (ts.isArrayLiteralExpression(node)) {
      return this.processArrayLiteralExpression(context, builder, node);
    } else if (ts.isNewExpression(node)) {
      return this.processNewExpression(context, builder, node);
    } else if (ts.isFunctionExpression(node)) {
      return this.processFunctionExpression(context, builder, node);
    } else if (ts.isConditionalExpression(node)) {
      return this.processConditionalExpression(context, builder, node);
    } else if (ts.isAsExpression(node)) {
      return this.processAsExpression(context, builder, node);
    } else if (ts.isTypeOfExpression(node)) {
      return this.processTypeOfExpression(context, builder, node);
    } else if (ts.isRegularExpressionLiteral(node)) {
      return this.processRegularExpressionLiteral(context, builder, node);
    } else if (ts.isTemplateLiteralToken(node)) {
      return this.processTemplateLiteralToken(context, builder, node);
    } else if (ts.isTemplateExpression(node)) {
      return this.processTemplateExpression(context, builder, node);
    } else if (ts.isNonNullExpression(node)) {
      return this.processNonNullExpression(context, builder, node);
    } else if (ts.isClassExpression(node)) {
      return this.processClassExpression(context, builder, node);
    } else if (ts.isVoidExpression(node)) {
      return this.processVoidExpression(context, builder, node);
    } else if (ts.isTypeAssertionExpression(node)) {
      return this.processTypeAssertion(context, builder, node);
    } else if (ts.isDeleteExpression(node)) {
      return this.processDeleteExpression(context, builder, node);
    } else if (ts.isAwaitExpression(node)) {
      return this.processAwaitExpression(context, builder, node);
    } else if (ts.isYieldExpression(node)) {
      return this.processYieldExpression(context, builder, node);
    } else if (ts.isJsxExpression(node)) {
      return this.processJsxExpression(context, builder, node);
    } else if (
      ts.isMetaProperty(node) &&
      node.keywordToken === ts.SyntaxKind.NewKeyword &&
      node.name.text === 'target'
    ) {
      // new.target
      return builder.buildNewTarget();
    } else {
      this.onError(node, 'Expression not supported');
    }
  }

  processClassLikeDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ClassLikeDeclaration,
    isExpression: boolean,
  ): NativeCompilerBuilderVariableID {
    let parentClass: NativeCompilerBuilderVariableID | undefined;
    if (node.heritageClauses) {
      for (const heritageClause of node.heritageClauses) {
        if (heritageClause.token === ts.SyntaxKind.ExtendsKeyword) {
          if (parentClass || heritageClause.types.length !== 1) {
            this.onError(heritageClause, `Found more than one inherited type`);
          }

          const inheritedType = heritageClause.types[0];
          parentClass = this.processExpression(context, builder, inheritedType.expression);
        }
      }
    }

    let ctorDeclaration: ts.ConstructorDeclaration | undefined;
    const propertiesToInitialize: [Boolean, ts.PropertyDeclaration][] = [];
    const pendingClassElementsToProcess: [boolean, ts.ClassElement][] = [];

    for (const classElement of node.members) {
      const modifiers = ts.getCombinedModifierFlags(classElement);
      if (modifiers & ts.ModifierFlags.Abstract) {
        continue;
      }

      const isStatic = (modifiers & ts.ModifierFlags.Static) != 0;

      if (ts.isConstructorDeclaration(classElement)) {
        ctorDeclaration = classElement;
        continue;
      }
      if (ts.isPropertyDeclaration(classElement)) {
        if (classElement.initializer) {
          propertiesToInitialize.push([isStatic, classElement]);
        }
        continue;
      }

      pendingClassElementsToProcess.push([isStatic, classElement]);
    }

    this.appendNodeDebugInfo(builder, node);

    let methodNamePath = context.namePath;

    const constructorVariableID = this.doProcessFunctionDeclaration(
      context,
      builder,
      node.name,
      undefined,
      false,
      true,
      isExpression,
      parentClass,
      (context, builder) => {
        methodNamePath = context.namePath;

        let ctorStatements: ts.Statement[] = [];
        if (ctorDeclaration && ctorDeclaration.body) {
          const ctorBodyStatements = ctorDeclaration.body.statements;
          for (const ctorBodyStatement of ctorBodyStatements) {
            ctorStatements.push(ctorBodyStatement);
          }
        }

        const propertyInitializers: Array<
          (context: NativeCompilerContext, builder: INativeCompilerBlockBuilder) => void
        > = [];

        // ctor parameters
        if (ctorDeclaration) {
          this.processParameterDeclarations(
            context,
            builder,
            ctorDeclaration.parameters,
            ctorDeclaration,
            false,
            (deferredStatement) => {
              propertyInitializers.push(deferredStatement);
            },
          );
        }
        // Properties
        for (const [isStatic, propertyDeclaration] of propertiesToInitialize) {
          if (!isStatic) {
            propertyInitializers.push((context, builder) => {
              this.processPropertyDeclaration(
                context,
                builder,
                propertyDeclaration,
                this.processThis(context, builder, propertyDeclaration),
              );
            });
          }
        }

        // There is no super call in the constructor
        if (!ctorStatements.some(statementHasSuperCall)) {
          let thisValue;
          if (parentClass) {
            // class has base but no super call
            // synthetize a super() call
            const superCtor = builder.buildGetSuperConstructor();
            thisValue = builder.buildConstructorInvocationWithArgsArray(
              superCtor,
              builder.buildNewTarget(),
              builder.buildGetFunctionArgumentsObject(0),
            );
          } else {
            // no base class
            // create empty object from prototype
            thisValue = builder.buildConstructorInvocation(
              this.processUndefinedKeyword(builder, node),
              builder.buildNewTarget(),
              [],
            );
          }
          // init the 'this' reference
          builder.buildAssignment(this.processThis(context, builder, node), thisValue);
          // init members
          for (const initializer of propertyInitializers) {
            initializer(context, builder);
          }
        }

        const ctorContext = context.withNamePath(context.namePath.appending(CONSTRUCTOR_NAME));

        // Process the body of the constructor method
        this.processStatementsInBlock(ctorContext, builder, ctorStatements, (statement) => {
          // Run initializers after processing a top level super call
          // (non-toplevel super calls cannot have initialized properties, so no need to worry aobut them)
          if (getSuperCallFromStatement(statement) !== undefined) {
            for (const initializer of propertyInitializers) {
              initializer(context, builder);
            }
          }
        });

        builder.buildReturn(this.processThis(context, builder, node));
        return ctorDeclaration?.parameters.length ?? 0;
      },
    );

    const methodContext = context.withNamePath(methodNamePath);

    // Append method and property getters and setters
    for (const [isStatic, classElement] of pendingClassElementsToProcess) {
      if (ts.isGetAccessorDeclaration(classElement)) {
        this.processGetAccessorDeclaration(methodContext, builder, classElement, constructorVariableID, isStatic);
      } else if (ts.isSetAccessorDeclaration(classElement)) {
        this.processSetAccessorDeclaration(methodContext, builder, classElement, constructorVariableID, isStatic);
      } else if (ts.isMethodDeclaration(classElement)) {
        this.processMethodDeclaration(methodContext, builder, classElement, constructorVariableID, isStatic);
      } else if (ts.isSemicolonClassElement(classElement)) {
        // semicolon is allowed in class but does nothing
      } else {
        this.onError(classElement, 'Unrecognized class element type');
      }
    }

    this.appendExportedDeclarationIfNeeded(context, builder, node, node.name, constructorVariableID);

    // Append static properties

    for (const [isStatic, propertyDeclaration] of propertiesToInitialize) {
      if (isStatic) {
        this.processPropertyDeclaration(methodContext, builder, propertyDeclaration, constructorVariableID);
      }
    }

    return constructorVariableID;
  }

  processClassDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ClassDeclaration,
  ): void {
    this.processClassLikeDeclaration(context, builder, node, false);
  }

  processClassExpression(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ClassExpression,
  ): NativeCompilerBuilderVariableID {
    return this.processClassLikeDeclaration(context, builder, node, true);
  }

  private processClassElement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.MethodDeclaration | ts.GetAccessorDeclaration | ts.SetAccessorDeclaration,
    type: NativeCompilerIR.ClassElementType,
    namePathPrefix: string | undefined,
    classVariable: NativeCompilerBuilderVariableID,
    isStatic: boolean,
    name: ts.PropertyName,
  ): void {
    if (!node.body) {
      return;
    }

    const propertyName = this.processPropertyName(context, builder, name);

    this.appendNodeDebugInfo(builder, node);

    const functionVariableID = this.doProcessFunctionDeclaration(
      context,
      builder,
      ts.isIdentifier(name) ? name : undefined,
      namePathPrefix,
      true,
      false,
      false,
      undefined,
      (context, builder) => {
        this.processParameterDeclarations(context, builder, node.parameters, node, false, undefined);
        if (node.body) {
          const isGenerator = !!node.asteriskToken;
          const isAsync = !!node.modifiers?.some((m) => m.kind == ts.SyntaxKind.AsyncKeyword);
          this.processFunctionBody(context, builder, node.body, isGenerator, isAsync);
        }
        return node.parameters.length;
      },
    );

    if (propertyName instanceof NativeCompilerBuilderVariableID) {
      builder.buildSetClassElementValue(type, classVariable, propertyName, functionVariableID, isStatic);
    } else {
      builder.buildSetClassElement(type, classVariable, propertyName, functionVariableID, isStatic);
    }
  }

  private processMethodDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.MethodDeclaration,
    classVariable: NativeCompilerBuilderVariableID,
    isStatic: boolean,
  ) {
    this.processClassElement(
      context,
      builder,
      node,
      NativeCompilerIR.ClassElementType.Method,
      undefined,
      classVariable,
      isStatic,
      node.name,
    );
  }

  private processGetAccessorDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.GetAccessorDeclaration,
    classVariable: NativeCompilerBuilderVariableID,
    isStatic: boolean,
  ): void {
    this.processClassElement(
      context,
      builder,
      node,
      NativeCompilerIR.ClassElementType.Getter,
      '_get__',
      classVariable,
      isStatic,
      node.name,
    );
  }

  private processSetAccessorDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.SetAccessorDeclaration,
    classVariable: NativeCompilerBuilderVariableID,
    isStatic: boolean,
  ): void {
    this.processClassElement(
      context,
      builder,
      node,
      NativeCompilerIR.ClassElementType.Setter,
      '_set__',
      classVariable,
      isStatic,
      node.name,
    );
  }

  private processPropertyDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.PropertyDeclaration,
    thisVariable: NativeCompilerBuilderVariableID,
  ): void {
    if (!node.initializer) {
      // Nothing to do for a property that is declared but is not initialized
      return;
    }

    this.appendNodeDebugInfo(builder, node);

    const propertyName = node.name;
    const nameVariable = this.processPropertyName(context, builder, propertyName);

    const newNamePath = context.namePath.appendingTSNode(propertyName);
    const expressionVariable = this.processExpression(context.withNamePath(newNamePath), builder, node.initializer);

    this.doBuildSetProperty(context, builder, nameVariable, thisVariable, expressionVariable);
  }

  private doBuildSetProperty(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    propertyName: NativeCompilerBuilderAtomID | NativeCompilerBuilderVariableID,
    thisVariable: NativeCompilerBuilderVariableID,
    valueVariable: NativeCompilerBuilderVariableID,
  ) {
    if (propertyName instanceof NativeCompilerBuilderAtomID) {
      builder.buildSetProperty(thisVariable, propertyName, valueVariable);
    } else {
      builder.buildSetPropertyValue(thisVariable, propertyName, valueVariable);
    }
  }

  private appendNodeDebugInfo(builder: INativeCompilerBlockBuilder, node: ts.Node) {
    const sourceFile = node.getSourceFile();
    if (!sourceFile) {
      return;
    }

    const lineAndCharacter = sourceFile.getLineAndCharacterOfPosition(node.getStart());
    let nodeComments = node.getText().trim();
    if (nodeComments.length > MAX_DEBUG_INFO_LENGTH) {
      nodeComments = nodeComments.substring(0, MAX_DEBUG_INFO_LENGTH) + '\n[...]';
    }

    let comments = `At line ${lineAndCharacter.line + 1}:${lineAndCharacter.character}\n` + nodeComments + '\n';
    builder.buildComments(comments);

    builder.buildProgramCounterInfo(lineAndCharacter.line + 1, lineAndCharacter.character);
  }

  processEnumDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.EnumDeclaration,
  ) {
    // TODO(simon): Const enum
    this.appendNodeDebugInfo(builder, node);
    const enumName = this.processIdentifierAsAtom(builder, node.name);
    const enumObject = builder.buildNewObject();
    const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, node.name);
    builder.buildSetVariableRef(variableRef, enumObject);

    this.appendExportedDeclarationIfNeeded(context, builder, node, node.name, enumObject);

    let enumValueSequence = 0;
    for (const enumMember of node.members) {
      this.appendNodeDebugInfo(builder, enumMember);
      const enumName = this.processPropertyName(context, builder, enumMember.name);
      let enumValue: NativeCompilerBuilderVariableID;
      if (enumMember.initializer) {
        const constantValue = this.tryProcessEnumConstantValuePrologue(context, builder, enumMember);
        if (constantValue !== undefined) {
          enumValue = this.processEnumConstantValueEpilogue(context, builder, constantValue);
          if (typeof constantValue === 'number') {
            enumValueSequence = Math.floor(constantValue) + 1;
          }
        } else {
          const enumInitializer = enumMember.initializer;
          enumValue = this.processExpression(context, builder, enumInitializer);
          if (ts.isNumericLiteral(enumInitializer)) {
            enumValueSequence = Number.parseInt(enumInitializer.text) + 1;
          }
        }
      } else {
        enumValue = builder.buildLiteralInteger(enumValueSequence.toString());
        enumValueSequence++;
      }
      // TODO: set_properties
      this.doBuildSetProperty(context, builder, enumName, enumObject, enumValue);
    }
  }

  private appendExportedVariable(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
    exportedName: string,
    isDefaultExport: boolean,
    variable: NativeCompilerBuilderVariableID,
  ) {
    const exportsVariable = builder.resolveExportsVariable();

    if (isDefaultExport) {
      const exportedNameAtom = this.processIdentifierStringAsAtom(builder, 'default');

      builder.buildSetProperty(exportsVariable, exportedNameAtom, variable);
    } else {
      const exportedNameAtom = this.processIdentifierStringAsAtom(builder, exportedName);

      builder.buildSetProperty(exportsVariable, exportedNameAtom, variable);
    }
  }

  private isExportedDeclaration(node: ts.Declaration): boolean {
    const modifiers = ts.getCombinedModifierFlags(node);
    if ((modifiers & ts.ModifierFlags.ExportDefault) === ts.ModifierFlags.ExportDefault) {
      this.onError(node, 'Default exports not yet supported');
    }
    return (modifiers & ts.ModifierFlags.Export) !== 0;
  }

  private appendExportedDeclarationIfNeeded(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Declaration,
    declarationName: ts.Identifier | undefined,
    variable: NativeCompilerBuilderVariableID,
  ): boolean {
    const modifiers = ts.getCombinedModifierFlags(node);
    const isDefaultExport = (modifiers & ts.ModifierFlags.ExportDefault) === ts.ModifierFlags.ExportDefault;

    if ((modifiers & ts.ModifierFlags.Export) === 0) {
      return false;
    }

    if (!declarationName) {
      this.onError(node, 'Expected declaration name');
    }

    const declarationNameString = declarationName.text;

    this.appendExportedVariable(context, builder, node, declarationNameString, isDefaultExport, variable);

    return true;
  }

  private isNodeResolvingToValue(context: NativeCompilerContext, node: ts.Node): boolean {
    if (!context.emitResolver) {
      return true;
    }

    return context.emitResolver.isValueAliasDeclaration(node);
  }

  processExportDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ExportDeclaration,
  ) {
    if (node.isTypeOnly) {
      return;
    }
    if (!node.exportClause) {
      this.onError(node, 'Unsupported export statement type');
    }

    if (ts.isNamedExports(node.exportClause)) {
      let importedModuleVariable: NativeCompilerBuilderVariableID | undefined;

      for (const exportSpecifier of node.exportClause.elements) {
        if (exportSpecifier.isTypeOnly || !this.isNodeResolvingToValue(context, exportSpecifier)) {
          continue;
        }

        const exportedVariableName = exportSpecifier.name.text;
        let variableName: string;
        if (exportSpecifier.propertyName) {
          variableName = exportSpecifier.propertyName.text;
        } else {
          variableName = exportedVariableName;
        }

        if (node.moduleSpecifier && !importedModuleVariable) {
          importedModuleVariable = this.doImportModule(context, builder, node, node.moduleSpecifier);
        }

        let resolvedVar: NativeCompilerBuilderVariableID | undefined;
        if (importedModuleVariable) {
          const exportedVariableAtom = this.processIdentifierStringAsAtom(builder, variableName);
          resolvedVar = builder.buildGetProperty(importedModuleVariable, exportedVariableAtom, importedModuleVariable);
        } else {
          resolvedVar = builder.resolveVariableInScope(variableName);
          if (resolvedVar === undefined) {
            this.onError(exportSpecifier, `Could not resolve exported variable '${variableName}' `);
          }
        }

        this.appendExportedVariable(context, builder, exportSpecifier, exportedVariableName, false, resolvedVar);
      }
    } else {
      this.onError(node, 'Unsupported export clause');
    }
  }

  private doImportModule(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
    modulePathExpression: ts.Expression,
  ) {
    const requireVariable = this.resolveValueDelegate(context, builder, 'require', node).getValue().variable;
    const modulePathString = this.processExpression(context, builder, modulePathExpression);

    return builder.buildFunctionInvocation(requireVariable, [modulePathString], builder.buildUndefined());
  }

  private processImportModule(
    context: NativeCompilerContext,
    importModuleBuilder: INativeCompilerBlockBuilder,
    builder: INativeCompilerBlockBuilder,
    node: ts.Node,
    modulePathExpression: ts.Expression,
    variableRefName: string,
  ): NativeCompilerBuilderVariableID {
    const variableRef = this.resolveRegisteredVariableRefForIdentifierName(
      context,
      importModuleBuilder,
      node,
      variableRefName,
    );
    // Emit the require() call the first time we actually use a symbol from the module
    if (!this.loadedModules.has(variableRefName)) {
      this.loadedModules.add(variableRefName);

      const loadedModuleVariable = this.doImportModule(context, importModuleBuilder, node, modulePathExpression);
      importModuleBuilder.buildSetVariableRef(variableRef, loadedModuleVariable);
    }

    const outputVariable = builder.resolveVariableInScope(variableRefName);
    if (!outputVariable) {
      throw new Error('Could not resolve imported module variable');
    }

    return outputVariable;
  }

  private processForAwaitOfStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ForOfStatement,
  ): void {
    this.appendNodeDebugInfo(builder, node);
    const builderLoop = builder.buildLoop();
    const helper = this.getGlobalVariable(context, builderLoop.initBuilder, '__tsn_get_async_iterator').variable;
    const iterator = builderLoop.initBuilder.buildFunctionInvocation(
      helper,
      [this.processExpression(context, builderLoop.initBuilder, node.expression)],
      builderLoop.initBuilder.buildUndefined(),
    );
    const nextFunc = builderLoop.condBuilder.buildGetProperty(
      iterator,
      this.moduleBuilder.registerAtom('next'),
      iterator,
    );
    const promise = builderLoop.condBuilder.buildFunctionInvocation(nextFunc, [], iterator);
    const suspendBlock = builderLoop.condBuilder.buildSubBuilder(false);
    const rp = builderLoop.condBuilder.buildJumpTarget('resume_point');
    suspendBlock.buildSuspend(promise, rp.target);
    const iteratorResult = builderLoop.condBuilder.buildResume();
    const iteratorValue = builderLoop.condBuilder.buildGetProperty(
      iteratorResult,
      this.moduleBuilder.registerAtom('value'),
      iteratorResult,
    );
    builderLoop.condBuilder.buildBranch(
      builderLoop.condBuilder.buildGetProperty(iteratorResult, this.moduleBuilder.registerAtom('done'), iteratorResult),
      NativeCompilerBuilderBranchType.Truthy,
      builderLoop.exitTarget,
      builderLoop.bodyJumpTargetBuilder.target,
    );
    if (ts.isVariableDeclarationList(node.initializer)) {
      const variableDeclarations = node.initializer.declarations;
      if (variableDeclarations.length !== 1) {
        this.onError(node.initializer, 'Expecting only one variable declaration inside "for of" or "for in" statement');
      }

      this.prepareVariableDeclarationList(builderLoop.bodyJumpTargetBuilder.builder, node.initializer);

      this.processVariableDeclaration(
        context,
        builderLoop.bodyJumpTargetBuilder.builder,
        variableDeclarations[0],
        (context) => {
          return iteratorValue;
        },
      );
    } else {
      if (ts.isIdentifier(node.initializer)) {
        // no-op
      } else {
        this.onError(
          node.initializer,
          'Expecting variable declaration list or identifier in "for of" or "for in" statement',
        );
      }
    }

    this.processStatement(context, builderLoop.bodyJumpTargetBuilder.builder, node.statement);
  }

  private processForInOrOfStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ForOfStatement | ts.ForInStatement,
    isForOf: boolean,
  ): void {
    this.appendNodeDebugInfo(builder, node);

    const builderLoop = builder.buildLoop();

    const iteratorArgument = this.processExpression(context, builderLoop.initBuilder, node.expression);
    const iterator = isForOf
      ? builderLoop.initBuilder.buildIterator(iteratorArgument)
      : builderLoop.initBuilder.buildKeysIterator(iteratorArgument);
    const iteratorValue = isForOf
      ? builderLoop.condBuilder.buildIteratorNext(iterator, NativeCompilerBuilderVariableType.Object)
      : builderLoop.condBuilder.buildKeysIteratorNext(iterator, NativeCompilerBuilderVariableType.Object);

    builderLoop.condBuilder.buildBranch(
      iterator,
      NativeCompilerBuilderBranchType.Truthy,
      builderLoop.bodyJumpTargetBuilder.target,
      builderLoop.exitTarget,
    );

    if (ts.isVariableDeclarationList(node.initializer)) {
      const variableDeclarations = node.initializer.declarations;
      if (variableDeclarations.length !== 1) {
        this.onError(node.initializer, 'Expecting only one variable declaration inside "for of" or "for in" statement');
      }

      this.prepareVariableDeclarationList(builderLoop.bodyJumpTargetBuilder.builder, node.initializer);

      this.processVariableDeclaration(
        context,
        builderLoop.bodyJumpTargetBuilder.builder,
        variableDeclarations[0],
        (context) => {
          return iteratorValue;
        },
      );
    } else {
      if (ts.isIdentifier(node.initializer)) {
        // no-op
      } else {
        this.onError(
          node.initializer,
          'Expecting variable declaration list or identifier in "for of" or "for in" statement',
        );
      }
    }

    this.processStatement(context, builderLoop.bodyJumpTargetBuilder.builder, node.statement);
  }

  processForOfStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ForOfStatement,
  ): void {
    if (node.awaitModifier) {
      this.processForAwaitOfStatement(context, builder, node);
    } else {
      this.processForInOrOfStatement(context, builder, node, true);
    }
  }

  processForInStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ForInStatement,
  ): void {
    this.processForInOrOfStatement(context, builder, node, false);
  }

  processImportDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ImportDeclaration,
  ) {
    if (!node.importClause) {
      return;
    }

    if (node.importClause.isTypeOnly) {
      return;
    }

    const importVariableRefName = this.getVariableRefNameForImportedModule(node.moduleSpecifier);
    const importModuleBuilder = builder.buildSubBuilder(false);

    if (node.importClause.name) {
      builder.registerVariableDelegate(node.importClause.name.text, {
        onGetValue: (builder) => {
          const importedModuleVariable = this.processImportModule(
            context,
            importModuleBuilder,
            builder,
            node,
            node.moduleSpecifier,
            importVariableRefName,
          );

          return builder.buildGetProperty(
            importedModuleVariable,
            this.processIdentifierStringAsAtom(builder, 'default'),
            importedModuleVariable,
          );
        },
      });
    }

    if (node.importClause.namedBindings) {
      if (ts.isNamedImports(node.importClause.namedBindings)) {
        for (const importSpecifier of node.importClause.namedBindings.elements) {
          if (importSpecifier.isTypeOnly) {
            continue;
          }

          const variableName = importSpecifier.name;

          builder.registerVariableDelegate(variableName.text, {
            onGetValue: (builder) => {
              const importedModuleVariable = this.processImportModule(
                context,
                importModuleBuilder,
                builder,
                node,
                node.moduleSpecifier,
                importVariableRefName,
              );

              let importedVariableName: NativeCompilerBuilderAtomID;
              if (importSpecifier.propertyName) {
                importedVariableName = this.processIdentifierAsAtom(builder, importSpecifier.propertyName);
              } else {
                importedVariableName = this.processIdentifierAsAtom(builder, variableName);
              }

              return builder.buildGetProperty(importedModuleVariable, importedVariableName, importedModuleVariable);
            },
          });
        }
      } else if (ts.isNamespaceImport(node.importClause.namedBindings)) {
        const variableName = node.importClause.namedBindings.name;

        builder.registerVariableDelegate(variableName.text, {
          onGetValue: (builder) => {
            return this.processImportModule(
              context,
              importModuleBuilder,
              builder,
              node,
              node.moduleSpecifier,
              importVariableRefName,
            );
          },
        });
      } else {
        this.onError(node, 'Unsupported import clause');
      }
    }
  }

  processModuleDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ModuleDeclaration,
  ): void {
    if (!ts.isIdentifier(node.name)) {
      this.onError(node, 'Module declaration named with strings are not yet supported');
    }

    const moduleName = node.name;

    const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, moduleName);
    const moduleVariableId = builder.buildNewObject();
    builder.buildSetVariableRef(variableRef, moduleVariableId);
    this.appendExportedDeclarationIfNeeded(context, builder, node, node.name, moduleVariableId);

    if (node.body) {
      if (ts.isModuleBlock(node.body)) {
        const moduleBlock = node.body as ts.ModuleBlock;
        const namespaceBlockBuilder = builder.buildSubBuilder(true);
        // Any exports now should refer to the namespace instead of the 'exports' of the module
        namespaceBlockBuilder.registerExportsVariableRef(variableRef);
        this.processStatementsInBlock(context, namespaceBlockBuilder, moduleBlock.statements);
      } else {
        this.onError(node.body, `Only module blocks are currently supported`);
      }
    }
  }

  processContinueStatement(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ContinueStatement,
  ): void {
    if (node.label) {
      this.onError(node, 'Continue labels are not yet supported');
    }

    this.appendNodeDebugInfo(builder, node);

    const continueTarget = builder.resolveContinueTargetInScope();
    if (!continueTarget) {
      this.onError(node, 'Could not resolve outer jump target for processing continue statement');
    }

    builder.buildJump(continueTarget);
  }

  processDoStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.DoStatement): void {
    this.appendNodeDebugInfo(builder, node);

    const builderLoop = builder.buildSubBuilder(true);

    const builderLoopMain = builderLoop.buildSubBuilder(true);
    const builderLoopMainBody = builderLoopMain.buildJumpTarget('body');
    const builderLoopMainCondition = builderLoopMain.buildJumpTarget('cond');
    const exitBuilder = builderLoop.buildJumpTarget('exit');

    builderLoopMainBody.builder.registerExitTargetInScope(exitBuilder.target);
    builderLoopMainBody.builder.registerContinueTargetInScope(builderLoopMainCondition.target);

    this.processStatement(context, builderLoopMainBody.builder, node.statement);

    let conditionVariable = this.processExpression(context, builderLoopMainCondition.builder, node.expression);
    builderLoopMainCondition.builder.buildBranch(
      conditionVariable,
      NativeCompilerBuilderBranchType.Truthy,
      builderLoopMainBody.target,
      exitBuilder.target,
    );
  }

  processExportAssignment(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ExportAssignment,
  ): void {
    const result = this.processExpression(context, builder, node.expression);
    if (node.isExportEquals) {
      // export = <expr>
      // Emit as module.exports = <expr>
      const moduleVariable = this.resolveValueDelegate(context, builder, 'module', node).getValue().variable;
      builder.buildSetProperty(moduleVariable, this.processIdentifierStringAsAtom(builder, 'exports'), result);
    } else {
      // export default <expr>
      // Emit as exports.default = <expr>
      const exportsVariable = this.resolveValueDelegate(context, builder, 'exports', node).getValue().variable;
      builder.buildSetProperty(exportsVariable, this.processIdentifierStringAsAtom(builder, 'default'), result);
    }
  }

  processEntityName(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.EntityName,
  ): NativeCompilerBuilderVariableID {
    if (ts.isIdentifier(node)) {
      return this.processIdentifierAsValueDelegate(context, builder, node).getValue().variable;
    } else {
      const qualifiedName: ts.QualifiedName = node;
      const leftVariable = this.processEntityName(context, builder, qualifiedName.left);
      return builder.buildGetProperty(
        leftVariable,
        this.processIdentifierAsAtom(builder, qualifiedName.right),
        leftVariable,
      );
    }
  }

  processImportEqualsDeclaration(
    context: NativeCompilerContext,
    builder: INativeCompilerBlockBuilder,
    node: ts.ImportEqualsDeclaration,
  ): void {
    if (node.isTypeOnly) {
      return;
    }

    const variableRef = this.resolveRegisteredVariableRefForIdentifier(context, builder, node.name);

    let variableId: NativeCompilerBuilderVariableID;
    const moduleReference = node.moduleReference;
    if (ts.isExternalModuleReference(moduleReference)) {
      variableId = this.processExpression(context, builder, moduleReference.expression);
    } else {
      variableId = this.processEntityName(context, builder, moduleReference);
    }

    builder.buildSetVariableRef(variableRef, variableId);
  }

  private prepareBindingElement(
    builder: INativeCompilerBlockBuilder,
    node: ts.BindingElement,
    scope: VariableRefScope,
  ): void {
    if (ts.isIdentifier(node.name)) {
      const variableName = node.name;
      builder.buildVariableRef(variableName.text, NativeCompilerBuilderVariableType.Empty, scope);
    } else if (ts.isObjectBindingPattern(node.name)) {
      for (const element of node.name.elements) {
        this.prepareBindingElement(builder, element, scope);
      }
    } else if (ts.isArrayBindingPattern(node.name)) {
      for (const element of node.name.elements) {
        if (ts.isOmittedExpression(element)) {
          continue;
        }
        this.prepareBindingElement(builder, element, scope);
      }
    }
  }

  private prepareBindingName(
    builder: INativeCompilerBlockBuilder,
    name: ts.BindingName,
    scope: VariableRefScope,
    isConst?: boolean,
  ): void {
    if (ts.isIdentifier(name)) {
      builder.buildVariableRef(name.text, NativeCompilerBuilderVariableType.Empty, scope, isConst);
    } else if (ts.isObjectBindingPattern(name)) {
      const objectBindingPattern: ts.ObjectBindingPattern = name;

      for (const element of objectBindingPattern.elements) {
        this.prepareBindingElement(builder, element, scope);
      }
    } else if (ts.isArrayBindingPattern(name)) {
      const arrayBindingPattern: ts.ArrayBindingPattern = name;

      for (const element of arrayBindingPattern.elements) {
        if (ts.isOmittedExpression(element)) {
          continue;
        }

        this.prepareBindingElement(builder, element, scope);
      }
    } else {
      this.onError(name, 'Unsupported variable declaration type');
    }
  }

  private prepareVariableDeclaration(
    builder: INativeCompilerBlockBuilder,
    node: ts.VariableDeclaration,
    scope: VariableRefScope,
    isConst?: boolean,
  ) {
    this.prepareBindingName(builder, node.name, scope, isConst);
  }

  private prepareVariableDeclarationList(builder: INativeCompilerBlockBuilder, node: ts.VariableDeclarationList) {
    const isLet = (node.flags & ts.NodeFlags.Let) != 0;
    const isConst = (node.flags & ts.NodeFlags.Const) != 0;
    const scope: VariableRefScope = isLet || isConst ? VariableRefScope.Block : VariableRefScope.Function;

    for (const declaration of node.declarations) {
      this.prepareVariableDeclaration(builder, declaration, scope, isConst);
    }
  }

  private prepareVariableStatement(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.VariableStatement,
  ): boolean {
    this.prepareVariableDeclarationList(builder, node.declarationList);
    return false;
  }

  private prepareFunctionDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.FunctionDeclaration,
  ): boolean {
    if (!node.body) {
      return false;
    }

    if (node.name) {
      builder.buildVariableRef(node.name.text, NativeCompilerBuilderVariableType.Object, VariableRefScope.Block);
    }

    // Function declaration should be hoisted at the beginning of the block
    plan.statementsToProcess.splice(plan.functionDeclarationInsertionIndex, 0, node);
    plan.functionDeclarationInsertionIndex++;

    return true;
  }

  private prepareClassDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.ClassDeclaration,
  ): boolean {
    if (node.name) {
      builder.buildVariableRef(node.name.text, NativeCompilerBuilderVariableType.Object, VariableRefScope.Block);
    }

    return false;
  }

  private prepareEnumDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.EnumDeclaration,
  ): boolean {
    builder.buildVariableRef(node.name.text, NativeCompilerBuilderVariableType.Object, VariableRefScope.Block);

    return false;
  }

  private prepareModuleDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.ModuleDeclaration,
  ): boolean {
    if (ts.isIdentifier(node.name)) {
      builder.buildVariableRef(node.name.text, NativeCompilerBuilderVariableType.Object, VariableRefScope.Block);
    }
    return false;
  }

  private prepareImportDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.ImportDeclaration,
  ): boolean {
    if (!node.importClause) {
      return true;
    }

    if (node.importClause.isTypeOnly) {
      return true;
    }

    const importVariableRefName = this.createVariableRefNameForImportedModule(node.moduleSpecifier);
    builder.registerLazyVariableRef(importVariableRefName, VariableRefScope.Block, (builder) =>
      builder.buildVariableRef(importVariableRefName, NativeCompilerBuilderVariableType.Empty, VariableRefScope.Block),
    );

    // We should process the next function declarations after our import
    plan.functionDeclarationInsertionIndex = plan.statementsToProcess.length + 1;

    return false;
  }

  private prepareImportEqualsDeclaration(
    plan: CompilationPlan,
    builder: INativeCompilerBlockBuilder,
    node: ts.ImportEqualsDeclaration,
  ): boolean {
    if (node.isTypeOnly) {
      return true;
    }

    builder.buildVariableRef(node.name.text, NativeCompilerBuilderVariableType.Empty, VariableRefScope.Block);

    return false;
  }

  private prepareStatement(plan: CompilationPlan, builder: INativeCompilerBlockBuilder, node: ts.Statement): void {
    if (this.isAmbientDeclaration(node)) {
      // Nothing to do for ambiant declarations
      return;
    }

    let fullyProcessed = false;

    if (ts.isVariableStatement(node)) {
      fullyProcessed = this.prepareVariableStatement(plan, builder, node);
    } else if (ts.isFunctionDeclaration(node)) {
      fullyProcessed = this.prepareFunctionDeclaration(plan, builder, node);
    } else if (ts.isClassDeclaration(node)) {
      fullyProcessed = this.prepareClassDeclaration(plan, builder, node);
    } else if (ts.isEnumDeclaration(node)) {
      fullyProcessed = this.prepareEnumDeclaration(plan, builder, node);
    } else if (ts.isModuleDeclaration(node)) {
      fullyProcessed = this.prepareModuleDeclaration(plan, builder, node);
    } else if (ts.isImportDeclaration(node)) {
      fullyProcessed = this.prepareImportDeclaration(plan, builder, node);
    } else if (ts.isImportEqualsDeclaration(node)) {
      fullyProcessed = this.prepareImportEqualsDeclaration(plan, builder, node);
    }

    if (!fullyProcessed) {
      plan.statementsToProcess.push(node);
    }
  }

  processStatement(context: NativeCompilerContext, builder: INativeCompilerBlockBuilder, node: ts.Statement) {
    if (!node) {
      return;
    }

    this.lastProcessingDeclaration = node;

    if (ts.isInterfaceDeclaration(node)) {
      this.processInterfaceDeclaration(builder, node);
    } else if (ts.isFunctionDeclaration(node)) {
      this.processFunctionDeclaration(context, builder, node, false);
    } else if (ts.isBlock(node)) {
      this.processBlock(context, builder, node);
    } else if (ts.isReturnStatement(node)) {
      this.processReturnStatement(context, builder, node);
    } else if (ts.isExpressionStatement(node)) {
      this.processExpressionStatement(context, builder, node);
    } else if (ts.isForStatement(node)) {
      this.processForStatement(context, builder, node);
    } else if (ts.isForOfStatement(node)) {
      this.processForOfStatement(context, builder, node);
    } else if (ts.isForInStatement(node)) {
      this.processForInStatement(context, builder, node);
    } else if (ts.isIfStatement(node)) {
      this.processIfStatement(context, builder, node);
    } else if (ts.isWhileStatement(node)) {
      this.processWhileStatement(context, builder, node);
    } else if (ts.isDoStatement(node)) {
      this.processDoStatement(context, builder, node);
    } else if (ts.isThrowStatement(node)) {
      this.processThrowStatement(context, builder, node);
    } else if (ts.isBreakStatement(node)) {
      this.processBreakStatement(builder, node);
    } else if (ts.isContinueStatement(node)) {
      this.processContinueStatement(context, builder, node);
    } else if (ts.isTryStatement(node)) {
      this.processTryStatement(context, builder, node);
    } else if (ts.isSwitchStatement(node)) {
      this.processSwitchStatement(context, builder, node);
    } else if (ts.isEmptyStatement(node)) {
      // no-op
    } else if (ts.isClassDeclaration(node)) {
      this.processClassDeclaration(context, builder, node);
    } else if (ts.isEnumDeclaration(node)) {
      this.processEnumDeclaration(context, builder, node);
    } else if (ts.isExportDeclaration(node)) {
      this.processExportDeclaration(context, builder, node);
    } else if (ts.isVariableStatement(node)) {
      this.processVariableStatement(context, builder, node);
    } else if (ts.isImportDeclaration(node)) {
      this.processImportDeclaration(context, builder, node);
    } else if (ts.isModuleDeclaration(node)) {
      this.processModuleDeclaration(context, builder, node);
    } else if (ts.isExportAssignment(node)) {
      this.processExportAssignment(context, builder, node);
    } else if (ts.isImportEqualsDeclaration(node)) {
      this.processImportEqualsDeclaration(context, builder, node);
    } else if (ts.isJsxExpression(node)) {
      this.processJsxExpression(context, builder, node);
    } else if (ts.isTypeAliasDeclaration(node)) {
      // no-op
    } else {
      this.onError(node, 'Cannot process statement');
    }
  }

  compile(): NativeCompilerIR.Base[] {
    try {
      const initFunctionName = this.allocateFunctionName(`${this.moduleName}__module_init__`);

      this.processSourceFile(initFunctionName, this.sourceFile);
      return this.moduleBuilder.finalize(initFunctionName, this.options);
    } catch (exc: any) {
      if (!(exc instanceof NativeHelper.NativeCompilerError)) {
        if (this.lastProcessingDeclaration) {
          // Decorate error with last processing declaration
          try {
            const resolvedError = new NativeHelper.NativeCompilerError(
              this.filePath,
              this.sourceFile,
              this.lastProcessingDeclaration,
              exc.message,
            );

            resolvedError.stack = exc.stack;

            throw resolvedError;
          } catch (e: any) {
            throw exc;
          }
        } else {
          throw exc;
        }
      } else {
        throw exc;
      }
    }
  }
}

export function compileIRsToC(
  ir: NativeCompilerIR.Base[],
  options: NativeCompilerOptions,
): NativeCompilerEmitterOutput {
  const emitter = new CompilerNativeCEmitter(options);
  emitterWalk(emitter, ir);
  return emitter.finalize();
}
