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

#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
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

// The width of an item's name in its row (auction.rcss), reference px.
constexpr int NameBoxWidth = 134;

// The shortest item data of an entry which is an item.
constexpr std::uint8_t MinimumItemLength = 5;

// How long the search field rests after the typing before the search goes.
constexpr auto SearchDelay = std::chrono::milliseconds(500);

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

// A price as the rows show it: "1.500.000 zen", "10 Bless".
std::wstring PriceText(std::uint8_t currency, std::uint32_t price)
{
    const std::wstring unit = currency == GameLogic::Items::AuctionCurrencyZen ? L"zen" : CurrencyName(currency);
    return GroupedNumber(price) + L" " + unit;
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

// The item types whose name in the client's language holds the search, for the server.
std::vector<std::uint16_t> ItemTypesNamed(const std::wstring& search)
{
    return GameLogic::Items::MatchItemTypes(
        search, MAX_ITEM,
        [](int type)
        {
            wchar_t name[64] = {};
            GetItemName(type, 0, name);
            return std::wstring(name);
        },
        GameLogic::Items::AuctionMaximumSearchTypes);
}

std::wstring ItemName(const ITEM& item)
{
    return GetItemNameText(item.Type, item.Level, &item);
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
} // namespace

CAuctionWindow::CAuctionWindow()
{
    for (int i = 0; i < AuctionRowsPerPage; ++i)
    {
        m_RowTargets[static_cast<std::size_t>(i)] = std::make_unique<UI::Items::ItemCameraTarget>(
            [this, i](const Rml::Vector2f& offset, const Rml::Vector2f& size) { RenderRowItem(i, offset, size); });
    }
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
    // The tabs, the list and the buttons are RmlUi's (see Update()).
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
        if (page.view == AuctionView::Browse)
        {
            m_KindFilter = page.kindFilter;
            m_CurrencyFilter = page.currencyFilter;
            m_ServerPage = page.page;
        }

        // A new page starts at its first part, or at its last when the pager went back to it; a
        // list which shrank (an item sold) keeps the part on screen as far as it can.
        const int last = (GameLogic::Items::AuctionRowPages(page.entries.size()) - 1) * AuctionRowsPerPage;
        const bool showLast = std::exchange(m_ShowLastRows, false) && page.view == AuctionView::Browse;
        m_FirstRow = showLast ? last : std::min(m_FirstRow, last);
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
    // Talking to Otto sends the first page of everything.
    m_KindFilter = AuctionCategory::All;
    m_CurrencyFilter = GameLogic::Items::AuctionCurrencyAny;
    m_Quality = GameLogic::Items::AuctionQuality::Any;
    m_Search.clear();
    m_SearchField.clear();
    if (m_RmlView.Document())
    {
        m_RmlView.GetModel().searchText.clear();
        m_RmlView.MarkDirty("search_text");
    }
    m_FirstRow = 0;
    m_ShowLastRows = false;
    m_Busy = false;
    m_QueuedRequest.reset();
    m_HandledResultVersion = GameLogic::Items::CurrentAuctionResultVersion();
    m_HoveredRow = -1;
    m_ShownFirstRow = -1;
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
    }

    std::wstring search = GameLogic::Items::FoldSearchText(StringUtils::NarrowToWide(m_SearchField));
    if (search.size() < GameLogic::Items::AuctionMinimumSearchLength)
        search.clear();
    if (search == m_Search || m_Tab != Tab::Browse || m_Busy || now - m_SearchChangedAt < SearchDelay)
        return;

    m_Search = std::move(search);
    m_FirstRow = 0;
    RequestBrowse(0);
}

void CAuctionWindow::HandleClicks()
{
    const int tab = std::exchange(m_PendingTab, -1);
    const int kindDelta = std::exchange(m_PendingKindDelta, 0);
    const int qualityDelta = std::exchange(m_PendingQualityDelta, 0);
    const int currencyDelta = std::exchange(m_PendingCurrencyDelta, 0);
    const int pageDelta = std::exchange(m_PendingPageDelta, 0);
    const int row = std::exchange(m_PendingRowClick, -1);
    const bool claimZen = std::exchange(m_PendingClaimZen, false);
    if (!IsVisible() || m_Busy)
        return;

    if (tab >= 0 && tab <= 2 && static_cast<Tab>(tab) != m_Tab)
    {
        m_Tab = static_cast<Tab>(tab);
        m_HoveredRow = -1;
        m_FirstRow = 0;
        if (m_Tab == Tab::Browse)
            RequestBrowse(m_ServerPage);
        else if (m_Tab == Tab::Mine)
            SendRequest(AuctionAction::Mine);
        else
            RebuildSellItems();
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    if (pageDelta != 0)
    {
        StepRows(pageDelta);
        return;
    }

    if (m_Tab == Tab::Browse && (kindDelta != 0 || currencyDelta != 0 || qualityDelta != 0))
    {
        if (kindDelta != 0)
            m_KindFilter = GameLogic::Items::StepKind(m_KindFilter, kindDelta);
        if (qualityDelta != 0)
            m_Quality = GameLogic::Items::StepQuality(m_Quality, qualityDelta);
        if (currencyDelta != 0)
            m_CurrencyFilter = GameLogic::Items::StepFilterCurrency(m_CurrencyFilter, currencyDelta);
        m_FirstRow = 0;
        RequestBrowse(0);
        return;
    }

    if (m_Tab == Tab::Sell && currencyDelta != 0)
    {
        m_SellCurrency = GameLogic::Items::StepSellCurrency(m_SellCurrency, currencyDelta);
        return;
    }

    if (m_Tab == Tab::Mine && claimZen && PageFitsTab() && GameLogic::Items::CurrentAuctionPage().zenToClaim > 0)
    {
        SendRequest(AuctionAction::ClaimZen);
        return;
    }

    if (row >= 0)
        HandleRowClick(row);
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
    if (g_pGenericConfirmDialog == nullptr || RowItem(row) == nullptr)
        return;

    const int index = m_FirstRow + row;
    const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    wchar_t line[256] = {};

    if (m_Tab == Tab::Sell)
    {
        const SellItem& sell = m_SellItems[static_cast<std::size_t>(index)];
        const std::uint8_t currency = m_SellCurrency;
        const std::uint32_t maxPrice = GameLogic::Items::MaxAuctionPrice(currency);
        mu_swprintf(line, I18N::Game::AuctionConfirmSell, ItemName(sell.item).c_str(), CurrencyName(currency).c_str());

        GenericDialogConfig::InputField input;
        input.numericOnly = true;
        input.maxLength = currency == GameLogic::Items::AuctionCurrencyZen ? 10 : 4;
        cfg.input = std::move(input);
        cfg.item3D = sell.item;
        cfg.tag = "auction_list";
        const std::uint8_t slot = sell.serverSlot;
        const DWORD key = sell.item.Key;
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
    }
    else
    {
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
    }

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
    m_RequestAction = static_cast<AuctionAction>(data[offsetof(Network::Auction::Request, action)]);
    m_RequestPageVersion = GameLogic::Items::CurrentAuctionPageVersion();
    m_RequestTime = std::chrono::steady_clock::now();
}

void CAuctionWindow::RequestBrowse(std::uint16_t page)
{
    std::optional<std::vector<std::uint16_t>> itemTypes;
    if (!m_Search.empty())
    {
        if (m_SearchTypesFor != m_Search)
        {
            m_SearchTypes = ItemTypesNamed(m_Search);
            m_SearchTypesFor = m_Search;
        }
        itemTypes = m_SearchTypes;
    }
    const std::vector<std::uint8_t> request =
        Network::Auction::MakeSearchRequest(m_KindFilter, m_CurrencyFilter, page, m_Quality, itemTypes);
    Send(request.data(), request.size());
}

void CAuctionWindow::RequestCurrentView()
{
    // After a sale from the Vender tab the window stays there and asks for the account's items,
    // whose count its footer doesn't need but the Meus itens tab will.
    if (m_Tab == Tab::Browse)
        RequestBrowse(m_ServerPage);
    else
        SendRequest(AuctionAction::Mine);
}

bool CAuctionWindow::PageFitsTab() const
{
    const AuctionView view = GameLogic::Items::CurrentAuctionPage().view;
    return (m_Tab == Tab::Browse && view == AuctionView::Browse) || (m_Tab == Tab::Mine && view == AuctionView::Mine);
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
}

void CAuctionWindow::DisableRowIcons()
{
    for (auto& target : m_RowTargets)
        target->Disable();
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
    c.Bind("busy", &model.busy);
    c.Bind("kind_text", &model.kindText);
    c.Bind("filter_currency_text", &model.filterCurrencyText);
    c.Bind("quality_text", &model.qualityText);
    c.Bind("search_text", &model.searchText);
    c.Bind("search_hint", &model.searchHint);
    c.Bind("sell_currency_text", &model.sellCurrencyText);
    c.Bind("account_items_text", &model.accountItemsText);
    c.Bind("empty_text", &model.emptyText);
    c.Bind("empty", &model.empty);
    c.Bind("page_text", &model.pageText);
    c.Bind("can_prev_page", &model.canPrevPage);
    c.Bind("can_next_page", &model.canNextPage);
    c.Bind("footer_text", &model.footerText);
    c.Bind("zen_to_claim_text", &model.zenToClaimText);
    c.Bind("claim_button_text", &model.claimButtonText);
    c.Bind("can_claim_zen", &model.canClaimZen);

    auto row = c.RegisterStruct<AuctionRow>();
    row.RegisterMember("index", &AuctionRow::index);
    row.RegisterMember("name", &AuctionRow::name);
    row.RegisterMember("name_px", &AuctionRow::namePx);
    row.RegisterMember("price", &AuctionRow::price);
    row.RegisterMember("detail", &AuctionRow::detail);
    row.RegisterMember("mine", &AuctionRow::mine);
    row.RegisterMember("ended", &AuctionRow::ended);
    row.RegisterMember("pending", &AuctionRow::pending);
    row.RegisterMember("clickable", &AuctionRow::clickable);
    c.RegisterArray<std::vector<AuctionRow>>();
    c.Bind("rows", &model.rows);

    const auto intArgument = [](const Rml::VariantList& arguments, int fallback)
    { return arguments.size() == 1 ? arguments[0].Get<int>(fallback) : fallback; };
    c.BindEventCallback("auction_tab", [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingTab = intArgument(a, -1); });
    c.BindEventCallback("auction_kind_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingKindDelta = intArgument(a, 0); });
    c.BindEventCallback("auction_quality_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingQualityDelta = intArgument(a, 0); });
    c.BindEventCallback("auction_currency_step",
                        [this, intArgument](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a)
                        { m_PendingCurrencyDelta = intArgument(a, 0); });
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

    model.title = Narrow(I18N::Game::AuctionTitle);
    model.tabBuyText = Narrow(I18N::Game::AuctionTabBuy);
    model.tabMineText = Narrow(I18N::Game::AuctionTabMine);
    model.tabSellText = Narrow(I18N::Game::AuctionTabSell);
    model.claimButtonText = Narrow(I18N::Game::AuctionClaimButton);
    model.emptyText = Narrow(I18N::Game::AuctionEmpty);
    model.searchHint = Narrow(I18N::Game::AuctionSearchHint);
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
    SyncField(binder, &AuctionRmlModel::boldTextPx, "bold_text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold));
    SyncField(binder, &AuctionRmlModel::labelLinePx, "label_line_px",
              CUIRenderTextSDLTtf::LineHeightPx(UI::Scaling::FontRole::Normal));
    SyncField(binder, &AuctionRmlModel::activeTab, "active_tab", static_cast<int>(m_Tab));
    SyncField(binder, &AuctionRmlModel::busy, "busy", m_Busy);

    // The rows change only with the page, the bag, the tab, the filters, the part or the lock.
    const unsigned pageVersion = GameLogic::Items::CurrentAuctionPageVersion();
    if (m_ShownPageVersion != pageVersion || m_ShownSellRevision != m_SellRevision || m_ShownTab != m_Tab ||
        m_ShownKindFilter != m_KindFilter || m_ShownQuality != m_Quality || m_ShownCurrencyFilter != m_CurrencyFilter ||
        m_ShownSellCurrency != m_SellCurrency || m_ShownFirstRow != m_FirstRow || m_ShownBusy != m_Busy)
    {
        m_ShownPageVersion = pageVersion;
        m_ShownSellRevision = m_SellRevision;
        m_ShownTab = m_Tab;
        m_ShownKindFilter = m_KindFilter;
        m_ShownQuality = m_Quality;
        m_ShownCurrencyFilter = m_CurrencyFilter;
        m_ShownSellCurrency = m_SellCurrency;
        m_ShownFirstRow = m_FirstRow;
        m_ShownBusy = m_Busy;

        const AuctionPage& page = GameLogic::Items::CurrentAuctionPage();
        const bool fits = PageFitsTab();
        SyncField(binder, &AuctionRmlModel::kindText, "kind_text", Narrow(KindName(m_KindFilter)));
        SyncField(binder, &AuctionRmlModel::qualityText, "quality_text", Narrow(QualityName(m_Quality)));
        SyncField(binder, &AuctionRmlModel::filterCurrencyText, "filter_currency_text",
                  Narrow(CurrencyName(m_CurrencyFilter)));
        SyncField(binder, &AuctionRmlModel::sellCurrencyText, "sell_currency_text",
                  Narrow(CurrencyName(m_SellCurrency)));

        wchar_t text[256] = {};
        mu_swprintf(text, I18N::Game::AuctionAccountItems, static_cast<int>(page.accountItems),
                    static_cast<int>(page.maxItemsPerAccount));
        SyncField(binder, &AuctionRmlModel::accountItemsText, "account_items_text", Narrow(text));

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
        else if (m_Tab == Tab::Sell)
            mu_swprintf(text, I18N::Game::AuctionSellFooter, GroupedNumber(page.listingFee).c_str(),
                        static_cast<int>(page.durationDays), GroupedNumber(page.cancelFee).c_str());
        else
            text[0] = L'\0';
        SyncField(binder, &AuctionRmlModel::footerText, "footer_text", Narrow(text));

        mu_swprintf(text, I18N::Game::AuctionZenToClaim, GroupedNumber(page.zenToClaim).c_str());
        SyncField(binder, &AuctionRmlModel::zenToClaimText, "zen_to_claim_text", Narrow(text));
        SyncField(binder, &AuctionRmlModel::canClaimZen, "can_claim_zen", !m_Busy && page.zenToClaim > 0);

        AuctionRmlModel& model = m_RmlView.GetModel();
        model.rows.clear();
        for (int i = 0; i < AuctionRowsPerPage && m_FirstRow + i < count; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(m_FirstRow + i);
            AuctionRow entryRow;
            entryRow.index = i;
            const ITEM* item = RowItem(i);
            const std::wstring name = item != nullptr ? ItemName(*item) : L"?";
            entryRow.name = Narrow(name);
            entryRow.namePx = TextPxInBox(name, NameBoxWidth);
            if (m_Tab == Tab::Sell)
            {
                entryRow.clickable = !m_Busy && item != nullptr;
            }
            else
            {
                const AuctionEntry& entry = page.entries[index];
                entryRow.price = Narrow(PriceText(entry.currency, entry.price));
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
                    entryRow.detail = Narrow(seller + L"  " + timeLeft);
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
            }
            model.rows.push_back(std::move(entryRow));
        }
        m_RmlView.MarkDirty("rows");
        SyncField(binder, &AuctionRmlModel::empty, "empty", model.rows.empty() && (fits || m_Tab == Tab::Sell));
    }

    SyncRowIcons();
}
