CLKnDGR-Readme
Subtle explanation of the contract I am working on. - Misfitcompany K.I.R | D.O.M

### DISCLAIMER: Claude Coded

# Cloak and Dagger (CLKnDGR)

**A proposed arbitrage liquidity protocol for Qubic — a design in development, shared for community feedback.**

> **Note:** This describes a design in development. The CLKnDGR contract is not yet deployed — whether
> it reaches the Qubic network is up to Computor Governance, which would need to approve it. The
> protocols it builds on — Qearn, QX, and Qswap — are already live on-chain; it's CLKnDGR itself that's
> still being built. The shareholder and depositor governance described below is the contract's own
> internal governance, which would apply once it's running. Everything here describes how the contract
> is intended to work.

---

## What is Cloak and Dagger?

Cloak and Dagger is a proposed smart contract for the Qubic network, designed to run automated
arbitrage between QU and Qubic-native token markets. It would pool capital from two groups —
shareholders and vault depositors — and use that capital to find and capture price discrepancies
across pools, returning profit to participants at the end of each epoch.

The contract name is `CLKnDGR`. Shares would be issued via Qubic's IPO mechanism, if it proceeds to one.

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

The contract scans registered asset pools approximately **4 times per second** (once per network tick) for profitable price discrepancies between the asset's QU value on one side and market price on the other. When a discrepancy exceeds a configurable minimum **net** profit threshold (after trading fees), the contract executes a trade using its available capital.

Pools are registered on-chain via the governance system (shareholder vote). Active pools
participate in arbitrage; deactivated pools are skipped but their records are retained.

---

## Trading Strategies

The contract runs two distinct named strategies every tick, in order. They share the same pool registry and capital base, but operate independently and serve different market conditions.

### The Cloak — Swing Trading

The Cloak is a medium-term swing trading strategy. It watches for meaningful price dips on Qswap using two moving averages, **dollar-cost-averages into** a position as the dip deepens, then **averages out** as the price recovers.

**Entry condition:** The current 1-week average price for a pool has fallen to **≤ 90% of the 3-month average price** — a statistically significant dip relative to recent history.

**On entry & accumulation (DCA-in):** The first buy on a fresh dip uses **1% of trading capital**. If the dip is still present at later monthly checks, it **adds 0.25% more** each time — averaging down — **until the position's cost basis reaches 5% of capital**. That cap re-opens automatically as the fund grows or as the position is sold down. All buys use up to 5% slippage tolerance.

**Exit condition:** The current Qswap pool price is **≥ 112% of the average cost paid** for the held position.

**On exit:** a governable chunk — **50% of the held position by default** (tunable 10–50% via UPDATE_SWING_SELL_PCT) — is offered on QX as an ask order, priced at 10% below the current Qswap pool price to ensure fills. The position is held until fully sold across multiple exit events.

**Check cadence:** The Cloak evaluates each pool about **once a month** — for a first buy, a DCA-in add, or a sale. It only queries the live market price for pools it **actually holds** (needed for the sell decision); idle pools it's merely watching for a dip are evaluated from the stored weekly price history at no market-call cost. This keeps the long-term strategy patient and cheap to run through quiet stretches.

**Liquidity check (exit only):** Before placing the ask, the contract verifies that at least 80% of the target proceeds exist as qualifying bids on QX. If the order book is too thin, the exit is deferred to the next eligible tick.

Each pool runs its own independent Cloak position. The contract holds one swing position per pool, which it **dollar-cost-averages into** on continued dips (up to the 5%-of-capital cap) and **averages out of** as the price recovers.

---

### The Dagger — Spot Arbitrage (bidirectional)

The Dagger is a real-time cross-exchange arbitrage strategy. It identifies and captures price differences between QX (order book) and Qswap (AMM) in a single tick. It works in **both directions** — capturing the spread whichever way the price gap happens to lean.

**Volatility-gated scanning (VIX).** Checking the exchanges costs execution fees, so the Dagger does not poll every pool constantly. Instead it keeps a cheap per-token **volatility index**, sampled from the Qswap pool once a day by default (governable to 2× or 3× via UPDATE_VIX_PULSE_RATE), that tracks how much each token is moving — a fast (≈5-day) reading against a slow (≈4-week) baseline. When a token's recent volatility breaks out above its own baseline, the Dagger wakes and hunts it at full speed (that's when cross-exchange gaps actually appear); when a token is calm it falls back to a sparse safety-net check. This concentrates the fee budget on tokens that are actually moving. The breakout sensitivity (UPDATE_VIX_FACTOR) and the minimum-movement floor (UPDATE_VIX_FLOOR) are both governable.

**Direction B — buy QX, sell Qswap:** when the best *ask* on QX is cheaper than what Qswap would pay for the same tokens.
- **Leg 1:** Buy tokens on QX at the best available ask price.
- **Leg 2:** Immediately sell those tokens on Qswap, capturing the spread.

**Direction A — buy Qswap, sell QX:** the mirror image, for when Qswap is cheaper than the best *bid* on QX (a buyer waiting on QX is offering more than Qswap charges).
- **Leg 1:** Buy tokens on Qswap.
- **Leg 2:** Sell those tokens into the QX bid, capturing the spread. (The QX sale matches immediately; the QU it pays out lands the following tick and flows into the next profit cycle — so this side's profit is realized a tick later, the same way the Cloak's sales are.)

For any given pool at any given moment only one direction can be profitable — a buyer can't be paying more than a seller is asking on a healthy market — so the two never compete. Each is checked independently and at most one fires per pool per tick.

**Profit floor:** A trade only executes if the estimated net profit exceeds the `minProfitQu` threshold (governable), and each direction re-checks profitability against the *actual* fill before committing capital — it never sells at a thin or negative margin. Trades below the floor are skipped.

**Capital scaling:** When a large price gap would produce profit far above the floor, the trade size is scaled down proportionally. This prevents any single pool from consuming most of the trading capital in one tick, keeping later pools in the loop competitive.

**Reserve accumulation:** A portion of every Dagger trade — either direction — is held back as a per-pool token reserve rather than immediately sold. These reserves accumulate over time and are eventually liquidated when they can be sold profitably above a governable minimum return threshold.

**Per-pool cooldown:** Each pool carries independent cooldown timers — one per direction, since a missing opportunity one way says nothing about the other. Pools that are unaffordable, have no liquidity, or don't exist yet on Qswap are placed on a longer cooldown to avoid wasting ticks rechecking them.

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
| **Dev fund** | Protocol development reserve (shareholder-controlled withdrawal) |
| **CCF** | Qubic Computor Controlled Fund contribution |

### The payout presets

| Preset | Trading pool | Exec fees | Qearn boost | Shareholders | Dev fund | CCF |
|---|---|---|---|---|---|---|
| **0 — Default** | 55% | 30% | 3% | 10% | 1% | 1% |
| **1 — Growth** | 61% | 27% | 3% | 7% | 1% | 1% |
| **2 — Aggressive** | 65% | 25% | 3% | 5% | 1% | 1% |
| **3 — Recovery** | 0% | 100% | 0% | 0% | 0% | 0% |

Preset 0 pays shareholders the most (10%) and funds execution moderately. Presets 1 and 2
progressively steer more toward the trading pool and execution-fee funding, with a smaller
shareholder dividend (7% → 5%). The Qearn boost (3%), dev fund (1%), and CCF (1%) are constant
across presets 0–2. Shareholders vote to change the active preset.

**Preset 3 — Recovery / limp mode** is special: the contract **keeps trading normally** but routes
**100% of its profit** into the execution-fee reserve, suspending the shareholder, Qearn, dev-fund, and
CCF payouts — so it rebuilds its on-chain fee budget fast, from its own earnings. It is applied **automatically** whenever the execution-fee reserve falls
below the governable floor (see UPDATE_EXEC_RESERVE_FLOOR), and shareholders can also select it
manually. The contract returns to the chosen preset on its own **once the reserve climbs back to 10%
above the floor** — a hysteresis buffer, so it can't rapidly flap in and out of recovery at the
boundary. (The previously-chosen preset is preserved; recovery never overwrites it.)

---

## On-Chain Governance

> **Important:** There is no front-end or web interface planned for Cloak and Dagger governance. As designed, all proposal submission, voting, and veto actions would be performed directly via the `qubic-cli` command-line tool. A full ABI reference with exact command formats for every procedure is provided alongside this document. Shareholders and depositors would interact with the contract directly — no intermediary tool is required or endorsed.

Governance proposals are submitted by shareholders (requires holding ≥1 CLKnDGR share) and
voted on by the shareholder group. A proposal passes when:

1. A minimum number of unique voters participate (default: 15)
2. A minimum total weighted share count has voted
3. At least 2/3 of weighted votes are Yes (supermajority)
4. Fewer than 125 qualifying depositor NO votes have been cast (depositor veto)

The depositor veto exists specifically so that large depositors cannot be harmed by governance
decisions without a meaningful check. Veto votes are re-validated at epoch end against the
current NAV — a depositor who has lost significant value no longer qualifies.

### Governable parameters (23 proposal types)

| # | Proposal | Fee | What it changes |
|---|---|---|---|
| 1 | **ADD_POOL** | 200M QU | Registers a new token/QU pool for arbitrage. Asset name and issuer required. Max 255 pools total. |
| 2 | **REMOVE_POOL** | 50M QU | Deactivates an active pool. Arbitrage stops; pool record and any held reserves are retained. |
| 3 | **REACTIVATE_POOL** | 50M QU | Re-enables a previously deactivated pool. |
| 4 | **UPDATE_MIN_PROFIT** | 50M QU | Sets the minimum **net** QU profit per arb (after the Qswap swap fee) required before a Dagger trade executes. Also used as the Cloak's minimum capital to open a buy. Allowlist: 100,100 / 250,100 / 420,000 / 676,420. |
| 5 | **WITHDRAW_QU_RESERVE** | 50M QU | Transfers QU from the on-chain dev fund reserve to a specified destination. Amount must not exceed the current reserve. |
| 6 | **UPDATE_PROPOSAL_FEE** | 50M QU | Changes the default submission fee for all proposal types except ADD_POOL and UPDATE_PAYOUT. |
| 7 | **UPDATE_PAYOUT** | 69M QU | Switches the active profit distribution preset (0, 1, 2, or 3 = recovery — see table above). |
| 8 | **UPDATE_FEE_ADD_POOL** | 50M QU | Changes the submission fee specifically for ADD_POOL proposals. |
| 9 | **UPDATE_FEE_PAYOUT** | 50M QU | Changes the submission fee specifically for UPDATE_PAYOUT proposals. |
| 10 | **UPDATE_MIN_QUORUM** | 50M QU | Changes the minimum number of unique qualified voters required for any proposal to pass (range: 15–676). |
| 11 | **WITHDRAW_ASSET_RESERVE** | 50M QU | Transfers token reserve holdings from a deactivated pool to a specified destination address. |
| 12 | **UPDATE_VAULT_TIER** | 50M QU | Changes the minimum deposit size for the vault using a tier index (0–8). At initial share price, this ranges from 10,000 QU (tier 0) to 10,000,000 QU (tier 8, default). |
| 13 | **UPDATE_RESERVE_PROFIT_PCT** | 50M QU | Changes the minimum profit percentage required before the contract sells accumulated token reserves. Valid values: 2%, 5%, 7%, or 10%. |
| 14 | **UPDATE_DEPOSITOR_VOTE_MIN** | 50M QU | Changes the minimum locked QU required for a vault depositor's veto vote to qualify. Options: 50M, 150M (default), 250M, or 350M QU. |
| 15 | **UPDATE_RELOCK_AMOUNT** | 50M QU | Changes the minimum additional QU a depositor must add when relocking their vault position. Options: 1M, 5M, 10M (default), 20M, 25M, or 50M QU. |
| 16 | **UPDATE_EXEC_RESERVE_FLOOR** | 50M QU | Sets the execution-fee-reserve safety floor. If the contract's on-chain fee budget drops below it, the contract keeps trading but routes 100% of its profit to the fee reserve (recovery preset) until it refills to 10% above the floor. Options: 0 (off, default), 1B, 5B, 10B, or 20B QU. |
| 17 | **SELL_POOL_TOKENS** | 50M QU | Market-sells a chosen percentage (1–100%) of a pool's token holdings on Qswap for QU. The proceeds stay in the contract and are folded into the next profit split — so **both vault depositors and shareholders** benefit (unlike WITHDRAW_ASSET_RESERVE, which sends raw tokens to an outside address). The governance "take profit" / "exit a bag" button. Works on active pools (skim profit while trading continues) or inactive ones (liquidate a paused bag). A 10% slippage floor reverts a catastrophic fill. |
| 18 | **UPDATE_VIX_FACTOR** | 50M QU | Tunes the Dagger's volatility-breakout sensitivity — how far a token's recent (≈5-day) volatility must rise above its own (≈4-week) baseline before the Dagger starts hunting it. Stored ×100. Options (multiplier of the token's own baseline): 0.09, 0.18, 0.37, 0.75, 1.5, 2 (default), 2.25, 2.75, 3.5, 4.5, 5 — submitted as 9 / 18 / 37 / 75 / 150 / 200 / 225 / 275 / 350 / 450 / 500. Below 1× the Dagger hunts almost always (the floor becomes the gate); higher = more selective. |
| 19 | **UPDATE_VIX_FLOOR** | 50M QU | Minimum absolute recent volatility (in basis points) for a token to count as "breaking out" — keeps a near-dead token from triggering on a tiny wiggle. Options: 0 (ratio-only), 10, 25 (default), 50, 100, 200 bps. |
| 20 | **UPDATE_VIX_PULSE_RATE** | 50M QU | How many times a day the Dagger samples each pool's price to update its volatility reading. Options: **1 (default)**, 2, or 3 per day. More = sharper/faster detection but more fee cost; fewer = cheaper but slower to notice a token heating up. The 5-day/4-week volatility horizons stay fixed regardless — only the sampling frequency changes. |
| 21 | **UPDATE_SWING_SELL_PCT** | 50M QU | The Cloak's sell chunk — what % of a held bag it sells each time the +12% profit trigger fires. Options: 10, 15, 20, 25, 33, **50 (default)**. Higher = takes profit faster; lower = rides winners longer. |
| 22 | **UPDATE_BREAKOUT_RESCAN** | 50M QU | How often the Dagger re-checks a hot (breaking-out) pool while it looks for a gap (it still trades every tick once a real gap is found — this only paces the empty looks). Submitted in seconds: 30, 60, 120, 180, 240, **300 (default = 5 min)**. Shorter = catches fast gaps but costs more; longer = cheaper. |
| 23 | **UPDATE_QX_FEE_MODE** | 50M QU | How the contract sources QX's share-transfer fee for its sell orders. **0 (default)** = use a per-epoch cached value (cheapest; QX's fee is a fixed 100 QU today). **1** = fetch the fee live from QX before every sell. A forward-looking switch: if QX ever changes its fee model so the fee can move tick-to-tick, shareholders flip this to 1 — no contract re-deploy needed. |

**What happens to the fee:** every proposal fee funds the contract's **execution-fee reserve** — none is burned from supply, and none is kept as trading capital.

| Outcome | Proposer receives back | To execution-fee reserve |
|---|---|---|
| **Passed** | Nothing — fee fully consumed | **100%** |
| **Failed / Vetoed** | **69%** (refunded at epoch end) | 31% |

The non-refundable 31% is routed to the reserve at submission; on a pass the held 69% is sent there too (100% total), and on a failure that 69% is refunded to the proposer.

---

## Donations

Anyone can support the contract with a donation of **any amount of QU**. **100% of a donation
goes to the contract's execution-fee reserve** — the on-chain budget that lets the contract keep
running every tick. There is no fixed amount and no minimum; the full amount sent is kept.

Donations don't buy vault shares, earn yield, or change the vault's value — they're simply a way
for supporters to help keep the contract funded and operational.

---

## Security Design Notes

- All governance changes require an on-chain proposal with submission fee and supermajority vote.
- The depositor veto is an independent check on shareholder power.
- Early exit penalties protect depositors who honor the lock from value dilution by those who exit.
- No admin keys. No owner overrides. Governance is the only upgrade path. (Dev Fund is for hard fixes)
- The contract is open source; the source would be published if it ever advances toward deployment.

---

## What We're Looking For

This document is a design preview. The contract is written and in testing. We'd welcome
community input on:

1. **Profit split presets** — do the three options cover the range of preferences, or is something missing?
2. **Vault mechanics** — is the 26-epoch lock + early exit penalty the right trade-off? Too long? Too short?
3. **Depositor veto threshold** — 125 out of up to 5,000 depositors, with a minimum locked QU. Too easy to veto? Too hard?
4. **Donations** — does routing 100% of a donation to the contract's execution-fee reserve (keeping it funded to run) feel right, or should donations do something else?
5. **Pool governance fees** — ADD_POOL costs 200M QU. Is this an appropriate barrier to prevent spam pools?
6. **Any other comment or concern**

---

## Status

- Contract: written, in unit testing — a work in progress, not yet deployed.
- Deployment: up to Computor Governance — reaching the network would require the Computors to approve
  the contract, which hasn't happened and isn't a given.
- Source code: would be published if the project advances toward deployment.

*Questions and feedback welcome — this is a proposed protocol and we'd like to get the design right.*
