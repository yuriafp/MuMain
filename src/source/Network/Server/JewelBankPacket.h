#pragma once

#include <cstddef>
#include <cstdint>

#include "GameLogic/Items/JewelBank.h"

// The packets of Ella, the Banqueira of our server (MUnique.OpenMU.Tbt.JewelBank): the jewels
// which the account keeps, C1 FB 02 (JewelBankContentsRef), and the requests of her window,
// C1 08 FB 03 (JewelBankRequest). The head code 0xFB is free in OpenMU and MuMain.
namespace Network::JewelBank
{
constexpr std::uint8_t HeadCode = 0xFB;
constexpr std::uint8_t ContentsSubCode = 0x02;
constexpr std::uint8_t RequestSubCode = 0x03;

#pragma pack(push, 1)
struct ContentsHeader
{
    std::uint8_t type;
    std::uint8_t size;
    std::uint8_t headCode;
    std::uint8_t subCode;
    std::uint8_t flags;
    std::uint8_t jewelCount;
    std::uint8_t reserved[2];
    std::int32_t depositFeePerJewel;
    std::int32_t withdrawalFeePerJewel;
};

// One jewel of C1 FB 02; the jewels follow the header.
struct ContentsJewel
{
    std::uint8_t group;
    std::uint8_t reserved;
    std::int16_t number;
    std::int32_t amount;
};

struct Request
{
    std::uint8_t type;
    std::uint8_t size;
    std::uint8_t headCode;
    std::uint8_t subCode;
    // GameLogic::Items::JewelBankAction.
    std::uint8_t action;
    // The jewel's index in C1 FB 02; any for DepositAll.
    std::uint8_t jewelIndex;
    // What a withdrawal takes out: 1, 10 or 30.
    std::uint8_t amount;
    std::uint8_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(ContentsHeader) == 16, "C1 FB 02 has its jewels from byte 16");
static_assert(offsetof(ContentsHeader, depositFeePerJewel) == 8, "C1 FB 02 has the fees at byte 8");
static_assert(sizeof(ContentsJewel) == 8, "C1 FB 02 has 8 bytes per jewel");
static_assert(sizeof(Request) == 8, "C1 FB 03 is 8 bytes");

enum Flags : std::uint8_t
{
    // The client opens the window; without it, the packet updates an open one.
    Open = 1,
};

// Reads C1 FB 02; false when it's shorter than its jewels.
bool Read(const std::uint8_t* data, std::size_t size, GameLogic::Items::JewelBankContents& contents, bool& open);

// C1 FB 03 for an action of the window.
Request MakeRequest(GameLogic::Items::JewelBankAction action, int jewelIndex, int amount);
} // namespace Network::JewelBank
