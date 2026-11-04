#include "GameLogic/Items/JewelBank.h"

#include <utility>

namespace GameLogic::Items
{
namespace
{
JewelBankContents s_CurrentJewelBank;
unsigned s_CurrentJewelBankVersion = 0;
} // namespace

bool CanWithdraw(const JewelBankContents& contents, int index, int amount)
{
    if (index < 0 || index >= static_cast<int>(contents.jewels.size()) || amount <= 0)
        return false;

    return contents.jewels[static_cast<std::size_t>(index)].amount >= amount;
}

const JewelBankContents& CurrentJewelBank()
{
    return s_CurrentJewelBank;
}

unsigned CurrentJewelBankVersion()
{
    return s_CurrentJewelBankVersion;
}

void SetCurrentJewelBank(JewelBankContents contents)
{
    s_CurrentJewelBank = std::move(contents);
    ++s_CurrentJewelBankVersion;
}
} // namespace GameLogic::Items
