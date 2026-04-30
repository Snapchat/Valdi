// Expected Valdi migrations — used to validate skill output correctness.
// Each section corresponds to a source snippet in flutter/compose/react examples.

import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import {
  ProvidersValuesViewModel,
  withProviders,
} from 'valdi_core/src/provider/withProviders';
import { CancelablePromise } from 'valdi_core/src/CancelablePromise';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { HTTPResponse } from 'valdi_http/src/HTTPTypes';
import { PersistentStore } from 'persistence/src/PersistentStore';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

// Stub for the async data-fetching function referenced in examples
declare function fetchProfile(id: string): Promise<{ name: string }>;

// ============================================================
// Theme service + Provider
// ============================================================

class AppThemeService {
  primary = '#FFFC00';
}
const ThemeProvider = createProviderComponentWithKeyName<AppThemeService>('ThemeProvider');

// ============================================================
// Stateless greeting (Flutter Greeting / Compose Greeting / React Greeting)
// ============================================================

interface GreetingViewModel {
  name: string;
}

class Greeting extends Component<GreetingViewModel> {
  onRender(): void {
    <label value={`Hello, ${this.viewModel.name}`} />;
  }
}

// ============================================================
// Stateful counter (Flutter Counter / Compose Counter / React Counter)
// ============================================================

interface CounterViewModel {
  label: string;
}

interface CounterState {
  count: number;
}

class Counter extends StatefulComponent<CounterViewModel, CounterState> {
  state = { count: 0 };

  // Class arrow function — never inline lambda in JSX props
  private increment = () => {
    this.setState({ count: this.state.count + 1 });
  };

  onRender(): void {
    <view onTap={this.increment}>
      <label value={`${this.viewModel.label}: ${this.state.count}`} />;
    </view>;
  }
}

// ============================================================
// Data loading (Flutter _UserListState / Compose UserProfile / React UserList)
// ============================================================

interface UserProfileViewModel {
  userId: string;
}
interface UserProfileState {
  profile: { name: string } | null;
}

class UserProfile extends StatefulComponent<UserProfileViewModel, UserProfileState> {
  state: UserProfileState = { profile: null };

  onCreate(): void {
    this.loadProfile(this.viewModel.userId);
  }

  onViewModelUpdate(previous?: UserProfileViewModel): void {
    if (this.viewModel.userId !== previous?.userId) {
      this.loadProfile(this.viewModel.userId);
    }
  }

  private async loadProfile(id: string) {
    const profile = await fetchProfile(id);
    this.setState({ profile });
  }

  onRender(): void {
    if (!this.state.profile) {
      <spinner />;
      return;
    }
    <label value={this.state.profile.name} />;
  }
}

// ============================================================
// List (Flutter ListView.builder / Compose LazyColumn / React FlatList)
// ============================================================

interface User { id: string; name: string; }

interface UserListViewModel {
  users: User[];
}

class UserList extends Component<UserListViewModel> {
  onRender(): void {
    // forEach, not map() — JSX is emitted as side-effects
    <scroll>
      {this.viewModel.users.forEach(user => {
        <label key={user.id} value={user.name} />;
      })}
    </scroll>;
  }
}

// ============================================================
// Layout (Flutter ProfileCard / Compose ProfileCard)
// ============================================================

interface ProfileCardViewModel {
  name: string;
  handle: string;
  bio: string;
}

class ProfileCard extends Component<ProfileCardViewModel> {
  onRender(): void {
    <view flexDirection="column" width="100%" padding={16}>
      <view flexDirection="row" width="100%">
        <label value={this.viewModel.name} font={systemFont(20)} />;
        <layout flexGrow={1} />;
        <label value={this.viewModel.handle} />;
      </view>;
      <layout height={8} />;
      <label value={this.viewModel.bio} />;
    </view>;
  }
}

// ============================================================
// Styled card (React styled-components Card)
// Style objects defined after the class, grouped in a const styles object
// ============================================================

interface CardViewModel {
  name: string;
  bio: string;
}

class ProfileCardStyled extends Component<CardViewModel> {
  onRender(): void {
    <view style={styles.card}>
      <label value={this.viewModel.name} font={systemFont(18)} />;
      <label value={this.viewModel.bio} />;
    </view>;
  }
}

const styles = {
  card: new Style<View>({
    backgroundColor: '#ffffff',
    borderRadius: 12,
    boxShadow: '0 2 8 rgba(0,0,0,0.15)',
    padding: 16,
  }),
};

// ============================================================
// Provider consumer (Flutter ThemedButton / Compose ThemedBadge / React ThemedBadge)
// ============================================================

interface ThemedBadgeViewModel extends ProvidersValuesViewModel<[AppThemeService]> {
  text: string;
}

class ThemedBadge extends Component<ThemedBadgeViewModel> {
  onRender(): void {
    const [theme] = this.viewModel.providersValues;
    <view backgroundColor={theme.primary}>
      <label value={this.viewModel.text} />;
    </view>;
  }
}
const ThemedBadgeWithProvider = withProviders(ThemeProvider)(ThemedBadge);

// ============================================================
// Persistent storage (Flutter SharedPreferences / React AsyncStorage)
// ============================================================

const settingsStore = new PersistentStore('settings');

interface SettingsState {
  notifications: boolean;
}

class SettingsScreen extends StatefulComponent<{}, SettingsState> {
  state = { notifications: false };

  async onCreate() {
    const raw = await settingsStore.fetchString('notifications');
    this.setState({ notifications: raw === 'true' });
  }

  private toggle = async () => {
    const next = !this.state.notifications;
    await settingsStore.storeString('notifications', String(next));
    this.setState({ notifications: next });
  };

  onRender(): void {
    <view flexDirection="column">
      <label value={`Notifications: ${this.state.notifications}`} />;
      <view onTap={this.toggle}>
        <label value="Toggle" />;
      </view>;
    </view>;
  }
}

// ============================================================
// HTTP fetch (React UserList with fetch / Flutter _loadUsers)
// ============================================================

interface FetchState {
  users: User[];
}

class UserListFetch extends StatefulComponent<{}, FetchState> {
  state: FetchState = { users: [] };

  private client = new HTTPClient('https://api.example.com');
  private request?: CancelablePromise<HTTPResponse>;

  onCreate(): void {
    this.request = this.client.get('/users');
    this.request.then(response => {
      const users = JSON.parse(new TextDecoder().decode(response.body));
      this.setState({ users });
    });
  }

  onDestroy(): void {
    this.request?.cancel?.();
  }

  onRender(): void {
    <scroll>
      {this.state.users.forEach(u => {
        <label key={u.id} value={u.name} />;
      })}
    </scroll>;
  }
}
