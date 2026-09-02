import { StatefulComponent, Component } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { Label, ScrollView, TextField, TextView, View } from 'valdi_tsx/src/NativeTemplateElements';

import { LedgerDb } from './sqlgen/LedgerDb';
import { ClientSQLSubscription, LedgerQueries } from './sqlgen/LedgerQueries';
import {
  Account,
  CountLedgerEntriesRow,
  CountTransactionLogRow,
  SelectBalancesRow,
  SelectRecentEntriesRow,
  SelectTransactionLogRow,
} from './sqlgen/LedgerTypes';

interface ViewModel {}

const LEDGER_PAGE_SIZE = 12;
const LEDGER_SHOW_ALL_LIMIT = -1;

enum LedgerMutation {
  Idle,
  Initializing,
  Transfer,
  StressBatch,
  Reset,
}

interface State {
  accounts: Account[];
  balances: SelectBalancesRow[];
  recentEntries: SelectRecentEntriesRow[];
  transfers: SelectTransactionLogRow[];
  ledgerEntryCount: number;
  recentEntriesOffset: number;
  showAllLedgerEntries: boolean;
  transferCount: number;
  fromAccountId: number;
  toAccountId: number;
  amount: string;
  memo: string;
  status: string;
  activeMutation: LedgerMutation;
  transferSequence: number;
}

interface AccountBalanceRowViewModel {
  balance: SelectBalancesRow;
  isFrom: boolean;
  isTo: boolean;
  onSelectFrom: (accountId: number) => void;
  onSelectTo: (accountId: number) => void;
}

class AccountBalanceRow extends Component<AccountBalanceRowViewModel> {
  onRender(): void {
    const balance = this.viewModel.balance;
    const balanceCents = balance.balance_cents ?? 0;
    const compact = isCompactLayout();
    <view style={compact ? styles.balanceRowCompact : styles.balanceRow}>
      <view style={compact ? styles.balanceIdentityCompact : styles.balanceIdentity}>
        <view style={styles.accountCodeBadge}>
          <label style={styles.accountCode} value={balance.code} />
        </view>
        <view style={styles.balanceNameBlock}>
          <label style={styles.accountName} value={balance.name} />
          <label
            style={styles.accountMeta}
            value={`${balance.entry_count} ledger ${balance.entry_count === 1 ? 'entry' : 'entries'}`}
          />
        </view>
      </view>
      <label
        style={
          balanceCents < 0
            ? compact
              ? styles.negativeAmountCompact
              : styles.negativeAmount
            : compact
              ? styles.positiveAmountCompact
              : styles.positiveAmount
        }
        value={formatCents(balanceCents)}
      />
      <view style={compact ? styles.selectionActionsCompact : styles.selectionActions}>
        <view
          style={
            this.viewModel.isFrom
              ? compact
                ? styles.selectionButtonSelectedCompact
                : styles.selectionButtonSelected
              : compact
                ? styles.selectionButtonCompact
                : styles.selectionButton
          }
          onTap={this.selectFrom}
        >
          <label
            style={this.viewModel.isFrom ? styles.selectionButtonTextSelected : styles.selectionButtonText}
            value="From"
          />
        </view>
        <view
          style={
            this.viewModel.isTo
              ? compact
                ? styles.selectionButtonSelectedCompact
                : styles.selectionButtonSelected
              : compact
                ? styles.selectionButtonCompact
                : styles.selectionButton
          }
          onTap={this.selectTo}
        >
          <label
            style={this.viewModel.isTo ? styles.selectionButtonTextSelected : styles.selectionButtonText}
            value="To"
          />
        </view>
      </view>
    </view>;
  }

  private readonly selectFrom = (): void => {
    this.viewModel.onSelectFrom(this.viewModel.balance.id);
  };

  private readonly selectTo = (): void => {
    this.viewModel.onSelectTo(this.viewModel.balance.id);
  };
}

interface TransferRowViewModel {
  transfer: SelectTransactionLogRow;
}

class TransferRow extends Component<TransferRowViewModel> {
  onRender(): void {
    const transfer = this.viewModel.transfer;
    <view style={styles.transferRow}>
      <view style={styles.transferHeader}>
        <label
          style={styles.transferRoute}
          value={`${transfer.from_account_name} -> ${transfer.to_account_name}`}
        />
        <view style={styles.transferAmountPill}>
          <label style={styles.transferAmount} value={formatCents(transfer.amount_cents)} />
        </view>
      </view>
      <label style={styles.transferMemo} value={transfer.memo} />
      <label
        style={styles.transferMeta}
        value={`${formatTime(transfer.created_at)} | ${shortGroup(transfer.transfer_group)}`}
      />
    </view>;
  }
}

interface LedgerEntryRowViewModel {
  entry: SelectRecentEntriesRow;
}

class LedgerEntryRow extends Component<LedgerEntryRowViewModel> {
  onRender(): void {
    const entry = this.viewModel.entry;
    const compact = isCompactLayout();
    <view style={compact ? styles.entryRowCompact : styles.entryRow}>
      <label
        style={compact ? styles.entryAccountCompact : styles.entryAccount}
        value={entry.account_name}
      />
      <label
        style={
          entry.amount_cents < 0
            ? compact
              ? styles.entryAmountNegativeCompact
              : styles.entryAmountNegative
            : compact
              ? styles.entryAmountPositiveCompact
              : styles.entryAmountPositive
        }
        value={formatCents(entry.amount_cents)}
      />
      <label
        style={compact ? styles.entryMemoCompact : styles.entryMemo}
        value={entry.memo}
      />
      <label
        style={compact ? styles.entryMetaCompact : styles.entryMeta}
        value={`#${entry.id} | ${formatTime(entry.created_at)} | ${shortGroup(entry.transfer_group)}`}
      />
    </view>;
  }
}

interface LedgerPaginationControlsViewModel {
  total: number;
  offset: number;
  pageSize: number;
  shown: number;
  showAll: boolean;
  onPrevious: () => void;
  onNext: () => void;
  onShowAllToggle: () => void;
}

class LedgerPaginationControls extends Component<LedgerPaginationControlsViewModel> {
  onRender(): void {
    const total = this.viewModel.total;
    const compact = isCompactLayout();
    const rangeStart = total === 0 ? 0 : this.viewModel.offset + 1;
    const rangeEnd = this.viewModel.showAll ? total : Math.min(total, this.viewModel.offset + this.viewModel.shown);
    const summary = this.viewModel.showAll
      ? `Showing all ${total} ledger ${total === 1 ? 'entry' : 'entries'}`
      : `Showing ${rangeStart}-${rangeEnd} of ${total}`;

    <view style={compact ? styles.paginationBarCompact : styles.paginationBar}>
      <label style={styles.paginationLabel} value={summary} />
      <view style={styles.paginationActions}>
        <view
          style={this.canPrevious() ? styles.paginationButton : styles.paginationButtonDisabled}
          onTap={this.previous}
        >
          <label
            style={this.canPrevious() ? styles.paginationButtonText : styles.paginationButtonTextDisabled}
            value="<"
          />
        </view>
        <view style={this.showAllButtonStyle()} onTap={this.toggleShowAll}>
          <label
            style={this.showAllTextStyle()}
            value={this.viewModel.showAll ? 'Page' : 'All'}
          />
        </view>
        <view style={this.canNext() ? styles.paginationButton : styles.paginationButtonDisabled} onTap={this.next}>
          <label
            style={this.canNext() ? styles.paginationButtonText : styles.paginationButtonTextDisabled}
            value=">"
          />
        </view>
      </view>
    </view>;
  }

  private canPrevious(): boolean {
    return !this.viewModel.showAll && this.viewModel.offset > 0;
  }

  private canNext(): boolean {
    return !this.viewModel.showAll && this.viewModel.offset + this.viewModel.shown < this.viewModel.total;
  }

  private showAllButtonStyle(): Style<View> {
    if (this.viewModel.showAll) {
      return styles.paginationButtonSelected;
    }
    return this.viewModel.total > this.viewModel.pageSize ? styles.paginationButton : styles.paginationButtonDisabled;
  }

  private showAllTextStyle(): Style<Label> {
    if (this.viewModel.showAll) {
      return styles.paginationToggleButtonTextSelected;
    }
    return this.viewModel.total > this.viewModel.pageSize
      ? styles.paginationToggleButtonText
      : styles.paginationToggleButtonTextDisabled;
  }

  private readonly previous = (): void => {
    if (this.canPrevious()) {
      this.viewModel.onPrevious();
    }
  };

  private readonly next = (): void => {
    if (this.canNext()) {
      this.viewModel.onNext();
    }
  };

  private readonly toggleShowAll = (): void => {
    if (this.viewModel.showAll || this.viewModel.total > this.viewModel.pageSize) {
      this.viewModel.onShowAllToggle();
    }
  };
}

class EmptyState extends Component {
  onRender(): void {
    <view style={styles.emptyState}>
      <label style={styles.emptyTitle} value="No ledger rows yet" />
      <label
        style={styles.emptyBody}
        value="Seed data and transfers will appear after the generated ClientSQL watchers receive their first result."
      />
    </view>;
  }
}

export class App extends StatefulComponent<ViewModel, State> {
  state: State = {
    accounts: [],
    balances: [],
    recentEntries: [],
    transfers: [],
    ledgerEntryCount: 0,
    recentEntriesOffset: 0,
    showAllLedgerEntries: false,
    transferCount: 0,
    fromAccountId: 0,
    toAccountId: 0,
    amount: '25.00',
    memo: 'Counterparty settlement',
    status: 'Opening LedgerDb...',
    activeMutation: LedgerMutation.Initializing,
    transferSequence: 0,
  };

  private db?: LedgerDb;
  private subscriptions: ClientSQLSubscription[] = [];
  private recentEntriesSubscription?: ClientSQLSubscription;
  private recentEntriesQueryLimit = 0;
  private recentEntriesQueryOffset = -1;
  private activeOperation?: Promise<void>;

  onCreate(): void {
    const db = LedgerDb.open('ledger_sql_demo');
    this.db = db;
    this.subscriptions = [
      db.ledgerQueries.watchSelectAccounts(accounts => {
        this.setState({
          accounts,
          ...selectionForAccounts(this.state.fromAccountId, this.state.toAccountId, accounts),
        });
      }),
      db.ledgerQueries.watchSelectBalances(balances => {
        this.setState({
          balances,
          status:
            this.state.status === 'Opening LedgerDb...' ? 'Loaded reactive balances from LedgerDb.' : this.state.status,
        });
      }),
      db.ledgerQueries.watchSelectTransactionLog(8, transfers => {
        this.setState({ transfers });
      }),
      db.ledgerQueries.watchCountLedgerEntries((rows: CountLedgerEntriesRow[]) => {
        const ledgerEntryCount = Number(rows[0]?.count ?? 0);
        const recentEntriesOffset = clampLedgerEntriesOffset(
          this.state.recentEntriesOffset,
          ledgerEntryCount,
          this.state.showAllLedgerEntries,
        );
        this.setState({ ledgerEntryCount, recentEntriesOffset });
        this.syncRecentEntriesSubscription({ ledgerEntryCount, recentEntriesOffset });
      }),
      db.ledgerQueries.watchCountTransactionLog((rows: CountTransactionLogRow[]) => {
        this.setState({ transferCount: Number(rows[0]?.count ?? 0) });
      }),
    ];
    this.syncRecentEntriesSubscription(undefined);
    this.trackOperation(this.seedLedgerIfNeeded());
  }

  onDestroy(): void {
    this.recentEntriesSubscription?.unsubscribe();
    this.recentEntriesSubscription = undefined;
    this.subscriptions.forEach(subscription => {
      subscription.unsubscribe();
    });
    this.subscriptions = [];

    const db = this.db;
    this.db = undefined;
    if (db) {
      const activeOperation = this.activeOperation;
      if (activeOperation) {
        void activeOperation.then(
          () => this.closeDatabase(db),
          error => {
            console.warn('Ledger operation failed while closing the demo.', error);
            this.closeDatabase(db);
          },
        );
      } else {
        this.closeDatabase(db);
      }
    }
  }

  onRender(): void {
    const compact = isCompactLayout();
    const mutationBusy = this.isMutationInProgress();
    const contentPaddingTop = compact && Device.isIOS() ? Device.getDisplayTopInset() + 18 : 24;
    const contentWidth = compact ? Device.getWindowWidth() - 32 : '100%';
    <view style={styles.root}>
      <scroll style={styles.scroll}>
        <view
          style={compact ? styles.contentCompact : styles.content}
          width={contentWidth}
          paddingTop={contentPaddingTop}
        >
          <view style={styles.header}>
            <label
              style={styles.kicker}
              value="CLIENTSQL TRANSACTIONS + WATCHERS"
              font={systemBoldFont(compact ? 11 : 13)}
            />
            <label
              style={styles.title}
              value="Ledger ClientSQL Demo"
              font={systemBoldFont(compact ? 25 : 32)}
            />
            <label
              style={styles.subtitle}
              value="Transfers write two ledger entries and one log row in a single transaction. Balance and log sections are generated reactive aggregate queries."
              font={systemFont(compact ? 14 : 15)}
            />
          </view>

          <view style={compact ? styles.summaryStripCompact : styles.summaryStrip}>
            <SummaryMetric title="Accounts" value={`${this.state.accounts.length}`} compact={compact} />
            <SummaryMetric title="Ledger Entries" value={`${this.state.ledgerEntryCount}`} compact={compact} />
            <SummaryMetric title="Logged Transfers" value={`${this.state.transferCount}`} compact={compact} />
          </view>

          <view style={compact ? styles.panelCompact : styles.panel}>
            <label style={styles.sectionTitle} value="Post transfer" />
            <view style={compact ? styles.formGridCompact : styles.formGrid}>
              <textfield
                style={compact ? styles.amountInputCompact : styles.amountInput}
                value={this.state.amount}
                placeholder="Amount"
                onChange={this.onAmountChange}
                returnKeyText="next"
              />
              <textview
                style={compact ? styles.memoInputCompact : styles.memoInput}
                value={this.state.memo}
                placeholder="Memo"
                onChange={this.onMemoChange}
              />
            </view>
            <view style={compact ? styles.actionsCompact : styles.actions}>
              <view
                style={compact ? styles.primaryButtonCompact : styles.primaryButton}
                opacity={mutationBusy ? 0.5 : 1}
                onTap={mutationBusy ? undefined : this.onTransfer}
              >
                <label
                  style={styles.primaryButtonText}
                  value={
                    this.state.activeMutation === LedgerMutation.Transfer
                      ? 'Posting...'
                      : 'Post transfer transaction'
                  }
                />
              </view>
              <view
                style={compact ? styles.secondaryButtonCompact : styles.secondaryButton}
                opacity={mutationBusy ? 0.5 : 1}
                onTap={mutationBusy ? undefined : this.onRunStressBatch}
              >
                <label
                  style={styles.secondaryButtonText}
                  value={
                    this.state.activeMutation === LedgerMutation.StressBatch
                      ? 'Running batch...'
                      : 'Run stress batch'
                  }
                />
              </view>
              <view
                style={compact ? styles.secondaryButtonCompact : styles.secondaryButton}
                opacity={mutationBusy ? 0.5 : 1}
                onTap={mutationBusy ? undefined : this.onResetLedger}
              >
                <label
                  style={styles.secondaryButtonText}
                  value={this.state.activeMutation === LedgerMutation.Reset ? 'Resetting...' : 'Reset seed data'}
                />
              </view>
            </view>
            <label style={styles.status} value={this.state.status} />
          </view>

          <view style={compact ? styles.twoColumnCompact : styles.twoColumn}>
            <view style={compact ? styles.leftColumnCompact : styles.leftColumn}>
              <view style={compact ? styles.panelCompact : styles.panel}>
                <label style={styles.sectionTitle} value="Reactive account balances" />
                {this.renderBalances()}
              </view>
            </view>
            <view style={compact ? styles.rightColumnCompact : styles.rightColumn}>
              <view style={compact ? styles.panelCompact : styles.panel}>
                <label
                  style={styles.sectionTitle}
                  value={`Transaction log (latest ${this.state.transfers.length} of ${this.state.transferCount})`}
                />
                {this.renderTransfers()}
              </view>
            </view>
          </view>

          <view style={compact ? styles.panelCompact : styles.panel}>
            <label style={styles.sectionTitle} value={this.ledgerEntriesTitle()} />
            {this.renderLedgerPagination()}
            {this.renderRecentEntries()}
          </view>
        </view>
      </scroll>
    </view>;
  }

  private renderBalances(): void {
    if (this.state.balances.length === 0) {
      <EmptyState />;
      return;
    }

    this.state.balances.forEach(balance => {
      <AccountBalanceRow
        key={`${balance.id}`}
        balance={balance}
        isFrom={balance.id === this.state.fromAccountId}
        isTo={balance.id === this.state.toAccountId}
        onSelectFrom={this.onSelectFrom}
        onSelectTo={this.onSelectTo}
      />;
    });
  }

  private renderTransfers(): void {
    if (this.state.transfers.length === 0) {
      <EmptyState />;
      return;
    }

    this.state.transfers.forEach(transfer => {
      <TransferRow key={`${transfer.id}`} transfer={transfer} />;
    });
  }

  private renderRecentEntries(): void {
    if (this.state.recentEntries.length === 0) {
      <EmptyState />;
      return;
    }

    <view style={styles.entriesList}>
      {this.state.recentEntries.forEach(entry => {
        <LedgerEntryRow key={`${entry.id}`} entry={entry} />;
      })}
    </view>;
  }

  private renderLedgerPagination(): void {
    if (this.state.ledgerEntryCount === 0) {
      return;
    }

    <LedgerPaginationControls
      total={this.state.ledgerEntryCount}
      offset={this.state.recentEntriesOffset}
      pageSize={LEDGER_PAGE_SIZE}
      shown={this.state.recentEntries.length}
      showAll={this.state.showAllLedgerEntries}
      onPrevious={this.onPreviousLedgerPage}
      onNext={this.onNextLedgerPage}
      onShowAllToggle={this.onToggleShowAllLedgerEntries}
    />;
  }

  private ledgerEntriesTitle(): string {
    if (this.state.showAllLedgerEntries) {
      return `Ledger entries (all ${this.state.ledgerEntryCount})`;
    }
    const pageCount = Math.max(1, Math.ceil(this.state.ledgerEntryCount / LEDGER_PAGE_SIZE));
    const currentPage = Math.floor(this.state.recentEntriesOffset / LEDGER_PAGE_SIZE) + 1;
    return `Ledger entries (page ${currentPage} of ${pageCount})`;
  }

  private readonly onAmountChange: NonNullable<TextField['onChange']> = event => {
    this.setState({ amount: event.text });
  };

  private readonly onMemoChange: NonNullable<TextView['onChange']> = event => {
    this.setState({ memo: event.text });
  };

  private readonly onSelectFrom = (accountId: number): void => {
    if (accountId === this.state.toAccountId) {
      this.setState({ status: 'Pick a different destination account before posting.' });
      return;
    }
    this.setState({ fromAccountId: accountId });
  };

  private readonly onSelectTo = (accountId: number): void => {
    if (accountId === this.state.fromAccountId) {
      this.setState({ status: 'Pick a different source account before posting.' });
      return;
    }
    this.setState({ toAccountId: accountId });
  };

  private readonly onTransfer = (): void => {
    if (this.isMutationInProgress()) {
      return;
    }
    this.trackOperation(this.postTransfer());
  };

  private readonly onRunStressBatch = (): void => {
    if (this.isMutationInProgress()) {
      return;
    }
    this.trackOperation(this.runStressBatch());
  };

  private readonly onResetLedger = (): void => {
    if (this.isMutationInProgress()) {
      return;
    }
    this.trackOperation(this.resetLedger());
  };

  private trackOperation(operation: Promise<void>): void {
    this.activeOperation = operation;
    void operation.then(
      () => this.clearActiveOperation(operation),
      error => {
        console.warn('Unexpected Ledger demo operation failure.', error);
        this.clearActiveOperation(operation);
      },
    );
  }

  private clearActiveOperation(operation: Promise<void>): void {
    if (this.activeOperation === operation) {
      this.activeOperation = undefined;
    }
  }

  private closeDatabase(db: LedgerDb): void {
    void db.close().then(undefined, error => {
      console.warn('Failed to close LedgerDb after the demo was destroyed.', error);
    });
  }

  private isMutationInProgress(): boolean {
    return this.state.activeMutation !== LedgerMutation.Idle;
  }

  private readonly onPreviousLedgerPage = (): void => {
    this.setLedgerEntriesPagination(false, this.state.recentEntriesOffset - LEDGER_PAGE_SIZE);
  };

  private readonly onNextLedgerPage = (): void => {
    this.setLedgerEntriesPagination(false, this.state.recentEntriesOffset + LEDGER_PAGE_SIZE);
  };

  private readonly onToggleShowAllLedgerEntries = (): void => {
    this.setLedgerEntriesPagination(!this.state.showAllLedgerEntries, 0);
  };

  private setLedgerEntriesPagination(showAllLedgerEntries: boolean, requestedOffset: number): void {
    const recentEntriesOffset = clampLedgerEntriesOffset(
      requestedOffset,
      this.state.ledgerEntryCount,
      showAllLedgerEntries,
    );
    this.setState({ showAllLedgerEntries, recentEntriesOffset });
    this.syncRecentEntriesSubscription({ showAllLedgerEntries, recentEntriesOffset });
  }

  private syncRecentEntriesSubscription(
    overrides: Partial<Pick<State, 'ledgerEntryCount' | 'recentEntriesOffset' | 'showAllLedgerEntries'>> | undefined,
  ): void {
    if (!this.db) {
      return;
    }

    const ledgerEntryCount = overrides?.ledgerEntryCount ?? this.state.ledgerEntryCount;
    const showAllLedgerEntries = overrides?.showAllLedgerEntries ?? this.state.showAllLedgerEntries;
    const recentEntriesOffset = clampLedgerEntriesOffset(
      overrides?.recentEntriesOffset ?? this.state.recentEntriesOffset,
      ledgerEntryCount,
      showAllLedgerEntries,
    );
    const limit = showAllLedgerEntries ? LEDGER_SHOW_ALL_LIMIT : LEDGER_PAGE_SIZE;
    if (
      this.recentEntriesSubscription &&
      this.recentEntriesQueryLimit === limit &&
      this.recentEntriesQueryOffset === recentEntriesOffset
    ) {
      return;
    }

    this.recentEntriesSubscription?.unsubscribe();
    this.recentEntriesQueryLimit = limit;
    this.recentEntriesQueryOffset = recentEntriesOffset;
    this.recentEntriesSubscription = this.db.ledgerQueries.watchSelectRecentEntries(
      limit,
      recentEntriesOffset,
      recentEntries => {
        this.setState({ recentEntries });
      },
    );
  }

  private async seedLedgerIfNeeded(): Promise<void> {
    try {
      const countRows = await this.db!.ledgerQueries.countAccounts();
      if (countRows[0] && countRows[0].count > 0) {
        this.setState({
          activeMutation: LedgerMutation.Idle,
          status: 'Ledger actions ready. Loaded reactive balances from LedgerDb.',
        });
        return;
      }

      await this.seedLedger();
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: 'Ledger actions ready. Seeded accounts and opening transfers in one ClientSQL transaction.',
      });
    } catch (error) {
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: `Initial seed failed: ${errorMessage(error)}`,
      });
    }
  }

  private async seedLedger(): Promise<void> {
    await this.db!.transaction(async transaction => {
      await this.seedLedgerQueries(transaction.ledgerQueries);
    });
  }

  private async seedLedgerQueries(queries: LedgerQueries): Promise<void> {
    await queries.insertAccount('Operating Cash', 'CASH', 'asset');
    await queries.insertAccount('Reserve Savings', 'SAVE', 'asset');
    await queries.insertAccount('Accounts Receivable', 'AR', 'asset');
    await queries.insertAccount('Opening Equity', 'EQTY', 'equity');

    const cash = await this.requireAccountByCode(queries, 'CASH');
    const savings = await this.requireAccountByCode(queries, 'SAVE');
    const receivable = await this.requireAccountByCode(queries, 'AR');
    const equity = await this.requireAccountByCode(queries, 'EQTY');
    const createdAt = Date.now();
    await this.insertTransfer(queries, equity.id, cash.id, 750000, 'Opening cash funding', 'seed-cash', createdAt);
    await this.insertTransfer(
      queries,
      equity.id,
      savings.id,
      250000,
      'Opening reserve funding',
      'seed-reserve',
      createdAt + 1,
    );
    await this.insertTransfer(
      queries,
      receivable.id,
      cash.id,
      12500,
      'Collected invoice A-1007',
      'seed-invoice',
      createdAt + 2,
    );
  }

  private async postTransfer(): Promise<void> {
    const amountCents = parseCents(this.state.amount);
    const memo = this.state.memo.trim();

    if (amountCents === null || amountCents <= 0) {
      this.setState({ status: 'Enter a positive amount, for example 25.00.' });
      return;
    }
    if (memo.length === 0) {
      this.setState({ status: 'Memo is required so the transaction log is readable.' });
      return;
    }
    if (this.state.fromAccountId === 0 || this.state.toAccountId === 0) {
      this.setState({ status: 'Select source and destination accounts.' });
      return;
    }
    if (this.state.fromAccountId === this.state.toAccountId) {
      this.setState({ status: 'Source and destination must be different accounts.' });
      return;
    }

    const fromAccountId = this.state.fromAccountId;
    const toAccountId = this.state.toAccountId;
    const nextSequence = this.state.transferSequence + 1;
    const transferGroup = `manual-${Date.now()}-${nextSequence}`;
    const createdAt = Date.now();
    this.setState({
      activeMutation: LedgerMutation.Transfer,
      status: 'Posting transfer inside LedgerDb.transaction()...',
    });
    try {
      await this.db!.transaction(async transaction => {
        await this.insertTransfer(
          transaction.ledgerQueries,
          fromAccountId,
          toAccountId,
          amountCents,
          memo,
          transferGroup,
          createdAt,
        );
      });
      this.setState({
        activeMutation: LedgerMutation.Idle,
        amount: '25.00',
        memo: 'Counterparty settlement',
        status: `Committed ${formatCents(amountCents)} transfer. Reactive aggregate queries refreshed after commit.`,
        transferSequence: nextSequence,
      });
    } catch (error) {
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: `Transfer failed: ${errorMessage(error)}`,
      });
    }
  }

  private async runStressBatch(): Promise<void> {
    const startSequence = this.state.transferSequence + 1;
    const createdAt = Date.now();
    const batchId = `stress-${createdAt}-${startSequence}`;
    this.setState({
      activeMutation: LedgerMutation.StressBatch,
      status: 'Running batched transfer transaction...',
    });

    try {
      const cash = await this.requireAccountByCode(this.db!.ledgerQueries, 'CASH');
      const savings = await this.requireAccountByCode(this.db!.ledgerQueries, 'SAVE');
      const receivable = await this.requireAccountByCode(this.db!.ledgerQueries, 'AR');
      const equity = await this.requireAccountByCode(this.db!.ledgerQueries, 'EQTY');
      await this.db!.transaction(async transaction => {
        await this.insertTransfer(
          transaction.ledgerQueries,
          cash.id,
          savings.id,
          1100,
          'Sweep operating cash to reserve',
          `${batchId}-1`,
          createdAt,
        );
        await this.insertTransfer(
          transaction.ledgerQueries,
          savings.id,
          cash.id,
          475,
          'Release reserve for payroll',
          `${batchId}-2`,
          createdAt + 1,
        );
        await this.insertTransfer(
          transaction.ledgerQueries,
          receivable.id,
          cash.id,
          820,
          'Collect small invoice batch',
          `${batchId}-3`,
          createdAt + 2,
        );
        await this.insertTransfer(
          transaction.ledgerQueries,
          equity.id,
          receivable.id,
          1395,
          'Recognize new receivable batch',
          `${batchId}-4`,
          createdAt + 3,
        );
      });
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: 'Committed four transfers in one transaction; watchers should observe one batched refresh.',
        transferSequence: startSequence + 4,
      });
    } catch (error) {
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: `Stress batch failed: ${errorMessage(error)}`,
      });
    }
  }

  private async resetLedger(): Promise<void> {
    this.setState({ activeMutation: LedgerMutation.Reset, status: 'Resetting ledger tables...' });
    try {
      await this.db!.transaction(async transaction => {
        await transaction.ledgerQueries.deleteLedgerEntries();
        await transaction.ledgerQueries.deleteTransactionLog();
        await transaction.ledgerQueries.deleteAccounts();
        await this.seedLedgerQueries(transaction.ledgerQueries);
      });
      this.setState({
        activeMutation: LedgerMutation.Idle,
        amount: '25.00',
        memo: 'Counterparty settlement',
        recentEntriesOffset: 0,
        showAllLedgerEntries: false,
        status: 'Reset and reseeded ledger tables.',
        transferSequence: 0,
      });
      this.syncRecentEntriesSubscription({ recentEntriesOffset: 0, showAllLedgerEntries: false });
    } catch (error) {
      this.setState({
        activeMutation: LedgerMutation.Idle,
        status: `Reset failed: ${errorMessage(error)}`,
      });
    }
  }

  private async insertTransfer(
    queries: LedgerQueries,
    fromAccountId: number,
    toAccountId: number,
    amountCents: number,
    memo: string,
    transferGroup: string,
    createdAt: number,
  ): Promise<void> {
    await queries.insertLedgerEntry(fromAccountId, -amountCents, memo, transferGroup, createdAt);
    await queries.insertLedgerEntry(toAccountId, amountCents, memo, transferGroup, createdAt);
    await queries.insertTransactionLog(
      transferGroup,
      fromAccountId,
      toAccountId,
      amountCents,
      memo,
      createdAt,
    );
  }

  private async requireAccountByCode(queries: LedgerQueries, code: string): Promise<Account> {
    const accounts = await queries.selectAccountByCode(code);
    if (accounts.length === 0) {
      throw new Error(`Missing seed account ${code}`);
    }
    return accounts[0];
  }
}

interface SummaryMetricViewModel {
  title: string;
  value: string;
  compact?: boolean;
}

class SummaryMetric extends Component<SummaryMetricViewModel> {
  onRender(): void {
    <view style={this.viewModel.compact ? styles.summaryMetricCompact : styles.summaryMetric}>
      <label
        style={styles.summaryValue}
        value={this.viewModel.value}
        font={systemBoldFont(this.viewModel.compact ? 20 : 24)}
      />
      <label style={styles.summaryTitle} value={this.viewModel.title} />
    </view>;
  }
}

function isCompactLayout(): boolean {
  return !Device.isDesktop() && Device.getWindowWidth() < 700;
}

function selectionForAccounts(
  fromAccountId: number,
  toAccountId: number,
  accounts: Account[],
): Pick<State, 'fromAccountId' | 'toAccountId'> {
  if (accounts.length === 0) {
    return { fromAccountId: 0, toAccountId: 0 };
  }

  const hasFrom = accounts.some(account => account.id === fromAccountId);
  const nextFrom = hasFrom ? fromAccountId : accounts[0].id;
  const hasTo = accounts.some(account => account.id === toAccountId && account.id !== nextFrom);
  const fallbackTo = accounts.find(account => account.id !== nextFrom);
  return {
    fromAccountId: nextFrom,
    toAccountId: hasTo ? toAccountId : fallbackTo ? fallbackTo.id : 0,
  };
}

function clampLedgerEntriesOffset(offset: number, total: number, showAll: boolean): number {
  if (showAll || total <= LEDGER_PAGE_SIZE) {
    return 0;
  }

  const lastPageOffset = Math.floor((total - 1) / LEDGER_PAGE_SIZE) * LEDGER_PAGE_SIZE;
  return Math.max(0, Math.min(offset, lastPageOffset));
}

function parseCents(value: string): number | null {
  const trimmed = value.trim();
  if (!/^\d+(\.\d{1,2})?$/.test(trimmed)) {
    return null;
  }

  const parts = trimmed.split('.');
  const dollars = Number(parts[0]);
  const cents = Number((parts[1] ?? '').padEnd(2, '0'));
  if (!Number.isFinite(dollars) || !Number.isFinite(cents)) {
    return null;
  }
  return dollars * 100 + cents;
}

function formatCents(cents: number): string {
  const sign = cents < 0 ? '-' : '';
  const absolute = Math.abs(cents);
  const dollars = Math.floor(absolute / 100);
  const remainder = `${absolute % 100}`.padStart(2, '0');
  return `${sign}$${dollars}.${remainder}`;
}

function formatTime(milliseconds: number): string {
  const date = new Date(milliseconds);
  const hours = `${date.getHours()}`.padStart(2, '0');
  const minutes = `${date.getMinutes()}`.padStart(2, '0');
  const seconds = `${date.getSeconds()}`.padStart(2, '0');
  return `${hours}:${minutes}:${seconds}`;
}

function shortGroup(group: string): string {
  return group.length > 18 ? `${group.slice(0, 18)}...` : group;
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

const styles = {
  root: new Style<View>({
    backgroundColor: '#F8FAFC',
    width: '100%',
    height: '100%',
  }),

  scroll: new Style<ScrollView>({
    width: '100%',
    height: '100%',
  }),

  content: new Style<View>({
    alignItems: 'stretch',
    padding: 24,
    width: '100%',
  }),

  contentCompact: new Style<View>({
    alignItems: 'stretch',
    paddingLeft: 16,
    paddingRight: 16,
    paddingBottom: 16,
  }),

  header: new Style<View>({
    marginBottom: 20,
  }),

  kicker: new Style<Label>({
    color: '#0F766E',
    marginBottom: 8,
    width: '100%',
  }),

  title: new Style<Label>({
    color: '#111827',
    marginBottom: 8,
    numberOfLines: 0,
    width: '100%',
  }),

  subtitle: new Style<Label>({
    color: '#4B5563',
    numberOfLines: 0,
    width: '100%',
  }),

  summaryStrip: new Style<View>({
    flexDirection: 'row',
    marginBottom: 16,
    width: '100%',
  }),

  summaryStripCompact: new Style<View>({
    flexDirection: 'row',
    marginBottom: 14,
    width: '100%',
  }),

  summaryMetric: new Style<View>({
    backgroundColor: '#ECFDF5',
    borderColor: '#A7F3D0',
    borderRadius: 8,
    borderWidth: 1,
    flexGrow: 1,
    marginRight: 10,
    padding: 14,
  }),

  summaryMetricCompact: new Style<View>({
    backgroundColor: '#ECFDF5',
    borderColor: '#A7F3D0',
    borderRadius: 8,
    borderWidth: 1,
    flexGrow: 1,
    marginRight: 8,
    padding: 10,
  }),

  summaryValue: new Style<Label>({
    color: '#065F46',
    marginBottom: 2,
    width: '100%',
  }),

  summaryTitle: new Style<Label>({
    color: '#047857',
    font: systemFont(12),
    width: '100%',
  }),

  panel: new Style<View>({
    backgroundColor: 'white',
    borderColor: '#E5E7EB',
    borderRadius: 8,
    borderWidth: 1,
    marginBottom: 16,
    padding: 16,
    width: '100%',
  }),

  panelCompact: new Style<View>({
    backgroundColor: 'white',
    borderColor: '#E5E7EB',
    borderRadius: 8,
    borderWidth: 1,
    marginBottom: 14,
    padding: 14,
    width: '100%',
  }),

  sectionTitle: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(18),
    marginBottom: 12,
    width: '100%',
  }),

  formGrid: new Style<View>({
    flexDirection: 'row',
    marginBottom: 12,
    width: '100%',
  }),

  formGridCompact: new Style<View>({
    flexDirection: 'column',
    marginBottom: 12,
    width: '100%',
  }),

  amountInput: new Style<TextField>({
    backgroundColor: '#F9FAFB',
    borderColor: '#D1D5DB',
    borderRadius: 6,
    borderWidth: 1,
    color: '#111827',
    font: systemFont(15),
    height: 42,
    marginRight: 12,
    placeholderColor: '#9CA3AF',
    width: 150,
  }),

  amountInputCompact: new Style<TextField>({
    backgroundColor: '#F9FAFB',
    borderColor: '#D1D5DB',
    borderRadius: 6,
    borderWidth: 1,
    color: '#111827',
    font: systemFont(15),
    height: 42,
    marginBottom: 10,
    placeholderColor: '#9CA3AF',
    width: '100%',
  }),

  memoInput: new Style<TextView>({
    backgroundColor: '#F9FAFB',
    borderColor: '#D1D5DB',
    borderRadius: 6,
    borderWidth: 1,
    color: '#111827',
    flexGrow: 1,
    font: systemFont(15),
    height: 64,
    placeholderColor: '#9CA3AF',
  }),

  memoInputCompact: new Style<TextView>({
    backgroundColor: '#F9FAFB',
    borderColor: '#D1D5DB',
    borderRadius: 6,
    borderWidth: 1,
    color: '#111827',
    font: systemFont(15),
    height: 58,
    placeholderColor: '#9CA3AF',
    width: '100%',
  }),

  actions: new Style<View>({
    flexDirection: 'row',
    marginBottom: 12,
  }),

  actionsCompact: new Style<View>({
    flexDirection: 'column',
    marginBottom: 12,
    width: '100%',
  }),

  primaryButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#0F766E',
    borderRadius: 6,
    justifyContent: 'center',
    marginRight: 10,
    minHeight: 40,
    paddingLeft: 16,
    paddingRight: 16,
  }),

  primaryButtonCompact: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#0F766E',
    borderRadius: 6,
    justifyContent: 'center',
    marginBottom: 8,
    minHeight: 42,
    paddingLeft: 16,
    paddingRight: 16,
    width: '100%',
  }),

  primaryButtonText: new Style<Label>({
    color: 'white',
    font: systemBoldFont(14),
  }),

  secondaryButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#E5E7EB',
    borderRadius: 6,
    justifyContent: 'center',
    marginRight: 10,
    minHeight: 40,
    paddingLeft: 16,
    paddingRight: 16,
  }),

  secondaryButtonCompact: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#E5E7EB',
    borderRadius: 6,
    justifyContent: 'center',
    marginBottom: 8,
    minHeight: 42,
    paddingLeft: 16,
    paddingRight: 16,
    width: '100%',
  }),

  secondaryButtonText: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(14),
  }),

  status: new Style<Label>({
    color: '#4B5563',
    font: systemFont(13),
    numberOfLines: 0,
    width: '100%',
  }),

  twoColumn: new Style<View>({
    flexDirection: 'row',
    width: '100%',
  }),

  twoColumnCompact: new Style<View>({
    flexDirection: 'column',
    width: '100%',
  }),

  leftColumn: new Style<View>({
    flexGrow: 1,
    marginRight: 16,
  }),

  leftColumnCompact: new Style<View>({
    width: '100%',
  }),

  rightColumn: new Style<View>({
    width: 330,
  }),

  rightColumnCompact: new Style<View>({
    width: '100%',
  }),

  balanceRow: new Style<View>({
    alignItems: 'center',
    borderColor: '#E5E7EB',
    borderRadius: 8,
    borderWidth: 1,
    flexDirection: 'row',
    marginBottom: 10,
    padding: 12,
    width: '100%',
  }),

  balanceRowCompact: new Style<View>({
    alignItems: 'stretch',
    borderColor: '#E5E7EB',
    borderRadius: 8,
    borderWidth: 1,
    flexDirection: 'column',
    marginBottom: 10,
    padding: 12,
    width: '100%',
  }),

  balanceIdentity: new Style<View>({
    alignItems: 'center',
    flexDirection: 'row',
    flexGrow: 1,
  }),

  balanceIdentityCompact: new Style<View>({
    alignItems: 'center',
    flexDirection: 'row',
    marginBottom: 8,
    width: '100%',
  }),

  accountCodeBadge: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F3F4F6',
    borderRadius: 5,
    height: 28,
    justifyContent: 'center',
    marginRight: 10,
    width: 58,
  }),

  accountCode: new Style<Label>({
    color: '#374151',
    font: systemBoldFont(13),
    textAlign: 'center',
    width: '100%',
  }),

  balanceNameBlock: new Style<View>({
    flexGrow: 1,
  }),

  accountName: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(16),
    marginBottom: 3,
    width: '100%',
  }),

  accountMeta: new Style<Label>({
    color: '#6B7280',
    font: systemFont(12),
    width: '100%',
  }),

  positiveAmount: new Style<Label>({
    color: '#047857',
    font: systemFont(15),
    marginRight: 12,
    textAlign: 'right',
    width: 112,
  }),

  positiveAmountCompact: new Style<Label>({
    color: '#047857',
    font: systemFont(15),
    marginBottom: 8,
    textAlign: 'left',
    width: '100%',
  }),

  negativeAmount: new Style<Label>({
    color: '#B91C1C',
    font: systemFont(15),
    marginRight: 12,
    textAlign: 'right',
    width: 112,
  }),

  negativeAmountCompact: new Style<Label>({
    color: '#B91C1C',
    font: systemFont(15),
    marginBottom: 8,
    textAlign: 'left',
    width: '100%',
  }),

  selectionActions: new Style<View>({
    flexDirection: 'row',
  }),

  selectionActionsCompact: new Style<View>({
    flexDirection: 'row',
    width: '100%',
  }),

  selectionButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F3F4F6',
    borderRadius: 5,
    justifyContent: 'center',
    marginLeft: 6,
    minHeight: 30,
    paddingLeft: 10,
    paddingRight: 10,
  }),

  selectionButtonCompact: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F3F4F6',
    borderRadius: 5,
    flexGrow: 1,
    justifyContent: 'center',
    marginRight: 8,
    minHeight: 32,
    paddingLeft: 10,
    paddingRight: 10,
  }),

  selectionButtonSelected: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#CCFBF1',
    borderColor: '#14B8A6',
    borderRadius: 5,
    borderWidth: 1,
    justifyContent: 'center',
    marginLeft: 6,
    minHeight: 30,
    paddingLeft: 10,
    paddingRight: 10,
  }),

  selectionButtonSelectedCompact: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#CCFBF1',
    borderColor: '#14B8A6',
    borderRadius: 5,
    borderWidth: 1,
    flexGrow: 1,
    justifyContent: 'center',
    marginRight: 8,
    minHeight: 32,
    paddingLeft: 10,
    paddingRight: 10,
  }),

  selectionButtonText: new Style<Label>({
    color: '#374151',
    font: systemBoldFont(12),
  }),

  selectionButtonTextSelected: new Style<Label>({
    color: '#0F766E',
    font: systemBoldFont(12),
  }),

  transferRow: new Style<View>({
    borderColor: '#E5E7EB',
    borderRadius: 6,
    borderWidth: 1,
    marginBottom: 8,
    paddingBottom: 10,
    paddingTop: 10,
    paddingLeft: 10,
    paddingRight: 10,
    width: '100%',
  }),

  transferHeader: new Style<View>({
    alignItems: 'center',
    flexDirection: 'row',
    marginBottom: 5,
    width: '100%',
  }),

  transferRoute: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(14),
    numberOfLines: 1,
    width: 198,
  }),

  transferAmountPill: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#ECFDF5',
    borderColor: '#A7F3D0',
    borderRadius: 5,
    borderWidth: 1,
    justifyContent: 'center',
    marginLeft: 8,
    minHeight: 28,
    width: 82,
  }),

  transferAmount: new Style<Label>({
    color: '#047857',
    font: systemFont(14),
    textAlign: 'center',
    width: '100%',
  }),

  transferMemo: new Style<Label>({
    color: '#4B5563',
    font: systemFont(13),
    marginBottom: 5,
    numberOfLines: 0,
    width: '100%',
  }),

  transferMeta: new Style<Label>({
    color: '#6B7280',
    font: systemFont(11),
    width: '100%',
  }),

  entryRow: new Style<View>({
    alignItems: 'center',
    borderColor: '#E5E7EB',
    borderRadius: 6,
    borderWidth: 1,
    flexDirection: 'row',
    marginBottom: 8,
    paddingBottom: 8,
    paddingTop: 8,
    paddingLeft: 10,
    paddingRight: 10,
    minHeight: 44,
    width: '100%',
  }),

  entryRowCompact: new Style<View>({
    alignItems: 'stretch',
    borderColor: '#E5E7EB',
    borderRadius: 6,
    borderWidth: 1,
    flexDirection: 'column',
    marginBottom: 8,
    paddingBottom: 8,
    paddingTop: 8,
    paddingLeft: 10,
    paddingRight: 10,
    minHeight: 92,
    width: '100%',
  }),

  entriesList: new Style<View>({
    width: '100%',
  }),

  entryAccount: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(13),
    width: 180,
  }),

  entryAccountCompact: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(13),
    marginBottom: 4,
    width: '100%',
  }),

  entryAmountPositive: new Style<Label>({
    color: '#047857',
    font: systemFont(13),
    textAlign: 'right',
    width: 100,
  }),

  entryAmountPositiveCompact: new Style<Label>({
    color: '#047857',
    font: systemFont(13),
    marginBottom: 4,
    textAlign: 'left',
    width: '100%',
  }),

  entryAmountNegative: new Style<Label>({
    color: '#B91C1C',
    font: systemFont(13),
    textAlign: 'right',
    width: 100,
  }),

  entryAmountNegativeCompact: new Style<Label>({
    color: '#B91C1C',
    font: systemFont(13),
    marginBottom: 4,
    textAlign: 'left',
    width: '100%',
  }),

  entryMemo: new Style<Label>({
    color: '#4B5563',
    flexGrow: 1,
    font: systemFont(12),
    marginLeft: 12,
    numberOfLines: 0,
  }),

  entryMemoCompact: new Style<Label>({
    color: '#4B5563',
    font: systemFont(12),
    marginBottom: 4,
    numberOfLines: 0,
    width: '100%',
  }),

  entryMeta: new Style<Label>({
    color: '#6B7280',
    font: systemFont(11),
    marginLeft: 12,
    numberOfLines: 1,
    textAlign: 'right',
    width: 150,
  }),

  entryMetaCompact: new Style<Label>({
    color: '#6B7280',
    font: systemFont(11),
    numberOfLines: 1,
    width: '100%',
  }),

  paginationBar: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F9FAFB',
    borderColor: '#E5E7EB',
    borderRadius: 6,
    borderWidth: 1,
    flexDirection: 'row',
    marginBottom: 12,
    paddingBottom: 8,
    paddingTop: 8,
    paddingLeft: 10,
    paddingRight: 10,
    width: '100%',
  }),

  paginationBarCompact: new Style<View>({
    alignItems: 'stretch',
    backgroundColor: '#F9FAFB',
    borderColor: '#E5E7EB',
    borderRadius: 6,
    borderWidth: 1,
    flexDirection: 'column',
    marginBottom: 12,
    paddingBottom: 8,
    paddingTop: 8,
    paddingLeft: 10,
    paddingRight: 10,
    width: '100%',
  }),

  paginationLabel: new Style<Label>({
    color: '#4B5563',
    flexGrow: 1,
    font: systemFont(12),
  }),

  paginationActions: new Style<View>({
    flexDirection: 'row',
  }),

  paginationButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#E5E7EB',
    borderRadius: 5,
    justifyContent: 'center',
    marginLeft: 6,
    minHeight: 30,
    width: 42,
  }),

  paginationButtonDisabled: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F3F4F6',
    borderRadius: 5,
    justifyContent: 'center',
    marginLeft: 6,
    minHeight: 30,
    width: 42,
  }),

  paginationButtonSelected: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#CCFBF1',
    borderColor: '#14B8A6',
    borderRadius: 5,
    borderWidth: 1,
    justifyContent: 'center',
    marginLeft: 6,
    minHeight: 30,
    width: 48,
  }),

  paginationButtonText: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(14),
    textAlign: 'center',
    width: '100%',
  }),

  paginationButtonTextDisabled: new Style<Label>({
    color: '#9CA3AF',
    font: systemBoldFont(14),
    textAlign: 'center',
    width: '100%',
  }),

  paginationToggleButtonText: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(12),
    textAlign: 'center',
    width: '100%',
  }),

  paginationToggleButtonTextDisabled: new Style<Label>({
    color: '#9CA3AF',
    font: systemBoldFont(12),
    textAlign: 'center',
    width: '100%',
  }),

  paginationToggleButtonTextSelected: new Style<Label>({
    color: '#0F766E',
    font: systemBoldFont(12),
    textAlign: 'center',
    width: '100%',
  }),

  emptyState: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#F9FAFB',
    borderColor: '#E5E7EB',
    borderRadius: 8,
    borderWidth: 1,
    padding: 18,
    width: '100%',
  }),

  emptyTitle: new Style<Label>({
    color: '#111827',
    font: systemBoldFont(16),
    marginBottom: 6,
    textAlign: 'center',
    width: '100%',
  }),

  emptyBody: new Style<Label>({
    color: '#6B7280',
    font: systemFont(13),
    numberOfLines: 0,
    textAlign: 'center',
    width: '100%',
  }),
};
