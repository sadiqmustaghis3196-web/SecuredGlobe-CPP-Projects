# SecuredGlobe — Advanced Banking Management

A C++17 / Qt 6 Widgets desktop application with a persistent SQLite database, role-based banking services, a navy-and-teal interface, transaction receipts, reports, and automated tests. All supplied people, accounts, cards, and money are fictional educational data. This is not a real banking platform or a certified financial system.

The earlier browser demo remains in `index.html`; it is independent of the desktop application's SQLite database.

## Quick start

Install Qt **6.2 or later**, including Core, GUI, Widgets, SQL, Network, Print Support, Qt Test, and the **QSQLITE driver**. Qt Network is used only for Qt's PBKDF2 password digestor. No external application libraries are required. Charts are painted with QPainter, so Qt Charts is not required.

### Linux (Ubuntu / Debian)

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev libqt6sql6-sqlite qt6-qpa-plugins
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/SecuredGlobe
ctest --test-dir build --output-on-failure
```

### Windows — MSVC

Install Visual Studio's Desktop Development with C++ workload, CMake 3.16+, and the Qt 6 kit matching your compiler (for example `msvc2022_64`). In a Developer Command Prompt:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
build\Release\SecuredGlobe.exe
ctest --test-dir build -C Release --output-on-failure
```

The Qt version path is an example; use the kit you installed. To distribute the Windows executable, run the matching Qt kit's `windeployqt` against it. Include the SQLite SQL plugin and platform plugin.

### Windows — MinGW

Install the MinGW compiler bundled with your Qt kit. Put that compiler and Qt's `bin` directory on PATH; never mix MSVC and MinGW Qt binaries.

```bat
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build-mingw --parallel
build-mingw\SecuredGlobe.exe
```

### macOS

Install CMake, a C++17 compiler, and Qt 6 using the Qt installer or your package manager. Point CMake at your installed Qt prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/SecuredGlobe
ctest --test-dir build --output-on-failure
```

Qt Creator and CLion can open `CMakeLists.txt` directly. In Visual Studio, open the folder and select a Qt-compatible toolchain.

## Demo accounts

On a fresh database, all four accounts use **`DemoBank!2026`** and must choose a new password before entering the workspace.

| Username | Role | Scope |
| --- | --- | --- |
| `admin` | Administrator | All modules, users, audit, settings, backups |
| `manager` | Bank Manager | Customers, accounts, banking operations, loan decisions, cards, reports |
| `teller` | Bank Teller | Customer registration, account opening, deposits, withdrawals, transfers, authorized history |
| `customer` | Customer | Phoenix Baker's own records, outgoing transfers, loan applications and repayments |

Passwords require 12–128 characters, uppercase, lowercase, a number, and a symbol. Passwords use PBKDF2-HMAC-SHA256, a random 24-byte salt per credential, and 600,000 iterations. Three failed attempts lock a login for five minutes. An idle session expires after 15 minutes. Password reset requests appear in the administrator's Notifications; the administrator verifies identity and supplies a temporary password outside the application. There is no email delivery service.

## Using the application

- Select a sidebar module. Double-click a customer, account, transaction, or loan to open its details; the equivalent action buttons are also available.
- New customers start with Pending KYC. A manager or administrator reviews the document reference before an account can open.
- Accounts open with zero balance. Fund them through Cash Deposit. Frozen and closed accounts reject financial operations.
- Transfers use the recipient's numeric account **ID**, shown in Bank Accounts. Customers can send from their own active accounts to an active recipient ID. Each submitted transfer has an idempotency key; a duplicate key is rejected.
- Pending loans require an explicit approval decision. Approval credits the linked account exactly once. Repayments debit that same account. Review the schedule before accepting an application.
- Tables support search, sortable columns, 20-row pagination, CSV export, PDF export, and printing. CSV/PDF exports include all rows matching the search, rather than only the current page. Currency is displayed in USD; storage uses signed 64-bit cents.
- User Management creates logins, changes roles, enables/disables logins, and performs administrator-assisted password resets. Employees link staff logins to branches.

## Forty GUI destinations

These are distinct screen purposes; some contextual forms are pages and others are dialogs. Their navigation and validation are implemented. Construction coverage is not a claim that every native file or printer dialog has been exercised on every operating system.

| # | Screen | How to reach it |
| --- | --- | --- |
| 1 | Splash | Application startup |
| 2 | Login | Startup / after logout |
| 3 | Forgot password / assistance | Login → Forgot password |
| 4 | Main dashboard | Dashboard |
| 5 | User profile | Top profile button |
| 6 | Change password | Profile → Change password; required at first sign-in |
| 7 | Customer list | Customers |
| 8 | Add customer | Customers → Add customer |
| 9 | Customer details | Customer row / View selected |
| 10 | Edit customer | Customer details → Edit |
| 11 | KYC verification | Customer details → Review KYC |
| 12 | Customer account history | Customer or account details → History |
| 13 | Account list | Bank Accounts |
| 14 | Open account | Bank Accounts → Open account |
| 15 | Account details | Account row / Manage selected |
| 16 | Edit account information | Account details → Edit |
| 17 | Freeze / unfreeze | Account details → Freeze / unfreeze |
| 18 | Account closure | Account details → Close account |
| 19 | All transactions | Transactions |
| 20 | Cash deposit | Deposits |
| 21 | Cash withdrawal | Withdrawals |
| 22 | Account transfer | Transfers |
| 23 | Transaction details | Transaction row / View selected |
| 24 | Transaction history and filters | Transactions → Date, type & amount filters |
| 25 | Transaction receipt | Transaction details → Receipt; after financial operations |
| 26 | Loan applications | Loans |
| 27 | Apply for loan | Loans → Apply |
| 28 | Loan approval / rejection | Loans → Review; Loan details → Approval |
| 29 | Loan repayment | Approved loan details → Make repayment |
| 30 | Loan details and schedule | Loan row / Details |
| 31 | Debit card management | Cards |
| 32 | Issue / block card | Cards → Issue or manage |
| 33 | Employee management | Employees; add/edit dialogs |
| 34 | Branch management | Branches; add/edit dialogs |
| 35 | Financial reports | Reports |
| 36 | Customer / account reports | Reports → Customer & account reports |
| 37 | Transaction analytics | Dashboard → Open transaction analytics |
| 38 | User / role management | User Management |
| 39 | Audit logs | Audit Logs |
| 40 | Settings and backup | Settings |

Role restrictions are enforced by the service layer for every operation, including ownership-scoped reads and reports. UI access is only a convenience. This local educational application does not protect against someone who has unrestricted access to its files or can modify its executable.

## Architecture

- `src/core`: database lifetime, migration transaction, RAII transaction guard, sessions, navigation catalog.
- `src/models`: user permission and bank account models.
- `src/repositories`: prepared statements and conversion into table/record values. Database access stays out of the UI.
- `src/services`: seeding, authentication, authorization, banking rules, loan calculations, exports' data selection.
- `src/ui`: desktop login, main window and contextual workflows.
- `src/ui/components`: validated form dialogs, searchable/paginated record tables, numeric money sorting, QPainter chart.
- `src/utils`: exact decimal-to-cents parsing, overflow checks, password derivation and constant-time hash comparison.
- `database/migrations/001_initial.sql`: schema, constraints, foreign keys, indexes and initial schema version.
- `tests`: Qt Test service integration tests, plus the application's offscreen screen-construction smoke check.

Each substantial class has its own header and implementation. Contextual screen builders are methods in `MainWindow`, allowing shared controls without separate duplicated CRUD classes. Widgets use Qt parent ownership; application service dependencies have scoped lifetime. Repository queries always bind user-supplied values.

## Database and business rules

The schema contains users, roles, permissions, user_roles, role_permissions, customers, customer_kyc, bank_accounts, account_types, transactions, transfers, loans, loan_repayments, debit_cards, employees, branches, audit_logs, login_attempts, notifications, application_settings, and schema_migrations. Foreign keys are enabled per connection. Migration version 1 is applied transactionally on a new database; a newer unsupported version is rejected.

Balances and transaction values are integer cents. Deposits, withdrawals, transfers, disbursements, and repayments update balances, ledger, and audit records inside one database transaction. Any exception rolls everything back. Transfers record a debit and matching credit plus a unique request key. Withdrawals/transfers enforce a configurable per-operation limit. Closure requires a zero balance and no pending or outstanding loan linked to the account; closure also blocks attached cards.

Loan rate is fixed at 8.50% annually in this demo. Monthly interest uses a reducing principal balance: `monthlyRate = annualRateBps / 120000`. The nominal monthly payment is `ceil(P*r/(1-(1+r)^(-n)))` in cents, or `ceil(P/n)` at zero interest. Each month's interest is rounded to the nearest cent. The final installment clears the remaining principal; the stored total due equals the sum of the schedule. Dates start one month after approval, with Qt calendar-month handling. Pending-loan dates are indicative. Repayments apply against the scheduled total without late fees, early-settlement interest rebates, variable rates, credit bureau scoring, or regulatory underwriting.

The fresh dataset has 30 customers, 40 accounts, 120 ledger transactions, 8 loan applications, 10 masked demo cards, 3 branches and 4 users. Opening-funding ledger entries reconcile with seeded balances. Dashboard values and charts are queried from the database after initialization; no fake metrics are inserted into the UI.

## Storage, backup, and restore

The default file is `banking.sqlite` inside `QStandardPaths::AppDataLocation` for organization/application `SecuredGlobe`. Typical locations:

- Linux: `~/.local/share/SecuredGlobe/SecuredGlobe/`
- Windows: `%APPDATA%\SecuredGlobe\SecuredGlobe\`
- macOS: `~/Library/Application Support/SecuredGlobe/SecuredGlobe/`

The exact location follows Qt/platform settings. Select a specific path with:

```sh
./build/SecuredGlobe --database /absolute/path/banking.sqlite
```

Settings → Create SQLite backup uses `VACUUM INTO` and requires a new filename. To restore, close all application instances, preserve the current database under a different filename, copy the backup to the selected database path, and relaunch. Do not copy over a database that is still open. The database is not encrypted; treat your local file access appropriately. The app is designed for a single local workstation, not a network drive or distributed banking server.

## Tests and verification

```sh
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/SecuredGlobe --smoke-test
```

The smoke test uses a temporary database, changes its demo admin password, constructs authenticated screens 4–40, creates the login and reset dialogs, and launches/renders the dashboard offscreen. It does not modify your normal database. Set `SG_SCREENSHOT=/absolute/path/dashboard.png` when running it to save a screenshot. Splash is rendered during startup. For screenshot instructions and tested scope, see `docs/VALIDATION.md`.

Four GUI workflow tests exercise real form submission and confirmation buttons for deposits/receipts, transfers, customer/KYC/account opening, and loan approval/repayment.

Service tests cover login, password requirements, lockout/expiry, RBAC and ownership, customer creation, KYC, account opening, exact money parsing, deposits, withdrawals, insufficient balance, frozen/closed states, atomic transfers, forced credit-failure rollback, duplicate requests, loan arithmetic/schedules/repayment, card state changes, backup, persistence, filters and audit events.

## Troubleshooting

- **Qt not found:** set `CMAKE_PREFIX_PATH` to the Qt kit root; ensure compiler and architecture match.
- **QSQLITE driver not loaded:** install/copy the SQLite plugin and its runtime dependencies. On Ubuntu install `libqt6sql6-sqlite`.
- **Qt platform plugin error:** use the appropriate platform plugin (`windows`, `cocoa`, or `xcb`). Offscreen mode is for tests only.
- **Login rejected:** verify the selected database, username, changed password, and five-minute lockout. The initial password only works before you change it.
- **Permission error:** the operation needs a different role. Existing users may need to sign in again after role changes.
- **Account cannot close:** clear the balance and resolve linked pending or approved loans.
- **Reports are empty:** check the date range and table search.
- **PDF or print failure:** verify the destination is writable and a printer is configured. Save PDF does not require a physical printer.

Windows and macOS build commands are provided but must be validated on those platforms. See the validation record for the exact locally tested environment.
