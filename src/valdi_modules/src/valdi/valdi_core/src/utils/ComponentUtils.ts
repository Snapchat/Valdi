import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { ElementRefs, LegacyVueComponent } from 'valdi_core/src/Valdi';

export interface KeyedItem<T> {
  readonly key: string;
  readonly item: T;
}

function doGetChildComponentsOfType<T extends IComponent>(
  component: IComponent,
  clazz: ComponentConstructor<T>,
  recursive: boolean,
  out: T[],
): void {
  const children = component.renderer.getComponentChildren(component);

  for (const child of children) {
    if (child instanceof clazz) {
      out.push(child);
    }
  }

  if (recursive) {
    for (const child of children) {
      doGetChildComponentsOfType(child, clazz, recursive, out);
    }
  }
}

/**
 * Helper function for safely getting a child component by its id.
 */
export function getChildComponent<T extends IComponent>(
  component: LegacyVueComponent,
  elementId: string,
): T | undefined {
  return component.elementRefs[elementId].single()?.emittingComponent as T;
}

/**
 * Returns a list of keyed components containing each component in a for-each.
 */
export function getForEachComponents<ComponentT extends IComponent, ElementT = any>(
  elementRefs: ElementRefs<ElementT>,
  componentClass: ComponentConstructor<ComponentT>,
  customId: string,
): KeyedItem<ComponentT>[] {
  const keyedComponents: KeyedItem<ComponentT>[] = [];

  for (const element of elementRefs[customId].all()) {
    if (element.emittingComponent instanceof componentClass) {
      keyedComponents.push({
        key: element.renderer.getComponentKey(element.emittingComponent),
        item: element.emittingComponent,
      });
    }
  }
  return keyedComponents;
}

/**
 * Returns a list of child components of the given type, optionally recursive.
 */
export function getChildComponentsOfType<T extends IComponent>(
  component: IComponent,
  clazz: ComponentConstructor<T>,
  recursive = false,
): T[] {
  const components: T[] = [];
  doGetChildComponentsOfType(component, clazz, recursive, components);
  return components;
}
