// valdi-perf skill reference — compile check for performance patterns.
// This file must compile cleanly against the Valdi TypeScript compiler.
// Run: bzl build //ai-skills/skills/valdi-perf/tests:tests

import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { createReusableCallback } from 'valdi_core/src/utils/Callback';
import { Layout, View } from 'valdi_tsx/src/NativeTemplateElements';

// ─── Module-level styles (defeats interning if created inside onRender) ───────

const styles = {
  card: new Style<View>({ backgroundColor: '#fff', borderRadius: 8, padding: 12 }),
  spacer: new Style<Layout>({ height: 16 }),
  row: new Style<View>({ flexDirection: 'row', alignItems: 'center' }),
};

// ─── Stable class property for constant arrays ───────────────────────────────

interface TabBarViewModel {
  tabs: string[];
}

class TabBar extends Component<TabBarViewModel> {
  onRender(): void {
    <view style={styles.row}>
      {this.viewModel.tabs.forEach(tab => {
        <label key={tab} value={tab} />;
      })}
    </view>;
  }
}

// ─── Pre-computed viewModel in onViewModelUpdate ─────────────────────────────

interface UserRowViewModel {
  name: string;
  subtitle: string;
}

class UserRow extends Component<UserRowViewModel> {
  onRender(): void {
    <view style={styles.card}>
      <label value={this.viewModel.name} />;
      <label value={this.viewModel.subtitle} />;
    </view>;
  }
}

interface UserListViewModel {
  user: { id: string; name: string; bio: string };
}

interface UserListState {
  selectedIndex: number;
}

class UserList extends StatefulComponent<UserListViewModel, UserListState> {
  state: UserListState = { selectedIndex: 0 };

  // Pre-computed to avoid new object on every render
  private userRowVM: UserRowViewModel = { name: '', subtitle: '' };

  onViewModelUpdate(previous?: UserListViewModel): void {
    if (this.viewModel.user.id !== previous?.user.id) {
      this.userRowVM = {
        name: this.viewModel.user.name,
        subtitle: this.viewModel.user.bio,
      };
    }
  }

  onRender(): void {
    // ✅ Stable reference — UserRow only re-renders when user.id changes
    <UserRow {...this.userRowVM} />;
  }
}

// ─── createReusableCallback: stable function for same arguments ───────────────

interface SectionViewModel {
  id: string;
  title: string;
}

interface SectionListViewModel {
  sections: SectionViewModel[];
}

interface SectionListState {
  selectedId: string | null;
}

class SectionList extends StatefulComponent<SectionListViewModel, SectionListState> {
  state: SectionListState = { selectedId: null };

  private handleSectionTap(id: string): void {
    this.setState({ selectedId: id });
  }

  onRender(): void {
    <view>
      {this.viewModel.sections.forEach(section => {
        // ✅ Inline Callback — Valdi merges identity, child doesn't re-render
        <view key={section.id} onTap={createReusableCallback(() => this.handleSectionTap(section.id))}>
          <label value={section.title} />;
        </view>;
      })}
    </view>;
  }
}

// ─── Stable render props as class arrow functions ────────────────────────────

interface Item {
  id: string;
  label: string;
}

interface FlatListViewModel {
  items: Item[];
  renderItem: (item: Item) => void;
}

class FlatList extends Component<FlatListViewModel> {
  onRender(): void {
    <scroll>
      {this.viewModel.items.forEach(item => {
        this.viewModel.renderItem(item);
      })}
    </scroll>;
  }
}

interface FeedViewModel {
  items: Item[];
}

class Feed extends Component<FeedViewModel> {
  // ✅ Stable reference — defined as class property, not inline in onRender
  private renderItem = (item: Item): void => {
    <view key={item.id}>
      <label value={item.label} />;
    </view>;
  };

  onRender(): void {
    <FlatList items={this.viewModel.items} renderItem={this.renderItem} />;
  }
}

// ─── <layout> vs <view>: no native view for invisible containers ──────────────

interface CardViewModel {
  title: string;
  subtitle: string;
}

class Card extends Component<CardViewModel> {
  onRender(): void {
    <view style={styles.card}>
      <label value={this.viewModel.title} />;
      {/* ✅ layout: no native view allocated for spacer */}
      <layout style={styles.spacer} />;
      {/* ✅ layout: invisible structural wrapper */}
      <layout flexDirection="row" alignItems="center">
        <label value={this.viewModel.subtitle} />;
      </layout>;
    </view>;
  }
}

// ─── Keys in lists: stable data ID, not index ────────────────────────────────

interface ListItem {
  id: string;
  text: string;
}

interface StableListViewModel {
  items: ListItem[];
}

class StableList extends Component<StableListViewModel> {
  onRender(): void {
    <scroll>
      {this.viewModel.items.forEach(item => {
        // ✅ Stable key from data ID — reorder/insert/remove works correctly
        <view key={item.id}>
          <label value={item.text} />;
        </view>;
      })}
    </scroll>;
  }
}

void TabBar;
void UserList;
void SectionList;
void Feed;
void Card;
void StableList;
