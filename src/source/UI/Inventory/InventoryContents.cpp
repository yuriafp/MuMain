#include "stdafx.h"

#include "UI/Inventory/InventoryContents.h"

#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/InventoryUtils.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"

#include <optional>

namespace UI::Inventory
{
void ClearAllItems()
{
    g_pMyInventory->UnequipAllItems();
    g_pMyInventory->DeleteAllItems();
    g_pMyInventoryExt->DeleteAllItems();
    g_pMyShopInventory->DeleteAllItems();
}

void RemoveItem(int index)
{
    if (index >= 0 && index < MAX_EQUIPMENT_INDEX)
        g_pMyInventory->UnequipItem(index);
    else if (IsMainInventorySlot(index))
        g_pMyInventory->DeleteItem(index);
    else if (IsInventoryExtensionSlot(index))
        g_pMyInventoryExt->DeleteItem(index);
    else if (IsMyShopSlot(index))
        g_pMyShopInventory->DeleteItem(index);
}

void RemoveDroppedItem(int index)
{
    if (index >= 0 && index < MAX_EQUIPMENT_INDEX)
        g_pMyInventory->UnequipItem(index);
    else
        g_pMyInventory->DeleteItem(index);
}

bool InsertItem(int index, std::span<const std::uint8_t> itemData)
{
    if (index >= 0 && index < MAX_EQUIPMENT_INDEX)
        return g_pMyInventory->EquipItem(index, itemData);
    if (IsMainInventorySlot(index))
        return g_pMyInventory->InsertItem(index, itemData);
    if (IsInventoryExtensionSlot(index))
        return g_pMyInventoryExt->InsertItem(index, itemData);
    if (IsMyShopSlot(index))
        return g_pMyShopInventory->InsertItem(index, itemData);
    return false;
}

void DiscardPickedItem()
{
    mu::ui::window::CInventoryCtrl::DeletePickedItem();
}

void RestorePickedItem()
{
    mu::ui::window::CInventoryCtrl::BackupPickedItem();
}

int TransferSourceIndex()
{
    if (auto* pickedItem = mu::ui::window::CInventoryCtrl::GetPickedItem())
        return pickedItem->GetSourceLinealPos();

    return g_pMyShopInventory->GetSourceIndex();
}

namespace
{
// What the client knows of a rented item. The server's item data carries no expiry (C1 D2 12
// announces it), so an item the server moves would lose it without this.
struct RentalPeriod
{
    short type = -1;
    bool expired = false;
    long expireTime = 0;
};

std::optional<RentalPeriod> PickedRentalPeriod()
{
    const auto* pickedItem = mu::ui::window::CInventoryCtrl::GetPickedItem();
    const ITEM* item = pickedItem != nullptr ? pickedItem->GetItem() : nullptr;
    if (item == nullptr || !item->bPeriodItem)
        return std::nullopt;

    return RentalPeriod{item->Type, item->bExpiredPeriod, item->lExpireTime};
}

// Only onto the same item: a slot the move did not fill keeps what it holds.
void KeepRentalPeriod(int index, const RentalPeriod& period)
{
    ITEM* placed = FindEquippedOrPlayerItem(index);
    if (placed == nullptr || placed->Type != period.type)
        return;

    placed->bPeriodItem = true;
    placed->bExpiredPeriod = period.expired;
    placed->lExpireTime = period.expireTime;
}

bool PlaceTransferredItem(int index, std::span<const std::uint8_t> itemData)
{
    if (index >= 0 && index < MAX_EQUIPMENT_INDEX)
    {
        g_pMyInventory->EquipItem(index, itemData);
        return true;
    }

    if (IsPlayerInventorySlot(index))
    {
        g_pStorageInventory->ProcessStorageItemAutoMoveSuccess();
        g_pStorageInventoryExt->ProcessStorageItemAutoMoveSuccess();
        return InsertItem(index, itemData);
    }

    if (IsMyShopSlot(index))
        return g_pMyShopInventory->InsertItem(index, itemData);

    return true;
}
} // namespace

bool ReceivePlayerTransfer(int index, std::span<const std::uint8_t> itemData)
{
    const std::optional<RentalPeriod> rentalPeriod = PickedRentalPeriod();
    DiscardPickedItem();

    const bool placed = PlaceTransferredItem(index, itemData);
    if (rentalPeriod)
        KeepRentalPeriod(index, *rentalPeriod);

    return placed;
}

void RejectTransfer()
{
    RestorePickedItem();
    if (g_pStorageInventory->IsItemAutoMove())
        g_pStorageInventory->ProcessStorageItemAutoMoveFailure();
    if (g_pStorageInventoryExt->IsItemAutoMove())
        g_pStorageInventoryExt->ProcessStorageItemAutoMoveFailure();
}

bool HasPickedItem()
{
    return mu::ui::window::CInventoryCtrl::GetPickedItem() != nullptr;
}

ITEM* FindPlayerItem(int index)
{
    if (!IsPlayerInventorySlot(index))
        return nullptr;

    if (IsMainInventorySlot(index))
        return g_pMyInventory != nullptr ? g_pMyInventory->FindItem(index) : nullptr;

    return g_pMyInventoryExt != nullptr ? g_pMyInventoryExt->FindItem(index) : nullptr;
}

ITEM* FindEquippedOrPlayerItem(int index)
{
    if (index >= 0 && index < MAX_EQUIPMENT_INDEX)
        return &CharacterMachine->Equipment[index];

    return FindPlayerItem(index);
}

ITEM* FindMainInventoryItem(int index)
{
    return g_pMyInventory->FindItem(index);
}

void DeleteMainInventoryItem(int index)
{
    g_pMyInventory->DeleteItem(index);
}

ITEM* StandbyItem()
{
    return g_pMyInventory->GetStandbyItem();
}

int StandbyItemIndex()
{
    return g_pMyInventory->GetStandbyItemIndex();
}
} // namespace UI::Inventory
