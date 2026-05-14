// Simple component used by the component-tests skill reference spec.
// Exercises: elementKeyFind, elementTypeFind, componentTypeFind, tapNodeWithKey.

import { Component } from 'valdi_core/src/Component';

// ─── FollowBadge: child component (used for componentTypeFind) ────────────────

export interface FollowBadgeViewModel {
  label: string;
}

export class FollowBadge extends Component<FollowBadgeViewModel> {
  onRender(): void {
    <label key="badge-label" value={this.viewModel.label} />;
  }
}

// ─── ProfileCard ──────────────────────────────────────────────────────────────

export interface ProfileCardViewModel {
  avatarUrl: string;
  name: string;
  isFollowing: boolean;
  onFollowTap: () => void;
}

export class ProfileCard extends Component<ProfileCardViewModel> {
  onRender(): void {
    <view key="card-container">
      <image key="avatar" src={this.viewModel.avatarUrl} />;
      <label key="name-label" value={this.viewModel.name} />;
      {this.viewModel.isFollowing && <FollowBadge label="Following" />}
      <view key="follow-button" onTap={this.viewModel.onFollowTap}>
        <label value="Follow" />;
      </view>;
    </view>;
  }
}
