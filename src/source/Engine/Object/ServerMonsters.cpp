#include "stdafx.h"

#include "ServerMonsters.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzOpenData.h"
#include "GameLogic/Monsters/MonsterAttackPlay.h"
#include "GameLogic/Monsters/MonsterNames.h"
#include "Render/Effects/ZzzEffect.h"

namespace Engine::Object::ServerMonsters
{
namespace
{
// The Mu Kronus treasure chest is about 2.6 tiles wide; these scales give it about the size of the
// reward boxes of the Doppelganger, the golden one bigger.
constexpr float TreasureChestScale = 0.5f;
constexpr float GoldenTreasureChestScale = 0.7f;
constexpr float TreasureChestEdgeScale = 1.1f;

constexpr short NoWeapon = -1;

// How much bigger the bosses of the boss invasion are than the scale of their models; the choice
// of the user in 2026-10-27, with 20 times their health on the server.
constexpr float InvasionBossSizeFactor = 1.5f;

// The monster skills of our invasion bosses with skills, the numbers the server sends for them
// (OpenMU, Tbt/BossInvasion/InvasionBossIntelligence.cs).
constexpr int EscortCallSkill = ATMON_SKILL_SUMMON;
constexpr int ShockSkill = ATMON_SKILL_SHOCK;
constexpr int PoisonSkill = ATMON_SKILL_SERUFAN_POISONGEM;
constexpr int IceSkill = ATMON_SKILL_SERUFAN_COOLSTORM;
constexpr int FurySkill = ATMON_SKILL_EMPIREGUARDIAN_BERSERKER;

// The effects of the skills are the ones of the game which they are like: the shock of Balgass'
// attack, the Poison and Ice skills, the summon of the Nightmare and the circles of Selupan's heal.
constexpr int ShockWaveCount = 5;
constexpr int ShockStoneCount = 4;
constexpr int ShockWaveSubType = 8;
constexpr int ShockCraterSubType = 2;
constexpr int ShockBigStoneSubType = 10;
constexpr int PoisonSmokeCount = 10;
constexpr int PoisonSmokeSubType = 1;
constexpr int IcePieceCount = 5;
constexpr int SummonSubType = 3;
constexpr int FuryCircleCount = 2;
constexpr int FuryCircleSubType = 13;
constexpr float FuryCircleScale = 8.f;

// The light colours of the effects, red, green and blue.
struct EffectLight
{
    float red;
    float green;
    float blue;
};

constexpr EffectLight ShockLight{1.0f, 0.6f, 0.4f};      // Balgass' shock waves.
constexpr EffectLight PoisonLight{0.4f, 0.6f, 1.f};      // The smoke of the Poison skill.
constexpr EffectLight IceLight{1.f, 1.f, 1.f};           // The ice of the Ice skill.
constexpr EffectLight EscortCallLight{0.3f, 0.2f, 0.1f}; // Selupan's summon.
constexpr EffectLight FuryLight{1.f, 0.2f, 0.1f};        // Red, for the rage.

struct InvasionBoss
{
    EMonsterType type;
    EMonsterModelType modelType;
    float scale;
    short secondWeapon; // Weapon[1], e.g. the staff of the Illusion of Kundun.
    bool hasSkills;     // Its skills come from the server (SetAttackAction).
};

constexpr InvasionBoss InvasionBosses[] = {
    // The bosses of the boss invasion from the Season 8 client. Their models measure 240 to 560
    // units; these scales make them about 450 to 560, like the bosses of Season 6 (Erohim 450,
    // Raymond 510). They have the skills of the invasion.
    {MONSTER_LORD_SILVESTER, MONSTER_MODEL_LORD_SILVESTER, 1.0f, NoWeapon, true},
    {MONSTER_SELLIHODEN, MONSTER_MODEL_SELLIHODEN, 1.4f, NoWeapon, true},
    {MONSTER_UKANVA, MONSTER_MODEL_UKANVA, 1.2f, NoWeapon, true},
    {MONSTER_SILLA, MONSTER_MODEL_SILLA, 1.9f, NoWeapon, true},
    {MONSTER_NORMUS, MONSTER_MODEL_NORMUS, 0.9f, NoWeapon, true},
    {MONSTER_MUFF, MONSTER_MODEL_MUFF, 1.4f, NoWeapon, true},
    {MONSTER_REAPER, MONSTER_MODEL_REAPER, 1.5f, NoWeapon, true},

    // The Season 6 bosses of the boss invasion whose characters only the code of their own maps
    // creates (Kalima, Crywolf, Aida, Kanturu, Raklion, the Imperial Guardian). Their maps still
    // create them there, with their effects; on any other map, like the invasion puts them, they
    // come from here with the model and the scale of their map, instead of the default Bull Fighter.
    {MONSTER_ILLUSION_OF_KUNDUN_7, MONSTER_MODEL_ILLUSION_OF_KUNDUN, 2.0f, MODEL_STAFF_OF_KUNDUN, false},
    {MONSTER_EROHIM, MONSTER_MODEL_EROHIM, 2.0f, NoWeapon, false},
    {MONSTER_HELL_MAINE, MONSTER_MODEL_HELL_MAINE, 1.8f, NoWeapon, false},
    {MONSTER_DARKELF, MONSTER_MODEL_DARK_ELF_1, 1.5f, NoWeapon, false},
    {MONSTER_BALGASS, MONSTER_MODEL_BALGASS, 2.0f, NoWeapon, false},
    {MONSTER_NIGHTMARE, MONSTER_MODEL_DARK_SKULL_SOLDIER_5, 1.6f, NoWeapon, false},
    {MONSTER_SELUPAN, MONSTER_MODEL_SELUPAN, 2.0f, NoWeapon, false},
    {MONSTER_RAYMOND, MONSTER_MODEL_RAYMOND, 1.45f, NoWeapon, false},
};

// The bosses of the boss invasion which the generic monster code creates, so they aren't in
// InvasionBosses.
constexpr EMonsterType GenericInvasionBosses[] = {MONSTER_DARK_PHOENIX};

struct MonsterVariant
{
    EMonsterType type;
    EMonsterType base;
    float scaleFactor;
};

// The monsters of Icarus 2: a monster of the original game, a little bigger the higher its level;
// the Balrog Lord is the boss of the map.
constexpr MonsterVariant MonsterVariants[] = {
    {MONSTER_FORGE_GORGON, MONSTER_GORGON, 1.1f},
    {MONSTER_FORGE_DEATH_GORGON, MONSTER_DEATH_GORGON, 1.15f},
    {MONSTER_FORGE_BALROG, MONSTER_BALROG, 1.1f},
    {MONSTER_FORGE_METAL_BALROG, MONSTER_METAL_BALROG, 1.2f},
    {MONSTER_FORGE_METAL_BALROG_ELITE, MONSTER_METAL_BALROG, 1.45f},
    {MONSTER_BALROG_LORD, MONSTER_METAL_BALROG, 2.2f},
};

const MonsterVariant* FindVariant(EMonsterType type)
{
    for (const MonsterVariant& variant : MonsterVariants)
    {
        if (variant.type == type)
            return &variant;
    }
    return nullptr;
}

CHARACTER* CreateNpc(int model, const wchar_t* name, int positionX, int positionY, int key)
{
    OpenNpc(model);
    CHARACTER* c = CreateCharacter(key, model, positionX, positionY);
    wcscpy(c->ID, name);
    return c;
}

CHARACTER* CreateTreasureChest(int model, const wchar_t* name, float scale, int positionX, int positionY, int key)
{
    CHARACTER* c = CreateNpc(model, name, positionX, positionY, key);
    c->Object.Scale = scale;
    c->Object.m_fEdgeScale = TreasureChestEdgeScale;
    return c;
}

const InvasionBoss* FindInvasionBoss(EMonsterType type)
{
    for (const InvasionBoss& boss : InvasionBosses)
    {
        if (boss.type == type)
            return &boss;
    }
    return nullptr;
}

CHARACTER* CreateInvasionBoss(const InvasionBoss& boss, int positionX, int positionY, int key)
{
    OpenMonsterModel(boss.modelType);
    CHARACTER* c = CreateCharacter(key, static_cast<int>(MODEL_MONSTER01) + boss.modelType, positionX, positionY);
    c->Object.Scale = boss.scale;
    c->Weapon[0].Type = NoWeapon;
    c->Weapon[1].Type = boss.secondWeapon;
    c->Weapon[1].Level = 0;
    // A character slot keeps the monster skill of its last monster, which would play its effect here.
    c->MonsterSkill = -1;
    return c;
}

void SetLight(const EffectLight& colour, vec3_t light)
{
    Vector(colour.red, colour.green, colour.blue, light);
}

bool IsInvasionBoss(EMonsterType type)
{
    if (FindInvasionBoss(type) != nullptr)
        return true;
    for (const EMonsterType boss : GenericInvasionBosses)
    {
        if (boss == type)
            return true;
    }
    return false;
}

// The shock of the hit around the boss: a crater with shock waves and stones at its feet.
void CreateShockEffect(OBJECT& boss)
{
    vec3_t light;
    SetLight(ShockLight, light);
    CreateEffect(BITMAP_CRATER, boss.Position, boss.Angle, boss.Light, ShockCraterSubType);
    for (int i = 0; i < ShockWaveCount; ++i)
        CreateEffect(BITMAP_SHOCK_WAVE, boss.Position, boss.Angle, light, ShockWaveSubType);
    for (int i = 0; i < ShockStoneCount; ++i)
    {
        CreateEffect(MODEL_BIG_STONE1, boss.Position, boss.Angle, boss.Light, ShockBigStoneSubType);
        CreateEffect(MODEL_STONE2, boss.Position, boss.Angle, boss.Light);
    }
}

// The poison at the hit player, like the Poison skill.
void CreatePoisonEffect(OBJECT& boss, OBJECT& target)
{
    vec3_t light;
    SetLight(PoisonLight, light);
    CreateEffect(MODEL_POISON, target.Position, boss.Angle, boss.Light);
    for (int i = 0; i < PoisonSmokeCount; ++i)
        CreateParticle(BITMAP_SMOKE, target.Position, boss.Angle, light, PoisonSmokeSubType);
    PlayBuffer(SOUND_HEART);
}

// The ice at the hit player, like the Ice skill.
void CreateIceEffect(OBJECT& boss, OBJECT& target)
{
    vec3_t light;
    SetLight(IceLight, light);
    CreateEffect(MODEL_ICE, target.Position, boss.Angle, light);
    for (int i = 0; i < IcePieceCount; ++i)
        CreateEffect(MODEL_ICE_SMALL, target.Position, boss.Angle, boss.Light);
    PlayBuffer(SOUND_ICE);
}

// The call of the escort, like the summon of the Nightmare.
void CreateEscortCallEffect(OBJECT& boss)
{
    vec3_t light;
    SetLight(EscortCallLight, light);
    CreateEffect(MODEL_SUMMON, boss.Position, boss.Angle, light, SummonSubType);
}

// The rage, red circles at the boss like the ones of Selupan's heal.
void CreateFuryEffect(OBJECT& boss)
{
    vec3_t light;
    SetLight(FuryLight, light);
    for (int i = 0; i < FuryCircleCount; ++i)
        CreateEffect(BITMAP_MAGIC + 1, boss.Position, boss.Angle, light, FuryCircleSubType, &boss, -1, 0, 0, 0,
                     FuryCircleScale);
}

void CreateSkillEffect(const CHARACTER& character, OBJECT& boss, int skill)
{
    switch (skill)
    {
    case ShockSkill:
        CreateShockEffect(boss);
        return;
    case EscortCallSkill:
        CreateEscortCallEffect(boss);
        return;
    case FurySkill:
        CreateFuryEffect(boss);
        return;
    default:
        break;
    }

    if (character.TargetCharacter < 0 || character.TargetCharacter >= MAX_CHARACTERS_CLIENT)
        return;
    OBJECT& target = CharactersClient[character.TargetCharacter].Object;
    if (skill == PoisonSkill)
        CreatePoisonEffect(boss, target);
    else if (skill == IceSkill)
        CreateIceEffect(boss, target);
}
} // namespace

CHARACTER* Create(EMonsterType type, int positionX, int positionY, int key)
{
    switch (type)
    {
    case MONSTER_WORKSHOP:
        return CreateNpc(MODEL_WORKSHOP, L"Alquimista", positionX, positionY, key);
    case MONSTER_TREASURE_CHEST:
        return CreateTreasureChest(MODEL_TREASURE_CHEST_NPC, L"Treasure Chest", TreasureChestScale, positionX,
                                   positionY, key);
    case MONSTER_GOLDEN_TREASURE_CHEST:
        return CreateTreasureChest(MODEL_GOLDEN_TREASURE_CHEST_NPC, L"Golden Treasure Chest",
                                   GoldenTreasureChestScale, positionX, positionY, key);
    default:
        break;
    }

    const InvasionBoss* boss = FindInvasionBoss(type);
    if (boss == nullptr)
        return nullptr;
    return CreateInvasionBoss(*boss, positionX, positionY, key);
}

bool GetVariant(EMonsterType type, EMonsterType& base, float& scaleFactor)
{
    const MonsterVariant* variant = FindVariant(type);
    if (variant == nullptr)
        return false;
    base = variant->base;
    scaleFactor = variant->scaleFactor;
    return true;
}

void ApplyVariant(CHARACTER& character, EMonsterType type, float scaleFactor)
{
    character.Object.Scale *= scaleFactor;
    GameLogic::Monsters::Names::WriteShownName(character, type);
}

void ScaleInvasionBoss(CHARACTER& character)
{
    if (IsInvasionBoss(static_cast<EMonsterType>(character.MonsterIndex)))
        character.Object.Scale *= InvasionBossSizeFactor;
}

bool SetAttackAction(CHARACTER& character, OBJECT& object)
{
    const InvasionBoss* boss = FindInvasionBoss(static_cast<EMonsterType>(character.MonsterIndex));
    if (boss == nullptr || !boss->hasSkills)
        return false;

    // The attacks may clear the skill as they play it.
    const int skill = character.MonsterSkill;
    if (!GameLogic::Monsters::Attacks::Play(character, object))
        return false;
    CreateSkillEffect(character, object, skill);
    return true;
}
} // namespace Engine::Object::ServerMonsters
