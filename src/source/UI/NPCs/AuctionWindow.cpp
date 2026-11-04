#include "stdafx.h"
#include "UI/NPCs/AuctionWindow.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/Auction.h"
#include "Network/Server/AuctionPacket.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Tooltip/LegacyTextListTooltip.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"
#include "UI/RmlBridge/RmlWindowClose.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cwctype>
#include <optional>
#include <span>
#include <string>
#include <utility>

using namespace mu::ui::window;
using GameLogic::Items::AuctionAction;
using GameLogic::Items::AuctionCategory;
using GameLogic::Items::AuctionEntry;
using GameLogic::Items::AuctionPage;
using GameLogic::Items::AuctionRowsPerPage;
using GameLogic::Items::AuctionSort;
using GameLogic::Items::AuctionView;

namespace
{
// How long a request waits for the server's answer before the buttons unlock: a page answers
// Comprar and Meus itens, which the server may refuse with a message only; a result answers a
// sale, a purchase, a return or a taking of zen, which write and save on the server for longer.
constexpr auto PageTimeout = std::chrono::seconds(3);
constexpr auto ResultTimeout = std::chrono::seconds(15);

bool AnswersWithPage(AuctionAction action)
{
    return action == AuctionAction::Browse || action == AuctionAction::Mine;
}

// The widths of the texts of a row (auction.rcss), reference px: the item's name by tab, the price.
constexpr int BrowseNameWidth = 128;
constexpr int MineNameWidth = 168;
constexpr int SellNameWidth = 196;
constexpr int PriceWidth = 80;
// The name of the selected item in the panel of Vender.
constexpr int SellPanelNameWidth = 164;

// The shortest item data of an entry which is an item.
constexpr std::uint8_t MinimumItemLength = 5;

// How long the search field rests after the typing before the search goes.
constexpr auto SearchDelay = std::chrono::milliseconds(500);

// The most names the search field suggests.
constexpr std::size_t MaximumSuggestions = 6;

// The kinds of the sidebar, in the order a player looks for them.
constexpr std::array<AuctionCategory, GameLogic::Items::AuctionCategoryCount> SidebarKinds = {
    AuctionCategory::All,         AuctionCategory::Weapons, AuctionCategory::Shields,
    AuctionCategory::Armor,       AuctionCategory::Wings,   AuctionCategory::Accessories,
    AuctionCategory::Jewels,      AuctionCategory::Pets,    AuctionCategory::Others,
};

// The arrows of the sorted column (U+25B2, U+25BC), which the UI fonts have.
constexpr const char* ArrowUp = " \xE2\x96\xB2";
constexpr const char* ArrowDown = " \xE2\x96\xBC";

std::wstring GroupedNumber(std::int64_t value)
{
    wchar_t text[32] = {};
    ConvertGold(static_cast<double>(value), text);
    return text;
}

std::string Narrow(const std::wstring& text)
{
    return StringUtils::WideToNarrow(text.c_str());
}

// The short name of a currency: "Zen", "Bless", ... "Pedra Maior", or "Todas" for any.
std::wstring CurrencyName(std::uint8_t currency)
{
    if (currency == GameLogic::Items::AuctionCurrencyAny)
        return I18N::Game::AuctionCurrencyAll;
    if (currency == 9)
        return I18N::Game::AuctionCurrencyLowerStone;
    if (currency == 10)
        return I18N::Game::AuctionCurrencyHigherStone;
    return StringUtils::NarrowToWide(std::string(GameLogic::Items::CurrencyName(currency)));
}

// The unit after a price: "zen", "Bless", ... "Pedra Maior".
std::wstring PriceUnit(std::uint8_t currency)
{
    return currency == GameLogic::Items::AuctionCurrencyZen ? L"zen" : CurrencyName(currency);
}

// A price on one line: "1.500.000 zen", "10 Bless".
std::wstring PriceText(std::uint8_t currency, std::uint32_t price)
{
    return GroupedNumber(price) + L" " + PriceUnit(currency);
}

const wchar_t* KindName(AuctionCategory kind)
{
    switch (kind)
    {
    case AuctionCategory::Weapons:
        return I18N::Game::AuctionKindWeapons;
    case AuctionCategory::Shields:
        return I18N::Game::AuctionKindShields;
    case AuctionCategory::Armor:
        return I18N::Game::AuctionKindArmor;
    case AuctionCategory::Wings:
        return I18N::Game::AuctionKindWings;
    case AuctionCategory::Jewels:
        return I18N::Game::AuctionKindJewels;
    case AuctionCategory::Others:
        return I18N::Game::AuctionKindOthers;
    case AuctionCategory::Accessories:
        return I18N::Game::AuctionKindAccessories;
    case AuctionCategory::Pets:
        return I18N::Game::AuctionKindPets;
    default:
        return I18N::Game::AuctionKindAll;
    }
}

std::wstring QualityName(GameLogic::Items::AuctionQuality quality)
{
    using GameLogic::Items::AuctionQuality;
    switch (quality)
    {
    case AuctionQuality::Excellent:
        return I18N::Game::AuctionQualityExcellent;
    case AuctionQuality::Ancient:
        return I18N::Game::AuctionQualityAncient;
    case AuctionQuality::Socket:
        return I18N::Game::AuctionQualitySocket;
    case AuctionQuality::Any:
        return I18N::Game::AuctionQualityAny;
    default:
        break;
    }
    wchar_t text[32] = {};
    mu_swprintf(text, I18N::Game::AuctionQualityLevel, GameLogic::Items::AuctionQualityLevel(quality));
    return text;
}

std::wstring ItemTypeName(int type)
{
    wchar_t name[64] = {};
    GetItemName(type, 0, name);
    return name;
}

// The item types whose name in the client's language holds the search (or is it), for the server.
std::vector<std::uint16_t> ItemTypesNamed(const std::wstring& search, bool exact)
{
    return GameLogic::Items::MatchItemTypes(search, MAX_ITEM, ItemTypeName, GameLogic::Items::AuctionMaximumSearchTypes,
                                            exact);
}

std::wstring ItemName(const ITEM& item)
{
    return GetItemNameText(item.Type, item.Level, &item);
}

// The class of a name's color in auction.rcss: the color its tooltip writes it in.
const char* NameClass(const ITEM* item)
{
    if (item == nullptr)
        return "q-normal";
    switch (GetItemNameColor(*item))
    {
    case TEXT_COLOR_BLUE:
        return "q-blue";
    case TEXT_COLOR_YELLOW:
        return "q-yellow";
    case TEXT_COLOR_GREEN:
        return "q-exc";
    case TEXT_COLOR_GREEN_BLUE:
        return "q-ancient";
    case TEXT_COLOR_VIOLET:
        return "q-socket";
    case TEXT_COLOR_PURPLE:
    case TEXT_COLOR_REDPURPLE:
        return "q-purple";
    case TEXT_COLOR_ORANGE:
        return "q-orange";
    case TEXT_COLOR_RED:
    case TEXT_COLOR_DARKRED:
        return "q-red";
    case TEXT_COLOR_GRAY:
        return "q-gray";
    default:
        return "q-normal";
    }
}

float TextWidth(const std::wstring& text)
{
    g_pRenderText->SetFont(g_hFont);
    return static_cast<float>(g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx);
}

// RenderText() shrinks a text wider than its box to fit it: the size it drew `text` at.
float TextPxInBox(const std::wstring& text, int boxWidth)
{
    return UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Normal, TextWidth(text),
                                            static_cast<float>(boxWidth));
}

std::string ItemIconId(int row)
{
    return "auction_item_" + std::to_string(row);
}

constexpr const char* PreviewId = "auction_preview";

// The heads of the columns of Comprar, as auction.rml numbers them: auction_sort(0..2), sort_column.
constexpr int SortColumnItem = 0;
constexpr int SortColumnPrice = 1;
constexpr int SortColumnTime = 2;
constexpr const char* SearchFieldId = "search_field";
} // namespace

CAuctionWindow::CAuctionWindow()
{
    for (int i = 0; i < AuctionRowsPerPage; ++i)
    {
        m_RowTargets[static_cast<std::size_t>(i)] = std::make_unique<UI::Items::ItemCameraTarget>(
            [this, i](const Rml::Vector2f& offset, const Rml::Vector2f& size) { RenderRowItem(i, offset, size); });
    }
    m_PreviewTarget = std::make_unique<UI::Items::ItemCameraTarget>(
        [this](const Rml::Vector2f& offset, const Rml::Vector2f& size) { RenderPreviewItem(offset, size); });
}

CAuctionWindow::~CAuctionWindow()
{
    Release();
}

bool CAuctionWindow::Create(CManager* pNewUIMng)
{
    if (nullptr == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_AUCTION, this);

    BuildRmlUi();

    Show(false);

    return true;
}

void CAuctionWindow::Release()
{
    DisableRowIcons();
    m_PageItems.clear();
    m_SellItems.clear();
    m_InventoryKeys.clear();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

bool CAuctionWindow::UpdateMouseEvent()
{
    // The tabs, the lists and the buttons are RmlUi's (see Update()).
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CAuctionWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_AUCTION) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_AUCTION);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

bool CAuctionWindow::Update()
{
    const unsigned pageVersion = GameLogic::Items::CurrentAuctionPageVersion();
    if (m_ItemsPageVersion != pageVersion)
    {
        m_ItemsPageVersion = pageVersion;
        RebuildPageItems();
        const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
        // The page of a price check answers Vender only; Comprar asks for its own again. Late too:
        // one which came after the timeout mustn't become the filters of Comprar.
        m_PageIsPriceCheck = std::exchange(m_RequestIsPriceCheck, false) && page.view == AuctionView::Browse;
        if (m_PageIsPriceCheck)
        {
            ReadPriceCheck();
        }
        else if (page.view == AuctionView::Browse)
        {
            m_KindFilter = page.kindFilter;
            m_CurrencyFilter = page.currencyFilter;
            m_Sort = page.sort;
            m_ServerPage = page.page;
        }

        // A new page starts at its first part, or at its last when the pager went back to it; a
        // list which shrank (an item sold) keeps the part on screen as far as it can.
        if (!m_PageIsPriceCheck && m_Tab != Tab::Sell)
        {
            const int last = (GameLogic::Items::AuctionRowPages(page.entries.size()) - 1) * AuctionRowsPerPage;
            const bool showLast = std::exchange(m_ShowLastRows, false) && page.view == AuctionView::Browse;
            m_FirstRow = showLast ? last : std::min(m_FirstRow, last);
        }
    }

    // A result answers the request of its action; the window asks again for what it shows. A
    // result after its timeout only refreshes.
    const unsigned resultVersion = GameLogic::Items::CurrentAuctionResultVersion();
    const auto waited = std::chrono::steady_clock::now() - m_RequestTime;
    if (m_HandledResultVersion != resultVersion)
    {
        m_HandledResultVersion = resultVersion;
        if (!m_Busy || GameLogic::Items::LastAuctionResult().action == m_RequestAction)
        {
            m_Busy = false;
            m_ShowLastRows = false;
            if (IsVisible() && !m_QueuedRequest)
                RequestCurrentView();
        }
    }
    else if (m_Busy && (AnswersWithPage(m_RequestAction)
                            ? (pageVersion != m_RequestPageVersion || waited > PageTimeout)
                            : waited > ResultTimeout))
    {
        m_Busy = false;
        m_ShowLastRows = false;
    }
    // A price check which the server didn't answer shows nothing.
    if (!m_Busy && m_PriceCheck == PriceCheck::Waiting)
        m_PriceCheck = PriceCheck::None;

    // A confirm which came while a request waited goes once the window is free.
    if (!m_Busy && m_QueuedRequest && IsVisible())
    {
        const Network::Auction::Request request = *std::exchange(m_QueuedRequest, std::nullopt);
        // A sale only for the item which was confirmed, if the bag moved in the meantime.
        if (static_cast<AuctionAction>(request.action) != AuctionAction::List || SlotHolds(request.arg1, m_QueuedItemKey))
            Send(reinterpret_cast<const std::uint8_t*>(&request), sizeof(request));
    }

    if (IsVisible() && m_Tab == Tab::Sell && HasInventoryChanged())
        RebuildSellItems();
    if (!m_Busy && !m_QueuedRequest && IsVisible() && m_Tab == Tab::Sell && m_PriceCheck == PriceCheck::Wanted)
        RequestPriceCheck();

    HandleClicks();
    UpdateSearch();
    SyncRmlModel();

    if (!IsVisible() || !UI::RmlBridge::IsPointerOver(m_RmlView.Document()))
        m_HoveredRow = -1;
    if (IsVisible() && RowItem(m_HoveredRow) == nullptr)
        UI::Tooltip::HideLegacyTextList();

    return true;
}

bool CAuctionWindow::Render()
{
    // The frame, the tabs and the rows are RmlUi; the tooltip of the item under the mouse is the
    // native one, drawn every frame at its picture like the inventory does.
    const ITEM* item = RowItem(m_HoveredRow);
    Rml::Element* icon = m_RmlView.Document() != nullptr
                             ? m_RmlView.Document()->GetElementById(ItemIconId(m_HoveredRow))
                             : nullptr;
    Rml::Vector2f point;
    if (item != nullptr && icon != nullptr && UI::RmlBridge::DrawnTopLeft(*icon, point))
        ::RenderItemInfoAtPx(point.x, point.y, item, false, 0, true);
    return true;
}

void CAuctionWindow::OpeningProcess()
{
    m_Tab = Tab::Browse;
    m_SellCurrency = GameLogic::Items::AuctionCurrencyZen;
    // Talking to Otto sends the first page of everything, the newest first.
    m_KindFilter = AuctionCategory::All;
    m_CurrencyFilter = GameLogic::Items::AuctionCurrencyAny;
    m_Quality = GameLogic::Items::AuctionQuality::Any;
    m_Sort = AuctionSort::Newest;
    m_Search.clear();
    m_SearchField.clear();
    m_SearchExact = false;
    m_Suggestions.clear();
    m_SuggestionsFor.clear();
    m_ShownSuggestionsFor = L"\x1";
    m_ShowSuggestions = false;
    if (m_RmlView.Document())
    {
        m_RmlView.GetModel().searchText.clear();
        m_RmlView.MarkDirty("search_text");
    }
    m_FirstRow = 0;
    m_ShowLastRows = false;
    m_SellKey = 0;
    m_PriceCheck = PriceCheck::None;
    m_PageIsPriceCheck = false;
    m_Busy = false;
    m_RequestIsPriceCheck = false;
    m_QueuedRequest.reset();
    m_HandledResultVersion = GameLogic::Items::CurrentAuctionResultVersion();
    m_HoveredRow = -1;
    m_Shown.firstRow = -1;
    m_InventoryKeys.clear();
}

void CAuctionWindow::ClosingProcess()
{
    m_HoveredRow = -1;
    m_QueuedRequest.reset();
    DisableRowIcons();
    UI::Tooltip::HideLegacyTextList();
    SocketClient->ToGameServer()->SendCloseNpcRequest();
}

float CAuctionWindow::GetLayerDepth()
{
    return 5.0f;
}

bool CAuctionWindow::TakesTypingFrom(const Rml::ElementDocument* document) const
{
    return document != nullptr && document == m_RmlView.Document();
}

bool CAuctionWindow::SearchFieldHasFocus() const
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr || document->GetContext() == nullptr)
        return false;
    Rml::Element* field = document->GetElementById(SearchFieldId);
    return field != nullptr && document->GetContext()->GetFocusElement() == field;
}

void CAuctionWindow::UpdateSearch()
{
    if (!IsVisible() || !m_RmlView.Document())
        return;

    const auto now = std::chrono::steady_clock::now();
    const Rml::String& field = m_RmlView.GetModel().searchText;
    if (field != m_SearchField)
    {
        m_SearchField = field;
        m_SearchChangedAt = now;
        m_SearchExact = false;
    }

    std::wstring search = GameLogic::Items::FoldSearchText(StringUtils::NarrowToWide(m_SearchField));
    if (search.size() < GameLogic::Items::AuctionMinimumSearchLength)
        search.clear();

    // The names to suggest, found again only when the text changes.
    if (search != m_SuggestionsFor)
    {
        m_SuggestionsFor = search;
        m_Suggestions = search.empty() ? std::vector<std::wstring>{}
                                       : GameLogic::Items::SuggestItemNames(search, MAX_ITEM, ItemTypeName,
                                                                            MaximumSuggestions);
    }
    const bool onlyItself =
        m_Suggestions.size() == 1 && GameLogic::Items::FoldSearchText(m_Suggestions.front()) == search;
    m_ShowSuggestions =
        m_Tab == Tab::Browse && !m_Suggestions.empty() && !m_SearchExact && !onlyItself && SearchFieldHasFocus();

    if ((search == m_Search && m_SearchExact == m_SearchTypesExact) || m_Tab != Tab::Browse || m_Busy ||
        now - m_SearchChangedAt < SearchDelay)
    {
        return;
    }

    m_Search = std::move(search);
    m_FirstRow = 0;
    RequestBrowse(0);
}

void CAuctionWindow::SetSearch(const std::wstring& text, bool exact)
{
    const std::string narrow = Narrow(text);
    if (m_RmlView.Document())
    {
        m_RmlView.GetModel().searchText = narrow;
        m_RmlView.MarkDirty("search_text");
        if (Rml::Element* field = m_RmlView.Document()->GetElementById(SearchFieldId))
            field->Blur();
    }
    m_SearchField = narrow;
    m_SearchExact = exact && !text.empty();
    m_ShowSuggestions = false;
    std::wstring search = GameLogic::Items::FoldSearchText(text);
    if (search.size() < GameLogic::Items::AuctionMinimumSearchLength)
        search.clear();
    m_Search = std::move(search);
    m_FirstRow = 0;
    PlayBuffer(SOUND_CLICK01);
    RequestBrowse(0);
}

CAuctionWindow::Clicks CAuctionWindow::TakeClicks()
{
    Clicks clicks;
    clicks.tab = std::exchange(m_PendingTab, -1);
    clicks.kind = std::exchange(m_PendingKind, -1);
    clicks.qualityDelta = std::exchange(m_PendingQualityDelta, 0);
    clicks.currencyDelta = std::exchange(m_PendingCurrencyDelta, 0);
    clicks.pageDelta = std::exchange(m_PendingPageDelta, 0);
    clicks.row = std::exchange(m_PendingRowClick, -1);
    clicks.sortColumn = std::exchange(m_PendingSortColumn, -1);
    clicks.suggestion = std::exchange(m_PendingSuggestion, -1);
    clicks.claimZen = std::exchange(m_PendingClaimZen, false);
    clicks.clearFilters = std::exchange(m_PendingClearFilters, false);
    clicks.clearSearch = std::exchange(m_PendingClearSearch, false);
    clicks.searchNow = std::exchange(m_PendingSearchNow, false);
    clicks.list = std::exchange(m_PendingList, false);
    return clicks;
}

void CAuctionWindow::HandleClicks()
{
    // While a request waits, the clicks wait for it (a price check follows each pick on Vender).
    if (m_Busy && IsVisible())
        return;
    const Clicks clicks = TakeClicks();
    if (!IsVisible())
        return;

    if (SwitchTab(clicks.tab))
        return;

    if (clicks.pageDelta != 0)
    {
        StepRows(clicks.pageDelta);
        return;
    }

    if (m_Tab == Tab::Browse && HandleBrowseClicks(clicks))
        return;

    if (m_Tab == Tab::Sell && HandleSellClicks(clicks))
        return;

    if (m_Tab == Tab::Mine && clicks.claimZen && PageFitsTab() &&
        GameLogic::Items::CurrentAuctionPage().zenToClaim > 0)
    {
        SendRequest(AuctionAction::ClaimZen);
        return;
    }

    if (clicks.row >= 0)
        HandleRowClick(clicks.row);
}

bool CAuctionWindow::SwitchTab(int tab)
{
    if (tab < 0 || tab > 2 || static_cast<Tab>(tab) == m_Tab)
        return false;

    m_Tab = static_cast<Tab>(tab);
    m_HoveredRow = -1;
    m_FirstRow = 0;
    m_ShowSuggestions = false;
    if (m_Tab == Tab::Browse)
        RequestBrowse(m_ServerPage);
    else if (m_Tab == Tab::Mine)
        SendRequest(AuctionAction::Mine);
    else
    {
        RebuildSellItems();
        m_PriceCheck = SelectedSellItem() != nullptr ? PriceCheck::Wanted : PriceCheck::None;
    }
    PlayBuffer(SOUND_CLICK01);
    return true;
}

bool CAuctionWindow::HandleBrowseClicks(const Clicks& clicks)
{
    if (clicks.suggestion >= 0 && clicks.suggestion < static_cast<int>(m_Suggestions.size()))
    {
        SetSearch(m_Suggestions[static_cast<std::size_t>(clicks.suggestion)], true);
        return true;
    }
    if (clicks.clearSearch)
    {
        SetSearch(L"", false);
        return true;
    }
    if (clicks.searchNow)
    {
        // Enter: the search goes without waiting for the typing to rest.
        m_SearchChangedAt = {};
        m_ShowSuggestions = false;
        if (Rml::Element* field = m_RmlView.Document()->GetElementById(SearchFieldId))
            field->Blur();
        return true;
    }
    if (clicks.clearFilters)
    {
        m_KindFilter = AuctionCategory::All;
        m_CurrencyFilter = GameLogic::Items::AuctionCurrencyAny;
        m_Quality = GameLogic::Items::AuctionQuality::Any;
        m_Sort = AuctionSort::Newest;
        SetSearch(L"", false);
        return true;
    }

    bool changed = false;
    if (clicks.kind >= 0 && clicks.kind < GameLogic::Items::AuctionCategoryCount &&
        static_cast<AuctionCategory>(clicks.kind) != m_KindFilter)
    {
        m_KindFilter = static_cast<AuctionCategory>(clicks.kind);
        changed = true;
    }
    if (clicks.qualityDelta != 0)
    {
        m_Quality = GameLogic::Items::StepQuality(m_Quality, clicks.qualityDelta);
        changed = true;
    }
    if (clicks.currencyDelta != 0)
    {
        m_CurrencyFilter = GameLogic::Items::StepFilterCurrency(m_CurrencyFilter, clicks.currencyDelta);
        changed = true;
    }
    if (clicks.sortColumn >= 0)
    {
        const AuctionSort sort = clicks.sortColumn == SortColumnPrice ? GameLogic::Items::NextPriceSort(m_Sort)
                                 : clicks.sortColumn == SortColumnTime ? AuctionSort::EndingSoon
                                                                       : AuctionSort::Newest;
        changed = changed || sort != m_Sort;
        m_Sort = sort;
    }
    if (!changed)
        return false;

    m_FirstRow = 0;
    PlayBuffer(SOUND_CLICK01);
    RequestBrowse(0);
    return true;
}

bool CAuctionWindow::HandleSellClicks(const Clicks& clicks)
{
    if (clicks.currencyDelta != 0)
    {
        m_SellCurrency = GameLogic::Items::StepSellCurrency(m_SellCurrency, clicks.currencyDelta);
        if (SelectedSellItem() != nullptr)
            m_PriceCheck = PriceCheck::Wanted;
        return true;
    }
    if (clicks.list)
    {
        ListSelectedItem();
        return true;
    }
    return false;
}

void CAuctionWindow::StepRows(int delta)
{
    const int count = RowCount();
    const int next = m_FirstRow + delta * AuctionRowsPerPage;
    if (next >= 0 && next < count)
    {
        m_FirstRow = next;
        m_HoveredRow = -1;
        return;
    }

    // Past the parts of this page of the server: its neighbour, for the items for sale.
    if (m_Tab != Tab::Browse || !PageFitsTab())
        return;
    const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
    if (delta < 0 && page.page > 0)
    {
        RequestBrowse(static_cast<std::uint16_t>(page.page - 1));
        m_ShowLastRows = m_Busy;
    }
    else if (delta > 0 && page.page + 1 < page.pages)
    {
        m_FirstRow = 0;
        RequestBrowse(static_cast<std::uint16_t>(page.page + 1));
    }
}

void CAuctionWindow::HandleRowClick(int row)
{
    if (RowItem(row) == nullptr)
        return;

    const int index = m_FirstRow + row;
    if (m_Tab == Tab::Sell)
    {
        // A click selects the item; its panel shows the lowest price and Anunciar.
        SelectSellItem(m_SellItems[static_cast<std::size_t>(index)].item.Key);
        return;
    }

    if (g_pGenericConfirmDialog == nullptr)
        return;
    const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    wchar_t line[256] = {};

    const AuctionEntry& entry = page.entries[static_cast<std::size_t>(index)];
    const ITEM& item = m_PageItems[static_cast<std::size_t>(index)];
    const std::int64_t id = entry.id;
    if (m_Tab == Tab::Browse)
    {
        if (entry.IsMine())
            return;
        mu_swprintf(line, I18N::Game::AuctionConfirmBuy, ItemName(item).c_str(),
                    PriceText(entry.currency, entry.price).c_str(), GroupedNumber(page.buyerFee).c_str());
        cfg.tag = "auction_buy";
        const std::uint8_t currency = entry.currency;
        const std::uint32_t price = entry.price;
        cfg.onPrimary = [this, id, currency, price]() { SendRequest(AuctionAction::Buy, 0, currency, id, price); };
    }
    else
    {
        if (entry.IsPending())
            return;
        const bool ended = entry.IsEnded();
        if (ended)
            mu_swprintf(line, I18N::Game::AuctionConfirmReturnFree, ItemName(item).c_str());
        else
            mu_swprintf(line, I18N::Game::AuctionConfirmReturnFee, ItemName(item).c_str(),
                        GroupedNumber(page.cancelFee).c_str());
        cfg.tag = "auction_return";
        cfg.onPrimary = [this, id, ended]() { SendRequest(AuctionAction::Return, ended ? 1 : 0, 0, id, 0); };
    }
    cfg.item3D = item;

    cfg.lines.push_back({line});
    PlayBuffer(SOUND_CLICK01);
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

void CAuctionWindow::ListSelectedItem()
{
    const SellItem* sell = SelectedSellItem();
    if (g_pGenericConfirmDialog == nullptr || sell == nullptr)
        return;

    const std::uint8_t currency = m_SellCurrency;
    const std::uint32_t maxPrice = GameLogic::Items::MaxAuctionPrice(currency);
    wchar_t line[256] = {};
    mu_swprintf(line, I18N::Game::AuctionConfirmSell, ItemName(sell->item).c_str(), CurrencyName(currency).c_str());

    GenericDialogConfig cfg;
    cfg.showCancel = true;
    GenericDialogConfig::InputField input;
    input.numericOnly = true;
    input.maxLength = currency == GameLogic::Items::AuctionCurrencyZen ? 10 : 4;
    cfg.input = std::move(input);
    cfg.item3D = sell->item;
    cfg.tag = "auction_list";
    const std::uint8_t slot = sell->serverSlot;
    const DWORD key = sell->item.Key;
    cfg.onPrimary = [this, slot, key, currency, maxPrice]()
    {
        std::uint64_t value = 0;
        for (const wchar_t ch : g_pGenericConfirmDialog->GetInputText())
        {
            if (!std::iswdigit(ch))
                continue;
            value = value * 10 + static_cast<std::uint64_t>(ch - L'0');
            if (value > maxPrice)
                break;
        }
        if (value == 0 || value > maxPrice)
        {
            g_pGenericConfirmDialog->KeepOpen();
            return;
        }
        // The server sells what is in the slot: only the item of the dialog, if it's still there.
        if (!SlotHolds(slot, key))
            return;
        m_QueuedItemKey = key;
        SendRequest(AuctionAction::List, slot, currency, 0, static_cast<std::uint32_t>(value));
    };
    cfg.lines.push_back({line});
    PlayBuffer(SOUND_CLICK01);
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

void CAuctionWindow::SendRequest(AuctionAction action, std::uint8_t arg1, std::uint8_t arg2, std::int64_t value,
                                 std::uint32_t price)
{
    if (SocketClient == nullptr)
        return;

    const Network::Auction::Request request = Network::Auction::MakeRequest(action, arg1, arg2, value, price);
    if (m_Busy)
    {
        // A confirmed sale, purchase or return waits for the request on its way, the last one only.
        if (!AnswersWithPage(action))
            m_QueuedRequest = request;
        return;
    }

    Send(reinterpret_cast<const std::uint8_t*>(&request), sizeof(request));
}

void CAuctionWindow::Send(const std::uint8_t* data, std::size_t size)
{
    if (m_Busy || SocketClient == nullptr || size < sizeof(Network::Auction::Request))
        return;

    SocketClient->Send(reinterpret_cast<const BYTE*>(data), static_cast<int>(size));
    m_Busy = true;
    m_ShowLastRows = false;
    m_RequestIsPriceCheck = false;
    m_RequestAction = static_cast<AuctionAction>(data[offsetof(Network::Auction::Request, action)]);
    m_RequestPageVersion = GameLogic::Items::CurrentAuctionPageVersion();
    m_RequestTime = std::chrono::steady_clock::now();
}

void CAuctionWindow::RequestBrowse(std::uint16_t page)
{
    std::optional<std::vector<std::uint16_t>> itemTypes;
    if (!m_Search.empty())
    {
        if (m_SearchTypesFor != m_Search || m_SearchTypesExact != m_SearchExact)
        {
            m_SearchTypes = ItemTypesNamed(m_Search, m_SearchExact);
            m_SearchTypesFor = m_Search;
            m_SearchTypesExact = m_SearchExact;
        }
        itemTypes = m_SearchTypes;
    }
    else
    {
        m_SearchTypesExact = m_SearchExact;
    }
    const std::vector<std::uint8_t> request =
        Network::Auction::MakeSearchRequest(m_KindFilter, m_CurrencyFilter, page, m_Quality, itemTypes, m_Sort);
    Send(request.data(), request.size());
}

void CAuctionWindow::RequestCurrentView()
{
    // After a sale from the Vender tab the window stays there and asks for the account's items,
    // whose count its header shows.
    if (m_Tab == Tab::Browse)
        RequestBrowse(m_ServerPage);
    else
        SendRequest(AuctionAction::Mine);
}

void CAuctionWindow::RequestPriceCheck()
{
    const SellItem* sell = SelectedSellItem();
    if (sell == nullptr || sell->item.Type < 0)
    {
        m_PriceCheck = PriceCheck::None;
        return;
    }
    const std::vector<std::uint16_t> types{static_cast<std::uint16_t>(sell->item.Type)};
    const std::vector<std::uint8_t> request = Network::Auction::MakeSearchRequest(
        AuctionCategory::All, m_SellCurrency, 0, GameLogic::Items::AuctionQuality::Any, types, AuctionSort::PriceLow);
    Send(request.data(), request.size());
    if (m_Busy)
    {
        m_RequestIsPriceCheck = true;
        m_PriceCheck = PriceCheck::Waiting;
    }
}

void CAuctionWindow::ReadPriceCheck()
{
    if (m_PriceCheck != PriceCheck::Waiting)
        return;
    // The cheapest of the others: the account's own items don't tell the market's price.
    const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
    const auto it = std::find_if(page.entries.begin(), page.entries.end(),
                                 [this](const AuctionEntry& entry)
                                 { return !entry.IsMine() && entry.currency == m_SellCurrency; });
    if (it != page.entries.end())
    {
        m_LowestPrice = it->price;
        m_PriceCheck = PriceCheck::Found;
    }
    else
    {
        // A page of the account's own only, with more after it: the price of the others is unknown.
        m_PriceCheck = page.total > static_cast<std::int32_t>(page.entries.size()) ? PriceCheck::None
                                                                                   : PriceCheck::NoneForSale;
    }
}

void CAuctionWindow::SelectSellItem(DWORD key)
{
    if (key == m_SellKey)
        return;
    m_SellKey = key;
    m_PriceCheck = SelectedSellItem() != nullptr ? PriceCheck::Wanted : PriceCheck::None;
    PlayBuffer(SOUND_CLICK01);
}

const CAuctionWindow::SellItem* CAuctionWindow::SelectedSellItem() const
{
    if (m_SellKey == 0)
        return nullptr;
    const auto it = std::find_if(m_SellItems.begin(), m_SellItems.end(),
                                 [this](const SellItem& sell) { return sell.item.Key == m_SellKey; });
    return it != m_SellItems.end() ? &*it : nullptr;
}

bool CAuctionWindow::PageFitsTab() const
{
    const AuctionView view = GameLogic::Items::CurrentAuctionPage().view;
    return (m_Tab == Tab::Browse && view == AuctionView::Browse && !m_PageIsPriceCheck) ||
           (m_Tab == Tab::Mine && view == AuctionView::Mine);
}

int CAuctionWindow::RowCount() const
{
    if (m_Tab == Tab::Sell)
        return static_cast<int>(m_SellItems.size());
    if (!PageFitsTab())
        return 0;
    return static_cast<int>(std::min(GameLogic::Items::CurrentAuctionPage().entries.size(), m_PageItems.size()));
}

void CAuctionWindow::RebuildPageItems()
{
    const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
    m_PageItems.assign(page.entries.size(), ITEM{});
    for (std::size_t i = 0; i < page.entries.size(); ++i)
    {
        const AuctionEntry& entry = page.entries[i];
        ITEM& item = m_PageItems[i];
        item.Type = -1;
        // Shorter than the item data's fixed head (type, level, durability, options, excellent): none.
        if (entry.itemLength < MinimumItemLength)
            continue;
        const ItemCreationParams params =
            ParseItemData(std::span<const BYTE>(entry.itemBytes.data(), entry.itemLength));
        FillItemFromParameters(item, params);
    }
}

bool CAuctionWindow::SlotHolds(std::uint8_t serverSlot, DWORD key) const
{
    CInventoryCtrl* ctrl = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
    if (ctrl == nullptr)
        return false;

    const std::size_t count = ctrl->GetNumberOfItems();
    for (std::size_t i = 0; i < count; ++i)
    {
        const ITEM* item = ctrl->GetItem(static_cast<int>(i));
        if (item != nullptr && item->x < GameLogic::Items::MainInventoryColumns &&
            item->y < GameLogic::Items::MainInventoryRows &&
            GameLogic::Items::MainInventoryServerSlot(item->x, item->y) == serverSlot)
        {
            return item->Key == key;
        }
    }
    return false;
}

bool CAuctionWindow::HasInventoryChanged() const
{
    CInventoryCtrl* ctrl = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
    if (ctrl == nullptr)
        return !m_InventoryKeys.empty();

    const std::size_t count = ctrl->GetNumberOfItems();
    if (count != m_InventoryKeys.size())
        return true;
    for (std::size_t i = 0; i < count; ++i)
    {
        const ITEM* item = ctrl->GetItem(static_cast<int>(i));
        if ((item != nullptr ? item->Key : 0) != m_InventoryKeys[i])
            return true;
    }
    return false;
}

void CAuctionWindow::RebuildSellItems()
{
    m_SellItems.clear();
    m_InventoryKeys.clear();

    CInventoryCtrl* ctrl = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
    if (ctrl != nullptr)
    {
        const std::size_t count = ctrl->GetNumberOfItems();
        for (std::size_t i = 0; i < count; ++i)
        {
            const ITEM* item = ctrl->GetItem(static_cast<int>(i));
            m_InventoryKeys.push_back(item != nullptr ? item->Key : 0);
            if (item == nullptr || item->x >= GameLogic::Items::MainInventoryColumns ||
                item->y >= GameLogic::Items::MainInventoryRows)
            {
                continue;
            }
            m_SellItems.push_back({GameLogic::Items::MainInventoryServerSlot(item->x, item->y), *item});
        }
        std::sort(m_SellItems.begin(), m_SellItems.end(),
                  [](const SellItem& a, const SellItem& b) { return a.serverSlot < b.serverSlot; });
    }

    // The selected item left the bag (sold, dropped, moved out): the panel empties.
    if (m_SellKey != 0 && SelectedSellItem() == nullptr)
    {
        m_SellKey = 0;
        m_PriceCheck = PriceCheck::None;
    }

    const int last = (GameLogic::Items::AuctionRowPages(m_SellItems.size()) - 1) * AuctionRowsPerPage;
    if (m_Tab == Tab::Sell)
        m_FirstRow = std::min(m_FirstRow, last);
    ++m_SellRevision;
}

const ITEM* CAuctionWindow::RowItem(int row) const
{
    if (row < 0 || row >= AuctionRowsPerPage)
        return nullptr;
    const int index = m_FirstRow + row;
    if (index >= RowCount())
        return nullptr;

    const ITEM* item = m_Tab == Tab::Sell ? &m_SellItems[static_cast<std::size_t>(index)].item
                                          : &m_PageItems[static_cast<std::size_t>(index)];
    return item->Type >= 0 ? item : nullptr;
}

void CAuctionWindow::RenderRowItem(int row, const Rml::Vector2f& offset, const Rml::Vector2f& size)
{
    if (const ITEM* item = RowItem(row))
    {
        RenderItem3DWithHover(offset.x, offset.y, size.x, size.y, item->Type, item->Level, item->ExcellentFlags,
                              item->AncientDiscriminator, m_HoveredRow == row);
    }
}

void CAuctionWindow::RenderPreviewItem(const Rml::Vector2f& offset, const Rml::Vector2f& size)
{
    if (const SellItem* sell = SelectedSellItem())
    {
        const ITEM& item = sell->item;
        RenderItem3DWithHover(offset.x, offset.y, size.x, size.y, item.Type, item.Level, item.ExcellentFlags,
                              item.AncientDiscriminator, false);
    }
}

void CAuctionWindow::SyncRowIcons()
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return;

    for (int i = 0; i < AuctionRowsPerPage; ++i)
    {
        Rml::Element* icon = document->GetElementById(ItemIconId(i));
        if (icon == nullptr)
            continue;
        const bool filled = RowItem(i) != nullptr;
        icon->SetClass("hidden", !filled);
        m_RowTargets[static_cast<std::size_t>(i)]->Sync(icon, IsVisible() && filled);
    }

    if (Rml::Element* preview = document->GetElementById(PreviewId))
    {
        const bool shown = m_Tab == Tab::Sell && SelectedSellItem() != nullptr;
        preview->SetClass("hidden", !shown);
        m_PreviewTarget->Sync(preview, IsVisible() && shown);
    }
}

void CAuctionWindow::DisableRowIcons()
{
    for (auto& target : m_RowTargets)
        target->Disable();
    if (m_PreviewTarget)
        m_PreviewTarget->Disable();
}

void CAuctionWindow::BindRmlModel(Rml::DataModelConstructor& c, AuctionRmlModel& model)
{
    UI::RmlBridge::BindWindowClose(c, mu::ui::window::INTERFACE_AUCTION);
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("label_line_px", &model.labelLinePx);
    c.Bind("title", &model.title);
    c.Bind("tab_buy_text", &model.tabBuyText);
    c.Bind("tab_mine_text", &model.tabMineText);
    c.Bind("tab_sell_text", &model.tabSellText);
    c.Bind("active_tab", &model.activeTab);
    c.Bind("account_items_text", &model.accountItemsText);
    c.Bind("busy", &model.busy);

    c.Bind("category_title", &model.categoryTitle);
    c.Bind("currency_title", &model.currencyTitle);
    c.Bind("filter_currency_text", &model.filterCurrencyText);
    c.Bind("quality_title", &model.qualityTitle);
    c.Bind("quality_text", &model.qualityText);
    c.Bind("clear_filters_text", &model.clearFiltersText);
    c.Bind("can_clear_filters", &model.canClearFilters);
    c.Bind("search_text", &model.searchText);
    c.Bind("search_hint", &model.searchHint);
    c.Bind("show_suggestions", &model.showSuggestions);
    c.Bind("result_count_text", &model.resultCountText);
    c.Bind("column_item_text", &model.columnItemText);
    c.Bind("column_price_text", &model.columnPriceText);
    c.Bind("column_time_text", &model.columnTimeText);
    c.Bind("column_state_text", &model.columnStateText);
    c.Bind("sort_column", &model.sortColumn);

    c.Bind("empty_text", &model.emptyText);
    c.Bind("empty", &model.empty);
    c.Bind("page_text", &model.pageText);
    c.Bind("can_prev_page", &model.canPrevPage);
    c.Bind("can_next_page", &model.canNextPage);
    c.Bind("footer_text", &model.footerText);
    c.Bind("zen_to_claim_text", &model.zenToClaimText);
    c.Bind("claim_button_text", &model.claimButtonText);
    c.Bind("can_claim_zen", &model.canClaimZen);
    c.Bind("return_button_text", &model.returnButtonText);

    c.Bind("bag_title", &model.bagTitle);
    c.Bind("has_sell_item", &model.hasSellItem);
    c.Bind("sell_choose_text", &model.sellChooseText);
    c.Bind("sell_item_name", &model.sellItemName);
    c.Bind("sell_item_name_px", &model.sellItemNamePx);
    c.Bind("sell_item_class", &model.sellItemClass);
    c.Bind("sell_currency_text", &model.sellCurrencyText);
    c.Bind("lowest_title", &model.lowestTitle);
    c.Bind("lowest_text", &model.lowestText);
    c.Bind("lowest_found", &model.lowestFound);
    c.Bind("sell_fee_listing_text", &model.sellFeeListingText);
    c.Bind("sell_fee_days_text", &model.sellFeeDaysText);
    c.Bind("sell_fee_cancel_text", &model.sellFeeCancelText);
    c.Bind("list_button_text", &model.listButtonText);
    c.Bind("can_list", &model.canList);

    auto row = c.RegisterStruct<AuctionRow>();
    row.RegisterMember("index", &AuctionRow::index);
    row.RegisterMember("name", &AuctionRow::name);
    row.RegisterMember("name_px", &AuctionRow::namePx);
    row.RegisterMember("name_class", &AuctionRow::nameClass);
    row.RegisterMember("price", &AuctionRow::price);
    row.RegisterMember("price_px", &AuctionRow::pricePx);
    row.RegisterMember("currency", &AuctionRow::currency);
    row.RegisterMember("detail", &AuctionRow::detail);
    row.RegisterMember("time", &AuctionRow::time);
    row.RegisterMember("mine", &AuctionRow::mine);
    row.RegisterMember("ended", &AuctionRow::ended);
    row.RegisterMember("pending", &AuctionRow::pending);
    row.RegisterMember("clickable", &AuctionRow::clickable);
    row.RegisterMember("selected", &AuctionRow::selected);
    c.RegisterArray<std::vector<AuctionRow>>();
    c.Bind("rows", &model.rows);

    auto kindRow = c.RegisterStruct<AuctionKindRow>();
    kindRow.RegisterMember("kind", &AuctionKindRow::kind);
    kindRow.RegisterMember("label", &AuctionKindRow::label);
    kindRow.RegisterMember("selected", &AuctionKindRow::selected);
    c.RegisterArray<std::vector<AuctionKindRow>>();
    c.Bind("kinds", &model.kinds);

    auto suggestionRow = c.RegisterStruct<AuctionSuggestion>();
    suggestionRow.RegisterMember("index", &AuctionSuggestion::index);
    suggestionRow.RegisterMember("name", &AuctionSuggestion::name);
    c.RegisterArray<std::vector<AuctionSuggestion>>();
    c.Bind("suggestions", &model.suggestions);

    const auto intArgument = [](const Rml::VariantList& arguments, int fallback)
    { return arguments.size() == 1 ? arguments[0].Get<int>(fallback) : fallback; };
    c.BindEventCallback("auction_tab", [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingTab = intArgument(a, -1); });
    c.BindEventCallback("auction_kind", [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingKind = intArgument(a, -1); });
    c.BindEventCallback("auction_quality_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingQualityDelta = intArgument(a, 0); });
    c.BindEventCallback("auction_currency_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingCurrencyDelta = intArgument(a, 0); });
    c.BindEventCallback("auction_sort",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingSortColumn = intArgument(a, -1); });
    c.BindEventCallback("auction_page_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingPageDelta = intArgument(a, 0); });
    c.BindEventCallback("auction_row_click",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingRowClick = intArgument(a, -1); });
    c.BindEventCallback("auction_row_hover",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_HoveredRow = intArgument(a, -1); });
    c.BindEventCallback("auction_row_unhover", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_HoveredRow = -1; });
    c.BindEventCallback("auction_claim_zen", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingClaimZen = true; });
    c.BindEventCallback("auction_clear_filters", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingClearFilters = true; });
    c.BindEventCallback("auction_clear_search", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingClearSearch = true; });
    // A suggestion is taken on the press: the field loses the focus then, which hides the list.
    c.BindEventCallback("auction_suggestion",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingSuggestion = intArgument(a, -1); });
    c.BindEventCallback("auction_search_key", [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                        {
                            const auto key = static_cast<Rml::Input::KeyIdentifier>(
                                event.GetParameter<int>("key_identifier", 0));
                            if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
                                m_PendingSearchNow = true;
                        });
    c.BindEventCallback("auction_list", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingList = true; });

    model.title = Narrow(I18N::Game::AuctionTitle);
    model.tabBuyText = Narrow(I18N::Game::AuctionTabBuy);
    model.tabMineText = Narrow(I18N::Game::AuctionTabMine);
    model.tabSellText = Narrow(I18N::Game::AuctionTabSell);
    model.categoryTitle = Narrow(I18N::Game::AuctionCategoryTitle);
    model.currencyTitle = Narrow(I18N::Game::AuctionCurrencyTitle);
    model.qualityTitle = Narrow(I18N::Game::AuctionQualityTitle);
    model.clearFiltersText = Narrow(I18N::Game::AuctionClearFilters);
    model.columnStateText = Narrow(I18N::Game::AuctionColumnState);
    model.claimButtonText = Narrow(I18N::Game::AuctionClaimButton);
    model.returnButtonText = Narrow(I18N::Game::AuctionReturnButton);
    model.emptyText = Narrow(I18N::Game::AuctionEmpty);
    model.searchHint = Narrow(I18N::Game::AuctionSearchHint);
    model.bagTitle = Narrow(I18N::Game::AuctionBagTitle);
    model.sellChooseText = Narrow(I18N::Game::AuctionSellChoose);
    model.lowestTitle = Narrow(I18N::Game::AuctionLowestTitle);
    model.listButtonText = Narrow(I18N::Game::AuctionListButton);
    for (const AuctionCategory kind : SidebarKinds)
        model.kinds.push_back({static_cast<int>(kind), Narrow(KindName(kind)), kind == AuctionCategory::All});
}

void CAuctionWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void CAuctionWindow::SyncRmlModel()
{
    BuildRmlUi();
    if (!m_RmlView.Document())
        return;

    // Layer depth 5: over the HUD like the other panels.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
    {
        DisableRowIcons();
        return;
    }

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());
    auto& binder = m_RmlView.Binder();
    AuctionRmlModel& model = m_RmlView.GetModel();
    SyncField(binder, &AuctionRmlModel::boldTextPx, "bold_text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold));
    SyncField(binder, &AuctionRmlModel::labelLinePx, "label_line_px",
              CUIRenderTextSDLTtf::LineHeightPx(UI::Scaling::FontRole::Normal));
    SyncField(binder, &AuctionRmlModel::activeTab, "active_tab", static_cast<int>(m_Tab));
    SyncField(binder, &AuctionRmlModel::busy, "busy", m_Busy);

    // The suggestions change with the text of the field only.
    if (m_ShownSuggestionsFor != m_SuggestionsFor)
    {
        m_ShownSuggestionsFor = m_SuggestionsFor;
        model.suggestions.clear();
        for (std::size_t i = 0; i < m_Suggestions.size(); ++i)
            model.suggestions.push_back({static_cast<int>(i), Narrow(m_Suggestions[i])});
        m_RmlView.MarkDirty("suggestions");
    }
    SyncField(binder, &AuctionRmlModel::showSuggestions, "show_suggestions", m_ShowSuggestions);

    // The rest changes only with the page, the bag, the tab, the filters, the part, the lock or the
    // panel of Vender.
    const Shown shown{GameLogic::Items::CurrentAuctionPageVersion(),
                      m_SellRevision,
                      m_Tab,
                      m_KindFilter,
                      m_Quality,
                      m_Sort,
                      m_CurrencyFilter,
                      m_SellCurrency,
                      m_FirstRow,
                      m_Busy,
                      m_SellKey,
                      m_PriceCheck,
                      !m_Search.empty()};
    if (shown != m_Shown)
    {
        m_Shown = shown;
        const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
        const bool fits = PageFitsTab();
        wchar_t text[256] = {};

        mu_swprintf(text, I18N::Game::AuctionAccountItems, static_cast<int>(page.accountItems),
                    static_cast<int>(page.maxItemsPerAccount));
        SyncField(binder, &AuctionRmlModel::accountItemsText, "account_items_text", Narrow(text));

        SyncBrowseControls(page, fits);
        SyncPagerAndFooter(page, fits);
        SyncRows(page);
        SyncSellPanel(page);
    }

    SyncRowIcons();
}

void CAuctionWindow::SyncBrowseControls(const AuctionPage& page, bool fits)
{
    auto& binder = m_RmlView.Binder();
    AuctionRmlModel& model = m_RmlView.GetModel();
    wchar_t text[256] = {};

    // Comprar: the sidebar, the result count and the heads of the columns.
    for (AuctionKindRow& kind : model.kinds)
        kind.selected = kind.kind == static_cast<int>(m_KindFilter);
    m_RmlView.MarkDirty("kinds");
    SyncField(binder, &AuctionRmlModel::qualityText, "quality_text", Narrow(QualityName(m_Quality)));
    SyncField(binder, &AuctionRmlModel::filterCurrencyText, "filter_currency_text",
              Narrow(CurrencyName(m_CurrencyFilter)));
    SyncField(binder, &AuctionRmlModel::canClearFilters, "can_clear_filters",
              !m_Busy && (m_KindFilter != AuctionCategory::All ||
                          m_CurrencyFilter != GameLogic::Items::AuctionCurrencyAny ||
                          m_Quality != GameLogic::Items::AuctionQuality::Any || m_Sort != AuctionSort::Newest ||
                          !m_Search.empty()));
    if (fits && m_Tab == Tab::Browse)
    {
        if (page.total == 1)
            mu_swprintf(text, L"%ls", I18N::Game::AuctionResultOne);
        else
            mu_swprintf(text, I18N::Game::AuctionResultCount, static_cast<int>(page.total));
    }
    else
        text[0] = L'\0';
    SyncField(binder, &AuctionRmlModel::resultCountText, "result_count_text", Narrow(text));
    const int sortColumn = m_Sort == AuctionSort::PriceLow || m_Sort == AuctionSort::PriceHigh ? SortColumnPrice
                           : m_Sort == AuctionSort::EndingSoon                                 ? SortColumnTime
                                                                                               : SortColumnItem;
    SyncField(binder, &AuctionRmlModel::sortColumn, "sort_column", sortColumn);
    const std::string item = Narrow(I18N::Game::AuctionColumnItem);
    const std::string price = Narrow(I18N::Game::AuctionColumnPrice);
    const std::string time = Narrow(I18N::Game::AuctionColumnTime);
    const bool sortable = m_Tab == Tab::Browse;
    SyncField(binder, &AuctionRmlModel::columnItemText, "column_item_text",
              sortable && m_Sort == AuctionSort::Newest ? item + ArrowDown : item);
    SyncField(binder, &AuctionRmlModel::columnPriceText, "column_price_text",
              sortable && m_Sort == AuctionSort::PriceLow    ? price + ArrowUp
              : sortable && m_Sort == AuctionSort::PriceHigh ? price + ArrowDown
                                                             : price);
    SyncField(binder, &AuctionRmlModel::columnTimeText, "column_time_text",
              sortable && m_Sort == AuctionSort::EndingSoon ? time + ArrowUp : time);

}

void CAuctionWindow::SyncPagerAndFooter(const AuctionPage& page, bool fits)
{
    auto& binder = m_RmlView.Binder();
    wchar_t text[256] = {};

    // The pager: on Comprar the pages of the server, each in its parts; elsewhere the parts.
    const int count = RowCount();
    const int parts = GameLogic::Items::AuctionRowPages(static_cast<std::size_t>(count));
    const int part = m_FirstRow / AuctionRowsPerPage;
    int shownPage = part + 1;
    int shownPages = parts;
    bool canPrev = part > 0;
    bool canNext = part + 1 < parts;
    if (m_Tab == Tab::Browse && fits)
    {
        shownPage = page.page * parts + part + 1;
        shownPages = std::max<int>(1, page.pages) * parts;
        canPrev = canPrev || page.page > 0;
        canNext = canNext || page.page + 1 < page.pages;
    }
    mu_swprintf(text, I18N::Game::AuctionPageNumber, shownPage, std::max(shownPage, shownPages));
    SyncField(binder, &AuctionRmlModel::pageText, "page_text", Narrow(text));
    SyncField(binder, &AuctionRmlModel::canPrevPage, "can_prev_page", !m_Busy && canPrev);
    SyncField(binder, &AuctionRmlModel::canNextPage, "can_next_page", !m_Busy && canNext);

    if (m_Tab == Tab::Browse)
        mu_swprintf(text, I18N::Game::AuctionBuyerFooter, GroupedNumber(page.buyerFee).c_str());
    else if (m_Tab == Tab::Mine)
        mu_swprintf(text, I18N::Game::AuctionMineFooter, GroupedNumber(page.cancelFee).c_str());
    else
        text[0] = L'\0';
    SyncField(binder, &AuctionRmlModel::footerText, "footer_text", Narrow(text));

    mu_swprintf(text, I18N::Game::AuctionZenToClaim, GroupedNumber(page.zenToClaim).c_str());
    SyncField(binder, &AuctionRmlModel::zenToClaimText, "zen_to_claim_text", Narrow(text));
    SyncField(binder, &AuctionRmlModel::canClaimZen, "can_claim_zen", !m_Busy && page.zenToClaim > 0);

}

void CAuctionWindow::SyncRows(const AuctionPage& page)
{
    auto& binder = m_RmlView.Binder();
    AuctionRmlModel& model = m_RmlView.GetModel();
    const int count = RowCount();
    const bool fits = PageFitsTab();
    const int nameWidth = m_Tab == Tab::Browse ? BrowseNameWidth : m_Tab == Tab::Mine ? MineNameWidth : SellNameWidth;
    wchar_t text[256] = {};

    model.rows.clear();
    for (int i = 0; i < AuctionRowsPerPage && m_FirstRow + i < count; ++i)
    {
        const std::size_t index = static_cast<std::size_t>(m_FirstRow + i);
        AuctionRow entryRow;
        entryRow.index = i;
        const ITEM* item = RowItem(i);
        const std::wstring name = item != nullptr ? ItemName(*item) : L"?";
        entryRow.name = Narrow(name);
        entryRow.namePx = TextPxInBox(name, nameWidth);
        entryRow.nameClass = NameClass(item);
        if (m_Tab == Tab::Sell)
        {
            entryRow.selected = item != nullptr && item->Key == m_SellKey;
            entryRow.clickable = !m_Busy && item != nullptr;
            model.rows.push_back(std::move(entryRow));
            continue;
        }

        const AuctionEntry& entry = page.entries[index];
        const std::wstring price = GroupedNumber(entry.price);
        entryRow.price = Narrow(price);
        entryRow.pricePx = TextPxInBox(price, PriceWidth);
        entryRow.currency = Narrow(PriceUnit(entry.currency));
        entryRow.mine = entry.IsMine();
        entryRow.ended = entry.IsEnded();
        entryRow.pending = entry.IsPending();
        const std::wstring timeLeft = StringUtils::NarrowToWide(GameLogic::Items::FormatTimeLeft(entry.secondsLeft));
        if (m_Tab == Tab::Browse)
        {
            std::wstring seller = GameLogic::Items::SellerName(entry);
            if (entry.IsMine())
            {
                mu_swprintf(text, I18N::Game::AuctionSellerYours, seller.c_str());
                seller = text;
            }
            entryRow.detail = Narrow(seller);
            entryRow.time = entry.IsEnded() ? Narrow(I18N::Game::AuctionStateEnded) : Narrow(timeLeft);
            entryRow.clickable = !m_Busy && item != nullptr && !entry.IsMine();
        }
        else
        {
            if (entry.IsPending())
                entryRow.detail = Narrow(I18N::Game::AuctionStatePending);
            else if (entry.IsEnded())
                entryRow.detail = Narrow(I18N::Game::AuctionStateEnded);
            else
            {
                mu_swprintf(text, I18N::Game::AuctionStateForSale, timeLeft.c_str());
                entryRow.detail = Narrow(text);
            }
            entryRow.clickable = !m_Busy && item != nullptr && !entry.IsPending();
        }
        model.rows.push_back(std::move(entryRow));
    }
    m_RmlView.MarkDirty("rows");
    SyncField(binder, &AuctionRmlModel::empty, "empty", model.rows.empty() && (fits || m_Tab == Tab::Sell));
}

void CAuctionWindow::SyncSellPanel(const AuctionPage& page)
{
    auto& binder = m_RmlView.Binder();
    const SellItem* sell = SelectedSellItem();
    wchar_t text[256] = {};

    SyncField(binder, &AuctionRmlModel::hasSellItem, "has_sell_item", sell != nullptr);
    const std::wstring name = sell != nullptr ? ItemName(sell->item) : std::wstring{};
    SyncField(binder, &AuctionRmlModel::sellItemName, "sell_item_name", Narrow(name));
    SyncField(binder, &AuctionRmlModel::sellItemNamePx, "sell_item_name_px", TextPxInBox(name, SellPanelNameWidth));
    SyncField(binder, &AuctionRmlModel::sellItemClass, "sell_item_class",
              Rml::String(NameClass(sell != nullptr ? &sell->item : nullptr)));
    SyncField(binder, &AuctionRmlModel::sellCurrencyText, "sell_currency_text", Narrow(CurrencyName(m_SellCurrency)));

    std::wstring lowest;
    switch (m_PriceCheck)
    {
    case PriceCheck::Wanted:
    case PriceCheck::Waiting:
        lowest = I18N::Game::AuctionLowestSearching;
        break;
    case PriceCheck::Found:
        lowest = PriceText(m_SellCurrency, m_LowestPrice);
        break;
    case PriceCheck::NoneForSale:
        lowest = I18N::Game::AuctionLowestNone;
        break;
    default:
        lowest = L"-";
        break;
    }
    SyncField(binder, &AuctionRmlModel::lowestText, "lowest_text", Narrow(lowest));
    SyncField(binder, &AuctionRmlModel::lowestFound, "lowest_found", m_PriceCheck == PriceCheck::Found);

    mu_swprintf(text, I18N::Game::AuctionSellFeeListing, GroupedNumber(page.listingFee).c_str());
    SyncField(binder, &AuctionRmlModel::sellFeeListingText, "sell_fee_listing_text", Narrow(text));
    mu_swprintf(text, I18N::Game::AuctionSellFeeDays, static_cast<int>(page.durationDays));
    SyncField(binder, &AuctionRmlModel::sellFeeDaysText, "sell_fee_days_text", Narrow(text));
    mu_swprintf(text, I18N::Game::AuctionSellFeeCancel, GroupedNumber(page.cancelFee).c_str());
    SyncField(binder, &AuctionRmlModel::sellFeeCancelText, "sell_fee_cancel_text", Narrow(text));
    SyncField(binder, &AuctionRmlModel::canList, "can_list", !m_Busy && sell != nullptr);
}
