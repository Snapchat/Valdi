export class Locale {
  readonly language: string;
  readonly region: string | undefined;
  readonly script: string | undefined;

  constructor(language: string, region: string | undefined, script: string | undefined = undefined) {
    this.language = language.toLowerCase();
    this.script = script;
    this.region = region?.toLocaleUpperCase();
  }

  toString(): string {
    let localeString = '';
    if (this.language) {
      localeString += this.language;
    }
    if (this.script) {
      localeString += `-${this.script}`;
    }
    if (this.region) {
      localeString += `-${this.region}`;
    }
    return localeString;
  }

  static parse(input: string): Locale {
    const components = input.split('-');
    if (components.length === 2) {
      return new Locale(components[0] /* language */, components[1] /* region */);
    }
    return new Locale(components[0] /* language */, components[2] /* region */, components[1] /* script */);
  }
}
