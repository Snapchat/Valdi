import { ElementRef } from 'valdi_core/src/ElementRef';
import 'jasmine/src/jasmine';
import { valdiIt } from './JSXTestUtils';

// The standalone test runtime must not replace Yoga's handlers for these attributes on <view>.
describe('standalone layout attributes on view', () => {
  valdiIt('applies justifyContent', async driver => {
    const secondRef = new ElementRef();

    driver.render(() => {
      <view width="100%" height="100%" flexDirection="row" justifyContent="space-between">
        <view width={20} height={20} />
        <view ref={secondRef} width={20} height={20} />
      </view>;
    });

    await driver.performLayout({ width: 100, height: 100 });

    expect(secondRef.single()!.frame.x).toBeCloseTo(80, 1);
  });

  valdiIt('applies padding', async driver => {
    const childRef = new ElementRef();

    driver.render(() => {
      <view width="100%" height="100%" padding={10}>
        <view ref={childRef} width={20} height={20} />
      </view>;
    });

    await driver.performLayout({ width: 100, height: 100 });

    expect(childRef.single()!.frame.x).toBeCloseTo(10, 1);
    expect(childRef.single()!.frame.y).toBeCloseTo(10, 1);
  });

  valdiIt('applies alignItems', async driver => {
    const childRef = new ElementRef();

    driver.render(() => {
      <view width="100%" height="100%" alignItems="center">
        <view ref={childRef} width={20} height={20} />
      </view>;
    });

    await driver.performLayout({ width: 100, height: 100 });

    expect(childRef.single()!.frame.x).toBeCloseTo(40, 1);
  });

  valdiIt('applies top', async driver => {
    const childRef = new ElementRef();

    driver.render(() => {
      <view width="100%" height="100%">
        <view ref={childRef} position="absolute" top={15} width={20} height={20} />
      </view>;
    });

    await driver.performLayout({ width: 100, height: 100 });

    expect(childRef.single()!.frame.y).toBeCloseTo(15, 1);
  });
});
