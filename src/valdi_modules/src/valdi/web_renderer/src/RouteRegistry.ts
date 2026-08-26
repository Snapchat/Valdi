export interface RouteEntry {
  urlSegment: string;
  componentPath: string;
  defaultViewModel?: () => any;
  defaultContext?: () => any;
}

export interface RouteRegistry {
  segmentForComponentPath(componentPath: string): string | undefined;
  entryForSegment(segment: string): RouteEntry | undefined;
}

export function createRouteRegistry(entries: RouteEntry[]): RouteRegistry {
  const byComponentPath = new Map<string, RouteEntry>();
  const bySegment = new Map<string, RouteEntry>();

  for (const entry of entries) {
    byComponentPath.set(entry.componentPath, entry);
    bySegment.set(entry.urlSegment, entry);
  }

  return {
    segmentForComponentPath(componentPath: string): string | undefined {
      return byComponentPath.get(componentPath)?.urlSegment;
    },
    entryForSegment(segment: string): RouteEntry | undefined {
      return bySegment.get(segment);
    },
  };
}
