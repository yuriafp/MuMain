#include "Network/Server/AuctionPacket.h"

#include <algorithm>
#include <cstring>

namespace Network::Auction
{
bool ReadPage(const std::uint8_t* data, std::size_t size, GameLogic::Items::AuctionPage& page, bool& open)
{
    if (data == nullptr || size < sizeof(PageHeader))
        return false;

    PageHeader header;
    std::memcpy(&header, data, sizeof(PageHeader));

    const std::size_t needed = sizeof(PageHeader) + static_cast<std::size_t>(header.entryCount) * sizeof(PageEntry);
    const std::size_t declared = (static_cast<std::size_t>(header.sizeHigh) << 8) | header.sizeLow;
    if (size < needed || (declared != 0 && declared < needed))
        return false;

    open = (header.flags & Open) != 0;
    page.view = static_cast<GameLogic::Items::AuctionView>(header.view);
    page.kindFilter = static_cast<GameLogic::Items::AuctionCategory>(header.kindFilter);
    page.currencyFilter = header.currencyFilter;
    page.page = header.page;
    page.pages = header.pages;
    page.listingFee = header.listingFee;
    page.cancelFee = header.cancelFee;
    page.buyerFee = header.buyerFee;
    page.durationDays = header.durationDays;
    page.maxItemsPerAccount = header.maxItemsPerAccount;
    page.accountItems = header.accountItems;
    page.zenToClaim = header.zenToClaim;
    page.sort = header.sort < GameLogic::Items::AuctionSortCount ? static_cast<GameLogic::Items::AuctionSort>(header.sort)
                                                                 : GameLogic::Items::AuctionSort::Newest;
    page.total = header.total;

    page.entries.clear();
    page.entries.reserve(header.entryCount);
    for (std::size_t i = 0; i < header.entryCount; ++i)
    {
        PageEntry raw;
        std::memcpy(&raw, data + sizeof(PageHeader) + i * sizeof(PageEntry), sizeof(PageEntry));

        GameLogic::Items::AuctionEntry entry;
        entry.id = raw.id;
        entry.currency = raw.currency;
        entry.flags = raw.flags;
        entry.price = raw.price;
        entry.secondsLeft = raw.secondsLeft;

        std::size_t sellerLen = 0;
        while (sellerLen < sizeof(raw.seller) && raw.seller[sellerLen] != '\0')
            ++sellerLen;
        entry.seller.assign(raw.seller, sellerLen);

        entry.itemLength = static_cast<std::uint8_t>(std::min<std::size_t>(raw.itemLength, sizeof(raw.item)));
        entry.itemBytes.fill(0);
        std::memcpy(entry.itemBytes.data(), raw.item, entry.itemLength);

        page.entries.push_back(std::move(entry));
    }
    return true;
}

bool ReadResult(const std::uint8_t* data, std::size_t size, GameLogic::Items::AuctionResult& result)
{
    if (data == nullptr || size < sizeof(Result))
        return false;

    Result raw;
    std::memcpy(&raw, data, sizeof(Result));
    result.action = static_cast<GameLogic::Items::AuctionAction>(raw.action);
    result.done = raw.done != 0;
    return true;
}

Request MakeRequest(GameLogic::Items::AuctionAction action, std::uint8_t arg1, std::uint8_t arg2,
                    std::int64_t value, std::uint32_t price)
{
    Request request{};
    request.type = 0xC1;
    request.size = sizeof(Request);
    request.headCode = HeadCode;
    request.subCode = RequestSubCode;
    request.action = static_cast<std::uint8_t>(action);
    request.arg1 = arg1;
    request.arg2 = arg2;
    request.value = value;
    request.price = price;
    return request;
}

std::vector<std::uint8_t> MakeSearchRequest(GameLogic::Items::AuctionCategory kind, std::uint8_t currency,
                                            std::uint16_t page, GameLogic::Items::AuctionQuality quality,
                                            const std::optional<std::vector<std::uint16_t>>& itemTypes,
                                            GameLogic::Items::AuctionSort sort)
{
    constexpr std::size_t Types = GameLogic::Items::AuctionSearchLength;
    static_assert(Types == sizeof(Request) + 3, "the quality, the order and the count follow the request");
    const std::size_t count =
        itemTypes ? std::min(itemTypes->size(), GameLogic::Items::AuctionMaximumSearchTypes) : 0;
    std::vector<std::uint8_t> data(Types + count * 2);
    const Request request = MakeRequest(GameLogic::Items::AuctionAction::Browse, static_cast<std::uint8_t>(kind),
                                        currency, page, 0);
    std::memcpy(data.data(), &request, sizeof(Request));
    data[1] = static_cast<std::uint8_t>(data.size());
    data[sizeof(Request)] = static_cast<std::uint8_t>(quality);
    data[sizeof(Request) + 1] = static_cast<std::uint8_t>(sort);
    data[sizeof(Request) + 2] = itemTypes ? static_cast<std::uint8_t>(count) : NoNameSearch;
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint16_t type = (*itemTypes)[i];
        data[Types + i * 2] = static_cast<std::uint8_t>(type & 0xFF);
        data[Types + 1 + i * 2] = static_cast<std::uint8_t>(type >> 8);
    }
    return data;
}
} // namespace Network::Auction
