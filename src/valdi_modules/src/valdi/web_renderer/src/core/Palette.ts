export interface ColorPalette {
  [key: string]: string;
}

export class ColorPaletteManager {
  private readonly palettesByName = new Map<string, ColorPalette>();
  private readonly changeListeners = new Set<() => void>();
  private activeColorPaletteName = 'default';

  configureColorPalette(name: string, palette: ColorPalette): void {
    this.palettesByName.set(name, palette);
    this.notifyChangeListeners();
  }

  getColorPalette(name?: string): ColorPalette | undefined {
    return this.palettesByName.get(name ?? this.activeColorPaletteName);
  }

  getActiveColorPaletteName(): string {
    return this.activeColorPaletteName;
  }

  setActiveColorPalette(name: string): void {
    if (this.activeColorPaletteName === name) {
      return;
    }
    this.activeColorPaletteName = name;
    this.notifyChangeListeners();
  }

  resolveColor(paletteName: string, value: string): string {
    return this.getColorPalette(paletteName)?.[value] ?? value;
  }

  addChangeListener(listener: () => void): () => void {
    this.changeListeners.add(listener);
    return () => {
      this.changeListeners.delete(listener);
    };
  }

  private notifyChangeListeners(): void {
    const listeners = Array.from(this.changeListeners);
    for (const listener of listeners) {
      try {
        listener();
      } catch (error) {
        console.error('Valdi web renderer palette listener failed', error);
      }
    }
  }
}

export const COLOR_PALETTE_MANAGER = new ColorPaletteManager();
