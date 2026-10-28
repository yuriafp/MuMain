#pragma once

#include "Core/Globals/_enum.h"

class CHARACTER;
class OBJECT;

// The monsters of our server, outside the original numbers (1000 on): the
// Alquimista of the Oficina, the treasure chests and the bosses of the boss
// invasion. Their models are only in the local client (Mu Kronus, the Season 8
// client), never in the public fork. Also the Season 6 bosses of the invasion
// on maps other than their own, whose code is the only one which creates them.
namespace Engine::Object::ServerMonsters
{
// Creates the character of one of these monsters; nullptr for any other number.
CHARACTER* Create(EMonsterType type, int positionX, int positionY, int key);

// The monsters of Icarus 2 (1010 to 1015) look like a monster of the original game, only bigger: the
// base monster whose model, weapons and attack effects they use, and how much bigger they are.
// False, with nothing set, for any other number.
bool GetVariant(EMonsterType type, EMonsterType& base, float& scaleFactor);

// Gives the character just created for the base monster the name and the size of its variant.
void ApplyVariant(CHARACTER& character, EMonsterType type, float scaleFactor);

// Makes a boss of the boss invasion bigger than the scale of its model,
// wherever it appears, also in its own event: the server gives the bosses many
// times their health. Any other character stays as it is.
void ScaleInvasionBoss(CHARACTER& character);

// Plays the attack of one of our invasion bosses with skills: the animation of
// its attacks entry (Monsters.json) and the effect of the monster skill the
// server sent (packet 0x69), e.g. the ice at the hit player. False, with
// nothing done, for any other monster.
bool SetAttackAction(CHARACTER& character, OBJECT& object);
} // namespace Engine::Object::ServerMonsters
