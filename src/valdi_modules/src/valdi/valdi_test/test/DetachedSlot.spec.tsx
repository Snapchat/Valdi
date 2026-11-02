import { DetachedSlot } from 'valdi_core/src/slot/DetachedSlot';
import { DetachedSlotRenderer } from 'valdi_core/src/slot/DetachedSlotRenderer';
import 'jasmine/src/jasmine';
import { valdiIt } from './JSXTestUtils';

describe('DetachedSlot', () => {
  valdiIt('can render zone', async driver => {
    const slot = new DetachedSlot();

    const nodes = driver.render(() => {
      <view>
        <layout>
          <view />
        </layout>
        <layout>
          <DetachedSlotRenderer detachedSlot={slot} />
        </layout>
      </view>;
    });

    expect(nodes.length).toBe(1);
    expect(nodes[0].children.length).toBe(2);
    expect(nodes[0].children[1].children.length).toBe(1);

    const renderZoneRendererNode = nodes[0].children[1].children[0];

    // The zone should be initially empty
    expect(renderZoneRendererNode.children.length).toBe(0);

    slot.slotted(() => {
      <label value="Hello" />;
    });

    // The zone should now have a child
    expect(renderZoneRendererNode.children.length).toBe(1);
    expect(renderZoneRendererNode.children[0].element?.viewClass).toBe('SCValdiLabel');
  });

  valdiIt('can update zone', async driver => {
    const slot = new DetachedSlot();

    slot.slotted(() => {
      <label value="Hello" />;
    });

    const nodes = driver.render(() => {
      <view>
        <layout>
          <view />
        </layout>
        <layout>
          <DetachedSlotRenderer detachedSlot={slot} />
        </layout>
      </view>;
    });

    const renderZoneRendererNode = nodes[0]?.children[1]?.children[0];
    expect(renderZoneRendererNode).toBeTruthy();

    expect(renderZoneRendererNode.children.length).toBe(1);
    expect(renderZoneRendererNode.children[0].element?.viewClass).toBe('SCValdiLabel');

    slot.slotted(() => {
      <view />;
    });

    expect(renderZoneRendererNode.children.length).toBe(1);
    expect(renderZoneRendererNode.children[0].element?.viewClass).toBe('SCValdiView');

    slot.slotted(() => {});

    expect(renderZoneRendererNode.children.length).toBe(0);
  });

  valdiIt('can render zone in same render pass', async driver => {
    const slot = new DetachedSlot();

    const nodes = driver.render(() => {
      <view>
        <layout>
          <view />
        </layout>
        <layout>
          <DetachedSlotRenderer detachedSlot={slot} />
        </layout>
      </view>;

      slot.slotted(() => {
        <label value="Hello" />;
      });
    });

    const renderZoneRendererNode = nodes[0]?.children[1]?.children[0];
    expect(renderZoneRendererNode).toBeTruthy();

    expect(renderZoneRendererNode.children.length).toBe(1);
    expect(renderZoneRendererNode.children[0].element?.viewClass).toBe('SCValdiLabel');
  });
});
