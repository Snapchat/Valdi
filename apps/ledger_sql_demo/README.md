# Ledger SQL Demo

Valdi app that exercises ClientSQL generated bindings with a small double-entry ledger.

The app opens `LedgerDb`, seeds a handful of accounts, and subscribes to reactive aggregate queries for balances, recent ledger entries, and the transaction log. Transfers run inside `LedgerDb.transaction()`: each transfer inserts the debit entry, credit entry, and transaction log row as one committed unit. The generated binding calls the native ClientSQL transaction API, so writer work is serialized by the SQLite runtime and watched aggregate queries refresh after the native commit completes.

The Valdi debugger Data section includes a ClientSQL browser that can inspect the live database while this demo is running. For a live target it shows table rows plus ClientSQL runtime state such as active transaction, deferred writer work, handle count, reader pool readiness, watcher count, and a bounded transaction history with commit/rollback durations.

Expected local workflow:

```bash
valdi install macos --application //apps/ledger_sql_demo:ledger_sql_demo_macos
valdi hotreload --target //apps/ledger_sql_demo:ledger_sql_demo_hotreload
```

Hot reload can update the Valdi TypeScript UI, but changes to the native ClientSQL runtime require rebuilding and relaunching the macOS app.

iOS build validation:

```bash
cd apps/ledger_sql_demo
npm run ios:build
```

Android build and install workflow:

```bash
valdi install android \
  --application //apps/ledger_sql_demo:ledger_sql_demo_android
```

To run the same flow as an Android smoke check, with a UI assertion and screenshot capture:

```bash
cd apps/ledger_sql_demo
npm run android:smoke:build
```

The smoke script waits for a connected emulator/device, installs the APK, launches the demo, taps `Run stress batch`, waits for the transaction-complete status text, and writes `/tmp/ledger-sql-android-smoke.png`. Use `ADB=/path/to/adb` or `--device <serial>` when your local `adb` selection needs to be explicit.

The demo should seed on first launch, and the `Run stress batch` button should append four transfer rows in one transaction, causing the reactive totals to refresh once after commit.
