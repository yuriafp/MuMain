#include "doctest.h"
#include "GameLogic/Items/JewelBank.h"
#include "Network/Server/JewelBankPacket.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

using GameLogic::Items::JewelBankAction;
using GameLogic::Items::JewelBankContents;

namespace
{
// A packet in the server's layout (MUnique.OpenMU.Tbt.JewelBank.JewelBankContentsRef): Bless 25,
// Soul 0, with the fees 100,000 and 200,000.
std::vector<std::uint8_t> ServerPacket(std::uint8_t flags)
{
    std::vector<std::uint8_t> data(sizeof(Network::JewelBank::ContentsHeader) + 2 * sizeof(Network::JewelBank::ContentsJewel));
    data[0] = 0xC1;
    data[1] = static_cast<std::uint8_t>(data.size());
    data[2] = Network::JewelBank::HeadCode;
    data[3] = Network::JewelBank::ContentsSubCode;
    data[4] = flags;
    data[5] = 2;
    const std::int32_t fees[] = {100000, 200000};
    std::memcpy(&data[8], fees, sizeof(fees));
    const std::uint8_t jewels[] = {14, 0, 13, 0, 25, 0, 0, 0, 14, 0, 14, 0, 0, 0, 0, 0};
    std::memcpy(&data[16], jewels, sizeof(jewels));
    return data;
}
} // namespace

TEST_CASE("The jewels of the server read into the bank")
{
    const auto data = ServerPacket(Network::JewelBank::Open);
    JewelBankContents contents;
    bool open = false;
    REQUIRE(Network::JewelBank::Read(data.data(), data.size(), contents, open));
    CHECK(open);
    CHECK(contents.depositFeePerJewel == 100000);
    CHECK(contents.withdrawalFeePerJewel == 200000);
    REQUIRE(contents.jewels.size() == 2);
    CHECK(contents.jewels[0] == GameLogic::Items::JewelBankJewel{14, 13, 25});
    CHECK(contents.jewels[1] == GameLogic::Items::JewelBankJewel{14, 14, 0});
}

TEST_CASE("An update of the bank doesn't open the window")
{
    const auto data = ServerPacket(0);
    JewelBankContents contents;
    bool open = true;
    REQUIRE(Network::JewelBank::Read(data.data(), data.size(), contents, open));
    CHECK_FALSE(open);
}

TEST_CASE("A packet shorter than its jewels isn't read")
{
    const auto data = ServerPacket(Network::JewelBank::Open);
    JewelBankContents contents;
    bool open = false;
    CHECK_FALSE(Network::JewelBank::Read(data.data(), data.size() - 1, contents, open));
    CHECK_FALSE(Network::JewelBank::Read(data.data(), 15, contents, open));
    CHECK_FALSE(Network::JewelBank::Read(nullptr, data.size(), contents, open));
}

TEST_CASE("The request is what the server reads")
{
    const auto request = Network::JewelBank::MakeRequest(JewelBankAction::Withdraw, 3, 30);
    std::array<std::uint8_t, sizeof(request)> bytes{};
    std::memcpy(bytes.data(), &request, sizeof(request));
    CHECK(bytes == std::array<std::uint8_t, 8>{0xC1, 0x08, 0xFB, 0x03, 3, 3, 30, 0});
}

TEST_CASE("A withdrawal needs what the account keeps")
{
    JewelBankContents contents;
    contents.jewels = {{14, 13, 25}, {14, 14, 0}};
    CHECK(GameLogic::Items::CanWithdraw(contents, 0, 1));
    CHECK(GameLogic::Items::CanWithdraw(contents, 0, 10));
    CHECK_FALSE(GameLogic::Items::CanWithdraw(contents, 0, 30));
    CHECK_FALSE(GameLogic::Items::CanWithdraw(contents, 1, 1));
    CHECK_FALSE(GameLogic::Items::CanWithdraw(contents, 2, 1));
    CHECK_FALSE(GameLogic::Items::CanWithdraw(contents, -1, 1));
    CHECK_FALSE(GameLogic::Items::CanWithdraw(contents, 0, 0));
}

TEST_CASE("Each answer of the server is a new version of the bank")
{
    const unsigned before = GameLogic::Items::CurrentJewelBankVersion();
    JewelBankContents contents;
    contents.depositFeePerJewel = 5;
    GameLogic::Items::SetCurrentJewelBank(contents);
    GameLogic::Items::SetCurrentJewelBank(contents);
    CHECK(GameLogic::Items::CurrentJewelBankVersion() == before + 2);
    CHECK(GameLogic::Items::CurrentJewelBank() == contents);
}
