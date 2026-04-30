// valdi-async skill reference — compile check for async lifecycle safety patterns.
// This file must compile cleanly against the Valdi TypeScript compiler.
// Run: bzl build //ai-skills/skills/valdi-async/tests:tests

import { StatefulComponent } from 'valdi_core/src/Component';
import { CancelablePromise, promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { HTTPResponse } from 'valdi_http/src/HTTPTypes';

// ─── HTTPClient: store, cancel in onDestroy, cancel before restart ────────────

interface UserProfileViewModel {
  userId: string;
}

interface UserProfileState {
  user: { name: string } | null;
  loading: boolean;
}

class UserProfile extends StatefulComponent<UserProfileViewModel, UserProfileState> {
  state: UserProfileState = { user: null, loading: true };

  private client = new HTTPClient('https://api.example.com');
  private request?: CancelablePromise<HTTPResponse>;

  onCreate(): void {
    this.fetchUser(this.viewModel.userId);
  }

  onViewModelUpdate(previous?: UserProfileViewModel): void {
    if (this.viewModel.userId !== previous?.userId) {
      this.request?.cancel?.();           // Cancel in-flight before starting new one
      this.fetchUser(this.viewModel.userId);
    }
  }

  onDestroy(): void {
    this.request?.cancel?.();             // Always clean up
  }

  private fetchUser(id: string): void {
    this.request = this.client.get(`/users/${id}`);
    this.request.then(response => {
      const user = JSON.parse(new TextDecoder().decode(response.body)) as { name: string };
      this.setState({ user, loading: false });
    });
  }

  onRender(): void {
    if (this.state.loading) {
      <spinner />;
      return;
    }
    <label value={this.state.user?.name ?? ''} />;
  }
}

// ─── registerDisposable: timers and subscriptions ────────────────────────────

interface ClockViewModel {
  format: string;
}

interface ClockState {
  ticks: number;
}

class LiveClock extends StatefulComponent<ClockViewModel, ClockState> {
  state: ClockState = { ticks: 0 };

  onCreate(): void {
    // ✅ Automatically cancelled on destroy — no onDestroy() needed
    const id = setInterval(() => this.tick(), 1000);
    this.registerDisposable({ unsubscribe: () => clearInterval(id) });
  }

  private tick = (): void => {
    this.setState({ ticks: this.state.ticks + 1 });
  };

  onRender(): void {
    <label value={String(this.state.ticks)} />;
  }
}

// ─── isDestroyed() guard for plain Promises ──────────────────────────────────

declare function expensiveOperation(): Promise<string>;

interface DataState {
  data: string | null;
}

interface DataViewModel {
  query: string;
}

class DataLoader extends StatefulComponent<DataViewModel, DataState> {
  state: DataState = { data: null };

  onCreate(): void {
    this.loadData().catch(() => {});
  }

  private async loadData(): Promise<void> {
    const result = await expensiveOperation();

    if (this.isDestroyed()) return;   // Guard before setState

    this.setState({ data: result });
  }

  onRender(): void {
    <label value={this.state.data ?? 'Loading…'} />;
  }
}

// ─── setTimeoutInterruptible for debounce ────────────────────────────────────
// setTimeoutInterruptible returns number (timer ID) — cancel with clearTimeout

interface SearchViewModel {
  query: string;
}

interface SearchState {
  results: string[];
}

class SearchBar extends StatefulComponent<SearchViewModel, SearchState> {
  state: SearchState = { results: [] };

  private debounceId?: number;
  private searchRequest?: CancelablePromise<HTTPResponse>;
  private client = new HTTPClient('https://api.example.com');

  onViewModelUpdate(previous?: SearchViewModel): void {
    if (this.viewModel.query !== previous?.query) {
      clearTimeout(this.debounceId);    // Cancel pending debounce
      this.debounceId = setTimeoutInterruptible(() => {
        this.fetchResults(this.viewModel.query);
      }, 300);
    }
  }

  onDestroy(): void {
    this.searchRequest?.cancel?.();
  }

  private fetchResults(query: string): void {
    this.searchRequest?.cancel?.();
    this.searchRequest = this.client.get(`/search?q=${encodeURIComponent(query)}`);
    this.searchRequest.then(response => {
      const data = JSON.parse(new TextDecoder().decode(response.body)) as { results: string[] };
      this.setState({ results: data.results });
    });
  }

  onRender(): void {
    <scroll>
      {this.state.results.forEach(result => {
        <label key={result} value={result} />;
      })}
    </scroll>;
  }
}

// ─── promiseToCancelablePromise ──────────────────────────────────────────────

declare function thirdPartyFetch(url: string): Promise<string>;

interface FetchState {
  content: string | null;
}

interface FetchViewModel {
  url: string;
}

class ThirdPartyFetcher extends StatefulComponent<FetchViewModel, FetchState> {
  state: FetchState = { content: null };

  private request?: CancelablePromise<string>;

  onCreate(): void {
    const rawPromise = thirdPartyFetch(this.viewModel.url);
    this.request = promiseToCancelablePromise(rawPromise, () => {
      // optional cleanup on cancel
    });
    this.request.then(content => {
      if (this.isDestroyed()) return;
      this.setState({ content });
    });
  }

  onDestroy(): void {
    this.request?.cancel?.();
  }

  onRender(): void {
    <label value={this.state.content ?? ''} />;
  }
}

void UserProfile;
void LiveClock;
void DataLoader;
void SearchBar;
void ThirdPartyFetcher;
