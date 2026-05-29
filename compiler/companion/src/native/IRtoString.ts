import { OutputWriter } from '../OutputWriter';
import { NativeCodeWriter } from './NativeCodeWriter';
import {
  NativeCompilerBuilderAtomID,
  NativeCompilerBuilderBranchType,
  NativeCompilerBuilderJumpTargetID,
  NativeCompilerBuilderVariableID,
} from './builder/INativeCompilerBuilder';
import { NativeCompilerIR } from './builder/NativeCompilerBuilderIR';

function getUnaryOperatorName(unaryOpKind: NativeCompilerIR.UnaryOperator): string {
  switch (unaryOpKind) {
    case NativeCompilerIR.UnaryOperator.Neg:
      return 'neg';
    case NativeCompilerIR.UnaryOperator.Plus:
      return 'plus';
    case NativeCompilerIR.UnaryOperator.Inc:
      return 'inc';
    case NativeCompilerIR.UnaryOperator.Dec:
      return 'dec';
    case NativeCompilerIR.UnaryOperator.BitwiseNot:
      return 'bitwisenot';
    case NativeCompilerIR.UnaryOperator.LogicalNot:
      return 'logicalnot';
    case NativeCompilerIR.UnaryOperator.TypeOf:
      return 'typeof';
  }
}

function getBinaryOpOperatorName(binaryOpKind: NativeCompilerIR.BinaryOperator): string {
  switch (binaryOpKind) {
    case NativeCompilerIR.BinaryOperator.Mult:
      return 'mult';
    case NativeCompilerIR.BinaryOperator.Add:
      return 'add';
    case NativeCompilerIR.BinaryOperator.Sub:
      return 'sub';
    case NativeCompilerIR.BinaryOperator.Div:
      return 'div';
    case NativeCompilerIR.BinaryOperator.LeftShift:
      return 'leftshift';
    case NativeCompilerIR.BinaryOperator.RightShift:
      return 'rightshift';
    case NativeCompilerIR.BinaryOperator.UnsignedRightShift:
      return 'urightshift';
    case NativeCompilerIR.BinaryOperator.BitwiseXOR:
      return 'xor';
    case NativeCompilerIR.BinaryOperator.BitwiseAND:
      return 'and';
    case NativeCompilerIR.BinaryOperator.BitwiseOR:
      return 'or';
    case NativeCompilerIR.BinaryOperator.LessThan:
      return 'lt';
    case NativeCompilerIR.BinaryOperator.LessThanOrEqual:
      return 'lte';
    case NativeCompilerIR.BinaryOperator.LessThanOrEqualEqual:
      return 'ltee';
    case NativeCompilerIR.BinaryOperator.GreaterThan:
      return 'gt';
    case NativeCompilerIR.BinaryOperator.GreaterThanOrEqual:
      return 'gte';
    case NativeCompilerIR.BinaryOperator.GreaterThanOrEqualEqual:
      return 'gtee';
    case NativeCompilerIR.BinaryOperator.EqualEqual:
      return 'eq';
    case NativeCompilerIR.BinaryOperator.EqualEqualEqual:
      return 'eqstrict';
    case NativeCompilerIR.BinaryOperator.DifferentThan:
      return 'ne';
    case NativeCompilerIR.BinaryOperator.DifferentThanStrict:
      return 'nestrict';
    case NativeCompilerIR.BinaryOperator.Modulo:
      return 'mod';
    case NativeCompilerIR.BinaryOperator.Exponentiation:
      return 'exp';
    case NativeCompilerIR.BinaryOperator.InstanceOf:
      return 'instanceof';
    case NativeCompilerIR.BinaryOperator.In:
      return 'in';
  }
}

function getBranchTypeName(branchType: NativeCompilerBuilderBranchType): string {
  switch (branchType) {
    case NativeCompilerBuilderBranchType.Truthy:
      return 'branch';
    case NativeCompilerBuilderBranchType.NotUndefinedOrNull:
      return 'nullable_branch';
  }
}

function variableToString(variable: NativeCompilerBuilderVariableID): string {
  return variable.variable.toString();
}

function escapeIRString(str: string): string {
  return str
    .replace(/\\/g, '\\\\')  // backslash first
    .replace(/'/g, "\\'")    // single quote
    .replace(/\n/g, '\\n')   // newline
    .replace(/\r/g, '\\r')   // carriage return
    .replace(/\t/g, '\\t');  // tab
}

class IRStringWriter {
  constructor(readonly writer: OutputWriter) {}

  append(str: string): IRStringWriter {
    this.writer.append(str);
    return this;
  }

  appendQuotedString(str: string | undefined): IRStringWriter {
    if (str === undefined) {
      return this.append(' <null>');
    } else {
      return this.append(` '${escapeIRString(str)}'`);
    }
  }

  appendVariable(variable: NativeCompilerBuilderVariableID): IRStringWriter {
    return this.append(` @${variable.variable}`);
  }

  appendVariables(variables: NativeCompilerBuilderVariableID[]): IRStringWriter {
    this.append(' [');
    for (let i = 0; i < variables.length; i++) {
      if (i > 0) {
        this.append(',');
      }
      this.appendVariable(variables[i]);
    }
    this.append(' ]');
    return this;
  }

  appendAtom(atom: NativeCompilerBuilderAtomID): IRStringWriter {
    return this.appendQuotedString(atom.identifier);
  }

  appendAtoms(atoms: NativeCompilerBuilderAtomID[]): IRStringWriter {
    this.append(' [');
    for (let i = 0; i < atoms.length; i++) {
      if (i > 0) {
        this.append(',');
      }
      this.appendAtom(atoms[i]);
    }
    this.append(' ]');
    return this;
  }

  appendJumpTarget(jumpTarget: NativeCompilerBuilderJumpTargetID): IRStringWriter {
    return this.appendQuotedString(jumpTarget.tag);
  }
}

function outputIR(ir: NativeCompilerIR.Base, output: IRStringWriter): IRStringWriter {
  switch (ir.kind) {
    case NativeCompilerIR.Kind.Slot: {
      const typedIR = ir as NativeCompilerIR.Slot;
      // slot '<type>' <variable>
      return output.append('slot').appendQuotedString(typedIR.value.getTypeString()).appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.Global: {
      const typedIR = ir as NativeCompilerIR.Global;

      // getglobal <output_variable>

      return output.append(`getglobal`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.Keyword: {
      const typedIR = ir as NativeCompilerIR.Keyword;
      // storeundefined <output_variable>
      // storenull <output_variable>
      // storethis <output_variable>

      switch (typedIR.keyword) {
        case NativeCompilerIR.KeywordKind.Undefined:
          return output.append(`storeundefined`).appendVariable(typedIR.variable);
        case NativeCompilerIR.KeywordKind.Null:
          return output.append(`storenull`).appendVariable(typedIR.variable);
        case NativeCompilerIR.KeywordKind.This:
          return output.append(`storethis`).appendVariable(typedIR.variable);
      }
    }
    case NativeCompilerIR.Kind.Exception: {
      const typedIR = ir as NativeCompilerIR.Exception;

      // storeexception <output_variable>

      return output.append(`storeexception`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.SetProperty: {
      const typedIR = ir as NativeCompilerIR.SetProperty;

      // setprop <object> <prop> <value>

      if (typedIR.properties.length == 1) {
        return output
          .append('setprop')
          .appendVariable(typedIR.object)
          .appendAtom(typedIR.properties[0])
          .appendVariable(typedIR.values[0]);
      } else {
        return output
          .append('setprops')
          .appendVariable(typedIR.object)
          .appendAtoms(typedIR.properties)
          .appendVariables(typedIR.values);
      }
    }
    case NativeCompilerIR.Kind.SetPropertyValue: {
      const typedIR = ir as NativeCompilerIR.SetPropertyValue;
      // setpropv <object> <prop> <value>
      return output
        .append('setpropv')
        .appendVariable(typedIR.object)
        .appendVariable(typedIR.property)
        .appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.SetPropertyIndex: {
      const typedIR = ir as NativeCompilerIR.SetPropertyIndex;
      // setpropi <object> <index> <value>
      return output
        .append('setpropi')
        .appendVariable(typedIR.object)
        .append(` ${typedIR.index}`)
        .appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.CopyPropertiesFrom: {
      const typedIR = ir as NativeCompilerIR.CopyPropertiesFrom;

      // copyprops <src> <target>

      output.append('copyprops').appendVariable(typedIR.value).appendVariable(typedIR.object);
      if (typedIR.propertiesToIgnore) {
        return output.appendAtoms(typedIR.propertiesToIgnore);
      } else {
        return output;
      }
    }
    case NativeCompilerIR.Kind.GetProperty: {
      const typedIR = ir as NativeCompilerIR.GetProperty;

      // getprop <object> <property_name> <output_variable>

      return output
        .append(`getprop`)
        .appendVariable(typedIR.object)
        .appendAtom(typedIR.property)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetPropertyFree: {
      const typedIR = ir as NativeCompilerIR.GetPropertyFree;

      // getpropfree <object> <property_name> <output_variable>

      return output
        .append(`getpropfree`)
        .appendVariable(typedIR.object)
        .appendAtom(typedIR.property)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetPropertyValue: {
      const typedIR = ir as NativeCompilerIR.GetPropertyValue;

      // getpropvalue <object> <property_name_from_variable> <output_variable>

      return output
        .append(`getpropvalue`)
        .appendVariable(typedIR.object)
        .appendVariable(typedIR.property)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.DeleteProperty: {
      const typedIR = ir as NativeCompilerIR.DeleteProperty;

      // delprop <object> <property_name> <output_variable>

      return output
        .append(`delprop`)
        .appendVariable(typedIR.object)
        .appendAtom(typedIR.property)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.DeletePropertyValue: {
      const typedIR = ir as NativeCompilerIR.DeletePropertyValue;

      // delpropvalue <object> <property_name_from_variable> <output_variable>

      return output
        .append(`delpropvalue`)
        .appendVariable(typedIR.object)
        .appendVariable(typedIR.property)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetFunctionArg: {
      const typedIR = ir as NativeCompilerIR.GetFunctionArg;

      // getfnarg <arg_index> <output_variable>

      return output.append(`getfnarg ${typedIR.index}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetFunctionArgumentsObject: {
      const typedIR = ir as NativeCompilerIR.GetFunctionArgumentsObject;

      // getfnarguments <start_index> <output_variable>

      return output.append(`getfnarguments ${typedIR.startIndex}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetSuper: {
      const typedIR = ir as NativeCompilerIR.GetSuper;

      // getsuper <output_variable>

      return output.append(`getsuper`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetSuperConstructor: {
      const typedIR = ir as NativeCompilerIR.GetSuperConstructor;

      // getsuperctor <output_variable>

      return output.append(`getsuperctor`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetClosureArg: {
      const typedIR = ir as NativeCompilerIR.GetClosureArg;

      // getclosurearg <arg_index> <output_variable>

      return output.append(`getclosurearg ${typedIR.index}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.GetModuleConst: {
      const typedIR = ir as NativeCompilerIR.GetModuleConst;

      // getmoduleconst <const_index> <output_variable>

      return output.append(`getmoduleconst ${typedIR.index}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LiteralInteger: {
      const typedIR = ir as NativeCompilerIR.LiteralInteger;

      // storeint <int_value> <output_variable>

      return output.append(`storeint ${typedIR.value}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LiteralLong: {
      const typedIR = ir as NativeCompilerIR.LiteralLong;

      // storelong <long_value> <output_variable>

      return output.append(`storelong ${typedIR.value}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LiteralDouble: {
      const typedIR = ir as NativeCompilerIR.LiteralDouble;

      // storedouble <double_value> <output_variable>

      return output.append(`storedouble ${typedIR.value}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LiteralString: {
      const typedIR = ir as NativeCompilerIR.LiteralString;

      // storestring '<string_value>' <output_variable>

      return output.append('storestring').appendQuotedString(typedIR.value).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LiteralBool: {
      const typedIR = ir as NativeCompilerIR.LiteralBool;

      // storebool <bool_value> <output_variable>

      return output.append(`storebool ${typedIR.value ? 'true' : 'false'}`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.UnaryOp: {
      const typedIR = ir as NativeCompilerIR.UnaryOp;

      // <operator_name> <object> <output_variable>

      return output
        .append(getUnaryOperatorName(typedIR.operator))
        .appendVariable(typedIR.operand)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.BinaryOp: {
      const typedIR = ir as NativeCompilerIR.BinaryOp;

      // <operator_name> <left_variable> <right_variable> <output_variable>

      return output
        .append(getBinaryOpOperatorName(typedIR.operator))
        .appendVariable(typedIR.left)
        .appendVariable(typedIR.right)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.NewObject: {
      const typedIR = ir as NativeCompilerIR.NewObject;

      // newobject <output_variable>

      return output.append(`newobject`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.NewArray: {
      const typedIR = ir as NativeCompilerIR.NewArray;

      // newarray <output_variable>

      return output.append(`newarray`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.NewArrowFunctionValue: {
      const typedIR = ir as NativeCompilerIR.NewArrowFunctionValue;

      // newarrowfn <name> [<closure_args>] <output_variable>

      return output
        .append(`newarrowfn`)
        .appendQuotedString(typedIR.functionName)
        .appendVariables(typedIR.closureArgs)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.NewFunctionValue: {
      const typedIR = ir as NativeCompilerIR.NewFunctionValue;

      // newfn <name> [<closure_args>] <output_variable>

      return output
        .append(`newfn`)
        .appendQuotedString(typedIR.functionName)
        .appendAtom(typedIR.name)
        .appendVariables(typedIR.closureArgs)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.NewClassValue: {
      const typedIR = ir as NativeCompilerIR.NewClassValue;

      // newclass <name> <ctor_name> <parent_ctor> [<closure_args>] <output_variable>

      output.append(`newclass`).appendQuotedString(typedIR.functionName).appendAtom(typedIR.name);

      if (typedIR.parentClass) {
        output.appendVariable(typedIR.parentClass);
      } else {
        output.append(' <null>');
      }

      return output.appendVariables(typedIR.closureArgs).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.SetClassElement: {
      const typedIR = ir as NativeCompilerIR.SetClassElement;
      let irName: string;

      switch (typedIR.type) {
        case NativeCompilerIR.ClassElementType.Method:
          irName = typedIR.static ? 'setclassstaticmethod' : 'setclassmethod';
          break;
        case NativeCompilerIR.ClassElementType.Getter:
          irName = typedIR.static ? 'setclassstaticgetter' : 'setclassgetter';
          break;
        case NativeCompilerIR.ClassElementType.Setter:
          irName = typedIR.static ? 'setclassstaticsetter' : 'setclasssetter';
          break;
      }

      return output.append(irName).appendVariable(typedIR.cls).appendAtom(typedIR.name).appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.SetClassElementValue: {
      const typedIR = ir as NativeCompilerIR.SetClassElementValue;
      let irName: string;

      switch (typedIR.type) {
        case NativeCompilerIR.ClassElementType.Method:
          irName = typedIR.static ? 'setclassstaticmethodv' : 'setclassmethodv';
          break;
        case NativeCompilerIR.ClassElementType.Getter:
          irName = typedIR.static ? 'setclassstaticgetter' : 'setclassgetterv';
          break;
        case NativeCompilerIR.ClassElementType.Setter:
          irName = typedIR.static ? 'setclassstaticsetterv' : 'setclasssetterv';
          break;
      }

      return output
        .append(irName)
        .appendVariable(typedIR.cls)
        .appendVariable(typedIR.name)
        .appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.Retain: {
      const typedIR = ir as NativeCompilerIR.Retain;
      // retain <value>
      return output.append('retain').appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.Free: {
      const typedIR = ir as NativeCompilerIR.Free;

      // release <variable>

      return output.append('release').appendVariable(typedIR.value);
    }
    case NativeCompilerIR.Kind.FreeV: {
      const typedIR = ir as NativeCompilerIR.FreeV;

      // release_v <variables...>

      return output.append('release_v').appendVariables(typedIR.values);
    }
    case NativeCompilerIR.Kind.NewVariableRef: {
      const typedIR = ir as NativeCompilerIR.NewVariableRef;

      // newvref '<name>' <out>
      return output.append('newvref').appendQuotedString(typedIR.name).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.LoadVariableRef: {
      const typedIR = ir as NativeCompilerIR.LoadVariableRef;

      // loadvref <target> <out>
      return output.append('loadvref').appendVariable(typedIR.target).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.SetVariableRef: {
      const typedIR = ir as NativeCompilerIR.SetVariableRef;

      // setvref <value> <target>
      return output.append('setvref').appendVariable(typedIR.value).appendVariable(typedIR.target);
    }
    case NativeCompilerIR.Kind.IntrinsicCall: {
      const typedIR = ir as NativeCompilerIR.IntrinsicCall;
      return output
        .append('intrinsic')
        .appendQuotedString(typedIR.func)
        .appendVariables(typedIR.args)
        .appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.FunctionInvocation: {
      const typedIR = ir as NativeCompilerIR.FunctionInvocation;

      switch (typedIR.argsType) {
        case NativeCompilerIR.FunctionArgumentsType.Direct:
          // call <fn> <this> [<parameters>] <out>
          return output
            .append('call')
            .appendVariable(typedIR.func)
            .appendVariable(typedIR.obj)
            .appendVariables(typedIR.args)
            .appendVariable(typedIR.variable);
        case NativeCompilerIR.FunctionArgumentsType.Indirect:
          // callv <fn> <this> <parameters_array> <out>
          return output
            .append('callv')
            .appendVariable(typedIR.func)
            .appendVariable(typedIR.obj)
            .appendVariable(typedIR.args[0])
            .appendVariable(typedIR.variable);
        case NativeCompilerIR.FunctionArgumentsType.ForwardFromCurrentCall:
          // callf <fn> <this> <out>
          return output
            .append('callf')
            .appendVariable(typedIR.func)
            .appendVariable(typedIR.obj)
            .appendVariable(typedIR.variable);
      }
    }
    case NativeCompilerIR.Kind.ConstructorInvocation: {
      const typedIR = ir as NativeCompilerIR.ConstructorInvocation;

      switch (typedIR.argsType) {
        case NativeCompilerIR.FunctionArgumentsType.Direct:
          // new <fn> [<parameters>] <out>
          return output
            .append('new')
            .appendVariable(typedIR.func)
            .appendVariables(typedIR.args)
            .appendVariable(typedIR.variable);
        case NativeCompilerIR.FunctionArgumentsType.Indirect:
          // newv <fn> <parameters_array> <out>
          return output
            .append('newv')
            .appendVariable(typedIR.func)
            .appendVariable(typedIR.args[0])
            .appendVariable(typedIR.variable);
        case NativeCompilerIR.FunctionArgumentsType.ForwardFromCurrentCall:
          // newv <fn> <parameters_array> <out>
          return output.append('newf').appendVariable(typedIR.func).appendVariable(typedIR.variable);
      }
    }
    case NativeCompilerIR.Kind.BindJumpTarget: {
      const typedIR = ir as NativeCompilerIR.BindJumpTarget;

      // jumptarget <jump_target>
      return output.append('jumptarget').appendJumpTarget(typedIR.target);
    }
    case NativeCompilerIR.Kind.Jump: {
      const typedIR = ir as NativeCompilerIR.Jump;

      // jump <jump_target>

      return output.append('jump').appendJumpTarget(typedIR.target);
    }
    case NativeCompilerIR.Kind.Branch: {
      const typedIR = ir as NativeCompilerIR.Branch;

      // branch <variable> <jump_target_if_true> <jump_target_if_false>

      output
        .append(getBranchTypeName(typedIR.type))
        .appendVariable(typedIR.conditionVariable)
        .appendJumpTarget(typedIR.trueTarget);
      if (typedIR.falseTarget) {
        return output.appendJumpTarget(typedIR.falseTarget);
      } else {
        return output.append(' <null>');
      }
    }

    case NativeCompilerIR.Kind.Iterator: {
      const typedIR = ir as NativeCompilerIR.Iterator;

      // iterator <variable> <out>
      return output.append('iterator').appendVariable(typedIR.arg).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.IteratorNext: {
      const typedIR = ir as NativeCompilerIR.IteratorNext;

      // iteratornext <iterator> <out>

      return output.append('iteratornext').appendVariable(typedIR.iterator).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.KeysIterator: {
      const typedIR = ir as NativeCompilerIR.KeysIterator;

      // keysiterator <variable> <out>
      return output.append('keysiterator').appendVariable(typedIR.arg).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.KeysIteratorNext: {
      const typedIR = ir as NativeCompilerIR.KeysIteratorNext;

      // keysiteratornext <iterator> <out>

      return output.append('keysiteratornext').appendVariable(typedIR.iterator).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.Generator: {
      const typedIR = ir as NativeCompilerIR.Generator;
      return output.append('generator').appendVariable(typedIR.arg).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.Resume: {
      const typedIR = ir as NativeCompilerIR.Resume;
      return output.append('resume').appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.Reentry: {
      const typedIR = ir as NativeCompilerIR.Reentry;
      output.append('reentry');
      for (const rp of typedIR.resumePoints) {
        output.appendJumpTarget(rp);
      }
      return output;
    }
    case NativeCompilerIR.Kind.SetResumePoint: {
      const typedIR = ir as NativeCompilerIR.SetResumePoint;
      return output.append('setresumepoint').appendJumpTarget(typedIR.resumePoint);
    }
    case NativeCompilerIR.Kind.Assignment: {
      const typedIR = ir as NativeCompilerIR.Assignment;

      // assign <variable> <output_variable>
      return output.append('assign').appendVariable(typedIR.right).appendVariable(typedIR.left);
    }
    case NativeCompilerIR.Kind.GetException: {
      const typedIR = ir as NativeCompilerIR.GetException;

      // getexception <output_variable>

      return output.append(`getexception`).appendVariable(typedIR.variable);
    }
    case NativeCompilerIR.Kind.CheckException: {
      const typedIR = ir as NativeCompilerIR.CheckException;

      // checkexception <jump_target>

      return output.append(`checkexception`).appendJumpTarget(typedIR.exceptionTarget);
    }
    case NativeCompilerIR.Kind.Throw: {
      const typedIR = ir as NativeCompilerIR.Throw;

      // throwexception <variable> <jump_target>

      return output.append('throwexception').appendVariable(typedIR.value).appendJumpTarget(typedIR.target);
    }
    case NativeCompilerIR.Kind.StartFunction: {
      const typedIR = ir as NativeCompilerIR.StartFunction;

      // function_begin '<name>'

      return output.append('function_begin').appendQuotedString(typedIR.name);
    }
    case NativeCompilerIR.Kind.EndFunction: {
      const typedIR = ir as NativeCompilerIR.EndFunction;

      // function_end <output_variable>
      return output.append('function_end').appendVariable(typedIR.returnVariable).append('\n');
    }
    case NativeCompilerIR.Kind.Comments:
    case NativeCompilerIR.Kind.ProgramCounterInfo:
    case NativeCompilerIR.Kind.BuilderStub:
    case NativeCompilerIR.Kind.BoilerplatePrologue:
    case NativeCompilerIR.Kind.BoilerplateEpilogue:
      return output;
  }
}

export function IRtoString(irs: NativeCompilerIR.Base[]): string {
  const writer = new OutputWriter();
  const irWriter = new IRStringWriter(writer);

  for (const ir of irs) {
    const writtenLength = writer.writtenLength;
    outputIR(ir, irWriter);
    if (writtenLength !== writer.writtenLength) {
      writer.append('\n');
    }
  }

  return writer.content();
}
