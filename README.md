CLKnDGR-Readme
Subtle explanation of the contract I am working on. - Misfitcompany K.I.R | D.O.M

### DISCLAIMER: Claude Coded

# Cloak and Dagger (CLKnDGR)

**An on-chain arbitrage liquidity protocol for Qubic — seeking community feedback on design.**

---

## What is Cloak and Dagger?

Cloak and Dagger is a smart contract on the Qubic network that runs automated arbitrage between
QU and Qubic-native token markets. It pools capital from two groups — shareholders and vault
depositors — and uses that capital to find and capture price discrepancies across pools, returning
profit to participants at the end of each epoch.

The contract name is `CLKnDGR`. Shares are issued via Qubic's IPO mechanism.

---

## Two Participant Classes

### Shareholders

Shareholders hold IPO governance tokens. They propose and vote on changes to contract parameters
via an on-chain proposal system. A supermajority is required for any proposal to pass, and
depositors hold a veto right that can block shareholder decisions.

### Vault Depositors

Anyone can deposit QU into the vault as additional trading capital. Depositors receive vault
shares priced against a NAV (net asset value) that rises or falls with trading performance.
Deposits are subject to a **26-epoch lock period**. Early exits are permitted but carry a
significant penalty — designed to protect remaining depositors from timing games.

These two roles are separate and non-exclusive. A single wallet can hold both governance shares
and a vault position.

---

## How the Vault Works

- **Deposit:** Send QU to the vault. Shares are issued at the current share price (NAV per share).
  If the vault is at capacity (5,000 depositors), you are placed on a waitlist (up to 500 entries,
  served largest-first as slots open).
- **Share price:** Calculated from total vault capital divided by total shares outstanding. Grows
  with profitable epochs; shrinks with unprofitable ones.
- **Lock period:** 26 epochs. You can exit early with a penalty, or relock within the final
  4 epochs — relocking resets your position to a fresh 26-epoch lock from the current epoch
  and requires adding a minimum amount of QU (initial minimum: 10M QU, governable).
- **Withdrawal:** At lock expiry, a 2% management fee applies. Profitable exits also carry a 5%
  performance fee. Early exits pay a 38% penalty on top.
- **Depositor veto:** Depositors with sufficient locked QU can veto shareholder proposals — a
  check-and-balance built into the governance system.

---

## How Arbitrage Works

The contract scans registered asset pools approximately **4 times per second** (once per network tick) for profitable price discrepancies between the asset's QU value on one side and market price on the other. When a discrepancy exceeds a configurable minimum profit threshold, the contract executes a trade using its available capital.

Pools are registered on-chain via the governance system (shareholder vote). Active pools
participate in arbitrage; deactivated pools are skipped but their records are retained.

---

## Trading Strategies

The contract runs two distinct named strategies every tick, in order. They share the same pool registry and capital base, but operate independently and serve different market conditions.

### The Cloak — Swing Trading

The Cloak is a medium-term swing trading strategy. It watches for meaningful price dips on Qswap using two moving averages and builds a position, then exits gradually as the price recovers.

**Entry condition:** The current 1-week average price for a pool has fallen to **≤ 90% of the 3-month average price** — a statistically significant dip relative to recent history.

**On entry:** The contract buys tokens on Qswap using **1% of the current available trading capital**, with up to 5% slippage tolerance.

**Exit condition:** The current Qswap pool price is **≥ 112% of the average cost paid** for the held position.

**On exit:** **20% of the held position** is offered on QX as an ask order, priced at 10% below the current Qswap pool price to ensure fills. The position is held until fully sold across multiple exit events.

**Cooldown:** ~31.25 hours after any buy or sell action on a given pool, preventing over-trading on volatile days.

**Liquidity check (exit only):** Before placing the ask, the contract verifies that at least 80% of the target proceeds exist as qualifying bids on QX. If the order book is too thin, the exit is deferred to the next eligible tick.

Each pool runs its own independent Cloak position. The contract holds at most one swing position per pool at a time and does not open a new entry while a position is already held.

---

### The Dagger — Spot Arbitrage

The Dagger is a real-time cross-exchange arbitrage strategy. It identifies and captures price differences between QX (order book) and Qswap (AMM) in a single tick.

**Opportunity:** The best ask price on QX is lower than what Qswap would pay for the same tokens.

**Execution (two legs, same tick):**
- **Leg 1:** Buy tokens on QX at the best available ask price.
- **Leg 2:** Immediately sell those tokens on Qswap, capturing the spread as profit.

**Profit floor:** The trade only executes if the estimated net profit exceeds the `minProfitQu` threshold (governable). Trades that would generate less profit than the floor are skipped.

**Capital scaling:** When a large price gap would produce profit far above the floor, the trade size is scaled down proportionally. This prevents any single pool from consuming most of the trading capital in one tick, keeping later pools in the loop competitive.

**Reserve accumulation:** A portion of every Dagger trade is held back as a per-pool token reserve rather than immediately sold. These reserves accumulate over time and are eventually liquidated when they can be sold profitably above a governable minimum return threshold.

**Per-pool cooldown:** Each pool carries an independent cooldown timer. Pools that are unaffordable, have no liquidity, or don't exist yet on Qswap are placed on a longer cooldown to avoid wasting ticks rechecking them.

---

### Recovery System

Both strategies transfer share management rights between the contract, QX, and Qswap as part of their execution. If a rights transfer fails mid-trade (a network-level edge case), the contract does not lose track of the tokens. A dedicated recovery pass runs at the start of every tick, silently retrying failed transfers until they succeed. Recovered tokens are either returned to their original position or folded into the pool reserve depending on where in the trade they became stuck.

---

## Profit Distribution

At the end of each epoch, trading profit is split across several destinations. The exact split
is a governable parameter — shareholders can vote to change the active preset. The presets
balance reinvestment (growing the trading pool and funding execution), ecosystem contribution
(boosting Qearn and the Computor Controlled Fund), and shareholder reward.

**A note on Qearn:** rather than locking its Qearn allocation to earn yield for itself, Cloak and
Dagger **donates** that slice to the Qearn bonus pool — raising the locking rewards earned by the
entire Qubic community. The contract keeps nothing back: it bolsters Qearn rather than profiting
from it.

| Destination | Role |
|---|---|
| **Trading pool** | Reinvested as vault capital — benefits depositors via NAV growth |
| **Execution fees** | Funds on-chain execution costs |
| **Qearn boost** | Donated to the Qearn bonus pool — raises locking rewards for the whole Qubic community |
| **Shareholders** | Direct epoch dividend to governance token holders |
| **Burn** | Deflationary supply reduction (currently set to 0% in all presets) |
| **Dev fund** | Protocol development reserve (shareholder-controlled withdrawal) |
| **CCF** | Qubic Computor Controlled Fund contribution |

### The three presets

| Preset | Trading pool | Exec fees | Qearn boost | Shareholders | Burn | Dev fund | CCF |
|---|---|---|---|---|---|---|---|
| **0 — Default** | 55% | 30% | 3% | 10% | 0% | 1% | 1% |
| **1 — Growth** | 61% | 27% | 3% | 7% | 0% | 1% | 1% |
| **2 — Aggressive** | 65% | 25% | 3% | 5% | 0% | 1% | 1% |

Preset 0 pays shareholders the most (10%) and funds execution moderately. Presets 1 and 2
progressively steer more toward the trading pool and execution-fee funding, with a smaller
shareholder dividend (7% → 5%). The Qearn boost (3%), dev fund (1%), and CCF (1%) are constant
across all three, and trading profit is no longer burned (burns still occur via vault fees on
withdrawal). Shareholders vote to change the active preset.

---

## On-Chain Governance

> **Important:** There is no front-end or web interface planned for Cloak and Dagger governance. All proposal submission, voting, and veto actions are performed directly via the `qubic-cli` command-line tool. A full ABI reference with exact command formats for every procedure is published alongside this document. Shareholders and depositors are expected to interact with the contract directly — no intermediary tool is required or endorsed.

Governance proposals are submitted by shareholders (requires holding ≥1 CLKnDGR share) and
voted on by the shareholder group. A proposal passes when:

1. A minimum number of unique voters participate (default: 15)
2. A minimum total weighted share count has voted
3. At least 2/3 of weighted votes are Yes (supermajority)
4. Fewer than 125 qualifying depositor NO votes have been cast (depositor veto)

The depositor veto exists specifically so that large depositors cannot be harmed by governance
decisions without a meaningful check. Veto votes are re-validated at epoch end against the
current NAV — a depositor who has lost significant value no longer qualifies.

### Governable parameters (15 proposal types)

| # | Proposal | Fee | What it changes |
|---|---|---|---|
| 1 | **ADD_POOL** | 200M QU | Registers a new token/QU pool for arbitrage. Asset name and issuer required. Max 255 pools total. |
| 2 | **REMOVE_POOL** | 50M QU | Deactivates an active pool. Arbitrage stops; pool record and any held reserves are retained. |
| 3 | **REACTIVATE_POOL** | 50M QU | Re-enables a previously deactivated pool. |
| 4 | **UPDATE_MIN_PROFIT** | 50M QU | Sets the minimum QU profit required before a Dagger trade executes. Also gates the Cloak's buy entries. |
| 5 | **WITHDRAW_QU_RESERVE** | 50M QU | Transfers QU from the on-chain dev fund reserve to a specified destination. Amount must not exceed the current reserve. |
| 6 | **UPDATE_PROPOSAL_FEE** | 50M QU | Changes the default submission fee for all proposal types except ADD_POOL and UPDATE_PAYOUT. |
| 7 | **UPDATE_PAYOUT** | 69M QU | Switches the active profit distribution preset (0, 1, or 2 — see table above). |
| 8 | **UPDATE_FEE_ADD_POOL** | 50M QU | Changes the submission fee specifically for ADD_POOL proposals. |
| 9 | **UPDATE_FEE_PAYOUT** | 50M QU | Changes the submission fee specifically for UPDATE_PAYOUT proposals. |
| 10 | **UPDATE_MIN_QUORUM** | 50M QU | Changes the minimum number of unique qualified voters required for any proposal to pass (range: 15–676). |
| 11 | **WITHDRAW_ASSET_RESERVE** | 50M QU | Transfers token reserve holdings from a deactivated pool to a specified destination address. |
| 12 | **UPDATE_VAULT_TIER** | 50M QU | Changes the minimum deposit size for the vault using a tier index (0–8). At initial share price, this ranges from 10,000 QU (tier 0) to 10,000,000 QU (tier 8, default). |
| 13 | **UPDATE_RESERVE_PROFIT_PCT** | 50M QU | Changes the minimum profit percentage required before the contract sells accumulated token reserves. Valid values: 2%, 5%, 7%, or 10%. |
| 14 | **UPDATE_DEPOSITOR_VOTE_MIN** | 50M QU | Changes the minimum locked QU required for a vault depositor's veto vote to qualify. Options: 50M, 150M (default), 250M, or 350M QU. |
| 15 | **UPDATE_RELOCK_AMOUNT** | 50M QU | Changes the minimum additional QU a depositor must add when relocking their vault position. Options: 1M, 5M, 10M (default), 20M, 25M, or 50M QU. |

**What happens to the fee:**

| Outcome | Proposer receives back | Burned | Execution costs | Trading capital |
|---|---|---|---|---|
| **Passed** | Nothing — fee is fully consumed | 5% | 5% | 90% |
| **Failed / Vetoed** | 69% refund at epoch end | 5% | 5% | 21% |

The 5% burn and 5% execution cost are taken immediately at submission. The remainder stays in the contract as unearmarked trading capital — benefiting depositors via NAV growth over time.

---

## Donation Mechanisms

Two paths exist for anyone to contribute QU to the contract's capital base:

- **Shareholder donation (`DonateToContract`):** Available to wallets holding at least one
  CLKnDGR share. Send exactly **6,900,420 QU** — split evenly between the trading pool and
  the Qearn boost. Overpayment is refunded; underpayment is rejected.
- **Public donation (`PublicDonate`):** Open to any wallet. Send exactly **3,000,000 QU** —
  80% goes to the Qearn boost, 20% to the trading pool. Overpayment is refunded;
  underpayment is rejected.

Donations credit the vault NAV at the next epoch boundary — they do not trigger an immediate
share price update mid-epoch.

---

## Security Design Notes

- All governance changes require an on-chain proposal with submission fee and supermajority vote.
- The depositor veto is an independent check on shareholder power.
- Early exit penalties protect depositors who honor the lock from value dilution by those who exit.
- No admin keys. No owner overrides. Governance is the only upgrade path. (Dev Fund is for hard fixes)
- The contract is open source and will be published to the Qubic core repository.

---

## What We're Looking For

This document is a design preview. The contract is written and in testing. We'd welcome
community input on:

1. **Profit split presets** — do the three options cover the range of preferences, or is something missing?
2. **Vault mechanics** — is the 26-epoch lock + early exit penalty the right trade-off? Too long? Too short?
3. **Depositor veto threshold** — 125 out of up to 5,000 depositors, with a minimum locked QU. Too easy to veto? Too hard?
4. **Donation split** — does the 50/50 shareholder donation and 80/20 public donation feel right as incentive design?
5. **Pool governance fees** — ADD_POOL costs 200M QU. Is this an appropriate barrier to prevent spam pools?
6. **Any other comment or concern**

---

## Status

- Contract: written, in unit testing
- Deployment: pending IPO epoch scheduling
- Source code: to be published alongside deployment announcement

*Questions and feedback welcome. This is a community protocol and we want to get the design right before launch.*
