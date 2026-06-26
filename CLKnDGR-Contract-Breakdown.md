# CLKnDGR (Cloak & Dagger) — Complete Contract Breakdown

> Reference document. **Generated 2026-06-12; fully resynced 2026-06-24** against the current
> contract source (`src/contracts/CLKnDGR.h`),
> `Cloak-and-Dagger-README.md`, and `CLOAKandDAGGER-ABI.md`.
>
> **Source of truth:** Where the README and ABI disagree (notably the profit-split preset
> percentages), the **contract source wins**. The preset table below uses the source values.
> Container capacities shown (1024 / 8192 / 4096 / 16 / 512) are the power-of-2 sizes set during
> the QPI refactor; the *logical* limits (676 voters, 5,000 depositors, 500 waitlist,
> 10 proposals) are unchanged.

---

## 1. What it is

An on-chain **arbitrage liquidity protocol** on Qubic (**contract index 29**). It pools QU from
two participant groups, runs two automated trading strategies every tick against **QX** (order-book
DEX, index 1) and **Qswap** (AMM, index 13), and distributes profit each epoch. Governance is fully
on-chain via `qubic-cli` — no website, no admin keys, no owner override (the dev fund is the only
escape hatch, and it is governance-controlled).

---

## 2. Two participant classes

|  | Shareholders | Vault Depositors |
|---|---|---|
| Token | IPO governance shares (max 676) | Vault shares (NAV-priced) |
| Power | Propose + vote on changes | Deposit trading capital; **veto** shareholder proposals |
| Commitment | None | 26-epoch lock; early-exit penalty |
| Reward | Epoch dividend | NAV growth |

Roles are separate and non-exclusive — one wallet can be both.

---

## 3. Trading strategies

Both run every tick, in order, sharing the same pool registry and capital base.

### 🥷 The Cloak — swing trading (DCA-in / DCA-out, on Qswap)
- **Entry (buy the dip):** a pool's 1-week avg price ≤ **70% of its 3-month avg** (a **30% dip**, the
  default; governable **5–30%** via Type 27). Opens with the **position-sizing preset** (default:
  **1%** of trading capital first buy, **0.25%** DCA-in adds, up to a **5%-of-capital cap**; the whole
  bundle is governable via Type 26 — presets 1/2/3 raise the cap to 7.5%/10%/15%). Buys use up to 5% slippage.
- **Exit (sell the rally):** pool price ≥ **106% of average cost** (a **6% gain**, the default;
  governable **6–30%** via Type 28). Sells **50% of the position** per trigger (default; governable
  10–50% via Type 21) via a QX ask priced 10% below market to guarantee fills.
- **Stop-loss (cut a loser):** if a bag falls **≥45% below average cost** (default; governable
  0=off / 15–90% via Type 24), it sells **60%** of the bag (default; Type 25) on Qswap, splitting the
  salvage **90% back to trading capital / 10% burned to the execution-fee reserve** (hardcoded).
- **Liquidity check (exit):** needs ≥80% of target proceeds available as QX bids, else defers.
- **Cadence:** ~**once a month** (30-day cooldown) per pool. Idle pools decide the dip from free stored
  price history; the live market price is read only for pools actually held. One position per pool.

### 🗡️ The Dagger — spot arbitrage (QX ↔ Qswap), VIX-gated
- **Bidirectional**, each in a single tick: **Direction B** = buy cheap on QX best ask → sell on Qswap;
  **Direction A** = buy cheap on Qswap → sell into QX best bid. The two have **separate** cooldowns.
- **Profit floor:** only fires if estimated net profit > `minProfitQu` (governable, default 100,100).
- **VIX gate (cost control):** a cheap per-pool volatility index, sampled **1×/day** (governable 1–3),
  decides *when* to scan — the Dagger hunts every **5 minutes** during a volatility breakout and sleeps
  to a **2-week** safety scan when the token is calm. Breakout = fast (~5-day) volatility ≥ slow
  (~4-week) baseline × factor (default 2×) **and** ≥ an absolute floor (default 25 bps).
- **Reserve accumulation:** keeps **10%** (`RESERVE_PCT`) of acquired tokens per pool as a reserve,
  sold later when profitable (above `minReserveProfitPct`).
- **Capital scaling:** large gaps are sized *down* so one pool can't consume all capital in a tick.

### Recovery system
Both strategies move share-management rights between the contract, QX, and Qswap. If a transfer
fails mid-trade, a recovery pass at the **start of every tick** silently retries until it succeeds.
Tokens are never lost; recovered tokens return to position or fold into the pool reserve depending
on where they got stuck (Cloak and Dagger keep separate recovery state).

---

## 4. Lifecycle — the 4 system procedures (network-called, not users)

| Procedure | When | What it does |
|---|---|---|
| `INITIALIZE` | Once, post-IPO | Sets `owner`, all initial parameters, `selfAsset` (share identity), initial vault share price |
| `BEGIN_EPOCH` | Each epoch start | Updates vault NAV (excludes all payout slices), auto-pays lock-expired depositors, promotes waitlist, reserve burn/sell every 4 epochs, samples swing prices, resets governance state |
| `BEGIN_TICK` | Each tick (~4/sec) | Recovery pass → **VIX sampler** → **Cloak** → **Dagger** (Dir B, then Dir A) |
| `END_EPOCH` | Each epoch end | Tallies votes, applies depositor vetoes, executes passed proposals, splits epoch profit, issues fee refunds |

---

## 5. The 9 user procedures (write — require a seed)

| ID | Procedure | Sends | Purpose |
|---|---|---|---|
| 1 | `SubmitProposal` | 50M–200M QU | Submit a governance proposal (one of **28** types) |
| 2 | `VoteOnProposal` | — | Shareholder votes Yes/No on a proposal slot (0–9) |
| 3 | `VaultDeposit` | deposit amount | Join the vault (or get waitlisted if full) |
| 4 | `DepositorVeto` | — | Depositor casts a veto (NO) vote |
| 5 | `WaitlistWithdraw` | — | Cancel a waitlist entry, reclaim queued QU |
| 6 | `VaultWithdraw` | — | Exit vault position (fees/penalty apply) |
| 7 | `VaultRelock` | additional QU | Reset to a fresh 26-epoch lock (only within final 4 epochs) |
| 8 | `DonateToContract` | **any positive amount** | Donation → **100% burned to the execution-fee reserve** |
| 9 | `PublicDonate` | **any positive amount** | Identical to DonateToContract — **100% to the execution-fee reserve** |

*(The ABI procedure table lists only the first 7; the source also registers the two donation
procedures as IDs 8 and 9. Both now accept any amount and route 100% to the fee reserve — the old
fixed amounts / trading-vs-Qearn splits were removed.)*

---

## 6. The 5 read-only functions (no seed)

| ID | Function | Returns |
|---|---|---|
| 1 | `GetStats` | totalArbs, totalProfit, quBalance, quReserve, qearnReserve, poolCount |
| 2 | `GetPool` | asset name, issuer, active flag for a pool |
| 3 | `GetProposal` | full proposal state + vote counts + depositor veto counts for a slot |
| 4 | `GetGovernanceParams` | all 21 governable params (fees, quorum, payout preset, vault thresholds, VIX, swing/stop-loss knobs, QX-fee mode) |
| 5 | `GetWaitlistPosition` | caller's waitlist rank, queued amount, total waiting, isFull |

---

## 7. Governance — the 28 proposal types

**A proposal passes only if ALL four hold:**
1. ≥15 unique qualified voters (governable, range 15–676)
2. ≥222 total weighted shares voted (yes+no)
3. ≥2/3 of weighted votes are Yes (supermajority)
4. Fewer than **500** qualifying depositor NO votes (veto)

**Fee handling:** every proposal fee funds the **execution-fee reserve** — none is burned from supply,
none is kept as trading capital. The non-refundable **31%** goes to the reserve at submission; on a
**PASS** the held **69%** is also routed to the reserve; on **FAIL/VETO** that 69% is **refunded** to
the proposer at epoch end.

| # | Type | Fee (QU) | Changes |
|---|---|---|---|
| 1 | ADD_POOL | 200M | Register a new token/QU pool (max 255) |
| 2 | REMOVE_POOL | 50M | Deactivate a pool (records + reserves retained) |
| 3 | REACTIVATE_POOL | 50M | Re-enable a deactivated pool |
| 4 | UPDATE_MIN_PROFIT | 50M | Dagger profit floor `minProfitQu` (also gates Cloak buys); 100,100 / 250,100 / 420,000 / 676,420 |
| 5 | WITHDRAW_QU_RESERVE | 50M | Move QU from dev fund to an address (≤ current reserve) |
| 6 | UPDATE_PROPOSAL_FEE | 50M | Default proposal fee |
| 7 | UPDATE_PAYOUT | 69M | Switch profit-split preset (0/1/2/**3 = recovery**) |
| 8 | UPDATE_FEE_ADD_POOL | 50M | ADD_POOL fee |
| 9 | UPDATE_FEE_PAYOUT | 50M | UPDATE_PAYOUT fee |
| 10 | UPDATE_MIN_QUORUM | 50M | Min unique voters (15–676) |
| 11 | WITHDRAW_ASSET_RESERVE | 50M | Move a deactivated pool's token reserve to an address |
| 12 | UPDATE_VAULT_TIER | 50M | Min deposit tier 0–8 (10K → 10M QU at initial share price) |
| 13 | UPDATE_RESERVE_PROFIT_PCT | 50M | Reserve-sell profit threshold (2/5/7/10%) |
| 14 | UPDATE_DEPOSITOR_VOTE_MIN | 50M | Min locked QU to veto (50M/150M/250M/350M) |
| 15 | UPDATE_RELOCK_AMOUNT | 50M | Min QU to relock (1M/5M/10M/20M/25M/50M) |
| 16 | UPDATE_EXEC_RESERVE_FLOOR | 50M | Fee-reserve safety floor; below it, 100% of profit → reserve (recovery) until refilled. 0 (off) / 1B / 5B / 10B / 20B |
| 17 | SELL_POOL_TOKENS | 50M | Market-sell 1–100% of a pool's tokens on Qswap → proceeds folded into the vault (benefits depositors + shareholders) |
| 18 | UPDATE_VIX_FACTOR | 50M | Dagger VIX breakout sensitivity ×100 (default 200 = 2×); 9/18/37/75/150/200/225/275/350/450/500 |
| 19 | UPDATE_VIX_FLOOR | 50M | VIX absolute floor in bps (default 25); 0/10/25/50/100/200 |
| 20 | UPDATE_VIX_PULSE_RATE | 50M | VIX samples per day (default 1); 1/2/3 |
| 21 | UPDATE_SWING_SELL_PCT | 50M | Cloak sell chunk per rally trigger (default 50%); 10/15/20/25/33/50 |
| 22 | UPDATE_BREAKOUT_RESCAN | 50M | Dagger hot re-scan pace, seconds (default 300); 30/60/120/180/240/300 |
| 23 | UPDATE_QX_FEE_MODE | 50M | **One-way** latch to fetch QX's fee live per trade (newValue must be 1); default 0 = cached |
| 24 | UPDATE_STOP_LOSS_TRIGGER | 50M | Cloak stop-loss depth, % below avg cost (default 45; **0 = off**); 0/15/30/45/60/75/90 |
| 25 | UPDATE_STOP_LOSS_SELL | 50M | % of a losing bag sold per stop-loss trigger (default 60); 15/30/45/60/75/90 |
| 26 | UPDATE_SWING_SIZING | 50M | Cloak position-sizing preset (first-buy/DCA-add/cap bundle): **0**=1%/0.25%/5% · 1=1%/0.25%/7.5% · 2=2%/0.50%/10% · 3=3%/0.25%/15% |
| 27 | UPDATE_SWING_DIP | 50M | Cloak buy-dip threshold, % below 3-month avg (default 30); 5/10/15/20/25/30 |
| 28 | UPDATE_SWING_RALLY | 50M | Cloak rally-sell threshold, % above cost (default 6); 6/12/18/24/30 |

---

## 8. Profit-split presets (SOURCE-authoritative)

Each preset allocates 100% of epoch profit; **trading %** is the implicit remainder (reinvested as
vault capital → benefits depositors via NAV).

| Preset | Trading | Exec fees | Qearn | Shareholders | Dev fund | CCF |
|---|---|---|---|---|---|---|
| **0 — Default** | 55% | 30% | 3% | 10% | 1% | 1% |
| **1** | 61% | 27% | 3% | 7% | 1% | 1% |
| **2** | 65% | 25% | 3% | 5% | 1% | 1% |
| **3 — Recovery / limp** | 0% | **100%** | 0% | 0% | 0% | 0% |

Preset 3 (recovery) auto-engages when the execution-fee reserve falls below `execReserveFloor`, or is
selectable manually via UPDATE_PAYOUT. In it the contract **keeps trading normally** but routes 100%
of profit to the fee reserve until it refills to 10% above the floor (hysteresis latch). The old
deflationary **burn** bucket has been removed (it was 0% in every preset).

---

## 9. The vault (deposit mechanics)

- **Deposit:** QU → shares at current NAV. If vault is full (5,000), you join the waitlist
  (max 500, served largest-first; a larger offer displaces the smallest queued entry).
- **Lock:** 26 epochs. Relock within the final 4 epochs (resets to a fresh 26-epoch lock; requires
  adding ≥ `relockAddAmount`, default 10M QU).
- **Withdraw:** 2% management fee always; 5% performance fee on profit; +38% penalty if early
  (40% total early-exit cost). The penalty/performance fee flows into `epochProfit` for distribution.
- **Veto:** depositors with locked QU ≥ `depositorVoteMinQu` (default 150M) can cast veto votes;
  re-validated against current NAV at epoch end. **500** qualifying NO votes block a passed proposal.

---

## 10. Key constants

**Trading:** `MIN_PROFIT_QU` 100,100 (governance options 100,100 / 250,100 / 420,000 / 676,420) ·
`RESERVE_PCT` 10% · reserve action every 4 epochs · Dagger cooldown is **VIX-driven** (≈5 min while
breaking out, ≈2 weeks baseline when calm).

**Vault:** 26-epoch lock · 4-epoch relock window · 2% mgmt fee · 5% perf fee · 38% early-exit
penalty · 8 deposit tiers (1–1,000 min shares) · initial share price 10,000 QU.

**Cloak:** 13 price slots (~3 months) · **buy dip 30%** (default) · **sell gain 6%** (default) ·
**sell 50%/trigger** (default) · sizing preset **1% first / 0.25% add / 5% cap** (default) ·
**stop-loss 45% trigger / 60% cut** (default; 90/10 capital/exec split) · 80% liquidity required ·
10% ask discount · 5% buy slippage · ~30-day (monthly) cooldown.

**VIX (Dagger gate):** sample 1×/day (default) · fast ~5-day / slow ~4-week volatility EWMAs ·
breakout factor 2× (default) · absolute floor 25 bps (default) · hunt ≈5 min / baseline ≈2 weeks.

**Governance:** 676 voter capacity · 15 initial min quorum · 222 min shares quorum · **500 depositor
veto threshold** · 10 proposals/epoch max · 5,000 max depositors · 500 waitlist size · proposal fee
split 31% reserve (non-refundable) / 69% reserve-on-pass-or-refund-on-fail.

---

## 11. Full state-variable reference (`StateData`)

### Identity & pools
| Variable | Type | Meaning |
|---|---|---|
| `owner` | id | Deployer (set in INITIALIZE; **DEP1** placeholder pending) |
| `poolAssetNames` / `poolIssuers` / `poolActive` | Array[256] | Per-pool asset name / issuer id / active flag |
| `poolCount` | uint8 | Registered pools (active + inactive) |

### Arbitrage accounting
| Variable | Type | Meaning |
|---|---|---|
| `totalArbsExecuted` | uint64 | Cumulative completed arbs |
| `totalProfitEarned` | sint64 | Cumulative QU profit |
| `quBalance` | sint64 | Cached on-chain balance |
| `epochProfit` | sint64 | Profit accumulated this epoch (pre-split) |
| `reserveSellProceeds` | sint64 | Reserve-sell QU this epoch (excluded from NAV until split) |
| `poolReserveTokens` / `poolReserveCostBasis` | Array[256] | Per-pool reserve token count + QU cost |
| `epochCounter` | uint8 | Counts epochs; triggers reserve action every 4 |
| `quReserve` | sint64 | **Dev fund** (governance-withdrawn) |
| `qearnReserve` | sint64 | Qearn accumulator |
| `qxTransferFee` | uint32 | Cached QX share-transfer fee (refreshed per epoch; live mode via Type 23) |
| `poolCooldownTick` / `poolCooldownTickA` | Array[256] | Per-pool re-check tick — **Dir B** / **Dir A** (separate, VIX-driven) |
| `poolPendingRecoveryAmt` / `Source` / `CostBasis` | Array[256] | Dagger TSRM-failure recovery state |

### Governance
| Variable | Type | Meaning |
|---|---|---|
| `selfAsset` | Asset | CLKnDGR's own share identity (**selfAsset** task pending) |
| `minProfitQu` | sint64 | Dagger profit floor |
| `proposalFeeDefault` / `AddPool` / `PayoutStructure` | sint64 | The three fee tiers |
| `payoutStructure` | uint8 | Active preset **0/1/2/3** |
| `inLimpMode` | uint8 | 1 = in recovery/limp mode (hysteresis latch) |
| `execReserveFloor` | sint64 | Fee-reserve safety floor that triggers recovery |
| `minVoterQuorum` | uint16 | Min unique voters |
| `proposalsThisEpoch` | uint8 | Proposals submitted this epoch |
| `proposals` | Array[16] | This epoch's proposal buffer (logical max 10) |
| `proposalVoterMap` / `voterYesChoiceMap` | HashMap[1024] | Per-voter bitmask of slots voted / voted-Yes |
| `voterList` / `voterCount` | Array[1024] / uint16 | Unique voters this epoch (re-verified at END_EPOCH) |

### Vault
| Variable | Type | Meaning |
|---|---|---|
| `vaultSharePrice` | sint64 | NAV per share |
| `totalVaultShares` | sint64 | Shares outstanding |
| `totalDepositorPool` | sint64 | Depositor-allocated QU (NAV-tracked) |
| `prevTradingBalance` | sint64 | Prior-epoch post-split balance (for the NAV ratio) |
| `vaultDepositTier` | uint8 | Min-deposit tier 0–8 |
| `minReserveProfitPct` | uint8 | Reserve-sell threshold |
| `depositorCount` | uint16 | Active depositors |
| `depositorVoteMinQu` | sint64 | Min locked QU to veto |
| `relockAddAmount` | sint64 | Min QU to relock |
| `depositorInfo` | HashMap[8192] | **One atomic record per wallet** — `{shares, costBasis, epoch}` (formerly three parallel maps) |
| `depositorList` | Array[8192] | Ordered active depositors |

### Depositor veto
| Variable | Type | Meaning |
|---|---|---|
| `depositorVoteMap` | HashMap[8192] | Per-depositor slots-voted bitmask |
| `depositorNoVotes` / `depositorYesVotes` | Array[16] | Per-slot NO (veto) / YES counts |

### Waitlist
| Variable | Type | Meaning |
|---|---|---|
| `waitlist` | Array[512] | Queued wallets (sorted largest-first) |
| `waitlistCount` / `waitlistQu` | uint16 / sint64 | Count + total queued QU (excluded from NAV) |

### Cloak / swing
| Variable | Type | Meaning |
|---|---|---|
| `swingPriceHistory` | Array[4096] | Flattened per-pool price history (256 × 13) |
| `swingPriceHead` / `swingPriceCount` | Array[256] | Rolling write pointer / samples taken |
| `swingTokens` / `swingCostBasis` | Array[256] | Per-pool swing position + cost |
| `swingCooldownTick` | Array[256] | Per-pool swing cooldown (~monthly) |
| `swingSellPct` | sint64 | Cloak sell chunk % (Type 21) |
| `swingSizingPreset` | uint8 | Position-sizing preset 0–3 (Type 26) |
| `swingBuyDipPct` | sint64 | Buy-dip threshold % (Type 27) |
| `swingSellGainPct` | sint64 | Rally-sell threshold % (Type 28) |
| `stopLossTriggerPct` / `stopLossSellPct` | sint64 | Stop-loss depth (Type 24) / cut size (Type 25) |
| `swingPendingRecoveryAmt` / `Cost` / `Source` | Array[256] | Cloak TSRM-failure recovery state |

### VIX (Dagger volatility gate)
| Variable | Type | Meaning |
|---|---|---|
| `vixFast` / `vixSlow` / `vixLastPrice` | Array[256] | Per-pool fast/slow volatility EWMAs + last sampled price |
| `vixLastSampleTick` / `vixSampleCount` / `vixBreakout` | Array[256] | Last sample tick / samples taken / breakout flag |
| `vixBreakoutFactor` / `vixAbsFloorBps` / `vixSampleInterval` | sint64 / sint64 / uint32 | Breakout sensitivity (Type 18) / floor bps (Type 19) / sample interval (Type 20) |
| `breakoutRescanTicks` | uint32 | Dagger hot re-scan pace (Type 22) |
| `qxFeeLivePerTrade` | uint8 | QX-fee mode latch (Type 23) |

---

## 12. Pending pre-deployment tasks
- **DEP1:** replace `id(1,0,0,0)` placeholder in `INITIALIZE` with the real deployer public key.
- **selfAsset:** confirm the hardcoded asset name `23159306787179587` (= `assetNameFromString("CLKNDGR")`) matches the IPO share registration.
- **Construction epoch / tick rate:** confirm before the IPO epoch (tick-rate assumption = 4/sec).
- Build + full test suite: **passing** (grafted into the June-23 core; logic-audited, taint-audited,
  fuzz-tested). The GitHub re-upload of `CLKnDGR.h` + `contract_clkndgr.cpp` is owner-side.
