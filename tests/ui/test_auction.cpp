#include "doctest.h"
#include "GameLogic/Items/Auction.h"
#include "Network/Server/AuctionPacket.h"

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using GameLogic::Items::AuctionAction;
using GameLogic::Items::AuctionCategory;
using GameLogic::Items::AuctionPage;
using GameLogic::Items::AuctionView;

namespace
{
template <typename T> void Put(std::vector<std::uint8_t>& data, std::size_t offset, T value)
{
    std::memcpy(&data[offset], &value, sizeof(T));
}

// A page in the server's layout (MUnique.OpenMU.Tbt.Auction.AuctionContentsWriter): page 1 of 3 of
// the weapons in any currency, with up to two entries: an item of a seller with a Latin-1 name for
// 1,500,000 zen, and an item of the account for 10 Soul whose sale ended.
std::vector<std::uint8_t> ServerPage(std::uint8_t entries)
{
    std::vector<std::uint8_t> data(38 + 48 * static_cast<std::size_t>(entries));
    data[0] = 0xC2;
    data[1] = static_cast<std::uint8_t>(data.size() >> 8);
    data[2] = static_cast<std::uint8_t>(data.size() & 0xFF);
    data[3] = 0xFB;
    data[4] = 0x04;
    data[5] = 1;    // view: for sale
    data[6] = 1;    // open
    data[7] = 1;    // weapons
    data[8] = 0xFF; // any currency
    data[9] = entries;
    Put<std::uint16_t>(data, 10, 1);
    Put<std::uint16_t>(data, 12, 3);
    Put<std::int32_t>(data, 14, 100000);
    Put<std::int32_t>(data, 18, 50000);
    Put<std::int32_t>(data, 22, 120000);
    data[26] = 7;
    data[27] = 20;
    data[28] = 4;
    Put<std::int64_t>(data, 30, 3000000000LL);
    if (entries >= 1)
    {
        const std::size_t e = 38;
        Put<std::int64_t>(data, e, 42);
        data[e + 8] = 0;
        Put<std::uint32_t>(data, e + 12, 1500000);
        Put<std::int32_t>(data, e + 16, 6 * 86400 + 23 * 3600);
        std::memcpy(&data[e + 20], "Jo\xE3o", 4);
        data[e + 30] = 12;
        data[e + 32] = 2;
        data[e + 33] = 9 << 3;
    }
    if (entries >= 2)
    {
        const std::size_t e = 38 + 48;
        Put<std::int64_t>(data, e, 43);
        data[e + 8] = 2;
        data[e + 9] = 1 | 2;
        Put<std::uint32_t>(data, e + 12, 10);
        Put<std::int32_t>(data, e + 16, 0);
        std::memcpy(&data[e + 20], "Yuri", 4);
        data[e + 30] = 12;
    }
    return data;
}
} // namespace

TEST_CASE("A page of Otto reads with its entries")
{
    const auto data = ServerPage(2);
    AuctionPage page;
    bool open = false;
    REQUIRE(Network::Auction::ReadPage(data.data(), data.size(), page, open));
    CHECK(open);
    CHECK(page.view == AuctionView::Browse);
    CHECK(page.kindFilter == AuctionCategory::Weapons);
    CHECK(page.currencyFilter == GameLogic::Items::AuctionCurrencyAny);
    CHECK(page.page == 1);
    CHECK(page.pages == 3);
    CHECK(page.listingFee == 100000);
    CHECK(page.cancelFee == 50000);
    CHECK(page.buyerFee == 120000);
    CHECK(page.durationDays == 7);
    CHECK(page.maxItemsPerAccount == 20);
    CHECK(page.accountItems == 4);
    CHECK(page.zenToClaim == 3000000000LL);
    REQUIRE(page.entries.size() == 2);

    const auto& kris = page.entries[0];
    CHECK(kris.id == 42);
    CHECK(kris.currency == GameLogic::Items::AuctionCurrencyZen);
    CHECK(kris.price == 1500000u);
    CHECK_FALSE(kris.IsMine());
    CHECK_FALSE(kris.IsEnded());
    CHECK((GameLogic::Items::SellerName(kris) == std::wstring{L'J', L'o', static_cast<wchar_t>(0xE3), L'o'}));
    CHECK(kris.itemLength == 12);
    CHECK(kris.itemBytes[0] == 2);
    CHECK(kris.itemBytes[1] == (9 << 3));

    const auto& mine = page.entries[1];
    CHECK(mine.id == 43);
    CHECK(mine.currency == 2);
    CHECK(mine.IsMine());
    CHECK(mine.IsEnded());
    CHECK_FALSE(mine.IsPending());
    CHECK((GameLogic::Items::SellerName(mine) == L"Yuri"));
}

TEST_CASE("A page shorter than its entries is refused")
{
    auto data = ServerPage(2);
    data.resize(data.size() - 1);
    AuctionPage page;
    bool open = false;
    CHECK_FALSE(Network::Auction::ReadPage(data.data(), data.size(), page, open));
    CHECK_FALSE(Network::Auction::ReadPage(data.data(), 20, page, open));
}

TEST_CASE("The requests of the window have the server's layout")
{
    const auto bytes = [](const Network::Auction::Request& request)
    {
        std::vector<std::uint8_t> data(sizeof(request));
        std::memcpy(data.data(), &request, sizeof(request));
        return data;
    };

    const auto browse = bytes(Network::Auction::MakeRequest(AuctionAction::Browse, 4, 0xFF, 2, 0));
    CHECK(browse == std::vector<std::uint8_t>{0xC1, 20, 0xFB, 0x05, 1, 4, 0xFF, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    const auto list = bytes(Network::Auction::MakeRequest(AuctionAction::List, 12 + 2 * 8 + 3, 1, 0, 250));
    CHECK(list == std::vector<std::uint8_t>{0xC1, 20, 0xFB, 0x05, 3, 31, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 250, 0, 0, 0});

    const auto buy = bytes(Network::Auction::MakeRequest(AuctionAction::Buy, 0, 0, 0x0102030405LL, 1500000));
    CHECK(buy == std::vector<std::uint8_t>{0xC1, 20, 0xFB, 0x05, 4, 0, 0, 0, 5, 4, 3, 2, 1, 0, 0, 0, 0x60, 0xE3, 0x16, 0});

    const auto back = bytes(Network::Auction::MakeRequest(AuctionAction::Return, 1, 0, 43, 0));
    CHECK(back[4] == 5);
    CHECK(back[5] == 1);
    CHECK(back[8] == 43);

    CHECK(bytes(Network::Auction::MakeRequest(AuctionAction::Mine))[4] == 2);
    CHECK(bytes(Network::Auction::MakeRequest(AuctionAction::ClaimZen))[4] == 6);
}

TEST_CASE("The result of Otto reads")
{
    const std::uint8_t data[] = {0xC1, 8, 0xFB, 0x06, 4, 1, 0, 0};
    GameLogic::Items::AuctionResult result;
    REQUIRE(Network::Auction::ReadResult(data, sizeof(data), result));
    CHECK(result.action == AuctionAction::Buy);
    CHECK(result.done);
    CHECK_FALSE(Network::Auction::ReadResult(data, 5, result));
}

TEST_CASE("The time left reads in days, hours or minutes")
{
    CHECK(GameLogic::Items::FormatTimeLeft(6 * 86400 + 23 * 3600 + 59) == "6d 23h");
    CHECK(GameLogic::Items::FormatTimeLeft(5 * 3600 + 10 * 60) == "5h 10m");
    CHECK(GameLogic::Items::FormatTimeLeft(12 * 60 + 30) == "12m");
    CHECK(GameLogic::Items::FormatTimeLeft(20) == "1m");
}

TEST_CASE("The currencies step around, the filter with any")
{
    CHECK(std::string(GameLogic::Items::CurrencyName(0)) == "Zen");
    CHECK(std::string(GameLogic::Items::CurrencyName(1)) == "Bless");
    CHECK(std::string(GameLogic::Items::CurrencyName(8)) == "Gemstone");
    CHECK(GameLogic::Items::StepFilterCurrency(GameLogic::Items::AuctionCurrencyAny, 1) == 0);
    CHECK(GameLogic::Items::StepFilterCurrency(GameLogic::Items::AuctionCurrencyAny, -1) == 10);
    CHECK(GameLogic::Items::StepFilterCurrency(10, 1) == GameLogic::Items::AuctionCurrencyAny);
    CHECK(GameLogic::Items::StepSellCurrency(0, -1) == 10);
    CHECK(GameLogic::Items::StepSellCurrency(10, 1) == 0);
    CHECK(GameLogic::Items::StepKind(AuctionCategory::Others, 1) == AuctionCategory::Accessories);
    CHECK(GameLogic::Items::StepKind(AuctionCategory::Pets, 1) == AuctionCategory::All);
    CHECK(GameLogic::Items::StepKind(AuctionCategory::All, -1) == AuctionCategory::Pets);
}

TEST_CASE("A list takes parts of six rows")
{
    CHECK(GameLogic::Items::AuctionRowPages(0) == 1);
    CHECK(GameLogic::Items::AuctionRowPages(6) == 1);
    CHECK(GameLogic::Items::AuctionRowPages(7) == 2);
    CHECK(GameLogic::Items::AuctionRowPages(20) == 4);
    CHECK(GameLogic::Items::MainInventoryServerSlot(3, 2) == 31);
}

TEST_CASE("A search has the server's layout after the request")
{
    using GameLogic::Items::AuctionQuality;
    const std::vector<std::uint16_t> types{0, 1, 6145};
    const auto search = Network::Auction::MakeSearchRequest(AuctionCategory::Armor, 0xFF, 2, AuctionQuality::Level9, types);
    REQUIRE(search.size() == 28);
    CHECK(search[0] == 0xC1);
    CHECK(search[1] == 28);
    CHECK(search[2] == 0xFB);
    CHECK(search[3] == 0x05);
    CHECK(search[4] == 1);
    CHECK(search[5] == static_cast<std::uint8_t>(AuctionCategory::Armor));
    CHECK(search[6] == 0xFF);
    CHECK(search[8] == 2);
    CHECK(search[20] == 5);
    CHECK(search[21] == 3);
    CHECK(std::vector<std::uint8_t>(search.begin() + 22, search.end()) == std::vector<std::uint8_t>{0, 0, 1, 0, 0x01, 0x18});

    const auto any = Network::Auction::MakeSearchRequest(AuctionCategory::All, 0, 0, AuctionQuality::Any, std::nullopt);
    REQUIRE(any.size() == 22);
    CHECK(any[1] == 22);
    CHECK(any[20] == 0);
    CHECK(any[21] == Network::Auction::NoNameSearch);

    const std::vector<std::uint16_t> many(200, 7);
    const auto capped = Network::Auction::MakeSearchRequest(AuctionCategory::All, 0, 0, AuctionQuality::Any, many);
    CHECK(capped.size() == 22 + 2 * GameLogic::Items::AuctionMaximumSearchTypes);
    CHECK(capped[21] == GameLogic::Items::AuctionMaximumSearchTypes);
    CHECK(capped.size() <= 255);
}

TEST_CASE("A search finds the names in any case and without accents")
{
    using GameLogic::Items::FoldSearchText;
    CHECK((FoldSearchText(L"  Cris ") == L"cris"));
    CHECK((FoldSearchText(std::wstring{static_cast<wchar_t>(0xC1), L'g', L'u', L'i', L'a'}) == L"aguia"));
    CHECK((FoldSearchText(std::wstring{L'C', L'o', static_cast<wchar_t>(0xE7), static_cast<wchar_t>(0xE3), L'o'}) == L"cocao"));
    CHECK(FoldSearchText(L"   ").empty());

    const auto nameOf = [](int type) -> std::wstring
    {
        switch (type)
        {
        case 0:
            return L"Cris";
        case 3:
            return L"Espada Cristalina";
        case 5:
            return L"Machado";
        default:
            return {};
        }
    };
    CHECK((GameLogic::Items::MatchItemTypes(L"CRIS", 10, nameOf, 10) == std::vector<std::uint16_t>{0, 3}));
    CHECK((GameLogic::Items::MatchItemTypes(L"cris", 10, nameOf, 1) == std::vector<std::uint16_t>{0}));
    CHECK(GameLogic::Items::MatchItemTypes(L"arco", 10, nameOf, 10).empty());
    CHECK(GameLogic::Items::MatchItemTypes(L"  ", 10, nameOf, 10).empty());
}

TEST_CASE("The qualities step around, the levels with their minimum")
{
    using GameLogic::Items::AuctionQuality;
    CHECK(GameLogic::Items::StepQuality(AuctionQuality::Any, -1) == AuctionQuality::Level13);
    CHECK(GameLogic::Items::StepQuality(AuctionQuality::Level13, 1) == AuctionQuality::Any);
    CHECK(GameLogic::Items::StepQuality(AuctionQuality::Excellent, 1) == AuctionQuality::Ancient);
    CHECK(GameLogic::Items::AuctionQualityLevel(AuctionQuality::Level7) == 7);
    CHECK(GameLogic::Items::AuctionQualityLevel(AuctionQuality::Level13) == 13);
    CHECK(GameLogic::Items::AuctionQualityLevel(AuctionQuality::Socket) == 0);
}
