import { JSXProcessor } from './JSXProcessor';
import 'ts-jest';
import * as ts from 'typescript';
import { getSourceMap } from './SourceMapUtils';
import { trimAllLines } from './utils/StringUtils';

function sanitize(text: string): string {
  return trimAllLines(text);
}

function compile(text: string, includeSourceMapping?: boolean): string {
  const processor = JSXProcessor.createFromFile('File.jsx', text, !!includeSourceMapping);
  let result = processor.process();

  if (!includeSourceMapping) {
    result = sanitize(result);
  }

  return result;
}

// TODO(3521): Update module names to valdi

describe('JSXProcessor', () => {
  it('supports single element', () => {
    const result = compile(`<layout/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element with static number attribute', () => {
    const result = compile(`<layout width={42}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['width', 42]);
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element with static string attribute', () => {
    const result = compile(`<layout width={'42'}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['width', '42']);
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element with static bool attribute', () => {
    const result = compile(`<layout enableFeature/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['enableFeature', true]);
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element with dynamic attribute', () => {
    const result = compile(`<layout width={42 * 2}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributeNumber('width', 42 * 2);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element with dynamic and static attributes', () => {
    const result = compile(`<layout width={42 * 2} height={30}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['height', 30]);
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributeNumber('width', 42 * 2);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element children', () => {
    const result = compile(`
    <layout>
     <view/>
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element children with attributes', () => {
    const result = compile(`
    <layout width={42 * 2}>
     <view height={84 / 2}/>
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributeNumber('width', 42 * 2);
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.setAttributeNumber('height', 84 / 2);
    __Renderer.endRender();
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('throws when adding non empty JSX Text', () => {
    expect(() => {
      compile(`
      <layout>
        some text
        <view/>
      </layout>
      `);
    }).toThrow();
  });
  it('supports component', () => {
    const result = compile(`
    <MyComponent/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element key', () => {
    const result = compile(`<layout key={'myKey'}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, 'myKey');
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component key', () => {
    const result = compile(`<MyComponent key={'myKey'}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, 'myKey', undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element key from identifier', () => {
    const result = compile(`<layout key={myLayoutKey}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, myLayoutKey);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element key from expression', () => {
    const result = compile(`<layout key={viewModel.myLayoutKey}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, viewModel.myLayoutKey);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports element key from complex expression', () => {
    const result = compile(`
    const getKey = (obj) => { return obj.myLayoutKey; };
    <layout key={getKey(this.viewModel)}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const getKey = (obj) => { return obj.myLayoutKey; };
    __Renderer.beginRender(__nodeLayout1, getKey(this.viewModel));
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component key from identifier', () => {
    const result = compile(`<MyComponent key={myLayoutKey}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, myLayoutKey, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component key from expression', () => {
    const result = compile(`<MyComponent key={viewModel.myLayoutKey}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, viewModel.myLayoutKey, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component key from complex expression', () => {
    const result = compile(`
    const getKey = (obj) => { return obj.myLayoutKey; };
    <MyComponent key={getKey(this.viewModel)}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const getKey = (obj) => { return obj.myLayoutKey; };
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, getKey(this.viewModel), undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component ref from identifier', () => {
    const result = compile(`<MyComponent ref={someComponentRef}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, someComponentRef);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component ref from expression', () => {
    const result = compile(`<MyComponent ref={viewModel.someComponentRef}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, viewModel.someComponentRef);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component ref from complex expression', () => {
    const result = compile(`
    const getRef = (obj) => { return obj.myRef; };
    <MyComponent ref={getRef(this.viewModel)}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const getRef = (obj) => { return obj.myRef; };
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, getRef(this.viewModel));
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component context from identifier', () => {
    const result = compile(`<MyComponent context={someContext}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    if (!__Renderer.hasContext()) {
      __Renderer.setContext(someContext);
    }
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component context from expression', () => {
    const result = compile(`<MyComponent context={this.context}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    if (!__Renderer.hasContext()) {
      __Renderer.setContext(this.context);
    }
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component context from complex expression', () => {
    const result = compile(`
    const getContext = (obj) => { return obj.context; };
    <MyComponent context={getContext(this.viewModel)}/>;
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const getContext = (obj) => { return obj.context; };
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    if (!__Renderer.hasContext()) {
      __Renderer.setContext(getContext(this.viewModel));
    }
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports default slotted', () => {
    const result = compile(`
    <MyComponent>
      <layout/>
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setUnnamedSlot(() => {
      __Renderer.beginRender(__nodeLayout1, undefined);
      __Renderer.endRender();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports explicit slotted', () => {
    const result = compile(`
    <MyComponent>
      <slotted slot='mySlot'>
        <layout/>
      </slotted>
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setNamedSlot('mySlot', () => {
      __Renderer.beginRender(__nodeLayout1, undefined);
      __Renderer.endRender();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports expression in slotted', () => {
    const result = compile(`
    <MyComponent>
      <slotted slot={viewModel.destSlot}>
        <layout/>
      </slotted>
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setNamedSlot(viewModel.destSlot, () => {
      __Renderer.beginRender(__nodeLayout1, undefined);
      __Renderer.endRender();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports slot passed as expression', () => {
    const result = compile(`
    const name = 'header';
    <MyComponent>
    {
      $namedSlots({
        [name]: () => {
          <layout/>
        }
      })
    }
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const name = 'header';
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setNamedSlots({
      [name]: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports slot passed as expression with comments', () => {
    const result = compile(`
    const name = 'header';
    <MyComponent>
    {
      /* Pass back the header */
      $namedSlots({
        [name]: () => {
          <layout/>
        }
      })
    }
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const name = 'header';
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
      /* Pass back the header */
      __Renderer.setNamedSlots({
      [name]: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('optimizes named slots', () => {
    const result = compile(`
    <MyComponent>
    {
      $namedSlots({
        header: () => {
          <layout/>
        },
        body: () => {
          <view/>
        }
      })
    }
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setNamedSlot('header', () => {
      __Renderer.beginRender(__nodeLayout1, undefined);
      __Renderer.endRender();
    });
    __Renderer.setNamedSlot('body', () => {
      __Renderer.beginRender(__nodeView1, undefined);
      __Renderer.endRender();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports slot passed as lambda', () => {
    const result = compile(`
    <MyComponent>
    {
      $slot((text) => {
        <label value={text}/>
      })
    }
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __nodeLabel1 = __Renderer.makeNodePrototype('label');
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setUnnamedSlot((text) => {
      __Renderer.beginRender(__nodeLabel1, undefined);
      __Renderer.setAttribute('value', text);
      __Renderer.endRender();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('throws on orphan slotted elements', () => {
    expect(() => {
      compile(`
      <slotted slot='mySlot'>
        <layout/>
      </slotted>
      `);
    }).toThrow();
  });
  it('throws when mixing explicit and default slotted', () => {
    expect(() => {
      compile(`
      <MyComponent>
        <slotted slot={viewModel.destSlot}>
          <layout/>
        </slotted>
        <label/>
        <view/>
        <slotted slot='nice'>
          <scroll/>
        </slotted>
      </MyComponent>
      `);
    }).toThrow();
  });

  it('supports default slot', () => {
    const result = compile(`
    <slot/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderUnnamedSlot(this);
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports explicit slot', () => {
    const result = compile(`
    <slot name='mySlot'/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderNamedSlot('mySlot', this);
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports slot with expressions', () => {
    const result = compile(`
    <slot name={viewModel.aSlot}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderNamedSlot(viewModel.aSlot, this);
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports unnamed slot with key', () => {
    const result = compile(`
    <slot key={'myKey'}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderUnnamedSlot(this, undefined, 'myKey');
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports named slot with key', () => {
    const result = compile(`
    <slot name='mySlot' key={'myKey'}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderNamedSlot('mySlot', this, undefined, 'myKey');
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('throws when adding child to slot', () => {
    expect(() => {
      compile(`
        <slot>
          <layout/>
        </slot>
        `);
    }).toThrow();
  });
  it('throws when using dash in attribute', () => {
    expect(() => {
      compile(`
        <layout some-attribute={true}/>
        `);
    }).toThrow();
  });
  it('supports static viewModel property', () => {
    const result = compile(`
    <MyComponent hello='world' welcome={42}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype(['hello', 'world', 'welcome', 42]);
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports dynamic viewModel property', () => {
    const result = compile(`
    <MyComponent welcome={42 * 2}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setViewModelProperty('welcome', 42 * 2);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports combined static and dynamic viewModel properties', () => {
    const result = compile(`
    <MyComponent hello='world' welcome={42 * 2}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype(['hello', 'world']);
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setViewModelProperty('welcome', 42 * 2);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports viewModel spread', () => {
    const result = compile(`
    <MyComponent {...myProps}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setViewModelProperties(myProps);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports viewModel spread with appended property', () => {
    const result = compile(`
    <MyComponent {...myProps} nice='42'/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setViewModelProperties(myProps);
    __Renderer.setViewModelProperty('nice', '42');
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports viewModel spread with prepended property', () => {
    const result = compile(`
    <MyComponent nice='42' {...myProps}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype(['nice', '42']);
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setViewModelProperties(myProps);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports attributes spread', () => {
    const result = compile(`<layout {...myProps}/>`);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributes(myProps);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports attributes spread with appended property', () => {
    const result = compile(`
    <layout {...myProps} nice='42'/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributes(myProps);
    __Renderer.setAttribute('nice', '42');
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports attributes spread with prepended property', () => {
    const result = compile(`
    <layout nice='42' {...myProps}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['nice', '42']);
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributes(myProps);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports inner expression in element', () => {
    const result = compile(`
    <layout>
      {when(someValue, () => {})}
      <view/>
      {when(someOtherValue, () => {})}
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    {when(someValue, () => { })}
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    {when(someOtherValue, () => { })}
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports JSX inside inner expressions', () => {
    const result = compile(`
    <layout>
      {when(someValue, () => {
        <label/>
      })}
      <view/>
      {when(someOtherValue, () => {
        <label/>
      })}
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLabel1 = __Renderer.makeNodePrototype('label');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    const __nodeLabel2 = __Renderer.makeNodePrototype('label');
    __Renderer.beginRender(__nodeLayout1, undefined);
    {when(someValue, () => {
    __Renderer.beginRender(__nodeLabel1, undefined);
    __Renderer.endRender();
    })}
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    {when(someOtherValue, () => {
    __Renderer.beginRender(__nodeLabel2, undefined);
    __Renderer.endRender();
    })}
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('automatically injects block in single expression lambda', () => {
    const result = compile(`
    <layout>
      {when(someValue, () => <label/> )}
      <view/>
      {when(someOtherValue, () => <label/> )}
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLabel1 = __Renderer.makeNodePrototype('label');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    const __nodeLabel2 = __Renderer.makeNodePrototype('label');
    __Renderer.beginRender(__nodeLayout1, undefined);
    {when(someValue, () => {
    __Renderer.beginRender(__nodeLabel1, undefined);
    __Renderer.endRender();
    })}
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    {when(someOtherValue, () => {
    __Renderer.beginRender(__nodeLabel2, undefined);
    __Renderer.endRender();
    })}
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('transform JSX in element attributes', () => {
    const result = compile(`
    const items = ['a', 'b', 'c'];
    <element sections={items.map((section) => {
      return {
        key: section,
        onRenderBody: () => {
          <layout/>
        }
      }
    })}>
    </element>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeElement1 = __Renderer.makeNodePrototype('element');
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const items = ['a', 'b', 'c'];
    __Renderer.beginRender(__nodeElement1, undefined);
    __Renderer.setAttribute('sections', items.map((section) => {
      return {
        key: section,
        onRenderBody: () => {
          __Renderer.beginRender(__nodeLayout1, undefined);
          __Renderer.endRender();
        }
      };
    }));
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('transform JSX in viewModel properties', () => {
    const result = compile(`
    const items = ['a', 'b', 'c'];
    <MyComponent>
      <SectionList sections={items.map((section) => {
        return {
          key: section,
          onRenderHeader: () => {
            <SectionHeader title={section}/>
          },
          onRenderBody: () => {
            <layout/>
          }
        }
      })}/>
    </MyComponent>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    const __componentSectionList1 = __Renderer.makeComponentPrototype();
    const __componentSectionHeader1 = __Renderer.makeComponentPrototype();
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const items = ['a', 'b', 'c'];
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setUnnamedSlot(() => {
      __Renderer.beginComponent(SectionList, __componentSectionList1, undefined, undefined);
      __Renderer.setViewModelProperty('sections', items.map((section) => {
        return {
          key: section,
          onRenderHeader: () => {
            __Renderer.beginComponent(SectionHeader, __componentSectionHeader1, undefined, undefined);
            __Renderer.setViewModelProperty('title', section);
            __Renderer.endComponent();
          },
          onRenderBody: () => {
            __Renderer.beginRender(__nodeLayout1, undefined);
            __Renderer.endRender();
          }
        };
      }));
      __Renderer.endComponent();
    });
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports JSX in ternary', () => {
    const result = compile(`
    <layout>
      {someValue ? <layout/> : undefined}
      {someValue ? undefined : <view/>}
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLayout2 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    {someValue ? (() => {
    __Renderer.beginRender(__nodeLayout2, undefined);
    __Renderer.endRender();
    })() : undefined}
    {someValue ? undefined : (() => {
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    })()}
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('avoids wrapping in pure JSX expressions', () => {
    const result = compile(`
    <layout>
      {<layout/>}
      <view/>
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLayout2 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.beginRender(__nodeLayout2, undefined);
    __Renderer.endRender();
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('removes parenthesized expressions', () => {
    const result = compile(`
    (
      <layout>
        <view/>
      </layout>
    );
    ('this should be kept')
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    __Renderer.endRender();
    ('this should be kept');
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports component class from expression', () => {
    const result = compile(`
    <viewModel.componentClass {...viewModel.properties}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentViewModelcomponentClass1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(viewModel.componentClass, __componentViewModelcomponentClass1, undefined, undefined);
    __Renderer.setViewModelProperties(viewModel.properties);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('can render in functions', () => {
    const result = compile(`
    function onRender() {
      const text = this.viewModel.text;
      <layout>
        <label value={text}/>
      </layout>;
      return true;
    }
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLabel1 = __Renderer.makeNodePrototype('label');
    function onRender() {
    const text = this.viewModel.text;
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.beginRender(__nodeLabel1, undefined);
    __Renderer.setAttribute('value', text);
    __Renderer.endRender();
    __Renderer.endRender();
    return true;
    }
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('supports complex nested hierarchy', () => {
    const result = compile(`
    function onRender() {
      <MyComponent height={42}>
        <layout>
          {!this.viewModel.showTitle || <label/>}
          <ChildComponent someProp={this.viewModel.prop}/>
          {renderBody()}
        </layout>
        <view/>
      </MyComponent>
    }
    function renderBody() {
      <layout>
        <view/>
      </layout>
    }
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype(['height', 42]);
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeLabel1 = __Renderer.makeNodePrototype('label');
    const __componentChildComponent1 = __Renderer.makeComponentPrototype();
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    const __nodeLayout2 = __Renderer.makeNodePrototype('layout');
    const __nodeView2 = __Renderer.makeNodePrototype('view');
    function onRender() {
      __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
      __Renderer.setUnnamedSlot(() => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        {!this.viewModel.showTitle || (() => {
          __Renderer.beginRender(__nodeLabel1, undefined);
          __Renderer.endRender();
        })()}
        __Renderer.beginComponent(ChildComponent, __componentChildComponent1, undefined, undefined);
        __Renderer.setViewModelProperty('someProp', this.viewModel.prop);
        __Renderer.endComponent();
        {renderBody()}
        __Renderer.endRender();
        __Renderer.beginRender(__nodeView1, undefined);
        __Renderer.endRender();
      });
      __Renderer.endComponent();
    }
    function renderBody() {
    __Renderer.beginRender(__nodeLayout2, undefined);
    __Renderer.beginRender(__nodeView2, undefined);
    __Renderer.endRender();
    __Renderer.endRender();
    }
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('append declarations after use strict', () => {
    const result = compile(`
    'use strict';
    <layout/>
    `);
    const expected = `
    'use strict';
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports lazy', () => {
    const result = compile(`
    <layout lazy>
      <map/>
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout', ['lazy', true]);
    const __nodeMap1 = __Renderer.makeNodePrototype('map');
    if (__Renderer.beginRenderIfNeeded(__nodeLayout1, undefined)) {
      __Renderer.beginRender(__nodeMap1, undefined);
      __Renderer.endRender();
      __Renderer.endRender();
    }
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('ignores JSX text with semicolon', () => {
    const result = compile(`
    <layout>
      <view/>;
      <layout/>; ;;; ;
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    const __nodeLayout2 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.beginRender(__nodeView1, undefined);
    __Renderer.endRender();
    __Renderer.beginRender(__nodeLayout2, undefined);
    __Renderer.endRender();
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports element ref', () => {
    const result = compile(`
    <layout ref={someRefSomewhere}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    __Renderer.setAttributeRef(someRefSomewhere);
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports element ref in slot', () => {
    const result = compile(`
    <slot ref={someRefSomewhere}/>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    __Renderer.renderUnnamedSlot(this, someRefSomewhere);
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports function component', () => {
    const result = compile(`
    function MyCard() {

    }
    class MyCardComponent {

    }

    <MyCard/>;
    <MyCardComponent />;
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyCardComponent1 = __Renderer.makeComponentPrototype();
    function MyCard() {

    }
    class MyCardComponent {

    }

    __Renderer.renderFnComponent(MyCard, {});
    __Renderer.beginComponent(MyCardComponent, __componentMyCardComponent1, undefined, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports inline function components via a variable', () => {
    const result = compile(`
    function MyCardOriginal() {

    }

    const MyCard = MyCardOriginal;

    class MyCardComponent {

    }

    <MyCard/>;
    <MyCardComponent />;
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyCardComponent1 = __Renderer.makeComponentPrototype();
    function MyCardOriginal() {

    }
    const MyCard = MyCardOriginal;
    class MyCardComponent {

    }

    __Renderer.renderFnComponent(MyCard, {});
    __Renderer.beginComponent(MyCardComponent, __componentMyCardComponent1, undefined, undefined);
    __Renderer.endComponent();
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports inline function component through declare', () => {
    const result = compile(`
    interface ViewModel {
      text: string;
      numberOfLines?: number;
      marginBottom?: number;
  }
  export declare const DialogTitle: ({ text, numberOfLines, marginBottom }: ViewModel) => void;

  export const PublishingDialog = (): void => {
      <DialogTitle text={'Hello world'} />
  };
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;

    export const PublishingDialog = () => {
      __Renderer.renderFnComponent(DialogTitle, {
        text: 'Hello world'
      });
    };

    `;
    expect(result).toBe(sanitize(expected));
  });

  it('can pass children to function component', () => {
    const result = compile(`
    function MyCard(viewModel) {
      viewModel.children();
    }

    <MyCard>
      <layout/>
    </MyCard>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    function MyCard(viewModel) {
      viewModel.children();
    }

    __Renderer.renderFnComponent(MyCard, {
      children: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('can pass attribute to function component', () => {
    const result = compile(`
    function MyCard(viewModel) {
    }

    <MyCard hello={'world'}/>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    function MyCard(viewModel) {
    }

    __Renderer.renderFnComponent(MyCard, {
      hello: 'world'
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('can pass attribute and children combined to a function component', () => {
    const result = compile(`
    function MyCard(viewModel) {
      viewModel.children();
    }

    <MyCard hello={'world'}>
      <layout/>
    </MyCard>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    function MyCard(viewModel) {
      viewModel.children();
    }

    __Renderer.renderFnComponent(MyCard, {
      hello: 'world',
      children: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports jsx in function component body', () => {
    const result = compile(`
    function MyCard(viewModel) {
      <view backgroundColor='red'>
        {viewModel.children()}
      </view>
    }

    <MyCard hello={'world'}>
      <layout/>
    </MyCard>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeView1 = __Renderer.makeNodePrototype('view', ['backgroundColor', 'red']);
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    function MyCard(viewModel) {
      __Renderer.beginRender(__nodeView1, undefined);
      {viewModel.children()}
      __Renderer.endRender();
    }

    __Renderer.renderFnComponent(MyCard, {
      hello: 'world',
      children: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('supports higher order function components', () => {
    const result = compile(`
    const withRedBackground = (innerFunctionComponent: () => void) => (viewModel) => {
      <view backgroundColor='red'>
        {innerFunctionComponent(viewModel)}
      </view>
    }

    function MyCardOriginal(viewModel) {
      viewModel.children()
    }

    const MyCard = withRedBackground(MyCardOriginal);

    <MyCard hello={'world'}>
      <layout/>
    </MyCard>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeView1 = __Renderer.makeNodePrototype('view', ['backgroundColor', 'red']);
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const withRedBackground = (innerFunctionComponent) => (viewModel) => {
      __Renderer.beginRender(__nodeView1, undefined);
      {innerFunctionComponent(viewModel)}
      __Renderer.endRender();
    };
    function MyCardOriginal(viewModel) {
      viewModel.children();
    }
    const MyCard = withRedBackground(MyCardOriginal);
    __Renderer.renderFnComponent(MyCard, {
      hello: 'world',
      children: () => {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.endRender();
      }
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('can pass spread attributes to function component', () => {
    const result = compile(`
    function MyCard(viewModel) {
    }

    <MyCard hello='world' {...someObject} welcome='to_paradise'/>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    function MyCard(viewModel) {
    }

    __Renderer.renderFnComponent(MyCard, {
      hello: 'world',
      ...someObject,
      welcome: 'to_paradise'
    });
    `;
    expect(result).toBe(sanitize(expected));
  });

  it('can generate source maps', () => {
    const jsxToCompile = `
function onRender(width, height) {
  <layout width={width}>
    <view height={height}/>
  </layout>
}
  `.trim();

    const result = compile(jsxToCompile, true);

    const sourceMapConsumer = getSourceMap(result)!;
    expect(sourceMapConsumer).toBeTruthy();

    /**
     * Emitted source file:
     *
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    function onRender(width, height) {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.setAttribute('width', width);
        __Renderer.beginRender(__nodeView1, undefined);
        __Renderer.setAttribute('height', height);
        __Renderer.endRender();
        __Renderer.endRender();
    }
     */

    // Check that source maps point back to the original JSX

    // get location of onRender()
    let position = sourceMapConsumer.originalPositionFor({ line: 4, column: 9 });
    expect(position.line).toBe(1);
    expect(position.column).toBe(9);

    // Get location of the first beginRender()
    position = sourceMapConsumer.originalPositionFor({ line: 5, column: 4 });
    expect(position.line).toBe(2);
    expect(position.column).toBe(3);

    // Get location of the last endRender()
    position = sourceMapConsumer.originalPositionFor({ line: 10, column: 4 });
    expect(position.line).toBe(4);
    expect(position.column).toBe(4);

    // Get location of width attribute
    position = sourceMapConsumer.originalPositionFor({ line: 6, column: 4 });
    expect(position.line).toBe(2);
    expect(position.column).toBe(10);

    // Get location of height attribute
    position = sourceMapConsumer.originalPositionFor({ line: 8, column: 4 });
    expect(position.line).toBe(3);
    expect(position.column).toBe(10);
  });

  it('merges sources map', () => {
    // We first compile a tsx into jsx
    const tsx = `
enum Size {
  SMALL = 40,
  LARGE = 100,
}

interface ViewModel {
  size: Size;
  height: number;
}

function onRender(viewModel: ViewModel) {
  <layout width={viewModel.size}>
    <view height={viewModel.height}/>
  </layout>
}
    `.trim();

    let compilerOptions: ts.CompilerOptions = {
      target: ts.ScriptTarget.ES2015,
      jsx: ts.JsxEmit.Preserve,
      removeComments: false,
      sourceMap: true,
      sourceRoot: '/',
      inlineSourceMap: true,
      inlineSources: true,
    };

    const jsx = ts.transpileModule(tsx, {
      compilerOptions: compilerOptions,
      fileName: 'MyFile.tsx',
    }).outputText;

    // We now compile the JSX into JS
    const result = compile(jsx, true);

    /**
     * Emitted source file:
     *
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    const __nodeView1 = __Renderer.makeNodePrototype('view');
    const Size;
    (function (Size) {
        Size[Size["SMALL"] = 40] = "SMALL";
        Size[Size["LARGE"] = 100] = "LARGE";
    })(Size || (Size = {}));
    function onRender(viewModel) {
        __Renderer.beginRender(__nodeLayout1, undefined);
        __Renderer.setAttribute('width', viewModel.size);
        __Renderer.beginRender(__nodeView1, undefined);
        __Renderer.setAttribute('height', viewModel.height);
        __Renderer.endRender();
        __Renderer.endRender();
    }
     */

    const sourceMap = getSourceMap(result)!;
    expect(sourceMap).toBeTruthy();

    // We should be able to map a beginRender back to the original TS file

    let position = sourceMap.originalPositionFor({ line: 10, column: 4 });
    expect(position.line).toBe(12);
    expect(position.column).toBe(3);

    // We should also be able to map a TS value back to the original TS file

    position = sourceMap.originalPositionFor({ line: 6, column: 32 });
    expect(position.line).toBe(2);
    expect(position.column).toBe(2);
  });

  it('ignores comments in slotted', () => {
    const result = compile(`
    <MyComponent>
      {/* Header */}
      <slotted slot='header'/>
      {/* Body */}
      <slotted slot='body'/>
    </MyComponent>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setNamedSlot('header', () => {
    });
    __Renderer.setNamedSlot('body', () => {
    });
    __Renderer.endComponent();
    `;

    expect(result).toBe(sanitize(expected));
  });

  it('supports injected attributes', () => {
    const result = compile(`
    <MyComponent $width={42}/>
    `);

    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __componentMyComponent1 = __Renderer.makeComponentPrototype();
    __Renderer.beginComponent(MyComponent, __componentMyComponent1, undefined, undefined);
    __Renderer.setInjectedAttribute('$width', 42);
    __Renderer.endComponent();
    `;

    expect(result).toBe(sanitize(expected));
  });

  it('fails when passing unbound function', () => {
    expect(() => {
      compile(`
      class MyComponent {
        onRender() {
          <view onTap={this.myCallback}/>
        }
        myCallback() {
        }
      }
      `);
    }).toThrow();
  });

  it('succeeds when passing bound function', () => {
    compile(`
      class MyComponent {
        onRender() {
          <view onTap={this.myCallback}/>
        }
        myCallback = () => {
        }
      }
    `);
  });

  it('preserves comments', () => {
    const result = compile(`
    // The following is a JSX expression:
    <layout>
      {/* This is a JSX comment */}
    </layout>
    `);
    const expected = `
    const __Renderer = require('valdi_core/src/JSX').jsx;
    const __nodeLayout1 = __Renderer.makeNodePrototype('layout');
    __Renderer.beginRender(__nodeLayout1, undefined);
    {/* This is a JSX comment */}
    __Renderer.endRender();
    `;
    expect(result).toBe(sanitize(expected));
  });
  it('emits setAttributeString for custom-view androidClass and webClass', () => {
    const result = compile(`
    const AC = 'com.example.FooView';
    const WC = 'WebFooView';
    <custom-view androidClass={AC} webClass={WC} width="100%" height={120}/>
    `);
    expect(result).toContain("__Renderer.setAttributeString('androidClass', AC)");
    expect(result).toContain("__Renderer.setAttributeString('webClass', WC)");
    expect(result).toContain("makeNodePrototype('custom-view'");
  });

  it('emits setAttributeString for all custom-view class attrs (androidClass, iosClass, macosClass, webClass)', () => {
    const result = compile(`
    const AC = 'com.snap.modules.valdi_polyglot.FilePickerView';
    const IC = 'SCFilePickerView';
    const MC = 'SCFilePickerView';
    const WC = 'ValdiPolyglotFilePicker';
    <custom-view androidClass={AC} iosClass={IC} macosClass={MC} webClass={WC} width="100%" height={120}/>
    `);
    expect(result).toContain("__Renderer.setAttributeString('androidClass', AC)");
    expect(result).toContain("__Renderer.setAttributeString('iosClass', IC)");
    expect(result).toContain("__Renderer.setAttributeString('macosClass', MC)");
    expect(result).toContain("__Renderer.setAttributeString('webClass', WC)");
  });
});
