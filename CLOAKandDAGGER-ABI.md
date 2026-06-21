# Cloak and Dagger (CLKnDGR) — On-Chain Governance ABI Reference

This document describes how to interact with Cloak and Dagger (CLKnDGR) directly using `qubic-cli`. It covers
the governance system (shareholder proposals and voting), the depositor vault (deposit, relock,
withdraw), the depositor veto mechanism, and contract donations. It is intended for Cloak and Dagger shareholders,
vault depositors, and community tool builders.

**Contract index:** TBD at deployment  
**Source code:** https://github.com/[TBD]/core (see `src/contracts/CLKnDGR.h`)

---

## Overview

Cloak and Dagger exposes nine public **procedures** (require a seed, write to state) and five
public **functions** (read-only, no seed required).

### Procedures

| Procedure | ID | Fee / Amount sent | Seed required |
|---|---|---|---|
| `SubmitProposal` | 1 | Yes — 50M–200M QU (by proposal type) | Yes |
| `VoteOnProposal` | 2 | None | Yes |
| `VaultDeposit` | 3 | The QU you want to deposit (invocationReward) | Yes |
| `DepositorVeto` | 4 | None | Yes |
| `WaitlistWithdraw` | 5 | None | Yes |
| `VaultWithdraw` | 6 | None | Yes |
| `VaultRelock` | 7 | Additional QU to add to your position (invocationReward) | Yes |
| `DonateToContract` | 8 | Any amount (invocationReward) → execution-fee reserve | Yes |
| `PublicDonate` | 9 | Any amount (invocationReward) → execution-fee reserve | Yes |

### Functions (read-only)

| Function | ID | Purpose |
|---|---|---|
| `GetStats` | 1 | Total arbs, profit, QU balance, reserve amounts, pool count |
| `GetPool` | 2 | Asset name, issuer, active status for a pool |
| `GetProposal` | 3 | Full proposal state + depositor vote counts for a given slot (0–9) |
| `GetGovernanceParams` | 4 | Current fee levels, payout preset, quorum settings, vault thresholds |
| `GetWaitlistPosition` | 5 | Your waitlist rank and queue status |

> **Two participant classes:** Cloak and Dagger *shareholders* hold IPO governance tokens (up to 676
> total) and vote on proposals. Vault *depositors* lock QU as trading capital and can veto
> shareholder proposals. These are separate and non-exclusive — one wallet can hold both.

**Security notice:** Any tool that asks for your seed phrase should be treated with suspicion.
Use the lookup tables in this document — no tool is required for voting or vetoing.
For submitting proposals, only use tools whose source code is publicly available and auditable.

---

## qubic-cli command structure

All commands follow this structure:

```
./qubic-cli [config flags] -<command> [command arguments]
```

**Common config flags:**

| Flag | Description |
|---|---|
| `-nodeip <IP>` | IP address of the Qubic node to connect to |
| `-nodeport <PORT>` | Port of the node (default: 21841) |
| `-seed <SEED>` | Your 55-character seed (required for procedures only) |
| `-scheduletick <OFFSET>` | Tick offset for scheduling the transaction (default: 20; at 4 ticks/second, 20 ticks ≈ 5 seconds) |

**Commands used in this document:**

| Command | Purpose |
|---|---|
| `-invokecontractprocedure <CONTRACT_INDEX> <PROCEDURE_ID> <AMOUNT> "<INPUT_FORMAT>"` | Call a write procedure (requires seed) |
| `-callcontractfunction <CONTRACT_INDEX> <FUNCTION_ID> "<INPUT_FORMAT>" "<OUTPUT_FORMAT>"` | Call a read-only function (no seed) |

### Format string syntax

qubic-cli uses **format strings** to encode input and decode output rather than raw hex.
Each field is written as `<value><type>`, separated by commas:

```
"<value>uint8, <value>uint64, <identity_string>id, ..."
```

- `uint8`, `uint16`, `uint32`, `uint64`, `sint64` etc. — numeric types with the value prepended
- `id` — a Qubic identity string (60-character base-26) with `id` appended directly
- **NULL_ID** (zero identity) = `AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid`

Example — `uint8` value 3, `uint64` value 50000000:
```
"3uint8,50000000uint64"
```

---

## Section 1 — Voting (Shareholder, Direct Lookup Table)

Shareholder voting requires no encoding tool. Find your slot number by querying `GetProposal`
on-chain (see Section 3), then copy the matching command. **No tool ever touches your seed.**

### Step 1 — Find active proposals

Query each slot 0–9 to see what proposals are active this epoch:

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 3 "SLOT_NUMBERuint8" \
  "uint8,uint8,id,uint64,id,uint64,sint64,sint64,id,sint64,sint64,sint64,uint16,uint16"
```

Replace `SLOT_NUMBER` with `0` through `9`. A proposal is active when the second field
(`status`) returns `1`.

**Output fields in order:** proposalType, status, proposer, assetName, assetIssuer, poolIndex,
newValue, withdrawAmount, destination, votesYes, votesNo, feePaid,
**depositorVotesNo**, **depositorVotesYes**

> The last two fields are depositor veto votes. `depositorVotesNo` reaching **125** will
> block the proposal at epoch end even if shareholders passed it.

### Step 2 — Cast your vote

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 2 0 "SLOT_NUMBERuint8,VOTE_VALUEuint8"
```

Replace `SLOT_NUMBER` with the slot you are voting on, and `VOTE_VALUE` with `1` for Yes
or `0` for No.

### Complete lookup table

> **How to use:** Find your slot and vote. Copy the INPUT_FORMAT string.
> Paste it into the command above as the final argument.
> Replace `NODE_IP`, `NODE_PORT`, `CONTRACT_INDEX`, and `YOURSEED` with your own values.
> Run it yourself — never hand your seed to any third-party tool.

| Slot | Vote | INPUT_FORMAT string |
|------|------|---------------------|
| 0    | Yes  | `"0uint8,1uint8"`   |
| 0    | No   | `"0uint8,0uint8"`   |
| 1    | Yes  | `"1uint8,1uint8"`   |
| 1    | No   | `"1uint8,0uint8"`   |
| 2    | Yes  | `"2uint8,1uint8"`   |
| 2    | No   | `"2uint8,0uint8"`   |
| 3    | Yes  | `"3uint8,1uint8"`   |
| 3    | No   | `"3uint8,0uint8"`   |
| 4    | Yes  | `"4uint8,1uint8"`   |
| 4    | No   | `"4uint8,0uint8"`   |
| 5    | Yes  | `"5uint8,1uint8"`   |
| 5    | No   | `"5uint8,0uint8"`   |
| 6    | Yes  | `"6uint8,1uint8"`   |
| 6    | No   | `"6uint8,0uint8"`   |
| 7    | Yes  | `"7uint8,1uint8"`   |
| 7    | No   | `"7uint8,0uint8"`   |
| 8    | Yes  | `"8uint8,1uint8"`   |
| 8    | No   | `"8uint8,0uint8"`   |
| 9    | Yes  | `"9uint8,1uint8"`   |
| 9    | No   | `"9uint8,0uint8"`   |

---

## Section 2 — Submitting Proposals (For Tool Builders)

This section describes the `SubmitProposal_input` struct and format strings so that
community tools can encode proposals correctly.

### Governance requirements

A proposal will be rejected at submission time if any of the following are not met:

- The caller holds **≥1 CLKnDGR share**
- The `AMOUNT` passed to `-invokecontractprocedure` meets the required fee (see below)
- A slot is available (max 10 proposals per epoch)
- No conflicting active proposal of the same type already exists this epoch

### Proposal fees

| Proposal type | AMOUNT (QU) |
|---|---|
| `ADD_POOL` (type 1) | 200,000,000 |
| `UPDATE_PAYOUT` (type 7) | 69,000,000 |
| All other types | 50,000,000 |

**Fee handling:** every proposal fee funds the contract's **execution-fee reserve** — nothing is
burned from supply and nothing is kept as trading capital. The non-refundable **31%** is routed to
the reserve at submission; the remaining **69%** is held by the contract. If the proposal **passes**,
that 69% is also sent to the reserve (**100% of the fee funds execution**). If it **does not pass**
(failed quorum, failed supermajority, or depositor veto), the **69% is refunded** to the proposer at
epoch end (so the reserve keeps only the 31%).

### SubmitProposal_input struct

```cpp
struct SubmitProposal_input
{
    uint8  proposalType;    // proposal type constant (1–23)
    uint64 assetName;       // ADD_POOL only; 0 otherwise
    id     assetIssuer;     // ADD_POOL only; NULL_ID otherwise
    uint64 poolIndex;       // pool-indexed types only; 0 otherwise
    sint64 newValue;        // value-update types only; 0 otherwise
    sint64 withdrawAmount;  // WITHDRAW_QU_RESERVE only; 0 otherwise
    id     destination;     // WITHDRAW_QU_RESERVE / WITHDRAW_ASSET_RESERVE only; NULL_ID otherwise
};
```

**NULL_ID in format strings:**
```
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid
```

### Base format string template

Every SubmitProposal call uses this field order:

```
"<proposalType>uint8, <assetName>uint64, <assetIssuer>id, <poolIndex>uint64, <newValue>sint64, <withdrawAmount>sint64, <destination>id"
```

Unused fields are set to `0` (numeric) or NULL_ID (`AAAA...id`).

### Asset name encoding

Qubic asset names are uppercase ASCII strings packed into a `uint64` with the first
character in the lowest byte (little-endian), zero-padded to 8 bytes. Pass the resulting
decimal integer as the `assetName` value in the format string.

Example — `QWATZ`:
```
Q=0x51  W=0x57  A=0x41  T=0x54  Z=0x5A
→ little-endian bytes: 51 57 41 54 5A 00 00 00
→ decimal uint64: 97786616701264977 (use this value in the format string)
```

### Full command template

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 1 AMOUNT "FORMAT_STRING"
```

### Proposal type reference

---

#### Type 1 — ADD_POOL

Add a new asset pool to the Cloak and Dagger arbitrage system.

**Fee:** 200,000,000 QU

**Validation:** Pool must not already exist (active or inactive). Max 255 pools total.

**Format string:**
```
"1uint8, <assetName>uint64, <assetIssuer>id, 0uint64, 0sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 2 — REMOVE_POOL

Deactivate an active pool (stops arbitrage on that asset).

**Fee:** 50,000,000 QU

**Validation:** Pool must currently be active.

**Format string:**
```
"2uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, <poolIndex>uint64, 0sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 3 — REACTIVATE_POOL

Re-enable a previously deactivated pool.

**Fee:** 50,000,000 QU

**Validation:** Pool must currently be inactive.

**Format string:**
```
"3uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, <poolIndex>uint64, 0sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 4 — UPDATE_MIN_PROFIT

Change the minimum **net** profit per arb (in QU) required before a trade executes. "Net" means after the
Qswap swap fee — the gates and slippage guards add the fee back internally, so this value is what the
contract keeps per arb.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be one of the allowlisted values — `100100`, `250100`, `420000`, or
`676420` (default `100100`). Off-allowlist values are rejected. The allowlist prevents settings that
mechanically break the Dagger (too low) or render it permanently inactive (too high).

**Format string:**
```
"4uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 5 — WITHDRAW_QU_RESERVE

Transfer QU from the contract's reserve fund to a destination address.

**Fee:** 50,000,000 QU

**Validation:** `withdrawAmount` must be > 0 and ≤ current `quReserve`. Destination must not be NULL_ID.

**Format string:**
```
"5uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, 0sint64, <withdrawAmount>sint64, <destination>id"
```

---

#### Type 6 — UPDATE_PROPOSAL_FEE

Change the default proposal submission fee (applies to all types except ADD_POOL and UPDATE_PAYOUT).

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be > 0.

**Format string:**
```
"6uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 7 — UPDATE_PAYOUT

Switch the epoch profit distribution preset.

**Fee:** 69,000,000 QU

**Validation:** `newValue` must be 0, 1, 2, or 3.

| newValue | Trading | Exec fees | Qearn | Shareholders | Dev fund | CCF |
|---|---|---|---|---|---|---|
| 0 (default) | 55% | 30% | 3% | 10% | 1% | 1% |
| 1 | 61% | 27% | 3% | 7% | 1% | 1% |
| 2 | 65% | 25% | 3% | 5% | 1% | 1% |
| 3 (recovery) | 0% | 100% | 0% | 0% | 0% | 0% |

Preset 3 is the **recovery / limp-mode** split: 100% of profit refills the execution-fee reserve and all
other payouts are suspended, while the contract keeps trading normally. It is **auto-applied** whenever
the fee reserve drops below `execReserveFloor` (see Type 16), and can also be selected manually here.

**Format string:**
```
"7uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <0|1|2|3>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 8 — UPDATE_FEE_ADD_POOL

Change the submission fee for ADD_POOL proposals specifically.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be > 0.

**Format string:**
```
"8uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 9 — UPDATE_FEE_PAYOUT

Change the submission fee for UPDATE_PAYOUT proposals specifically.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be > 0.

**Format string:**
```
"9uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 10 — UPDATE_MIN_QUORUM

Change the minimum number of unique qualified voters required for consensus.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be between 15 and 676 (inclusive).

**Format string:**
```
"10uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 11 — WITHDRAW_ASSET_RESERVE

Transfer token reserve holdings from a deactivated pool to a destination address.

**Fee:** 50,000,000 QU

**Validation:** Pool must be inactive. Destination must not be NULL_ID.

**Format string:**
```
"11uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, <poolIndex>uint64, 0sint64, 0sint64, <destination>id"
```

---

#### Type 12 — UPDATE_VAULT_TIER

Change the minimum deposit size for the vault (expressed as a tier index, not a QU amount).
The effective minimum in QU is `minShares × currentSharePrice`.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be 0–8.

| newValue | Min shares | Effective minimum at initial 10,000 QU/share |
|---|---|---|
| 0 | 1 | 10,000 QU |
| 1 | 5 | 50,000 QU |
| 2 | 10 | 100,000 QU |
| 3 | 25 | 250,000 QU |
| 4 | 50 | 500,000 QU |
| 5 | 100 | 1,000,000 QU |
| 6 | 250 | 2,500,000 QU |
| 7 | 500 | 5,000,000 QU |
| 8 (default) | 1,000 | 10,000,000 QU |

**Format string:**
```
"12uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <0-8>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 13 — UPDATE_RESERVE_PROFIT_PCT

Change the minimum reserve profit percentage required before the contract sells reserve tokens.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be 2, 5, 7, or 10.

**Format string:**
```
"13uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <2|5|7|10>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 14 — UPDATE_DEPOSITOR_VOTE_MIN

Change the minimum locked QU required for a vault depositor's vote to qualify for the
veto mechanism. Depositors below this threshold cannot cast qualifying veto votes.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 50,000,000 / 150,000,000 / 250,000,000 / 350,000,000.

| newValue | Min locked QU |
|---|---|
| 50000000 | 50M QU |
| 150000000 (default) | 150M QU |
| 250000000 | 250M QU |
| 350000000 | 350M QU |

**Format string:**
```
"14uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 15 — UPDATE_RELOCK_AMOUNT

Change the minimum additional QU required when a depositor re-locks their vault position
(extends their 26-epoch lock within the final 4 epochs by adding more QU).

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 1,000,000 / 5,000,000 / 10,000,000 / 20,000,000 / 25,000,000 / 50,000,000.

| newValue | Minimum to re-lock |
|---|---|
| 1000000 | 1M QU |
| 5000000 | 5M QU |
| 10000000 (default) | 10M QU |
| 20000000 | 20M QU |
| 25000000 | 25M QU |
| 50000000 | 50M QU |

**Format string:**
```
"15uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 16 — UPDATE_EXEC_RESERVE_FLOOR

Sets the **execution-fee-reserve safety floor**. When the contract's execution-fee reserve falls below
this value, the epoch profit split automatically switches to the recovery preset (3) — routing **100% of
profit** into the reserve and suspending the shareholder / Qearn / dev-fund / CCF payouts — while the
contract **keeps trading normally** to earn that profit. It returns to the chosen preset **once the
reserve climbs back to 10% above this floor** (a hysteresis buffer that prevents rapid on/off flapping).
This lets the contract rebuild its on-chain fee budget from its own earnings when network execution fees
rise. `0` disables the valve.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 0 / 1,000,000,000 / 5,000,000,000 / 10,000,000,000 / 20,000,000,000.

| newValue | Reserve floor |
|---|---|
| 0 (default) | Disabled — valve off (no auto-recovery) |
| 1000000000 | 1B QU |
| 5000000000 | 5B QU |
| 10000000000 | 10B QU |
| 20000000000 | 20B QU |

**Format string:**
```
"16uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 17 — SELL_POOL_TOKENS

Market-sells a shareholder-chosen **percentage** of a pool's token holdings on **Qswap** for QU.
Unlike `WITHDRAW_ASSET_RESERVE` (Type 11), which ships raw tokens to an external address (depositors
gain nothing), the QU proceeds stay in the contract and are folded into the next epoch's profit split —
so **both vault depositors** (via NAV) **and shareholders** (via the distribution slice) benefit. This
is the governance "take profit" / "exit a bag" button.

The sell takes `newValue`% from **both** the Cloak swing position and the pool's reserve bucket, so the
pool's accounting stays proportional. Proceeds route through the same path as the periodic reserve
liquidation (`reserveSellProceeds` → `epochProfit`, distributed the following epoch). A **10% slippage
floor** protects against a catastrophic fill: if the swap can't return at least 90% of the quoted QU,
it reverts and the tokens are recovered next tick rather than dumped.

**Fee:** 50,000,000 QU

**Validation:** `poolIndex` must reference a valid pool (active **or** inactive). `newValue` (the
percent) must be 1–100. (If a recovery is already in flight on the pool, the sell is skipped that epoch
and can be re-proposed.)

| newValue | Sells |
|---|---|
| 1–100 | That percent of the pool's swing + reserve token holdings |

**Format string:**
```
"17uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, <poolIndex>uint64, <percent 1-100>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 18 — UPDATE_VIX_FACTOR

Tunes the Dagger's **volatility-breakout sensitivity**. The Dagger only scans a pool for arbitrage when
that token's recent (~5-day) volatility rises above its own (~4-week) baseline by this multiplier. A
lower value makes the Dagger more eager (scans sooner, spends more execution fees); a higher value makes
it more selective. Stored ×100 (200 = 2.00×).

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 9, 18, 37, 75, 150, 200, 225, 275, 350, 450, 500 (= 0.09× to 5×).

| newValue | Breakout when recent vol ≥ | Note |
|---|---|---|
| 9 | 0.09× baseline | very eager — floor is the real gate |
| 18 | 0.18× baseline | |
| 37 | 0.37× baseline | |
| 75 | 0.75× baseline | |
| 150 | 1.5× baseline | |
| 200 (default) | 2.0× baseline | |
| 225 | 2.25× baseline | |
| 275 | 2.75× baseline | |
| 350 | 3.5× baseline | |
| 450 | 4.5× baseline | |
| 500 | 5.0× baseline | most selective |

**Format string:**
```
"18uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 19 — UPDATE_VIX_FLOOR

Sets the **minimum absolute volatility** (basis points of average price move per sample; sample period = the governable pulse rate, Type 20) for a
token to qualify as "breaking out." This stops a near-dead token — whose tiny absolute moves could be a
large *ratio* over an even tinier baseline — from waking the Dagger. `0` disables the floor (ratio-only gate).

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 0, 10, 25, 50, 100, 200.

| newValue | Minimum recent volatility |
|---|---|
| 0 | Disabled (ratio gate only) |
| 10 | 0.10% per sample |
| 25 (default) | 0.25% per sample |
| 50 | 0.50% per sample |
| 100 | 1.00% per sample |
| 200 | 2.00% per sample |

**Format string:**
```
"19uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 20 — UPDATE_VIX_PULSE_RATE

Sets **how many times per day** the VIX sampler reads each pool's price to update its volatility
reading. More pulses = sharper, faster detection of a token heating up, but more execution-fee cost;
fewer = cheaper but slower to react. The ≈5-day / ≈4-week volatility horizons stay fixed regardless —
only the sampling frequency changes (the averaging auto-scales with the rate).

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 1, 2, 3.

| newValue | Pulse rate | Interval |
|---|---|---|
| 1 (default) | once a day | every 24h |
| 2 | twice a day | every 12h |
| 3 | three times a day | every 8h |

**Format string:**
```
"20uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 21 — UPDATE_SWING_SELL_PCT

Sets the Cloak's **sell chunk** — what percentage of a held swing position it sells each time that
position hits its +12%-over-cost sell trigger. Higher = takes profit faster (less left riding); lower =
exits more gradually.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be exactly one of: 10, 15, 20, 25, 33, 50.

| newValue | Sells per trigger |
|---|---|
| 10 | 10% of the bag |
| 15 | 15% |
| 20 | 20% |
| 25 | 25% |
| 33 | 33% |
| 50 (default) | half the bag |

**Format string:**
```
"21uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 22 — UPDATE_BREAKOUT_RESCAN

Sets how often the Dagger re-checks a pool that is in a **volatility breakout** while hunting for a gap.
(It still trades every tick once an actual gap is found — this only paces the *looking* when a look comes
up empty.) Submitted in **seconds**; stored internally as ticks (× 4 ticks/sec).

**Fee:** 50,000,000 QU

**Validation:** `newValue` (seconds) must be exactly one of: 30, 60, 120, 180, 240, 300.

| newValue (seconds) | Re-scan pace |
|---|---|
| 30 | every 30 sec |
| 60 | every 1 min |
| 120 | every 2 min |
| 180 | every 3 min |
| 240 | every 4 min |
| 300 (default) | every 5 min |

**Format string:**
```
"22uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

#### Type 23 — UPDATE_QX_FEE_MODE

Sets how the contract sources QX's **share-transfer fee** for the sell leg of its trades (the fee paid when
handing QX managing rights via `releaseShares`). QX's fee is a fixed **100 QU** today — set once at QX's
init with no runtime setter — so the contract caches it per epoch and the sell legs read the cache,
avoiding a QX `Fees` lookup on every trade.

This proposal is **forward-looking insurance**: if QX ever changes its fee model so the fee can vary
tick-to-tick, shareholders set this to **1** and the Cloak + Dagger sell legs fetch the fee **live** before
each sell — **no contract re-deploy required**. Until then, leave it at **0**. A stale cache is harmless
either way: if it were ever too low, the sell simply doesn't execute (no loss); too high, a few QU overpaid.

**Fee:** 50,000,000 QU

**Validation:** `newValue` must be 0 or 1.

| newValue | QX fee source |
|---|---|
| 0 (default) | Per-epoch cached fee (cheapest) |
| 1 | Live QX fetch before every sell |

**Format string:**
```
"23uint8, 0uint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid, 0uint64, <newValue>sint64, 0sint64, AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAid"
```

---

### Consensus requirements

For a proposal to pass at epoch end, **all four** conditions must be met:

1. **≥15 unique qualified voters** must have voted (governable via UPDATE_MIN_QUORUM)
2. **≥222 total weighted shares** must have voted yes or no combined
3. **≥2/3 of weighted votes** must be yes (supermajority)
4. **Fewer than 125 qualifying depositor NO votes** (depositor veto; see Section 5)

If conditions 1–3 are met but condition 4 fails, the proposal is **vetoed** and the
proposer receives the standard 69% fee refund.

---

## Section 3 — Querying Contract State

### GetStats (Function ID 1)

Returns overall contract statistics. No input required.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 1 "" \
  "uint64,sint64,sint64,sint64,sint64,uint8"
```

**Output fields in order:**

| Field | Type | Description |
|---|---|---|
| `totalArbsExecuted` | uint64 | Cumulative number of completed arbitrage trades |
| `totalProfitEarned` | sint64 | Cumulative QU profit earned across all arbs |
| `quBalance` | sint64 | Total on-chain QU balance (includes all reserves) |
| `quReserve` | sint64 | QU earmarked as dev fund — not available for trading |
| `qearnReserve` | sint64 | QU accumulating toward the next donation to Qearn's bonus pool — not available for trading |
| `poolCount` | uint8 | Number of registered pools (active and inactive) |

> The actual capital available for trading is `quBalance - quReserve - qearnReserve`.

---

### GetPool (Function ID 2)

Returns one pool's asset name, issuer, and active status by index (0 .. poolCount − 1).

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 2 "POOL_INDEXuint64" \
  "uint64,id,uint8"
```

**Output fields in order:**

| Field | Type | Description |
|---|---|---|
| `assetName` | uint64 | The pool token's asset name (ticker encoded as uint64) |
| `assetIssuer` | id | The pool token's issuer address |
| `active` | uint8 | 1 if actively traded; 0 if paused/inactive (also 0 if the index is out of range) |

---

### GetProposal (Function ID 3)

Returns the full state of one governance proposal slot (0–9) for the current epoch, including depositor veto tallies.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 3 "SLOT_NUMBERuint8" \
  "uint8,uint8,id,uint64,id,uint64,sint64,sint64,id,sint64,sint64,sint64,uint16,uint16"
```

**Output fields in order:**

| Field | Type | Description |
|---|---|---|
| `proposalType` | uint8 | Proposal type 1–23 (0 if the slot is empty) |
| `status` | uint8 | 0 = empty, 1 = active (awaiting epoch-end tally), 2 = passed, 3 = failed |
| `proposer` | id | Address that submitted the proposal |
| `assetName` | uint64 | Target pool token name (ADD_POOL) |
| `assetIssuer` | id | Target pool token issuer (ADD_POOL) |
| `poolIndex` | uint64 | Target pool index (pool-indexed types) |
| `newValue` | sint64 | Proposed value (parameter-update types) |
| `withdrawAmount` | sint64 | QU amount (WITHDRAW_QU_RESERVE) |
| `destination` | id | Recipient address (withdraw / sweep types) |
| `votesYes` | sint64 | Total weighted YES shares (tallied at epoch end) |
| `votesNo` | sint64 | Total weighted NO shares (tallied at epoch end) |
| `feePaid` | sint64 | Proposal fee paid by the proposer |
| `depositorVotesNo` | uint16 | Qualifying depositor NO (veto) votes for this slot |
| `depositorVotesYes` | uint16 | Depositor YES votes for this slot |

---

### GetGovernanceParams (Function ID 4)

Returns all governable parameters in a single call. No input required.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 4 "" \
  "sint64,sint64,sint64,sint64,uint8,uint16,uint8,uint8,sint64,sint64,sint64,uint8,sint64,sint64,uint32,sint64,uint32,uint8"
```

**Output fields in order:**

| Field | Type | Description |
|---|---|---|
| `minProfitQu` | sint64 | Minimum NET QU profit per arb, after the Qswap swap fee |
| `proposalFeeDefault` | sint64 | Default proposal submission fee (most types) |
| `proposalFeeAddPool` | sint64 | Fee for ADD_POOL proposals |
| `proposalFeePayoutStructure` | sint64 | Fee for UPDATE_PAYOUT proposals |
| `payoutStructure` | uint8 | Active payout preset (0, 1, 2, or 3 = recovery) |
| `minVoterQuorum` | uint16 | Min unique qualified voters required for consensus |
| `proposalsThisEpoch` | uint8 | Number of proposals submitted this epoch |
| `minReserveProfitPct` | uint8 | Min % profit required before selling reserve tokens |
| `depositorVoteMinQu` | sint64 | Min locked QU for a depositor veto vote to qualify |
| `relockAddAmount` | sint64 | Min additional QU required to re-lock a vault position |
| `execReserveFloor` | sint64 | Execution-fee-reserve safety floor; when the reserve drops below this, all profit is routed to the fee reserve (recovery preset) until it refills (0 = disabled) |
| `inLimpMode` | uint8 | 1 if currently in recovery/limp mode (100% of profit routed to the fee reserve; the contract keeps trading); returns to 0 once the reserve reaches 10% above the floor |
| `vixBreakoutFactor` | sint64 | VIX breakout sensitivity ×100 (200 = 2.00×): how far a token's recent volatility must exceed its own baseline to wake the Dagger |
| `vixAbsFloorBps` | sint64 | Minimum absolute recent volatility (basis points) for a breakout to count |
| `vixSampleInterval` | uint32 | Ticks between VIX price samples (345600 = 1/day, 172800 = 2/day, 115200 = 3/day) |
| `swingSellPct` | sint64 | Cloak sell chunk: % of a held bag sold each time the +12% trigger fires |
| `breakoutRescanTicks` | uint32 | Dagger hot re-scan pace in ticks (÷4 = seconds) while a pool is breaking out |
| `qxFeeLivePerTrade` | uint8 | QX-fee source: 0 = per-epoch cached fee (default); 1 = fetch the QX fee live before each sell |

---

### GetWaitlistPosition (Function ID 5)

Returns the caller's current position on the vault waitlist. No input required.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 5 "" \
  "uint8,uint16,sint64,uint16,sint64,uint8"
```

**Output fields in order:**

| Field | Type | Description |
|---|---|---|
| `onWaitlist` | uint8 | 1 if caller is on the waitlist; 0 if not |
| `position` | uint16 | Rank (1 = next to be promoted; larger stakes rank higher) |
| `amount` | sint64 | QU the caller has queued |
| `totalWaiting` | uint16 | Total number of entries currently in the waitlist |
| `minAmount` | sint64 | Smallest amount currently queued (0 if waitlist empty) |
| `isFull` | uint8 | 1 if waitlist is at capacity (500 entries) |

> To displace an existing waitlist entry, your deposit must exceed `minAmount`.

---

## Section 4 — Depositor Vault

The vault accepts QU deposits and puts them to work as additional trading capital. Depositors
share in trading profits and losses symmetrically. All deposits are subject to a **26-epoch
personal lock**.

### VaultDeposit (Procedure ID 3)

Deposit QU into the vault. The entire `AMOUNT` field is your deposit — there is no separate
input format. Shares are issued at the current `vaultSharePrice`.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 3 DEPOSIT_AMOUNT ""
```

**Output format:** `"uint8,sint64,sint64"`

| Field | Type | Description |
|---|---|---|
| `success` | uint8 | 0=rejected, 1=deposited immediately, 2=added to waitlist |
| `sharesIssued` | sint64 | Vault shares issued (0 if waitlisted) |
| `newSharePrice` | sint64 | Current share price after deposit |

> If the vault is full (5,000 depositor slots), you are automatically queued on the waitlist
> (up to 500 entries, served largest-first when slots open). Use `WaitlistWithdraw` to cancel.

---

### WaitlistWithdraw (Procedure ID 5)

Cancel your waitlist entry and reclaim your queued QU. No fee, no input.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 5 0 ""
```

**Output format:** `"uint8,sint64"`

| Field | Type | Description |
|---|---|---|
| `success` | uint8 | 1 if refunded; 0 if caller is not on the waitlist |
| `amountRefunded` | sint64 | QU returned to caller |

---

### VaultWithdraw (Procedure ID 6)

Exit your vault position. If the lock has not expired, a **38% early exit penalty** applies
(combined with the 2% management fee = 40% total gross deduction). After lock expiry, only
the 2% management fee applies. A 5% performance fee is charged on any profit.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 6 0 ""
```

**Output format:** `"uint8,sint64,sint64"`

| Field | Type | Description |
|---|---|---|
| `success` | uint8 | 0=not a depositor, 1=normal exit (lock expired), 2=early exit with penalty |
| `amountReturned` | sint64 | Net QU transferred to caller |
| `penaltyApplied` | sint64 | Early exit penalty deducted (0 for normal exits) |

---

### VaultRelock (Procedure ID 7)

Extend your 26-epoch lock by adding more QU. **Only available within the last 4 epochs of
your current lock window.** The additional QU is deposited at the current share price, new
shares are issued, and your lock epoch resets to the current epoch — giving you a full new
26-epoch commitment.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 7 ADDITIONAL_QU_AMOUNT ""
```

**Minimum additional QU:** Check `relockAddAmount` in `GetGovernanceParams` (default: 10,000,000 QU).
If you send less than the minimum, the full amount is refunded and no re-lock occurs.

**Output format:** `"uint8,sint64,uint32"`

| Field | Type | Description |
|---|---|---|
| `success` | uint8 | 0=not a depositor, 1=window not yet open, 2=lock already expired, 3=insufficient QU sent, 4=success |
| `sharesIssued` | sint64 | New shares issued for the additional QU |
| `newDepEpoch` | uint32 | Your new lock start epoch (current epoch) |

> **Success code 1 (window not yet open):** You are more than 4 epochs before your lock
> expiry. Wait until you are within 4 epochs and try again.
>
> **Success code 2 (lock already expired):** Your lock has fully expired. Use `VaultWithdraw`
> instead; re-lock is no longer available on an expired position.

---

### DonateToContract (Procedure ID 8) · PublicDonate (Procedure ID 9)

Open to **anyone** — send **any amount of QU**. **100% is burned into the contract's
execution-fee reserve** — the on-chain budget that keeps the contract running each tick. Donations
do **not** buy vault shares, earn yield, or affect the vault's value; they simply keep the contract
funded and operational. There is no fixed amount or minimum; the full amount sent is kept (no refund).
Both procedure IDs behave identically.

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 8 3000000 ""
```

(Use procedure ID `9` for `PublicDonate` — same effect.)

**Output format:** `"uint8,sint64"`

| Field | Type | Description |
|---|---|---|
| `success` | uint8 | 1 if accepted; 0 if nothing was sent |
| `toExecutionReserve` | sint64 | QU routed to the execution-fee reserve (the full amount sent, on success) |

---

## Section 5 — Depositor Veto Process

Vault depositors can veto any shareholder-passed proposal by accumulating **125 qualifying
NO votes** before epoch end. A vetoed proposal is treated as failed (proposer receives the
standard 69% fee refund).

### Eligibility requirements

To cast a qualifying veto vote you must:

1. Be an **active vault depositor** (shares > 0, position not expired/withdrawn)
2. Have locked QU ≥ `depositorVoteMinQu` (default: **150,000,000 QU**)  
   Locked QU = your shares × current `vaultSharePrice`
3. Not have already voted on this proposal slot this epoch

> Check the current threshold: query `GetGovernanceParams` (Function 4) and read the
> `depositorVoteMinQu` field.

### Important: votes are re-validated at epoch end

When epoch end arrives, the contract **re-checks** each NO voter's locked QU against the
current `depositorVoteMinQu` and current share price. If your locked value has fallen below
the threshold (due to NAV changes) between the time you voted and epoch end, your NO vote
**will not count** toward the veto threshold at settlement.

### Step-by-step veto process

**Step 1 — Check the current depositorVoteMinQu**

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 4 "" \
  "sint64,sint64,sint64,sint64,uint8,uint16,uint8,uint8,sint64,sint64,sint64,uint8,sint64,sint64,uint32,sint64,uint32,uint8"
```

The 9th field is `depositorVoteMinQu`. Confirm your locked QU exceeds it.

---

**Step 2 — Find the proposal you want to veto**

Query slots 0–9 to find active proposals (status = 1):

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 3 "SLOT_NUMBERuint8" \
  "uint8,uint8,id,uint64,id,uint64,sint64,sint64,id,sint64,sint64,sint64,uint16,uint16"
```

Check `depositorVotesNo` (13th field) to see how many depositor NO votes have already been cast.
The veto triggers at **125** qualifying NO votes.

---

**Step 3 — Cast your veto vote (NO)**

Use Procedure ID 4 (`DepositorVeto`) with `voteYes = 0` (NO):

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT -seed YOURSEED \
  -invokecontractprocedure CONTRACT_INDEX 4 0 "SLOT_NUMBERuint8,0uint8"
```

Replace `SLOT_NUMBER` with the slot you identified in Step 2.

### Complete veto lookup table

| Slot | Veto (NO) | INPUT_FORMAT string |
|------|-----------|---------------------|
| 0    | NO        | `"0uint8,0uint8"`   |
| 1    | NO        | `"1uint8,0uint8"`   |
| 2    | NO        | `"2uint8,0uint8"`   |
| 3    | NO        | `"3uint8,0uint8"`   |
| 4    | NO        | `"4uint8,0uint8"`   |
| 5    | NO        | `"5uint8,0uint8"`   |
| 6    | NO        | `"6uint8,0uint8"`   |
| 7    | NO        | `"7uint8,0uint8"`   |
| 8    | NO        | `"8uint8,0uint8"`   |
| 9    | NO        | `"9uint8,0uint8"`   |

> Depositors may also cast YES votes (`voteYes = 1`) to signal support. YES votes do **not**
> contribute to the veto threshold but are tracked on-chain and visible in `GetProposal`.

---

**Step 4 — Verify your vote landed**

Re-query the proposal and confirm `depositorVotesNo` increased by 1:

```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 3 "SLOT_NUMBERuint8" \
  "uint8,uint8,id,uint64,id,uint64,sint64,sint64,id,sint64,sint64,sint64,uint16,uint16"
```

Do not rely solely on any tool's confirmation — always verify on-chain.

---

**Step 5 — What happens at epoch end**

At the end of the epoch, the contract:

1. Re-validates every depositor NO vote against current locked QU
2. Counts only those still meeting the `depositorVoteMinQu` threshold
3. If the re-validated count is **≥125**, the proposal is set to FAILED and the proposer
   receives a 69% fee refund — regardless of whether shareholders voted yes

---

## Section 6 — Verifying On-Chain

After any action (vote, veto, deposit, withdrawal), verify it landed correctly by querying
the relevant function directly:

**Check a proposal (votes + depositor veto counts):**
```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 3 "SLOT_NUMBERuint8" \
  "uint8,uint8,id,uint64,id,uint64,sint64,sint64,id,sint64,sint64,sint64,uint16,uint16"
```

**Check your waitlist position:**
```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 5 "" \
  "uint8,uint16,sint64,uint16,sint64,uint8"
```

**Check governance parameters (fees, thresholds):**
```
./qubic-cli -nodeip NODE_IP -nodeport NODE_PORT \
  -callcontractfunction CONTRACT_INDEX 4 "" \
  "sint64,sint64,sint64,sint64,uint8,uint16,uint8,uint8,sint64,sint64,sint64,uint8,sint64,sint64,uint32,sint64,uint32,uint8"
```

---

## Section 7 — Community Tool Guidelines

Cloak and Dagger (CLKnDGR) is open source. Community members are welcome to build proposal submission tools.
The following guidelines are strongly recommended:

1. **Open source your tool.** Closed-source tools cannot be verified and should not be
   trusted by shareholders or depositors.

2. **Never request the user's seed.** Your tool should generate and print the complete
   `qubic-cli` command for the user to run themselves. The seed should only ever be
   entered by the user, in their own terminal, directly into `qubic-cli`.

3. **Voting and vetoing are out of scope for tooling.** The lookup tables in Sections 1
   and 5 are sufficient. Building a tool that intermediates these operations introduces
   unnecessary seed risk for a trivially simple operation.

4. **Test on testnet first.** Verify your format strings produce the correct on-chain
   result before releasing any mainnet tooling.

5. **Stay within the published ABI.** This document is the authoritative spec. If you
   find a discrepancy between this document and the contract source code, the contract
   source code is correct.

---

*This document is maintained alongside `CLKnDGR.h`. Report discrepancies by opening
an issue on the repository.*
