#pragma once

#include "GameLogic/Items/WorkshopOptions.h"

#include <cstddef>

typedef struct tagITEM ITEM;
namespace mu::ui::window { class CInventoryCtrl; }

// The mix window as the Oficina's (GameLogic/Items/WorkshopOptions.h): this server has no chaos card
// NPC, so the chaos card window is the Oficina's.
namespace UI::Mix::Workshop
{
bool IsOpen();

// The Oficina's options for the weapons, armors and shields in the mix box.
GameLogic::Items::Workshop::Options GetOptions(mu::ui::window::CInventoryCtrl& box);

// The only weapon, armor or shield in the mix box, or nullptr.
const ITEM* FindItem(mu::ui::window::CInventoryCtrl& box);

// The name of the choice for the only weapon, armor or shield in the mix box, or nullptr.
const wchar_t* GetOptionName(mu::ui::window::CInventoryCtrl& box, int choice);

// The line telling the player what to do before the recipe lines can; false when they tell it.
bool FormatStatus(const GameLogic::Items::Workshop::Options& options, bool optionChosen, wchar_t* text, size_t size);
}
