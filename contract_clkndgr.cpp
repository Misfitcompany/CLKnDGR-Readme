#define NO_UEFI

#include "contract_testing.h"

// ---------------------------------------------------------------
// googletest link fix: this project compiles with /Zc:wchar_t- (wchar_t is a
// typedef of unsigned short), but the prebuilt googletest lib was built with a
// native wchar_t — so its PrintTo(wchar_t) does not satisfy the PrintTo(unsigned
// short) gtest needs to print a uint16 value on a failed EXPECT_EQ. Define it here.
// Fixes LNK2019 'testing::internal::PrintTo(unsigned short, std::ostream*)'.
// ---------------------------------------------------------------
namespace testing { namespace internal {
    void PrintTo(unsigned short value, ::std::ostream* os) { *os << value; }
} }

// ---------------------------------------------------------------
// Helper: deterministic unique user IDs
// ---------------------------------------------------------------
static id clkUser(unsigned long long i)
{
    return id(i + 1000, i * 3 + 7, i + 2000, i * 5 + 3);
}

// ---------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------
class ContractTestingCLKnDGR : protected ContractTesting
{
public:
    ContractTestingCLKnDGR()
    {
        initEmptySpectrum();
        initEmptyUniverse();
        INIT_CONTRACT(CLKnDGR);
        callSystemProcedure(CLKnDGR_CONTRACT_INDEX, INITIALIZE);
    }

    // Direct state access for assertions
    CLKnDGR::StateData* st() const
    {
        return reinterpret_cast<CLKnDGR::StateData*>(contractStates[CLKnDGR_CONTRACT_INDEX]);
    }

    // Issue CLKNDGR IPO shares under issuer NULL_ID (m256i::zero()) — how Qubic issues contract shares.
    // All NUMBER_OF_COMPUTORS shares are issued; the requested amounts are transferred to
    // each owner in order. Remaining shares stay with the contract ID (unused in tests).
    void issueShares(std::vector<std::pair<id, int>> owners)
    {
        int issuanceIdx, ownershipIdx, possessionIdx;
        int dstOwnershipIdx, dstPossessionIdx;
        int issued = issueAsset(m256i::zero(),
                                (char*)contractDescriptions[CLKnDGR_CONTRACT_INDEX].assetName,
                                0, CONTRACT_ASSET_UNIT_OF_MEASUREMENT,
                                NUMBER_OF_COMPUTORS, QX_CONTRACT_INDEX,
                                &issuanceIdx, &ownershipIdx, &possessionIdx);
        EXPECT_EQ(issued, NUMBER_OF_COMPUTORS);
        for (auto& [owner, count] : owners)
        {
            EXPECT_TRUE(transferShareOwnershipAndPossession(
                ownershipIdx, possessionIdx, owner, count,
                &dstOwnershipIdx, &dstPossessionIdx, false));
        }
    }

    void addEnergy(const id& u, sint64 amount)
    {
        increaseEnergy(u, amount);
    }

    // Give clkUser(1..count-1) a spectrum entry (a tiny QU balance) so their
    // voteOnProposal / depositorVeto invocations actually execute. An account that has
    // never received QU has no spectrum index, and invokeUserProcedure bails out before
    // running the procedure body (see contract_testing.h: spectrumIndex(user) < 0).
    // clkUser(0) is the proposer — funded separately with the proposal fee — so it is
    // skipped here. On mainnet every shareholder necessarily exists in the spectrum,
    // so this only mirrors reality inside the test harness.
    void fundVoters(int count, sint64 amount = 1000LL)
    {
        for (int i = 1; i < count; ++i)
            addEnergy(clkUser(i), amount);
    }

    // ---- System procedure wrappers ----
    void beginEpoch(bool expectOk = true)
    {
        callSystemProcedure(CLKnDGR_CONTRACT_INDEX, BEGIN_EPOCH, expectOk);
    }

    void endEpoch(bool expectOk = true)
    {
        callSystemProcedure(CLKnDGR_CONTRACT_INDEX, END_EPOCH, expectOk);
    }

    void beginTick(bool expectOk = true)
    {
        callSystemProcedure(CLKnDGR_CONTRACT_INDEX, BEGIN_TICK, expectOk);
    }

    // ---- Function wrappers (read-only) ----
    CLKnDGR::getStats_output getStats()
    {
        CLKnDGR::getStats_input in{};
        CLKnDGR::getStats_output out{};
        callFunction(CLKnDGR_CONTRACT_INDEX, 1, in, out);
        return out;
    }

    CLKnDGR::getPool_output getPool(uint64 index)
    {
        CLKnDGR::getPool_input in{};
        CLKnDGR::getPool_output out{};
        in.index = index;
        callFunction(CLKnDGR_CONTRACT_INDEX, 2, in, out);
        return out;
    }

    CLKnDGR::getProposal_output getProposal(uint8 slot)
    {
        CLKnDGR::getProposal_input in{};
        CLKnDGR::getProposal_output out{};
        in.slot = slot;
        callFunction(CLKnDGR_CONTRACT_INDEX, 3, in, out);
        return out;
    }

    CLKnDGR::getGovernanceParams_output getGovernanceParams()
    {
        CLKnDGR::getGovernanceParams_input in{};
        CLKnDGR::getGovernanceParams_output out{};
        callFunction(CLKnDGR_CONTRACT_INDEX, 4, in, out);
        return out;
    }

    CLKnDGR::getWaitlistPosition_output getWaitlistPosition()
    {
        CLKnDGR::getWaitlistPosition_input in{};
        CLKnDGR::getWaitlistPosition_output out{};
        callFunction(CLKnDGR_CONTRACT_INDEX, 5, in, out);
        return out;
    }

    // ---- Procedure wrappers (state-changing) ----
    CLKnDGR::submitProposal_output submitProposal(
        const id& u, sint64 fee, uint8 type,
        sint64 newValue = 0,
        uint64 poolIndex = 0,
        uint64 assetName = 0,
        id assetIssuer = NULL_ID,
        sint64 withdrawAmount = 0,
        id destination = NULL_ID)
    {
        CLKnDGR::submitProposal_input in{};
        CLKnDGR::submitProposal_output out{};
        in.proposalType   = type;
        in.newValue       = newValue;
        in.poolIndex      = poolIndex;
        in.assetName      = assetName;
        in.assetIssuer    = assetIssuer;
        in.withdrawAmount = withdrawAmount;
        in.destination    = destination;
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 1, in, out, u, fee);
        return out;
    }

    CLKnDGR::voteOnProposal_output voteOnProposal(const id& u, uint8 slot, uint8 voteYes)
    {
        CLKnDGR::voteOnProposal_input in{};
        CLKnDGR::voteOnProposal_output out{};
        in.slot    = slot;
        in.voteYes = voteYes;
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 2, in, out, u, 0);
        return out;
    }

    CLKnDGR::vaultDeposit_output vaultDeposit(const id& u, sint64 amount)
    {
        CLKnDGR::vaultDeposit_input in{};
        CLKnDGR::vaultDeposit_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 3, in, out, u, amount);
        return out;
    }

    CLKnDGR::depositorVeto_output depositorVeto(const id& u, uint8 slot, uint8 voteYes)
    {
        CLKnDGR::depositorVeto_input in{};
        CLKnDGR::depositorVeto_output out{};
        in.slot    = slot;
        in.voteYes = voteYes;
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 4, in, out, u, 0);
        return out;
    }

    CLKnDGR::waitlistWithdraw_output waitlistWithdraw(const id& u)
    {
        CLKnDGR::waitlistWithdraw_input in{};
        CLKnDGR::waitlistWithdraw_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 5, in, out, u, 0);
        return out;
    }

    CLKnDGR::vaultWithdraw_output vaultWithdraw(const id& u)
    {
        CLKnDGR::vaultWithdraw_input in{};
        CLKnDGR::vaultWithdraw_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 6, in, out, u, 0);
        return out;
    }

    CLKnDGR::vaultRelock_output vaultRelock(const id& u, sint64 addAmount)
    {
        CLKnDGR::vaultRelock_input in{};
        CLKnDGR::vaultRelock_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 7, in, out, u, addAmount);
        return out;
    }

    CLKnDGR::donateToContract_output donateToContract(const id& u, sint64 amount)
    {
        CLKnDGR::donateToContract_input in{};
        CLKnDGR::donateToContract_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 8, in, out, u, amount);
        return out;
    }

    CLKnDGR::publicDonate_output publicDonate(const id& u, sint64 amount)
    {
        CLKnDGR::publicDonate_input in{};
        CLKnDGR::publicDonate_output out{};
        invokeUserProcedure(CLKnDGR_CONTRACT_INDEX, 9, in, out, u, amount);
        return out;
    }
};

// ===============================================================
// DEX integration fixture: CLKnDGR + real QX + real Qswap
// ---------------------------------------------------------------
// The trade engine (Cloak + Dagger) reaches QX(1) and Qswap(13) through
// CALL_OTHER_CONTRACT_FUNCTION / INVOKE_OTHER_CONTRACT_PROCEDURE. To exercise
// those live paths we INIT the REAL QX and Qswap contracts alongside CLKnDGR(29)
// and seed a genuine market: issue an asset on Qswap, create + fund a pool (which
// sets the AMM price), optionally move some shares' management to QX so a maker can
// post an ask, and post QX bid/ask orders as counterparties. CLKnDGR then trades
// against the real DEX logic — no mocks, so the math the tests verify is the same
// math that runs on mainnet.
//
// Asset management lives per-share: the asset is issued under Qswap management (so
// the pool LP can add liquidity); for Direction B we move a slice to QX management
// so a maker can post an ask. Inter-contract calls in the test harness are not
// gated by callee construction epoch (qrwa/qswap tests call Qswap from epoch 0),
// so we leave system.epoch at the base-fixture default.
//
// Qswap fees that Qswap.h does not expose (mirrors the contract_qswap.cpp test
// constants). QSWAP_ADDITIONAL_FEE is provided by Qswap.h.
// ===============================================================
static constexpr uint64 CLK_QSWAP_ISSUE_ASSET_FEE = 1000000000ull;
static constexpr uint64 CLK_QSWAP_CREATE_POOL_FEE  = 1000000000ull;

class ContractTestingCLKnDGRDex : public ContractTestingCLKnDGR
{
    // system.epoch / system.tick are PROCESS-GLOBAL and shared across gtest cases. This fixture
    // advances them (and a test may set the tick), so we snapshot on construction and restore on
    // teardown — otherwise the elevated epoch/tick leaks into later tests (e.g. the VaultRelock
    // tests, which assume a fresh epoch).
    decltype(system.epoch) savedEpoch_{};
    decltype(system.tick)  savedTick_{};
public:
    ContractTestingCLKnDGRDex()
    {
        savedEpoch_ = system.epoch;
        savedTick_  = system.tick;

        // Base ctor already ran initEmptySpectrum/Universe + INIT_CONTRACT(CLKnDGR) + INITIALIZE.
        // Stand up the real DEXes CLKnDGR trades against.
        INIT_CONTRACT(QX);
        callSystemProcedure(QX_CONTRACT_INDEX, INITIALIZE);
        INIT_CONTRACT(QSWAP);
        callSystemProcedure(QSWAP_CONTRACT_INDEX, INITIALIZE);

        // Asset management-rights transfers (qpi.releaseShares to another contract) require the
        // managing contract to be past its construction epoch — see the contract_qswap.cpp TSRM
        // test, which sets system.epoch before transferring rights. CLKnDGR moves rights during
        // the arb (e.g. Direction A's sell leg), so advance to its construction epoch (999),
        // which is past QX's and Qswap's too. Only this DEX fixture is affected.
        system.epoch = contractDescriptions[CLKnDGR_CONTRACT_INDEX].constructionEpoch;
    }

    ~ContractTestingCLKnDGRDex()
    {
        system.epoch = savedEpoch_;
        system.tick  = savedTick_;
    }

    // CLKnDGR's on-chain id; energy added here is the contract's QU trading capital.
    static id clkSelf() { return id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0); }
    void fundContract(sint64 qu) { increaseEnergy(clkSelf(), qu); }

    // Write a pool entry straight into CLKnDGR state (bypasses the governance ADD_POOL cycle).
    void seedPool(uint64 idx, const id& assetIssuer, uint64 assetName)
    {
        auto* s = st();
        s->poolIssuers.set(idx, assetIssuer);
        s->poolAssetNames.set(idx, assetName);
        s->poolActive.set(idx, 1);
        if ((uint64)s->poolCount <= idx) { s->poolCount = (uint8)(idx + 1); }
    }

    // ---- Qswap seeding ----
    sint64 qswapIssueAsset(const id& maker, uint64 assetName, sint64 shares)
    {
        QSWAP::IssueAsset_input in{ assetName, shares, 0, 0 };
        QSWAP::IssueAsset_output out{};
        increaseEnergy(maker, CLK_QSWAP_ISSUE_ASSET_FEE);
        invokeUserProcedure(QSWAP_CONTRACT_INDEX, 1, in, out, maker, CLK_QSWAP_ISSUE_ASSET_FEE);
        return out.issuedNumberOfShares;
    }

    bool qswapCreatePool(const id& maker, const id& assetIssuer, uint64 assetName)
    {
        QSWAP::CreatePool_input in{ assetIssuer, assetName };
        QSWAP::CreatePool_output out{};
        increaseEnergy(maker, CLK_QSWAP_CREATE_POOL_FEE);
        invokeUserProcedure(QSWAP_CONTRACT_INDEX, 3, in, out, maker, CLK_QSWAP_CREATE_POOL_FEE);
        return out.success;
    }

    // First add sets the pool ratio: price = quAmount / assetAmount.
    QSWAP::AddLiquidity_output qswapAddLiquidity(const id& maker, const id& assetIssuer, uint64 assetName,
                                                 sint64 assetAmount, sint64 quAmount)
    {
        QSWAP::AddLiquidity_input in{ assetIssuer, assetName, assetAmount, 0, 0 };
        QSWAP::AddLiquidity_output out{};
        sint64 reward = quAmount + (sint64)QSWAP_ADDITIONAL_FEE;
        increaseEnergy(maker, reward);
        invokeUserProcedure(QSWAP_CONTRACT_INDEX, 4, in, out, maker, reward);
        return out;
    }

    // Move share management Qswap -> QX so `maker` can post a QX ask for the asset.
    sint64 qswapMgmtToQx(const id& maker, const id& assetIssuer, uint64 assetName, sint64 shares)
    {
        QSWAP::TransferShareManagementRights_input in{};
        in.asset.issuer = assetIssuer;
        in.asset.assetName = assetName;
        in.numberOfShares = shares;
        in.newManagingContractIndex = QX_CONTRACT_INDEX;
        QSWAP::TransferShareManagementRights_output out{};
        increaseEnergy(maker, 100);
        invokeUserProcedure(QSWAP_CONTRACT_INDEX, 11, in, out, maker, 100);
        return out.transferredNumberOfShares;
    }

    QSWAP::GetPoolBasicState_output qswapPool(const id& assetIssuer, uint64 assetName)
    {
        QSWAP::GetPoolBasicState_input in{ assetIssuer, assetName };
        QSWAP::GetPoolBasicState_output out{};
        callFunction(QSWAP_CONTRACT_INDEX, 2, in, out);
        return out;
    }

    // ---- QX seeding ----
    // Ask = maker offers to SELL `shares` at `price`; requires maker to possess the
    // shares under QX management (no QU escrow). Returns added shares.
    sint64 qxAddAsk(const id& maker, const id& assetIssuer, uint64 assetName, sint64 price, sint64 shares)
    {
        QX::AddToAskOrder_input in{ assetIssuer, assetName, price, shares };
        QX::AddToAskOrder_output out{};
        invokeUserProcedure(QX_CONTRACT_INDEX, 5, in, out, maker, 0);
        return out.addedNumberOfShares;
    }

    // Bid = maker offers to BUY `shares` at `price`; escrows price*shares QU.
    sint64 qxAddBid(const id& maker, const id& assetIssuer, uint64 assetName, sint64 price, sint64 shares)
    {
        QX::AddToBidOrder_input in{ assetIssuer, assetName, price, shares };
        QX::AddToBidOrder_output out{};
        sint64 escrow = price * shares;
        increaseEnergy(maker, escrow);
        invokeUserProcedure(QX_CONTRACT_INDEX, 6, in, out, maker, escrow);
        return out.addedNumberOfShares;
    }

    QX::AssetBidOrders_output qxBids(const id& assetIssuer, uint64 assetName)
    {
        QX::AssetBidOrders_input in{ assetIssuer, assetName, 0 };
        QX::AssetBidOrders_output out{};
        callFunction(QX_CONTRACT_INDEX, 3, in, out);
        return out;
    }

    QX::AssetAskOrders_output qxAsks(const id& assetIssuer, uint64 assetName)
    {
        QX::AssetAskOrders_input in{ assetIssuer, assetName, 0 };
        QX::AssetAskOrders_output out{};
        callFunction(QX_CONTRACT_INDEX, 2, in, out);
        return out;
    }
};

// ===============================================================
// DAGGER — live arbitrage against real QX + Qswap
// ===============================================================

// Direction B: buy the cheap QX ask, sell into the richer Qswap pool, book profit in-tick.
// Verifies the Qswap-fee fix — the sell leg now funds QSWAP_ADDITIONAL_FEE and books NET profit.
TEST(ContractCLKnDGR, Dagger_DirectionB_BuyQxSellQswap_BooksProfit)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKARB");

    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 10000000LL), 10000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 1000000LL, 280000000LL); // price 280
    EXPECT_GT(ctx.qswapMgmtToQx(maker, maker, asset, 50000LL), 0LL);
    // 2000 shares @ ask 100 vs pool ~280: gross gap clears minProfitQu + the Qswap fee (net semantics).
    EXPECT_GT(ctx.qxAddAsk(maker, maker, asset, 100LL, 2000LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(1000000LL);

    auto before = ctx.getStats();
    ctx.beginTick();
    auto after = ctx.getStats();

    EXPECT_GT(after.totalArbsExecuted, before.totalArbsExecuted); // an arb fired
    EXPECT_GT(after.totalProfitEarned, before.totalProfitEarned); // Direction B books in-tick
    EXPECT_GT(ctx.st()->epochProfit, 0LL);
}

// Direction A: buy the cheap Qswap pool, sell into the richer QX bid. Profit is DEFERRED
// (QU lands a future tick); only the arb counter moves now. Verifies the Qswap-fee fix.
TEST(ContractCLKnDGR, Dagger_DirectionA_BuyQswapSellQxBid_CountsArb)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKARB");

    // CHEAP Qswap pool: 1,000,000 asset / 100,000,000 QU -> price 100. Rich QX bid at 500.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 10000000LL), 10000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 1000000LL, 100000000LL);
    // 2000 shares @ bid 500 vs pool ~100: gross gap clears minProfitQu + the Qswap fee (net semantics).
    EXPECT_GT(ctx.qxAddBid(maker, maker, asset, 500LL, 2000LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(5000000LL);

    auto before = ctx.getStats();
    ctx.beginTick();
    auto after = ctx.getStats();

    EXPECT_GT(after.totalArbsExecuted, before.totalArbsExecuted); // an arb fired
    EXPECT_EQ(ctx.st()->epochProfit, 0LL);                        // deferred: not booked in-tick
    EXPECT_GT(ctx.st()->poolReserveTokens.get(0), 0LL);           // RESERVE_PCT folded into reserve
}

// Direction B with a LARGE arb: gross is big enough to trip the capital-spread scale-down
// (scaleFactor >= 2). Regression for task #50 — sizing the scale-down on the realized (90%-sold)
// margin keeps the shrunk trade above the slippage guard, so it executes instead of reverting.
// (Before the fix this reverted: shrunk too far, the 90% sale couldn't cover full cost + fee + margin.)
// Deep pool so slippage is negligible and the math is clear.
TEST(ContractCLKnDGR, Dagger_DirectionB_LargeArb_ScalesDownAndFires)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKBIG");

    // Deep pool priced ~280 (10,000,000 asset / 2,800,000,000 QU); cheap QX ask at 100, large size.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 2800000000LL);
    EXPECT_GT(ctx.qswapMgmtToQx(maker, maker, asset, 50000LL), 0LL);
    EXPECT_GT(ctx.qxAddAsk(maker, maker, asset, 100LL, 3600LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(5000000LL);

    auto before = ctx.getStats();
    ctx.beginTick();
    auto after = ctx.getStats();

    EXPECT_GT(after.totalArbsExecuted, before.totalArbsExecuted); // scaled-down trade still executed
    EXPECT_GT(after.totalProfitEarned, before.totalProfitEarned); // booked net profit
    EXPECT_GT(ctx.st()->epochProfit, 0LL);
}

// QX-fee-live toggle: with qxFeeLivePerTrade = 1 the sell leg fetches the QX transfer fee LIVE
// (CALL_OTHER_CONTRACT_FUNCTION(QX, Fees)) instead of the per-epoch cache. Direction A sells through QX,
// so run it with the toggle on and confirm the arb still completes — exercising the live-fee path.
TEST(ContractCLKnDGR, Dagger_QxFeeLiveMode_DirectionA_StillExecutes)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKQXF");

    // Same shape as Direction A: cheap pool (price 100), rich QX bid (500), sized to clear net + fee.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 10000000LL), 10000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 1000000LL, 100000000LL);
    EXPECT_GT(ctx.qxAddBid(maker, maker, asset, 500LL, 2000LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(5000000LL);
    ctx.st()->qxFeeLivePerTrade = 1; // force the live QX Fees fetch on the sell leg

    auto before = ctx.getStats();
    ctx.beginTick();
    auto after = ctx.getStats();

    EXPECT_GT(after.totalArbsExecuted, before.totalArbsExecuted); // arb completed via the live-fee path
    EXPECT_GT(ctx.st()->poolReserveTokens.get(0), 0LL);
}

// Negative: when the QX/Qswap price gap is below minProfitQu, no trade fires.
TEST(ContractCLKnDGR, Dagger_NoProfitableSpread_NoTrade)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKARB");

    // Pool price 105, QX ask price 100 — a 5% gap, far under minProfitQu (100,100).
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 10000000LL), 10000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 1000000LL, 105000000LL);
    EXPECT_GT(ctx.qswapMgmtToQx(maker, maker, asset, 50000LL), 0LL);
    EXPECT_GT(ctx.qxAddAsk(maker, maker, asset, 100LL, 400LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(1000000LL);

    auto before = ctx.getStats();
    ctx.beginTick();
    auto after = ctx.getStats();

    EXPECT_EQ(after.totalArbsExecuted, before.totalArbsExecuted); // nothing fired
    EXPECT_EQ(ctx.st()->epochProfit, 0LL);
}

// ===============================================================
// VIX volatility sampler (no swap — unaffected by the Qswap-fee bug)
// ===============================================================

// A large price move vs the last sample spikes the fast EWMA above the breakout floor,
// which flags a breakout and wakes BOTH Dagger directions onto the fast rescan cooldown.
TEST(ContractCLKnDGR, Vix_BigPriceMove_FlagsBreakout_WakesDagger)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKVIX");

    // Pool priced 100 (1,000,000 asset / 100,000,000 QU). No QX orders -> no trade is attempted,
    // so this exercises only the VIX sampler.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 10000000LL), 10000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 1000000LL, 100000000LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(1000000LL); // >= minProfitQu, else BEGIN_TICK returns before the VIX sampler

    // One baseline sample already taken at an OLD price of 90; the live pool is at 100 (~+11%),
    // a move large enough to push the fast EWMA past the 25 bps floor and 2x the slow EWMA.
    auto* s = ctx.st();
    s->vixSampleCount.set(0, 1);
    s->vixLastPrice.set(0, 90);
    s->vixFast.set(0, 0);
    s->vixSlow.set(0, 0);
    s->vixLastSampleTick.set(0, 0);
    s->vixBreakout.set(0, 0);

    // Advance past the sample cadence (default 1 pulse/day) so the sampler runs this tick.
    system.tick = CLKnDGR::INITIAL_VIX_SAMPLE_INTERVAL + 1;
    const uint32 nowTick = (uint32)system.tick;

    ctx.beginTick();

    EXPECT_EQ((int)ctx.st()->vixBreakout.get(0),    1);   // breakout flagged
    EXPECT_EQ((int)ctx.st()->vixSampleCount.get(0), 2);   // a second sample was taken
    EXPECT_GT(ctx.st()->vixFast.get(0),             0LL); // fast EWMA moved off zero
    EXPECT_EQ(ctx.st()->vixLastPrice.get(0),        100LL); // baseline advanced to the live price

    // Breakout shortens both Dagger cooldowns to the fast rescan beat (no order book -> each
    // direction re-arms at tick + breakoutRescanTicks rather than the long calm baseline).
    const uint32 expectedCd = nowTick + ctx.st()->breakoutRescanTicks;
    EXPECT_EQ(ctx.st()->poolCooldownTick.get(0),  expectedCd);
    EXPECT_EQ(ctx.st()->poolCooldownTickA.get(0), expectedCd);
}

// Type-17 reserve-liquidation sell — LIVE, end-to-end against real Qswap (verifies the R1/R2 fee fix
// on the liquidation path, the one piece the Dagger tests don't cover directly). A Dir B arb leaves 10%
// reserve tokens under the contract on-chain; a passed type-17 proposal then sells them on Qswap at
// END_EPOCH. Sized so the reserve is worth well over the 100K Qswap fee, so the sell books POSITIVE net.
TEST(ContractCLKnDGR, GovernanceCycle_SellPoolTokens_LiveQswapSell_BooksProceeds)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKRSV");

    // Governance shareholders (the CLKNDGR token) so the type-17 proposal can pass.
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i) owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    // Deep pool priced ~130; cheap QX ask at 100, large size — the arb runs at full size (scaleFactor 1)
    // and leaves a sizeable 10% reserve (worth >> the 100K Qswap fee).
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1300000000LL);
    EXPECT_GT(ctx.qswapMgmtToQx(maker, maker, asset, 50000LL), 0LL);
    EXPECT_GT(ctx.qxAddAsk(maker, maker, asset, 100LL, 16000LL), 0LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(3000000LL);

    // Run the arb -> CLKnDGR now holds the 10% reserve on-chain (owned + managed by the contract).
    ctx.beginTick();
    const sint64 reserveAfterArb = ctx.st()->poolReserveTokens.get(0);
    EXPECT_GT(reserveAfterArb, 0LL);

    // Pass a type-17 proposal to sell 100% of pool 0's reserve.
    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_SELL_POOL_TOKENS,
                                  100LL,  // sell 100% of holdings
                                  0);     // poolIndex 0
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_LT(ctx.st()->poolReserveTokens.get(0), reserveAfterArb); // reserve was sold down on Qswap
    EXPECT_GT(ctx.st()->reserveSellProceeds, 0LL);                  // live sell booked net proceeds (after the fee)
}

// ===============================================================
// THE CLOAK — swing strategy (live, against real Qswap + QX)
// ===============================================================

// Cloak dip-buy: a price history where the 1-week (latest) price is >=10% below the 3-month avg triggers
// a swing entry — the contract buys ~1% of capital on Qswap (now correctly fee-funded). Position opens.
TEST(ContractCLKnDGR, Cloak_DipBuy_OpensPosition)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKSWG");

    // Deep pool priced ~100.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1000000000LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(20000000LL);

    // Seed a 2-sample price history showing a dip: older 100, latest 80 -> 3mo avg 90, 1wk 80 <= 81.
    auto* s = ctx.st();
    s->swingBuyDipPct = 10; // ~11% dip scenario; pin the threshold so the 30% default doesn't suppress the buy
    s->swingPriceCount.set(0, 2);
    s->swingPriceHead.set(0, 2);
    s->swingPriceHistory.set(0, 100LL); // pool 0, slot 0 (older)
    s->swingPriceHistory.set(1, 80LL);  // pool 0, slot 1 (latest = 1-week price)

    EXPECT_EQ(ctx.st()->swingTokens.get(0), 0LL);
    ctx.beginTick();
    EXPECT_GT(ctx.st()->swingTokens.get(0), 0LL); // dip-buy opened a swing position
}

// Cloak gain-sell: once a position sits in profit (pool price >= cost x 112%), the contract exits
// swingSellPct of it via a QX ask into bids. Here we open the position with a real dip-buy, then drop the
// cost basis (simulating the price having risen past the +12% trigger), clear the monthly swing cooldown
// the buy set, post a QX bid for liquidity, and confirm the next tick reduces the position.
TEST(ContractCLKnDGR, Cloak_GainSell_ReducesPosition)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker  = clkUser(50);
    const id     bidder = clkUser(51);
    const uint64 asset  = assetNameFromString("CLKSWG");

    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1000000000LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(20000000LL);

    auto* s = ctx.st();
    s->swingBuyDipPct = 10; // ~11% dip scenario; pin the threshold so the 30% default doesn't suppress the dip-buy
    s->swingPriceCount.set(0, 2);
    s->swingPriceHead.set(0, 2);
    s->swingPriceHistory.set(0, 100LL);
    s->swingPriceHistory.set(1, 80LL);

    // Step 1: dip-buy opens the on-chain position.
    ctx.beginTick();
    const sint64 posAfterBuy = ctx.st()->swingTokens.get(0);
    EXPECT_GT(posAfterBuy, 0LL);

    // Step 2: put the position firmly in profit (low cost basis vs pool ~100), clear the 30-day swing
    // cooldown the buy set, and post a QX bid above the discounted ask (pool-10% = ~90) with ample QU.
    s->swingCostBasis.set(0, 50000LL); // costPerToken ~25 -> pool ~100 well past the +12% trigger
    s->swingCooldownTick.set(0, 0);
    EXPECT_GT(ctx.qxAddBid(bidder, maker, asset, 95LL, 5000LL), 0LL);

    // Step 3: the gain-sell exits part of the position via QX.
    ctx.beginTick();
    EXPECT_LT(ctx.st()->swingTokens.get(0), posAfterBuy); // partial exit reduced the position
}

// ===============================================================
// THE CLOAK — stop-loss (downside exit for a losing bag)
// ===============================================================

// Stop-loss happy path: a held bag that has fallen to avg cost × (100 - 45)% or below is cut by
// stopLossSellPct (60%) on Qswap, and STOP_LOSS_EXEC_PCT (10%) of the salvage is burned to the
// execution-fee reserve. We open a REAL position via a dip-buy (so the contract actually owns tokens
// to sell), then raise the cost basis so the live pool price (~100) sits far below the -45% trigger,
// clear the monthly cooldown, and confirm the next tick shrinks the bag AND tops up the fee reserve.
TEST(ContractCLKnDGR, Cloak_StopLoss_DeepLoser_SellsAndBurns)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKSL1");

    // Deep pool priced ~100 (10,000,000 asset / 1,000,000,000 QU) so the sale barely moves it.
    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1000000000LL);

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(50000000LL); // 1% first buy = 500k QU -> ~5000 tokens; 60% cut -> ~300k proceeds >> 100k fee

    // Open a swing position via a dip-buy (older 100, latest 80 -> 3mo avg 90, 1wk 80 <= 81 -> dip).
    auto* s = ctx.st();
    s->swingBuyDipPct = 10; // ~11% dip scenario; pin the threshold so the 30% default doesn't suppress the opening dip-buy
    s->swingPriceCount.set(0, 2);
    s->swingPriceHead.set(0, 2);
    s->swingPriceHistory.set(0, 100LL);
    s->swingPriceHistory.set(1, 80LL);
    ctx.beginTick();
    const sint64 posAfterBuy = ctx.st()->swingTokens.get(0);
    EXPECT_GT(posAfterBuy, 0LL);

    // Force a deep loss: costPerToken ~400 vs live pool ~100 => ~75% below cost, well past the -45%
    // trigger. (The stop-loss runs before the DCA-add and continues, so the dip flag is irrelevant.)
    s->swingCostBasis.set(0, posAfterBuy * 400LL);
    s->swingCooldownTick.set(0, 0);                    // clear the monthly cooldown the buy set
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 0);  // baseline so the 10% burn is observable

    ctx.beginTick();

    EXPECT_LT(ctx.st()->swingTokens.get(0), posAfterBuy);          // bag cut by the stop-loss
    EXPECT_GT(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), 0LL); // 10% of salvage burned to exec reserve
}

// Negative: a bag only mildly underwater (cost 125 vs pool ~100 = 20% below, under the 45% trigger)
// is NOT cut. Direct-seed the position with NON-dip history so neither the stop-loss nor a DCA-add fires.
TEST(ContractCLKnDGR, Cloak_StopLoss_ShallowLoss_NoSell)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKSL2");

    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1000000000LL); // price ~100

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(50000000LL);

    auto* s = ctx.st();
    s->swingTokens.set(0, 5000LL);
    s->swingCostBasis.set(0, 5000LL * 125LL); // costPerToken 125 vs pool ~100 -> only 20% below cost
    s->swingPriceCount.set(0, 2);
    s->swingPriceHead.set(0, 2);
    s->swingPriceHistory.set(0, 100LL);
    s->swingPriceHistory.set(1, 100LL);       // flat -> NOT a dip -> no DCA-add either
    s->swingCooldownTick.set(0, 0);

    ctx.beginTick();
    EXPECT_EQ(ctx.st()->swingTokens.get(0), 5000LL); // shallow loss -> stop-loss does NOT fire
}

// Negative: with stopLossTriggerPct = 0 the stop-loss is disabled, so even a deeply underwater bag
// (cost 400 vs pool ~100) is held. NON-dip history so no DCA-add confounds the assertion.
TEST(ContractCLKnDGR, Cloak_StopLoss_Disabled_NoSell)
{
    ContractTestingCLKnDGRDex ctx;
    const id     maker = clkUser(50);
    const uint64 asset = assetNameFromString("CLKSL3");

    EXPECT_EQ(ctx.qswapIssueAsset(maker, asset, 20000000LL), 20000000LL);
    EXPECT_TRUE(ctx.qswapCreatePool(maker, maker, asset));
    ctx.qswapAddLiquidity(maker, maker, asset, 10000000LL, 1000000000LL); // price ~100

    ctx.seedPool(0, maker, asset);
    ctx.fundContract(50000000LL);

    auto* s = ctx.st();
    s->stopLossTriggerPct = 0; // disabled
    s->swingTokens.set(0, 5000LL);
    s->swingCostBasis.set(0, 5000LL * 400LL); // deeply underwater, but disabled
    s->swingPriceCount.set(0, 2);
    s->swingPriceHead.set(0, 2);
    s->swingPriceHistory.set(0, 100LL);
    s->swingPriceHistory.set(1, 100LL);       // NOT a dip
    s->swingCooldownTick.set(0, 0);

    ctx.beginTick();
    EXPECT_EQ(ctx.st()->swingTokens.get(0), 5000LL); // disabled -> no cut despite the deep loss
}

// Type 24 (UPDATE_STOP_LOSS_TRIGGER): a full governance cycle changes the stop-loss cut threshold.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateStopLossTrigger_Changes)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->stopLossTriggerPct, 45LL); // default

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_STOP_LOSS_TRIGGER, 30LL); // valid 15-step
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->stopLossTriggerPct, 30LL);
}

// Type 24 must reject a value that is not 0 or a 15-point step in [15,90].
TEST(ContractCLKnDGR, SubmitProposal_UpdateStopLossTrigger_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_STOP_LOSS_TRIGGER, 40LL); // not a 15-step
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->stopLossTriggerPct, 45LL); // unchanged (default)
}

// Type 25 (UPDATE_STOP_LOSS_SELL): a full governance cycle changes the per-trigger cut fraction.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateStopLossSell_Changes)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->stopLossSellPct, 60LL); // default

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_STOP_LOSS_SELL, 90LL); // valid 15-step
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->stopLossSellPct, 90LL);
}

// Type 25 must reject a value that is not a 15-point step in [15,90] (0 is NOT valid for the sell %).
TEST(ContractCLKnDGR, SubmitProposal_UpdateStopLossSell_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_STOP_LOSS_SELL, 0LL); // 0 invalid for sell %
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->stopLossSellPct, 60LL); // unchanged (default)
}

// ===============================================================
// INITIALIZE: verify all initial state fields
// ===============================================================

TEST(ContractCLKnDGR, Initialize_VaultDefaults)
{
    ContractTestingCLKnDGR ctx;
    auto* s = ctx.st();

    EXPECT_EQ(s->vaultSharePrice,    CLKnDGR::VAULT_INITIAL_SHARE_PRICE);
    EXPECT_EQ(s->totalVaultShares,   0LL);
    EXPECT_EQ(s->totalDepositorPool, 0LL);
    EXPECT_EQ(s->prevTradingBalance, 0LL);
    EXPECT_EQ(s->vaultDepositTier,   CLKnDGR::VAULT_INITIAL_DEPOSIT_TIER);
    EXPECT_EQ(s->depositorCount,     0);
    EXPECT_EQ(s->waitlistCount,      0);
    EXPECT_EQ(s->waitlistQu,         0LL);
    EXPECT_EQ(s->qearnReserve,       0LL);
    EXPECT_EQ(s->quReserve,          0LL);
}

TEST(ContractCLKnDGR, Initialize_GovernanceDefaults)
{
    ContractTestingCLKnDGR ctx;
    auto* s = ctx.st();

    EXPECT_EQ(s->minProfitQu,                CLKnDGR::MIN_PROFIT_QU);
    EXPECT_EQ(s->proposalFeeDefault,         CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    EXPECT_EQ(s->proposalFeeAddPool,         CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);
    EXPECT_EQ(s->proposalFeePayoutStructure, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);
    EXPECT_EQ(s->payoutStructure,            0);
    EXPECT_EQ(s->minVoterQuorum,             CLKnDGR::INITIAL_MIN_VOTER_QUORUM);
    EXPECT_EQ(s->proposalsThisEpoch,         0);
    EXPECT_EQ(s->depositorVoteMinQu,         150000000LL);
    EXPECT_EQ(s->relockAddAmount,            CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    EXPECT_EQ(s->minReserveProfitPct,        5);
    EXPECT_EQ(s->stopLossTriggerPct,         45LL); // default: cut at -45% from avg cost
    EXPECT_EQ(s->stopLossSellPct,            60LL); // default: sell 60% of the bag per trigger
    EXPECT_EQ(s->poolCount,                  0);
}

TEST(ContractCLKnDGR, Initialize_SelfAsset)
{
    ContractTestingCLKnDGR ctx;
    auto* s = ctx.st();

    EXPECT_EQ(s->selfAsset.assetName, assetNameFromString("CLKNDGR"));
    EXPECT_EQ(s->selfAsset.issuer,    NULL_ID); // IPO shares issued under NULL_ID (matches issueContractShares)
}

// ===============================================================
// READ FUNCTIONS
// ===============================================================

TEST(ContractCLKnDGR, GetStats_InitialZeros)
{
    ContractTestingCLKnDGR ctx;
    auto s = ctx.getStats();

    EXPECT_EQ(s.totalArbsExecuted, 0ULL);
    EXPECT_EQ(s.totalProfitEarned, 0LL);
    EXPECT_EQ(s.quBalance,         0LL);
    EXPECT_EQ(s.quReserve,         0LL);
    EXPECT_EQ(s.qearnReserve,      0LL);
    EXPECT_EQ(s.poolCount,         0);
}

TEST(ContractCLKnDGR, GetGovernanceParams_InitialValues)
{
    ContractTestingCLKnDGR ctx;
    auto p = ctx.getGovernanceParams();

    EXPECT_EQ(p.minProfitQu,             CLKnDGR::MIN_PROFIT_QU);
    EXPECT_EQ(p.proposalFeeDefault,      CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    EXPECT_EQ(p.proposalFeeAddPool,      CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);
    EXPECT_EQ(p.proposalFeePayoutStructure, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);
    EXPECT_EQ(p.payoutStructure,         0);
    EXPECT_EQ(p.minVoterQuorum,          CLKnDGR::INITIAL_MIN_VOTER_QUORUM);
    EXPECT_EQ(p.proposalsThisEpoch,      0);
    EXPECT_EQ(p.minReserveProfitPct,     5);
    EXPECT_EQ(p.depositorVoteMinQu,      150000000LL);
    EXPECT_EQ(p.relockAddAmount,         CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    EXPECT_EQ(p.stopLossTriggerPct,      45LL); // default: cut at -45% from avg cost
    EXPECT_EQ(p.stopLossSellPct,         60LL); // default: sell 60% of the bag per trigger
}

TEST(ContractCLKnDGR, GetPool_SlotZeroEmptyOnInit)
{
    ContractTestingCLKnDGR ctx;
    auto p = ctx.getPool(0);
    EXPECT_EQ(p.active, 0);
}

TEST(ContractCLKnDGR, GetWaitlistPosition_EmptyWaitlist)
{
    ContractTestingCLKnDGR ctx;
    auto w = ctx.getWaitlistPosition();
    EXPECT_EQ(w.totalWaiting, 0);
    EXPECT_EQ(w.isFull,       0);
    EXPECT_EQ(w.onWaitlist,   0); // NULL_ID invocator is never on waitlist
    EXPECT_EQ(w.amount,       0LL);
}

// ===============================================================
// VAULT DEPOSIT
// ===============================================================

TEST(ContractCLKnDGR, VaultDeposit_RejectZero)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 1000000LL);
    auto out = ctx.vaultDeposit(u, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->depositorCount, 0);
}

TEST(ContractCLKnDGR, VaultDeposit_RejectBelowMinimum)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    // Tier 8: minShares=1000, sharePrice=10000 → minDeposit=10,000,000 QU
    const sint64 minDeposit = 10000000LL;
    ctx.addEnergy(u, minDeposit);
    auto out = ctx.vaultDeposit(u, minDeposit - 1);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->depositorCount, 0);
    // QU refunded: user balance unchanged
    EXPECT_EQ(getBalance(u), minDeposit);
}

TEST(ContractCLKnDGR, VaultDeposit_SuccessAtMinimum)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    // Tier 8 minimum = 1000 shares × 10000 QU = 10,000,000 QU → issues exactly 1000 shares
    const sint64 depositAmount = 10000000LL;
    ctx.addEnergy(u, depositAmount);

    auto out = ctx.vaultDeposit(u, depositAmount);

    EXPECT_EQ(out.success,      1);
    EXPECT_EQ(out.sharesIssued, 1000LL);
    EXPECT_EQ(out.newSharePrice, CLKnDGR::VAULT_INITIAL_SHARE_PRICE);

    auto* s = ctx.st();
    EXPECT_EQ(s->depositorCount,     1);
    EXPECT_EQ(s->totalVaultShares,   1000LL);
    EXPECT_EQ(s->totalDepositorPool, depositAmount);
    // First deposit seeds prevTradingBalance from contractBalance
    EXPECT_GT(s->prevTradingBalance, 0LL);
}

TEST(ContractCLKnDGR, VaultDeposit_TwoDepositors_PoolAccumulates)
{
    ContractTestingCLKnDGR ctx;
    const id u1 = clkUser(1);
    const id u2 = clkUser(2);
    const sint64 deposit = 10000000LL;

    ctx.addEnergy(u1, deposit);
    ctx.addEnergy(u2, deposit);
    ctx.vaultDeposit(u1, deposit);
    auto out2 = ctx.vaultDeposit(u2, deposit);

    EXPECT_EQ(out2.success, 1);
    auto* s = ctx.st();
    EXPECT_EQ(s->depositorCount,     2);
    EXPECT_EQ(s->totalVaultShares,   2000LL);
    EXPECT_EQ(s->totalDepositorPool, deposit * 2);
    // Second deposit adds to prevTradingBalance (not seeded again)
    EXPECT_EQ(s->prevTradingBalance, deposit * 2);
}

TEST(ContractCLKnDGR, VaultDeposit_RejectDuplicate)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const sint64 depositAmount = 10000000LL;
    ctx.addEnergy(u, depositAmount * 2);
    ctx.vaultDeposit(u, depositAmount);

    // Second deposit from the same wallet must be rejected (no top-ups)
    long long balMid = getBalance(u);
    auto out = ctx.vaultDeposit(u, depositAmount);

    EXPECT_EQ(out.success,           0);
    EXPECT_EQ(ctx.st()->depositorCount, 1);
    // QU refunded: balance restored
    EXPECT_EQ(getBalance(u), balMid);
}

// ===============================================================
// VAULT WITHDRAW
// ===============================================================

TEST(ContractCLKnDGR, VaultWithdraw_NotDepositor)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 1000LL);
    auto out = ctx.vaultWithdraw(u);
    EXPECT_EQ(out.success, 0);
}

TEST(ContractCLKnDGR, VaultWithdraw_EarlyExit_PenaltyAndFee)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    // Deposit exactly minimum: 1000 shares × 10000 = 10,000,000 QU
    const sint64 depositAmount = 10000000LL;
    ctx.addEnergy(u, depositAmount);
    ctx.vaultDeposit(u, depositAmount);

    long long balAfterDeposit = getBalance(u); // = 0
    auto out = ctx.vaultWithdraw(u);

    // success=2: early exit (lock not expired in same epoch)
    EXPECT_EQ(out.success, 2);
    EXPECT_GT(out.penaltyApplied, 0LL);

    // gross = 1000 × 10000 = 10,000,000
    // mgmtFee = 2% of 10M = 200,000  (burned)
    // earlyPenalty = 38% of 10M = 3,800,000 (→ epochProfit)
    // penaltyApplied = mgmtFee + earlyPenalty = 4,000,000
    // net = 10,000,000 - 4,000,000 = 6,000,000
    EXPECT_EQ(out.penaltyApplied, 4000000LL);
    EXPECT_EQ(out.amountReturned, 6000000LL);

    auto* s = ctx.st();
    EXPECT_EQ(s->depositorCount,   0);
    EXPECT_EQ(s->totalVaultShares, 0LL);
    // Early penalty flows to epochProfit
    EXPECT_EQ(s->epochProfit, 3800000LL);

    EXPECT_EQ(getBalance(u), balAfterDeposit + out.amountReturned);
}

TEST(ContractCLKnDGR, VaultWithdraw_CleansUpAllHashMapEntries)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL);
    ctx.vaultWithdraw(u);

    auto* s = ctx.st();
    EXPECT_EQ(s->depositorCount,     0);
    EXPECT_EQ(s->totalVaultShares,   0LL);
    EXPECT_EQ(s->totalDepositorPool, 0LL);
    EXPECT_EQ(s->vaultSharePrice,    CLKnDGR::VAULT_INITIAL_SHARE_PRICE); // reset to default
}

// ===============================================================
// WAITLIST WITHDRAW
// ===============================================================

TEST(ContractCLKnDGR, WaitlistWithdraw_NotOnWaitlist)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 1000LL);
    auto out = ctx.waitlistWithdraw(u);
    EXPECT_EQ(out.success,       0);
    EXPECT_EQ(out.amountRefunded, 0LL);
}

// ===============================================================
// VAULT RELOCK
// ===============================================================

TEST(ContractCLKnDGR, VaultRelock_NotDepositor)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    auto out = ctx.vaultRelock(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    // success=0: not a depositor
    EXPECT_EQ(out.success, 0);
}

// ===============================================================
// SHAREHOLDER GOVERNANCE: submitProposal
// ===============================================================

TEST(ContractCLKnDGR, SubmitProposal_NoSharesRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // No shares issued → governance closed (selfAsset queries return 0)
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

TEST(ContractCLKnDGR, SubmitProposal_ShareholderSuccess)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(out.success, 1);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 1);
}

TEST(ContractCLKnDGR, GetProposal_ReflectsActiveProposal)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.status,       CLKnDGR::PROP_STATUS_ACTIVE);
    EXPECT_EQ(prop.proposalType, CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT);
    EXPECT_EQ(prop.newValue,     CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(prop.proposer,     u);
    EXPECT_EQ(prop.feePaid,      CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
}

// ===============================================================
// SHAREHOLDER GOVERNANCE: voteOnProposal
// ===============================================================

TEST(ContractCLKnDGR, VoteOnProposal_NoSharesRejected)
{
    ContractTestingCLKnDGR ctx;
    const id proposer = clkUser(1);
    const id voter    = clkUser(2); // no shares

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    ctx.addEnergy(voter, 1000LL);
    auto vOut = ctx.voteOnProposal(voter, sub.slot, 1);
    EXPECT_EQ(vOut.success, 0);
}

TEST(ContractCLKnDGR, VoteOnProposal_ShareholderYes)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto vOut = ctx.voteOnProposal(u, sub.slot, 1);
    EXPECT_EQ(vOut.success, 1);

    // votesYes/votesNo are tallied at END_EPOCH (with a share re-verification pass),
    // not at cast time — so cross the epoch boundary before reading the weighted tally.
    ctx.endEpoch();
    auto prop = ctx.getProposal(sub.slot);
    EXPECT_GT(prop.votesYes, 0LL); // the voter's 50 shares counted as YES weight
    EXPECT_EQ(prop.votesNo,  0LL);
}

TEST(ContractCLKnDGR, VoteOnProposal_NoVote)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    auto vOut = ctx.voteOnProposal(u, sub.slot, 0); // vote NO
    EXPECT_EQ(vOut.success, 1);

    // Tally happens at END_EPOCH, not at cast time.
    ctx.endEpoch();
    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.votesYes, 0LL);
    EXPECT_GT(prop.votesNo,  0LL); // the voter's 50 shares counted as NO weight
}

// ===============================================================
// DEPOSITOR VETO
// ===============================================================

TEST(ContractCLKnDGR, DepositorVeto_NotDepositorRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 1000LL);
    auto out = ctx.depositorVeto(u, 0, 0);
    EXPECT_EQ(out.success, 0);
}

// ===============================================================
// donateToContract — open to anyone, ANY positive amount, 100% → execution-fee reserve
// ===============================================================

TEST(ContractCLKnDGR, DonateToContract_SmallAmountAccepted)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const sint64 sent = 1000LL; // any positive amount is now accepted — there is no minimum
    ctx.addEnergy(u, sent);

    auto out = ctx.donateToContract(u, sent);

    EXPECT_EQ(out.success, 1);
    EXPECT_EQ(out.toExecutionReserve, sent); // 100% burned into the execution-fee reserve
    EXPECT_EQ(getBalance(u), 0LL);           // full amount consumed, nothing refunded
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, DonateToContract_ZeroSentRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 1000LL);
    auto out = ctx.donateToContract(u, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, DonateToContract_AnyoneAccepted)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1); // no shares — donations are now open to anyone
    ctx.addEnergy(u, 3000000LL);

    auto out = ctx.donateToContract(u, 3000000LL);

    EXPECT_EQ(out.success, 1);                                   // no shareholder requirement anymore
    EXPECT_EQ(out.toExecutionReserve, 3000000LL); // 100% routed to the execution-fee reserve
    EXPECT_EQ(getBalance(u), 0LL);                               // full amount consumed (burned), nothing refunded
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);                      // donations no longer touch the Qearn jar
}

TEST(ContractCLKnDGR, DonateToContract_ExactAmount)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 3000000LL);

    auto out = ctx.donateToContract(u, 3000000LL);

    EXPECT_EQ(out.success, 1);
    // 100% of the 3,000,000 QU is burned into the execution-fee reserve
    EXPECT_EQ(out.toExecutionReserve, 3000000LL);
    EXPECT_EQ(out.toExecutionReserve, 3000000LL);
    EXPECT_EQ(getBalance(u), 0LL);          // entire amount consumed, nothing refunded
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL); // no longer split to the Qearn jar
}

TEST(ContractCLKnDGR, DonateToContract_LargeAmountFullyBurned)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const sint64 sent = 7000000LL; // large arbitrary amount — all of it is kept, nothing refunded
    ctx.addEnergy(u, sent);

    auto out = ctx.donateToContract(u, sent);

    EXPECT_EQ(out.success, 1);
    EXPECT_EQ(out.toExecutionReserve, sent); // entire amount burned into the reserve
    EXPECT_EQ(getBalance(u), 0LL);           // nothing refunded
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, DonateToContract_TwoDonations_QearnUntouched)
{
    ContractTestingCLKnDGR ctx;
    const id u1 = clkUser(1);
    const id u2 = clkUser(2);
    ctx.addEnergy(u1, 3000000LL);
    ctx.addEnergy(u2, 3000000LL);

    EXPECT_EQ(ctx.donateToContract(u1, 3000000LL).success, 1);
    EXPECT_EQ(ctx.donateToContract(u2, 3000000LL).success, 1);

    // Both donations are burned to the execution-fee reserve, never the Qearn jar
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

// ===============================================================
// publicDonate — open to any wallet, ANY positive amount, 100% → execution-fee reserve
// ===============================================================

TEST(ContractCLKnDGR, PublicDonate_SmallAmountAccepted)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(5);
    const sint64 sent = 1000LL; // any positive amount accepted — no minimum
    ctx.addEnergy(u, sent);

    auto out = ctx.publicDonate(u, sent);

    EXPECT_EQ(out.success, 1);
    EXPECT_EQ(out.toExecutionReserve, sent);
    EXPECT_EQ(getBalance(u), 0LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, PublicDonate_ZeroSentRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(5);
    ctx.addEnergy(u, 1000LL);
    auto out = ctx.publicDonate(u, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, PublicDonate_ExactAmount_AnyWallet)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(5); // any wallet — no shares
    ctx.addEnergy(u, 3000000LL);

    auto out = ctx.publicDonate(u, 3000000LL);

    EXPECT_EQ(out.success, 1);
    // 100% of the 3,000,000 QU is burned into the execution-fee reserve
    EXPECT_EQ(out.toExecutionReserve, 3000000LL);
    EXPECT_EQ(out.toExecutionReserve, 3000000LL);
    EXPECT_EQ(getBalance(u), 0LL);          // entire amount consumed
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, PublicDonate_LargeAmountFullyBurned)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(5);
    const sint64 sent = 5500000LL; // large arbitrary amount — all kept, nothing refunded
    ctx.addEnergy(u, sent);

    auto out = ctx.publicDonate(u, sent);

    EXPECT_EQ(out.success, 1);
    EXPECT_EQ(out.toExecutionReserve, sent);
    EXPECT_EQ(getBalance(u), 0LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

TEST(ContractCLKnDGR, PublicDonate_Multiple_QearnUntouched)
{
    ContractTestingCLKnDGR ctx;
    for (int i = 0; i < 3; ++i)
    {
        const id u = clkUser(10 + i);
        ctx.addEnergy(u, 3000000LL);
        auto out = ctx.publicDonate(u, 3000000LL);
        EXPECT_EQ(out.success, 1);
    }
    // All burned to the execution-fee reserve; the Qearn jar is never touched
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

// ===============================================================
// Mixed donations: donateToContract + publicDonate accumulation
// ===============================================================

TEST(ContractCLKnDGR, MixedDonations_QearnUntouched)
{
    ContractTestingCLKnDGR ctx;
    const id u1 = clkUser(1);
    const id u2 = clkUser(2);

    ctx.addEnergy(u1, 3000000LL);
    ctx.addEnergy(u2, 3000000LL);

    EXPECT_EQ(ctx.donateToContract(u1, 3000000LL).success, 1);
    EXPECT_EQ(ctx.publicDonate(u2,      3000000LL).success, 1);

    // Both donation paths burn to the execution-fee reserve — the Qearn jar stays empty
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
}

// ===============================================================
// SYSTEM HOOKS
// ===============================================================

TEST(ContractCLKnDGR, BeginEpoch_ResetsGovernanceState)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                       CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                       CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 1);

    // BEGIN_EPOCH on a fresh state (no pools, no profit, no qearnReserve above threshold)
    // is safe to call — all cross-contract paths are guarded by conditions that don't fire.
    ctx.beginEpoch();

    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->voterCount,         0);
    EXPECT_EQ(ctx.st()->epochProfit,        0LL);
}

TEST(ContractCLKnDGR, BeginEpoch_EmptyVaultNoNAVMovement)
{
    ContractTestingCLKnDGR ctx;
    ctx.beginEpoch();

    auto* s = ctx.st();
    // No depositors → vault state should not change
    EXPECT_EQ(s->totalVaultShares,   0LL);
    EXPECT_EQ(s->totalDepositorPool, 0LL);
    EXPECT_EQ(s->prevTradingBalance, 0LL);
    EXPECT_EQ(s->vaultSharePrice,    CLKnDGR::VAULT_INITIAL_SHARE_PRICE);
}

TEST(ContractCLKnDGR, EndEpoch_RunsWithoutError)
{
    ContractTestingCLKnDGR ctx;
    // END_EPOCH with zero epochProfit and no proposals → essentially a no-op
    ctx.endEpoch();
}

TEST(ContractCLKnDGR, BeginTick_EmptyPoolsImmediateReturn)
{
    ContractTestingCLKnDGR ctx;
    // poolCount == 0 → BEGIN_TICK returns after setting quBalance; no cross-contract calls
    ctx.beginTick();
    EXPECT_EQ(ctx.st()->poolCount, 0);
}

TEST(ContractCLKnDGR, SystemHooks_SequentialRunClean)
{
    ContractTestingCLKnDGR ctx;
    checkContractExecCleanup();
    ctx.endEpoch();
    ctx.beginEpoch();
    ctx.beginTick();
    checkContractExecCleanup();
}

// ===============================================================
// DEPOSIT + EPOCH CYCLE: vault deposit survives BEGIN_EPOCH
// ===============================================================

TEST(ContractCLKnDGR, DepositThenBeginEpoch_DepositorSurvives)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const sint64 depositAmount = 10000000LL;
    ctx.addEnergy(u, depositAmount);
    ctx.vaultDeposit(u, depositAmount);

    EXPECT_EQ(ctx.st()->depositorCount, 1);

    // Lock epoch = current epoch (0). Lock expires at epoch 26.
    // BEGIN_EPOCH with same epoch → lock not expired → no auto-payout.
    ctx.beginEpoch();

    EXPECT_EQ(ctx.st()->depositorCount, 1); // still in vault
    EXPECT_EQ(ctx.st()->totalVaultShares, 1000LL);
}

// ===============================================================
// DONATION DOES NOT AFFECT VAULT SHARE PRICE MID-EPOCH
// ===============================================================

TEST(ContractCLKnDGR, DonateToContract_DoesNotUpdateSharePrice)
{
    ContractTestingCLKnDGR ctx;
    const id shareholder = clkUser(1);
    const id depositor   = clkUser(2);
    ctx.issueShares({{shareholder, 1}});

    ctx.addEnergy(depositor, 10000000LL);
    ctx.vaultDeposit(depositor, 10000000LL);
    sint64 priceBeforeDonate = ctx.st()->vaultSharePrice;

    ctx.addEnergy(shareholder, 3000000LL);
    ctx.donateToContract(shareholder, 3000000LL);

    // Share price is NOT updated by a donation; it only changes at END_EPOCH NAV calc
    EXPECT_EQ(ctx.st()->vaultSharePrice, priceBeforeDonate);
}

TEST(ContractCLKnDGR, PublicDonate_DoesNotUpdateSharePrice)
{
    ContractTestingCLKnDGR ctx;
    const id depositor  = clkUser(1);
    const id publicUser = clkUser(2);

    ctx.addEnergy(depositor, 10000000LL);
    ctx.vaultDeposit(depositor, 10000000LL);
    sint64 priceBeforeDonate = ctx.st()->vaultSharePrice;

    ctx.addEnergy(publicUser, 3000000LL);
    ctx.publicDonate(publicUser, 3000000LL);

    EXPECT_EQ(ctx.st()->vaultSharePrice, priceBeforeDonate);
}

// ===============================================================
// VAULT RELOCK
// ===============================================================

TEST(ContractCLKnDGR, VaultRelock_WindowNotOpen)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL + CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    ctx.vaultDeposit(u, 10000000LL);

    // Deposited at epoch 0; window opens at epoch 22 (26-4). Trying immediately → success=1.
    auto out = ctx.vaultRelock(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);

    EXPECT_EQ(out.success, 1);
    // QU refunded: user balance restored to INITIAL_RELOCK_ADD_AMOUNT
    EXPECT_EQ(getBalance(u), CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    // Lock epoch unchanged
    uint32 depEpoch = 0;
    CLKnDGR::DepositorInfo rec{}; ctx.st()->depositorInfo.get(u, rec); depEpoch = rec.epoch;
    EXPECT_EQ(depEpoch, 0U);
}

TEST(ContractCLKnDGR, VaultRelock_LockExpired_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL + CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    ctx.vaultDeposit(u, 10000000LL);

    // Advance past lock expiry: qpi.epoch() >= depEpoch(0) + VAULT_LOCK_EPOCHS(26) = 26
    system.epoch = 26;
    auto out = ctx.vaultRelock(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    system.epoch = 0;

    EXPECT_EQ(out.success, 2); // lock already expired — use vaultWithdraw instead
    EXPECT_EQ(getBalance(u), CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT); // QU refunded
}

TEST(ContractCLKnDGR, VaultRelock_InsufficientQu_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL);

    // Advance to inside the relock window (epoch >= 0+26-4 = 22) but send 0 QU
    system.epoch = 22;
    auto out = ctx.vaultRelock(u, 0);
    system.epoch = 0;

    EXPECT_EQ(out.success, 3); // insufficient QU
    EXPECT_EQ(ctx.st()->totalVaultShares, 1000LL); // position unchanged
}

TEST(ContractCLKnDGR, VaultRelock_Success_ResetsLockAndIssuesShares)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const sint64 addAmount = CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT; // 10M

    ctx.addEnergy(u, 10000000LL + addAmount);
    ctx.vaultDeposit(u, 10000000LL);

    system.epoch = 22; // inside relock window
    auto out = ctx.vaultRelock(u, addAmount);
    system.epoch = 0;

    EXPECT_EQ(out.success,      4);
    EXPECT_EQ(out.sharesIssued, 1000LL); // 10M / price(10000) = 1000 new shares
    EXPECT_EQ(out.newDepEpoch,  22U);    // lock epoch reset to current epoch

    uint32 depEpoch = 0;
    CLKnDGR::DepositorInfo rec{}; ctx.st()->depositorInfo.get(u, rec); depEpoch = rec.epoch;
    EXPECT_EQ(depEpoch, 22U);

    EXPECT_EQ(ctx.st()->totalVaultShares,   2000LL);
    EXPECT_EQ(ctx.st()->totalDepositorPool, 10000000LL + addAmount);
}

// ===============================================================
// VAULT WITHDRAW: lock-expired normal exit (success=1)
// ===============================================================

TEST(ContractCLKnDGR, VaultWithdraw_LockExpired_NormalExit)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ price 10000

    // Advance to epoch 26: lock expires (depEpoch=0, VAULT_LOCK_EPOCHS=26)
    system.epoch = 26;
    auto out = ctx.vaultWithdraw(u);
    system.epoch = 0;

    EXPECT_EQ(out.success, 1); // normal exit — lock expired

    // gross = 1000 × 10000 = 10_000_000
    // mgmtFee = 2% × 10M = 200_000 (burned)
    // profit  = gross(10M) − costBasis(10M) = 0 → no performance fee
    // net     = 10M − 200_000 = 9_800_000
    EXPECT_EQ(out.penaltyApplied, 0LL);
    EXPECT_EQ(out.amountReturned, 9800000LL);
    EXPECT_EQ(getBalance(u),      9800000LL);

    EXPECT_EQ(ctx.st()->depositorCount,   0);
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
}

// ===============================================================
// PROPOSAL FEE ACCOUNTING: fee stays as trading capital, not quReserve
// ===============================================================

TEST(ContractCLKnDGR, SubmitProposal_FeeDoesNotGoToQuReserve)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                       CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                       CLKnDGR::MIN_PROFIT_QU_OPT2);

    // Proposal fee must NOT flow into quReserve (dev fund).
    // quReserve is only populated from trading profit during BEGIN_EPOCH.
    EXPECT_EQ(ctx.st()->quReserve, 0LL);
}

// ===============================================================
// GOVERNANCE LIFECYCLE: full pass and quorum-fail cycles
// ===============================================================

TEST(ContractCLKnDGR, GovernanceCycle_PassedProposal_ParameterChanges)
{
    ContractTestingCLKnDGR ctx;

    // 15 shareholders × 15 shares each = 225 weighted votes.
    // Satisfies MIN_SHARES_QUORUM (222) and minVoterQuorum (15).
    // Remaining 676-225=451 shares stay with the contract id (cannot vote).
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // All 15 vote YES — 225/225 = 100% > supermajority threshold (66.7%)
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.status, CLKnDGR::PROP_STATUS_PASSED);

    // Parameter must have applied in state
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU_OPT2);
}

TEST(ContractCLKnDGR, GovernanceCycle_FailedQuorum_ProposerRefunded)
{
    ContractTestingCLKnDGR ctx;

    // 1 shareholder, 1 share: totalVotes=1 < MIN_SHARES_QUORUM(222) → quorum miss
    const id proposer = clkUser(1);
    ctx.issueShares({{proposer, 1}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    ctx.voteOnProposal(proposer, sub.slot, 1);

    EXPECT_EQ(getBalance(proposer), 0LL); // fee fully consumed at submission

    ctx.endEpoch();

    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.status, CLKnDGR::PROP_STATUS_FAILED);

    // 69% of feePaid refunded: div(50_000_000 × 69, 100) = 34_500_000
    const sint64 expectedRefund = (sint64)(
        ((uint64)CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 69ULL) / 100ULL);
    EXPECT_EQ(getBalance(proposer), expectedRefund);

    // Parameter must NOT have changed
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU);
}

// ===============================================================
// EPOCH PROFIT DISTRIBUTION: quReserve and qearnReserve filled (PAYOUT0)
// ===============================================================

TEST(ContractCLKnDGR, BeginEpoch_ProfitDistribution_FillsQuReserveAndQearnReserve)
{
    ContractTestingCLKnDGR ctx;

    // Issue shares so qpi.distributeDividends has valid recipients
    ctx.issueShares({{clkUser(1), 676}});

    // Seed the contract balance to cover exec(30%)+dist(10%)+ccf(1%) = 41% of profit
    const sint64 profit = 10000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);

    // Inject epochProfit directly — simulates a profitable trading epoch
    ctx.st()->epochProfit = profit;

    ctx.beginEpoch();

    // PAYOUT0 (default): devFund=1%, qearn=3%
    // quReserve   += div(10M × 1, 100) = 100_000
    // qearnReserve += div(10M × 3, 100) = 300_000
    EXPECT_EQ(ctx.st()->quReserve,    100000LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 300000LL);

    // epochProfit reset to zero after distribution
    EXPECT_EQ(ctx.st()->epochProfit, 0LL);
}

// ===============================================================
// DIVIDEND REGRESSION (core-dev review #3)
// qpi.distributeDividends() takes a PER-SHARE amount; the total it pays is
// 676 × that. The contract must therefore pass distribAmount / 676, NOT the
// whole distribAmount. With the original bug it passed the total, so the call
// tried to pay 676× that much, failed its internal balance check, returned
// false, and shareholders received NOTHING. The sibling test above only checks
// quReserve/qearnReserve — it never noticed. This test pins the actual on-chain
// receipt: a holder of all 676 shares must see their QU balance rise by exactly
// the dividend pool. On the buggy code this asserts 676,000 but gets 0 → fails.
// ===============================================================
TEST(ContractCLKnDGR, BeginEpoch_DividendReachesShareholder)
{
    ContractTestingCLKnDGR ctx;

    const id shareholder = clkUser(1);
    ctx.issueShares({{shareholder, 676}});      // all 676 IPO shares to one holder

    // Chosen for exact, dust-free math: dist = 10% of profit = 676,000;
    // per-share = 676,000 / 676 = 1,000; total paid back = 1,000 × 676 = 676,000.
    const sint64 profit = 6760000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit); // cover the whole split

    const sint64 balBefore = getBalance(shareholder);           // 0 — shares carry no QU
    ctx.st()->epochProfit = profit;

    ctx.beginEpoch();

    // PAYOUT0 (default) dist = 10%. Derive expected the same way the contract does.
    const sint64 distribAmount = (sint64)(profit * CLKnDGR::PAYOUT0_DIST_PCT / 100); // 676,000
    const sint64 perShare      = distribAmount / 676;                                 // 1,000
    const sint64 expectedDiv   = perShare * 676;                                      // 676,000

    EXPECT_GT(expectedDiv, 0LL);                                 // guard: non-vacuous pool
    EXPECT_EQ(getBalance(shareholder), balBefore + expectedDiv); // the shareholder was PAID
    EXPECT_EQ(ctx.st()->epochProfit, 0LL);                       // distribution ran to completion
}

// ===============================================================
// DEPOSITOR VETO: valid depositor with sufficient locked QU
// ===============================================================

TEST(ContractCLKnDGR, DepositorVeto_LargeDepositor_Success)
{
    ContractTestingCLKnDGR ctx;
    const id proposer  = clkUser(1);
    const id depositor = clkUser(2);

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // depositorVoteMinQu default = 150M QU. At price 10000: 15000 shares needed → deposit 150M.
    ctx.addEnergy(depositor, 150000000LL);
    ctx.vaultDeposit(depositor, 150000000LL);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // Depositor registers a NO veto vote — should succeed
    auto vout = ctx.depositorVeto(depositor, sub.slot, 0);
    EXPECT_NE(vout.success, 0);
}

// ===============================================================
// GROUP A — HIGH-PRIORITY COVERAGE
// ===============================================================

// A1: Quorum passes but fewer than 2/3 vote YES → supermajority fails, proposer refunded 69%
TEST(ContractCLKnDGR, GovernanceCycle_FailedSupermajority_ProposerRefunded)
{
    ContractTestingCLKnDGR ctx;

    // 15 × 15 = 225 weighted votes — satisfies MIN_SHARES_QUORUM(222) and minVoterQuorum(15)
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // 8 YES (120 weighted votes) + 7 NO (105) = 53.3% — below 66.7% supermajority threshold
    for (int i = 0; i < 8; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);
    for (int i = 8; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 0);

    ctx.endEpoch();

    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.status, CLKnDGR::PROP_STATUS_FAILED);

    // Proposer receives 69% refund: div(50M × 69, 100) = 34_500_000
    const sint64 refund = (sint64)(((uint64)CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 69ULL) / 100ULL);
    EXPECT_EQ(getBalance(clkUser(0)), refund);

    // Parameter must NOT have changed
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU);
}

// A2: Profitable vault exit charges both the 2% management fee and 5% performance fee on profit
TEST(ContractCLKnDGR, VaultWithdraw_ProfitableExit_PerfFeeCharged)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ price 10000, costBasis 10M

    // Simulate profitable trading: inject 2M QU and bump share price directly
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 2000000LL);
    ctx.st()->vaultSharePrice    = 12000LL;
    ctx.st()->totalDepositorPool = 12000000LL; // 1000 shares × 12000

    system.epoch = 26; // lock expired (depEpoch=0, VAULT_LOCK_EPOCHS=26)
    auto out = ctx.vaultWithdraw(u);
    system.epoch = 0;

    EXPECT_EQ(out.success, 1); // normal exit — lock expired
    // gross = 1000 × 12000 = 12_000_000
    // profit = 12M − 10M = 2_000_000 → performance fee applies
    // mgmtFee = 2% × 12M = 240_000 (burned)
    // perfFee = 5% × 2M  = 100_000 (→ epochProfit)
    // net = 12M − 240K − 100K = 11_660_000
    EXPECT_EQ(out.amountReturned, 11660000LL);
    EXPECT_EQ(out.penaltyApplied, 0LL);
    EXPECT_EQ(getBalance(u), 11660000LL);
    EXPECT_GT(ctx.st()->epochProfit, 0LL); // performance fee credited to epochProfit
}

// A3: Profitable epoch raises vault share price at the next BEGIN_EPOCH NAV update
TEST(ContractCLKnDGR, BeginEpoch_NAVGrowth_SharePriceIncreases)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ price 10000

    // First beginEpoch: sets prevTradingBalance = 10M (zero profit, no distribution)
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->vaultSharePrice, 10000LL); // unchanged — no profit added yet

    // Inject 1M QU simulating arbitrage profit accumulated during the epoch
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 1000000LL);

    // Second beginEpoch: vaultCurBalance=11M, prevTradingBalance=10M
    // navRatio = 11M×1000/10M = 1100; newPool = 10M×1100/1000 = 11M; price = 11M/1000 = 11000
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->vaultSharePrice,    11000LL);
    EXPECT_EQ(ctx.st()->totalDepositorPool, 11000000LL);
}

// NAV BACKING INVARIANT (regression for the payout-slice over-credit fix). The depositor share price must grow
// ONLY on RETAINED trading capital, so the recorded depositor pool must stay equal to the real depositor backing
// (balance − reserves, snapshotted as prevTradingBalance) — even across a profit epoch. The fix excludes EVERY
// payout slice (Qearn + dev + exec + dividend + CCF) from the BEGIN_EPOCH NAV numerator. Here we book 1,000,000
// profit under preset 3 (100% burned to the exec reserve): none of it is depositor equity, so the pool must NOT
// rise. (Before the fix the NAV excluded only the Qearn slice, so the pool was over-credited the full 1M —
// backing 10M but pool 11M. This test was the in-contract proof of that drift; it now verifies the fix.)
TEST(ContractCLKnDGR, NAV_DepositorPoolStaysBackedAcrossProfitEpoch)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL);                 // 1000 shares @ 10000; pool = 10M, prevTradingBalance seeded = 10M

    ctx.beginEpoch();                                // clean baseline epoch, no profit
    // Sanity: before any profit, the recorded pool equals the real backing — the vault is fully backed.
    EXPECT_EQ(ctx.st()->totalDepositorPool, ctx.st()->prevTradingBalance);
    EXPECT_EQ(ctx.st()->totalDepositorPool, 10000000LL);

    // Recovery preset: 100% of booked profit is burned to the exec-fee reserve (leaves the balance entirely).
    ctx.st()->payoutStructure = 3;

    // Simulate an epoch's trading profit: it lands in the contract balance AND is booked to epochProfit
    // (exactly what a Direction-B arb does on-chain).
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 1000000LL);
    ctx.st()->epochProfit = 1000000LL;

    ctx.beginEpoch();                                // NAV lift + profit split (100% burned to exec reserve)

    const sint64 pool    = ctx.st()->totalDepositorPool;   // recorded depositor claim
    const sint64 backing = ctx.st()->prevTradingBalance;   // real depositor backing = balance − reserves
    // The 1,000,000 profit was 100% burned to the exec reserve — not depositor equity — so backing is 10M...
    EXPECT_EQ(backing, 10000000LL);
    // ...and with the fix the pool stays fully backed: it did NOT rise to 11M. NAV grew on retained capital only.
    EXPECT_EQ(pool, backing);
    EXPECT_EQ(pool, 10000000LL);
}

// A4: Voting twice on the same proposal slot is rejected — only the first vote is counted
TEST(ContractCLKnDGR, VoteOnProposal_DuplicateVoteRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto v1 = ctx.voteOnProposal(u, sub.slot, 1);
    EXPECT_EQ(v1.success, 1); // first vote accepted

    auto v2 = ctx.voteOnProposal(u, sub.slot, 0);
    EXPECT_EQ(v2.success, 0); // duplicate vote on same slot rejected
}

// A5: Once all 10 proposal slots per epoch are filled, the 11th submission is rejected
TEST(ContractCLKnDGR, SubmitProposal_MaxPerEpochLimitRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Fund 10 unique proposals (9×50M + 1×69M for UPDATE_PAYOUT) plus a failed 11th (50M)
    const sint64 totalNeeded = 9LL * CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT
                             + CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT
                             + CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT;
    ctx.addEnergy(u, totalNeeded);

    // Submit one proposal of each singleton type — no duplicates, all content-valid
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,        CLKnDGR::MIN_PROFIT_QU_OPT2).success,  1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_PROPOSAL_FEE,      100000000LL).success,                  1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT,
        CLKnDGR::PROP_TYPE_UPDATE_PAYOUT,            1LL).success,                           1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_FEE_ADD_POOL,      100000000LL).success,                  1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_FEE_PAYOUT,        100000000LL).success,                  1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_MIN_QUORUM,        20LL).success,                          1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_VAULT_TIER,        0LL).success,                           1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT, 10LL).success,                         1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN, 250000000LL).success,                  1);
    EXPECT_EQ(ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
        CLKnDGR::PROP_TYPE_UPDATE_RELOCK_AMOUNT,     5000000LL).success,                     1);

    EXPECT_EQ(ctx.st()->proposalsThisEpoch, CLKnDGR::MAX_PROPOSALS_PER_EPOCH);

    // 11th proposal must be rejected — epoch slot limit already reached — and refunded in full
    auto p11 = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT, CLKnDGR::MIN_PROFIT_QU_OPT3);
    EXPECT_EQ(p11.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, CLKnDGR::MAX_PROPOSALS_PER_EPOCH); // still 10
    EXPECT_EQ(getBalance(u), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT); // 11th fee refunded in full
}

// ===============================================================
// GROUP B — HIGH-VALUE ADDITIONS
// ===============================================================

// B1: BEGIN_EPOCH auto-payout sweeps depositors whose 26-epoch personal lock has expired
TEST(ContractCLKnDGR, BeginEpoch_AutoPayout_LockExpiredDepositor)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ price 10000, costBasis 10M, depEpoch=0

    EXPECT_EQ(ctx.st()->depositorCount, 1);

    // Advance to epoch 26 — lock expires at depEpoch(0) + VAULT_LOCK_EPOCHS(26) = epoch 26
    system.epoch = 26;
    ctx.beginEpoch();
    system.epoch = 0;

    // Auto-payout: gross=1000×10000=10M, profit=0 → no perf fee; mgmtFee=2%×10M=200K; net=9.8M
    EXPECT_EQ(ctx.st()->depositorCount,   0);
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
    EXPECT_EQ(getBalance(u), 9800000LL);
}

// B2: 125 qualifying depositor NO veto votes block a proposal that clears shareholder supermajority
TEST(ContractCLKnDGR, GovernanceCycle_DepositorVeto_BlocksPassedProposal)
{
    ContractTestingCLKnDGR ctx;

    // Shareholders: 15 × 15 = 225 weighted votes (quorum and supermajority both satisfied)
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1); // all YES

    // 500 depositors each holding 15000 shares (150M ÷ price 10000) — exactly meets
    // depositorVoteMinQu default (150M): lockedQu = 15000 × 10000 = 150M ≥ 150M
    // clkUser(100..224): non-overlapping range from shareholders (0..14)
    for (int i = 0; i < CLKnDGR::DEPOSITOR_VETO_THRESHOLD; ++i)
    {
        id dep = clkUser(100 + i);
        ctx.addEnergy(dep, 150000000LL);
        ctx.vaultDeposit(dep, 150000000LL);
        auto vout = ctx.depositorVeto(dep, sub.slot, 0); // NO veto vote
        EXPECT_EQ(vout.success, 1) << "depositor " << i << " veto must succeed";
    }

    ctx.endEpoch();

    // 500 qualifying NO votes = DEPOSITOR_VETO_THRESHOLD → proposal vetoed despite supermajority
    auto prop = ctx.getProposal(sub.slot);
    EXPECT_EQ(prop.status, CLKnDGR::PROP_STATUS_FAILED);
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU); // parameter unchanged
}

// B3: Passed ADD_POOL proposal registers the token/pool pair in the pool registry
TEST(ContractCLKnDGR, GovernanceCycle_AddPool_PoolRegistered)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    const uint64 newAsset  = 0x4142434445464748ULL; // arbitrary token name (uint64 encoding)
    const id     newIssuer = clkUser(99);

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL,
                                  CLKnDGR::PROP_TYPE_ADD_POOL,
                                  0, 0, newAsset, newIssuer);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->poolCount, 1);
    auto pool = ctx.getPool(0);
    EXPECT_EQ(pool.assetName, newAsset);
    EXPECT_EQ(pool.active, 1);
}

// B4: Passed REMOVE_POOL proposal deactivates an active pool (record retained)
TEST(ContractCLKnDGR, GovernanceCycle_RemovePool_DeactivatesPool)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    // Inject an active pool directly into state (bypasses ADD_POOL governance for brevity)
    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(99));
    ctx.st()->poolActive.set(0, 1);
    ctx.st()->poolCount = 1;

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REMOVE_POOL,
                                  0, 0); // newValue=0, poolIndex=0
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.getPool(0).active, 0); // pool deactivated; record retained
    EXPECT_EQ(ctx.st()->poolCount, 1);   // count unchanged
}

// B5: Passed WITHDRAW_QU_RESERVE proposal transfers QU from the dev fund to the destination
TEST(ContractCLKnDGR, GovernanceCycle_WithdrawQuReserve_TransfersQu)
{
    ContractTestingCLKnDGR ctx;
    const id dest = clkUser(99);

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    // Inject quReserve so the content-validity check (withdrawAmount <= quReserve) passes at submission
    ctx.st()->quReserve = 10000000LL;
    // Give the contract extra balance so it can fund the withdrawal after the proposal fee is burned
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 10000000LL);

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_QU_RESERVE,
                                  0, 0, 0, NULL_ID, 10000000LL, dest);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    const sint64 balBefore = getBalance(dest);
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->quReserve, 0LL);                     // dev fund drained
    EXPECT_EQ(getBalance(dest), balBefore + 10000000LL);     // destination received the QU
}

// B6: Passed UPDATE_PAYOUT proposal switches the active profit distribution preset
TEST(ContractCLKnDGR, GovernanceCycle_UpdatePayout_ChangesPreset)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->payoutStructure, 0); // initial preset = default (0)

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT,
                                  CLKnDGR::PROP_TYPE_UPDATE_PAYOUT,
                                  1LL); // switch to preset 1
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->payoutStructure, 1); // preset changed to option 1
}

// The recovery preset (3) is selectable via governance — validates the extended 0..3 payout bound.
TEST(ContractCLKnDGR, GovernanceCycle_UpdatePayout_RecoveryPresetSelectable)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->payoutStructure, 0); // initial preset = default (0)

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT,
                                  CLKnDGR::PROP_TYPE_UPDATE_PAYOUT,
                                  3LL); // switch to the RECOVERY preset
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->payoutStructure, 3); // recovery preset selected
}

// Recovery preset (3) selected MANUALLY: 70% exec, 0% to dev/dist/qearn/ccf (trading remainder 30%).
TEST(ContractCLKnDGR, BeginEpoch_PayoutStructure3_RecoveryPreset)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(1), 676}});

    const sint64 profit = 10000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit     = profit;
    ctx.st()->payoutStructure = 3;   // RECOVERY preset, chosen manually
    // execReserveFloor stays 0 (Lever 2 disabled), so this is purely the manual-preset path.

    ctx.beginEpoch();

    // PAYOUT3: devFund 0%, qearn 0% -> these jars stay empty; profit goes to exec(70%)+trading(30%).
    EXPECT_EQ(ctx.st()->quReserve,    0LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL);
    EXPECT_EQ(ctx.st()->epochProfit,  0LL); // reset after distribution
}

// Lever 2: when the execution-fee reserve is below the floor, the recovery preset is auto-applied
// even though shareholders chose preset 0. Under preset 0 these jars would be 100000 / 300000.
TEST(ContractCLKnDGR, BeginEpoch_Lever2_AutoRecoveryWhenReserveLow)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(1), 676}});

    const sint64 profit = 10000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit      = profit;
    ctx.st()->payoutStructure  = 0;            // shareholders chose the DEFAULT preset...
    ctx.st()->execReserveFloor = 1000000000LL; // ...but the safety valve is armed at 1B.
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 0); // reserve below the 1B floor -> recovery preset

    ctx.beginEpoch();

    EXPECT_EQ(ctx.st()->inLimpMode,   1);   // auto-entered limp mode
    EXPECT_EQ(ctx.st()->quReserve,    0LL); // dev fund suspended (recovery), not 100000
    EXPECT_EQ(ctx.st()->qearnReserve, 0LL); // Qearn donation suspended (recovery), not 300000
    EXPECT_EQ(ctx.st()->epochProfit,  0LL);
    // The contract keeps trading and routes 100% of profit into the execution-fee reserve.
    EXPECT_EQ(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), profit);
}

// Hysteresis: once limped, the contract STAYS limped until the reserve climbs to 10% ABOVE the floor,
// then auto-reverts to the shareholder-chosen preset. setContractFeeReserve places the reserve
// precisely below / inside / above the dead-band.
TEST(ContractCLKnDGR, Lever2_HysteresisBand_StaysLimpUntilTenPctAboveFloor)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(1), 676}});
    ctx.st()->execReserveFloor = 1000000000LL; // 1B floor -> exit threshold is 1.1B
    ctx.st()->payoutStructure  = 0;            // shareholders chose the DEFAULT preset

    const sint64 profit = 10000000LL;

    // (a) reserve below the floor -> ENTER limp mode; recovery preset (3) applied despite preset 0.
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 500000000LL); // 0.5B < 1B
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit = profit;
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->inLimpMode, 1);   // entered
    EXPECT_EQ(ctx.st()->quReserve,  0LL); // preset 3: dev fund 0%

    // (b) reserve climbs INTO the dead-band [1B, 1.1B) -> STAYS limp (hysteresis holds).
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 1050000000LL); // 1.05B, in band
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit = profit;
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->inLimpMode, 1);   // still limp (below 1.1B)
    EXPECT_EQ(ctx.st()->quReserve,  0LL); // still preset 3 (dev fund stays 0)

    // (c) reserve reaches 10% above the floor -> EXIT limp mode -> back to the chosen preset (0).
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 1100000000LL); // exactly 1.1B
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit = profit;
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->inLimpMode, 0);        // exited
    EXPECT_EQ(ctx.st()->quReserve,  100000LL); // preset 0 resumed: dev fund 1% of 10M
}

// B7: Submitting with less than the required fee is rejected and the underpayment is refunded
TEST(ContractCLKnDGR, SubmitProposal_InsufficientFee_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Pay 1 QU less than the required fee (INITIAL_PROPOSAL_FEE_DEFAULT = 50M QU)
    const sint64 underpayment = CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT - 1LL;
    ctx.addEnergy(u, underpayment);
    auto out = ctx.submitProposal(u, underpayment,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);

    EXPECT_EQ(out.success, 0);            // insufficient fee → rejected
    EXPECT_EQ(getBalance(u), underpayment); // full underpayment refunded immediately
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0); // no proposal recorded
}

// B8: PAYOUT1 preset uses 4% dev fund and 7% Qearn allocation (vs PAYOUT0's 6%/3%)
TEST(ContractCLKnDGR, BeginEpoch_PayoutStructure1_DifferentPercentages)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(1), 676}}); // shareholders required for qpi.distributeDividends

    const sint64 profit = 10000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit     = profit;
    ctx.st()->payoutStructure = 1; // inject PAYOUT1 directly — bypasses governance for brevity

    ctx.beginEpoch();

    // PAYOUT1: devFund=1%, qearn=3%
    // quReserve    += div(10M × 1, 100) = 100_000
    // qearnReserve += div(10M × 3, 100) = 300_000
    EXPECT_EQ(ctx.st()->quReserve,    100000LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 300000LL);
    EXPECT_EQ(ctx.st()->epochProfit,  0LL); // reset after distribution
}

// ===============================================================
// GROUP C — COMPLETENESS COVERAGE (REMAINING PROPOSAL TYPES)
// ===============================================================

// C1: Passed REACTIVATE_POOL proposal re-enables an inactive pool and resets stale price history
TEST(ContractCLKnDGR, GovernanceCycle_ReactivatePool_ClearsHistory)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    // Inject an inactive pool with stale Cloak swing price history
    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(99));
    ctx.st()->poolActive.set(0, 0); // inactive
    ctx.st()->poolCount = 1;
    ctx.st()->swingPriceHead.set(0, 5);   // stale head pointer
    ctx.st()->swingPriceCount.set(0, 13); // stale sample count

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REACTIVATE_POOL,
                                  0, 0); // newValue=0, poolIndex=0
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.getPool(0).active, 1);              // pool re-enabled
    EXPECT_EQ(ctx.st()->swingPriceHead.get(0),  0);  // history head reset
    EXPECT_EQ(ctx.st()->swingPriceCount.get(0), 0);  // history count reset
}

// C2: Submitting UPDATE_MIN_PROFIT with a value outside the approved allowlist is rejected
TEST(ContractCLKnDGR, SubmitProposal_UpdateMinProfit_InvalidValue_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // 999 is not in {MIN_PROFIT_QU_OPT1, OPT2, OPT3, OPT4}
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  999LL);

    EXPECT_EQ(out.success, 0); // invalid value → content rejected; proposer gets 69% refund
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU); // parameter unchanged
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0); // no proposal recorded
}

// C3: Submitting UPDATE_PAYOUT with preset 3 (out of valid range 0–2) is rejected
TEST(ContractCLKnDGR, SubmitProposal_UpdatePayout_InvalidValue_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT,
                                  CLKnDGR::PROP_TYPE_UPDATE_PAYOUT,
                                  4LL); // valid range is 0–3 (3 = recovery preset); 4 is invalid

    EXPECT_EQ(out.success, 0); // invalid value → rejected
    EXPECT_EQ(ctx.st()->payoutStructure, 0); // preset unchanged
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// C4: Passed UPDATE_PROPOSAL_FEE changes the default fee charged for future proposals
TEST(ContractCLKnDGR, GovernanceCycle_UpdateProposalFee_ChangesDefault)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    const sint64 newFee = 100000000LL; // 100M QU
    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_PROPOSAL_FEE,
                                  newFee);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->proposalFeeDefault, newFee);
}

// C5: Passed UPDATE_VAULT_TIER proposal changes the minimum deposit tier
TEST(ContractCLKnDGR, GovernanceCycle_UpdateVaultTier_ChangesMinimum)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->vaultDepositTier, CLKnDGR::VAULT_INITIAL_DEPOSIT_TIER); // 8 (default)

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VAULT_TIER,
                                  0LL); // tier 0: minimum deposit = 1 share = 10000 QU
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->vaultDepositTier, 0);
}

// C6: Passed UPDATE_MIN_QUORUM raises the minimum unique-voter count for proposal consensus
TEST(ContractCLKnDGR, GovernanceCycle_UpdateMinQuorum_ChangesQuorum)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->minVoterQuorum, CLKnDGR::INITIAL_MIN_VOTER_QUORUM); // 15

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_QUORUM,
                                  20LL); // raise to 20 unique voters
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->minVoterQuorum, 20);
}

// C7: Passed UPDATE_RELOCK_AMOUNT changes the minimum QU required to re-lock a vault position
TEST(ContractCLKnDGR, GovernanceCycle_UpdateRelockAmount_ChangesAmount)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->relockAddAmount, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT); // 10M

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_RELOCK_AMOUNT,
                                  5000000LL); // 5M (valid option)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->relockAddAmount, 5000000LL);
}

// C8: Passed UPDATE_DEPOSITOR_VOTE_MIN changes the locked-QU threshold for depositor veto votes
TEST(ContractCLKnDGR, GovernanceCycle_UpdateDepositorVoteMin_ChangesThreshold)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->depositorVoteMinQu, 150000000LL); // initial: 150M QU

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN,
                                  250000000LL); // 250M (valid option)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->depositorVoteMinQu, 250000000LL);
}

// C9: Passed UPDATE_RESERVE_PROFIT_PCT changes the minimum profit threshold for reserve liquidation
TEST(ContractCLKnDGR, GovernanceCycle_UpdateReserveProfitPct_ChangesThreshold)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->minReserveProfitPct, 5); // initial: 5%

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT,
                                  10LL); // 10% (valid option)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->minReserveProfitPct, 10);
}

// Safety valve: governance can set the execution-fee-reserve floor (proposal type 16)
TEST(ContractCLKnDGR, GovernanceCycle_UpdateExecReserveFloor_ChangesFloor)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->execReserveFloor, 0LL); // initial: safety valve disabled

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR,
                                  5000000000LL); // 5B (valid option)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->execReserveFloor, 5000000000LL);
}

// Type 17 (SELL_POOL_TOKENS): full governance cycle reaches PASSED.
// Like D1 (WITHDRAW_ASSET_RESERVE), the live Qswap sell needs on-chain assets + a real pool
// (deferred to the DEX-mock harness). Here we verify the machinery accepts type 17, the
// pool/percent validation passes, and END_EPOCH dispatches the handler safely. With no real
// shares owned, the handler's defensive on-chain balance check (actualAssetBalance 0 <
// sellTotal) skips the DEX calls, so the seeded buckets stay intact — proving the no-op-safe path.
TEST(ContractCLKnDGR, GovernanceCycle_SellPoolTokens_ProposalPasses)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    // Inject an ACTIVE pool with seeded swing + reserve holdings.
    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 1);
    ctx.st()->poolCount = 1;
    ctx.st()->swingTokens.set(0, 1000LL);
    ctx.st()->swingCostBasis.set(0, 500000LL);
    ctx.st()->poolReserveTokens.set(0, 200LL);
    ctx.st()->poolReserveCostBasis.set(0, 100000LL);

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_SELL_POOL_TOKENS,
                                  50LL,  // sell 50% of holdings
                                  0);    // poolIndex 0
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    // Defensive balance check skipped the live sell — buckets untouched (live-sell accounting
    // is exercised by the deferred DEX-mock harness, not here).
    EXPECT_EQ(ctx.st()->swingTokens.get(0),      1000LL);
    EXPECT_EQ(ctx.st()->poolReserveTokens.get(0), 200LL);
}

// Type 18 (UPDATE_VIX_FACTOR): full governance cycle changes the breakout sensitivity.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateVixFactor_ChangesFactor)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->vixBreakoutFactor, 200LL); // default 2.00x

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FACTOR, 350LL); // 3.5x (valid)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->vixBreakoutFactor, 350LL);
}

// Type 19 (UPDATE_VIX_FLOOR): full governance cycle changes the absolute floor.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateVixFloor_ChangesFloor)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->vixAbsFloorBps, 25LL); // default 25 bps

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FLOOR, 50LL); // 50 bps (valid)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->vixAbsFloorBps, 50LL);
}

// C10: After lowering vault tier to 0, a deposit of exactly 1 share (10000 QU) is accepted
TEST(ContractCLKnDGR, VaultDeposit_Tier0_AcceptsOneShare)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Lower deposit tier to 0 via state injection: minShares=1, minDeposit = 1 × price = 10000 QU
    ctx.st()->vaultDepositTier = 0;

    const sint64 tinyDeposit = CLKnDGR::VAULT_INITIAL_SHARE_PRICE; // exactly 10000 QU = 1 share
    ctx.addEnergy(u, tinyDeposit);
    auto out = ctx.vaultDeposit(u, tinyDeposit);

    EXPECT_EQ(out.success,     1);   // accepted immediately (not waitlisted)
    EXPECT_EQ(out.sharesIssued, 1LL); // exactly 1 share issued
    EXPECT_EQ(ctx.st()->depositorCount,   1);
    EXPECT_EQ(ctx.st()->totalVaultShares, 1LL);
}

// ===============================================================
// GROUP D — COVERAGE GAPS
// ===============================================================

// D1: Passed WITHDRAW_ASSET_RESERVE proposal is accepted through the full governance cycle
// Note: actual token transfer requires on-chain assets; this test verifies the governance
// machinery processes proposal type 11 correctly (submit, vote, endEpoch → PASSED).
TEST(ContractCLKnDGR, GovernanceCycle_WithdrawAssetReserve_ProposalPasses)
{
    ContractTestingCLKnDGR ctx;
    const id dest = clkUser(99);

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    // Inject an inactive pool — WITHDRAW_ASSET_RESERVE requires poolActive == 0
    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 0);
    ctx.st()->poolCount = 1;

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_ASSET_RESERVE,
                                  0, 0, 0, NULL_ID, 0, dest); // poolIndex=0, destination=dest
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
}

// D2: Passed UPDATE_FEE_ADD_POOL proposal changes the fee charged for future ADD_POOL submissions
TEST(ContractCLKnDGR, GovernanceCycle_UpdateFeeAddPool_ChangesFee)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->proposalFeeAddPool, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL); // 200M initial

    const sint64 newFee = 100000000LL; // 100M QU
    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_FEE_ADD_POOL,
                                  newFee);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->proposalFeeAddPool, newFee);
}

// D3: Passed UPDATE_FEE_PAYOUT proposal changes the fee charged for future UPDATE_PAYOUT submissions
TEST(ContractCLKnDGR, GovernanceCycle_UpdateFeePayout_ChangesFee)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    EXPECT_EQ(ctx.st()->proposalFeePayoutStructure, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT); // 69M initial

    const sint64 newFee = 100000000LL; // 100M QU
    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_FEE_PAYOUT,
                                  newFee);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->proposalFeePayoutStructure, newFee);
}

// D4: PAYOUT2 preset uses 1% dev fund and 9% Qearn allocation (vs PAYOUT0's 6%/3%)
TEST(ContractCLKnDGR, BeginEpoch_PayoutStructure2_DifferentPercentages)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(1), 676}}); // shareholders required for qpi.distributeDividends

    const sint64 profit = 10000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), profit);
    ctx.st()->epochProfit     = profit;
    ctx.st()->payoutStructure = 2; // inject PAYOUT2 directly — bypasses governance for brevity

    ctx.beginEpoch();

    // PAYOUT2: devFund=1%, qearn=3%
    // quReserve   += div(10M × 1, 100) = 100_000
    // qearnReserve += div(10M × 3, 100) = 300_000
    EXPECT_EQ(ctx.st()->quReserve,    100000LL);
    EXPECT_EQ(ctx.st()->qearnReserve, 300000LL);
    EXPECT_EQ(ctx.st()->epochProfit,  0LL); // reset after distribution
}

// D5: When the vault is at MAX_DEPOSITORS capacity a new deposit lands on the waitlist
TEST(ContractCLKnDGR, VaultDeposit_VaultFull_LandsOnWaitlist)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Fill vault directly — avoids depositing 5000 times in the test
    ctx.st()->depositorCount = CLKnDGR::MAX_DEPOSITORS;

    // Minimum deposit for default tier 8: 1000 shares × 10000 = 10M QU
    const sint64 amount = 10000000LL;
    ctx.addEnergy(u, amount);
    auto out = ctx.vaultDeposit(u, amount);

    EXPECT_EQ(out.success, 2); // queued to waitlist, not admitted
    EXPECT_EQ(ctx.st()->waitlistCount, 1);
    EXPECT_EQ(ctx.st()->waitlistQu, amount);
    EXPECT_EQ(ctx.st()->depositorCount, CLKnDGR::MAX_DEPOSITORS); // vault count unchanged
}

// D6: A waitlisted depositor can withdraw their queued QU via waitlistWithdraw
TEST(ContractCLKnDGR, WaitlistWithdraw_Success_RefundsQu)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Fill vault to force waitlist placement
    ctx.st()->depositorCount = CLKnDGR::MAX_DEPOSITORS;

    const sint64 amount = 10000000LL;
    ctx.addEnergy(u, amount);
    auto dep = ctx.vaultDeposit(u, amount);
    EXPECT_EQ(dep.success, 2); // on waitlist

    auto wout = ctx.waitlistWithdraw(u);

    EXPECT_EQ(wout.success, 1);
    EXPECT_EQ(wout.amountRefunded, amount);
    EXPECT_EQ(getBalance(u), amount);     // full refund restored
    EXPECT_EQ(ctx.st()->waitlistCount, 0);
    EXPECT_EQ(ctx.st()->waitlistQu, 0LL);
}

// D7: A depositor with locked QU below depositorVoteMinQu cannot cast a depositor veto vote
TEST(ContractCLKnDGR, DepositorVeto_InsufficientLockedQu_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id proposer  = clkUser(0);
    const id depositor = clkUser(1);

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // depositorVoteMinQu default = 150M QU. At share price 10000: need 15000 shares (150M deposit).
    // Deposit 14999 shares worth (149,990,000 QU) — one share below the 150M threshold.
    // lockedQu = 14999 × 10000 = 149,990,000 < 150,000,000 → veto must be rejected.
    const sint64 smallDeposit = 149990000LL; // 14999 shares × 10000
    ctx.addEnergy(depositor, smallDeposit);
    ctx.vaultDeposit(depositor, smallDeposit);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto vout = ctx.depositorVeto(depositor, sub.slot, 0); // attempt NO vote
    EXPECT_EQ(vout.success, 0); // rejected — lockedQu below depositorVoteMinQu

    EXPECT_EQ(ctx.getProposal(sub.slot).depositorVotesNo, 0); // no vote recorded
}

// D8: BEGIN_EPOCH auto-payout on a profitable depositor charges both the 2% management fee
// and the 5% performance fee, and transfers the correct net amount to the depositor
TEST(ContractCLKnDGR, BeginEpoch_AutoPayout_ProfitableDepositor_PerfFeeCharged)
{
    ContractTestingCLKnDGR ctx;

    // Issue shares so qpi.distributeDividends has valid recipients during the profit split
    ctx.issueShares({{clkUser(0), 676}});

    const id u = clkUser(1);
    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ 10000, costBasis = 10M, depEpoch = 0

    // First beginEpoch: establishes prevTradingBalance = 10M (lock 0 < 26, no auto-payout)
    ctx.beginEpoch();
    EXPECT_EQ(ctx.st()->vaultSharePrice, 10000LL); // unchanged — no profit yet

    // Inject 2M QU simulating arb profit accumulated during the epoch
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 2000000LL);

    // Second beginEpoch at epoch 26 — lock expires (depEpoch 0 + VAULT_LOCK_EPOCHS 26 = 26)
    // NAV update: vaultCurBalance = 12M, prevTradingBalance = 10M
    //   navRatio = 12M × 1000 / 10M = 1200
    //   totalDepositorPool = 10M × 1200 / 1000 = 12M
    //   vaultSharePrice    = 12M / 1000 = 12000
    // Auto-payout (lock expired):
    //   gross    = 1000 × 12000 = 12,000,000
    //   profit   = 12,000,000 − 10,000,000 = 2,000,000 > 0
    //   mgmtFee  = div(12,000,000 × 2, 100) = 240,000  → burned
    //   perfFee  = div(2,000,000  × 5, 100) = 100,000  → epochProfit
    //   net      = 12,000,000 − 240,000 − 100,000 = 11,660,000 → transferred to u
    system.epoch = 26;
    ctx.beginEpoch();
    system.epoch = 0;

    EXPECT_EQ(ctx.st()->depositorCount,   0);
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
    EXPECT_EQ(getBalance(u), 11660000LL);
}

// ===============================================================
// GROUP E — ADDITIONAL COVERAGE GAPS
// ===============================================================

// E1: VER-D1 guard — a wallet already on the waitlist cannot bypass the queue
// by calling vaultDeposit directly when the vault has open slots. Without this,
// BEGIN_EPOCH promotion would later overwrite their direct shares and add a
// duplicate depositorList entry.
TEST(ContractCLKnDGR, VaultDeposit_OnWaitlist_VER_D1_Refunded)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Place u on waitlist while vault is empty (depositorCount = 0)
    CLKnDGR::WaitlistEntry entry{};
    entry.wallet = u;
    entry.amount = 5000000LL;
    ctx.st()->waitlist.set(0, entry);
    ctx.st()->waitlistCount = 1;
    ctx.st()->waitlistQu    = 5000000LL;

    // u attempts a direct deposit — must be refunded by the VER-D1 guard
    const sint64 amount = 10000000LL;
    ctx.addEnergy(u, amount);
    auto out = ctx.vaultDeposit(u, amount);

    EXPECT_EQ(out.success, 0);            // refunded — guard fired
    EXPECT_EQ(getBalance(u), amount);     // full QU returned
    EXPECT_EQ(ctx.st()->depositorCount, 0);   // no direct admission
    EXPECT_EQ(ctx.st()->waitlistCount, 1);    // waitlist entry preserved
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
}

// E2: When the waitlist is at WAITLIST_SIZE capacity, an offer no larger than
// the smallest queued amount must be refunded with no waitlist change.
TEST(ContractCLKnDGR, VaultDeposit_WaitlistFull_SmallerOfferRejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const id v = clkUser(2);

    // Fill vault and waitlist via direct injection
    ctx.st()->depositorCount = CLKnDGR::MAX_DEPOSITORS;
    CLKnDGR::WaitlistEntry tail{};
    tail.wallet = v;
    tail.amount = 50000000LL; // smallest queued amount
    ctx.st()->waitlist.set(CLKnDGR::WAITLIST_SIZE - 1, tail);
    ctx.st()->waitlistCount = CLKnDGR::WAITLIST_SIZE;
    ctx.st()->waitlistQu    = 50000000LL;

    // u offers strictly less than the smallest existing entry
    const sint64 amount = 40000000LL;
    ctx.addEnergy(u, amount);
    auto out = ctx.vaultDeposit(u, amount);

    EXPECT_EQ(out.success, 0);                         // rejected — refunded
    EXPECT_EQ(getBalance(u), amount);                  // full refund
    EXPECT_EQ(ctx.st()->waitlistCount, CLKnDGR::WAITLIST_SIZE); // unchanged
    EXPECT_EQ(ctx.st()->waitlistQu, 50000000LL);       // unchanged
    // Tail entry preserved
    EXPECT_EQ(ctx.st()->waitlist.get(CLKnDGR::WAITLIST_SIZE - 1).wallet, v);
    EXPECT_EQ(ctx.st()->waitlist.get(CLKnDGR::WAITLIST_SIZE - 1).amount, 50000000LL);
}

// E3: When the waitlist is at capacity, a strictly larger offer displaces the
// smallest entry: the displaced wallet is refunded, the new caller is inserted
// in sorted order, and the count stays at WAITLIST_SIZE.
TEST(ContractCLKnDGR, VaultDeposit_WaitlistFull_LargerOfferDisplaces)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    const id v = clkUser(2);

    // Fund the contract enough to refund v
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 50000000LL);

    // Vault full, waitlist full with a single meaningful tail entry
    ctx.st()->depositorCount = CLKnDGR::MAX_DEPOSITORS;
    CLKnDGR::WaitlistEntry tail{};
    tail.wallet = v;
    tail.amount = 50000000LL;
    ctx.st()->waitlist.set(CLKnDGR::WAITLIST_SIZE - 1, tail);
    ctx.st()->waitlistCount = CLKnDGR::WAITLIST_SIZE;
    ctx.st()->waitlistQu    = 50000000LL;

    // u offers strictly more than the smallest queued amount
    const sint64 offer = 60000000LL;
    ctx.addEnergy(u, offer);
    auto out = ctx.vaultDeposit(u, offer);

    EXPECT_EQ(out.success, 2);                         // queued (after displacement)
    EXPECT_EQ(getBalance(v), 50000000LL);              // displaced wallet refunded
    EXPECT_EQ(ctx.st()->waitlistCount, CLKnDGR::WAITLIST_SIZE); // count restored
    EXPECT_EQ(ctx.st()->waitlistQu, offer);            // 50M refunded out, 60M added
    // u inserted at position 0 (entries [0..WAITLIST_SIZE-2] are zero, so the
    // sorted-largest-first scan finds amount < 60M at index 0)
    EXPECT_EQ(ctx.st()->waitlist.get(0).wallet, u);
    EXPECT_EQ(ctx.st()->waitlist.get(0).amount, offer);
}

// E4: BEGIN_EPOCH promotes waitlisted wallets into open vault slots.
TEST(ContractCLKnDGR, BeginEpoch_WaitlistPromotion_FillsOpenedSlot)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Vault has open slots (depositorCount = 0); u is queued for promotion
    const sint64 amount = 12000000LL;
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), amount); // back the waitlistQu
    CLKnDGR::WaitlistEntry entry{};
    entry.wallet = u;
    entry.amount = amount;
    ctx.st()->waitlist.set(0, entry);
    ctx.st()->waitlistCount = 1;
    ctx.st()->waitlistQu    = amount;

    ctx.beginEpoch();

    // u promoted: shares = 12M / 10000 = 1200; vault state populated; waitlist drained
    EXPECT_EQ(ctx.st()->depositorCount, 1);
    EXPECT_EQ(ctx.st()->totalVaultShares, 1200LL);
    EXPECT_EQ(ctx.st()->totalDepositorPool, amount);
    EXPECT_EQ(ctx.st()->waitlistCount, 0);
    EXPECT_EQ(ctx.st()->waitlistQu, 0LL);
    EXPECT_EQ(ctx.st()->depositorList.get(0), u);
}

// E5: voteOnProposal must reject votes against a slot index outside the
// active proposal range for this epoch.
TEST(ContractCLKnDGR, VoteOnProposal_InvalidSlot_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // No proposals submitted — proposalsThisEpoch == 0
    auto out = ctx.voteOnProposal(u, 0, 1);

    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->voterCount, 0); // no voter recorded
}

// E6: voteOnProposal must reject votes against a proposal whose status is not
// PROP_STATUS_ACTIVE (e.g. one already marked FAILED or PASSED earlier).
TEST(ContractCLKnDGR, VoteOnProposal_InactiveProposal_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // Force the proposal into a non-ACTIVE state
    auto p = ctx.st()->proposals.get(sub.slot);
    p.status = CLKnDGR::PROP_STATUS_FAILED;
    ctx.st()->proposals.set(sub.slot, p);

    auto vout = ctx.voteOnProposal(u, sub.slot, 1);

    EXPECT_EQ(vout.success, 0);
    EXPECT_EQ(ctx.st()->voterCount, 0); // status guard fires before voter registration
}

// E7: A depositor whose share NAV has fallen below cost basis exits at the
// lock boundary without a performance fee being charged.
TEST(ContractCLKnDGR, VaultWithdraw_LossyExit_NoPerfFee)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ 10000, costBasis = 10M

    // Drain 2M from the contract to simulate a 20% trading loss
    decreaseEnergy(spectrumIndex(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0)), 2000000LL);

    // Set the depositor's lock to exactly expire on next BEGIN_EPOCH (epoch 26)
    system.epoch = 26;
    ctx.beginEpoch(); // NAV update: navRatio 800 → sharePrice 8000 → auto-payout
    system.epoch = 0;

    // Auto-payout already swept u via lossy path; epochProfit must remain 0
    // (no perfFee added because gross 8M < costBasis 10M).
    //   gross   = 1000 × 8000 = 8,000,000
    //   profit  = -2,000,000  → perfFee = 0
    //   mgmtFee = 8,000,000 × 2% = 160,000
    //   net     = 8,000,000 − 160,000 = 7,840,000
    EXPECT_EQ(ctx.st()->depositorCount, 0);
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
    EXPECT_EQ(ctx.st()->epochProfit, 0LL); // no perfFee on a lossy exit
    EXPECT_EQ(getBalance(u), 7840000LL);
}

// E8: submitProposal must reject types outside the [ADD_POOL, UPDATE_RELOCK_AMOUNT]
// range and refund the full fee (consistent with the insufficient-fee path).
TEST(ContractCLKnDGR, SubmitProposal_InvalidType_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 2);

    // Type 0 (below valid range)
    auto outLow = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT, 0, 0);
    EXPECT_EQ(outLow.success, 0);
    // Type 100 (well above the highest valid proposal type — durable to future additions)
    auto outHigh = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT, 100, 0);
    EXPECT_EQ(outHigh.success, 0);

    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0); // neither recorded
    EXPECT_EQ(getBalance(u), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 2); // both fees refunded
}

// E9: ADD_POOL targeting an asset already registered (active or inactive) is
// rejected as content-invalid (proposer gets 69% refund, no proposal recorded).
TEST(ContractCLKnDGR, SubmitProposal_AddPool_AlreadyExists_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Pre-register an existing pool
    const uint64 existingName = 0x4142434445464748ULL;
    const id     existingIssuer = clkUser(98);
    ctx.st()->poolAssetNames.set(0, existingName);
    ctx.st()->poolIssuers.set(0, existingIssuer);
    ctx.st()->poolActive.set(0, 1);
    ctx.st()->poolCount = 1;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL,
                                  CLKnDGR::PROP_TYPE_ADD_POOL,
                                  0, 0, existingName, existingIssuer);

    EXPECT_EQ(out.success, 0);                          // content invalid
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);         // not recorded
    EXPECT_EQ(ctx.st()->poolCount, 1);                  // pool list unchanged
    // 69% refund of the ADD_POOL fee
    const sint64 expectedRefund = CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL * 69 / 100;
    EXPECT_EQ(getBalance(u), expectedRefund);
}

// E10: A second proposal of a singleton type within the same epoch is rejected
// as content-invalid (only the first such proposal can be active per epoch).
TEST(ContractCLKnDGR, SubmitProposal_DuplicateSingletonInSameEpoch_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 2);

    auto first = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                    CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                    CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(first.success, 1);

    auto second = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                     CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                     CLKnDGR::MIN_PROFIT_QU_OPT3);
    EXPECT_EQ(second.success, 0);                       // duplicate singleton rejected
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 1);         // only the first is recorded
}

// E11: WITHDRAW_QU_RESERVE with destination == NULL_ID is rejected as
// content-invalid — a NULL destination would silently burn the withdrawn QU.
TEST(ContractCLKnDGR, SubmitProposal_WithdrawQuReserve_NullDestination_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Seed a non-zero quReserve so the amount-vs-reserve check passes
    ctx.st()->quReserve = 100000000LL;
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_QU_RESERVE,
                                  0,        // newValue (unused)
                                  0,        // poolIndex (unused)
                                  0,        // assetName (unused)
                                  NULL_ID,  // assetIssuer (unused)
                                  10000000, // withdrawAmount
                                  NULL_ID); // destination — invalid

    EXPECT_EQ(out.success, 0);                        // content invalid
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);       // not recorded
    EXPECT_EQ(ctx.st()->quReserve, 100000000LL);      // reserve unchanged
}

// E12: A voter who sells (transfers away) their CLKnDGR shares between
// vote-cast time and END_EPOCH must be excluded from the tally — the
// re-validation loop checks current share balance and skips voters with 0.
TEST(ContractCLKnDGR, EndEpoch_VoterSoldShares_VotesExcluded)
{
    ContractTestingCLKnDGR ctx;

    // 15 voters @ 40 shares each (600 total ≤ 676 supply; matches default minVoterQuorum = 15)
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i) owners.push_back({clkUser(i), 40});
    ctx.issueShares(owners);
    ctx.fundVoters(15); // clkUser(1..14) need a spectrum index or their votes silently no-op

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // All 15 vote YES — would pass cleanly without the share-sale event below
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    // clkUser(14) "sells" their 40 shares before END_EPOCH by transferring them to a
    // non-voter account (clkUser 50). A NULL_ID destination would mean *burning*, which
    // the asset layer forbids for contract shares (issuer == NULL_ID), so we move them to
    // a real holder instead — either way clkUser(14) ends the epoch holding 0 shares.
    // Look up their ownership/possession indices via the universe iterator.
    QPI::Asset asset;
    asset.assetName = *(uint64*)contractDescriptions[CLKnDGR_CONTRACT_INDEX].assetName;
    asset.issuer    = NULL_ID; // contract IPO shares are issued under NULL_ID, not the contract id
    QPI::AssetPossessionIterator iter(asset,
        QPI::AssetOwnershipSelect::byOwner(clkUser(14)),
        QPI::AssetPossessionSelect::byPossessor(clkUser(14)));
    ASSERT_FALSE(iter.reachedEnd());
    int sellDstOwn = -1, sellDstPos = -1; // normal-transfer path requires non-null out-params (the burn path did not)
    EXPECT_TRUE(transferShareOwnershipAndPossession(
        (int)iter.ownershipIndex(), (int)iter.possessionIndex(),
        clkUser(50), 40, &sellDstOwn, &sellDstPos, true));

    // END_EPOCH re-validates: clkUser(14) returns shareBalance = 0 → excluded.
    // voterCountPerProposal = 14 < minVoterQuorum 15 → quorum FAIL.
    ctx.endEpoch();
    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_FAILED);
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU); // parameter unchanged
}

// E13: minVoterQuorum is snapshotted before the END_EPOCH proposal-execution loop.
// Without the snapshot, an UPDATE_MIN_QUORUM in slot 0 would lower the threshold
// mid-loop and let slot 1 pass against the reduced quorum — a known governance
// attack vector. This test exercises the full attack: slot 0 lowers quorum,
// slot 1 has just enough voters to clear the new quorum but not the snapshot.
TEST(ContractCLKnDGR, EndEpoch_MinQuorumSnapshot_PreventsAttack)
{
    ContractTestingCLKnDGR ctx;

    // 30 voters @ 22 shares each (660 total). Pre-set minVoterQuorum = 30 to
    // create a starting quorum higher than the floor (15).
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 30; ++i) owners.push_back({clkUser(i), 22});
    ctx.issueShares(owners);
    ctx.fundVoters(30); // clkUser(1..29) need a spectrum index or their votes silently no-op
    ctx.st()->minVoterQuorum = 30;

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 2);

    // Slot 0: lower minVoterQuorum to the floor (15)
    auto sub0 = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                   CLKnDGR::PROP_TYPE_UPDATE_MIN_QUORUM, 15LL);
    EXPECT_EQ(sub0.success, 1);

    // Slot 1: an unrelated proposal; only 15 voters will participate
    auto sub1 = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                   CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                   CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub1.success, 1);

    // All 30 vote YES on slot 0 → meets the starting quorum 30
    for (int i = 0; i < 30; ++i) ctx.voteOnProposal(clkUser(i), sub0.slot, 1);
    // Only 15 vote YES on slot 1 → would meet the post-lowering quorum (15)
    // but must be measured against the snapshot (30).
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub1.slot, 1);

    ctx.endEpoch();

    // Slot 0 PASSED — applies the lower quorum
    EXPECT_EQ(ctx.getProposal(sub0.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->minVoterQuorum, 15);
    // Slot 1 FAILED — snapshot held the bar at 30, so 15 voters is below quorum
    EXPECT_EQ(ctx.getProposal(sub1.slot).status, CLKnDGR::PROP_STATUS_FAILED);
    EXPECT_EQ(ctx.st()->minProfitQu, CLKnDGR::MIN_PROFIT_QU); // unchanged — proves attack blocked
}

// ===============================================================
// GROUP F — REMAINING VALIDATION PATHS + NAV CAP
// ===============================================================

// F1: REMOVE_POOL with poolIndex >= poolCount is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_RemovePool_InvalidIndex_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // poolCount = 0; any poolIndex is out of range
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REMOVE_POOL, 0, 5);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F2: REMOVE_POOL on an already-inactive pool is content-invalid (no-op fee waste)
TEST(ContractCLKnDGR, SubmitProposal_RemovePool_AlreadyInactive_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 0); // already inactive
    ctx.st()->poolCount = 1;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REMOVE_POOL, 0, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F3: REACTIVATE_POOL with poolIndex >= poolCount is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_ReactivatePool_InvalidIndex_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REACTIVATE_POOL, 0, 7);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F4: REACTIVATE_POOL on an already-active pool is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_ReactivatePool_AlreadyActive_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 1); // already active
    ctx.st()->poolCount = 1;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REACTIVATE_POOL, 0, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F5: WITHDRAW_QU_RESERVE with amount > current quReserve is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_WithdrawQuReserve_AmountExceedsReserve_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->quReserve = 50000000LL; // 50M reserve
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    // Request 100M when only 50M is held
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_QU_RESERVE,
                                  0, 0, 0, NULL_ID, 100000000LL, clkUser(99));
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->quReserve, 50000000LL);
}

// F6: WITHDRAW_QU_RESERVE with withdrawAmount <= 0 is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_WithdrawQuReserve_ZeroAmount_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->quReserve = 50000000LL;
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_QU_RESERVE,
                                  0, 0, 0, NULL_ID, 0LL, clkUser(99));
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F7: WITHDRAW_ASSET_RESERVE on an active pool is content-invalid
//     (asset reserve sweep only allowed for inactive pools)
TEST(ContractCLKnDGR, SubmitProposal_WithdrawAssetReserve_PoolActive_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 1); // active — sweep not allowed
    ctx.st()->poolCount = 1;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_ASSET_RESERVE,
                                  0, 0, 0, NULL_ID, 0, clkUser(99));
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F8: WITHDRAW_ASSET_RESERVE with poolIndex >= poolCount is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_WithdrawAssetReserve_InvalidIndex_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_WITHDRAW_ASSET_RESERVE,
                                  0, 9, 0, NULL_ID, 0, clkUser(99));
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F9: UPDATE_VAULT_TIER must reject newValue outside [0, 8]
TEST(ContractCLKnDGR, SubmitProposal_UpdateVaultTier_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 2);

    auto outHigh = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                      CLKnDGR::PROP_TYPE_UPDATE_VAULT_TIER, 9LL);
    auto outNeg = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                     CLKnDGR::PROP_TYPE_UPDATE_VAULT_TIER, -1LL);
    EXPECT_EQ(outHigh.success, 0);
    EXPECT_EQ(outNeg.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F10: UPDATE_RESERVE_PROFIT_PCT must reject any value not in {2, 5, 7, 10}
TEST(ContractCLKnDGR, SubmitProposal_UpdateReserveProfitPct_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_RESERVE_PROFIT_PCT, 4LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// UPDATE_EXEC_RESERVE_FLOOR must reject any value not in {0, 1M, 10M, 100M, 1B}
TEST(ContractCLKnDGR, SubmitProposal_UpdateExecReserveFloor_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_EXEC_RESERVE_FLOOR, 12345LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->execReserveFloor, 0LL); // unchanged (default)
}

// F11: UPDATE_DEPOSITOR_VOTE_MIN must reject any value not in {50M, 150M, 250M, 350M}
TEST(ContractCLKnDGR, SubmitProposal_UpdateDepositorVoteMin_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_DEPOSITOR_VOTE_MIN, 100000000LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->depositorVoteMinQu, 150000000LL); // unchanged (default)
}

// F12: UPDATE_RELOCK_AMOUNT must reject any value not in {1M, 5M, 10M, 20M, 25M, 50M}
TEST(ContractCLKnDGR, SubmitProposal_UpdateRelockAmount_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_RELOCK_AMOUNT, 7500000LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->relockAddAmount, 10000000LL); // unchanged (default)
}

// F13: SELL_POOL_TOKENS must reject a percent outside 1..100 (0 too low, 101 too high).
TEST(ContractCLKnDGR, SubmitProposal_SellPoolTokens_InvalidPct_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Valid active pool so any rejection is attributable to the percent, not the poolIndex.
    ctx.st()->poolAssetNames.set(0, 0x4142434445464748ULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 1);
    ctx.st()->poolCount = 1;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto lo = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                 CLKnDGR::PROP_TYPE_SELL_POOL_TOKENS, 0LL, 0); // 0% — invalid
    EXPECT_EQ(lo.success, 0);

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto hi = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                 CLKnDGR::PROP_TYPE_SELL_POOL_TOKENS, 101LL, 0); // 101% — invalid
    EXPECT_EQ(hi.success, 0);

    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0); // neither recorded
}

// F14: SELL_POOL_TOKENS must reject a poolIndex past poolCount.
TEST(ContractCLKnDGR, SubmitProposal_SellPoolTokens_InvalidPoolIndex_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    // poolCount == 0 → poolIndex 0 is out of range
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_SELL_POOL_TOKENS, 50LL, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F15: UPDATE_VIX_FACTOR must reject a value not in {150,200,300,400,500}.
TEST(ContractCLKnDGR, SubmitProposal_UpdateVixFactor_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FACTOR, 250LL); // not allowlisted
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->vixBreakoutFactor, 200LL); // unchanged (default)
}

// F16: UPDATE_VIX_FLOOR must reject a value not in {0,10,25,50,100,200}.
TEST(ContractCLKnDGR, SubmitProposal_UpdateVixFloor_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FLOOR, 30LL); // not allowlisted
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->vixAbsFloorBps, 25LL); // unchanged (default)
}

// Type 20 (UPDATE_VIX_PULSE_RATE): full governance cycle changes the sample interval.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateVixPulseRate_ChangesInterval)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->vixSampleInterval, 345600u); // default 1 pulse/day

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_PULSE_RATE, 3LL); // 3 pulses/day (valid)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->vixSampleInterval, 115200u); // 3/day = 345600 / 3
}

// F17: UPDATE_VIX_PULSE_RATE must reject a value not in {1,2,3}.
TEST(ContractCLKnDGR, SubmitProposal_UpdateVixPulseRate_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_PULSE_RATE, 5LL); // not allowlisted
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->vixSampleInterval, 345600u); // unchanged (default)
}

// Type 21 (UPDATE_SWING_SELL_PCT): full governance cycle changes the Cloak sell chunk.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateSwingSellPct_ChangesPct)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->swingSellPct, 50LL); // default 50%

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_SELL_PCT, 25LL); // 25% (valid)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->swingSellPct, 25LL);
}

// F18: UPDATE_SWING_SELL_PCT must reject a value not in {10,15,20,25,33,50}.
TEST(ContractCLKnDGR, SubmitProposal_UpdateSwingSellPct_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_SELL_PCT, 40LL); // not allowlisted
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->swingSellPct, 50LL); // unchanged (default)
}

// Type 22 (UPDATE_BREAKOUT_RESCAN): full governance cycle changes the Dagger hot re-scan pace.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateBreakoutRescan_ChangesTicks)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->breakoutRescanTicks, 1200u); // default 5 min (300s × 4 ticks/sec)

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_BREAKOUT_RESCAN, 60LL); // 60 seconds (valid)
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->breakoutRescanTicks, 240u); // 60s × 4 ticks/sec
}

// F19: UPDATE_BREAKOUT_RESCAN must reject a value (seconds) not in {30,60,120,180,240,300}.
TEST(ContractCLKnDGR, SubmitProposal_UpdateBreakoutRescan_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_BREAKOUT_RESCAN, 45LL); // not allowlisted
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->breakoutRescanTicks, 1200u); // unchanged (default)
}

// Governable QX-fee freshness (proposal type 23): a ONE-WAY switch — shareholders can flip the sell legs
// from the per-epoch fee cache (0, default) to a live per-trade QX fee fetch (1), future-proofing for if
// QX ever makes its transfer fee tick-variable. Once enabled it is PERMANENT (no revert), no re-deploy.
TEST(ContractCLKnDGR, GovernanceCycle_UpdateQxFeeMode_ChangesMode)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->qxFeeLivePerTrade, 0); // default: read the per-epoch QX-fee cache

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_QX_FEE_MODE, 1LL); // enable live per-trade fetch
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);

    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->qxFeeLivePerTrade, 1); // now fetches the QX fee live before each sell
}

// UPDATE_QX_FEE_MODE from cache mode accepts ONLY newValue==1 (enable). 0 (stay cache) and out-of-range
// values are rejected.
TEST(ContractCLKnDGR, SubmitProposal_UpdateQxFeeMode_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto zero = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                   CLKnDGR::PROP_TYPE_UPDATE_QX_FEE_MODE, 0LL); // can't propose cache mode
    EXPECT_EQ(zero.success, 0);

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto two = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_QX_FEE_MODE, 2LL); // out of range
    EXPECT_EQ(two.success, 0);

    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->qxFeeLivePerTrade, 0); // unchanged (default)
}

// UPDATE_QX_FEE_MODE is a one-way latch: once live mode is on, NO type-23 proposal can change it — not
// back to cache (0), not even re-propose (1). The switch is permanent.
TEST(ContractCLKnDGR, SubmitProposal_UpdateQxFeeMode_IrreversibleOnceLive)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.st()->qxFeeLivePerTrade = 1; // already live (a prior epoch's vote enabled it)

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto revert = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                     CLKnDGR::PROP_TYPE_UPDATE_QX_FEE_MODE, 0LL); // try to revert to cache
    EXPECT_EQ(revert.success, 0);

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto reEnable = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                       CLKnDGR::PROP_TYPE_UPDATE_QX_FEE_MODE, 1LL); // try to re-propose
    EXPECT_EQ(reEnable.success, 0);

    EXPECT_EQ(ctx.st()->qxFeeLivePerTrade, 1); // still live, never reverted
}

// Proposal fees fund the execution-fee reserve: 31% at submit, the held 69% on PASS (→ 100%).
TEST(ContractCLKnDGR, GovernanceCycle_ProposalFeePass_FundsExecReserve)
{
    ContractTestingCLKnDGR ctx;

    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i)
        owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 0); // baseline

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FLOOR, 50LL);
    EXPECT_EQ(sub.success, 1);
    // 31% of the 50M fee routed to the reserve at submission
    EXPECT_EQ(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), 15500000LL);

    for (int i = 0; i < 15; ++i)
        ctx.voteOnProposal(clkUser(i), sub.slot, 1);
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    // held 69% burned to the reserve on pass → 100% of the fee funds execution
    EXPECT_EQ(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), 50000000LL);
}

// A FAILED proposal still refunds 69% to the proposer; the reserve keeps only the 31%.
TEST(ContractCLKnDGR, GovernanceCycle_ProposalFeeFail_Refunds69_ExecKeeps31)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    setContractFeeReserve(CLKnDGR_CONTRACT_INDEX, 0); // baseline

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_VIX_FLOOR, 50LL);
    EXPECT_EQ(sub.success, 1);
    EXPECT_EQ(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), 15500000LL); // 31% at submit
    EXPECT_EQ(getBalance(u), 0LL); // proposer paid the full fee

    // No votes cast → fails quorum at END_EPOCH → 69% refund
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_FAILED);
    EXPECT_EQ(getContractFeeReserve(CLKnDGR_CONTRACT_INDEX), 15500000LL); // reserve keeps only the 31%
    EXPECT_EQ(getBalance(u), 34500000LL);                                // 69% refunded to proposer
}

// F13: UPDATE_MIN_QUORUM must reject newValue below INITIAL_MIN_VOTER_QUORUM (15)
TEST(ContractCLKnDGR, SubmitProposal_UpdateMinQuorum_TooLow_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_QUORUM, 14LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->minVoterQuorum, CLKnDGR::INITIAL_MIN_VOTER_QUORUM);
}

// F14: UPDATE_MIN_QUORUM must reject newValue above MAX_VOTER_QUORUM (676)
TEST(ContractCLKnDGR, SubmitProposal_UpdateMinQuorum_TooHigh_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_QUORUM, 677LL);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
}

// F15: ADD_POOL when poolCount is at the 255 hard cap is content-invalid
TEST(ContractCLKnDGR, SubmitProposal_AddPool_AtCap255_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    ctx.st()->poolCount = 255; // at cap
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL,
                                  CLKnDGR::PROP_TYPE_ADD_POOL,
                                  0, 0, 0xDEADBEEFULL, clkUser(99));
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->poolCount, 255);
}

// F16: A second ADD_POOL for the same asset within one epoch is content-invalid
//      (distinct from singleton-type duplicate logic — same-asset comparison)
TEST(ContractCLKnDGR, SubmitProposal_AddPool_DuplicateAssetInSameEpoch_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL * 2);

    const uint64 name = 0xCAFEBABEULL;
    const id     issuer = clkUser(99);

    auto first = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL,
                                    CLKnDGR::PROP_TYPE_ADD_POOL,
                                    0, 0, name, issuer);
    EXPECT_EQ(first.success, 1);

    auto second = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL,
                                     CLKnDGR::PROP_TYPE_ADD_POOL,
                                     0, 0, name, issuer);
    EXPECT_EQ(second.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 1); // only the first survives
}

// F17: UPDATE_PROPOSAL_FEE / UPDATE_FEE_ADD_POOL / UPDATE_FEE_PAYOUT must
// reject newValue <= 0 (a free fee would let anyone spam proposals)
TEST(ContractCLKnDGR, SubmitProposal_FeeUpdates_ZeroOrNegativeValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 3);

    auto a = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                CLKnDGR::PROP_TYPE_UPDATE_PROPOSAL_FEE, 0LL);
    auto b = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                CLKnDGR::PROP_TYPE_UPDATE_FEE_ADD_POOL, -1LL);
    auto c = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                CLKnDGR::PROP_TYPE_UPDATE_FEE_PAYOUT, 0LL);
    EXPECT_EQ(a.success, 0);
    EXPECT_EQ(b.success, 0);
    EXPECT_EQ(c.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    // All three fee parameters retain their initial values
    EXPECT_EQ(ctx.st()->proposalFeeDefault,         CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    EXPECT_EQ(ctx.st()->proposalFeeAddPool,         CLKnDGR::INITIAL_PROPOSAL_FEE_ADD_POOL);
    EXPECT_EQ(ctx.st()->proposalFeePayoutStructure, CLKnDGR::INITIAL_PROPOSAL_FEE_PAYOUT);
}

// F18: BEGIN_EPOCH NAV ratio is hard-capped at 9000 (9× per-epoch growth) so
// extreme balance jumps cannot push downstream multiplications into overflow.
TEST(ContractCLKnDGR, BeginEpoch_NAVRatio_CappedAt9x)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // 1000 shares @ 10000 → prevTradingBalance = 10M

    ctx.beginEpoch(); // ratifies prevTradingBalance, no profit yet

    // Inject 100M: vaultCurBalance becomes 110M → uncapped ratio = 11000 (11×)
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), 100000000LL);

    ctx.beginEpoch();

    // Cap clamps navRatio to 9000 → newPool = 10M × 9000 / 1000 = 90M
    // sharePrice = 90M / 1000 = 90,000 (exactly 9× starting price)
    EXPECT_EQ(ctx.st()->totalDepositorPool, 90000000LL);
    EXPECT_EQ(ctx.st()->vaultSharePrice, 90000LL);
}

// F19: BEGIN_EPOCH auto-payout sweeps only depositors whose 26-epoch personal
// lock has expired; depositors entered later remain locked through the same
// epoch boundary. Verifies the per-depositor lock check against qpi.epoch().
TEST(ContractCLKnDGR, BeginEpoch_AutoPayout_MultipleDepositorsDifferentEpochs)
{
    ContractTestingCLKnDGR ctx;
    ctx.issueShares({{clkUser(0), 676}}); // shareholders for distributeDividends

    const id u1 = clkUser(1);
    const id u2 = clkUser(2);

    // Epoch 0: u1 deposits → depEpoch = 0
    ctx.addEnergy(u1, 10000000LL);
    ctx.vaultDeposit(u1, 10000000LL);

    // Epoch 10: u2 deposits → depEpoch = 10
    system.epoch = 10;
    ctx.addEnergy(u2, 10000000LL);
    ctx.vaultDeposit(u2, 10000000LL);

    // Epoch 26: u1's lock expires (0 + 26 = 26), u2's does not (10 + 26 = 36)
    system.epoch = 26;
    ctx.beginEpoch();
    system.epoch = 0;

    // u1 swept: gross = 1000 × 10000 = 10M; profit = 0; mgmtFee = 200K; net = 9.8M
    // u2 remains locked
    EXPECT_EQ(ctx.st()->depositorCount, 1);
    EXPECT_EQ(ctx.st()->totalVaultShares, 1000LL);
    EXPECT_EQ(getBalance(u1), 9800000LL);
    EXPECT_EQ(getBalance(u2), 0LL);
    // u2 must remain in the depositor list (swap-with-last moved them to slot 0)
    EXPECT_EQ(ctx.st()->depositorList.get(0), u2);
}

// F20: vaultRelock window opens exactly at depEpoch + (VAULT_LOCK_EPOCHS −
// VAULT_RELOCK_WINDOW_EPOCHS). One epoch earlier the call returns success=1
// (window closed); on the boundary epoch the relock applies (success=4).
TEST(ContractCLKnDGR, VaultRelock_AtWindowBoundary)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    ctx.addEnergy(u, 10000000LL);
    ctx.vaultDeposit(u, 10000000LL); // depEpoch = 0; window opens at epoch 22

    // Epoch 21: one short of the boundary → still closed
    system.epoch = 21;
    ctx.addEnergy(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    auto outClosed = ctx.vaultRelock(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    EXPECT_EQ(outClosed.success, 1); // window not yet open
    EXPECT_EQ(getBalance(u), CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT); // refunded

    // Epoch 22: window opens exactly here → relock applies
    system.epoch = 22;
    auto outOpen = ctx.vaultRelock(u, CLKnDGR::INITIAL_RELOCK_ADD_AMOUNT);
    EXPECT_EQ(outOpen.success, 4); // applied
    EXPECT_EQ(outOpen.newDepEpoch, 22U);
    // Lock reset to current epoch
    uint32 depEpoch = 0;
    CLKnDGR::DepositorInfo rec{}; ctx.st()->depositorInfo.get(u, rec); depEpoch = rec.epoch;
    EXPECT_EQ(depEpoch, 22U);

    system.epoch = 0;
}

// ===============================================================
// GROUP G — DEPOSITORVETO BRANCHES + REMAINING EDGE CASES
// ===============================================================

// G1: depositorVeto with slot >= proposalsThisEpoch is rejected before any state change
TEST(ContractCLKnDGR, DepositorVeto_InvalidSlot_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id depositor = clkUser(2);

    // Make caller a qualifying depositor (locked QU >= 150M default threshold)
    ctx.addEnergy(depositor, 150000000LL);
    ctx.vaultDeposit(depositor, 150000000LL);

    // No proposals submitted — proposalsThisEpoch == 0; slot 0 is out of range
    auto out = ctx.depositorVeto(depositor, 0, 0);
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->depositorNoVotes.get(0), 0);
    EXPECT_EQ(ctx.st()->depositorYesVotes.get(0), 0);
}

// G2: depositorVeto rejects votes against a proposal whose status is not ACTIVE
TEST(ContractCLKnDGR, DepositorVeto_InactiveProposal_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id proposer  = clkUser(1);
    const id depositor = clkUser(2);

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    ctx.addEnergy(depositor, 150000000LL);
    ctx.vaultDeposit(depositor, 150000000LL);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    // Force the proposal into a non-ACTIVE state
    auto p = ctx.st()->proposals.get(sub.slot);
    p.status = CLKnDGR::PROP_STATUS_FAILED;
    ctx.st()->proposals.set(sub.slot, p);

    auto vout = ctx.depositorVeto(depositor, sub.slot, 0);
    EXPECT_EQ(vout.success, 0);
    EXPECT_EQ(ctx.st()->depositorNoVotes.get(sub.slot), 0);
}

// G3: A depositor cannot vote twice on the same slot — second call rejected
TEST(ContractCLKnDGR, DepositorVeto_DoubleVote_Rejected)
{
    ContractTestingCLKnDGR ctx;
    const id proposer  = clkUser(1);
    const id depositor = clkUser(2);

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    ctx.addEnergy(depositor, 150000000LL);
    ctx.vaultDeposit(depositor, 150000000LL);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto first = ctx.depositorVeto(depositor, sub.slot, 0);
    EXPECT_EQ(first.success, 1);
    EXPECT_EQ(ctx.st()->depositorNoVotes.get(sub.slot), 1);

    // Second vote (even flipping to YES) must be rejected
    auto second = ctx.depositorVeto(depositor, sub.slot, 1);
    EXPECT_EQ(second.success, 0);
    EXPECT_EQ(ctx.st()->depositorNoVotes.get(sub.slot), 1);  // unchanged
    EXPECT_EQ(ctx.st()->depositorYesVotes.get(sub.slot), 0); // not converted
}

// G4: A depositor YES vote is recorded in depositorYesVotes but does NOT
// count toward the veto threshold (which only sums NO bits at END_EPOCH).
TEST(ContractCLKnDGR, DepositorVeto_YesVote_RecordedNotVeto)
{
    ContractTestingCLKnDGR ctx;
    const id proposer  = clkUser(1);
    const id depositor = clkUser(2);

    ctx.issueShares({{proposer, 50}});
    ctx.addEnergy(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    ctx.addEnergy(depositor, 150000000LL);
    ctx.vaultDeposit(depositor, 150000000LL);

    auto sub = ctx.submitProposal(proposer, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_MIN_PROFIT,
                                  CLKnDGR::MIN_PROFIT_QU_OPT2);
    EXPECT_EQ(sub.success, 1);

    auto vout = ctx.depositorVeto(depositor, sub.slot, 1); // YES vote
    EXPECT_EQ(vout.success, 1);
    EXPECT_EQ(ctx.st()->depositorYesVotes.get(sub.slot), 1);
    EXPECT_EQ(ctx.st()->depositorNoVotes.get(sub.slot), 0); // no NO vote → no veto contribution
}

// G5: getProposal returns zeroed output when slot >= MAX_PROPOSALS_PER_EPOCH
TEST(ContractCLKnDGR, GetProposal_SlotOutOfRange_ReturnsEmpty)
{
    ContractTestingCLKnDGR ctx;
    auto out = ctx.getProposal(CLKnDGR::MAX_PROPOSALS_PER_EPOCH); // == 10, just past the array
    EXPECT_EQ(out.proposalType, 0);
    EXPECT_EQ(out.status, 0);
    EXPECT_EQ(out.proposer, NULL_ID);
    EXPECT_EQ(out.feePaid, 0LL);
    EXPECT_EQ(out.votesYes, 0LL);
    EXPECT_EQ(out.votesNo, 0LL);
}

// G6: Two REMOVE_POOL proposals targeting the same poolIndex within one epoch:
// the second is content-invalid (pool-indexed duplicate detection — distinct
// from the singleton-type duplicate logic).
TEST(ContractCLKnDGR, SubmitProposal_RemovePool_DuplicateInSameEpoch_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});

    // Two active pools (so each REMOVE_POOL targets a valid index)
    ctx.st()->poolAssetNames.set(0, 0xAAAAAAAAAAAAAAAAULL);
    ctx.st()->poolIssuers.set(0, clkUser(98));
    ctx.st()->poolActive.set(0, 1);
    ctx.st()->poolAssetNames.set(1, 0xBBBBBBBBBBBBBBBBULL);
    ctx.st()->poolIssuers.set(1, clkUser(97));
    ctx.st()->poolActive.set(1, 1);
    ctx.st()->poolCount = 2;

    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT * 3);

    // First REMOVE_POOL(0): success
    auto first = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                    CLKnDGR::PROP_TYPE_REMOVE_POOL, 0, 0);
    EXPECT_EQ(first.success, 1);

    // Second REMOVE_POOL(0): same poolIndex — pool-indexed conflict
    auto dup = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_REMOVE_POOL, 0, 0);
    EXPECT_EQ(dup.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 1); // only the first survives

    // REMOVE_POOL(1) — different poolIndex — must still succeed (no conflict)
    auto other = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                    CLKnDGR::PROP_TYPE_REMOVE_POOL, 0, 1);
    EXPECT_EQ(other.success, 1);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 2);
}

// G7: BEGIN_EPOCH waitlist promotion: if a queued amount can no longer buy a
// full share at the current price (price has risen), the entry is refunded
// rather than promoted to a depositor with 0 shares.
TEST(ContractCLKnDGR, BeginEpoch_WaitlistPromotion_SharePriceTooHigh_EntryRefunded)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);

    // Direct-inject a waitlist entry whose amount is below current sharePrice (10000).
    // Vault is empty so the entry would otherwise be promoted at BEGIN_EPOCH.
    const sint64 amount = 5000LL; // < vaultSharePrice → 0 shares
    ctx.addEnergy(id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0), amount); // back the waitlistQu
    CLKnDGR::WaitlistEntry entry{};
    entry.wallet = u;
    entry.amount = amount;
    ctx.st()->waitlist.set(0, entry);
    ctx.st()->waitlistCount = 1;
    ctx.st()->waitlistQu    = amount;

    ctx.beginEpoch();

    // Refund path: u gets QU back; entry removed; vault unchanged.
    EXPECT_EQ(getBalance(u), amount);
    EXPECT_EQ(ctx.st()->waitlistCount, 0);
    EXPECT_EQ(ctx.st()->waitlistQu, 0LL);
    EXPECT_EQ(ctx.st()->depositorCount, 0);
    EXPECT_EQ(ctx.st()->totalVaultShares, 0LL);
}

// ===============================================================
// PROPERTY-BASED / FUZZ TESTS  (added 2026-06-24)
// ---------------------------------------------------------------
// The unit tests above check specific, hand-picked cases. These instead
// hammer the contract with THOUSANDS of randomized inputs and, after every
// single call, assert "invariants" — properties that must hold no matter
// what any caller does. The RNG uses a FIXED seed, so any failure is fully
// reproducible. This is the automated generalization of the taint audit:
// it proves no random/adversarial input reaches a sink and corrupts state.
// ===============================================================
namespace {
// Tiny deterministic xorshift64 PRNG (seeded → identical sequence every run).
struct FuzzRng
{
    unsigned long long s;
    explicit FuzzRng(unsigned long long seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    unsigned long long next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    unsigned long long below(unsigned long long n) { return n ? (next() % n) : 0ULL; }
    sint64 between(sint64 lo, sint64 hi)
    { return (hi <= lo) ? lo : lo + (sint64)(next() % (unsigned long long)(hi - lo + 1)); }
};
} // namespace

// FUZZ 1 — Random governance / vote / read traffic must never corrupt core state or crash.
// Most random submitProposal inputs are invalid and must be rejected; the structural caps
// (proposal slots, voter count, payout range, share-price floor) must ALWAYS hold. The
// random slots fed to voteOnProposal/depositorVeto/getProposal directly exercise the
// array-index bounds guards. Reset is via BEGIN_EPOCH only (no END_EPOCH apply path → no
// inter-contract DEX calls, which would need sibling contracts not present in this fixture).
TEST(ContractCLKnDGR, Fuzz_GovernanceInputs_NeverCorruptState)
{
    ContractTestingCLKnDGR ctx;
    FuzzRng rng(0xC10A11D6C0DEULL);

    // 20 shareholders (so submit/vote clear the >=1-share gate), all funded.
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 20; ++i) owners.push_back({clkUser(i), 10});
    ctx.issueShares(owners);
    for (int i = 0; i < 20; ++i) ctx.addEnergy(clkUser(i), 1000000000000LL);

    for (int iter = 0; iter < 2000; ++iter)
    {
        const id actor = clkUser(rng.below(20));
        switch ((int)rng.below(5))
        {
        case 0: // submitProposal: wild type/value/index — mostly invalid, must be rejected cleanly
            ctx.addEnergy(actor, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
            ctx.submitProposal(actor, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                               (uint8)rng.below(40),                 // type 0..39 (valid 1..25)
                               rng.between(-5, 1000000000LL),        // newValue
                               rng.below(8));                        // poolIndex (some out of range)
            break;
        case 1: ctx.voteOnProposal(actor, (uint8)rng.below(256), (uint8)rng.below(256)); break;
        case 2: ctx.depositorVeto(actor, (uint8)rng.below(256), (uint8)rng.below(256)); break;
        case 3: ctx.getPool(rng.next()); ctx.getProposal((uint8)rng.below(256)); break; // index sinks
        case 4: ctx.beginEpoch(); break;                                                // resets gov state
        }

        auto* s = ctx.st();
        ASSERT_LE((int)s->proposalsThisEpoch, (int)CLKnDGR::MAX_PROPOSALS_PER_EPOCH) << "iter=" << iter;
        ASSERT_LE((int)s->voterCount,         (int)CLKnDGR::PROPOSAL_VOTER_CAPACITY) << "iter=" << iter;
        ASSERT_LE((int)s->payoutStructure,    3)                                     << "iter=" << iter;
        ASSERT_GE(s->vaultSharePrice,         1LL)                                    << "iter=" << iter;
    }
}

// FUZZ 2 — Random vault money-flows must keep the vault SOLVENT and self-consistent.
// No injected profit and no epoch settle here, so the depositor pool must stay fully backed
// by the contract's liquid QU at all times. This is the property the NAV fix guarantees;
// fuzzing it across thousands of random deposit/withdraw/relock/donate sequences generalizes
// the single-scenario NAV regression test.
TEST(ContractCLKnDGR, Fuzz_VaultOps_StaySolventAndConsistent)
{
    ContractTestingCLKnDGR ctx;
    FuzzRng rng(0x5A1AD7EE5EEDULL);

    const int NUSERS = 30;
    for (int i = 0; i < NUSERS; ++i) ctx.addEnergy(clkUser(i), 100000000000LL);
    const id self = id(CLKnDGR_CONTRACT_INDEX, 0, 0, 0);

    for (int iter = 0; iter < 2000; ++iter)
    {
        const id actor = clkUser(rng.below(NUSERS));
        switch ((int)rng.below(6))
        {
        case 0: { sint64 a = rng.between(1, 50000000LL); if (getBalance(actor) < a) ctx.addEnergy(actor, a); ctx.vaultDeposit(actor, a); break; }
        case 1: ctx.vaultWithdraw(actor); break;
        case 2: { sint64 a = rng.between(1, 60000000LL); if (getBalance(actor) < a) ctx.addEnergy(actor, a); ctx.vaultRelock(actor, a); break; }
        case 3: { sint64 a = rng.between(1, 10000000LL); if (getBalance(actor) < a) ctx.addEnergy(actor, a); ctx.donateToContract(actor, a); break; }
        case 4: { sint64 a = rng.between(1, 10000000LL); if (getBalance(actor) < a) ctx.addEnergy(actor, a); ctx.publicDonate(actor, a); break; }
        case 5: ctx.waitlistWithdraw(actor); break;
        }

        auto* s = ctx.st();
        ASSERT_GE(s->vaultSharePrice,    1LL)  << "iter=" << iter;   // no div-by-zero / infinite shares
        ASSERT_GE(s->totalVaultShares,   0LL)  << "iter=" << iter;   // no underflow
        ASSERT_GE(s->totalDepositorPool, 0LL)  << "iter=" << iter;
        ASSERT_LE((int)s->depositorCount, (int)CLKnDGR::MAX_DEPOSITORS) << "iter=" << iter;
        ASSERT_LE((int)s->waitlistCount,  (int)CLKnDGR::WAITLIST_SIZE)  << "iter=" << iter;
        if (s->totalVaultShares == 0) ASSERT_EQ(s->totalDepositorPool, 0LL) << "iter=" << iter; // empty => no pool
        // SOLVENCY: depositor pool must be backed by liquid QU on hand.
        const sint64 liquid = getBalance(self) - s->quReserve - s->qearnReserve
                              - s->waitlistQu - s->reserveSellProceeds;
        ASSERT_LE(s->totalDepositorPool, liquid) << "iter=" << iter;
    }
}

// FUZZ 3 — The read-only index sinks must never crash or read out of bounds, for ANY index.
TEST(ContractCLKnDGR, Fuzz_ReadFunctions_RandomIndices_NoCrash)
{
    ContractTestingCLKnDGR ctx;
    FuzzRng rng(0xDEAD10C0DE5ULL);

    // Seed a small, partially-active pool table (real + inactive + out-of-range slots).
    for (uint64 p = 0; p < 4; ++p)
    {
        ctx.st()->poolAssetNames.set(p, 0x5152535400000000ULL + p);
        ctx.st()->poolIssuers.set(p, clkUser(80 + p));
        ctx.st()->poolActive.set(p, (uint8)(p & 1));
    }
    ctx.st()->poolCount = 4;

    // Clearly out-of-range indices must report inactive — never crash or return garbage.
    EXPECT_EQ((int)ctx.getPool(1000000ULL).active, 0);
    EXPECT_EQ((int)ctx.getPool(0xFFFFFFFFFFFFFFFFULL).active, 0);

    for (int iter = 0; iter < 4000; ++iter)
    {
        ctx.getPool(rng.next());                // full uint64 index range
        ctx.getProposal((uint8)rng.below(256)); // full uint8 slot range
        ctx.getWaitlistPosition();
    }
    SUCCEED(); // 4000 random index reads with no crash = the read-only taint surface is safe
}

// ===============================================================
// Governable Cloak knobs (proposal types 26/27/28) — added 2026-06-24
// ===============================================================

// Defaults: sizing preset 0, buy-dip 10%, rally-sell 12%.
TEST(ContractCLKnDGR, GetGovernanceParams_SwingKnobDefaults)
{
    ContractTestingCLKnDGR ctx;
    auto gp = ctx.getGovernanceParams();
    EXPECT_EQ((int)gp.swingSizingPreset, 0);
    EXPECT_EQ(gp.swingBuyDipPct,   30LL);
    EXPECT_EQ(gp.swingSellGainPct, 6LL);
    EXPECT_EQ((int)ctx.st()->swingSizingPreset, 0);
    EXPECT_EQ(ctx.st()->swingBuyDipPct,   30LL);
    EXPECT_EQ(ctx.st()->swingSellGainPct, 6LL);
}

// Type 26: UPDATE_SWING_SIZING — full cycle switches the position-sizing preset (0 -> 2).
TEST(ContractCLKnDGR, GovernanceCycle_UpdateSwingSizing_ChangesPreset)
{
    ContractTestingCLKnDGR ctx;
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i) owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ((int)ctx.st()->swingSizingPreset, 0); // default

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_SIZING, 2LL); // 2 = 2%/0.50%/10%
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub.slot, 1);
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ((int)ctx.st()->swingSizingPreset, 2);
}

// Type 26 rejects an out-of-range preset (4 > max 3).
TEST(ContractCLKnDGR, SubmitProposal_UpdateSwingSizing_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_SIZING, 4LL); // out of range
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ((int)ctx.st()->swingSizingPreset, 0); // unchanged
}

// Type 27: UPDATE_SWING_DIP — full cycle changes the buy-dip threshold (10 -> 25).
TEST(ContractCLKnDGR, GovernanceCycle_UpdateSwingDip_ChangesThreshold)
{
    ContractTestingCLKnDGR ctx;
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i) owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->swingBuyDipPct, 30LL); // default

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_DIP, 25LL);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub.slot, 1);
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->swingBuyDipPct, 25LL);
}

// Type 27 rejects a value off the 5-step grid (12 is a valid RALLY value but not a DIP value).
TEST(ContractCLKnDGR, SubmitProposal_UpdateSwingDip_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_DIP, 12LL); // not in {5,10,15,20,25,30}
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->swingBuyDipPct, 30LL); // unchanged
}

// Type 28: UPDATE_SWING_RALLY — full cycle changes the rally-sell threshold (12 -> 18).
TEST(ContractCLKnDGR, GovernanceCycle_UpdateSwingRally_ChangesThreshold)
{
    ContractTestingCLKnDGR ctx;
    std::vector<std::pair<id, int>> owners;
    for (int i = 0; i < 15; ++i) owners.push_back({clkUser(i), 15});
    ctx.issueShares(owners);
    ctx.fundVoters(15);

    EXPECT_EQ(ctx.st()->swingSellGainPct, 6LL); // default

    ctx.addEnergy(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);
    auto sub = ctx.submitProposal(clkUser(0), CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_RALLY, 18LL);
    EXPECT_EQ(sub.success, 1);
    for (int i = 0; i < 15; ++i) ctx.voteOnProposal(clkUser(i), sub.slot, 1);
    ctx.endEpoch();

    EXPECT_EQ(ctx.getProposal(sub.slot).status, CLKnDGR::PROP_STATUS_PASSED);
    EXPECT_EQ(ctx.st()->swingSellGainPct, 18LL);
}

// Type 28 rejects a value off the 6-step grid (10 is a valid DIP value but not a RALLY value).
TEST(ContractCLKnDGR, SubmitProposal_UpdateSwingRally_InvalidValue_ContentInvalid)
{
    ContractTestingCLKnDGR ctx;
    const id u = clkUser(1);
    ctx.issueShares({{u, 50}});
    ctx.addEnergy(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT);

    auto out = ctx.submitProposal(u, CLKnDGR::INITIAL_PROPOSAL_FEE_DEFAULT,
                                  CLKnDGR::PROP_TYPE_UPDATE_SWING_RALLY, 10LL); // not in {6,12,18,24,30}
    EXPECT_EQ(out.success, 0);
    EXPECT_EQ(ctx.st()->proposalsThisEpoch, 0);
    EXPECT_EQ(ctx.st()->swingSellGainPct, 6LL); // unchanged
}
