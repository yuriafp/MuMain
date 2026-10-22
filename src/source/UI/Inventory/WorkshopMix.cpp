#include "stdafx.h"

#include "UI/Inventory/WorkshopMix.h"

#include "GameLogic/Items/MixMgr.h"
#include "I18N/All.h"
#include "Network/Server/SocketSystem.h"
#include "UI/Inventory/InventoryCtrl.h"

#include <array>

namespace UI::Mix::Workshop
{
using namespace GameLogic::Items::Workshop;

namespace
{
constexpr int MaximumItemsInBox = MAX_MIX_INVENTORY;

// The weapons, armors and shields; arrows and bolts take no options.
bool IsEquipment(const ITEM& item)
{
    return item.Type >= ITEM_SWORD && item.Type < ITEM_WING && item.Type != ITEM_BOLT && item.Type != ITEM_ARROWS;
}

// The excellent options by number (1 to 6), as the item tooltip names them per kind of item.
constexpr std::array<const wchar_t* const*, ExcellentOptionCount> WeaponOptions{
    &I18N::Game::WorkshopManaAfterKill, &I18N::Game::WorkshopLifeAfterKill, &I18N::Game::WorkshopAttackSpeed,
    &I18N::Game::WorkshopDamagePercent, &I18N::Game::WorkshopDamageLevel, &I18N::Game::WorkshopExcellentDamage};
constexpr std::array<const wchar_t* const*, ExcellentOptionCount> StaffOptions{
    &I18N::Game::WorkshopManaAfterKill, &I18N::Game::WorkshopLifeAfterKill, &I18N::Game::WorkshopAttackSpeed,
    &I18N::Game::WorkshopWizardryPercent, &I18N::Game::WorkshopWizardryLevel, &I18N::Game::WorkshopExcellentDamage};
constexpr std::array<const wchar_t* const*, ExcellentOptionCount> DefenseOptions{
    &I18N::Game::WorkshopZenAfterKill, &I18N::Game::WorkshopDefenseRate, &I18N::Game::WorkshopReflectDamage,
    &I18N::Game::WorkshopDamageDecrease, &I18N::Game::WorkshopMaximumMana, &I18N::Game::WorkshopMaximumLife};

const std::array<const wchar_t* const*, ExcellentOptionCount>& GetExcellentOptions(const ITEM& item)
{
    if (item.Type < ITEM_STAFF)
        return WeaponOptions;
    if (item.Type < ITEM_SHIELD)
        return StaffOptions;
    return DefenseOptions;
}
} // namespace

bool IsOpen()
{
    return g_MixRecipeMgr.GetMixInventoryType() == SEASON3A::MIXTYPE_CHAOS_CARD;
}

Options GetOptions(mu::ui::window::CInventoryCtrl& box)
{
    std::array<Item, MaximumItemsInBox> items{};
    size_t count = 0;
    for (int i = 0; i < static_cast<int>(box.GetNumberOfItems()) && count < items.size(); ++i)
    {
        ITEM* item = box.GetItem(i);
        if (!IsEquipment(*item))
            continue;
        items[count++] = {item->Level, item->ExcellentFlags, item->HasLuck, item->AncientDiscriminator > 0,
                          g_SocketItemMgr.IsSocketItem(item) == TRUE || item->SocketCount > 0};
    }
    return Options({items.data(), count});
}

const ITEM* FindItem(mu::ui::window::CInventoryCtrl& box)
{
    const ITEM* found = nullptr;
    for (int i = 0; i < static_cast<int>(box.GetNumberOfItems()); ++i)
    {
        const ITEM* item = box.GetItem(i);
        if (!IsEquipment(*item))
            continue;
        if (found != nullptr)
            return nullptr;
        found = item;
    }
    return found;
}

const wchar_t* GetOptionName(mu::ui::window::CInventoryCtrl& box, int choice)
{
    const ITEM* item = FindItem(box);
    if (item == nullptr || choice < LuckChoice || choice > ExcellentOptionCount)
        return nullptr;
    if (choice == LuckChoice)
        return I18N::Game::WorkshopLuck;
    return *GetExcellentOptions(*item)[choice - 1];
}

bool FormatStatus(const Options& options, bool optionChosen, wchar_t* text, size_t size)
{
    switch (options.GetStatus())
    {
    case Status::NoItem:
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopPlaceItem);
        return true;
    case Status::TooManyItems:
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopOnlyOneItem);
        return true;
    case Status::Ancient:
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopNoAncient);
        return true;
    case Status::Socket:
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopNoSocket);
        return true;
    case Status::LevelTooHigh:
        mu_swprintf_s(text, size, I18N::Game::WorkshopMaximumLevel, MaximumItemLevel);
        return true;
    case Status::NoOptionLeft:
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopNoOptionLeft);
        return true;
    case Status::Ready:
        if (optionChosen)
            return false;
        mu_swprintf_s(text, size, L"%ls", I18N::Game::WorkshopChooseInList);
        return true;
    }
    return false;
}
}
