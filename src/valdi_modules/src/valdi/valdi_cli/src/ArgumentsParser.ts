import { StringMap } from 'coreutils/src/StringMap';

export interface Argument<T> {
  isSet: boolean;

  value?: T;
}

const enum ArgumentType {
  SINGLE,
  ARRAY,
  FLAG,
}

interface ArgumentInner {
  readonly name: string;
  readonly description: string;
  readonly required: boolean;
  readonly type: ArgumentType;
  readonly parser?: (value: string) => any;

  readonly valueContainer: Argument<any>;
}

export class ArgumentsParser {
  private arguments: ArgumentInner[] = [];
  private argumentByName: StringMap<ArgumentInner> = {};
  private readonly programArguments: string[];

  constructor(readonly programName: string, programArguments: string[]) {
    this.programArguments = programArguments.slice();
  }

  add<T = string>(name: string, description: string, required: boolean, parser?: (value: string) => T): Argument<T> {
    const argument: Argument<T> = {
      isSet: false,
    };

    this.doAdd(argument, ArgumentType.SINGLE, name, description, required, parser);

    return argument;
  }

  addArray<T = string>(
    name: string,
    description: string,
    required: boolean,
    parser?: (value: string) => T,
  ): Argument<T[]> {
    const argument: Argument<T[]> = {
      isSet: false,
    };

    this.doAdd(argument, ArgumentType.ARRAY, name, description, required, parser);

    return argument;
  }

  addFlag(name: string, description: string, required: boolean): Argument<boolean> {
    const argument: Argument<boolean> = {
      isSet: false,
    };

    this.doAdd(argument, ArgumentType.FLAG, name, description, required);

    return argument;
  }

  addNumber(name: string, description: string, required: boolean): Argument<number> {
    return this.add(name, description, required, v => Number.parseFloat(v));
  }

  addString(name: string, description: string, required: boolean): Argument<string> {
    return this.add(name, description, required);
  }

  private doAdd(
    argument: Argument<any>,
    type: ArgumentType,
    name: string,
    description: string,
    required: boolean,
    parser?: (value: string) => any,
  ) {
    if (this.argumentByName[name]) {
      throw new Error(`Duplicate argument ${name}`);
    }

    const innerArgument: ArgumentInner = {
      valueContainer: argument,
      name,
      description,
      required,
      type,
      parser,
    };

    this.arguments.push(innerArgument);
    this.argumentByName[name] = innerArgument;
  }

  printDescription() {
    const prefix = `usage: ${this.programName} `;
    console.info(prefix);

    let indent = '';
    while (indent.length < prefix.length) {
      indent += ' ';
    }

    for (const argument of this.arguments) {
      console.info(`${indent}${argument.name}: ${argument.description}`);
    }
  }

  private doParseArguments(programArguments: string[]) {
    const args = programArguments;
    // Ignore first argument
    args.shift();

    while (args.length) {
      const name = args.shift()!;

      const argument = this.argumentByName[name];
      if (!argument) {
        throw new Error(`Unrecognized argument ${name}`);
      }

      const wasSet = argument.valueContainer.isSet;
      argument.valueContainer.isSet = true;

      if (argument.type === ArgumentType.FLAG) {
        argument.valueContainer.value = true;
      } else {
        if (!args.length) {
          throw new Error(`Missing argument value for ${name}`);
        }

        let value: any;
        const argumentValue = args.shift()!;
        if (argument.parser) {
          try {
            value = argument.parser(argumentValue);
          } catch (err: any) {
            console.error(`Failed to parse argument ${argument.name}: ${err}`);
            console.error('Passed value:', argumentValue);
            throw err;
          }
        } else {
          value = argumentValue;
        }

        if (argument.type === ArgumentType.SINGLE) {
          if (wasSet) {
            throw new Error(`Argument ${argument.name} passed multiple times`);
          }
          argument.valueContainer.value = value;
        } else if (argument.type === ArgumentType.ARRAY) {
          if (!argument.valueContainer.value) {
            argument.valueContainer.value = [];
          }

          argument.valueContainer.value.push(argumentValue);
        } else {
          throw new Error('Unrecognized argumenet type');
        }
      }
    }
  }

  private checkForMissingArguments() {
    for (const argument of this.arguments) {
      if (argument.required && !argument.valueContainer.isSet) {
        throw new Error(`Argument ${argument.name} is required but was not provided`);
      }
    }
  }

  parse() {
    try {
      this.doParseArguments(this.programArguments);
      this.checkForMissingArguments();
    } catch (err: any) {
      this.printDescription();
      console.error(err.message);
      throw err;
    }
  }
}
