// valdi-component-tests skill reference — demonstrates all test utility patterns.
// Run: bzl test //ai-skills/skills/valdi-component-tests/tests:tests

import { componentGetElements } from 'foundation/test/util/componentGetElements';
import { componentTypeFind } from 'foundation/test/util/componentTypeFind';
import { elementKeyFind } from 'foundation/test/util/elementKeyFind';
import { elementTypeFind } from 'foundation/test/util/elementTypeFind';
import { tapNodeWithKey } from 'foundation/test/util/tapNodeWithKey';
import 'jasmine/src/jasmine';
import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';
import { valdiIt } from 'valdi_test/test/JSXTestUtils';
import { ImageView, View } from 'valdi_tsx/src/NativeTemplateElements';
import { FollowBadge, ProfileCard, ProfileCardViewModel } from '../src/ProfileCard';

// ─── Factory function with explicit return type ───────────────────────────────

const makeViewModel = (): ProfileCardViewModel => ({
  avatarUrl: 'https://example.com/avatar.png',
  name: 'Alice',
  isFollowing: false,
  onFollowTap: fail.bind(null, 'onFollowTap should not be called'),
});

// ─── Tests ───────────────────────────────────────────────────────────────────

describe('ProfileCardTest', () => {

  // elementKeyFind<ImageView>: typed access to src attribute
  valdiIt('Verify avatar src is bound from viewModel', async driver => {
    const nodes = driver.render(() => {
      <ProfileCard {...makeViewModel()} />;
    });
    expect(
      elementKeyFind<ImageView>(componentGetElements(nodes[0].component!), 'avatar')[0]
        ?.getAttribute('src'),
    ).toBe('https://example.com/avatar.png');
  });

  // elementKeyFind: untyped access to string attribute
  valdiIt('Verify name label is bound from viewModel', async driver => {
    const nodes = driver.render(() => {
      <ProfileCard {...makeViewModel()} />;
    });
    expect(
      elementKeyFind(componentGetElements(nodes[0].component!), 'name-label')[0]
        ?.getAttribute('value'),
    ).toBe('Alice');
  });

  // elementTypeFind: find all Label elements without key attributes
  valdiIt('Verify label count when not following', async driver => {
    const nodes = driver.render(() => {
      <ProfileCard {...makeViewModel()} />;
    });
    const labels = elementTypeFind(
      componentGetElements(nodes[0].component!),
      IRenderedElementViewClass.Label,
    );
    // name-label + follow button label = 2 (FollowBadge not rendered)
    expect(labels.length).toBe(2);
  });

  // componentTypeFind: conditional child component absent
  valdiIt('Verify FollowBadge is absent when isFollowing is false', async driver => {
    const nodes = driver.render(() => {
      <ProfileCard {...makeViewModel()} />;
    });
    expect(
      componentTypeFind(nodes[0].component as ProfileCard, FollowBadge).length,
    ).toBe(0);
  });

  // componentTypeFind: conditional child component present
  valdiIt('Verify FollowBadge is present when isFollowing is true', async driver => {
    const nodes = driver.render(() => {
      <ProfileCard {...{ ...makeViewModel(), isFollowing: true }} />;
    });
    expect(
      componentTypeFind(nodes[0].component as ProfileCard, FollowBadge).length,
    ).toBe(1);
  });

  // tapNodeWithKey: async tap triggers callback
  valdiIt('Verify onFollowTap fires when follow button is tapped', async driver => {
    const onFollowTap = jasmine.createSpy('onFollowTap');
    const nodes = driver.render(() => {
      <ProfileCard {...{ ...makeViewModel(), onFollowTap }} />;
    });
    await tapNodeWithKey(nodes[0].component!, 'follow-button');
    expect(onFollowTap).toHaveBeenCalled();
  });

  // elementKeyFind<View>: typed access to onTap via getAttribute
  valdiIt('Verify onTap is set on follow button', async driver => {
    const onFollowTap = jasmine.createSpy('onFollowTap');
    const nodes = driver.render(() => {
      <ProfileCard {...{ ...makeViewModel(), onFollowTap }} />;
    });
    const button = elementKeyFind<View>(
      componentGetElements(nodes[0].component!),
      'follow-button',
    )[0];
    button?.getAttribute('onTap')?.({} as any);
    expect(onFollowTap).toHaveBeenCalled();
  });

});
