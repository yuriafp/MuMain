#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "GameLogic/Items/Auction.h"

// The packets of Otto, o Leiloeiro (MUnique.OpenMU.Tbt.Auction): a page of his window, C2 FB 04
// (AuctionContentsWriter), the result of an action, C1 08 FB 06, and a request from the client,
// C1 14 FB 05 (AuctionRequest).
namespace Network::Auction
{
constexpr std::uint8_t HeadCode = 0xFB;
constexpr std::uint8_t PageSubCode = 0x04;
constexpr std::uint8_t RequestSubCode = 0x05;
constexpr std::uint8_t ResultSubCode = 0x06;

#pragma pack(push, 1)
struct PageHeader
{
    std::uint8_t type;
    std::uint8_t sizeHigh;
    std::uint8_t sizeLow;
    std::uint8_t headCode;
    std::uint8_t subCode;
    std::uint8_t view;
    std::uint8_t flags;
    std::uint8_t kindFilter;
    std::uint8_t currencyFilter;
    std::uint8_t entryCount;
    std::uint16_t page;
    std::uint16_t pages;
    std::int32_t listingFee;
    std::int32_t cancelFee;
    std::int32_t buyerFee;
    std::uint8_t durationDays;
    std::uint8_t maxItemsPerAccount;
    std::uint8_t accountItems;
    std::uint8_t sort;
    std::int64_t zenToClaim;
    std::int32_t total;
};

// One entry of C2 FB 04; the entries follow the header from byte 42.
struct PageEntry
{
    std::int64_t id;
    std::uint8_t currency;
    std::uint8_t flags;
    std::uint8_t reserved[2];
    std::uint32_t price;
    std::int32_t secondsLeft;
    char seller[10];
    std::uint8_t itemLength;
    std::uint8_t reservedItem;
    std::uint8_t item[16];
};

struct Result
{
    std::uint8_t type;
    std::uint8_t size;
    std::uint8_t headCode;
    std::uint8_t subCode;
    std::uint8_t action;
    std::uint8_t done;
    std::uint8_t reserved[2];
};

struct Request
{
    std::uint8_t type;
    std::uint8_t size;
    std::uint8_t headCode;
    std::uint8_t subCode;
    std::uint8_t action;
    std::uint8_t arg1;
    std::uint8_t arg2;
    std::uint8_t reserved;
    std::int64_t value;
    std::uint32_t price;
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == 42, "C2 FB 04 has its entries from byte 42");
static_assert(offsetof(PageHeader, view) == 5, "C2 FB 04 has view at byte 5");
static_assert(offsetof(PageHeader, flags) == 6, "C2 FB 04 has flags at byte 6");
static_assert(offsetof(PageHeader, kindFilter) == 7, "C2 FB 04 has kindFilter at byte 7");
static_assert(offsetof(PageHeader, currencyFilter) == 8, "C2 FB 04 has currencyFilter at byte 8");
static_assert(offsetof(PageHeader, entryCount) == 9, "C2 FB 04 has entryCount at byte 9");
static_assert(offsetof(PageHeader, page) == 10, "C2 FB 04 has page at byte 10");
static_assert(offsetof(PageHeader, pages) == 12, "C2 FB 04 has pages at byte 12");
static_assert(offsetof(PageHeader, listingFee) == 14, "C2 FB 04 has listingFee at byte 14");
static_assert(offsetof(PageHeader, cancelFee) == 18, "C2 FB 04 has cancelFee at byte 18");
static_assert(offsetof(PageHeader, buyerFee) == 22, "C2 FB 04 has buyerFee at byte 22");
static_assert(offsetof(PageHeader, durationDays) == 26, "C2 FB 04 has durationDays at byte 26");
static_assert(offsetof(PageHeader, maxItemsPerAccount) == 27, "C2 FB 04 has maxItemsPerAccount at byte 27");
static_assert(offsetof(PageHeader, accountItems) == 28, "C2 FB 04 has accountItems at byte 28");
static_assert(offsetof(PageHeader, sort) == 29, "C2 FB 04 has sort at byte 29");
static_assert(offsetof(PageHeader, zenToClaim) == 30, "C2 FB 04 has zenToClaim at byte 30");
static_assert(offsetof(PageHeader, total) == 38, "C2 FB 04 has total at byte 38");

static_assert(sizeof(PageEntry) == 48, "C2 FB 04 has 48 bytes per entry");
static_assert(offsetof(PageEntry, id) == 0, "PageEntry has id at byte 0");
static_assert(offsetof(PageEntry, currency) == 8, "PageEntry has currency at byte 8");
static_assert(offsetof(PageEntry, flags) == 9, "PageEntry has flags at byte 9");
static_assert(offsetof(PageEntry, price) == 12, "PageEntry has price at byte 12");
static_assert(offsetof(PageEntry, secondsLeft) == 16, "PageEntry has secondsLeft at byte 16");
static_assert(offsetof(PageEntry, seller) == 20, "PageEntry has seller at byte 20");
static_assert(offsetof(PageEntry, itemLength) == 30, "PageEntry has itemLength at byte 30");
static_assert(offsetof(PageEntry, item) == 32, "PageEntry has item at byte 32");

static_assert(sizeof(Result) == 8, "C1 FB 06 is 8 bytes");
static_assert(offsetof(Result, action) == 4, "C1 FB 06 has action at byte 4");
static_assert(offsetof(Result, done) == 5, "C1 FB 06 has done at byte 5");

static_assert(sizeof(Request) == 20, "C1 FB 05 is 20 bytes");
static_assert(offsetof(Request, action) == 4, "C1 FB 05 has action at byte 4");
static_assert(offsetof(Request, arg1) == 5, "C1 FB 05 has arg1 at byte 5");
static_assert(offsetof(Request, arg2) == 6, "C1 FB 05 has arg2 at byte 6");
static_assert(offsetof(Request, value) == 8, "C1 FB 05 has value at byte 8");
static_assert(offsetof(Request, price) == 16, "C1 FB 05 has price at byte 16");

enum Flags : std::uint8_t
{
    // The client opens the window; without it, the packet updates an open one.
    Open = 1,
};

// Reads C2 FB 04; false when it is shorter than its header or its entries.
bool ReadPage(const std::uint8_t* data, std::size_t size, GameLogic::Items::AuctionPage& page, bool& open);

// Reads C1 08 FB 06; false when it is shorter than Result.
bool ReadResult(const std::uint8_t* data, std::size_t size, GameLogic::Items::AuctionResult& result);

// C1 14 FB 05 for an action of the window.
Request MakeRequest(GameLogic::Items::AuctionAction action, std::uint8_t arg1 = 0, std::uint8_t arg2 = 0,
                    std::int64_t value = 0, std::uint32_t price = 0);

// The count of item types of a search without a search by name.
constexpr std::uint8_t NoNameSearch = 0xFF;

// C1 FB 05 of a search (Browse): the request, then the quality (byte 20), the order (byte 21), how many
// item types follow (byte 22, NoNameSearch without a search by name) and the item types, 2 bytes each
// from byte 23. More types than fit (GameLogic::Items::AuctionMaximumSearchTypes) are cut.
std::vector<std::uint8_t> MakeSearchRequest(GameLogic::Items::AuctionCategory kind, std::uint8_t currency,
                                            std::uint16_t page, GameLogic::Items::AuctionQuality quality,
                                            const std::optional<std::vector<std::uint16_t>>& itemTypes,
                                            GameLogic::Items::AuctionSort sort = GameLogic::Items::AuctionSort::Newest);
} // namespace Network::Auction
