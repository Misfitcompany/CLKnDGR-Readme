// Qubic Smart Contract: CLKnDGR
// Arbitrage bot — QX (order-book DEX, index 1) vs Qswap (AMM, index 13)
//
// Cross-contract indices verified against Qswap.h and Qx.h.
// Uses ContractBase / StateData / state.get() / state.mut() pattern.

using namespace QPI;

struct CLKnDGR2
{
};

struct CLKnDGR : public ContractBase
{
    // ---------------------------------------------------------------
    // Constants
    // Note: QX_CONTRACT_INDEX (1) and QSWAP_CONTRACT_INDEX (13) are
    // already defined as macros in contract_def.h — use them directly.
    // ---------------------------------------------------------------
    static constexpr sint64 MIN_PROFIT_QU     = 100100LL;       // initial value; runtime field is state.minProfitQu
    static constexpr uint32 COOLDOWN_TICKS_NO_ARB         = 2160000;  // 6.25 days at 4 ticks/sec
    static constexpr uint32 COOLDOWN_TICKS_UNAFFORDABLE   = 15120000; // 6.25 weeks at 4 ticks/sec — used when CLKnDGR cannot afford any QX ask share

    static constexpr uint64 RESERVE_PCT            = 10;  // % of arb-acquired assets retained as reserve
    static constexpr uint64 RESERVE_BURN_SELL_PCT  = 1;   // % of reserve to burn and sell every 4 epochs (only if profit > minReserveProfitPct)
    static constexpr uint8  RESERVE_ACTION_EPOCHS  = 4;   // epoch interval for reserve burn/sell
    // minReserveProfitPct is a governable state field (initial: 5); valid values: 2, 5, 7, 10
    static constexpr sint64 QEARN_DONATE_THRESHOLD = 10101010LL; // QU accumulated across epochs before the Qearn slice is donated to Qearn's bonus pool
    // minProfitQu valid governance values — allowlist prevents values that mechanically break Dagger (too low) or render it permanently inactive (too high)
    static constexpr sint64 MIN_PROFIT_QU_OPT1 = 100100LL;  // default
    static constexpr sint64 MIN_PROFIT_QU_OPT2 = 250100LL;
    static constexpr sint64 MIN_PROFIT_QU_OPT3 = 420000LL;
    static constexpr sint64 MIN_PROFIT_QU_OPT4 = 676420LL;

    // ---------------------------------------------------------------
    // Depositor vault constants
    // ---------------------------------------------------------------
    static constexpr sint64 VAULT_INITIAL_SHARE_PRICE  = 10000LL;  // QU per share at contract initialization
    static constexpr uint8  VAULT_INITIAL_DEPOSIT_TIER = 8;        // governance tier index [0–8]; higher = larger minimum deposit
    // Minimum share counts by tier index (governance selects index; min QU = minShares × currentSharePrice)
    // Square-bracket arrays are prohibited by QPI006; use a helper function instead.
    static sint64 vaultMinSharesForTier(uint8 tier)
    {
        if      (tier == 0) { return 1; }
        else if (tier == 1) { return 5; }
        else if (tier == 2) { return 10; }
        else if (tier == 3) { return 25; }
        else if (tier == 4) { return 50; }
        else if (tier == 5) { return 100; }
        else if (tier == 6) { return 250; }
        else if (tier == 7) { return 500; }
        else                { return 1000; } // tier == 8 (default/max)
    }
    static constexpr uint8  VAULT_LOCK_EPOCHS           = 26;       // personal lock duration (epochs from depositor's entry epoch)
    static constexpr uint8  VAULT_RELOCK_WINDOW_EPOCHS  = 4;        // epochs before lock expiry during which re-lock is available
    static constexpr sint64 INITIAL_RELOCK_ADD_AMOUNT   = 10000000LL; // default min QU to add when re-locking (10M)
    static constexpr uint64 VAULT_MANAGEMENT_FEE_PCT    = 2;        // % of withdrawal value → qpi.burn()
    static constexpr uint64 VAULT_PERFORMANCE_FEE_PCT          = 5;  // % of profit (withdrawal − cost basis) → epochProfit; only charged when profit > 0
    static constexpr uint64 VAULT_EARLY_WITHDRAWAL_PENALTY_PCT = 38; // % of gross penalty for early exit; combined with VAULT_MANAGEMENT_FEE_PCT = 40% total

    // Qubic burn address: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFXIB == id(0,0,0,0) == NULL_ID
    // Computer Controlled Fund: IAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABXSH == ID(8,0,0,0)

    // ---------------------------------------------------------------
    // Payout structure presets (selected at runtime by payoutStructure state field)
    // Each preset allocates 100% of epoch profit. Trading capital = implicit remainder.
    // DEV_FUND_PCT  — stored in quReserve (contract-held, withdrawn via governance)
    // EXEC_FEE_PCT  — burned immediately via qpi.burn() to top up on-chain execution fees
    // ---------------------------------------------------------------
    // Structure 0 — Default: 55% trading | 30% exec fees | 3% Qearn | 10% shareholders | 0% burn | 1% dev fund | 1% CCF
    static constexpr uint64 PAYOUT0_DEV_FUND_PCT  = 1;
    static constexpr uint64 PAYOUT0_EXEC_FEE_PCT  = 30;
    static constexpr uint64 PAYOUT0_DIST_PCT       = 10;
    static constexpr uint64 PAYOUT0_BURN_PCT       = 0;
    static constexpr uint64 PAYOUT0_QEARN_PCT      = 3;
    static constexpr uint64 PAYOUT0_CCF_PCT        = 1;

    // Structure 1 — Option 1: 61% trading | 27% exec fees | 3% Qearn | 7% shareholders | 0% burn | 1% dev fund | 1% CCF
    static constexpr uint64 PAYOUT1_DEV_FUND_PCT  = 1;
    static constexpr uint64 PAYOUT1_EXEC_FEE_PCT  = 27;
    static constexpr uint64 PAYOUT1_DIST_PCT       = 7;
    static constexpr uint64 PAYOUT1_BURN_PCT       = 0;
    static constexpr uint64 PAYOUT1_QEARN_PCT      = 3;
    static constexpr uint64 PAYOUT1_CCF_PCT        = 1;

    // Structure 2 — Option 2: 65% trading | 25% exec fees | 3% Qearn | 5% shareholders | 0% burn | 1% dev fund | 1% CCF
    static constexpr uint64 PAYOUT2_DEV_FUND_PCT  = 1;
    static constexpr uint64 PAYOUT2_EXEC_FEE_PCT  = 25;
    static constexpr uint64 PAYOUT2_DIST_PCT       = 5;
    static constexpr uint64 PAYOUT2_BURN_PCT       = 0;
    static constexpr uint64 PAYOUT2_QEARN_PCT      = 3;
    static constexpr uint64 PAYOUT2_CCF_PCT        = 1;

    // Structure 3 — RECOVERY / limp mode: 100% exec fees | 0% everything else (0% trading remainder).
    // Auto-applied when the execution-fee reserve < execReserveFloor, OR selectable manually via
    // UPDATE_PAYOUT. The contract KEEPS TRADING NORMALLY (it does NOT pause) and routes 100% of its
    // profit into the execution-fee reserve — rebuilding the fee budget from its own earnings — while
    // suspending the shareholder / Qearn / dev-fund / CCF payouts until the reserve recovers.
    static constexpr uint64 PAYOUT3_DEV_FUND_PCT  = 0;
    static constexpr uint64 PAYOUT3_EXEC_FEE_PCT  = 100;
    static constexpr uint64 PAYOUT3_DIST_PCT       = 0;
    static constexpr uint64 PAYOUT3_BURN_PCT       = 0;
    static constexpr uint64 PAYOUT3_QEARN_PCT      = 0;
    static constexpr uint64 PAYOUT3_CCF_PCT        = 0;

    // ---------------------------------------------------------------
    // Governance constants
    // ---------------------------------------------------------------
    // Proposal fees by type — each independently governable via proposals
    static constexpr sint64 INITIAL_PROPOSAL_FEE_DEFAULT  = 50000000LL;  // 50M QU: default for most proposal types
    static constexpr sint64 INITIAL_PROPOSAL_FEE_ADD_POOL = 200000000LL; // 200M QU: adding a new pool
    static constexpr sint64 INITIAL_PROPOSAL_FEE_PAYOUT   = 69000000LL;  // 69M QU: changing payout structure
    static constexpr uint8  MAX_PROPOSALS_PER_EPOCH        = 10;
    static constexpr uint16 PROPOSAL_VOTER_CAPACITY        = 676;         // max unique voters = total IPO shares
    static constexpr uint16 INITIAL_MIN_VOTER_QUORUM       = 15;          // minimum unique qualified voters for consensus
    static constexpr uint16 MAX_VOTER_QUORUM               = 676;         // equals PROPOSAL_VOTER_CAPACITY — allows governance to require up to full shareholder participation
    static constexpr sint64 MIN_SHARES_QUORUM              = 222; // minimum total weighted shares voted (yes+no) for consensus; 222/676 = ~33% of supply — prevents small coalitions from passing proposals

    // Depositor governance veto constants
    static constexpr uint16 MAX_DEPOSITORS           = 5000;         // max depositor vault slots; raised above IPO share count to allow broader participation
    // depositorVoteMinQu is a governable state field (initial: 150M); valid values: 50000000, 150000000, 250000000, 350000000
    static constexpr uint16 DEPOSITOR_VETO_THRESHOLD = 125;          // depositor NO votes required to veto a shareholder-passed proposal
    static constexpr uint16 WAITLIST_SIZE            = 500;          // max wallets queued for a vault slot; served largest-first when slots open
    // NOTE: QPI requires power-of-2 container capacities (qpi.h static_assert L && !(L & (L-1))).
    // The container template args below are the next 2^N ABOVE these logical caps; the caps still
    // bound actual population. Map: proposal slots 16>=10, voters 1024>=676, depositors 8192>=5000,
    // waitlist 512>=500, swing price history 4096>=(256 pools x SWING_PRICE_SLOTS 13 = 3328).

    // ---------------------------------------------------------------
    // Cloak (swing trade) constants
    // ---------------------------------------------------------------
    static constexpr uint32 SWING_COOLDOWN_TICKS         = 10368000; // 30 days @ 4 ticks/sec — the Cloak checks each pool ~once a month (buy / DCA-add / sell)
    static constexpr uint64 SWING_DCA_ADD_DIVISOR        = 400;      // each DCA-in add = trading capital / 400 = 0.25% (the first buy is SWING_BUY_CAPITAL_PCT = 1%)
    static constexpr uint64 MAX_SWING_POSITION_PCT       = 5;        // stop DCA-adding once a token's cost basis reaches 5% of trading capital (re-opens as capital grows or it sells)
    static constexpr uint8  SWING_PRICE_SLOTS            = 13;     // rolling epoch price buffer (~3 months)
    static constexpr uint64 SWING_BUY_DIP_PCT            = 10;     // buy when 1-week avg <= 3-month avg × 90%
    static constexpr uint64 SWING_SELL_GAIN_PCT          = 12;     // sell when pool price >= cost per token × 112%
    static constexpr uint64 INITIAL_SWING_SELL_PCT       = 50;     // default: sell 50% of the swing bag per +12% trigger; governable via PROP_TYPE_UPDATE_SWING_SELL_PCT
    static constexpr uint64 SWING_BUY_CAPITAL_PCT        = 1;      // 1% of tradingBalance per swing buy
    static constexpr uint64 SWING_LIQUIDITY_REQUIRED_PCT = 80;     // min % of desired QU available in QX bid book
    static constexpr uint64 SWING_ASK_DISCOUNT_PCT       = 10;     // ask placed 10% below pool price for guaranteed fill
    static constexpr uint64 SWING_BUY_SLIPPAGE_PCT       = 5;      // accept up to 5% slippage on Qswap buy

    // ---------------------------------------------------------------
    // VIX — volatility-gated Dagger constants
    // ---------------------------------------------------------------
    // A cheap per-pool volatility index, sampled from the Qswap pool 3×/day, decides WHEN the Dagger
    // bothers to scan: it hunts at tick speed during a volatility breakout and sleeps when calm.
    static constexpr uint32 VIX_DAY_TICKS                = 345600;  // 1 day @ 4 ticks/sec — base for the pulse-rate options
    static constexpr uint32 INITIAL_VIX_SAMPLE_INTERVAL  = 345600;  // default 1 pulse/day; governable via PROP_TYPE_UPDATE_VIX_PULSE_RATE (1/2/3 per day)
    static constexpr uint64 VIX_FAST_DAYS                = 5;       // fast EWMA horizon ≈ 5 days; divisor = days × pulses/day, so the TIME horizon stays fixed regardless of pulse rate
    static constexpr uint64 VIX_SLOW_DAYS                = 28;      // slow EWMA horizon ≈ 4 weeks — the per-token baseline
    static constexpr sint64 VIX_MOVE_CAP_BPS             = 100000;  // cap one sample's move at 1000% (outlier + overflow guard)
    static constexpr uint32 COOLDOWN_TICKS_BREAKOUT      = 1200;    // 5 min @ 4 ticks/sec — Dagger re-scan beat while a pool is breaking out
    static constexpr uint32 COOLDOWN_TICKS_BASELINE      = 4838400; // 2 weeks @ 4 ticks/sec — safety-net scan for calm pools (catches standing arbs without frequent polling)
    static constexpr sint64 INITIAL_VIX_BREAKOUT_FACTOR  = 200;     // breakout when fast vol >= slow × 2.00× (stored ×100); governable via PROP_TYPE_UPDATE_VIX_FACTOR
    static constexpr sint64 INITIAL_VIX_ABS_FLOOR_BPS    = 25;      // ...AND fast vol >= 25 bps absolute (excludes near-dead tokens); governable via PROP_TYPE_UPDATE_VIX_FLOOR

    // Proposal types
    static constexpr uint8 PROP_TYPE_ADD_POOL              = 1;
    static constexpr uint8 PROP_TYPE_REMOVE_POOL           = 2;
    static constexpr uint8 PROP_TYPE_REACTIVATE_POOL       = 3;
    static constexpr uint8 PROP_TYPE_UPDATE_MIN_PROFIT     = 4;
    static constexpr uint8 PROP_TYPE_WITHDRAW_QU_RESERVE   = 5;
    static constexpr uint8 PROP_TYPE_UPDATE_PROPOSAL_FEE   = 6;  // updates proposalFeeDefault
    static constexpr uint8 PROP_TYPE_UPDATE_PAYOUT         = 7;  // newValue = 0 (default), 1, 2, or 3 (recovery)
    static constexpr uint8 PROP_TYPE_UPDATE_FEE_ADD_POOL   = 8;  // updates proposalFeeAddPool
    static constexpr uint8 PROP_TYPE_UPDATE_FEE_PAYOUT     = 9;  // updates proposalFeePayoutStructure
    static constexpr uint8 PROP_TYPE_UPDATE_MIN_QUORUM          = 10; // updates minVoterQuorum
    static constexpr uint8 PROP_TYPE_WITHDRAW_ASSET_RESERVE     = 11; // sweep token reserve for an inactive pool to destination
    static constexpr uint8 PROP_TYPE_UPDATE_VAULT_TIER          = 12; // update vaultDepositTier [0–8]; controls minimum deposit size
    static constexpr uint8 PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT = 13; // update minReserveProfitPct; newValue = 2, 5, 7, or 10
    static constexpr uint8 PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN = 14; // update depositorVoteMinQu; newValue = 50000000, 150000000, 250000000, or 350000000
    static constexpr uint8 PROP_TYPE_UPDATE_RELOCK_AMOUNT      = 15; // update relockAddAmount; newValue = 1M, 5M, 10M, 20M, 25M, or 50M
    static constexpr uint8 PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR = 16; // update execReserveFloor (fee-reserve safety valve); newValue = 0(off), 1B, 5B, 10B, or 20B
    static constexpr uint8 PROP_TYPE_SELL_POOL_TOKENS         = 17; // market-sell newValue% (1..100) of a pool's tokens on Qswap for QU; proceeds → vault profit split (depositors + shareholders)
    static constexpr uint8 PROP_TYPE_UPDATE_VIX_FACTOR        = 18; // update vixBreakoutFactor (VIX breakout sensitivity ×100); newValue = 9,18,37,75,150,200,225,275,350,450,500 (= 0.09×..5×)
    static constexpr uint8 PROP_TYPE_UPDATE_VIX_FLOOR         = 19; // update vixAbsFloorBps (VIX min absolute move, bps); newValue = 0,10,25,50,100,200
    static constexpr uint8 PROP_TYPE_UPDATE_VIX_PULSE_RATE    = 20; // update vixSampleInterval (VIX pulses per day); newValue = 1,2,3 (default 1)
    static constexpr uint8 PROP_TYPE_UPDATE_SWING_SELL_PCT    = 21; // update swingSellPct (Cloak sell chunk %); newValue = 10,15,20,25,33,50 (default 50)
    static constexpr uint8 PROP_TYPE_UPDATE_BREAKOUT_RESCAN   = 22; // update breakoutRescanTicks (Dagger hot re-scan pace); newValue = 30,60,120,180,240,300 seconds (default 300)

    // ---------------------------------------------------------------
    // Direct action constants
    // ---------------------------------------------------------------
    // Donations (both donateToContract and publicDonate) accept ANY positive amount of QU;
    // 100% is burned into this contract's own execution-fee reserve via qpi.burn. No fixed amount.

    // Proposal statuses
    static constexpr uint8 PROP_STATUS_EMPTY  = 0;
    static constexpr uint8 PROP_STATUS_ACTIVE = 1;
    static constexpr uint8 PROP_STATUS_PASSED = 2;
    static constexpr uint8 PROP_STATUS_FAILED = 3;

    // Cross-contract calls use CALL_OTHER_CONTRACT_FUNCTION / INVOKE_OTHER_CONTRACT_PROCEDURE macros.
    // Types are accessed via QX:: and QSWAP:: prefixes (no #include needed; build system exposes all contracts).

    // ---------------------------------------------------------------
    // Governance proposal struct
    // ---------------------------------------------------------------
    struct Proposal
    {
        uint8  proposalType;    // PROP_TYPE_*
        uint8  status;          // PROP_STATUS_*
        id     proposer;
        uint64 assetName;       // PROP_TYPE_ADD_POOL
        id     assetIssuer;     // PROP_TYPE_ADD_POOL
        uint64 poolIndex;       // PROP_TYPE_REMOVE_POOL, PROP_TYPE_REACTIVATE_POOL
        sint64 newValue;        // PROP_TYPE_UPDATE_MIN_PROFIT, PROP_TYPE_UPDATE_PROPOSAL_FEE
        sint64 withdrawAmount;  // PROP_TYPE_WITHDRAW_QU_RESERVE
        id     destination;     // PROP_TYPE_WITHDRAW_QU_RESERVE
        sint64 votesYes;
        sint64 votesNo;
        sint64 feePaid; // QU fee paid at submission; used to compute 69% refund on failure
    };

    struct WaitlistEntry { id wallet; sint64 amount; };

    // One atomic depositor record — replaces the former three parallel HashMaps
    // (depositorShares / depositorCostBasis / depositorEpoch). Storing the three values
    // under one wallet key means they can never drift out of sync, and the 32-byte wallet
    // key is stored once instead of three times.
    struct DepositorInfo { sint64 shares; sint64 costBasis; uint32 epoch; };

    // ---------------------------------------------------------------
    // Public I/O structs
    // ---------------------------------------------------------------
    struct getStats_input  {};
    struct getStats_output { uint64 totalArbsExecuted; sint64 totalProfitEarned; sint64 quBalance; sint64 quReserve; sint64 qearnReserve; uint8 poolCount; };

    struct getPool_input  { uint64 index; };
    struct getPool_output { uint64 assetName; id assetIssuer; uint8 active; };

    struct getProposal_input  { uint8 slot; };
    struct getProposal_output
    {
        uint8  proposalType;
        uint8  status;
        id     proposer;
        uint64 assetName;
        id     assetIssuer;
        uint64 poolIndex;
        sint64 newValue;
        sint64 withdrawAmount;
        id     destination;
        sint64 votesYes;
        sint64 votesNo;
        sint64 feePaid;
        uint16 depositorVotesNo;   // depositor NO count this epoch for this slot
        uint16 depositorVotesYes;  // depositor YES count this epoch for this slot
    };

    struct getGovernanceParams_input  {};
    struct getGovernanceParams_output
    {
        sint64 minProfitQu;
        sint64 proposalFeeDefault;
        sint64 proposalFeeAddPool;
        sint64 proposalFeePayoutStructure;
        uint8  payoutStructure;
        uint16 minVoterQuorum;
        uint8  proposalsThisEpoch;
        uint8  minReserveProfitPct;
        sint64 depositorVoteMinQu;
        sint64 relockAddAmount;
        sint64 execReserveFloor;
        uint8  inLimpMode;
        sint64 vixBreakoutFactor;  // VIX breakout multiplier ×100 (200 = 2.00×)
        sint64 vixAbsFloorBps;     // VIX absolute floor in basis points
        uint32 vixSampleInterval;  // ticks between VIX pulses (345600=1/day, 172800=2/day, 115200=3/day)
        sint64 swingSellPct;       // Cloak sell chunk % per +12% trigger
        uint32 breakoutRescanTicks;// Dagger breakout re-scan pace (ticks; ÷4 = seconds)
    };

    struct submitProposal_input
    {
        uint8  proposalType;
        uint64 assetName;       // PROP_TYPE_ADD_POOL
        id     assetIssuer;     // PROP_TYPE_ADD_POOL
        uint64 poolIndex;       // PROP_TYPE_REMOVE_POOL, PROP_TYPE_REACTIVATE_POOL
        sint64 newValue;        // PROP_TYPE_UPDATE_MIN_PROFIT, PROP_TYPE_UPDATE_PROPOSAL_FEE
        sint64 withdrawAmount;  // PROP_TYPE_WITHDRAW_QU_RESERVE
        id     destination;     // PROP_TYPE_WITHDRAW_QU_RESERVE
    };
    struct submitProposal_output { uint8 success; uint8 slot; };
    struct submitProposal_locals { sint64 shareBalance; sint64 requiredFee; sint64 feeBurnAmt; sint64 feeExecAmt; uint8 slot; Proposal prop; uint64 i; uint8 found; uint8 contentValid; sint64 refundFee; Entity entity; };

    struct voteOnProposal_input  { uint8 slot; uint8 voteYes; }; // voteYes: 1=yes 0=no
    struct voteOnProposal_output { uint8 success; };
    struct voteOnProposal_locals { sint64 shareBalance; bit_64 voterBitfield; bit_64 voteYesBitfield; uint8 s; uint8 isFirstVote; };

    struct depositorVeto_input  { uint8 slot; uint8 voteYes; }; // voteYes: 1=yes 0=no; only NO votes count toward veto threshold
    struct depositorVeto_output { uint8 success; };
    struct depositorVeto_locals { DepositorInfo depInfo; sint64 depositorShares; bit_64 voteBitfield; sint64 lockedQu; };

    // success: 0=rejected, 1=deposited immediately, 2=added to waitlist
    struct vaultDeposit_input   {};  // amount = qpi.invocationReward() (QU sent with the call)
    struct vaultDeposit_output  { uint8 success; sint64 sharesIssued; sint64 newSharePrice; };
    struct vaultDeposit_locals
    {
        DepositorInfo depInfo;       // working depositor record
        Entity       entity;         // self contract-balance via qpi.getEntity(SELF, ...)
        sint64       depositAmount;  // QU received with this call (qpi.invocationReward())
        sint64       minDeposit;     // minimum QU required = VAULT_MIN_SHARE_TIERS[tier] × sharePrice
        sint64       sharesIssued;   // floor(depositAmount / vaultSharePrice)
        sint64       existing;       // existing shares for the caller (0 = new depositor)
        uint8        found;          // scratch for HashMap lookup
        uint16       wlI;            // waitlist scan / right-shift index
        uint16       wlJ;            // waitlist right-shift inner index
        WaitlistEntry wlEntry;       // scratch entry for sorted waitlist insertion
    };

    struct waitlistWithdraw_input  {};
    struct waitlistWithdraw_output { uint8 success; sint64 amountRefunded; };
    struct waitlistWithdraw_locals { uint16 i; uint16 j; };

    // success: 0=not a depositor, 1=normal exit (lock expired), 2=early exit with penalty
    struct vaultWithdraw_input  {};
    struct vaultWithdraw_output { uint8 success; sint64 amountReturned; sint64 penaltyApplied; };
    struct vaultWithdraw_locals
    {
        DepositorInfo depInfo; // working depositor record
        uint8  found;
        uint8  foundInList; // set to 1 if wallet was found and removed from depositorList
        uint32 depEpoch;
        sint64 shares;
        sint64 costBasis;
        sint64 gross;
        sint64 mgmtFee;
        sint64 earlyPenalty;
        sint64 perfFee;
        sint64 profit;
        sint64 net;
        uint16 k;
    };

    struct getWaitlistPosition_input  {};
    struct getWaitlistPosition_output
    {
        uint8  onWaitlist;   // 1 if caller is on the waitlist
        uint16 position;     // caller's rank (1 = next promoted, largest stake)
        sint64 amount;       // QU the caller has queued
        uint16 totalWaiting; // total entries currently in waitlist
        sint64 minAmount;    // smallest amount currently queued (beat this to displace); 0 if waitlist empty
        uint8  isFull;       // 1 if waitlist is at WAITLIST_SIZE capacity
    };
    struct getWaitlistPosition_locals { uint16 i; };

    // success: 0=not a depositor, 1=window not yet open, 2=lock already expired, 3=insufficient QU sent, 4=success
    struct vaultRelock_input   {};  // additional QU = qpi.invocationReward()
    struct vaultRelock_output  { uint8 success; sint64 sharesIssued; uint32 newDepEpoch; };
    struct vaultRelock_locals  { DepositorInfo depInfo; uint8 found; uint32 depEpoch; sint64 addAmount; sint64 newShares; sint64 curShares; sint64 curCostBasis; };

    struct donateToContract_input  {};
    struct donateToContract_output { uint8 success; sint64 toExecutionReserve; };
    struct donateToContract_locals { sint64 amt; };

    struct publicDonate_input  {};
    struct publicDonate_output { uint8 success; sint64 toExecutionReserve; };
    struct publicDonate_locals { sint64 amt; };

    // ---------------------------------------------------------------
    // System procedure locals
    // ---------------------------------------------------------------
    struct END_EPOCH_locals
    {
        DepositorInfo depInfo;       // working depositor record (veto re-validation)
        Entity  entity;              // self contract-balance via qpi.getEntity(SELF, ...)
        uint64  i;
        uint64  j;                   // inner loop for AddPool duplicate check
        uint16  k;                   // voter iteration index for re-verification pass
        id      currentVoter;
        sint64  currentVoterBalance;
        bit_64  voteSlotBits;        // which slots this voter voted on
        bit_64  voteYesBits;         // which slots this voter voted YES on
        Array<uint16, 16> voterCountPerProposal; // unique qualified voters per proposal slot (for quorum check)
        Proposal prop;
        sint64  totalVotes;
        sint64  refundAmt; // 69% of feePaid returned to proposer on failure
        sint64  execBurnAmt; // held 69% of feePaid burned to the execution-fee reserve when a proposal passes
        uint8   found;
        Asset   withdrawAsset;       // PROP_TYPE_WITHDRAW_ASSET_RESERVE: asset to sweep
        sint64  actualAssetBalance;  // PROP_TYPE_WITHDRAW_ASSET_RESERVE: on-chain balance
        sint64  transferResult;      // PROP_TYPE_WITHDRAW_ASSET_RESERVE: return value from transferShareOwnershipAndPossession
        uint16  dvNoCount;           // re-validated qualifying NO vote count for depositor veto check
        uint16  minVoterQuorumSnap;     // snapshot of minVoterQuorum at loop entry — prevents UPDATE_MIN_QUORUM from lowering the bar mid-loop
        sint64  depositorVoteMinSnap;   // snapshot of depositorVoteMinQu at loop entry — prevents UPDATE_DEPOSITOR_VOTE_MIN from disabling subsequent veto checks
        sint64  availableBalance;       // running contract balance — decremented after each transfer so multi-refund in one epoch can't overdraft
        // PROP_TYPE_SELL_POOL_TOKENS: market-sell pct% of a pool's holdings on Qswap for QU
        sint64  sellPct;                // validated 1..100 percent to sell
        sint64  sellSwingAmt;           // pct% of swingTokens
        sint64  sellReserveAmt;         // pct% of poolReserveTokens
        sint64  sellTotalAmt;           // swing + reserve tokens released to Qswap this sell
        sint64  sellSwingCost;          // cost basis of the swing tokens sold (for swap-fail recovery)
        sint64  sellReserveCost;        // cost basis of the reserve tokens sold (for swap-fail recovery)
        sint64  sellPerToken;           // scratch: cost basis per token of the bucket being reduced
        sint64  sellNewTokens;          // scratch: bucket token count after the sell
        QSWAP::QuoteExactAssetInput_input  sellQaIn;
        QSWAP::QuoteExactAssetInput_output sellQaOut;
        QSWAP::SwapExactAssetForQu_input   sellSwapIn;
        QSWAP::SwapExactAssetForQu_output  sellSwapOut;
    };

    struct BEGIN_EPOCH_locals
    {
        DepositorInfo depInfo;      // working depositor record (auto-payout + waitlist promotion)
        Entity entity;              // self contract-balance via qpi.getEntity(SELF, ...)
        // Payout structure dispatch — set from payoutStructure state field each epoch
        sint64 feeReserveNow;   // safety valve: current execution-fee reserve (qpi.queryFeeReserve)
        uint8  effectivePayout; // safety valve: payoutStructure, or 3 (recovery) when feeReserveNow < execReserveFloor
        uint64 activeDevFundPct;
        uint64 activeExecFeePct;
        uint64 activeDistPct;
        uint64 activeBurnPct;
        uint64 activeQearnPct;
        uint64 activeCcfPct;
        uint64 devFundAmt;      // QU added to quReserve (dev fund) this epoch
        uint64 execFeeAmt;      // QU burned via qpi.burn() to top up execution fees this epoch
        uint64 distribAmount;
        uint64 epochBurnAmount; // QU burned to NULL_ID each epoch
        uint64 ccfAmt;          // QU sent to Computer Controlled Fund each epoch
        // reserve burn/sell action (runs every RESERVE_ACTION_EPOCHS)
        uint64 i;
        uint64 j;          // proposal slot clearing loop
        uint64 navRatio;   // precision-scaled NAV ratio (vaultCurBalance / prevTradingBalance × 1000) for overflow-safe update
        sint64 burnAmt;
        sint64 sellAmt;
        sint64 sellProceeds; // QU received from reserve ask order fill (sellAmt is reused as token count elsewhere)
        sint64 currentValue;
        sint64 costBasis;
        sint64 costPerToken;  // cost basis per individual token (costBasis / reserveTokens)
        sint64 minAskPrice;   // minimum profitable ask price (costPerToken * (100 + minReserveProfitPct) / 100)
        sint64 profitPct;
        sint64 newReserve;
        sint64 newCostBasis;
        sint64 totalActioned;
        QX::AssetBidOrders_input  bidIn;
        QX::AssetBidOrders_output bidOut;
        QX::AddToAskOrder_input   addAskIn;
        QX::AddToAskOrder_output  addAskOut;
        QX::Fees_input            feesIn;
        QX::Fees_output           feesOut;
        Asset                     sellAsset; // for releaseShares call
        sint64 transferResult;
        uint64             qearnDonateAmt;
        Proposal           emptyProp;  // scratch proposal used to clear the proposals array each epoch
        // Vault NAV update and auto-payout
        sint64 vaultCurBalance;     // contractBalance - quReserve - qearnReserve - qearnDonateAmt(this epoch) - waitlistQu
        sint64 vaultNewPool;        // updated totalDepositorPool after NAV ratio
        sint64 vaultPostSplitBal;   // trading balance after profit split; saved as prevTradingBalance
        uint16 vaultK;              // loop index for auto-payout sweep
        id     vaultPayee;          // current depositor being processed
        uint8  vaultFound;          // HashMap lookup scratch flag
        uint32 vaultDepEpoch;       // depositor's entry epoch
        sint64 vaultShares;         // depositor's shares outstanding
        sint64 vaultGross;          // shares × vaultSharePrice
        sint64 vaultCostBasis;      // original deposit amount
        sint64 vaultProfit;         // vaultGross - vaultCostBasis
        sint64 vaultMgmtFee;        // 2% of gross → qpi.burn()
        sint64 vaultPerfFee;        // 5% of profit (when profit > 0) → epochProfit
        sint64 vaultNet;            // gross - mgmtFee - perfFee → transferred to depositor
        // Waitlist promotion (runs after auto-payout, before profit split)
        uint16       wlI;           // waitlist left-shift loop index
        uint16       wlBudget;      // per-epoch promotion cap (bounds O(n²) shift cost)
        WaitlistEntry wlEntry;      // scratch for waitlist-to-vault promotion
        sint64       wlShares;      // shares to issue from waitlist entry
        // Cloak: epoch price sampling
        QSWAP::GetPoolBasicState_input  swingPoolSampleIn;
        QSWAP::GetPoolBasicState_output swingPoolSampleOut;
        sint64 swingSpotPrice;
        uint8  swingHead;
    };

    struct BEGIN_TICK_locals
    {
        Entity entity;              // self contract-balance via qpi.getEntity(SELF, ...)
        uint64 i;
        sint64 estimatedProfit;
        sint64 quCostOnQx;
        QSWAP::GetPoolBasicState_input     poolIn;
        QSWAP::GetPoolBasicState_output    poolOut;
        QSWAP::QuoteExactAssetInput_input  qaIn;
        QSWAP::QuoteExactAssetInput_output qaOut;
        QX::AssetAskOrders_input           askIn;
        QX::AssetAskOrders_output          askOut;
        QSWAP::SwapExactAssetForQu_input   swapAssetIn;
        QSWAP::SwapExactAssetForQu_output  swapAssetOut;
        QX::AddToBidOrder_input            addBidIn;
        QX::AddToBidOrder_output           addBidOut;
        // reserve retention (10% of arb-acquired assets)
        sint64 reserveAmt;
        sint64 sellAmt;
        Asset                      tradeAsset; // for releaseShares calls
        sint64 transferResult;
        QSWAP::TransferShareManagementRights_input  swapTsrmIn;
        QSWAP::TransferShareManagementRights_output swapTsrmOut;
        QX::TransferShareManagementRights_input     qxTsrmIn;
        QX::TransferShareManagementRights_output    qxTsrmOut;
        sint64 recoveredAmt; // shares successfully recovered in the per-tick recovery pass
        sint64 recoveredCost; // proportional QU cost applied to recovered shares
        sint64 tradingBalance;        // quBalance minus earmarked reserves (quReserve + qearnReserve); actual capital available to trade
        sint64 initialTradingBalance; // tradingBalance snapshot taken once before any Cloak or Dagger deductions; used for unaffordable-cooldown gating
        sint64 scaleFactor;           // trade size divisor when estimatedProfit >> minProfitQu
        // Cloak (swing trade) locals
        QSWAP::GetPoolBasicState_input   swingPoolIn;
        QSWAP::GetPoolBasicState_output  swingPoolOut;
        QSWAP::QuoteExactQuInput_input   swingQuoteIn;
        QSWAP::QuoteExactQuInput_output  swingQuoteOut;
        QSWAP::SwapExactQuForAsset_input  swingBuyIn;
        QSWAP::SwapExactQuForAsset_output swingBuyOut;
        QSWAP::TransferShareManagementRights_input  swingTsrmIn;
        QSWAP::TransferShareManagementRights_output swingTsrmOut;
        QX::AssetBidOrders_input  swingBidIn;
        QX::AssetBidOrders_output swingBidOut;
        QX::Fees_input  swingFeesIn;
        QX::Fees_output swingFeesOut;
        QX::AddToAskOrder_input  swingAskIn;
        QX::AddToAskOrder_output swingAskOut;
        Asset   swingAsset;
        sint64  swingPoolPrice;
        sint64  swingOneWeekPrice;
        sint64  swingThreeMonthSum;
        sint64  swingThreeMonthAvg;
        uint8   swingHistCount;
        uint8   swingIsDip;     // 1 = token in a dip (1-week avg <= 3-month avg - SWING_BUY_DIP_PCT%); drives first buy + DCA-in add
        uint8   swingSlot;
        sint64  swingCostPerToken;
        sint64  swingAskPrice;
        sint64  swingSellAmt;
        sint64  swingDesiredQu;
        sint64  swingAvailBidQu;
        uint64  swingBidIdx;
        sint64  swingBuyQu;
        sint64  swingNewTokens;
        sint64  swingNewCost;
        sint64  swingRecoveredAmt;
        sint64  swingRecoveredCost;
        // Dagger Direction A (buy Qswap -> sell into QX bid) locals.
        // Runs after the Cloak and Direction B in the same BEGIN_TICK. To stay within the QPI
        // BEGIN_TICK_locals size cap (an AssetBidOrders_output alone carries a 256-order array),
        // Direction A REUSES the Cloak's swing* scratch — identical DEX call types, since the Cloak
        // performs the very same buy-Qswap / sell-into-QX-bid primitives:
        //   swingBidIn/swingBidOut   (QX AssetBidOrders)        — read the QX best bid
        //   swingBuyIn/swingBuyOut   (QSWAP SwapExactQuForAsset) — buy on Qswap
        //   swingFeesIn/swingFeesOut (QX Fees)                   — transfer fee for releaseShares
        //   swingAskIn/swingAskOut   (QX AddToAskOrder)          — sell into the bid
        // plus the Dagger's swapTsrmIn/swapTsrmOut, tradeAsset, transferResult and tradingBalance.
        // All are free by the time this loop runs. Only the QuoteExactAssetOutput scratch + scalars are new.
        QSWAP::QuoteExactAssetOutput_input  daQuoteIn;
        QSWAP::QuoteExactAssetOutput_output daQuoteOut;
        sint64 daBidPrice;     // QX best-bid price (deterministic fill price for our ask)
        sint64 daBidShares;    // QX best-bid size — cap our sell here for a guaranteed immediate full fill
        sint64 daTargetTokens; // tokens to buy on Qswap (== tokens we will hold)
        sint64 daBuyQu;        // QU to spend on Qswap to acquire daTargetTokens (from QuoteExactAssetOutput)
        sint64 daReserveAmt;   // RESERVE_PCT retained long-term (mirrors Direction B)
        sint64 daSellAmt;      // daTargetTokens - daReserveAmt, sold into the QX bid
        sint64 daProceeds;     // daSellAmt * daBidPrice — QU from the QX fill (lands next tick, booked deferred)
        sint64 daEstProfit;    // daProceeds - daBuyQu
        sint64 daScaleFactor;  // per-pool capital-spread divisor (mirrors Direction B scaleFactor)
        sint64 daTotalTokens;  // tokens actually received + reclaimed from Qswap (basis for reserve/sell split)
        sint64 daCostPerToken; // daBuyQu / daTotalTokens — per-token QU cost for reserve/pending basis
        // VIX sampler scratch (the sampler reuses poolIn/poolOut for the Qswap read)
        sint64 vixPrice;       // current sampled Qswap price (quReserve/assetReserve)
        sint64 vixDelta;       // |price - lastPrice|
        sint64 vixMoveBps;     // relative move this sample, basis points, capped
        uint8  vixBreakoutNow; // 1 = this pool just computed a breakout
        uint64 vixSpd;         // VIX pulses per day (1/2/3), derived from the governable interval
        uint64 vixFastDiv;     // fast EWMA divisor = VIX_FAST_DAYS × vixSpd (keeps the ~5-day horizon fixed)
        uint64 vixSlowDiv;     // slow EWMA divisor = VIX_SLOW_DAYS × vixSpd (keeps the ~4-week horizon fixed)
        uint32 cooldownNoArb;  // adaptive no-arb cooldown: COOLDOWN_TICKS_BREAKOUT (in breakout) or COOLDOWN_TICKS_BASELINE (calm)
    };

    // ---------------------------------------------------------------
    // Persistent state — only fields that must survive between ticks
    // ---------------------------------------------------------------
    struct StateData
    {
        id     owner;

        Array<uint64, 256> poolAssetNames;
        Array<id,    256>  poolIssuers;
        Array<uint8, 256>  poolActive;
        uint8  poolCount;

        uint64 totalArbsExecuted;
        sint64 totalProfitEarned;
        sint64 quBalance;
        sint64 epochProfit;
        sint64 reserveSellProceeds; // QU received from reserve sell orders this epoch; excluded from vault NAV until folded into epochProfit at profit-split time

        // Asset reserve — 10% of arb-acquired tokens retained per pool
        Array<sint64, 256> poolReserveTokens;    // token count held in reserve per pool
        Array<sint64, 256> poolReserveCostBasis; // total QU cost paid for current reserve per pool
        uint8 epochCounter;                       // counts epochs; triggers reserve action at RESERVE_ACTION_EPOCHS

        // QU dev fund — preset-variable % of epoch profit held in contract; withdrawn via governance
        sint64 quReserve;

        // Qearn accumulator — epoch Qearn allocations (and donation-routed Qearn portions) are added
        // here each epoch; when it reaches QEARN_DONATE_THRESHOLD the full amount is donated to the
        // Qearn contract (boosting its epoch bonus pool, not locked for our own yield) and reset to 0
        sint64 qearnReserve;

        // Per-pool cooldown: tick number after which this pool is eligible for arb checks again.
        // Set adaptively when no arb is found (either direction): COOLDOWN_TICKS_BREAKOUT (~5 min) while
        // the pool is in a VIX volatility breakout, else COOLDOWN_TICKS_BASELINE (~2 weeks safety-net).
        // The VIX sampler wakes a newly-breaking-out pool by clearing these to the current tick.
        // poolCooldownTick  gates Direction B (buy QX ask -> sell Qswap).
        // poolCooldownTickA gates Direction A (buy Qswap -> sell QX bid). They are SEPARATE because the
        // two directions fire in opposite market states: "no sell-side arb" (B idle) says nothing about
        // whether a buy-side arb (A) exists, so a single shared cooldown would wrongly suppress A.
        Array<uint32, 256> poolCooldownTick;
        Array<uint32, 256> poolCooldownTickA;

        // Transfer-rights recovery: when Leg-2 TSRM fails, shares are owned by CLKnDGR but
        // managed by the DEX that executed Leg 1. These fields schedule a retry for the next tick.
        // Source: 0 = none, 1 = stuck under Qswap managing rights, 2 = stuck under QX managing rights.
        Array<sint64, 256> poolPendingRecoveryAmt;
        Array<uint8,  256> poolPendingRecoverySource;
        Array<sint64, 256> poolPendingRecoveryCostBasis; // total QU cost paid for pending recovery tokens

        // ---------------------------------------------------------------
        // Governance — on-chain shareholder voting for pool and parameter changes
        // ---------------------------------------------------------------
        // selfAsset identifies CLKnDGR's own IPO shares for voting power queries.
        // TODO: set selfAsset.issuer and selfAsset.assetName correctly before deployment —
        // same approach as qRWA's mQmineAsset.
        Asset selfAsset;

        sint64 minProfitQu;               // runtime version of MIN_PROFIT_QU (initial: 100,100 QU)
        sint64 proposalFeeDefault;        // QU required for most proposal types (initial: 50,000,000 QU)
        sint64 proposalFeeAddPool;        // QU required for AddPool proposals (initial: 200,000,000 QU)
        sint64 proposalFeePayoutStructure; // QU required for UpdatePayout proposals (initial: 69,000,000 QU)
        uint8  payoutStructure;           // active payout preset: 0=default, 1=option1, 2=option2
        uint16 minVoterQuorum;            // minimum unique qualified voters for a proposal to reach consensus
        uint8  proposalsThisEpoch;        // number of proposals submitted in the current epoch

        Array<Proposal, 16> proposals; // current epoch's proposal buffer (max MAX_PROPOSALS_PER_EPOCH)

        // voter → bitmask of which proposal slots (0–9) the voter has already voted on this epoch
        HashMap<id, bit_64, 1024> proposalVoterMap;
        // voter → bitmask of which slots the voter chose YES (0 = NO, only meaningful if voted on that slot)
        // Vote weights are NOT pre-aggregated; END_EPOCH re-verifies current balance before tallying.
        HashMap<id, bit_64, 1024> voterYesChoiceMap;
        // Ordered list of unique voters this epoch, iterated at END_EPOCH for re-verification
        Array<id, 1024> voterList;
        uint16 voterCount; // number of unique voters recorded in voterList this epoch

        // ---------------------------------------------------------------
        // Depositor vault — external QU locked alongside trading capital
        // ---------------------------------------------------------------
        sint64 vaultSharePrice;       // current NAV per share (QU); initialized to VAULT_INITIAL_SHARE_PRICE
        sint64 totalVaultShares;      // sum of all shares currently outstanding
        sint64 totalDepositorPool;    // total QU in vault allocated to depositors (NAV-tracked)
        sint64 prevTradingBalance;    // trading balance snapshot from previous epoch; used for vault NAV ratio at epoch start
        uint8  vaultDepositTier;      // current deposit tier [0–8]; min shares = VAULT_MIN_SHARE_TIERS[tier]
        uint8  minReserveProfitPct;   // governance-controlled profit threshold for reserve burn/sell; valid: 2, 5, 7, 10
        uint16 depositorCount;        // number of active depositor slots occupied
        sint64 depositorVoteMinQu;    // governance-controlled minimum QU locked to cast a depositor veto vote; valid: 50M, 150M, 250M, 350M
        sint64 relockAddAmount;       // minimum QU to add when re-locking; initial: 10M; governable via PROP_TYPE_UPDATE_RELOCK_AMOUNT
        sint64 execReserveFloor;      // fee-reserve safety valve: when the execution-fee reserve < this, divert ALL profit to the reserve (recovery preset 3) until it refills — the contract keeps trading normally; 0 = disabled; governable via PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR
        uint8  inLimpMode;            // 1 = in recovery/limp mode. Hysteresis latch: enter when reserve < execReserveFloor, exit only once reserve >= execReserveFloor * 1.1 (prevents flapping at the boundary)

        // One record per depositor wallet — {shares, costBasis, epoch} (formerly three parallel maps;
        // merged so a wallet's three values are written/removed atomically and the key is stored once)
        HashMap<id, DepositorInfo, 8192> depositorInfo;
        Array<id, 8192>           depositorList;      // ordered list of active depositor wallets; iterated at epoch start for auto-payout

        // Depositor governance veto (reset each epoch alongside shareholder governance state)
        HashMap<id, bit_64, 8192> depositorVoteMap; // depositor → bitmask of proposal slots voted this epoch
        Array<uint16, 16> depositorNoVotes;                    // NO vote count per proposal slot; veto triggers at DEPOSITOR_VETO_THRESHOLD
        Array<uint16, 16> depositorYesVotes;                   // YES vote count per proposal slot (informational)

        // Depositor waitlist — wallets queued for a vault slot when vault is at capacity.
        // Maintained sorted by descending deposit amount (largest first).
        // QU is held in the contract; waitlistQu is subtracted from NAV and trading balance
        // so it is never counted as trading capital or credited as arb profit.
        Array<WaitlistEntry, 512> waitlist;
        uint16 waitlistCount; // number of entries currently in waitlist
        sint64 waitlistQu;    // total QU held across all waitlist entries

        // ---------------------------------------------------------------
        // Cloak (swing trade) state
        // ---------------------------------------------------------------
        // Per-pool Qswap spot price history — sampled once per epoch in BEGIN_EPOCH.
        // Flattened as [pool * SWING_PRICE_SLOTS + slot]; 256 pools × 13 epochs = 3,328 entries.
        Array<sint64, 4096> swingPriceHistory;
        Array<uint8,  256>  swingPriceHead;   // rolling write pointer per pool (0–12)
        Array<uint8,  256>  swingPriceCount;  // epochs sampled so far, capped at SWING_PRICE_SLOTS

        // Swing positions — separate from arb reserve; managed by the cloak strategy only
        Array<sint64, 256>  swingTokens;      // tokens held per pool from swing buys
        Array<sint64, 256>  swingCostBasis;   // total QU paid for swingTokens per pool

        // Per-pool cooldown shared by buy and sell actions (25-hour duration)
        Array<uint32, 256>  swingCooldownTick;

        // Swing TSRM recovery — tokens stuck under a DEX's managing rights.
        // Kept separate from dagger poolPendingRecovery* so the two strategies don't collide.
        // Source: 0 = ENTRY path (Qswap buy TSRM failed) → recovered tokens go to swingTokens
        //         2 = EXIT path (ask placed 0 shares + inline QX TSRM failed) → recovered tokens go to poolReserve
        Array<sint64, 256>  swingPendingRecoveryAmt;    // tokens awaiting TSRM retry
        Array<sint64, 256>  swingPendingRecoveryCost;   // QU cost basis for pending recovery tokens
        Array<uint8,  256>  swingPendingRecoverySource; // 0 = Qswap ENTRY, 2 = QX EXIT

        // ---------------------------------------------------------------
        // VIX — volatility-gated Dagger state (per pool)
        // ---------------------------------------------------------------
        // Sampled from the Qswap pool every vixSampleInterval ticks (governable pulse rate). vixFast/vixSlow are running
        // averages (EWMA) of the absolute % price move per sample, in basis points. A breakout
        // (fast >= slow × factor AND fast >= floor) flags the pool and wakes the Dagger to hunt it.
        Array<sint64, 256>  vixFast;            // fast volatility EWMA (bps), ~5-day feel
        Array<sint64, 256>  vixSlow;            // slow volatility EWMA (bps), ~4-week baseline
        Array<sint64, 256>  vixLastPrice;       // last sampled Qswap price (basis for the next move)
        Array<uint32, 256>  vixLastSampleTick;  // tick of the last VIX sample (8h cadence timer)
        Array<uint16, 256>  vixSampleCount;     // samples taken (0 = none yet → just establish baseline)
        Array<uint8,  256>  vixBreakout;        // 1 = pool currently in a volatility breakout
        sint64 vixBreakoutFactor;  // breakout multiplier ×100 (200 = 2.00×); governable via PROP_TYPE_UPDATE_VIX_FACTOR
        sint64 vixAbsFloorBps;     // min fast vol (bps) to count as a breakout; governable via PROP_TYPE_UPDATE_VIX_FLOOR
        uint32 vixSampleInterval;  // ticks between VIX pulses (345600=1/day default, 172800=2/day, 115200=3/day); governable via PROP_TYPE_UPDATE_VIX_PULSE_RATE
        sint64 swingSellPct;       // Cloak: % of a swing bag sold per +12% trigger; default 50; governable via PROP_TYPE_UPDATE_SWING_SELL_PCT
        uint32 breakoutRescanTicks;// Dagger: re-scan pace (ticks) while a pool is in a VIX breakout; default 1200 (5 min); governable via PROP_TYPE_UPDATE_BREAKOUT_RESCAN
    };

    // ---------------------------------------------------------------
    // Initialization — called once after successful IPO
    // Owner is hardcoded because qpi.invocator() returns NULL_ID for
    // system procedures (1-5).
    // DEPLOYMENT ACTION: Replace ID(1,0,0,0) with the actual deployer
    // address (four uint64 components from the deployer public key).
    // ---------------------------------------------------------------
    INITIALIZE()
    {
        state.mut().owner = id(1, 0, 0, 0); // DEPLOYMENT: replace with real deployer address

        state.mut().minProfitQu                = MIN_PROFIT_QU;
        state.mut().proposalFeeDefault         = INITIAL_PROPOSAL_FEE_DEFAULT;
        state.mut().proposalFeeAddPool         = INITIAL_PROPOSAL_FEE_ADD_POOL;
        state.mut().proposalFeePayoutStructure = INITIAL_PROPOSAL_FEE_PAYOUT;
        state.mut().payoutStructure            = 0;
        state.mut().minVoterQuorum             = INITIAL_MIN_VOTER_QUORUM;
        state.mut().proposalsThisEpoch         = 0;
        // Reset governance vote maps so they are usable before the first BEGIN_EPOCH.
        // QPI HashMaps require reset() to be initialized; zero-init alone is not sufficient.
        // (Mirrors the resets in BEGIN_EPOCH; covers the post-IPO / pre-first-epoch window.)
        state.mut().voterCount = 0;
        state.mut().proposalVoterMap.reset();
        state.mut().voterYesChoiceMap.reset();
        state.mut().depositorVoteMap.reset();

        // DEPLOYMENT ACTION: Set selfAsset so governance share-balance queries work.
        // issuer  = id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0)  — already set below.
        // assetName = the IPO share name encoded as uint64 (7 ASCII chars, little-endian).
        //   Compute with: assetNameFromString("CLKNDGR") or confirm from the IPO registration.
        //   Until this is set, numberOfShares() returns 0 for all callers and no proposals
        //   can be submitted (governance fails closed — safe but non-functional).
        state.mut().selfAsset.issuer    = NULL_ID; // contract IPO shares are issued under NULL_ID (Qubic convention; verified vs QVAULT qvaultShare.issuer)
        state.mut().selfAsset.assetName = 23159306787179587ULL; // == assetNameFromString("CLKNDGR"); 7 ASCII chars, little-endian uint64 (verified vs QRWA/MSVAULT encodings)

        // Vault initialization
        state.mut().vaultSharePrice    = VAULT_INITIAL_SHARE_PRICE;
        state.mut().totalVaultShares   = 0;
        state.mut().totalDepositorPool = 0;
        state.mut().prevTradingBalance = 0;
        state.mut().vaultDepositTier      = VAULT_INITIAL_DEPOSIT_TIER;
        state.mut().minReserveProfitPct   = 5; // initial threshold: 5%; governable via PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT
        state.mut().depositorCount        = 0;
        state.mut().depositorVoteMinQu    = 150000000LL; // initial: 150M QU; governable via PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN
        state.mut().relockAddAmount       = INITIAL_RELOCK_ADD_AMOUNT;
        state.mut().vixBreakoutFactor     = INITIAL_VIX_BREAKOUT_FACTOR; // 2.00×; governable via PROP_TYPE_UPDATE_VIX_FACTOR
        state.mut().vixAbsFloorBps         = INITIAL_VIX_ABS_FLOOR_BPS;  // 25 bps; governable via PROP_TYPE_UPDATE_VIX_FLOOR
        state.mut().vixSampleInterval      = INITIAL_VIX_SAMPLE_INTERVAL; // 1 pulse/day; governable via PROP_TYPE_UPDATE_VIX_PULSE_RATE
        state.mut().swingSellPct           = INITIAL_SWING_SELL_PCT;     // 50%; governable via PROP_TYPE_UPDATE_SWING_SELL_PCT
        state.mut().breakoutRescanTicks    = COOLDOWN_TICKS_BREAKOUT;    // 1200 ticks (5 min); governable via PROP_TYPE_UPDATE_BREAKOUT_RESCAN
        state.mut().execReserveFloor      = 0; // safety valve disabled by default; shareholders enable it via governance once real fee rates are observed
        state.mut().inLimpMode            = 0; // not in limp mode at init
    }

    // ---------------------------------------------------------------
    // Registration
    // ---------------------------------------------------------------
    REGISTER_USER_FUNCTIONS_AND_PROCEDURES()
    {
        REGISTER_USER_FUNCTION(getStats,             1);
        REGISTER_USER_FUNCTION(getPool,              2);
        REGISTER_USER_FUNCTION(getProposal,          3);
        REGISTER_USER_FUNCTION(getGovernanceParams,  4);
        REGISTER_USER_FUNCTION(getWaitlistPosition,  5);

        REGISTER_USER_PROCEDURE(submitProposal,   1);
        REGISTER_USER_PROCEDURE(voteOnProposal,   2);
        REGISTER_USER_PROCEDURE(vaultDeposit,     3);
        REGISTER_USER_PROCEDURE(depositorVeto,    4);
        REGISTER_USER_PROCEDURE(waitlistWithdraw, 5);
        REGISTER_USER_PROCEDURE(vaultWithdraw,    6);
        REGISTER_USER_PROCEDURE(vaultRelock,      7);
        REGISTER_USER_PROCEDURE(donateToContract, 8);
        REGISTER_USER_PROCEDURE(publicDonate,     9);
    }

    // ---------------------------------------------------------------
    // Epoch hook — distribute epoch profit per active payout preset
    // ---------------------------------------------------------------
    BEGIN_EPOCH_WITH_LOCALS()
    {
        // --- Clear governance proposals from previous epoch and reset counter ---
        state.mut().proposalsThisEpoch = 0;
        state.mut().voterCount = 0;
        state.mut().proposalVoterMap.reset();
        state.mut().voterYesChoiceMap.reset();
        state.mut().depositorVoteMap.reset();
        locals.emptyProp.proposalType   = 0;
        locals.emptyProp.status         = PROP_STATUS_EMPTY;
        locals.emptyProp.proposer       = NULL_ID;
        locals.emptyProp.assetName      = 0;
        locals.emptyProp.assetIssuer    = NULL_ID;
        locals.emptyProp.poolIndex      = 0;
        locals.emptyProp.newValue       = 0;
        locals.emptyProp.withdrawAmount = 0;
        locals.emptyProp.destination    = NULL_ID;
        locals.emptyProp.votesYes       = 0;
        locals.emptyProp.votesNo        = 0;
        locals.emptyProp.feePaid        = 0;
        for (locals.j = 0; locals.j < MAX_PROPOSALS_PER_EPOCH; locals.j = locals.j + 1)
        {
            state.mut().proposals.set(locals.j, locals.emptyProp);
            state.mut().depositorNoVotes.set(locals.j, 0);
            state.mut().depositorYesVotes.set(locals.j, 0);
        }

        // --- Select active payout percentages from the governance-chosen preset ---
        // Safety valve: if the execution-fee reserve has fallen below the governable floor, this epoch
        // automatically runs the RECOVERY preset (3) regardless of the chosen preset — routing ALL
        // profit into the reserve and suspending the shareholder / Qearn / dev-fund / CCF payouts until
        // it recovers. The contract keeps trading normally. Shareholders can also select preset 3 via
        // UPDATE_PAYOUT. Computed here, upstream of the Qearn-NAV pre-computation below, so the NAV
        // exclusion uses the same (possibly overridden) Qearn percentage as the actual split.
        locals.feeReserveNow = qpi.queryFeeReserve(CLKnDGR_CONTRACT_INDEX);
        // Hysteresis latch (identical rule to BEGIN_TICK): ENTER limp below the floor, EXIT only at
        // 10% above it. Updating it here too keeps it correct regardless of tick/epoch ordering.
        if (state.get().execReserveFloor > 0 && locals.feeReserveNow < state.get().execReserveFloor)
        {
            state.mut().inLimpMode = 1;
        }
        else if (locals.feeReserveNow >= state.get().execReserveFloor + (sint64)div((uint64)state.get().execReserveFloor, (uint64)10))
        {
            state.mut().inLimpMode = 0;
        }
        // While limp, run the RECOVERY preset (3); otherwise the shareholder-chosen preset.
        // payoutStructure is never overwritten, so exiting limp mode auto-reverts to the chosen preset.
        locals.effectivePayout = state.get().inLimpMode ? (uint8)3 : state.get().payoutStructure;
        if (locals.effectivePayout == 1)
        {
            locals.activeDevFundPct = PAYOUT1_DEV_FUND_PCT;
            locals.activeExecFeePct = PAYOUT1_EXEC_FEE_PCT;
            locals.activeDistPct    = PAYOUT1_DIST_PCT;
            locals.activeBurnPct    = PAYOUT1_BURN_PCT;
            locals.activeQearnPct   = PAYOUT1_QEARN_PCT;
            locals.activeCcfPct     = PAYOUT1_CCF_PCT;
        }
        else if (locals.effectivePayout == 2)
        {
            locals.activeDevFundPct = PAYOUT2_DEV_FUND_PCT;
            locals.activeExecFeePct = PAYOUT2_EXEC_FEE_PCT;
            locals.activeDistPct    = PAYOUT2_DIST_PCT;
            locals.activeBurnPct    = PAYOUT2_BURN_PCT;
            locals.activeQearnPct   = PAYOUT2_QEARN_PCT;
            locals.activeCcfPct     = PAYOUT2_CCF_PCT;
        }
        else if (locals.effectivePayout == 3) // RECOVERY / limp mode (safety-valve auto, or manual)
        {
            locals.activeDevFundPct = PAYOUT3_DEV_FUND_PCT;
            locals.activeExecFeePct = PAYOUT3_EXEC_FEE_PCT;
            locals.activeDistPct    = PAYOUT3_DIST_PCT;
            locals.activeBurnPct    = PAYOUT3_BURN_PCT;
            locals.activeQearnPct   = PAYOUT3_QEARN_PCT;
            locals.activeCcfPct     = PAYOUT3_CCF_PCT;
        }
        else // structure 0 (default)
        {
            locals.activeDevFundPct = PAYOUT0_DEV_FUND_PCT;
            locals.activeExecFeePct = PAYOUT0_EXEC_FEE_PCT;
            locals.activeDistPct    = PAYOUT0_DIST_PCT;
            locals.activeBurnPct    = PAYOUT0_BURN_PCT;
            locals.activeQearnPct   = PAYOUT0_QEARN_PCT;
            locals.activeCcfPct     = PAYOUT0_CCF_PCT;
        }

        // Pre-compute this epoch's Qearn allocation so it can be excluded from the NAV below.
        // The Qearn slice is donated to Qearn's bonus pool — a permanent outflow — so it is never
        // depositor trading capital and must not inflate the vault NAV.
        // Uses pre-auto-payout epochProfit; the profit split recomputes from the final value.
        locals.qearnDonateAmt = 0;
        if (state.get().epochProfit > 0)
        {
            locals.qearnDonateAmt = div((uint64)state.get().epochProfit * locals.activeQearnPct, (uint64)100);
        }

        // --- Vault NAV update (before profit split; donated Qearn allocation excluded — not depositor capital) ---
        if (state.get().totalVaultShares > 0 && state.get().prevTradingBalance > 0)
        {
            // Exclude quReserve, accumulated qearnReserve, this epoch's Qearn allocation,
            // waitlistQu, and reserveSellProceeds — none of these represent trading P&L for
            // active depositors. reserveSellProceeds are last epoch's reserve-sell QU sitting
            // in contractBalance pending the profit split; excluding them prevents the vault
            // NAV from being inflated by proceeds that will be distributed (not kept) this epoch.
            qpi.getEntity(SELF, locals.entity);
            locals.vaultCurBalance = (sint64)(locals.entity.incomingAmount - locals.entity.outgoingAmount)
                                     - state.get().quReserve
                                     - state.get().qearnReserve
                                     - (sint64)locals.qearnDonateAmt
                                     - state.get().waitlistQu
                                     - state.get().reserveSellProceeds;
            if (locals.vaultCurBalance > 0)
            {
                // Proportional update: new depositorPool = old × (current / prev)
                // Overflow-safe: scale by 1000 (not 1,000,000) so vaultCurBalance * 1000
                // fits in uint64 for any balance up to the full QU supply (~10^15 QU):
                //   10^15 × 1000 = 10^18 < uint64 max (1.84 × 10^19).
                // Cap navRatio at 9000 (9× per-epoch gain) so the second multiply
                //   totalDepositorPool × navRatio ≤ 10^15 × 9000 = 9 × 10^18 < uint64 max.
                locals.navRatio = div((uint64)locals.vaultCurBalance * 1000ULL,
                                      (uint64)state.get().prevTradingBalance);
                if (locals.navRatio > 9000ULL) { locals.navRatio = 9000ULL; }
                locals.vaultNewPool = (sint64)div((uint64)state.get().totalDepositorPool * locals.navRatio,
                                                  1000ULL);
                state.mut().totalDepositorPool = locals.vaultNewPool;
                // Recompute share price from updated pool (shares outstanding is unchanged)
                state.mut().vaultSharePrice = div((uint64)state.get().totalDepositorPool,
                                                  (uint64)state.get().totalVaultShares);
                if (state.get().vaultSharePrice <= 0) { state.mut().vaultSharePrice = 1; }
            }
        }

        // --- Vault auto-payout: sweep depositors whose 26-epoch personal lock has expired ---
        // Runs after NAV update (so final share price is current) and before profit split
        // (so performance fees flow into epochProfit for this epoch's distribution).
        // Uses swap-with-last removal: when a depositor is paid out, their slot is filled by
        // the last depositor in the list, depositorCount is decremented, and the loop index
        // is NOT advanced so the moved entry is checked on the same iteration.
        locals.vaultK = 0;
        while (locals.vaultK < state.get().depositorCount)
        {
            locals.vaultPayee   = state.get().depositorList.get(locals.vaultK);
            locals.vaultFound   = 0;
            locals.depInfo.shares = 0; locals.depInfo.costBasis = 0; locals.depInfo.epoch = 0;
            locals.vaultFound = (uint8)state.get().depositorInfo.get(locals.vaultPayee, locals.depInfo);
            locals.vaultDepEpoch = locals.depInfo.epoch;
            if (!locals.vaultFound)
            {
                // Stale list entry — remove it
                state.mut().depositorList.set(locals.vaultK,
                    state.get().depositorList.get(state.get().depositorCount - 1));
                state.mut().depositorCount = state.get().depositorCount - 1;
                continue; // recheck same index
            }

            // Check 26-epoch personal lock
            if ((uint32)qpi.epoch() < locals.vaultDepEpoch + (uint32)VAULT_LOCK_EPOCHS)
            {
                locals.vaultK = locals.vaultK + 1;
                continue;
            }

            // Lock expired — compute payout
            locals.vaultShares    = locals.depInfo.shares;
            locals.vaultCostBasis = locals.depInfo.costBasis;
            locals.vaultGross     = (sint64)((uint64)locals.vaultShares * (uint64)state.get().vaultSharePrice);

            // Management fee: 2% of gross → qpi.burn()
            locals.vaultMgmtFee = (sint64)div((uint64)locals.vaultGross * VAULT_MANAGEMENT_FEE_PCT, (uint64)100);

            // Performance fee: 5% of profit (only when in profit) → epochProfit
            locals.vaultProfit  = locals.vaultGross - locals.vaultCostBasis;
            locals.vaultPerfFee = 0;
            if (locals.vaultProfit > 0)
            {
                locals.vaultPerfFee = (sint64)div((uint64)locals.vaultProfit * VAULT_PERFORMANCE_FEE_PCT, (uint64)100);
                state.mut().epochProfit = state.get().epochProfit + locals.vaultPerfFee;
            }

            locals.vaultNet = locals.vaultGross - locals.vaultMgmtFee - locals.vaultPerfFee;

            // Clear depositor records — removeByKey marks slot as recyclable tombstone;
            // set(key,0) permanently occupies the slot (QPI HashMap behavior).
            state.mut().depositorInfo.removeByKey(locals.vaultPayee);
            state.mut().depositorInfo.cleanupIfNeeded();

            // Update vault totals
            state.mut().totalVaultShares   = state.get().totalVaultShares   - locals.vaultShares;
            state.mut().totalDepositorPool = state.get().totalDepositorPool - locals.vaultGross;

            // Remove from list via swap-with-last
            state.mut().depositorList.set(locals.vaultK,
                state.get().depositorList.get(state.get().depositorCount - 1));
            state.mut().depositorCount = state.get().depositorCount - 1;

            // Recalculate share price; reset if vault is now empty
            if (state.get().totalVaultShares > 0)
            {
                state.mut().vaultSharePrice = (sint64)div((uint64)state.get().totalDepositorPool,
                                                           (uint64)state.get().totalVaultShares);
                if (state.get().vaultSharePrice <= 0) { state.mut().vaultSharePrice = 1; }
            }
            else
            {
                state.mut().vaultSharePrice    = VAULT_INITIAL_SHARE_PRICE;
                state.mut().totalDepositorPool = 0;
                state.mut().prevTradingBalance = 0;
            }

            // Transfer to depositor
            if (locals.vaultMgmtFee > 0) { qpi.burn(locals.vaultMgmtFee); }
            if (locals.vaultNet > 0)      { qpi.transfer(locals.vaultPayee, locals.vaultNet); }
            // do NOT increment vaultK — the swapped entry must be checked next
        }

        // --- Zombie cleanup: reset depositor HashMaps when vault is completely drained ---
        // Each HashMap slot is permanently consumed by its first unique key even after zeroing.
        // Resetting on a full drain recycles those slots so future depositors are not blocked
        // by the 5000-unique-address ceiling carrying over from prior cycles.
        if (state.get().totalVaultShares == 0 && state.get().depositorCount == 0)
        {
            state.mut().depositorInfo.reset();
            // depositorVoteMap was already reset at the top of BEGIN_EPOCH
        }

        // --- Waitlist promotion: fill opened vault slots, largest-stake waiters first ---
        // Runs before the profit split so promoted depositors' QU is captured in the
        // prevTradingBalance snapshot and their NAV tracks correctly from next epoch.
        // Cap at 100 promotions per epoch: each promotion left-shifts up to WAITLIST_SIZE=500
        // entries (O(n) per promotion → O(n²) worst case). Without the cap, a full-drain
        // epoch (5000 auto-payouts + 500 promotions × 500 shifts = ~255,000 iterations)
        // risks exceeding the BEGIN_EPOCH execution budget. Remaining waitlisters are served
        // next epoch — their QU is held safely and they retain their sorted position.
        locals.wlBudget = 100;
        while (state.get().waitlistCount > 0 && state.get().depositorCount < MAX_DEPOSITORS && locals.wlBudget > 0)
        {
            locals.wlBudget = locals.wlBudget - 1;
            locals.wlEntry = state.get().waitlist.get(0); // waitlist is sorted largest-first

            locals.wlShares = (sint64)div((uint64)locals.wlEntry.amount,
                                          (uint64)state.get().vaultSharePrice);
            if (locals.wlShares <= 0)
            {
                // Share price has risen too high — this entry can no longer buy a full share. Refund.
                qpi.transfer(locals.wlEntry.wallet, (uint64)locals.wlEntry.amount);
                state.mut().waitlistQu = state.get().waitlistQu - locals.wlEntry.amount;
            }
            else
            {
                locals.depInfo.shares    = locals.wlShares;
                locals.depInfo.costBasis = locals.wlEntry.amount;
                locals.depInfo.epoch     = (uint32)qpi.epoch();
                state.mut().depositorInfo.set(locals.wlEntry.wallet, locals.depInfo);
                state.mut().depositorList.set(state.get().depositorCount, locals.wlEntry.wallet);
                state.mut().totalVaultShares   = state.get().totalVaultShares   + locals.wlShares;
                state.mut().totalDepositorPool = state.get().totalDepositorPool + locals.wlEntry.amount;
                state.mut().depositorCount     = state.get().depositorCount + 1;
                state.mut().waitlistQu         = state.get().waitlistQu - locals.wlEntry.amount;
                if (state.get().totalVaultShares > 0)
                {
                    state.mut().vaultSharePrice = (sint64)div((uint64)state.get().totalDepositorPool,
                                                               (uint64)state.get().totalVaultShares);
                    if (state.get().vaultSharePrice <= 0) { state.mut().vaultSharePrice = 1; }
                }
            }

            // Shift waitlist left to remove entry[0]
            for (locals.wlI = 0; locals.wlI + 1 < state.get().waitlistCount; locals.wlI = locals.wlI + 1)
            {
                state.mut().waitlist.set(locals.wlI, state.get().waitlist.get(locals.wlI + 1));
            }
            state.mut().waitlistCount = state.get().waitlistCount - 1;
        }

        // --- Epoch profit split (all percentages variable per preset; remainder is trading capital) ---
        // Fold reserve sell proceeds (accumulated in reserveSellProceeds to keep them out of the
        // vault NAV calculation above) into epochProfit now so they flow through the standard split.
        if (state.get().reserveSellProceeds > 0)
        {
            state.mut().epochProfit = state.get().epochProfit + state.get().reserveSellProceeds;
            state.mut().reserveSellProceeds = 0;
        }
        if (state.get().epochProfit > 0)
        {
            // Dev fund % stored in quReserve; withdrawn via governance WITHDRAW_QU_RESERVE
            locals.devFundAmt = div((uint64)state.get().epochProfit * locals.activeDevFundPct, (uint64)100);
            if (locals.devFundAmt > 0)
            {
                state.mut().quReserve = state.get().quReserve + (sint64)locals.devFundAmt;
            }

            // Exec fee % burned immediately via qpi.burn() to top up on-chain execution fee balance
            locals.execFeeAmt = div((uint64)state.get().epochProfit * locals.activeExecFeePct, (uint64)100);
            if (locals.execFeeAmt > 0)
            {
                qpi.burn(locals.execFeeAmt);
            }

            // Variable % to shareholders
            locals.distribAmount = div((uint64)state.get().epochProfit * locals.activeDistPct, (uint64)100);
            if (locals.distribAmount > 0)
            {
                qpi.distributeDividends((sint64)locals.distribAmount);
            }

            // Variable % burned — sent to NULL_ID (AAAAAA...FXIB), permanently removed from supply
            locals.epochBurnAmount = div((uint64)state.get().epochProfit * locals.activeBurnPct, (uint64)100);
            if (locals.epochBurnAmount > 0)
            {
                qpi.transfer(NULL_ID, locals.epochBurnAmount);
            }

            // Variable % to Computer Controlled Fund: IAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABXSH = ID(8,0,0,0)
            locals.ccfAmt = div((uint64)state.get().epochProfit * locals.activeCcfPct, (uint64)100);
            if (locals.ccfAmt > 0)
            {
                qpi.transfer(id(8, 0, 0, 0), locals.ccfAmt);
            }

            // Variable % accumulated into qearnReserve each epoch; once it reaches QEARN_DONATE_THRESHOLD
            // the full amount is DONATED to the Qearn contract's balance. Qearn folds any unaccounted
            // balance into its epoch bonus pool at BEGIN_EPOCH (see Qearn.h: current_balance - pre_epoch_balance
            // becomes _epochBonusAmount), so this lifts the yield of every Qearn locker. The contract
            // receives nothing back — a permanent outflow, by design: bolster Qearn, don't profit from it.
            locals.qearnDonateAmt = div((uint64)state.get().epochProfit * locals.activeQearnPct, (uint64)100);
            state.mut().qearnReserve += (sint64)locals.qearnDonateAmt;
            if (state.get().qearnReserve >= QEARN_DONATE_THRESHOLD)
            {
                qpi.transfer(id(QEARN_CONTRACT_INDEX, 0, 0, 0), state.get().qearnReserve);
                state.mut().qearnReserve = 0;
            }
        }
        // Unconditional reset — guards against any future code path that decrements epochProfit below zero,
        // which would otherwise carry a stale negative value into the next epoch's NAV exclusion.
        state.mut().epochProfit = 0;

        // --- Vault: snapshot post-payout trading balance for next epoch's NAV ratio ---
        // Only write when vault has active depositors. If vault is empty, reset to 0 so that
        // vaultDeposit's seeding code fires correctly for the next incoming depositor.
        // Without this guard, a stale snapshot would be written while totalVaultShares == 0,
        // causing vaultDeposit to skip seeding and the first post-drain depositor to have
        // their NAV calculated against an epoch they were not part of.
        qpi.getEntity(SELF, locals.entity);
        locals.vaultPostSplitBal = (sint64)(locals.entity.incomingAmount - locals.entity.outgoingAmount)
                                   - state.get().quReserve
                                   - state.get().qearnReserve
                                   - state.get().waitlistQu; // exclude waitlist QU — not trading capital
        if (state.get().totalVaultShares > 0 && locals.vaultPostSplitBal > 0)
        {
            state.mut().prevTradingBalance = locals.vaultPostSplitBal;
        }
        else if (state.get().totalVaultShares == 0)
        {
            state.mut().prevTradingBalance = 0;
        }

        // --- 4-epoch reserve burn/sell action (1% burn + 1% sell, only if profit > minReserveProfitPct) ---
        state.mut().epochCounter = state.get().epochCounter + 1;
        if (state.get().epochCounter >= RESERVE_ACTION_EPOCHS)
        {
            state.mut().epochCounter = 0;
            for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
            {
                if (!state.get().poolActive.get(locals.i))                    { continue; }
                if (state.get().poolReserveTokens.get(locals.i) <= 0)         { continue; }
                if (state.get().poolReserveCostBasis.get(locals.i) <= 0)      { continue; }

                // Check current best bid to value the reserve
                locals.bidIn.issuer    = state.get().poolIssuers.get(locals.i);
                locals.bidIn.assetName = state.get().poolAssetNames.get(locals.i);
                locals.bidIn.offset    = 0;
                { CALL_OTHER_CONTRACT_FUNCTION(QX, AssetBidOrders, locals.bidIn, locals.bidOut); }
                if (locals.bidOut.orders.get(0).price <= 0) { continue; } // no buyer, skip

                locals.currentValue = (sint64)((uint64)locals.bidOut.orders.get(0).price * (uint64)state.get().poolReserveTokens.get(locals.i));
                locals.costBasis    = state.get().poolReserveCostBasis.get(locals.i);

                // Hold if at a loss or profit <= minReserveProfitPct (governable: 2/5/7/10%)
                if (locals.currentValue <= locals.costBasis) { continue; }
                locals.profitPct = (sint64)div((uint64)(locals.currentValue - locals.costBasis) * 100ULL, (uint64)locals.costBasis);
                if (locals.profitPct <= (sint64)state.get().minReserveProfitPct) { continue; }

                // Minimum acceptable ask price: cost per token + minReserveProfitPct%.
                // Placing the ask here (rather than at bestBid.price) lets the order match against
                // all bids at minAskPrice and above simultaneously, liquidating more reserve per cycle.
                // Fill price is always the bid price (never lower than minAskPrice).
                // The profitPct check above already guarantees bestBid.price >= minAskPrice.
                locals.costPerToken = (sint64)div((uint64)locals.costBasis, (uint64)state.get().poolReserveTokens.get(locals.i));
                locals.minAskPrice  = locals.costPerToken + (sint64)div((uint64)locals.costPerToken * (uint64)state.get().minReserveProfitPct, (uint64)100);
                if (locals.minAskPrice <= 0) { continue; }

                locals.burnAmt = (sint64)div((uint64)state.get().poolReserveTokens.get(locals.i) * (uint64)RESERVE_BURN_SELL_PCT, (uint64)100);
                if (locals.burnAmt <= 0) { locals.burnAmt = 1; } // floor: pools with < 100 tokens still self-liquidate 1 token per cycle
                locals.sellAmt = (sint64)div((uint64)state.get().poolReserveTokens.get(locals.i) * (uint64)RESERVE_BURN_SELL_PCT, (uint64)100);
                if (locals.sellAmt <= 0) { locals.sellAmt = 1; }

                // Cap sellAmt to what the best bid can absorb — guarantees immediate full fill.
                // QX.AddToAskOrder matches immediately against existing bids; any remainder sits
                // as an open limit order that CLKnDGR cannot reclaim (QX.PRE_RELEASE_SHARES rejects).
                if (locals.sellAmt > locals.bidOut.orders.get(0).numberOfShares)
                {
                    locals.sellAmt = locals.bidOut.orders.get(0).numberOfShares;
                }

                // Burn: transfer to Qubic burn address (AAAAAA...FXIB = NULL_ID = id(0,0,0,0))
                // transferShareOwnershipAndPossession returns remaining possessed shares >= 0 on success,
                // or a negative value on failure. If the burn fails, treat burnAmt as 0 so totalActioned
                // below does not undercount reserve tokens that were never actually removed.
                if (locals.burnAmt > 0)
                {
                    locals.transferResult = qpi.transferShareOwnershipAndPossession(
                        state.get().poolAssetNames.get(locals.i),
                        state.get().poolIssuers.get(locals.i),
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        locals.burnAmt,
                        NULL_ID
                    );
                    if (locals.transferResult < 0)
                    {
                        locals.burnAmt = 0;
                    }
                }

                // Sell: give QX managing rights via releaseShares (pays QX transfer fee), then place ask order.
                locals.addAskOut.addedNumberOfShares = 0;
                if (locals.sellAmt > 0)
                {
                    { CALL_OTHER_CONTRACT_FUNCTION(QX, Fees, locals.feesIn, locals.feesOut); }
                    locals.sellAsset.issuer    = state.get().poolIssuers.get(locals.i);
                    locals.sellAsset.assetName = state.get().poolAssetNames.get(locals.i);
                    locals.transferResult = qpi.releaseShares(
                        locals.sellAsset,
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        locals.sellAmt,
                        QX_CONTRACT_INDEX, QX_CONTRACT_INDEX,
                        locals.feesOut.transferFee
                    );
                    // Guard: only place the ask if QX successfully received managing rights.
                    // Without this, AddToAskOrder would fail silently and tokens remain in
                    // reserve while sell proceeds are never received — leaving cost basis
                    // unchanged and the token count unchanged, which is correct via totalActioned below.
                    // releaseShares returns paid fee >= 0 on success; use >= 0 (not > 0) since fee could be 0.
                    if (locals.transferResult >= 0)
                    {
                        locals.addAskIn.issuer         = state.get().poolIssuers.get(locals.i);
                        locals.addAskIn.assetName      = state.get().poolAssetNames.get(locals.i);
                        locals.addAskIn.price          = locals.minAskPrice;
                        locals.addAskIn.numberOfShares = locals.sellAmt;
                        { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, AddToAskOrder, locals.addAskIn, locals.addAskOut, 0); }

                        // Accumulate sell proceeds into reserveSellProceeds (not epochProfit directly).
                        // They are folded into epochProfit just before the profit split so they are
                        // excluded from the vault NAV calculation that runs earlier this same epoch.
                        if (locals.addAskOut.addedNumberOfShares > 0)
                        {
                            locals.sellProceeds = (sint64)((uint64)locals.addAskOut.addedNumberOfShares * (uint64)locals.bidOut.orders.get(0).price);
                            state.mut().reserveSellProceeds = state.get().reserveSellProceeds + locals.sellProceeds;
                            state.mut().totalProfitEarned   = state.get().totalProfitEarned   + locals.sellProceeds;
                        }
                    }
                }

                // Reduce reserve and cost basis proportionally.
                // Use addAskOut.addedNumberOfShares (confirmed by QX) not sellAmt, in case
                // the ask was rejected entirely (e.g. rights transfer failed).
                locals.totalActioned = locals.burnAmt + locals.addAskOut.addedNumberOfShares;
                if (locals.totalActioned > 0)
                {
                    locals.newReserve = state.get().poolReserveTokens.get(locals.i) - locals.totalActioned;
                    if (locals.newReserve < 0) { locals.newReserve = 0; }
                    locals.newCostBasis = (locals.newReserve > 0)
                        ? (sint64)(div((uint64)locals.costBasis, (uint64)state.get().poolReserveTokens.get(locals.i)) * (uint64)locals.newReserve)
                        : 0;
                    state.mut().poolReserveTokens.set(locals.i, locals.newReserve);
                    state.mut().poolReserveCostBasis.set(locals.i, locals.newCostBasis);
                }
            }
        }

        // --- Cloak: sample per-pool Qswap spot price into rolling 13-epoch buffer ---
        // Runs at end of each epoch so per-tick Cloak strategy loop can compute 1-week and 3-month price averages.
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (!state.get().poolActive.get(locals.i)) { continue; }

            locals.swingPoolSampleIn.assetIssuer = state.get().poolIssuers.get(locals.i);
            locals.swingPoolSampleIn.assetName   = state.get().poolAssetNames.get(locals.i);
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, GetPoolBasicState, locals.swingPoolSampleIn, locals.swingPoolSampleOut); }

            if (!locals.swingPoolSampleOut.poolExists)              { continue; }
            if (locals.swingPoolSampleOut.reservedAssetAmount <= 0) { continue; }

            locals.swingSpotPrice = (sint64)div(
                (uint64)locals.swingPoolSampleOut.reservedQuAmount,
                (uint64)locals.swingPoolSampleOut.reservedAssetAmount);
            if (locals.swingSpotPrice <= 0) { continue; }

            locals.swingHead = state.get().swingPriceHead.get(locals.i);
            state.mut().swingPriceHistory.set(
                (uint64)locals.i * (uint64)SWING_PRICE_SLOTS + (uint64)locals.swingHead,
                locals.swingSpotPrice);
            state.mut().swingPriceHead.set(locals.i,
                (uint8)(mod((uint64)locals.swingHead + 1, (uint64)SWING_PRICE_SLOTS)));
            if (state.get().swingPriceCount.get(locals.i) < SWING_PRICE_SLOTS)
            {
                state.mut().swingPriceCount.set(locals.i,
                    state.get().swingPriceCount.get(locals.i) + 1);
            }
        }
    }
    END_EPOCH_WITH_LOCALS()
    {
        // --- Re-verify voter holdings and build fresh vote tallies at consensus time ---
        // Zero out votesYes/votesNo on all proposals (they were not pre-aggregated at cast time)
        for (locals.i = 0; locals.i < (uint64)state.get().proposalsThisEpoch; locals.i = locals.i + 1)
        {
            locals.prop = state.get().proposals.get(locals.i);
            locals.prop.votesYes = 0;
            locals.prop.votesNo  = 0;
            state.mut().proposals.set(locals.i, locals.prop);
            locals.voterCountPerProposal.set(locals.i, 0); // explicit reset; do not rely on QPI zero-init
        }

        // Iterate each voter that cast at least one vote this epoch.
        // Re-check their current share balance: if they sold shares since voting,
        // their weight is 0 and their votes are excluded from all proposal tallies.
        for (locals.k = 0; locals.k < state.get().voterCount; locals.k = locals.k + 1)
        {
            locals.currentVoter = state.get().voterList.get((uint64)locals.k);
            locals.currentVoterBalance = qpi.numberOfShares(
                state.get().selfAsset,
                AssetOwnershipSelect::byOwner(locals.currentVoter),
                AssetPossessionSelect::byPossessor(locals.currentVoter)
            );
            if (locals.currentVoterBalance <= 0) { continue; } // sold shares — excluded

            // Zero before each get() — HashMap leaves outValue unchanged on a miss, so stale bits
            // from the prior iteration would otherwise be attributed to this voter.
            locals.voteSlotBits.setAll(0);
            locals.voteYesBits.setAll(0);
            state.get().proposalVoterMap.get(locals.currentVoter, locals.voteSlotBits);
            state.get().voterYesChoiceMap.get(locals.currentVoter, locals.voteYesBits);

            for (locals.i = 0; locals.i < (uint64)state.get().proposalsThisEpoch; locals.i = locals.i + 1)
            {
                if (locals.voteSlotBits.get((uint8)locals.i) == 0) { continue; } // didn't vote on this slot

                locals.prop = state.get().proposals.get(locals.i);
                if (locals.voteYesBits.get((uint8)locals.i) == 1)
                {
                    locals.prop.votesYes = locals.prop.votesYes + locals.currentVoterBalance;
                }
                else
                {
                    locals.prop.votesNo = locals.prop.votesNo + locals.currentVoterBalance;
                }
                state.mut().proposals.set(locals.i, locals.prop);

                // Count this qualified voter toward the quorum for this slot
                locals.voterCountPerProposal.set(locals.i,
                    locals.voterCountPerProposal.get(locals.i) + 1);
            }
        }

        // --- Tally and execute governance proposals ---
        // Snapshot minVoterQuorum before the loop. UPDATE_MIN_QUORUM may execute mid-loop and
        // lower the threshold; without this snapshot every subsequent proposal inherits the
        // reduced quorum, enabling a governance attack where a supermajority first lowers
        // quorum then drains funds with fewer voters than the original threshold.
        locals.minVoterQuorumSnap = state.get().minVoterQuorum;
        // Same class as minVoterQuorumSnap: UPDATE_DEPOSITOR_VOTE_MIN could raise the veto
        // qualification threshold mid-loop, disqualifying depositors who would otherwise veto
        // a subsequent proposal (e.g., WITHDRAW_QU_RESERVE).
        locals.depositorVoteMinSnap = state.get().depositorVoteMinQu;
        // Running balance for refund guards: qpi.contractBalance() may be a start-of-epoch
        // snapshot that doesn't update between qpi.transfer() calls. Tracking it locally
        // ensures that multiple refunds issued within one END_EPOCH cannot collectively
        // overdraft the contract.
        qpi.getEntity(SELF, locals.entity);
        locals.availableBalance = (sint64)(locals.entity.incomingAmount - locals.entity.outgoingAmount);
        for (locals.i = 0; locals.i < (uint64)state.get().proposalsThisEpoch; locals.i = locals.i + 1)
        {
            locals.prop = state.get().proposals.get(locals.i);
            if (locals.prop.status != PROP_STATUS_ACTIVE) { continue; }

            locals.totalVotes = locals.prop.votesYes + locals.prop.votesNo;
            // Fail if quorum not met: requires both minimum unique qualified voters AND minimum total shares voted.
            // MIN_SHARES_QUORUM (222) prevents a small coalition of low-balance holders from passing proposals.
            if (locals.totalVotes < (sint64)MIN_SHARES_QUORUM ||
                locals.voterCountPerProposal.get(locals.i) < locals.minVoterQuorumSnap)
            {
                locals.prop.status = PROP_STATUS_FAILED;
                // 69% refund to proposer
                if (locals.prop.feePaid > 0 && !(locals.prop.proposer == NULL_ID))
                {
                    locals.refundAmt = (sint64)div((uint64)locals.prop.feePaid * 69ULL, (uint64)100);
                    if (locals.refundAmt > 0 && locals.refundAmt <= locals.availableBalance)
                    {
                        qpi.transfer(locals.prop.proposer, (uint64)locals.refundAmt);
                        locals.availableBalance = locals.availableBalance - locals.refundAmt;
                    }
                }
                state.mut().proposals.set(locals.i, locals.prop);
                continue;
            }

            // 2/3 supermajority of votes cast: votesYes * 3 >= totalVotes * 2
            if (locals.prop.votesYes * 3 >= locals.totalVotes * 2)
            {
                // Depositor veto: re-validate each NO vote against current locked value before counting
                locals.dvNoCount = 0;
                for (locals.k = 0; locals.k < (uint64)state.get().depositorCount; locals.k = locals.k + 1)
                {
                    locals.currentVoter = state.get().depositorList.get(locals.k);
                    locals.voteSlotBits.setAll(0); // prevent prior depositor's veto bits from carrying into this iteration on a map miss
                    state.get().depositorVoteMap.get(locals.currentVoter, locals.voteSlotBits);
                    if (locals.voteSlotBits.get((uint8)(32 + locals.i)) == 1)
                    {
                        locals.depInfo.shares = 0; // prevent stale share count if the depositor record misses
                        state.get().depositorInfo.get(locals.currentVoter, locals.depInfo);
                        locals.currentVoterBalance = locals.depInfo.shares;
                        locals.currentVoterBalance = (sint64)((uint64)locals.currentVoterBalance * (uint64)state.get().vaultSharePrice);
                        if (locals.currentVoterBalance >= locals.depositorVoteMinSnap)
                        {
                            locals.dvNoCount = locals.dvNoCount + 1;
                        }
                    }
                }
                if (locals.dvNoCount >= DEPOSITOR_VETO_THRESHOLD)
                {
                    locals.prop.status = PROP_STATUS_FAILED;
                    if (locals.prop.feePaid > 0 && !(locals.prop.proposer == NULL_ID))
                    {
                        locals.refundAmt = (sint64)div((uint64)locals.prop.feePaid * 69ULL, (uint64)100);
                        if (locals.refundAmt > 0 && locals.refundAmt <= locals.availableBalance)
                        {
                            qpi.transfer(locals.prop.proposer, (uint64)locals.refundAmt);
                            locals.availableBalance = locals.availableBalance - locals.refundAmt;
                        }
                    }
                    state.mut().proposals.set(locals.i, locals.prop);
                    continue;
                }

                locals.prop.status = PROP_STATUS_PASSED;

                // Passed → the fee is non-refundable; send the held 69% to the execution-fee reserve
                // (the 31% non-refundable share already went there at submission, so 100% of a passed
                // proposal's fee now funds execution). Guarded by the running balance like the refunds.
                locals.execBurnAmt = locals.prop.feePaid - (sint64)div((uint64)locals.prop.feePaid * 31ULL, (uint64)100);
                if (locals.execBurnAmt > 0 && locals.execBurnAmt <= locals.availableBalance)
                {
                    qpi.burn((uint64)locals.execBurnAmt);
                    locals.availableBalance = locals.availableBalance - locals.execBurnAmt;
                }

                if (locals.prop.proposalType == PROP_TYPE_ADD_POOL)
                {
                    // Duplicate check and pool cap
                    locals.found = 0;
                    for (locals.j = 0; locals.j < (uint64)state.get().poolCount; locals.j = locals.j + 1)
                    {
                        if (state.get().poolAssetNames.get(locals.j) == locals.prop.assetName &&
                            state.get().poolIssuers.get(locals.j)    == locals.prop.assetIssuer)
                        {
                            locals.found = 1;
                            break;
                        }
                    }
                    if (!locals.found && state.get().poolCount < 255)
                    {
                        state.mut().poolAssetNames.set(state.get().poolCount, locals.prop.assetName);
                        state.mut().poolIssuers.set(state.get().poolCount, locals.prop.assetIssuer);
                        state.mut().poolActive.set(state.get().poolCount, 1);
                        state.mut().poolCount = state.get().poolCount + 1;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_REMOVE_POOL)
                {
                    if (locals.prop.poolIndex < (uint64)state.get().poolCount)
                    {
                        state.mut().poolActive.set(locals.prop.poolIndex, 0);
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_REACTIVATE_POOL)
                {
                    if (locals.prop.poolIndex < (uint64)state.get().poolCount)
                    {
                        state.mut().poolActive.set(locals.prop.poolIndex, 1);
                        // Clear any stale Dagger cooldown — a pool reactivated after a long absence
                        // may have had a 5-week unaffordable cooldown set before removal. Without
                        // this reset, the pool would silently sit locked for the remainder of that
                        // cooldown with no visible indication to operators.
                        state.mut().poolCooldownTick.set(locals.prop.poolIndex, 0);
                        state.mut().poolCooldownTickA.set(locals.prop.poolIndex, 0);
                        // Reset Cloak price history so stale samples from before the inactive
                        // period do not distort the 3-month average. Without this, a pool that
                        // was removed during a price spike would immediately trigger a Cloak buy
                        // on reactivation because the 3-month avg is inflated by pre-removal data.
                        // Zeroing swingPriceCount causes the entry-path guard
                        //   (swingPriceCount < 2) to skip this pool until two fresh epochs are sampled.
                        state.mut().swingPriceHead.set(locals.prop.poolIndex, 0);
                        state.mut().swingPriceCount.set(locals.prop.poolIndex, 0);
                        // Reset VIX too — a stale baseline price from before removal would otherwise
                        // register as a huge artificial "move" on the first post-reactivation sample
                        // and trip a false breakout. Zeroing vixSampleCount re-seeds the baseline.
                        state.mut().vixSampleCount.set(locals.prop.poolIndex, 0);
                        state.mut().vixFast.set(locals.prop.poolIndex, 0);
                        state.mut().vixSlow.set(locals.prop.poolIndex, 0);
                        state.mut().vixLastPrice.set(locals.prop.poolIndex, 0);
                        state.mut().vixBreakout.set(locals.prop.poolIndex, 0);
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_MIN_PROFIT)
                {
                    if (locals.prop.newValue == MIN_PROFIT_QU_OPT1 || locals.prop.newValue == MIN_PROFIT_QU_OPT2 ||
                        locals.prop.newValue == MIN_PROFIT_QU_OPT3 || locals.prop.newValue == MIN_PROFIT_QU_OPT4)
                    {
                        state.mut().minProfitQu = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_WITHDRAW_QU_RESERVE)
                {
                    if (locals.prop.withdrawAmount > 0 &&
                        locals.prop.withdrawAmount <= state.get().quReserve &&
                        locals.prop.withdrawAmount <= locals.availableBalance)
                    {
                        qpi.transfer(locals.prop.destination, (uint64)locals.prop.withdrawAmount);
                        state.mut().quReserve = state.get().quReserve - locals.prop.withdrawAmount;
                        locals.availableBalance = locals.availableBalance - locals.prop.withdrawAmount;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_PROPOSAL_FEE)
                {
                    if (locals.prop.newValue > 0)
                    {
                        state.mut().proposalFeeDefault = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_PAYOUT)
                {
                    if (locals.prop.newValue >= 0 && locals.prop.newValue <= 3) // 3 = recovery preset
                    {
                        state.mut().payoutStructure = (uint8)locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_FEE_ADD_POOL)
                {
                    if (locals.prop.newValue > 0)
                    {
                        state.mut().proposalFeeAddPool = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_FEE_PAYOUT)
                {
                    if (locals.prop.newValue > 0)
                    {
                        state.mut().proposalFeePayoutStructure = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_MIN_QUORUM)
                {
                    if (locals.prop.newValue >= (sint64)INITIAL_MIN_VOTER_QUORUM &&
                        locals.prop.newValue <= (sint64)MAX_VOTER_QUORUM)
                    {
                        state.mut().minVoterQuorum = (uint16)locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_VAULT_TIER)
                {
                    if (locals.prop.newValue >= 0 && locals.prop.newValue <= 8)
                    {
                        state.mut().vaultDepositTier = (uint8)locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT)
                {
                    if (locals.prop.newValue == 2 || locals.prop.newValue == 5 ||
                        locals.prop.newValue == 7 || locals.prop.newValue == 10)
                    {
                        state.mut().minReserveProfitPct = (uint8)locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN)
                {
                    if (locals.prop.newValue == 50000000LL  || locals.prop.newValue == 150000000LL ||
                        locals.prop.newValue == 250000000LL || locals.prop.newValue == 350000000LL)
                    {
                        state.mut().depositorVoteMinQu = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_RELOCK_AMOUNT)
                {
                    if (locals.prop.newValue == 1000000LL  || locals.prop.newValue == 5000000LL  ||
                        locals.prop.newValue == 10000000LL || locals.prop.newValue == 20000000LL ||
                        locals.prop.newValue == 25000000LL || locals.prop.newValue == 50000000LL)
                    {
                        state.mut().relockAddAmount = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR)
                {
                    if (locals.prop.newValue == 0LL           || locals.prop.newValue == 1000000000LL  ||
                        locals.prop.newValue == 5000000000LL  || locals.prop.newValue == 10000000000LL ||
                        locals.prop.newValue == 20000000000LL)
                    {
                        state.mut().execReserveFloor = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_VIX_FACTOR)
                {
                    if (locals.prop.newValue == 9LL   || locals.prop.newValue == 18LL  || locals.prop.newValue == 37LL  ||
                        locals.prop.newValue == 75LL  || locals.prop.newValue == 150LL || locals.prop.newValue == 200LL ||
                        locals.prop.newValue == 225LL || locals.prop.newValue == 275LL || locals.prop.newValue == 350LL ||
                        locals.prop.newValue == 450LL || locals.prop.newValue == 500LL)
                    {
                        state.mut().vixBreakoutFactor = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_VIX_FLOOR)
                {
                    if (locals.prop.newValue == 0LL   || locals.prop.newValue == 10LL  ||
                        locals.prop.newValue == 25LL  || locals.prop.newValue == 50LL  ||
                        locals.prop.newValue == 100LL || locals.prop.newValue == 200LL)
                    {
                        state.mut().vixAbsFloorBps = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_VIX_PULSE_RATE)
                {
                    if (locals.prop.newValue == 1LL || locals.prop.newValue == 2LL || locals.prop.newValue == 3LL)
                    {
                        // store the interval in ticks (1/day=345600, 2/day=172800, 3/day=115200)
                        state.mut().vixSampleInterval = (uint32)div((uint64)VIX_DAY_TICKS, (uint64)locals.prop.newValue);
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_SWING_SELL_PCT)
                {
                    if (locals.prop.newValue == 10LL || locals.prop.newValue == 15LL || locals.prop.newValue == 20LL ||
                        locals.prop.newValue == 25LL || locals.prop.newValue == 33LL || locals.prop.newValue == 50LL)
                    {
                        state.mut().swingSellPct = locals.prop.newValue;
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_UPDATE_BREAKOUT_RESCAN)
                {
                    if (locals.prop.newValue == 30LL  || locals.prop.newValue == 60LL  || locals.prop.newValue == 120LL ||
                        locals.prop.newValue == 180LL || locals.prop.newValue == 240LL || locals.prop.newValue == 300LL)
                    {
                        // newValue is seconds → store as ticks (4 ticks/sec)
                        state.mut().breakoutRescanTicks = (uint32)((uint64)locals.prop.newValue * 4ULL);
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_WITHDRAW_ASSET_RESERVE)
                {
                    if (locals.prop.poolIndex < (uint64)state.get().poolCount &&
                        !state.get().poolActive.get(locals.prop.poolIndex))
                    {
                        locals.withdrawAsset.issuer    = state.get().poolIssuers.get(locals.prop.poolIndex);
                        locals.withdrawAsset.assetName = state.get().poolAssetNames.get(locals.prop.poolIndex);
                        locals.actualAssetBalance = qpi.numberOfShares(
                            locals.withdrawAsset,
                            AssetOwnershipSelect{ id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), CLKnDGR_CONTRACT_INDEX },
                            AssetPossessionSelect{ id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), CLKnDGR_CONTRACT_INDEX }
                        );
                        if (locals.actualAssetBalance > 0)
                        {
                            // transferShareOwnershipAndPossession returns remaining possessed shares >= 0
                            // on success, or a negative value on failure. Only clear reserve/recovery
                            // state if the transfer actually succeeded; otherwise leave state intact so
                            // a subsequent governance proposal can retry the sweep.
                            locals.transferResult = qpi.transferShareOwnershipAndPossession(
                                state.get().poolAssetNames.get(locals.prop.poolIndex),
                                state.get().poolIssuers.get(locals.prop.poolIndex),
                                id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                                id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                                locals.actualAssetBalance,
                                locals.prop.destination
                            );
                            if (locals.transferResult >= 0)
                            {
                                state.mut().poolReserveTokens.set(locals.prop.poolIndex, 0);
                                state.mut().poolReserveCostBasis.set(locals.prop.poolIndex, 0);
                                // Clear recovery tracking only when recovery is already complete.
                                // If tokens are still stuck under DEX managing rights (pendingRecoveryAmt > 0),
                                // leave the recovery fields intact so the recovery pass can surface those tokens
                                // and a future WITHDRAW_ASSET_RESERVE proposal can sweep them.
                                // If recovery is complete (pendingRecoveryAmt == 0), also clear source/cost to
                                // prevent a stale source flag from incorrectly triggering the recovery pass.
                                if (state.get().poolPendingRecoveryAmt.get(locals.prop.poolIndex) == 0)
                                {
                                    state.mut().poolPendingRecoverySource.set(locals.prop.poolIndex, 0);
                                    state.mut().poolPendingRecoveryCostBasis.set(locals.prop.poolIndex, 0);
                                }
                                // Clear Cloak swing position — those tokens were swept above.
                                // Mirror the Dagger pattern: only clear pending recovery fields when
                                // no recovery is outstanding. If swingPendingRecoveryAmt > 0, tokens
                                // are still under Qswap managing rights and were NOT included in the
                                // transfer above. Leaving the fields intact lets the Cloak recovery
                                // pass surface them via TSRM; a subsequent WITHDRAW_ASSET_RESERVE
                                // proposal can then sweep the recovered balance.
                                state.mut().swingTokens.set(locals.prop.poolIndex, 0);
                                state.mut().swingCostBasis.set(locals.prop.poolIndex, 0);
                                if (state.get().swingPendingRecoveryAmt.get(locals.prop.poolIndex) == 0)
                                {
                                    state.mut().swingPendingRecoveryCost.set(locals.prop.poolIndex, 0);
                                }
                                // swingPendingRecoveryAmt left intact when non-zero.
                            }
                        }
                    }
                }
                else if (locals.prop.proposalType == PROP_TYPE_SELL_POOL_TOKENS)
                {
                    // Governance "sell for profit": market-sell sellPct% of this pool's token
                    // holdings on Qswap for QU. Unlike WITHDRAW_ASSET_RESERVE (which ships raw
                    // tokens to an address — depositors gain nothing), the QU proceeds land in
                    // reserveSellProceeds and are folded into the next epoch's profit split, so
                    // BOTH vault depositors (via NAV) and shareholders (via the dist slice) benefit.
                    // Skip if a recovery is already in flight on this pool: some tokens may be under
                    // DEX managing rights, so a full-amount releaseShares would fail. Shareholders
                    // can re-propose next epoch once the BEGIN_TICK recovery pass clears it.
                    if (locals.prop.poolIndex < (uint64)state.get().poolCount &&
                        state.get().poolPendingRecoverySource.get(locals.prop.poolIndex) == 0 &&
                        state.get().swingPendingRecoveryAmt.get(locals.prop.poolIndex) == 0)
                    {
                        locals.sellPct        = locals.prop.newValue; // validated 1..100 at submit
                        locals.sellSwingAmt   = (sint64)div((uint64)state.get().swingTokens.get(locals.prop.poolIndex)       * (uint64)locals.sellPct, (uint64)100);
                        locals.sellReserveAmt = (sint64)div((uint64)state.get().poolReserveTokens.get(locals.prop.poolIndex) * (uint64)locals.sellPct, (uint64)100);
                        locals.sellTotalAmt   = locals.sellSwingAmt + locals.sellReserveAmt;
                        if (locals.sellTotalAmt > 0)
                        {
                            locals.withdrawAsset.issuer    = state.get().poolIssuers.get(locals.prop.poolIndex);
                            locals.withdrawAsset.assetName = state.get().poolAssetNames.get(locals.prop.poolIndex);
                            // Defensive: never release more than CLKnDGR actually owns+manages on-chain.
                            locals.actualAssetBalance = qpi.numberOfShares(
                                locals.withdrawAsset,
                                AssetOwnershipSelect{ id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), CLKnDGR_CONTRACT_INDEX },
                                AssetPossessionSelect{ id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), CLKnDGR_CONTRACT_INDEX }
                            );
                            if (locals.actualAssetBalance >= locals.sellTotalAmt)
                            {
                                // Hand Qswap managing rights (fee 0). releaseShares returns the paid
                                // fee (0 here) on success, < 0 on failure — check < 0, not <= 0.
                                locals.transferResult = qpi.releaseShares(
                                    locals.withdrawAsset,
                                    id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                                    id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                                    locals.sellTotalAmt,
                                    QSWAP_CONTRACT_INDEX, QSWAP_CONTRACT_INDEX,
                                    0
                                );
                                if (locals.transferResult >= 0)
                                {
                                    // Quote the swap, then require >= quote - 10% as the fill floor.
                                    // The quote already prices the real AMM impact of this size; the
                                    // 10% floor only guards atomic fill-vs-quote deviation, so a
                                    // catastrophic fill reverts (tokens recovered) instead of dumping.
                                    locals.sellQaIn.assetIssuer   = locals.withdrawAsset.issuer;
                                    locals.sellQaIn.assetName     = locals.withdrawAsset.assetName;
                                    locals.sellQaIn.assetAmountIn = locals.sellTotalAmt;
                                    { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactAssetInput, locals.sellQaIn, locals.sellQaOut); }

                                    locals.sellSwapIn.assetIssuer    = locals.withdrawAsset.issuer;
                                    locals.sellSwapIn.assetName      = locals.withdrawAsset.assetName;
                                    locals.sellSwapIn.assetAmountIn  = locals.sellTotalAmt;
                                    // quAmountOut - 10% via subtraction of (quote/10): overflow-safe, no ×10.
                                    locals.sellSwapIn.quAmountOutMin = locals.sellQaOut.quAmountOut
                                        - (sint64)div((uint64)locals.sellQaOut.quAmountOut, (uint64)10);
                                    { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, SwapExactAssetForQu, locals.sellSwapIn, locals.sellSwapOut, 0); }

                                    // Tokens left both buckets either way (sold for QU, or now stuck under
                                    // Qswap rights awaiting recovery). Reduce each bucket and its cost basis
                                    // proportionally (divide-first, overflow-safe — same form as reserve sell).
                                    locals.sellSwingCost = 0;
                                    if (locals.sellSwingAmt > 0 && state.get().swingTokens.get(locals.prop.poolIndex) > 0)
                                    {
                                        locals.sellPerToken  = (sint64)div((uint64)state.get().swingCostBasis.get(locals.prop.poolIndex), (uint64)state.get().swingTokens.get(locals.prop.poolIndex));
                                        locals.sellSwingCost = locals.sellPerToken * locals.sellSwingAmt;
                                        locals.sellNewTokens = state.get().swingTokens.get(locals.prop.poolIndex) - locals.sellSwingAmt;
                                        if (locals.sellNewTokens < 0) { locals.sellNewTokens = 0; }
                                        state.mut().swingTokens.set(locals.prop.poolIndex, locals.sellNewTokens);
                                        state.mut().swingCostBasis.set(locals.prop.poolIndex, locals.sellPerToken * locals.sellNewTokens);
                                    }
                                    locals.sellReserveCost = 0;
                                    if (locals.sellReserveAmt > 0 && state.get().poolReserveTokens.get(locals.prop.poolIndex) > 0)
                                    {
                                        locals.sellPerToken    = (sint64)div((uint64)state.get().poolReserveCostBasis.get(locals.prop.poolIndex), (uint64)state.get().poolReserveTokens.get(locals.prop.poolIndex));
                                        locals.sellReserveCost = locals.sellPerToken * locals.sellReserveAmt;
                                        locals.sellNewTokens   = state.get().poolReserveTokens.get(locals.prop.poolIndex) - locals.sellReserveAmt;
                                        if (locals.sellNewTokens < 0) { locals.sellNewTokens = 0; }
                                        state.mut().poolReserveTokens.set(locals.prop.poolIndex, locals.sellNewTokens);
                                        state.mut().poolReserveCostBasis.set(locals.prop.poolIndex, locals.sellPerToken * locals.sellNewTokens);
                                    }

                                    if (locals.sellSwapOut.quAmountOut > 0)
                                    {
                                        // Success: route QU to reserveSellProceeds (folded into epochProfit
                                        // at the next BEGIN_EPOCH, after NAV — same path as the reserve sell).
                                        state.mut().reserveSellProceeds = state.get().reserveSellProceeds + locals.sellSwapOut.quAmountOut;
                                        state.mut().totalProfitEarned   = state.get().totalProfitEarned   + locals.sellSwapOut.quAmountOut;
                                    }
                                    else
                                    {
                                        // Swap reverted (only if the pool is degenerate, since the quote was
                                        // taken atomically the same step): tokens are owned by CLKnDGR but stuck
                                        // under Qswap managing rights. Schedule TSRM recovery (source 1) — the
                                        // BEGIN_TICK recovery pass folds them back into poolReserveTokens next
                                        // tick. Guarded above so we never stomp an in-flight recovery.
                                        state.mut().poolPendingRecoveryAmt.set(locals.prop.poolIndex, locals.sellTotalAmt);
                                        state.mut().poolPendingRecoverySource.set(locals.prop.poolIndex, 1);
                                        state.mut().poolPendingRecoveryCostBasis.set(locals.prop.poolIndex, locals.sellSwingCost + locals.sellReserveCost);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                locals.prop.status = PROP_STATUS_FAILED;
                // 69% refund to proposer
                if (locals.prop.feePaid > 0 && !(locals.prop.proposer == NULL_ID))
                {
                    locals.refundAmt = (sint64)div((uint64)locals.prop.feePaid * 69ULL, (uint64)100);
                    if (locals.refundAmt > 0 && locals.refundAmt <= locals.availableBalance)
                    {
                        qpi.transfer(locals.prop.proposer, (uint64)locals.refundAmt);
                        locals.availableBalance = locals.availableBalance - locals.refundAmt;
                    }
                }
            }

            state.mut().proposals.set(locals.i, locals.prop);
        }
    }

    // ---------------------------------------------------------------
    // Tick hook — scan registered pools and execute profitable arbs
    // ---------------------------------------------------------------
    BEGIN_TICK_WITH_LOCALS()
    {
        qpi.getEntity(SELF, locals.entity);
        state.mut().quBalance = locals.entity.incomingAmount - locals.entity.outgoingAmount;

        // Subtract earmarked reserves from the available trading balance.
        // quReserve (dev fund) and qearnReserve (Qearn accumulator) are virtual accounting
        // counters — they sit in the same on-chain balance and would otherwise be silently
        // consumed by the arb loop.
        locals.tradingBalance = state.get().quBalance - state.get().quReserve - state.get().qearnReserve - state.get().waitlistQu;
        if (locals.tradingBalance < 0) { locals.tradingBalance = 0; }
        // Snapshot before any Cloak or Dagger deductions. Used by the Dagger unaffordable-cooldown
        // check to distinguish "vault is underfunded" (5-week cooldown) from "Cloak spent it this
        // tick" (no cooldown penalty — try again next tick).
        locals.initialTradingBalance = locals.tradingBalance;

        if (state.get().poolCount == 0)                          { return; }

        // NOTE: the fee-reserve safety valve does NOT pause trading. The contract keeps trading
        // normally in every tick; when the execution-fee reserve is low it instead routes 100% of its
        // profit to the reserve via the RECOVERY preset at BEGIN_EPOCH (see the dispatch there), so it
        // rebuilds the fee budget from its own earnings rather than by sitting idle.

        // ---------------------------------------------------------------
        // Recovery pass: retry TSRM for pools where Leg-2 failed last tick.
        // Shares are owned by CLKnDGR but managed by the DEX that ran Leg 1.
        // On success, fold recovered shares into the pool's reserve so the
        // 4-epoch burn/sell action can eventually liquidate them.
        // Cost basis is tracked in poolPendingRecoveryCostBasis and applied proportionally on partial recovery.
        // ---------------------------------------------------------------
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (state.get().poolPendingRecoverySource.get(locals.i) == 0) { continue; }
            if (state.get().poolPendingRecoveryAmt.get(locals.i) <= 0)
            {
                state.mut().poolPendingRecoverySource.set(locals.i, 0);
                continue;
            }

            locals.recoveredAmt = 0;

            if (state.get().poolPendingRecoverySource.get(locals.i) == 1) // stuck under Qswap
            {
                locals.swapTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
                locals.swapTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
                locals.swapTsrmIn.numberOfShares           = state.get().poolPendingRecoveryAmt.get(locals.i);
                locals.swapTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, TransferShareManagementRights, locals.swapTsrmIn, locals.swapTsrmOut, 0); }
                locals.recoveredAmt = locals.swapTsrmOut.transferredNumberOfShares;
            }
            else // stuck under QX (source == 2)
            {
                locals.qxTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
                locals.qxTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
                locals.qxTsrmIn.numberOfShares           = state.get().poolPendingRecoveryAmt.get(locals.i);
                locals.qxTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, TransferShareManagementRights, locals.qxTsrmIn, locals.qxTsrmOut, 0); }
                locals.recoveredAmt = locals.qxTsrmOut.transferredNumberOfShares;
            }

            if (locals.recoveredAmt > 0)
            {
                // Apply only the proportional fraction of the stored cost basis.
                // Full recovery uses the exact stored value to avoid rounding loss.
                // Partial recovery divides first then multiplies to prevent uint64 overflow
                // when costBasis and recoveredAmt are both large (costBasis * recoveredAmt
                // can exceed 2^64 for high-supply low-price assets).
                if (locals.recoveredAmt >= state.get().poolPendingRecoveryAmt.get(locals.i))
                {
                    locals.recoveredCost = state.get().poolPendingRecoveryCostBasis.get(locals.i);
                }
                else
                {
                    locals.recoveredCost = (state.get().poolPendingRecoveryAmt.get(locals.i) > 0)
                        ? (sint64)(div((uint64)state.get().poolPendingRecoveryCostBasis.get(locals.i),
                                       (uint64)state.get().poolPendingRecoveryAmt.get(locals.i)) * (uint64)locals.recoveredAmt)
                        : 0;
                }

                state.mut().poolReserveTokens.set(locals.i,
                    state.get().poolReserveTokens.get(locals.i) + locals.recoveredAmt);
                state.mut().poolReserveCostBasis.set(locals.i,
                    state.get().poolReserveCostBasis.get(locals.i) + locals.recoveredCost);

                if (locals.recoveredAmt >= state.get().poolPendingRecoveryAmt.get(locals.i))
                {
                    // Full recovery: clear all pending fields.
                    state.mut().poolPendingRecoveryAmt.set(locals.i, 0);
                    state.mut().poolPendingRecoverySource.set(locals.i, 0);
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i, 0);
                }
                else
                {
                    // Partial recovery: reduce pending fields by the recovered portion; retry next tick for remainder.
                    state.mut().poolPendingRecoveryAmt.set(locals.i,
                        state.get().poolPendingRecoveryAmt.get(locals.i) - locals.recoveredAmt);
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i,
                        state.get().poolPendingRecoveryCostBasis.get(locals.i) - locals.recoveredCost);
                }
            }
            // If recovery still fails (recoveredAmt == 0), source remains set — will retry next tick.
        }

        // ==============================================================
        // THE CLOAK — swing trade strategy (runs before the dagger)
        // Entry: 1-week avg price <= 3-month avg × 90% → buy 1% of trading capital on Qswap
        // Exit:  pool price >= cost per token × 112% → sell 20% of position on QX
        // Cooldown: 25 hours after any buy or sell action
        // ==============================================================

        // --- Cloak recovery pass: retry TSRM for swing positions where rights transfer failed ---
        // source=0: ENTRY path — tokens stuck under Qswap after a buy TSRM failure.
        //           On success: returned to swingTokens (restore swing position).
        // source=2: EXIT path — tokens stuck under QX after AddToAskOrder placed 0 shares
        //           AND the inline QX TSRM also failed. On success: folded into poolReserve.
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (state.get().swingPendingRecoveryAmt.get(locals.i) <= 0) { continue; }

            locals.swingRecoveredAmt = 0;
            if (state.get().swingPendingRecoverySource.get(locals.i) == 2) // EXIT path: stuck under QX
            {
                locals.qxTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
                locals.qxTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
                locals.qxTsrmIn.numberOfShares           = state.get().swingPendingRecoveryAmt.get(locals.i);
                locals.qxTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, TransferShareManagementRights, locals.qxTsrmIn, locals.qxTsrmOut, 0); }
                locals.swingRecoveredAmt = locals.qxTsrmOut.transferredNumberOfShares;
            }
            else // source=0: ENTRY path (default), stuck under Qswap
            {
                locals.swingTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
                locals.swingTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
                locals.swingTsrmIn.numberOfShares           = state.get().swingPendingRecoveryAmt.get(locals.i);
                locals.swingTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, TransferShareManagementRights, locals.swingTsrmIn, locals.swingTsrmOut, 0); }
                locals.swingRecoveredAmt = locals.swingTsrmOut.transferredNumberOfShares;
            }

            if (locals.swingRecoveredAmt > 0)
            {
                // Same divide-first pattern as Dagger recovery: avoids uint64 overflow on
                // costBasis * recoveredAmt for high-supply low-price assets.
                if (locals.swingRecoveredAmt >= state.get().swingPendingRecoveryAmt.get(locals.i))
                {
                    locals.swingRecoveredCost = state.get().swingPendingRecoveryCost.get(locals.i);
                }
                else
                {
                    locals.swingRecoveredCost = (state.get().swingPendingRecoveryAmt.get(locals.i) > 0)
                        ? (sint64)(div((uint64)state.get().swingPendingRecoveryCost.get(locals.i),
                                       (uint64)state.get().swingPendingRecoveryAmt.get(locals.i)) * (uint64)locals.swingRecoveredAmt)
                        : 0;
                }

                if (state.get().swingPendingRecoverySource.get(locals.i) == 2)
                {
                    // EXIT recovery: swingTokens was already decremented when the failed ask
                    // was processed. Fold recovered tokens into pool reserve for the 4-epoch
                    // burn/sell cycle rather than re-crediting a position we already closed.
                    state.mut().poolReserveTokens.set(locals.i,
                        state.get().poolReserveTokens.get(locals.i) + locals.swingRecoveredAmt);
                    state.mut().poolReserveCostBasis.set(locals.i,
                        state.get().poolReserveCostBasis.get(locals.i) + locals.swingRecoveredCost);
                }
                else
                {
                    // ENTRY recovery: return tokens to the swing position they came from.
                    state.mut().swingTokens.set(locals.i,
                        state.get().swingTokens.get(locals.i) + locals.swingRecoveredAmt);
                    state.mut().swingCostBasis.set(locals.i,
                        state.get().swingCostBasis.get(locals.i) + locals.swingRecoveredCost);
                }

                if (locals.swingRecoveredAmt >= state.get().swingPendingRecoveryAmt.get(locals.i))
                {
                    state.mut().swingPendingRecoveryAmt.set(locals.i,    0);
                    state.mut().swingPendingRecoveryCost.set(locals.i,   0);
                    state.mut().swingPendingRecoverySource.set(locals.i, 0);
                }
                else
                {
                    state.mut().swingPendingRecoveryAmt.set(locals.i,
                        state.get().swingPendingRecoveryAmt.get(locals.i) - locals.swingRecoveredAmt);
                    state.mut().swingPendingRecoveryCost.set(locals.i,
                        state.get().swingPendingRecoveryCost.get(locals.i) - locals.swingRecoveredCost);
                }
            }
            // recoveredAmt == 0: fields unchanged, retries next tick
        }

        // --- Cloak main loop ---
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (!state.get().poolActive.get(locals.i)) { continue; }
            // Cooldown check (wraparound-safe)
            if ((sint32)(state.get().swingCooldownTick.get(locals.i) - qpi.tick()) > 0) { continue; }
            // Skip while a buy recovery is still pending — don't open another position
            if (state.get().swingPendingRecoveryAmt.get(locals.i) > 0) { continue; }
            // Need at least 2 epoch samples to compute both averages
            if (state.get().swingPriceCount.get(locals.i) < 2) { continue; }

            // Monthly cadence: bump this pool's cooldown so it is re-evaluated about once a month,
            // whether or not it acts (idle / first buy / DCA-add / sell). One timer for the strategy.
            state.mut().swingCooldownTick.set(locals.i, qpi.tick() + SWING_COOLDOWN_TICKS);

            // Is token A in a dip? (1-week avg <= 3-month avg - SWING_BUY_DIP_PCT%). Computed from the
            // FREE stored price history — used by BOTH the first buy (not holding) and the DCA-in add
            // (holding). No market call, so idle pools cost almost nothing.
            locals.swingHistCount = state.get().swingPriceCount.get(locals.i);
            locals.swingSlot = (uint8)(mod((uint64)state.get().swingPriceHead.get(locals.i) + (uint64)SWING_PRICE_SLOTS - 1, (uint64)SWING_PRICE_SLOTS));
            locals.swingOneWeekPrice = state.get().swingPriceHistory.get(
                (uint64)locals.i * (uint64)SWING_PRICE_SLOTS + (uint64)locals.swingSlot);
            locals.swingThreeMonthSum = 0;
            for (locals.swingSlot = 0; locals.swingSlot < locals.swingHistCount; locals.swingSlot = locals.swingSlot + 1)
            {
                locals.swingThreeMonthSum = locals.swingThreeMonthSum +
                    state.get().swingPriceHistory.get(
                        (uint64)locals.i * (uint64)SWING_PRICE_SLOTS + (uint64)locals.swingSlot);
            }
            locals.swingThreeMonthAvg = (locals.swingHistCount > 0)
                ? (sint64)div((uint64)locals.swingThreeMonthSum, (uint64)locals.swingHistCount)
                : 0;
            // EC-C12: subtractive form (× SWING_BUY_DIP_PCT) avoids the × (100 - dip) overflow class.
            locals.swingIsDip = (locals.swingThreeMonthAvg > 0 && locals.swingOneWeekPrice > 0 &&
                locals.swingOneWeekPrice <= locals.swingThreeMonthAvg -
                    (sint64)div((uint64)locals.swingThreeMonthAvg * (uint64)SWING_BUY_DIP_PCT, (uint64)100))
                ? 1 : 0;

            locals.swingBuyQu = 0; // set below to 1% (first buy) or 0.25% (DCA-in add) if we decide to buy

            // ==========================================================
            // EXIT path: sell check when holding a swing position
            // ==========================================================
            // Cost-saving: ask Qswap for the live price ONLY for pools we actually hold (it is needed
            // only for the sell trigger). Idle pools already decided the dip from free history above.
            if (state.get().swingTokens.get(locals.i) > 0)
            {
                // Read current Qswap pool price (needed only here, for the sell decision)
                locals.swingPoolIn.assetIssuer = state.get().poolIssuers.get(locals.i);
                locals.swingPoolIn.assetName   = state.get().poolAssetNames.get(locals.i);
                { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, GetPoolBasicState, locals.swingPoolIn, locals.swingPoolOut); }
                if (!locals.swingPoolOut.poolExists)              { continue; }
                if (locals.swingPoolOut.reservedAssetAmount <= 0) { continue; }
                locals.swingPoolPrice = (sint64)div(
                    (uint64)locals.swingPoolOut.reservedQuAmount,
                    (uint64)locals.swingPoolOut.reservedAssetAmount);
                if (locals.swingPoolPrice <= 0) { continue; }

                locals.swingCostPerToken = (sint64)div(
                    (uint64)state.get().swingCostBasis.get(locals.i),
                    (uint64)state.get().swingTokens.get(locals.i));

                // Sell trigger: pool price >= cost per token × 112%
                // EC-C7: additive form avoids × 112 overflow (same class as EC-3)
                if (locals.swingPoolPrice >= locals.swingCostPerToken +
                        (sint64)div((uint64)locals.swingCostPerToken * (uint64)SWING_SELL_GAIN_PCT, (uint64)100))
                {
                    locals.swingSellAmt = (sint64)div(
                        (uint64)state.get().swingTokens.get(locals.i) * (uint64)state.get().swingSellPct,
                        (uint64)100);
                    if (locals.swingSellAmt <= 0) { locals.swingSellAmt = 1; } // floor: small positions still exit

                    // Ask at 10% below pool price — fills against all bids at or above this level
                    // EC-C13: subtractive form uses × SWING_ASK_DISCOUNT_PCT (×10) not × (100-10) (×90);
                    // raises overflow threshold from 2.05×10^17 to 1.84×10^18 — same class as EC-3 fixes.
                    locals.swingAskPrice = locals.swingPoolPrice -
                        (sint64)div((uint64)locals.swingPoolPrice * (uint64)SWING_ASK_DISCOUNT_PCT, (uint64)100);
                    if (locals.swingAskPrice <= 0) { continue; }

                    // Liquidity check: sum QX bid QU at prices >= askPrice
                    locals.swingBidIn.issuer    = state.get().poolIssuers.get(locals.i);
                    locals.swingBidIn.assetName = state.get().poolAssetNames.get(locals.i);
                    locals.swingBidIn.offset    = 0;
                    { CALL_OTHER_CONTRACT_FUNCTION(QX, AssetBidOrders, locals.swingBidIn, locals.swingBidOut); }

                    // Note: offset=0 fetches the first 256 bids only. If the book has >256 bids
                    // all priced above swingAskPrice, swingAvailBidQu will be understated —
                    // a conservative false-negative (may delay exit, never causes a wrong exit).
                    // desiredQu pre-computed before the loop: (a) enables early exit once threshold
                    // is met, (b) drives per-order saturation to prevent price×shares overflow.
                    locals.swingDesiredQu = (sint64)((uint64)locals.swingSellAmt * (uint64)locals.swingPoolPrice);

                    locals.swingAvailBidQu = 0;
                    for (locals.swingBidIdx = 0; locals.swingBidIdx < 256; locals.swingBidIdx = locals.swingBidIdx + 1)
                    {
                        if (locals.swingBidOut.orders.get(locals.swingBidIdx).price <= 0) { break; } // end of orders
                        if (locals.swingBidOut.orders.get(locals.swingBidIdx).price < locals.swingAskPrice) { break; } // bids sorted descending; below askPrice won't fill
                        // Per-order overflow guard: if price × shares would exceed swingDesiredQu (or
                        // overflow uint64), this single order already satisfies the threshold — cap there.
                        if (locals.swingBidOut.orders.get(locals.swingBidIdx).numberOfShares > 0 &&
                            (uint64)locals.swingBidOut.orders.get(locals.swingBidIdx).price >
                            div((uint64)locals.swingDesiredQu, (uint64)locals.swingBidOut.orders.get(locals.swingBidIdx).numberOfShares))
                        {
                            locals.swingAvailBidQu = locals.swingDesiredQu; // single order covers full need
                        }
                        else
                        {
                            locals.swingAvailBidQu = locals.swingAvailBidQu +
                                (sint64)((uint64)locals.swingBidOut.orders.get(locals.swingBidIdx).price *
                                         (uint64)locals.swingBidOut.orders.get(locals.swingBidIdx).numberOfShares);
                        }
                        if (locals.swingAvailBidQu >= locals.swingDesiredQu) { break; } // threshold met — no need to accumulate further
                    }
                    // Require 80% of desired QU available in qualifying bids
                    // EC-C14: subtractive form uses × (100-LIQUIDITY_PCT) (×20) not × LIQUIDITY_PCT (×80);
                    // raises overflow threshold from 2.30×10^17 to 9.2×10^17 — same class as EC-3 fixes.
                    if (locals.swingAvailBidQu < locals.swingDesiredQu -
                            (sint64)div((uint64)locals.swingDesiredQu * (uint64)(100 - SWING_LIQUIDITY_REQUIRED_PCT), (uint64)100))
                    {
                        continue; // insufficient liquidity — wait for next check
                    }

                    // Give QX managing rights via releaseShares (pays transfer fee)
                    { CALL_OTHER_CONTRACT_FUNCTION(QX, Fees, locals.swingFeesIn, locals.swingFeesOut); }
                    locals.swingAsset.issuer    = state.get().poolIssuers.get(locals.i);
                    locals.swingAsset.assetName = state.get().poolAssetNames.get(locals.i);
                    locals.transferResult = qpi.releaseShares(
                        locals.swingAsset,
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        locals.swingSellAmt,
                        QX_CONTRACT_INDEX, QX_CONTRACT_INDEX,
                        locals.swingFeesOut.transferFee);
                    if (locals.transferResult < 0) { continue; } // rights transfer failed — skip

                    // Place ask order on QX
                    locals.swingAskIn.issuer          = state.get().poolIssuers.get(locals.i);
                    locals.swingAskIn.assetName       = state.get().poolAssetNames.get(locals.i);
                    locals.swingAskIn.price           = locals.swingAskPrice;
                    locals.swingAskIn.numberOfShares  = locals.swingSellAmt;
                    { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, AddToAskOrder, locals.swingAskIn, locals.swingAskOut, 0); }

                    if (locals.swingAskOut.addedNumberOfShares > 0)
                    {
                        // Reduce position by confirmed placed shares; proportionally reduce cost basis
                        locals.swingNewTokens = state.get().swingTokens.get(locals.i) - locals.swingAskOut.addedNumberOfShares;
                        if (locals.swingNewTokens < 0) { locals.swingNewTokens = 0; }
                        // Save old cost basis before update (swingRecoveredCost is safe scratch here)
                        locals.swingRecoveredCost = state.get().swingCostBasis.get(locals.i);
                        // Divide-first to avoid uint64 overflow when costBasis × remainingTokens
                        // exceeds 2^64 (high-supply low-price assets). Same pattern as EC-3 recovery fix.
                        locals.swingNewCost = (locals.swingNewTokens > 0 && state.get().swingTokens.get(locals.i) > 0)
                            ? (sint64)(div((uint64)locals.swingRecoveredCost,
                                           (uint64)state.get().swingTokens.get(locals.i)) * (uint64)locals.swingNewTokens)
                            : 0;
                        state.mut().swingTokens.set(locals.i,    locals.swingNewTokens);
                        state.mut().swingCostBasis.set(locals.i, locals.swingNewCost);

                        // EC-C2: do NOT record epochProfit or totalProfitEarned here. The ask is placed
                        // but QU from the fill arrives in a future tick via qpi.contractBalance().
                        // Recording phantom proceeds now would inflate epochProfit against QU not yet
                        // in quBalance, causing BEGIN_EPOCH to over-distribute trading capital.
                        // Realized Cloak profit flows naturally through the quBalance delta each epoch.
                        state.mut().totalArbsExecuted = state.get().totalArbsExecuted + 1;
                    }
                    else
                    {
                        // AddToAskOrder placed 0 shares: swingSellAmt tokens are now under QX
                        // managing rights with no open order. The Cloak recovery pass uses Qswap
                        // TSRM only and cannot reclaim these. If no Dagger recovery is in flight,
                        // open a QX recovery entry (source=2). If one is in flight with the same
                        // source, fold additively. If Dagger holds a Qswap entry (source=1), attempt
                        // inline QX TSRM immediately rather than corrupting the source field.
                        // Divide-first: cost of stuck tokens — avoids overflow on costBasis × sellAmt.
                        locals.swingNewCost = (state.get().swingTokens.get(locals.i) > 0)
                            ? (sint64)(div((uint64)state.get().swingCostBasis.get(locals.i),
                                           (uint64)state.get().swingTokens.get(locals.i)) * (uint64)locals.swingSellAmt)
                            : 0;
                        if (state.get().poolPendingRecoveryAmt.get(locals.i) == 0)
                        {
                            // No Dagger recovery in flight — open a fresh entry.
                            state.mut().poolPendingRecoveryAmt.set(locals.i,       locals.swingSellAmt);
                            state.mut().poolPendingRecoverySource.set(locals.i,    2); // stuck under QX
                            state.mut().poolPendingRecoveryCostBasis.set(locals.i, locals.swingNewCost);
                        }
                        else if (state.get().poolPendingRecoverySource.get(locals.i) == 2)
                        {
                            // Dagger's pending entry is also source=2 (QX): safe to fold — same DEX.
                            state.mut().poolPendingRecoveryAmt.set(locals.i,
                                state.get().poolPendingRecoveryAmt.get(locals.i) + locals.swingSellAmt);
                            state.mut().poolPendingRecoveryCostBasis.set(locals.i,
                                state.get().poolPendingRecoveryCostBasis.get(locals.i) + locals.swingNewCost);
                        }
                        else
                        {
                            // Dagger's pending entry is source=1 (Qswap): cannot fold — wrong DEX.
                            // Attempt inline QX TSRM to reclaim these tokens immediately, since QX
                            // still holds managing rights from the releaseShares call above.
                            locals.qxTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
                            locals.qxTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
                            locals.qxTsrmIn.numberOfShares           = locals.swingSellAmt;
                            locals.qxTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                            { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, TransferShareManagementRights, locals.qxTsrmIn, locals.qxTsrmOut, 0); }
                            if (locals.qxTsrmOut.transferredNumberOfShares > 0)
                            {
                                // Reclaimed — fold directly into pool reserve; no recovery entry needed.
                                // Divide-first: prevents uint64 overflow when swingNewCost × transferredShares
                                // exceeds 2^64 on high-value moderate-supply assets (EC-C3 / EC-3 class fix).
                                locals.swingRecoveredCost = (locals.swingSellAmt > 0)
                                    ? (sint64)(div((uint64)locals.swingNewCost,
                                                   (uint64)locals.swingSellAmt) * (uint64)locals.qxTsrmOut.transferredNumberOfShares)
                                    : 0;
                                state.mut().poolReserveTokens.set(locals.i,
                                    state.get().poolReserveTokens.get(locals.i) + locals.qxTsrmOut.transferredNumberOfShares);
                                state.mut().poolReserveCostBasis.set(locals.i,
                                    state.get().poolReserveCostBasis.get(locals.i) + locals.swingRecoveredCost);
                            }
                            else
                            {
                                // Inline TSRM failed: tokens remain under QX with no open order.
                                // Dagger's poolPendingRecovery slot is occupied with source=1 (Qswap),
                                // so we cannot fold into that slot. Use Cloak's own swing recovery
                                // slot (source=2/QX). swingPendingRecoveryAmt is guaranteed to be 0
                                // here — the Cloak main loop (line above) skips this pool otherwise.
                                // The Cloak recovery pass will retry QX TSRM next tick and fold
                                // recovered tokens into poolReserve on success.
                                state.mut().swingPendingRecoveryAmt.set(locals.i,    locals.swingSellAmt);
                                state.mut().swingPendingRecoveryCost.set(locals.i,   locals.swingNewCost);
                                state.mut().swingPendingRecoverySource.set(locals.i, 2); // 2 = QX EXIT recovery
                            }
                        }
                        // Remove the stuck tokens from the swing position regardless —
                        // they are no longer under CLKnDGR managing rights.
                        locals.swingNewTokens = state.get().swingTokens.get(locals.i) - locals.swingSellAmt;
                        if (locals.swingNewTokens < 0) { locals.swingNewTokens = 0; }
                        // Divide-first: remaining cost basis after failed ask — avoids overflow on costBasis × remainingTokens.
                        locals.swingNewCost = (locals.swingNewTokens > 0 && state.get().swingTokens.get(locals.i) > 0)
                            ? (sint64)(div((uint64)state.get().swingCostBasis.get(locals.i),
                                           (uint64)state.get().swingTokens.get(locals.i)) * (uint64)locals.swingNewTokens)
                            : 0;
                        state.mut().swingTokens.set(locals.i,    locals.swingNewTokens);
                        state.mut().swingCostBasis.set(locals.i, locals.swingNewCost);
                    }
                    // Sold (or attempted) this check — done; skip the buy section below.
                    continue;
                }
                // Sell trigger not met: consider a DCA-in ADD on a continued dip, but only while this
                // token's cost basis is under the per-token cap (MAX_SWING_POSITION_PCT of capital).
                // The cap re-opens as trading capital grows or as the position is sold down.
                if (locals.swingIsDip
                    && locals.tradingBalance >= state.get().minProfitQu
                    && state.get().swingCostBasis.get(locals.i) <
                        (sint64)div((uint64)locals.tradingBalance * (uint64)MAX_SWING_POSITION_PCT, (uint64)100))
                {
                    locals.swingBuyQu = (sint64)div((uint64)locals.tradingBalance, (uint64)SWING_DCA_ADD_DIVISOR); // 0.25% add
                }
            }

            // ==========================================================
            // BUY: first entry (not holding) + the DCA-in add queued above
            // ==========================================================
            // First entry: a pool we don't yet hold, in a dip, buys 1% of capital. (DCA-in adds for
            // pools we already hold were queued in the EXIT branch above.) The shared block below then
            // executes whichever buy is queued in swingBuyQu — 1% (first) or 0.25% (DCA add) — or nothing.
            if (state.get().swingTokens.get(locals.i) == 0
                && locals.swingIsDip
                && locals.tradingBalance >= state.get().minProfitQu)
            {
                locals.swingBuyQu = (sint64)div(
                    (uint64)locals.tradingBalance * (uint64)SWING_BUY_CAPITAL_PCT,
                    (uint64)100); // 1% first buy
            }

            if (locals.swingBuyQu <= 0 || locals.swingBuyQu > locals.tradingBalance) { continue; } // nothing to buy this check

            // Quote expected token output for slippage floor
            locals.swingQuoteIn.assetIssuer = state.get().poolIssuers.get(locals.i);
            locals.swingQuoteIn.assetName   = state.get().poolAssetNames.get(locals.i);
            locals.swingQuoteIn.quAmountIn  = locals.swingBuyQu;
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactQuInput, locals.swingQuoteIn, locals.swingQuoteOut); }
            if (locals.swingQuoteOut.assetAmountOut <= 0) { continue; }

            // Buy on Qswap: send swingBuyQu, accept up to 5% slippage from quote
            locals.swingBuyIn.assetIssuer       = state.get().poolIssuers.get(locals.i);
            locals.swingBuyIn.assetName         = state.get().poolAssetNames.get(locals.i);
            // EC-C10: subtractive form uses × SLIPPAGE_PCT (×5) not × (100-SLIPPAGE_PCT) (×95),
            // raising overflow threshold from 1.94×10^17 to 3.69×10^18 — same class as EC-3 fixes.
            locals.swingBuyIn.assetAmountOutMin = locals.swingQuoteOut.assetAmountOut -
                (sint64)div((uint64)locals.swingQuoteOut.assetAmountOut * (uint64)SWING_BUY_SLIPPAGE_PCT,
                            (uint64)100);
            { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, SwapExactQuForAsset, locals.swingBuyIn, locals.swingBuyOut, locals.swingBuyQu); }

            if (locals.swingBuyOut.assetAmountOut <= 0) { continue; } // swap failed or excess slippage

            // Deduct spent QU from tradingBalance so later pools in this tick see real remaining capital
            locals.tradingBalance = locals.tradingBalance - locals.swingBuyQu;
            if (locals.tradingBalance < 0) { locals.tradingBalance = 0; }

            // TSRM: tokens arrived under Qswap managing rights — reclaim for CLKnDGR
            locals.swingTsrmIn.asset.issuer             = state.get().poolIssuers.get(locals.i);
            locals.swingTsrmIn.asset.assetName          = state.get().poolAssetNames.get(locals.i);
            locals.swingTsrmIn.numberOfShares           = locals.swingBuyOut.assetAmountOut;
            locals.swingTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
            { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, TransferShareManagementRights, locals.swingTsrmIn, locals.swingTsrmOut, 0); }

            if (locals.swingTsrmOut.transferredNumberOfShares <= 0)
            {
                // TSRM failed — schedule recovery for next tick
                state.mut().swingPendingRecoveryAmt.set(locals.i,    locals.swingBuyOut.assetAmountOut);
                state.mut().swingPendingRecoveryCost.set(locals.i,   locals.swingBuyQu);
                state.mut().swingPendingRecoverySource.set(locals.i, 0); // EC-C11: explicit Qswap path; recovery pass else-branch handles source=0
            }
            else
            {
                state.mut().swingTokens.set(locals.i,
                    state.get().swingTokens.get(locals.i) + locals.swingTsrmOut.transferredNumberOfShares);
                state.mut().swingCostBasis.set(locals.i,
                    state.get().swingCostBasis.get(locals.i) + locals.swingBuyQu);
            }
            // (cooldown already set once at the top of this check — monthly cadence)
        }

        // EC-C9: Dagger still requires minimum capital — gate it here after Cloak loop has run.
        if (locals.tradingBalance < state.get().minProfitQu) { return; }

        // ==============================================================
        // VIX sampler — cheap 3×/day volatility pulse that gates the Dagger.
        // For each active pool, at most every vixSampleInterval ticks (governable pulse rate), take ONE light Qswap
        // pool read, update the fast/slow volatility EWMAs from the % price move, and flag a breakout
        // (fast >= slow × factor AND fast >= floor). On a breakout, wake BOTH Dagger directions for
        // this pool so they hunt at tick speed; calm pools stay on the long baseline cooldown.
        // No order-book reads here — only the light GetPoolBasicState call. Reuses poolIn/poolOut.
        // ==============================================================
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (!state.get().poolActive.get(locals.i)) { continue; }
            // 8h cadence. First sample (count==0) runs immediately to seed a baseline. Unsigned tick
            // subtraction is wraparound-safe (elapsed >= interval) for any gap up to the uint32 range.
            if (state.get().vixSampleCount.get(locals.i) != 0 &&
                (uint32)((uint32)qpi.tick() - state.get().vixLastSampleTick.get(locals.i)) < state.get().vixSampleInterval)
            {
                continue;
            }

            locals.poolIn.assetIssuer = state.get().poolIssuers.get(locals.i);
            locals.poolIn.assetName   = state.get().poolAssetNames.get(locals.i);
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, GetPoolBasicState, locals.poolIn, locals.poolOut); }

            state.mut().vixLastSampleTick.set(locals.i, qpi.tick()); // hold cadence even if the read is unusable

            if (!locals.poolOut.poolExists)              { continue; }
            if (locals.poolOut.reservedAssetAmount <= 0) { continue; }
            locals.vixPrice = (sint64)div((uint64)locals.poolOut.reservedQuAmount, (uint64)locals.poolOut.reservedAssetAmount);
            if (locals.vixPrice <= 0) { continue; }

            // First valid sample for this pool: seed the baseline price (no move to measure yet).
            if (state.get().vixSampleCount.get(locals.i) == 0)
            {
                state.mut().vixLastPrice.set(locals.i, locals.vixPrice);
                state.mut().vixSampleCount.set(locals.i, 1);
                continue;
            }

            // Relative move since last sample, in basis points, capped (outlier + overflow guard).
            // |delta| * 10000 fits uint64 for any realistic Qswap price (quReserve <= total supply).
            locals.vixDelta = locals.vixPrice - state.get().vixLastPrice.get(locals.i);
            if (locals.vixDelta < 0) { locals.vixDelta = -locals.vixDelta; }
            locals.vixMoveBps = (sint64)div((uint64)locals.vixDelta * 10000ULL, (uint64)state.get().vixLastPrice.get(locals.i));
            if (locals.vixMoveBps > VIX_MOVE_CAP_BPS) { locals.vixMoveBps = VIX_MOVE_CAP_BPS; }

            // Derive the EWMA divisors from the live pulse rate so the TIME horizons stay fixed (≈5d / ≈4wk)
            // no matter how many pulses/day governance picks: divisor = horizon_days × pulses/day.
            locals.vixSpd     = (uint64)div((uint64)VIX_DAY_TICKS, (uint64)state.get().vixSampleInterval); // 1, 2, or 3
            locals.vixFastDiv = VIX_FAST_DAYS * locals.vixSpd;
            locals.vixSlowDiv = VIX_SLOW_DAYS * locals.vixSpd;
            // EWMA update, non-negative & overflow-safe: vix = (vix*(D-1) + move) / D.
            state.mut().vixFast.set(locals.i, (sint64)div(
                (uint64)state.get().vixFast.get(locals.i) * (locals.vixFastDiv - 1ULL) + (uint64)locals.vixMoveBps,
                locals.vixFastDiv));
            state.mut().vixSlow.set(locals.i, (sint64)div(
                (uint64)state.get().vixSlow.get(locals.i) * (locals.vixSlowDiv - 1ULL) + (uint64)locals.vixMoveBps,
                locals.vixSlowDiv));

            state.mut().vixLastPrice.set(locals.i, locals.vixPrice);
            if (state.get().vixSampleCount.get(locals.i) < 65535)
            {
                state.mut().vixSampleCount.set(locals.i, state.get().vixSampleCount.get(locals.i) + 1);
            }

            // Breakout: fast vol unusually high vs the token's OWN baseline, AND above the absolute floor.
            // During warm-up vixSlow is small, so the ratio passes easily and the floor does the gating.
            locals.vixBreakoutNow =
                (state.get().vixFast.get(locals.i) >= state.get().vixAbsFloorBps &&
                 (uint64)state.get().vixFast.get(locals.i) * 100ULL >= (uint64)state.get().vixSlow.get(locals.i) * (uint64)state.get().vixBreakoutFactor)
                ? 1 : 0;
            state.mut().vixBreakout.set(locals.i, locals.vixBreakoutNow);

            // On a breakout, wake BOTH Dagger directions to hunt this pool now (cooldown = current tick).
            if (locals.vixBreakoutNow)
            {
                state.mut().poolCooldownTick.set(locals.i, qpi.tick());
                state.mut().poolCooldownTickA.set(locals.i, qpi.tick());
            }
        }

        // ==============================================================
        // THE DAGGER — arb strategy (Direction B: buy QX, sell Qswap)
        // ==============================================================
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (!state.get().poolActive.get(locals.i)) { continue; }
            // Wraparound-safe cooldown check: use signed subtraction so the comparison
            // remains correct even when qpi.tick() overflows uint32 and wraps back to 0.
            // Works as long as cooldown durations are less than 2^31 (~497 days), which they are.
            if ((sint32)(state.get().poolCooldownTick.get(locals.i) - qpi.tick()) > 0) { continue; }
            // Skip pools with unresolved TSRM recovery to prevent overwriting pending recovery tracking.
            // A new arb TSRM failure would orphan the already-stuck shares by overwriting the recovery fields.
            if (state.get().poolPendingRecoverySource.get(locals.i) != 0) { continue; }
            // Adaptive no-arb cooldown: hunt fast (~5 min) while this pool is in a VIX breakout, else
            // sleep on the long baseline (~2 weeks). The VIX sampler wakes a fresh breakout (above).
            locals.cooldownNoArb = state.get().vixBreakout.get(locals.i) ? state.get().breakoutRescanTicks : COOLDOWN_TICKS_BASELINE;

            // --- Read Qswap pool state ---
            locals.poolIn.assetIssuer = state.get().poolIssuers.get(locals.i);
            locals.poolIn.assetName   = state.get().poolAssetNames.get(locals.i);
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, GetPoolBasicState, locals.poolIn, locals.poolOut); }

            if (!locals.poolOut.poolExists)              { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.poolOut.reservedAssetAmount <= 0) { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.poolOut.reservedQuAmount    <= 0) { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }

            // ==============================================================
            // Direction B: Buy on QX (match best ask), sell on Qswap
            // ==============================================================
            locals.askIn.issuer    = locals.poolIn.assetIssuer;
            locals.askIn.assetName = locals.poolIn.assetName;
            locals.askIn.offset    = 0;
            { CALL_OTHER_CONTRACT_FUNCTION(QX, AssetAskOrders, locals.askIn, locals.askOut); }

            if (locals.askOut.orders.get(0).price <= 0)          { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.askOut.orders.get(0).numberOfShares <= 0) { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }

            // Cap numberOfShares to what CLKnDGR can afford — prevents sint64 overflow
            // on the price * shares multiply and enables partial fills when QX supply
            // exceeds our balance (previously those pools would be skipped entirely).
            locals.addBidIn.numberOfShares = locals.askOut.orders.get(0).numberOfShares;
            if ((uint64)locals.addBidIn.numberOfShares > div((uint64)locals.tradingBalance, (uint64)locals.askOut.orders.get(0).price))
            {
                locals.addBidIn.numberOfShares = (sint64)div((uint64)locals.tradingBalance, (uint64)locals.askOut.orders.get(0).price);
            }
            if (locals.addBidIn.numberOfShares <= 0)
            {
                // Only apply the 5-week unaffordable cooldown when the vault itself was underfunded
                // at tick start (before Cloak ran). If initialTradingBalance could afford ≥1 share
                // but Cloak depleted capital mid-tick, skip without penalizing the pool — it will
                // be eligible again next tick once tradingBalance is replenished.
                if (div((uint64)locals.initialTradingBalance, (uint64)locals.askOut.orders.get(0).price) == 0)
                {
                    state.mut().poolCooldownTick.set(locals.i, qpi.tick() + COOLDOWN_TICKS_UNAFFORDABLE);
                }
                continue;
            }
            locals.quCostOnQx = (sint64)((uint64)locals.addBidIn.numberOfShares * (uint64)locals.askOut.orders.get(0).price);

            locals.qaIn.assetIssuer   = locals.poolIn.assetIssuer;
            locals.qaIn.assetName     = locals.poolIn.assetName;
            locals.qaIn.assetAmountIn = locals.addBidIn.numberOfShares;
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactAssetInput, locals.qaIn, locals.qaOut); }

            locals.estimatedProfit = locals.qaOut.quAmountOut - locals.quCostOnQx;

            if (locals.estimatedProfit > state.get().minProfitQu)
            {
                // Cap trade size to target ~minProfitQu profit per pool per tick.
                // With many pools active simultaneously, allowing each to trade at max size would
                // exhaust the QU balance early in the loop, starving later pools.
                // Scale down by (estimatedProfit / minProfitQu) — integer division, so actual profit
                // will be slightly above minProfitQu due to reduced AMM price impact on smaller trades.
                locals.scaleFactor = (sint64)div((uint64)locals.estimatedProfit, (uint64)state.get().minProfitQu);
                if (locals.scaleFactor > 1)
                {
                    locals.addBidIn.numberOfShares = (sint64)div((uint64)locals.addBidIn.numberOfShares, (uint64)locals.scaleFactor);
                    // Scale-down integer-divided to 0 (e.g., affordableShares=3, scaleFactor=5).
                    // Set 6.25-day cooldown so the pool is not re-checked every tick in a tight loop.
                    if (locals.addBidIn.numberOfShares <= 0) { state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
                    locals.quCostOnQx = (sint64)((uint64)locals.addBidIn.numberOfShares * (uint64)locals.askOut.orders.get(0).price);
                }

                // Leg 1: Bid on QX at ask price -> fills immediately
                locals.addBidIn.issuer    = locals.poolIn.assetIssuer;
                locals.addBidIn.assetName = locals.poolIn.assetName;
                locals.addBidIn.price     = locals.askOut.orders.get(0).price;
                // locals.addBidIn.numberOfShares already set and capped above
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, AddToBidOrder, locals.addBidIn, locals.addBidOut, locals.quCostOnQx); }

                if (locals.addBidOut.addedNumberOfShares <= 0) { continue; }

                // Deduct actual QU spent from tradingBalance so later pools in this tick
                // see the real remaining capital, not the stale tick-start snapshot.
                locals.tradingBalance = locals.tradingBalance - (sint64)((uint64)locals.addBidOut.addedNumberOfShares * (uint64)locals.askOut.orders.get(0).price);
                if (locals.tradingBalance < 0) { locals.tradingBalance = 0; }
                // Update quCostOnQx to reflect actual spend on partial QX fills
                locals.quCostOnQx = (sint64)((uint64)locals.addBidOut.addedNumberOfShares * (uint64)locals.askOut.orders.get(0).price);

                // Leg 2: tokens arrived from QX under QX managing rights.
                // Ask QX to release management rights to CLKnDGR (triggers CLKnDGR::PRE_ACQUIRE_SHARES).
                locals.reserveAmt = (sint64)div((uint64)locals.addBidOut.addedNumberOfShares * (uint64)RESERVE_PCT, (uint64)100);
                locals.sellAmt    = locals.addBidOut.addedNumberOfShares - locals.reserveAmt;

                locals.qxTsrmIn.asset.issuer             = locals.poolIn.assetIssuer;
                locals.qxTsrmIn.asset.assetName          = locals.poolIn.assetName;
                locals.qxTsrmIn.numberOfShares           = locals.addBidOut.addedNumberOfShares;
                locals.qxTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, TransferShareManagementRights, locals.qxTsrmIn, locals.qxTsrmOut, 0); }
                if (locals.qxTsrmOut.transferredNumberOfShares <= 0)
                {
                    // Leg 1 QU was spent; tokens are owned by CLKnDGR but still under QX
                    // managing rights. Schedule a TSRM retry for next tick.
                    state.mut().poolPendingRecoveryAmt.set(locals.i, locals.addBidOut.addedNumberOfShares);
                    state.mut().poolPendingRecoverySource.set(locals.i, 2); // 2 = stuck under QX
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i, (sint64)((uint64)locals.addBidOut.addedNumberOfShares * (uint64)locals.askOut.orders.get(0).price));
                    continue;
                }

                // sellAmt portion: give Qswap managing rights via releaseShares (QSWAP charges no transfer fee).
                locals.transferResult = 0;
                if (locals.sellAmt > 0)
                {
                    locals.tradeAsset.issuer    = locals.poolIn.assetIssuer;
                    locals.tradeAsset.assetName = locals.poolIn.assetName;
                    locals.transferResult = qpi.releaseShares(
                        locals.tradeAsset,
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                        locals.sellAmt,
                        QSWAP_CONTRACT_INDEX, QSWAP_CONTRACT_INDEX,
                        0  // QSWAP PRE_ACQUIRE_SHARES accepts at no fee
                    );
                    // Guard: if Qswap rejected the rights transfer, all tokens remain under
                    // CLKnDGR managing rights. Fold everything into reserve for 4-epoch
                    // liquidation rather than leaving them untracked.
                    // CRITICAL: releaseShares to Qswap passes fee=0, so success returns 0 (paid fee).
                    // Must check < 0 for failure, NOT <= 0, or Direction B never executes the swap.
                    if (locals.transferResult < 0)
                    {
                        state.mut().poolReserveTokens.set(locals.i,
                            state.get().poolReserveTokens.get(locals.i) + locals.qxTsrmOut.transferredNumberOfShares);
                        state.mut().poolReserveCostBasis.set(locals.i,
                            state.get().poolReserveCostBasis.get(locals.i)
                            + (sint64)((uint64)locals.qxTsrmOut.transferredNumberOfShares * (uint64)locals.askOut.orders.get(0).price));
                        continue;
                    }
                }
                // reserveAmt portion stays under CLKnDGR managing rights — no further action needed.

                locals.swapAssetIn.assetIssuer    = locals.poolIn.assetIssuer;
                locals.swapAssetIn.assetName      = locals.poolIn.assetName;
                locals.swapAssetIn.assetAmountIn  = locals.sellAmt;
                // Slippage guard: require Qswap to return at least quCostOnQx + minProfitQu.
                // quCostOnQx (updated at line ~2019) = addBidOut.addedNumberOfShares × price = full
                // QU spent on QX including the 10% reserve. Using the full cost ensures net profit
                // (quAmountOut − quCostOnQx) ≥ minProfitQu even when reserveAmt > 0. The prior
                // sellAmt × price guard understated the minimum by reserveAmt × price, allowing
                // below-threshold (or negative) net profit on expensive tokens with ≥10-share fills.
                locals.swapAssetIn.quAmountOutMin = locals.quCostOnQx + state.get().minProfitQu;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, SwapExactAssetForQu, locals.swapAssetIn, locals.swapAssetOut, 0); }

                if (locals.swapAssetOut.quAmountOut <= 0)
                {
                    // Swap failed — sellAmt tokens remain under Qswap managing rights.
                    // Schedule TSRM recovery to reclaim them next tick.
                    state.mut().poolPendingRecoveryAmt.set(locals.i, locals.sellAmt);
                    state.mut().poolPendingRecoverySource.set(locals.i, 1); // stuck under Qswap
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i,
                        (sint64)((uint64)locals.sellAmt * (uint64)locals.askOut.orders.get(0).price));
                    if (locals.reserveAmt > 0)
                    {
                        state.mut().poolReserveTokens.set(locals.i,
                            state.get().poolReserveTokens.get(locals.i) + locals.reserveAmt);
                        state.mut().poolReserveCostBasis.set(locals.i,
                            state.get().poolReserveCostBasis.get(locals.i)
                            + (sint64)((uint64)locals.reserveAmt * (uint64)locals.askOut.orders.get(0).price));
                    }
                    continue;
                }

                // Track reserve cost basis (price paid per token on QX)
                if (locals.reserveAmt > 0)
                {
                    state.mut().poolReserveTokens.set(locals.i,
                        state.get().poolReserveTokens.get(locals.i) + locals.reserveAmt);
                    state.mut().poolReserveCostBasis.set(locals.i,
                        state.get().poolReserveCostBasis.get(locals.i)
                        + (sint64)((uint64)locals.reserveAmt * (uint64)locals.askOut.orders.get(0).price));
                }

                state.mut().totalArbsExecuted = state.get().totalArbsExecuted + 1;
                if (locals.swapAssetOut.quAmountOut > locals.quCostOnQx)
                {
                    locals.estimatedProfit = locals.swapAssetOut.quAmountOut - locals.quCostOnQx;
                    state.mut().totalProfitEarned = state.get().totalProfitEarned + locals.estimatedProfit;
                    state.mut().epochProfit       = state.get().epochProfit       + locals.estimatedProfit;
                }
            }
            else
            {
                // No profitable arb found on Qswap: pause checks for 5 days
                state.mut().poolCooldownTick.set(locals.i, qpi.tick() + locals.cooldownNoArb);
            }
        }

        // ==============================================================
        // THE DAGGER — Direction A: buy Qswap, sell into QX bid
        // Mirror of Direction B for the opposite price gap (Qswap cheaper than the QX best bid).
        // Kept as a SEPARATE loop so the audited Direction B path above stays byte-for-byte intact.
        // Shares locals.tradingBalance (carried from Direction B — capital already spent there is
        // reflected, so no double-spend) and the recovery system (source 1 = stuck under Qswap,
        // 2 = stuck under QX). Uses its own poolCooldownTickA.
        // PROFIT IS BOOKED DEFERRED (Cloak EC-C2 model): selling into a QX bid via AddToAskOrder
        // matches immediately, but the QU lands in a FUTURE tick — not in-tick like a Qswap swap.
        // Recording it now would inflate epochProfit against QU not yet in quBalance and over-distribute
        // at BEGIN_EPOCH. It flows in naturally via the quBalance delta. Only totalArbsExecuted is bumped.
        // ==============================================================
        for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
        {
            if (!state.get().poolActive.get(locals.i)) { continue; }
            // Wraparound-safe cooldown (same signed-subtraction pattern as Direction B).
            if ((sint32)(state.get().poolCooldownTickA.get(locals.i) - qpi.tick()) > 0) { continue; }
            // Skip pools with an unresolved TSRM recovery (slot is shared with Cloak/Direction B);
            // a new failure here would overwrite pending recovery tracking and orphan stuck shares.
            if (state.get().poolPendingRecoverySource.get(locals.i) != 0) { continue; }
            // Direction A spends QU to buy on Qswap, so it requires minimum trading capital.
            if (locals.tradingBalance < state.get().minProfitQu) { continue; }
            // Adaptive no-arb cooldown (same as Direction B): breakout → fast re-scan, calm → baseline.
            locals.cooldownNoArb = state.get().vixBreakout.get(locals.i) ? state.get().breakoutRescanTicks : COOLDOWN_TICKS_BASELINE;

            // --- Read Qswap pool state ---
            locals.poolIn.assetIssuer = state.get().poolIssuers.get(locals.i);
            locals.poolIn.assetName   = state.get().poolAssetNames.get(locals.i);
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, GetPoolBasicState, locals.poolIn, locals.poolOut); }

            if (!locals.poolOut.poolExists)              { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.poolOut.reservedAssetAmount <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.poolOut.reservedQuAmount    <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }

            // --- Read QX best bid: the price buyers pay (our deterministic fill price) and the
            //     size we can sell immediately. Capping our ask to this size guarantees a full fill;
            //     any remainder would sit as an open order CLKnDGR cannot reclaim. ---
            locals.swingBidIn.issuer    = locals.poolIn.assetIssuer;
            locals.swingBidIn.assetName = locals.poolIn.assetName;
            locals.swingBidIn.offset    = 0;
            { CALL_OTHER_CONTRACT_FUNCTION(QX, AssetBidOrders, locals.swingBidIn, locals.swingBidOut); }

            if (locals.swingBidOut.orders.get(0).price <= 0)          { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            if (locals.swingBidOut.orders.get(0).numberOfShares <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            locals.daBidPrice  = locals.swingBidOut.orders.get(0).price;
            locals.daBidShares = locals.swingBidOut.orders.get(0).numberOfShares;

            // Target tokens to buy on Qswap = best-bid size. We retain RESERVE_PCT and sell the
            // remainder (< daBidShares) into the bid, so the sell always fully fills.
            locals.daTargetTokens = locals.daBidShares;

            // Quote QU required to buy daTargetTokens on Qswap.
            locals.daQuoteIn.assetIssuer    = locals.poolIn.assetIssuer;
            locals.daQuoteIn.assetName      = locals.poolIn.assetName;
            locals.daQuoteIn.assetAmountOut = locals.daTargetTokens;
            { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactAssetOutput, locals.daQuoteIn, locals.daQuoteOut); }
            locals.daBuyQu = locals.daQuoteOut.quAmountIn;
            if (locals.daBuyQu <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }

            // Capital cap: if we cannot afford the full target, scale the token count down to fit
            // current capital and re-quote. Smaller trades incur less Qswap slippage, so this never
            // worsens per-token economics (same reasoning as Direction B's affordability cap).
            if (locals.daBuyQu > locals.tradingBalance)
            {
                locals.daTargetTokens = (sint64)div((uint64)locals.daTargetTokens * (uint64)locals.tradingBalance, (uint64)locals.daBuyQu);
                if (locals.daTargetTokens <= 0) { continue; } // even 1 token costs more than our capital — retry next tick
                locals.daQuoteIn.assetAmountOut = locals.daTargetTokens;
                { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactAssetOutput, locals.daQuoteIn, locals.daQuoteOut); }
                locals.daBuyQu = locals.daQuoteOut.quAmountIn;
                if (locals.daBuyQu <= 0 || locals.daBuyQu > locals.tradingBalance) { continue; }
            }

            // Estimated split and proceeds (proceeds land next tick; this is the profitability gate).
            locals.daReserveAmt = (sint64)div((uint64)locals.daTargetTokens * (uint64)RESERVE_PCT, (uint64)100);
            locals.daSellAmt    = locals.daTargetTokens - locals.daReserveAmt;
            if (locals.daSellAmt <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
            locals.daProceeds  = (sint64)((uint64)locals.daSellAmt * (uint64)locals.daBidPrice);
            locals.daEstProfit = locals.daProceeds - locals.daBuyQu;

            if (locals.daEstProfit > state.get().minProfitQu)
            {
                // Capital-spread scale-down (mirrors Direction B): when profit >> minProfitQu, shrink the
                // trade so a single pool does not exhaust capital before later pools are checked.
                locals.daScaleFactor = (sint64)div((uint64)locals.daEstProfit, (uint64)state.get().minProfitQu);
                if (locals.daScaleFactor > 1)
                {
                    locals.daTargetTokens = (sint64)div((uint64)locals.daTargetTokens, (uint64)locals.daScaleFactor);
                    if (locals.daTargetTokens <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
                    locals.daQuoteIn.assetAmountOut = locals.daTargetTokens;
                    { CALL_OTHER_CONTRACT_FUNCTION(QSWAP, QuoteExactAssetOutput, locals.daQuoteIn, locals.daQuoteOut); }
                    locals.daBuyQu = locals.daQuoteOut.quAmountIn;
                    if (locals.daBuyQu <= 0 || locals.daBuyQu > locals.tradingBalance) { continue; }
                    locals.daReserveAmt = (sint64)div((uint64)locals.daTargetTokens * (uint64)RESERVE_PCT, (uint64)100);
                    locals.daSellAmt    = locals.daTargetTokens - locals.daReserveAmt;
                    if (locals.daSellAmt <= 0) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
                    locals.daProceeds   = (sint64)((uint64)locals.daSellAmt * (uint64)locals.daBidPrice);
                    // Re-verify profit after the smaller re-quote before committing capital.
                    if (locals.daProceeds < locals.daBuyQu + state.get().minProfitQu) { state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb); continue; }
                }

                // Leg 1: buy daTargetTokens on Qswap, spending exactly daBuyQu (5% slippage floor on tokens out).
                locals.swingBuyIn.assetIssuer       = locals.poolIn.assetIssuer;
                locals.swingBuyIn.assetName         = locals.poolIn.assetName;
                locals.swingBuyIn.assetAmountOutMin = locals.daTargetTokens -
                    (sint64)div((uint64)locals.daTargetTokens * (uint64)SWING_BUY_SLIPPAGE_PCT, (uint64)100);
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, SwapExactQuForAsset, locals.swingBuyIn, locals.swingBuyOut, locals.daBuyQu); }

                if (locals.swingBuyOut.assetAmountOut <= 0) { continue; } // swap failed or excess slippage — QU not committed

                // Exact-QU-in swap spends the full daBuyQu; reflect it so later pools see real capital.
                locals.tradingBalance = locals.tradingBalance - locals.daBuyQu;
                if (locals.tradingBalance < 0) { locals.tradingBalance = 0; }

                // Leg 2: tokens arrived under Qswap managing rights — reclaim for CLKnDGR.
                locals.swapTsrmIn.asset.issuer             = locals.poolIn.assetIssuer;
                locals.swapTsrmIn.asset.assetName          = locals.poolIn.assetName;
                locals.swapTsrmIn.numberOfShares           = locals.swingBuyOut.assetAmountOut;
                locals.swapTsrmIn.newManagingContractIndex = CLKnDGR_CONTRACT_INDEX;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QSWAP, TransferShareManagementRights, locals.swapTsrmIn, locals.swapTsrmOut, 0); }
                if (locals.swapTsrmOut.transferredNumberOfShares <= 0)
                {
                    // QU spent; tokens owned by CLKnDGR but stuck under Qswap. Schedule recovery (source 1).
                    state.mut().poolPendingRecoveryAmt.set(locals.i,       locals.swingBuyOut.assetAmountOut);
                    state.mut().poolPendingRecoverySource.set(locals.i,    1); // stuck under Qswap
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i, locals.daBuyQu);
                    continue;
                }

                // Re-derive the split from the CONFIRMED reclaimed amount, and re-cap the sell to the
                // bid size so the QX ask still fully fills. Everything not sold becomes reserve.
                locals.daTotalTokens = locals.swapTsrmOut.transferredNumberOfShares;
                locals.daSellAmt     = locals.daTotalTokens - (sint64)div((uint64)locals.daTotalTokens * (uint64)RESERVE_PCT, (uint64)100);
                if (locals.daSellAmt > locals.daBidShares) { locals.daSellAmt = locals.daBidShares; }
                locals.daReserveAmt   = locals.daTotalTokens - locals.daSellAmt;
                locals.daCostPerToken = (sint64)div((uint64)locals.daBuyQu, (uint64)locals.daTotalTokens);

                // daSellAmt rounded to 0 (tiny reclaim): keep everything as reserve, nothing to sell.
                if (locals.daSellAmt <= 0)
                {
                    state.mut().poolReserveTokens.set(locals.i,    state.get().poolReserveTokens.get(locals.i)    + locals.daTotalTokens);
                    state.mut().poolReserveCostBasis.set(locals.i, state.get().poolReserveCostBasis.get(locals.i) + locals.daBuyQu);
                    continue;
                }

                // Hard post-slippage profit guard (the Direction A analog of Direction B's Qswap
                // quAmountOutMin at the sell leg). AddToAskOrder has no min-proceeds parameter, and the
                // Qswap buy may have returned up to SWING_BUY_SLIPPAGE_PCT fewer tokens than quoted, so
                // re-check against ACTUAL tokens before committing the sell. Require the (deterministic,
                // bid-priced) proceeds to recoup the FULL buy cost + minProfitQu — same as Direction B,
                // so the retained reserve is pure upside. If it no longer clears, do NOT sell at a thin
                // margin: fold the cheaply-bought tokens into reserve for the liquidation engine to
                // realize later. No forced loss.
                if ((sint64)((uint64)locals.daSellAmt * (uint64)locals.daBidPrice) < locals.daBuyQu + state.get().minProfitQu)
                {
                    state.mut().poolReserveTokens.set(locals.i,    state.get().poolReserveTokens.get(locals.i)    + locals.daTotalTokens);
                    state.mut().poolReserveCostBasis.set(locals.i, state.get().poolReserveCostBasis.get(locals.i) + locals.daBuyQu);
                    continue;
                }

                // Leg 3a: give QX managing rights for the sell portion (pays QX transfer fee).
                { CALL_OTHER_CONTRACT_FUNCTION(QX, Fees, locals.swingFeesIn, locals.swingFeesOut); }
                locals.tradeAsset.issuer    = locals.poolIn.assetIssuer;
                locals.tradeAsset.assetName = locals.poolIn.assetName;
                locals.transferResult = qpi.releaseShares(
                    locals.tradeAsset,
                    id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                    id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0),
                    locals.daSellAmt,
                    QX_CONTRACT_INDEX, QX_CONTRACT_INDEX,
                    locals.swingFeesOut.transferFee);
                if (locals.transferResult < 0)
                {
                    // QX rejected the rights transfer — nothing was released. Fold ALL reclaimed tokens
                    // into reserve for later liquidation rather than leaving them untracked.
                    state.mut().poolReserveTokens.set(locals.i,    state.get().poolReserveTokens.get(locals.i)    + locals.daTotalTokens);
                    state.mut().poolReserveCostBasis.set(locals.i, state.get().poolReserveCostBasis.get(locals.i) + locals.daBuyQu);
                    continue;
                }

                // Leg 3b: place an ask at the bid price, sized to daSellAmt (<= bid -> immediate full fill).
                locals.swingAskIn.issuer         = locals.poolIn.assetIssuer;
                locals.swingAskIn.assetName      = locals.poolIn.assetName;
                locals.swingAskIn.price          = locals.daBidPrice;
                locals.swingAskIn.numberOfShares = locals.daSellAmt;
                { INVOKE_OTHER_CONTRACT_PROCEDURE(QX, AddToAskOrder, locals.swingAskIn, locals.swingAskOut, 0); }

                // Any shares QX did not match are stuck under QX management with no reclaimable order.
                // (Normally zero, since we priced at the bid and capped to bid size.) Route the unmatched
                // remainder to recovery (source 2); the matched portion's QU lands next tick (deferred).
                if (locals.swingAskOut.addedNumberOfShares < locals.daSellAmt)
                {
                    locals.daSellAmt = (locals.swingAskOut.addedNumberOfShares > 0) ? locals.swingAskOut.addedNumberOfShares : 0;
                    state.mut().poolPendingRecoveryAmt.set(locals.i,       (locals.daTotalTokens - locals.daReserveAmt) - locals.daSellAmt);
                    state.mut().poolPendingRecoverySource.set(locals.i,    2); // stuck under QX
                    state.mut().poolPendingRecoveryCostBasis.set(locals.i, (sint64)((uint64)locals.daCostPerToken * (uint64)((locals.daTotalTokens - locals.daReserveAmt) - locals.daSellAmt)));
                }

                // Reserve portion never left CLKnDGR management — fold it in (cost = per-token Qswap cost).
                if (locals.daReserveAmt > 0)
                {
                    state.mut().poolReserveTokens.set(locals.i,    state.get().poolReserveTokens.get(locals.i)    + locals.daReserveAmt);
                    state.mut().poolReserveCostBasis.set(locals.i, state.get().poolReserveCostBasis.get(locals.i) + (sint64)((uint64)locals.daCostPerToken * (uint64)locals.daReserveAmt));
                }

                // Count the arb only if some shares actually sold. Profit is DEFERRED (see header note):
                // do NOT touch epochProfit/totalProfitEarned — the QU is not yet in quBalance.
                if (locals.daSellAmt > 0)
                {
                    state.mut().totalArbsExecuted = state.get().totalArbsExecuted + 1;
                }
            }
            else
            {
                // No profitable Direction A arb: pause checks for 5 days.
                state.mut().poolCooldownTickA.set(locals.i, qpi.tick() + locals.cooldownNoArb);
            }
        }
    }
    END_TICK()
    {
    }

    // ---------------------------------------------------------------
    // Function: getStats
    // ---------------------------------------------------------------
    PUBLIC_FUNCTION(getStats)
    {
        output.totalArbsExecuted = state.get().totalArbsExecuted;
        output.totalProfitEarned = state.get().totalProfitEarned;
        output.quBalance         = state.get().quBalance;
        output.quReserve         = state.get().quReserve;
        output.qearnReserve      = state.get().qearnReserve;
        output.poolCount         = state.get().poolCount;
    }

    // ---------------------------------------------------------------
    // Function: getPool
    // ---------------------------------------------------------------
    PUBLIC_FUNCTION(getPool)
    {
        if (input.index >= (uint64)state.get().poolCount) { output.active = 0; return; }
        output.assetName   = state.get().poolAssetNames.get(input.index);
        output.assetIssuer = state.get().poolIssuers.get(input.index);
        output.active      = state.get().poolActive.get(input.index);
    }

    // ---------------------------------------------------------------
    // Procedure: submitProposal
    // Any CLKnDGR shareholder (≥1 share) may submit a proposal by
    // attaching the current proposalFee as invocationReward.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(submitProposal)
    {
        output.success = 0;
        output.slot    = 255; // invalid sentinel

        // Check proposal slot availability — full refund on rejection so a typo
        // or losing the last-slot race does not silently consume the proposer's fee.
        if (state.get().proposalsThisEpoch >= MAX_PROPOSALS_PER_EPOCH)
        {
            if (qpi.invocationReward() > 0) { qpi.transfer(qpi.invocator(), qpi.invocationReward()); }
            return;
        }

        // Validate proposal type — full refund on rejection (mirrors the insufficient-fee path below).
        if (input.proposalType < PROP_TYPE_ADD_POOL ||
            input.proposalType > PROP_TYPE_UPDATE_BREAKOUT_RESCAN)
        {
            if (qpi.invocationReward() > 0) { qpi.transfer(qpi.invocator(), qpi.invocationReward()); }
            return;
        }

        // Determine required fee based on proposal type
        if (input.proposalType == PROP_TYPE_ADD_POOL)
            locals.requiredFee = state.get().proposalFeeAddPool;
        else if (input.proposalType == PROP_TYPE_UPDATE_PAYOUT)
            locals.requiredFee = state.get().proposalFeePayoutStructure;
        else
            locals.requiredFee = state.get().proposalFeeDefault;
        if ((sint64)qpi.invocationReward() < locals.requiredFee)
        {
            if (qpi.invocationReward() > 0) { qpi.transfer(qpi.invocator(), qpi.invocationReward()); }
            return;
        }
        // Refund any overpayment immediately — feePaid records exactly requiredFee
        if ((sint64)qpi.invocationReward() > locals.requiredFee)
        {
            qpi.transfer(qpi.invocator(), (uint64)((sint64)qpi.invocationReward() - locals.requiredFee));
        }

        // Check shareholder requirement: proposer must hold ≥1 CLKnDGR share
        locals.shareBalance = qpi.numberOfShares(
            state.get().selfAsset,
            AssetOwnershipSelect::byOwner(qpi.invocator()),
            AssetPossessionSelect::byPossessor(qpi.invocator())
        );
        if (locals.shareBalance <= 0)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.requiredFee); // refund — not a shareholder
            return;
        }

        // All eligibility checks passed — content failures from here refund 69% of the fee.
        locals.contentValid = 1;

        // Type-specific input validation
        if (input.proposalType == PROP_TYPE_ADD_POOL)
        {
            // Reject if pool already exists (active or inactive) — re-adding is a no-op and would waste the fee
            locals.found = 0;
            for (locals.i = 0; locals.i < (uint64)state.get().poolCount; locals.i = locals.i + 1)
            {
                if (state.get().poolAssetNames.get(locals.i) == input.assetName &&
                    state.get().poolIssuers.get(locals.i)    == input.assetIssuer)
                {
                    locals.found = 1;
                    break;
                }
            }
            if (locals.found)                      { locals.contentValid = 0; }
            if (locals.contentValid && state.get().poolCount >= 255) { locals.contentValid = 0; } // pool cap
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_REMOVE_POOL)
        {
            if (input.poolIndex >= (uint64)state.get().poolCount) { locals.contentValid = 0; }
            if (locals.contentValid && !state.get().poolActive.get(input.poolIndex)) { locals.contentValid = 0; } // already inactive
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_REACTIVATE_POOL)
        {
            if (input.poolIndex >= (uint64)state.get().poolCount) { locals.contentValid = 0; }
            if (locals.contentValid && state.get().poolActive.get(input.poolIndex)) { locals.contentValid = 0; } // already active
        }
        if (locals.contentValid &&
            (input.proposalType == PROP_TYPE_UPDATE_PROPOSAL_FEE ||
             input.proposalType == PROP_TYPE_UPDATE_FEE_ADD_POOL ||
             input.proposalType == PROP_TYPE_UPDATE_FEE_PAYOUT   ||
             input.proposalType == PROP_TYPE_UPDATE_MIN_QUORUM))
        {
            if (input.newValue <= 0) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_MIN_PROFIT)
        {
            // Allowlist: only the four governance-approved values are valid.
            // Prevents both mechanical failure (too low) and permanent deactivation (too high).
            if (input.newValue != MIN_PROFIT_QU_OPT1 && input.newValue != MIN_PROFIT_QU_OPT2 &&
                input.newValue != MIN_PROFIT_QU_OPT3 && input.newValue != MIN_PROFIT_QU_OPT4)
            { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_MIN_QUORUM)
        {
            // Floor: cannot lower below the initial minimum (prevents single-voter capture).
            // Ceiling: MAX_VOTER_QUORUM = PROPOSAL_VOTER_CAPACITY = 676 (full shareholder participation).
            if (input.newValue < (sint64)INITIAL_MIN_VOTER_QUORUM ||
                input.newValue > (sint64)MAX_VOTER_QUORUM) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_PAYOUT)
        {
            if (input.newValue < 0 || input.newValue > 3) { locals.contentValid = 0; } // 0=default,1,2, 3=recovery
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_WITHDRAW_QU_RESERVE)
        {
            if (input.withdrawAmount <= 0)                    { locals.contentValid = 0; }
            if (locals.contentValid && input.withdrawAmount > state.get().quReserve) { locals.contentValid = 0; } // snapshot check; execution re-validates
            if (locals.contentValid && input.destination == NULL_ID)                 { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_WITHDRAW_ASSET_RESERVE)
        {
            if (input.poolIndex >= (uint64)state.get().poolCount) { locals.contentValid = 0; }
            if (locals.contentValid && state.get().poolActive.get(input.poolIndex)) { locals.contentValid = 0; } // pool must be inactive
            if (locals.contentValid && input.destination == NULL_ID)                { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_VAULT_TIER)
        {
            if (input.newValue < 0 || input.newValue > 8) { locals.contentValid = 0; } // valid tier indices: 0–8
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT)
        {
            if (input.newValue != 2 && input.newValue != 5 &&
                input.newValue != 7 && input.newValue != 10) { locals.contentValid = 0; } // valid options: 2, 5, 7, 10
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN)
        {
            if (input.newValue != 50000000LL && input.newValue != 150000000LL &&
                input.newValue != 250000000LL && input.newValue != 350000000LL) { locals.contentValid = 0; } // valid options: 50M, 150M, 250M, 350M
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_RELOCK_AMOUNT)
        {
            if (input.newValue != 1000000LL  && input.newValue != 5000000LL  &&
                input.newValue != 10000000LL && input.newValue != 20000000LL &&
                input.newValue != 25000000LL && input.newValue != 50000000LL) { locals.contentValid = 0; } // valid options: 1M, 5M, 10M, 20M, 25M, 50M
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR)
        {
            // Fee-reserve safety-valve floor. 0 disables the valve; the rest are buffer sizes.
            if (input.newValue != 0LL           && input.newValue != 1000000000LL  &&
                input.newValue != 5000000000LL  && input.newValue != 10000000000LL &&
                input.newValue != 20000000000LL) { locals.contentValid = 0; } // valid: 0(off), 1B, 5B, 10B, 20B
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_SELL_POOL_TOKENS)
        {
            // Sell newValue% (1..100) of the pool's tokens on Qswap for QU. Allowed on active
            // pools (take profit while the strategy keeps running) AND inactive pools (liquidate a
            // paused bag into the vault — strictly better for depositors than the raw token sweep).
            if (input.poolIndex >= (uint64)state.get().poolCount) { locals.contentValid = 0; }
            if (locals.contentValid && (input.newValue < 1 || input.newValue > 100)) { locals.contentValid = 0; } // percent of holdings to sell
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_VIX_FACTOR)
        {
            // VIX breakout sensitivity ×100: fast vol must reach slow baseline × this/100 to wake the Dagger.
            // Range 0.09×–5×: sub-1× = hunt almost always (floor is the real gate); high = very selective.
            if (input.newValue != 9LL   && input.newValue != 18LL  && input.newValue != 37LL  &&
                input.newValue != 75LL  && input.newValue != 150LL && input.newValue != 200LL &&
                input.newValue != 225LL && input.newValue != 275LL && input.newValue != 350LL &&
                input.newValue != 450LL && input.newValue != 500LL) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_VIX_FLOOR)
        {
            // VIX absolute floor (bps): min fast volatility to count as a breakout (0 = ratio-only gate).
            if (input.newValue != 0LL  && input.newValue != 10LL  && input.newValue != 25LL &&
                input.newValue != 50LL && input.newValue != 100LL && input.newValue != 200LL) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_VIX_PULSE_RATE)
        {
            // VIX pulses per day: more = sharper volatility detection, more execution-fee cost.
            if (input.newValue != 1LL && input.newValue != 2LL && input.newValue != 3LL) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_SWING_SELL_PCT)
        {
            // Cloak sell chunk: % of the bag sold each time the +12% trigger fires.
            if (input.newValue != 10LL && input.newValue != 15LL && input.newValue != 20LL &&
                input.newValue != 25LL && input.newValue != 33LL && input.newValue != 50LL) { locals.contentValid = 0; }
        }
        if (locals.contentValid && input.proposalType == PROP_TYPE_UPDATE_BREAKOUT_RESCAN)
        {
            // Dagger breakout re-scan pace, submitted in SECONDS (stored ×4 as ticks): 30s–5min.
            if (input.newValue != 30LL  && input.newValue != 60LL  && input.newValue != 120LL &&
                input.newValue != 180LL && input.newValue != 240LL && input.newValue != 300LL) { locals.contentValid = 0; }
        }

        // Reject duplicate proposals: prevent two conflicting proposals of the same type in one epoch.
        // Pool-indexed types conflict only if the same poolIndex is targeted; ADD_POOL conflicts only
        // if the same asset is proposed; all singleton types reject any second proposal of that type.
        for (locals.i = 0; locals.i < (uint64)state.get().proposalsThisEpoch && locals.contentValid; locals.i = locals.i + 1)
        {
            locals.prop = state.get().proposals.get(locals.i);
            if (locals.prop.proposalType == input.proposalType &&
                locals.prop.status       == PROP_STATUS_ACTIVE)
            {
                if (input.proposalType == PROP_TYPE_REMOVE_POOL        ||
                    input.proposalType == PROP_TYPE_REACTIVATE_POOL    ||
                    input.proposalType == PROP_TYPE_WITHDRAW_ASSET_RESERVE ||
                    input.proposalType == PROP_TYPE_SELL_POOL_TOKENS)
                {
                    if (locals.prop.poolIndex == input.poolIndex) { locals.contentValid = 0; }
                }
                else if (input.proposalType == PROP_TYPE_ADD_POOL)
                {
                    if (locals.prop.assetName   == input.assetName &&
                        locals.prop.assetIssuer == input.assetIssuer) { locals.contentValid = 0; }
                }
                else
                {
                    locals.contentValid = 0; // singleton type — any duplicate is a conflict
                }
            }
        }

        // Content validation failed: refund 69% of the proposal fee and reject.
        // The proposer was eligible (correct fee paid, holds shares) but the proposal content was invalid.
        if (!locals.contentValid)
        {
            locals.refundFee = (sint64)div((uint64)locals.requiredFee * 69, (uint64)100);
            qpi.getEntity(SELF, locals.entity);
            if (locals.refundFee > 0 && locals.refundFee <= (sint64)(locals.entity.incomingAmount - locals.entity.outgoingAmount)) { qpi.transfer(qpi.invocator(), (uint64)locals.refundFee); }
            return;
        }

        // Build proposal
        locals.prop.proposalType   = input.proposalType;
        locals.prop.status         = PROP_STATUS_ACTIVE;
        locals.prop.proposer       = qpi.invocator();
        locals.prop.assetName      = input.assetName;
        locals.prop.assetIssuer    = input.assetIssuer;
        locals.prop.poolIndex      = input.poolIndex;
        locals.prop.newValue       = input.newValue;
        locals.prop.withdrawAmount = input.withdrawAmount;
        locals.prop.destination    = input.destination;
        locals.prop.votesYes       = 0;
        locals.prop.votesNo        = 0;
        locals.prop.feePaid        = locals.requiredFee;

        locals.slot = state.get().proposalsThisEpoch;
        state.mut().proposals.set(locals.slot, locals.prop);
        state.mut().proposalsThisEpoch = state.get().proposalsThisEpoch + 1;

        // Proposal fees fund the execution-fee reserve (no supply burn, none kept as trading capital).
        // The non-refundable 31% is routed to the reserve now via qpi.burn(); on a PASS the held 69%
        // is also sent to the reserve at END_EPOCH, and on a FAIL that 69% is refunded to the proposer.
        locals.feeExecAmt = (sint64)div((uint64)locals.requiredFee * 31, (uint64)100);
        if (locals.feeExecAmt > 0)
        {
            qpi.burn((uint64)locals.feeExecAmt);
        }

        output.success = 1;
        output.slot    = locals.slot;
        // Remaining 69% held in balance: burned to the execution-fee reserve on a PASS, or refunded to the proposer on a FAIL.
    }

    // ---------------------------------------------------------------
    // Procedure: voteOnProposal
    // Any CLKnDGR shareholder may vote once per proposal slot per epoch.
    // Vote weight = current share balance. No fee required.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(voteOnProposal)
    {
        output.success = 0;

        // Validate slot
        if (input.slot >= state.get().proposalsThisEpoch) { return; }
        if (state.get().proposals.get(input.slot).status != PROP_STATUS_ACTIVE) { return; }

        // Check shareholder requirement
        locals.shareBalance = qpi.numberOfShares(
            state.get().selfAsset,
            AssetOwnershipSelect::byOwner(qpi.invocator()),
            AssetPossessionSelect::byPossessor(qpi.invocator())
        );
        if (locals.shareBalance <= 0) { return; }

        // Check for double vote on this slot (bit per slot in voterBitfield)
        locals.voterBitfield.setAll(0); // clear stale bits: HashMap.get leaves outValue unchanged on a miss, and QPI locals are not guaranteed zero-init (same defensive pattern used in END_EPOCH)
        state.get().proposalVoterMap.get(qpi.invocator(), locals.voterBitfield);
        if (locals.voterBitfield.get(input.slot) == 1) { return; } // already voted

        // Add voter to voterList if this is their first vote this epoch.
        // voterBitfield was read above before any bit was set; if all slot bits are 0 the voter is new.
        locals.isFirstVote = 1;
        for (locals.s = 0; locals.s < MAX_PROPOSALS_PER_EPOCH; locals.s = locals.s + 1)
        {
            if (locals.voterBitfield.get(locals.s) == 1) { locals.isFirstVote = 0; }
        }
        // If this is a new voter and voterList is at capacity, reject the vote entirely.
        // Accepting the vote but omitting the voter from voterList would cause their votes
        // to be silently dropped at END_EPOCH when the list is iterated for re-verification.
        if (locals.isFirstVote == 1 && state.get().voterCount >= PROPOSAL_VOTER_CAPACITY) { return; }
        if (locals.isFirstVote == 1)
        {
            state.mut().voterList.set((uint64)state.get().voterCount, qpi.invocator());
            state.mut().voterCount = state.get().voterCount + 1;
        }

        // Record YES/NO choice — do NOT aggregate weight now.
        // END_EPOCH will re-verify the voter's current share balance at tally time,
        // so anyone who sells shares before vote consensus contributes 0 weight.
        locals.voteYesBitfield.setAll(0); // clear stale bits before get (HashMap miss leaves outValue unchanged)
        state.get().voterYesChoiceMap.get(qpi.invocator(), locals.voteYesBitfield);
        if (input.voteYes == 1) { locals.voteYesBitfield.set(input.slot, 1); }
        // (NO vote leaves the bit clear — absence means NO)
        state.mut().voterYesChoiceMap.set(qpi.invocator(), locals.voteYesBitfield);

        // Mark voter as having voted on this slot
        locals.voterBitfield.set(input.slot, 1);
        state.mut().proposalVoterMap.set(qpi.invocator(), locals.voterBitfield);

        output.success = 1;
    }

    // ---------------------------------------------------------------
    // Procedure: depositorVeto
    // Any vault depositor with >= depositorVoteMinQu locked may vote yes/no on
    // an active proposal. If NO votes reach DEPOSITOR_VETO_THRESHOLD before END_EPOCH,
    // the proposal is blocked even if shareholders passed it.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(depositorVeto)
    {
        output.success = 0;

        // Validate slot
        if (input.slot >= state.get().proposalsThisEpoch) { return; }
        if (state.get().proposals.get(input.slot).status != PROP_STATUS_ACTIVE) { return; }

        // Check caller is a depositor
        locals.depInfo.shares = 0; // HashMap miss leaves outValue unchanged
        state.get().depositorInfo.get(qpi.invocator(), locals.depInfo);
        locals.depositorShares = locals.depInfo.shares;
        if (locals.depositorShares <= 0) { return; }

        // Check locked value meets minimum (shares × current share price)
        locals.lockedQu = (sint64)((uint64)locals.depositorShares * (uint64)state.get().vaultSharePrice);
        if (locals.lockedQu < state.get().depositorVoteMinQu) { return; }

        // Prevent double vote on this slot
        locals.voteBitfield.setAll(0); // clear stale bits before get (HashMap miss leaves outValue unchanged; same fix as VoteOnProposal/END_EPOCH)
        state.get().depositorVoteMap.get(qpi.invocator(), locals.voteBitfield);
        if (locals.voteBitfield.get(input.slot) == 1) { return; }

        // Record that this depositor has voted on this slot
        locals.voteBitfield.set(input.slot, 1);
        if (input.voteYes == 0)
        {
            locals.voteBitfield.set((uint8)(32 + input.slot), 1); // mark NO in upper bits for END_EPOCH re-validation
        }
        state.mut().depositorVoteMap.set(qpi.invocator(), locals.voteBitfield);

        // Tally vote
        if (input.voteYes == 0)
        {
            state.mut().depositorNoVotes.set(input.slot, state.get().depositorNoVotes.get(input.slot) + 1);
        }
        else
        {
            state.mut().depositorYesVotes.set(input.slot, state.get().depositorYesVotes.get(input.slot) + 1);
        }

        output.success = 1;
    }

    // ---------------------------------------------------------------
    // Function: getProposal
    // ---------------------------------------------------------------
    PUBLIC_FUNCTION(getProposal)
    {
        if (input.slot >= MAX_PROPOSALS_PER_EPOCH) { return; }
        Proposal p = state.get().proposals.get(input.slot);
        output.proposalType   = p.proposalType;
        output.status         = p.status;
        output.proposer       = p.proposer;
        output.assetName      = p.assetName;
        output.assetIssuer    = p.assetIssuer;
        output.poolIndex      = p.poolIndex;
        output.newValue       = p.newValue;
        output.withdrawAmount = p.withdrawAmount;
        output.destination    = p.destination;
        output.votesYes          = p.votesYes;
        output.votesNo           = p.votesNo;
        output.feePaid           = p.feePaid;
        output.depositorVotesNo  = state.get().depositorNoVotes.get(input.slot);
        output.depositorVotesYes = state.get().depositorYesVotes.get(input.slot);
    }

    // ---------------------------------------------------------------
    // Function: getGovernanceParams
    // ---------------------------------------------------------------
    PUBLIC_FUNCTION(getGovernanceParams)
    {
        output.minProfitQu                = state.get().minProfitQu;
        output.proposalFeeDefault         = state.get().proposalFeeDefault;
        output.proposalFeeAddPool         = state.get().proposalFeeAddPool;
        output.proposalFeePayoutStructure = state.get().proposalFeePayoutStructure;
        output.payoutStructure            = state.get().payoutStructure;
        output.minVoterQuorum             = state.get().minVoterQuorum;
        output.proposalsThisEpoch         = state.get().proposalsThisEpoch;
        output.minReserveProfitPct        = state.get().minReserveProfitPct;
        output.depositorVoteMinQu         = state.get().depositorVoteMinQu;
        output.relockAddAmount            = state.get().relockAddAmount;
        output.execReserveFloor           = state.get().execReserveFloor;
        output.vixBreakoutFactor          = state.get().vixBreakoutFactor;
        output.vixAbsFloorBps             = state.get().vixAbsFloorBps;
        output.vixSampleInterval          = state.get().vixSampleInterval;
        output.swingSellPct               = state.get().swingSellPct;
        output.breakoutRescanTicks        = state.get().breakoutRescanTicks;
        output.inLimpMode                 = state.get().inLimpMode;
    }

    // ---------------------------------------------------------------
    // Procedure: vaultDeposit
    // Lock QU into the depositor vault for 26 epochs.
    // Deposit amount = qpi.invocationReward() (QU sent with the call).
    // Minimum: VAULT_MIN_SHARE_TIERS[vaultDepositTier] × vaultSharePrice.
    // One deposit per wallet per cycle; no top-ups allowed.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(vaultDeposit)
    {
        output.success      = 0;
        output.sharesIssued = 0;
        output.newSharePrice = state.get().vaultSharePrice;

        locals.depositAmount = (sint64)qpi.invocationReward();

        // Reject zero-value calls
        if (locals.depositAmount <= 0)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund
            return;
        }

        // Enforce minimum deposit (min shares × current share price)
        locals.minDeposit = (sint64)((uint64)vaultMinSharesForTier(state.get().vaultDepositTier) * (uint64)state.get().vaultSharePrice);
        if (locals.depositAmount < locals.minDeposit)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund
            return;
        }

        // No top-ups: reject if this wallet already holds active shares (O(1) HashMap check — fast path first)
        locals.found    = 0;
        locals.depInfo.shares = 0; // HashMap miss leaves outValue unchanged
        locals.found = (uint8)state.get().depositorInfo.get(qpi.invocator(), locals.depInfo);
        locals.existing = locals.depInfo.shares;
        if (locals.found && locals.existing > 0)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund
            return;
        }

        // VER-D1: block waitlisted callers from bypassing the queue when vault opens.
        // Without this, wallet W (on waitlist) could join vault directly, then BEGIN_EPOCH
        // promotion would overwrite W's shares and add a duplicate depositorList entry.
        for (locals.wlI = 0; locals.wlI < state.get().waitlistCount; locals.wlI = locals.wlI + 1)
        {
            if (state.get().waitlist.get(locals.wlI).wallet == qpi.invocator())
            {
                qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund — call waitlistWithdraw first
                return;
            }
        }

        // Vault full: add to waitlist sorted by descending deposit amount.
        if (state.get().depositorCount >= MAX_DEPOSITORS)
        {
            // Only scan waitlist when vault is full (skip O(n) scan in the common admission path)
            for (locals.wlI = 0; locals.wlI < state.get().waitlistCount; locals.wlI = locals.wlI + 1)
            {
                if (state.get().waitlist.get(locals.wlI).wallet == qpi.invocator())
                {
                    qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund — already queued
                    return;
                }
            }

            if (state.get().waitlistCount >= WAITLIST_SIZE)
            {
                // Waitlist full: displace the last entry (smallest stake) if caller offers strictly more.
                // Call getWaitlistPosition first to read minAmount before committing QU.
                if (locals.depositAmount <= state.get().waitlist.get(state.get().waitlistCount - 1).amount)
                {
                    qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // cannot displace — refund
                    return;
                }
                // Displace: refund the last entry immediately, shrink list by 1, then fall through to insert
                locals.wlEntry = state.get().waitlist.get(state.get().waitlistCount - 1);
                qpi.transfer(locals.wlEntry.wallet, (uint64)locals.wlEntry.amount);
                state.mut().waitlistQu    = state.get().waitlistQu - locals.wlEntry.amount;
                state.mut().waitlistCount = state.get().waitlistCount - 1;
                // waitlistCount is now WAITLIST_SIZE - 1; sorted insert below fits exactly
            }
            // Build entry
            locals.wlEntry.wallet = qpi.invocator();
            locals.wlEntry.amount = locals.depositAmount;
            // Find sorted insertion position (largest first)
            locals.wlI = state.get().waitlistCount; // default: append
            for (locals.wlJ = 0; locals.wlJ < state.get().waitlistCount; locals.wlJ = locals.wlJ + 1)
            {
                if (state.get().waitlist.get(locals.wlJ).amount < locals.depositAmount)
                {
                    locals.wlI = locals.wlJ;
                    break;
                }
            }
            // Shift entries right to make room at locals.wlI
            for (locals.wlJ = state.get().waitlistCount; locals.wlJ > locals.wlI; locals.wlJ = locals.wlJ - 1)
            {
                state.mut().waitlist.set(locals.wlJ, state.get().waitlist.get(locals.wlJ - 1));
            }
            state.mut().waitlist.set(locals.wlI, locals.wlEntry);
            state.mut().waitlistCount = state.get().waitlistCount + 1;
            state.mut().waitlistQu    = state.get().waitlistQu + locals.depositAmount;
            output.success = 2; // queued
            return;
        }

        // Calculate shares to issue (floor division; any remainder stays as excess capital)
        locals.sharesIssued = (sint64)div((uint64)locals.depositAmount, (uint64)state.get().vaultSharePrice);
        if (locals.sharesIssued <= 0)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.depositAmount); // refund
            return;
        }

        // Record depositor state (slot consumed only on success)
        locals.depInfo.shares    = locals.sharesIssued;
        locals.depInfo.costBasis = locals.depositAmount;
        locals.depInfo.epoch     = (uint32)qpi.epoch();
        state.mut().depositorInfo.set(qpi.invocator(), locals.depInfo);
        // Append to depositorList so the auto-payout loop can find this wallet at epoch start
        state.mut().depositorList.set(state.get().depositorCount, qpi.invocator());

        // Update vault totals
        state.mut().totalVaultShares   = state.get().totalVaultShares   + locals.sharesIssued;
        state.mut().totalDepositorPool = state.get().totalDepositorPool + locals.depositAmount;
        state.mut().depositorCount     = state.get().depositorCount + 1;

        // Share price is unchanged by a deposit (NAV-neutral: pool and shares grow proportionally)

        // Update prevTradingBalance so mid-epoch deposits are not treated as trading profit
        // by the NAV ratio computation (navRatio = vaultCurBalance / prevTradingBalance)
        // at the start of the next epoch.
        // Without this update, Bob's 1M deposit after Alice's seed would make navRatio = 2×
        // even with zero arb profit, doubling the pool on paper while the contract holds
        // only the original 2M QU — an insolvency that drains Alice on exit and leaves Bob
        // with nothing.
        // First deposit: seed from contractBalance (prevTB is 0 after init or full drain).
        // Subsequent deposits: add the new capital to the existing baseline so navRatio
        //   only captures actual arb P&L, not new capital inflow.
        if (state.get().prevTradingBalance == 0)
        {
            qpi.getEntity(SELF, locals.entity);
            state.mut().prevTradingBalance = (sint64)(locals.entity.incomingAmount - locals.entity.outgoingAmount)
                                             - state.get().quReserve
                                             - state.get().qearnReserve
                                             - state.get().waitlistQu;
        }
        else
        {
            state.mut().prevTradingBalance = state.get().prevTradingBalance + locals.depositAmount;
        }

        output.success       = 1;
        output.sharesIssued  = locals.sharesIssued;
        output.newSharePrice = state.get().vaultSharePrice;
    }

    // ---------------------------------------------------------------
    // Procedure: waitlistWithdraw
    // Removes the caller from the waitlist and refunds their QU.
    // Must be called before BEGIN_EPOCH processes the waitlist; once
    // promoted to an active depositor the 26-epoch vault lock applies.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(waitlistWithdraw)
    {
        output.success       = 0;
        output.amountRefunded = 0;

        for (locals.i = 0; locals.i < state.get().waitlistCount; locals.i = locals.i + 1)
        {
            if (state.get().waitlist.get(locals.i).wallet == qpi.invocator())
            {
                output.amountRefunded = state.get().waitlist.get(locals.i).amount;
                // Shift waitlist left to close the gap
                for (locals.j = locals.i; locals.j + 1 < state.get().waitlistCount; locals.j = locals.j + 1)
                {
                    state.mut().waitlist.set(locals.j, state.get().waitlist.get(locals.j + 1));
                }
                state.mut().waitlistCount = state.get().waitlistCount - 1;
                state.mut().waitlistQu    = state.get().waitlistQu - output.amountRefunded;
                if (output.amountRefunded > 0)
                {
                    qpi.transfer(qpi.invocator(), (uint64)output.amountRefunded);
                }
                output.success = 1;
                return;
            }
        }
    }

    // ---------------------------------------------------------------
    // Procedure: vaultWithdraw
    // Allows an active depositor to exit the vault at any time.
    // If the 26-epoch personal lock has expired: normal fees apply
    //   (2% management fee burned + 5% performance fee on profit).
    // If still within the lock period: 2% management fee + 38% early
    //   withdrawal penalty (= 40% total). The 38% penalty is added to
    //   epochProfit and distributes through the standard payout split.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(vaultWithdraw)
    {
        output.success        = 0;
        output.amountReturned = 0;
        output.penaltyApplied = 0;

        // Verify caller is an active depositor
        locals.found  = 0;
        locals.depInfo.shares = 0; locals.depInfo.costBasis = 0; locals.depInfo.epoch = 0;
        locals.found = (uint8)state.get().depositorInfo.get(qpi.invocator(), locals.depInfo);
        locals.shares = locals.depInfo.shares;
        if (!locals.found || locals.shares <= 0) { return; }

        // EC-W1: zero before each get — a HashMap miss on either map leaves the value at 0.
        // depEpoch=0 → lock appears expired (under-charges); costBasis=0 → full gross treated as profit
        // (over-charges perfFee). Both are wrong but neither creates a drain vector; proceeding is
        // safer than aborting (abort would permanently lock the depositor's funds on desync).
        locals.depEpoch  = locals.depInfo.epoch;
        locals.costBasis = locals.depInfo.costBasis;
        locals.gross     = (sint64)((uint64)locals.shares * (uint64)state.get().vaultSharePrice);

        // Management fee: 2% of gross → qpi.burn() (same as normal auto-payout exit)
        locals.mgmtFee = (sint64)div((uint64)locals.gross * VAULT_MANAGEMENT_FEE_PCT, (uint64)100);

        if ((uint32)qpi.epoch() >= locals.depEpoch + (uint32)VAULT_LOCK_EPOCHS)
        {
            // Lock expired — apply standard exit fees (same as auto-payout in BEGIN_EPOCH)
            locals.profit  = locals.gross - locals.costBasis;
            locals.perfFee = 0;
            if (locals.profit > 0)
            {
                locals.perfFee = (sint64)div((uint64)locals.profit * VAULT_PERFORMANCE_FEE_PCT, (uint64)100);
                state.mut().epochProfit = state.get().epochProfit + locals.perfFee;
            }
            locals.net            = locals.gross - locals.mgmtFee - locals.perfFee;
            output.penaltyApplied = 0;
            output.success        = 1;
        }
        else
        {
            // Early exit — 38% penalty on gross flows to epochProfit for distribution
            locals.earlyPenalty     = (sint64)div((uint64)locals.gross * VAULT_EARLY_WITHDRAWAL_PENALTY_PCT, (uint64)100);
            state.mut().epochProfit = state.get().epochProfit + locals.earlyPenalty;
            locals.net              = locals.gross - locals.mgmtFee - locals.earlyPenalty;
            output.penaltyApplied   = locals.mgmtFee + locals.earlyPenalty;
            output.success          = 2;
        }

        // Clear depositor HashMap records — removeByKey marks slot as recyclable tombstone;
        // set(key,0) permanently occupies the slot (QPI HashMap behavior).
        state.mut().depositorInfo.removeByKey(qpi.invocator());
        state.mut().depositorInfo.cleanupIfNeeded();

        // Update vault totals
        state.mut().totalVaultShares   = state.get().totalVaultShares   - locals.shares;
        state.mut().totalDepositorPool = state.get().totalDepositorPool - locals.gross;
        // Decrement prevTradingBalance by the gross withdrawn so that the next epoch's
        // NAV ratio (vaultCurBalance / prevTradingBalance) sees a correctly reduced baseline
        // and does not report a phantom loss for depositors who remain.
        if (state.get().prevTradingBalance >= locals.gross)
        {
            state.mut().prevTradingBalance = state.get().prevTradingBalance - locals.gross;
        }
        else
        {
            state.mut().prevTradingBalance = 0;
        }

        // Remove from depositorList via swap-with-last (before decrementing depositorCount)
        locals.foundInList = 0;
        for (locals.k = 0; locals.k < state.get().depositorCount; locals.k = locals.k + 1)
        {
            if (state.get().depositorList.get(locals.k) == qpi.invocator())
            {
                state.mut().depositorList.set(locals.k,
                    state.get().depositorList.get(state.get().depositorCount - 1));
                locals.foundInList = 1;
                break;
            }
        }
        if (locals.foundInList && state.get().depositorCount > 0)
        {
            state.mut().depositorCount = state.get().depositorCount - 1;
        }

        // Recompute share price; reset if vault is now empty
        if (state.get().totalVaultShares > 0)
        {
            state.mut().vaultSharePrice = (sint64)div((uint64)state.get().totalDepositorPool,
                                                       (uint64)state.get().totalVaultShares);
            if (state.get().vaultSharePrice <= 0) { state.mut().vaultSharePrice = 1; }
        }
        else
        {
            state.mut().vaultSharePrice    = VAULT_INITIAL_SHARE_PRICE;
            state.mut().totalDepositorPool = 0;
            state.mut().prevTradingBalance = 0;
            // Reset depositor HashMaps to recycle stale key slots from all prior depositors.
            // Without this, key-space exhaustion prevents new deposits even when depositorCount == 0.
            // BEGIN_EPOCH zombie cleanup does the same, but only fires at the next epoch boundary.
            state.mut().depositorInfo.reset();
        }

        // Execute transfers
        if (locals.mgmtFee > 0) { qpi.burn(locals.mgmtFee); }
        if (locals.net > 0)     { qpi.transfer(qpi.invocator(), (uint64)locals.net); }

        output.amountReturned = locals.net;
    }

    // ---------------------------------------------------------------
    // Procedure: vaultRelock
    // Extends the caller's 26-epoch vault lock within the last
    // VAULT_RELOCK_WINDOW_EPOCHS epochs of their current lock.
    // The caller must send at least relockAddAmount additional QU.
    // On success: the depositor's lock epoch resets to qpi.epoch() (fresh 26-epoch
    // lock), and new shares are issued at the current vaultSharePrice.
    // success codes: 0=not a depositor, 1=window not yet open,
    //   2=lock already expired (use vaultWithdraw), 3=insufficient QU,
    //   4=success
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(vaultRelock)
    {
        output.success     = 0;
        output.sharesIssued = 0;
        output.newDepEpoch  = 0;

        // Verify caller is an active depositor
        locals.found    = 0;
        locals.depInfo.shares = 0; locals.depInfo.costBasis = 0; locals.depInfo.epoch = 0;
        locals.found = (uint8)state.get().depositorInfo.get(qpi.invocator(), locals.depInfo);
        locals.curShares = locals.depInfo.shares;
        if (!locals.found || locals.curShares <= 0) { return; } // success=0: not a depositor

        // EC-VR1: zero before get — a miss leaves the value at 0 (depEpoch=0 causes false
        // expiry rejection; curCostBasis=0 loses prior cost history at next withdrawal).
        locals.depEpoch     = locals.depInfo.epoch;
        locals.curCostBasis = locals.depInfo.costBasis;
        locals.addAmount    = (sint64)qpi.invocationReward();

        // Lock must not have already expired — use vaultWithdraw instead
        if ((uint32)qpi.epoch() >= locals.depEpoch + (uint32)VAULT_LOCK_EPOCHS)
        {
            if (locals.addAmount > 0) { qpi.transfer(qpi.invocator(), (uint64)locals.addAmount); }
            output.success = 2;
            return;
        }

        // Must be within the last VAULT_RELOCK_WINDOW_EPOCHS epochs of the lock
        if ((uint32)qpi.epoch() < locals.depEpoch + (uint32)VAULT_LOCK_EPOCHS - (uint32)VAULT_RELOCK_WINDOW_EPOCHS)
        {
            if (locals.addAmount > 0) { qpi.transfer(qpi.invocator(), (uint64)locals.addAmount); }
            output.success = 1;
            return;
        }

        // Enforce minimum additional deposit
        if (locals.addAmount < state.get().relockAddAmount)
        {
            if (locals.addAmount > 0) { qpi.transfer(qpi.invocator(), (uint64)locals.addAmount); }
            output.success = 3;
            return;
        }

        // Issue new shares for the additional QU at current share price
        locals.newShares = (sint64)div((uint64)locals.addAmount, (uint64)state.get().vaultSharePrice);
        if (locals.newShares <= 0)
        {
            qpi.transfer(qpi.invocator(), (uint64)locals.addAmount); // refund — can't buy a full share
            output.success = 3;
            return;
        }

        // Reset lock epoch to now and update depositor records
        locals.depInfo.epoch     = (uint32)qpi.epoch();
        locals.depInfo.shares    = locals.curShares + locals.newShares;
        locals.depInfo.costBasis = locals.curCostBasis + locals.addAmount;
        state.mut().depositorInfo.set(qpi.invocator(), locals.depInfo);

        // Update vault totals (NAV-neutral: pool and shares grow proportionally)
        state.mut().totalVaultShares   = state.get().totalVaultShares   + locals.newShares;
        state.mut().totalDepositorPool = state.get().totalDepositorPool + locals.addAmount;
        // Update prevTradingBalance baseline so the relock top-up is not treated as
        // trading profit by the NAV ratio at next epoch start.
        state.mut().prevTradingBalance = state.get().prevTradingBalance + locals.addAmount;

        output.success     = 4;
        output.sharesIssued = locals.newShares;
        output.newDepEpoch  = (uint32)qpi.epoch();
    }

    // ---------------------------------------------------------------
    // Procedure: donateToContract
    // Open to anyone — send ANY positive amount of QU. 100% is burned into this contract's own
    // execution-fee reserve, keeping it running. Zero (or nothing) sent is a no-op.
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(donateToContract)
    {
        output.success            = 0;
        output.toExecutionReserve = 0;

        locals.amt = (sint64)qpi.invocationReward();
        if (locals.amt <= 0) { return; } // nothing sent — nothing to donate

        // 100% of whatever was sent → this contract's own execution-fee reserve.
        // qpi.burn(amount) debits the contract's QU balance and credits the same amount to its
        // fee reserve (qpi_spectrum_impl.h: addToContractFeeReserve). No refund — the full amount is kept.
        qpi.burn(locals.amt);

        output.success            = 1;
        output.toExecutionReserve = locals.amt;
    }

    // ---------------------------------------------------------------
    // Procedure: publicDonate
    // Open to any wallet — send ANY positive amount of QU. 100% is burned into this contract's own
    // execution-fee reserve, keeping it running. Zero (or nothing) sent is a no-op.
    // (Identical behavior to donateToContract — both kept as separate entry points.)
    // ---------------------------------------------------------------
    PUBLIC_PROCEDURE_WITH_LOCALS(publicDonate)
    {
        output.success            = 0;
        output.toExecutionReserve = 0;

        locals.amt = (sint64)qpi.invocationReward();
        if (locals.amt <= 0) { return; } // nothing sent — nothing to donate

        // 100% of whatever was sent → this contract's own execution-fee reserve (see donateToContract).
        qpi.burn(locals.amt);

        output.success            = 1;
        output.toExecutionReserve = locals.amt;
    }

    // ---------------------------------------------------------------
    // Function: getWaitlistPosition
    // Returns the caller's current position in the waitlist (1-indexed,
    // sorted largest-first), their queued amount, and total queue depth.
    // Returns onWaitlist=0 if the caller is not in the queue.
    // ---------------------------------------------------------------
    PUBLIC_FUNCTION_WITH_LOCALS(getWaitlistPosition)
    {
        output.onWaitlist   = 0;
        output.position     = 0;
        output.amount       = 0;
        output.totalWaiting = state.get().waitlistCount;
        output.isFull       = (state.get().waitlistCount >= WAITLIST_SIZE) ? 1 : 0;
        // Smallest entry is always the last (waitlist is sorted largest-first)
        output.minAmount    = (state.get().waitlistCount > 0)
                              ? state.get().waitlist.get(state.get().waitlistCount - 1).amount
                              : 0;

        for (locals.i = 0; locals.i < state.get().waitlistCount; locals.i = locals.i + 1)
        {
            if (state.get().waitlist.get(locals.i).wallet == qpi.invocator())
            {
                output.onWaitlist = 1;
                output.position   = locals.i + 1; // 1-indexed: position 1 = next to be promoted
                output.amount     = state.get().waitlist.get(locals.i).amount;
                return;
            }
        }
    }

    // Only allow QX and Qswap to release management rights to CLKnDGR.
    // Rejecting all other sources prevents unknown contracts from pushing
    // untracked tokens onto CLKnDGR with no cost basis or registered pool.
    PRE_ACQUIRE_SHARES()
    {
        output.allowTransfer = (input.otherContractIndex == QX_CONTRACT_INDEX ||
                                input.otherContractIndex == QSWAP_CONTRACT_INDEX);
    }
};
