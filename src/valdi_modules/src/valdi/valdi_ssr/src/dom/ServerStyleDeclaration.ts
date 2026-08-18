export class ServerStyleDeclaration {
  private readonly values = new Map<string, string>();

  constructor(private readonly onMutation: () => void) {}

  get cssText(): string {
    return this.entries.map(entry => `${entry[0]}: ${entry[1]};`).join(' ');
  }

  set cssText(value: string) {
    this.values.clear();
    const declarations = value.split(';');
    for (let index = 0; index < declarations.length; index++) {
      const declaration = declarations[index];
      const separator = declaration.indexOf(':');
      if (separator < 0) {
        continue;
      }
      const name = declaration.slice(0, separator).trim();
      const propertyValue = declaration.slice(separator + 1).trim();
      if (name && propertyValue) {
        this.values.set(name, propertyValue);
      }
    }
    this.onMutation();
  }

  get entries(): Array<[string, string]> {
    return Array.from(this.values.entries());
  }

  getPropertyValue(name: string): string {
    return this.values.get(name) ?? '';
  }

  setProperty(name: string, value: string | null, _priority?: string): void {
    this.setValue(name, value ?? '');
  }

  removeProperty(name: string): string {
    const previousValue = this.values.get(name) ?? '';
    if (this.values.delete(name)) {
      this.onMutation();
    }
    return previousValue;
  }

  setValue(name: string, value: unknown): void {
    const stringValue = String(value);
    if (!stringValue) {
      this.removeProperty(name);
      return;
    }
    if (this.values.get(name) === stringValue) {
      return;
    }
    this.values.set(name, stringValue);
    this.onMutation();
  }
}

export function createServerStyleDeclaration(onMutation: () => void): ServerStyleDeclaration {
  const declaration = new ServerStyleDeclaration(onMutation);
  return new Proxy(declaration, {
    get(target, property, receiver): unknown {
      if (typeof property === 'string' && !(property in target)) {
        return target.getPropertyValue(property);
      }
      const value = Reflect.get(target, property, receiver);
      return typeof value === 'function' ? value.bind(target) : value;
    },
    set(target, property, value, receiver): boolean {
      if (typeof property === 'string' && !(property in target)) {
        target.setValue(property, value);
        return true;
      }
      return Reflect.set(target, property, value, receiver);
    },
  });
}
