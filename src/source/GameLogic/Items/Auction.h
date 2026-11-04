#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The auction house of Otto, o Leiloeiro (MUnique.OpenMU.Tbt.Auction): the page of listings and
// account state sent in C2 FB 04, the result of an action sent in C1 08 FB 06, the currency names
// and the time-left formatting.
namespace GameLogic::Items
{
// What the window asks Otto (MUnique.OpenMU.Tbt.Auction.AuctionAction).
enum class AuctionAction : std::uint8_t
{
    Browse = 1,
    Mine = 2,
    List = 3,
    Buy = 4,
    Return = 5,
    ClaimZen = 6,
};

// Which page C2 FB 04 carries (MUnique.OpenMU.Tbt.Auction.AuctionView).
enum class AuctionView : std::uint8_t
{
    Browse = 1,
    Mine = 2,
};

// Kind filter values in C2 FB 04 and Browse requests (MUnique.OpenMU.Tbt.Auction.AuctionCategory).
enum class AuctionCategory : std::uint8_t
{
    All = 0,
    Weapons = 1,
    Shields = 2,
    Armor = 3,
    Wings = 4,
    Jewels = 5,
    Others = 6,
    Accessories = 7,
    Pets = 8,
};

constexpr int AuctionCategoryCount = 9;

// What a search looks for besides the kind and the currency: an option or a level
// (MUnique.OpenMU.Tbt.Auction.AuctionQuality).
enum class AuctionQuality : std::uint8_t
{
    Any = 0,
    Excellent = 1,
    Ancient = 2,
    Socket = 3,
    Level7 = 4,
    Level9 = 5,
    Level11 = 6,
    Level13 = 7,
};

constexpr int AuctionQualityCount = 8;

// The lowest level of a level quality, 0 for the others.
constexpr int AuctionQualityLevel(AuctionQuality quality)
{
    switch (quality)
    {
    case AuctionQuality::Level7:
        return 7;
    case AuctionQuality::Level9:
        return 9;
    case AuctionQuality::Level11:
        return 11;
    case AuctionQuality::Level13:
        return 13;
    default:
        return 0;
    }
}

// The search by name: the server keeps no translated names, so the client finds the item types
// whose name in its language holds the text, and sends them (group * 512 + number).
constexpr std::size_t AuctionMinimumSearchLength = 2;
// The most item types a search sends: what fits in a C1 packet after its 22 bytes.
constexpr std::size_t AuctionMaximumSearchTypes = (255 - 22) / 2;

// The text folded for a search: lower case, the accents of Latin-1 dropped, spaces trimmed.
std::wstring FoldSearchText(std::wstring_view text);

// The item types below `typeCount` whose name (`nameOf`, empty for none) holds the folded `query`,
// at most `limit` of them, in order.
std::vector<std::uint16_t> MatchItemTypes(std::wstring_view query, int typeCount,
                                          const std::function<std::wstring(int)>& nameOf, std::size_t limit);

// Currency codes in C2 FB 04 and C1 FB 05 (MUnique.OpenMU.Tbt.Auction.AuctionCurrency).
constexpr std::uint8_t AuctionCurrencyZen = 0;
constexpr std::uint8_t AuctionCurrencyMax = 10;
constexpr std::uint8_t AuctionCurrencyAny = 0xFF;

constexpr std::uint32_t MaxAuctionZenPrice = 2'000'000'000u;
constexpr std::uint32_t MaxAuctionJewelPrice = 9'999u;

constexpr std::uint32_t MaxAuctionPrice(std::uint8_t currency)
{
    return currency == AuctionCurrencyZen ? MaxAuctionZenPrice : MaxAuctionJewelPrice;
}

// Server inventory slot numbering for the main 8x8 bag.
constexpr std::uint8_t MainInventoryFirstSlot = 12;
constexpr int MainInventoryColumns = 8;
constexpr int MainInventoryRows = 8;

constexpr std::uint8_t MainInventoryServerSlot(int columnX, int rowY)
{
    return static_cast<std::uint8_t>(MainInventoryFirstSlot + rowY * MainInventoryColumns + columnX);
}

// Entry flags in byte 9 of each 48-byte entry of C2 FB 04.
enum AuctionEntryFlags : std::uint8_t
{
    AuctionEntryNone = 0,
    AuctionEntryMine = 1,
    AuctionEntryEnded = 2,
    AuctionEntryPending = 4,
};

struct AuctionEntry
{
    std::int64_t id = 0;
    std::uint8_t currency = AuctionCurrencyZen;
    std::uint8_t flags = AuctionEntryNone;
    std::uint32_t price = 0;
    std::int32_t secondsLeft = 0;
    std::string seller;
    std::uint8_t itemLength = 0;
    std::array<std::uint8_t, 16> itemBytes{};

    bool IsMine() const { return (flags & AuctionEntryMine) != 0; }
    bool IsEnded() const { return (flags & AuctionEntryEnded) != 0 || secondsLeft <= 0; }
    bool IsPending() const { return (flags & AuctionEntryPending) != 0; }

    bool operator==(const AuctionEntry&) const = default;
};

struct AuctionPage
{
    AuctionView view = AuctionView::Browse;
    AuctionCategory kindFilter = AuctionCategory::All;
    std::uint8_t currencyFilter = AuctionCurrencyAny;
    std::uint16_t page = 0;
    std::uint16_t pages = 1;
    std::int32_t listingFee = 0;
    std::int32_t cancelFee = 0;
    std::int32_t buyerFee = 0;
    std::uint8_t durationDays = 0;
    std::uint8_t maxItemsPerAccount = 0;
    std::uint8_t accountItems = 0;
    std::int64_t zenToClaim = 0;
    std::vector<AuctionEntry> entries;

    bool operator==(const AuctionPage&) const = default;
};

struct AuctionResult
{
    AuctionAction action = AuctionAction::Browse;
    bool done = false;

    bool operator==(const AuctionResult&) const = default;
};

// Display name of a currency selector code: "Todas" for 0xFF, "Zen" for 0, and the ten jewels of
// BankJewel.All for 1..10 ("Bless" .. "Pedra Menor", "Pedra Maior").
std::string_view CurrencyName(std::uint8_t currency);

// Unit name of a listing's currency as shown after a price ("zen" for 0, CurrencyName for 1..10).
std::string_view PriceCurrencyName(std::uint8_t currency);

// Steps the currency filter (including 0xFF = Todas) by delta (-1 or +1).
std::uint8_t StepFilterCurrency(std::uint8_t current, int delta);

// Steps the sell currency (0..10, without Todas) by delta (-1 or +1).
std::uint8_t StepSellCurrency(std::uint8_t current, int delta);

// Steps the kind filter (Todos .. Pets) by delta (-1 or +1), wrapping around.
AuctionCategory StepKind(AuctionCategory current, int delta);

// Steps the quality filter by delta (-1 or +1), wrapping around.
AuctionQuality StepQuality(AuctionQuality current, int delta);

// Formats seconds left as "6d 23h", "5h 10m" or "12m".
std::string FormatTimeLeft(std::int32_t secondsLeft);

// The seller's name, which the server writes in Latin-1.
std::wstring SellerName(const AuctionEntry& entry);

// How many rows the window shows at once: a page of the server is shown in parts of this many.
constexpr int AuctionRowsPerPage = 6;

// How many parts of AuctionRowsPerPage rows `count` rows take, at least one.
constexpr int AuctionRowPages(std::size_t count)
{
    return count == 0 ? 1 : static_cast<int>((count + AuctionRowsPerPage - 1) / AuctionRowsPerPage);
}

const AuctionPage& CurrentAuctionPage();
unsigned CurrentAuctionPageVersion();
void SetCurrentAuctionPage(AuctionPage page);

const AuctionResult& LastAuctionResult();
unsigned CurrentAuctionResultVersion();
void SetLastAuctionResult(AuctionResult result);
} // namespace GameLogic::Items
