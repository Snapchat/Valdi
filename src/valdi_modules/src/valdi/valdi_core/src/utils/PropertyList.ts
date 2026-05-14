import { StringMap } from 'coreutils/src/StringMap';
import { PropertyList } from 'valdi_tsx/src/PropertyList';

export type { PropertyList };

export function enumeratePropertyList(
  propertyList: PropertyList,
  enumerator: (name: string, value: any) => void,
): void {
  if (Array.isArray(propertyList)) {
    const length = propertyList.length;
    for (let i = 0; i + 1 < length; ) {
      const key = propertyList[i++];
      const value = propertyList[i++];
      enumerator(key as string, value);
    }
  } else {
    for (const key in propertyList) {
      const value = propertyList[key];
      enumerator(key, value);
    }
  }
}

export function removeProperty(propertyList: PropertyList, property: string): any | undefined {
  if (Array.isArray(propertyList)) {
    const index = propertyList.indexOf(property);
    if (index < 0 || index + 1 == propertyList.length) {
      return undefined;
    }
    const value = propertyList[index + 1];
    propertyList.splice(index, 2);
    return value;
  } else {
    const value = propertyList[property];
    delete propertyList[property];
    return value;
  }
}

export function hasProperty(propertyList: PropertyList, property: string): boolean {
  if (Array.isArray(propertyList)) {
    // Array form alternates [key, value, key, value]. Only check keys.
    const length = propertyList.length;
    for (let i = 0; i + 1 < length; i += 2) {
      if (propertyList[i] === property) return true;
    }
    return false;
  }
  return property in propertyList;
}

export function propertyListToObject(propertyList: PropertyList): StringMap<any> {
  if (!Array.isArray(propertyList)) {
    return propertyList;
  }

  const out: StringMap<any> = {};
  const length = propertyList.length;
  for (let i = 0; i + 1 < length; ) {
    const key = propertyList[i++];
    const value = propertyList[i++];
    out[key as string] = value;
  }

  return out;
}
