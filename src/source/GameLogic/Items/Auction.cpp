#include "GameLogic/Items/Auction.h"

#include <algorithm>
#include <utility>

namespace GameLogic::Items
{
namespace
{
constexpr std::array<std::string_view, 11> CurrencyNames = {
    "Zen",      "Bless",    "Soul",        "Life",        "Chaos", "Creation",
    "Guardian", "Harmony",  "Gemstone",    "Pedra Menor", "Pedra Maior",
};

constexpr std::array<std::uint8_t, 12> FilterCurrencyOrder = {
    AuctionCurrencyAny, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
};

AuctionPage s_CurrentAuctionPage;
unsigned s_CurrentAuctionPageVersion = 0;

AuctionResult s_LastAuctionResult;
unsigned s_CurrentAuctionResultVersion = 0;
} // namespace

std::string_view CurrencyName(std::uint8_t currency)
{
    if (currency == AuctionCurrencyAny)
        return "Todas";
    if (currency < CurrencyNames.size())
        return CurrencyNames[currency];
    return {};
}

std::string_view PriceCurrencyName(std::uint8_t currency)
{
    if (currency == AuctionCurrencyZen)
        return "zen";
    return CurrencyName(currency);
}

std::uint8_t StepFilterCurrency(std::uint8_t current, int delta)
{
    const auto it = std::find(FilterCurrencyOrder.begin(), FilterCurrencyOrder.end(), current);
    const int count = static_cast<int>(FilterCurrencyOrder.size());
    const int index = it != FilterCurrencyOrder.end()
        ? static_cast<int>(std::distance(FilterCurrencyOrder.begin(), it))
        : 0;
    const int next = ((index + delta) % count + count) % count;
    return FilterCurrencyOrder[static_cast<std::size_t>(next)];
}

std::uint8_t StepSellCurrency(std::uint8_t current, int delta)
{
    const int count = static_cast<int>(AuctionCurrencyMax) + 1;
    const int index = current <= AuctionCurrencyMax ? static_cast<int>(current) : 0;
    const int next = ((index + delta) % count + count) % count;
    return static_cast<std::uint8_t>(next);
}

AuctionCategory StepKind(AuctionCategory current, int delta)
{
    const int index = static_cast<int>(current) < AuctionCategoryCount ? static_cast<int>(current) : 0;
    const int next = ((index + delta) % AuctionCategoryCount + AuctionCategoryCount) % AuctionCategoryCount;
    return static_cast<AuctionCategory>(next);
}

AuctionQuality StepQuality(AuctionQuality current, int delta)
{
    const int index = static_cast<int>(current) < AuctionQualityCount ? static_cast<int>(current) : 0;
    const int next = ((index + delta) % AuctionQualityCount + AuctionQualityCount) % AuctionQualityCount;
    return static_cast<AuctionQuality>(next);
}

std::wstring FoldSearchText(std::wstring_view text)
{
    // Latin-1 0xC0 .. 0xFF without their accents, lower case (0xD7 and 0xF7 are signs).
    static constexpr wchar_t Latin1[] = L"aaaaaaaceeeeiiiidnooooo\u00d7ouuuuyts"
                                        L"aaaaaaaceeeeiiiidnooooo\u00f7ouuuuyty";
    std::wstring folded;
    folded.reserve(text.size());
    for (wchar_t ch : text)
    {
        if (ch >= L'A' && ch <= L'Z')
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        else if (ch >= 0xC0 && ch <= 0xFF && ch != 0xD7 && ch != 0xF7)
            ch = Latin1[ch - 0xC0];
        folded.push_back(ch);
    }
    const auto first = folded.find_first_not_of(L' ');
    if (first == std::wstring::npos)
        return {};
    const auto last = folded.find_last_not_of(L' ');
    return folded.substr(first, last - first + 1);
}

std::vector<std::uint16_t> MatchItemTypes(std::wstring_view query, int typeCount,
                                          const std::function<std::wstring(int)>& nameOf, std::size_t limit)
{
    std::vector<std::uint16_t> types;
    const std::wstring folded = FoldSearchText(query);
    if (folded.empty())
        return types;
    for (int type = 0; type < typeCount && types.size() < limit; ++type)
    {
        const std::wstring name = nameOf(type);
        if (!name.empty() && FoldSearchText(name).find(folded) != std::wstring::npos)
            types.push_back(static_cast<std::uint16_t>(type));
    }
    return types;
}

std::wstring SellerName(const AuctionEntry& entry)
{
    std::wstring name;
    name.reserve(entry.seller.size());
    for (const char ch : entry.seller)
        name.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
    return name;
}

std::string FormatTimeLeft(std::int32_t secondsLeft)
{
    if (secondsLeft <= 0)
        return "0m";

    const std::int32_t days = secondsLeft / 86400;
    const std::int32_t hours = (secondsLeft % 86400) / 3600;
    const std::int32_t minutes = (secondsLeft % 3600) / 60;

    if (days > 0)
        return std::to_string(days) + "d " + std::to_string(hours) + "h";
    if (hours > 0)
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    return std::to_string(std::max<std::int32_t>(1, minutes)) + "m";
}

const AuctionPage& CurrentAuctionPage()
{
    return s_CurrentAuctionPage;
}

unsigned CurrentAuctionPageVersion()
{
    return s_CurrentAuctionPageVersion;
}

void SetCurrentAuctionPage(AuctionPage page)
{
    s_CurrentAuctionPage = std::move(page);
    ++s_CurrentAuctionPageVersion;
}

const AuctionResult& LastAuctionResult()
{
    return s_LastAuctionResult;
}

unsigned CurrentAuctionResultVersion()
{
    return s_CurrentAuctionResultVersion;
}

void SetLastAuctionResult(AuctionResult result)
{
    s_LastAuctionResult = result;
    ++s_CurrentAuctionResultVersion;
}
} // namespace GameLogic::Items
