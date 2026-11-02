import { Component } from 'valdi_core/src/Component';
import { ElementRef } from 'valdi_core/src/ElementRef';
import 'jasmine/src/jasmine';
import { valdiIt } from './JSXTestUtils';

describe('ComponentTestExample', () => {
  valdiIt('can create component', async driver => {
    class MyComponent extends Component {}

    const nodes = driver.render(() => {
      <MyComponent />;
    });

    expect(nodes.length).toBe(1);

    expect(nodes[0].component instanceof MyComponent).toBeTruthy();
  });

  valdiIt('can create element', async driver => {
    const nodes = driver.render(() => {
      <layout />;
    });

    expect(nodes.length).toBe(1);
    expect(nodes[0].element).toBeTruthy();
    expect(nodes[0].element?.viewClass).toEqual('Layout');
  });

  valdiIt('can perform layout', async driver => {
    const rootRef = new ElementRef();
    const childRef = new ElementRef();

    driver.render(() => {
      <layout ref={rootRef} width="100%" height="100%" justifyContent="center" alignItems="center">
        <layout ref={childRef} width={25} height={25} />
      </layout>;
    });

    await driver.performLayout({ width: 100, height: 100 });

    expect(rootRef.single()!.frame).toEqual({ x: 0, y: 0, width: 100, height: 100 });
    expect(childRef.single()!.frame).toEqual({ x: 37.5, y: 37.5, width: 25, height: 25 });
  });
});
