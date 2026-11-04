#pragma once

#include <array>
#include <cstdint>
#include <vector>

// The jewels which Ella, the Banqueira of our server, keeps for an account (the "TBT: Banqueira"
// plugin, MUnique.OpenMU.Tbt.JewelBank). Talking to her, the server sends them with her fees
// (Network/Server/JewelBankPacket.h) and the client opens her window; each request of the window
// (C1 FB 03) is answered with the jewels again.
namespace GameLogic::Items
{
// What the window asks her, by the server's numbers (MUnique.OpenMU.Tbt.JewelBank.JewelBankAction).
enum class JewelBankAction : std::uint8_t
{
    // Keep the jewels and packs of one kind which are in the inventory.
    Deposit = 1,
    // Keep those of all kinds.
    DepositAll = 2,
    // Take out one jewel or a pack of them.
    Withdraw = 3,
};

// What one withdrawal takes out: a jewel, a pack of 10 (+0) or a pack of 30 (+2).
constexpr std::array<int, 3> JewelBankWithdrawalAmounts{1, 10, 30};

struct JewelBankJewel
{
    // The item of the single jewel.
    int group = 0;
    int number = 0;
    // What the account keeps of it, a pack counted as its jewels.
    int amount = 0;

    bool operator==(const JewelBankJewel&) const = default;
};

struct JewelBankContents
{
    // The fees in zen for each jewel kept or taken out; a pack costs as many times as it has jewels.
    int depositFeePerJewel = 0;
    int withdrawalFeePerJewel = 0;
    // In the server's order: a request names a jewel by its index here.
    std::vector<JewelBankJewel> jewels;

    bool operator==(const JewelBankContents&) const = default;
};

// Whether the account keeps enough of the jewel at `index` for a withdrawal of `amount`.
bool CanWithdraw(const JewelBankContents& contents, int index, int amount);

// The jewels as the server last sent them, and how many times it sent them since the client
// started, so a window sees an answer to its request even when nothing changed.
const JewelBankContents& CurrentJewelBank();
unsigned CurrentJewelBankVersion();
void SetCurrentJewelBank(JewelBankContents contents);
} // namespace GameLogic::Items
