#include "stdafx.h"

#include "UI/Inventory/WorkshopMix.h"

#include "Data/GameData/ItemData/ItemDatabase.h"
#include "GameLogic/Items/MixMgr.h"
#include "I18N/All.h"
#include "Network/Server/SocketSystem.h"
#include "UI/Inventory/InventoryCtrl.h"

#include <array>
#include <optional>

namespace UI::Mix::Workshop
{
using namespace GameLogic::Items::Workshop;
namespace ItemData = Data::Items;

namespace
{
constexpr int MaximumItemsInBox = MAX_MIX_INVENTORY;

// The weapons, armors and shields; arrows and bolts take no options.
bool IsEquipment(const ITEM& item)
{
    return item.Type >= ITEM_SWORD && item.Type < ITEM_WING && item.Type != ITEM_BOLT && item.Type != ITEM_ARROWS;
}

// The tier of a wing or cape in the item data, or nothing for the other items and the small wings.
std::optional<WingTier> GetWingTier(const ITEM& item)
{
    switch (g_ItemDatabase.GetWingTier(item.Type))
    {
    case ItemData::WingTier::First:
        return WingTier::First;
    case ItemData::WingTier::Second:
        return WingTier::Second;
    case ItemData::WingTier::Third:
        return WingTier::Third;
    default:
        return std::nullopt;
    }
}

bool IsWorkshopItem(const ITEM& item)
{
    return IsEquipment(item) || GetWingTier(item).has_value();
}

// The item as the Oficina's rules see it, with the wing options of its tier for a wing.
Item ToWorkshopItem(const ITEM& item)
{
    Item result{item.Level, item.ExcellentFlags, item.HasLuck, item.AncientDiscriminator > 0,
                g_SocketItemMgr.IsSocketItem(&item) == TRUE || item.SocketCount > 0};
    if (const std::optional<WingTier> tier = GetWingTier(item))
        SetWing(result, *tier, item.Type == ITEM_CAPE_OF_LORD);
    return result;
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

// The wing options by number, as the item tooltip names them per tier (CalcWingOptions); only the
// Cape of Lord has the fourth of the second tier.
constexpr std::array<const wchar_t* const*, CapeOfLordOptionCount> SecondTierWingOptions{
    &I18N::Game::WorkshopWingLife, &I18N::Game::WorkshopWingMana, &I18N::Game::WorkshopWingIgnoreDefense3,
    &I18N::Game::WorkshopWingCommand};
constexpr std::array<const wchar_t* const*, ThirdTierWingOptionCount> ThirdTierWingOptions{
    &I18N::Game::WorkshopWingIgnoreDefense5, &I18N::Game::WorkshopWingReflect, &I18N::Game::WorkshopWingRecoverLife,
    &I18N::Game::WorkshopWingRecoverMana};

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
        const ITEM* item = box.GetItem(i);
        if (IsWorkshopItem(*item))
            items[count++] = ToWorkshopItem(*item);
    }
    return Options({items.data(), count});
}

const ITEM* FindItem(mu::ui::window::CInventoryCtrl& box)
{
    const ITEM* found = nullptr;
    for (int i = 0; i < static_cast<int>(box.GetNumberOfItems()); ++i)
    {
        const ITEM* item = box.GetItem(i);
        if (!IsWorkshopItem(*item))
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
    if (item == nullptr || choice < LuckChoice)
        return nullptr;
    if (choice == LuckChoice)
        return I18N::Game::WorkshopLuck;

    const Item workshopItem = ToWorkshopItem(*item);
    if (choice > workshopItem.optionCount)
        return nullptr;
    switch (workshopItem.kind)
    {
    case Kind::Equipment:
        return *GetExcellentOptions(*item)[choice - 1];
    case Kind::Wing:
        return *SecondTierWingOptions[choice - 1];
    case Kind::ThirdTierWing:
        return *ThirdTierWingOptions[choice - 1];
    }
    return nullptr;
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
