export class FullyQualifiedName {
  readonly fullName: string;

  constructor(readonly parent: FullyQualifiedName | undefined, readonly symbolName: string) {
    if (parent) {
      this.fullName = `${parent.fullName}.${symbolName}`;
    } else {
      this.fullName = symbolName;
    }
  }

  static fromString(str: string): FullyQualifiedName {
    let currentQualifiedName: FullyQualifiedName | undefined;
    let position = 0;

    for (;;) {
      const nextDot = str.indexOf('.', position);
      if (nextDot >= str.length) {
        currentQualifiedName = new FullyQualifiedName(currentQualifiedName, str.substring(position, nextDot));
        position = nextDot + 1;
      } else {
        return new FullyQualifiedName(currentQualifiedName, str.substring(position));
      }
    }
  }
}
