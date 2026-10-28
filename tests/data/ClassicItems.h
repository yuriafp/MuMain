#pragma once

// The classic 0.97d items of our server, which upstream's data does not have: new item
// numbers that reuse the models of Season 6 items. The tests that count upstream's shipped
// items add these.
namespace ClassicItems
{
// The maces 2/30-35 (stat profile weapon).
constexpr int Maces = 6;
// The armour parts 7/100-104 and 8-11/100-106 (stat profile armor).
constexpr int ArmourParts = 33;
constexpr int Items = Maces + ArmourParts;
// Each shares the model of its Season 6 item, all but the Mace of the King (Mace14, its own).
constexpr int SharedModels = Items - 1;
// The sets that copy metal sets and declare specular like them: 100-103, the copies of the
// Dark Lord sets 25-28 (five parts each), and 105 and 106, the copies of the Magic Gladiator
// sets 20 and 23 (no helm).
constexpr int MetalArmourSets[] = {100, 101, 102, 103, 105, 106};
constexpr int MetalArmourParts = 4 * 5 + 2 * 4;
} // namespace ClassicItems
