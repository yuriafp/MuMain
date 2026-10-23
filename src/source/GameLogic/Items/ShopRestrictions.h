#pragma once

typedef struct tagITEM ITEM;

namespace GameLogic::Items
{
    bool IsPersonalShopBan(const ITEM* pItem);
    bool IsSellingBan(const ITEM* pItem);
    bool IsRepairBan(const ITEM* pItem);
    // The sale to an NPC asks the player first: an expensive item, or a rented one while its
    // rental time runs.
    bool AsksBeforeSellingToNpc(const ITEM* pItem);
}
