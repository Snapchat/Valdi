import 'jasmine/src/jasmine';
import { Component } from 'valdi_core/src/Component';
import { ValdiHTMLRenderer } from '../src/ValdiHTMLRenderer';

interface GreetingViewModel {
  readonly name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <view flexDirection='column' padding={24} backgroundColor='#ff0000'>
      <label value={`Hello ${this.viewModel.name}`} />
    </view>;
  }
}

describe('ValdiHTMLRenderer', () => {
  it('renders a component to HTML', async () => {
    const renderer = new ValdiHTMLRenderer(Greeting, {}, { hasUpdatedHtml: () => {} });
    try {
      await renderer.render({ name: 'server' });
      const html = renderer.currentHtml();

      expect(html).toMatch(/^<\?start name="valdi-html-\d+">/);
      expect(html).toContain('Hello server');
      expect(html).toContain('flex-direction: column');
      expect(html).toContain('padding-top: 24px');
      expect(html).toContain('background-color: #ff0000');
    } finally {
      renderer.destroy();
    }
  });

  it('emits a template update when the view model changes', async () => {
    let updates = 0;
    const renderer = new ValdiHTMLRenderer(Greeting, {}, { hasUpdatedHtml: () => updates++ });
    try {
      await renderer.render({ name: 'first' });
      await renderer.render({ name: 'second' });
      await new Promise(resolve => setTimeout(resolve, 50));

      expect(updates).toBeGreaterThan(0);
      expect(renderer.currentHtml()).toMatch(/^<template for="valdi-html-\d+">/);
      expect(renderer.currentHtml()).toContain('Hello second');
    } finally {
      renderer.destroy();
    }
  });
});
