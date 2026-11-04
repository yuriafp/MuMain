#pragma once

#include "GameLogic/Items/Auction.h"
#include "Network/Server/AuctionPacket.h"
#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/NPCs/AuctionRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The window of Otto, the auctioneer of our server, docked right next to the inventory, opened when
// the server answers the talk to him with the first page of items for sale (GameLogic/Items/Auction.h).
// auction.rml draws its three tabs: Comprar (the items for sale, by kind and currency), Meus itens
// (the items of the account with him, and the zen of its sales) and Vender (the items of the main bag).
// C++ keeps the items of the page and of the bag, draws them in 3D and their tooltip, asks with the
// confirm dialog, sends the requests (one at a time), and handles Escape and the close of the NPC dialog.
class CAuctionWindow : public CObject
{
public:
    enum class Tab : int
    {
        Browse = 0,
        Mine = 1,
        Sell = 2,
    };

private:
    // An item of the main bag which the Vender tab offers, with its slot as the server numbers it.
    struct SellItem
    {
        std::uint8_t serverSlot = 0;
        ITEM item{};
    };

    CManager* m_pNewUIMng = nullptr;

    void BindRmlModel(Rml::DataModelConstructor& c, AuctionRmlModel& model);
    UI::RmlBridge::ThemedView<AuctionRmlModel> m_RmlView{"auction",
        [this](Rml::DataModelConstructor& c, AuctionRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/auction.rml"}}};

    // The clicks RmlUi reported, handled in Update().
    int m_PendingTab = -1;
    int m_PendingKindDelta = 0;
    int m_PendingQualityDelta = 0;
    int m_PendingCurrencyDelta = 0;
    int m_PendingPageDelta = 0;
    int m_PendingRowClick = -1;
    bool m_PendingClaimZen = false;

    Tab m_Tab = Tab::Browse;
    GameLogic::Items::AuctionCategory m_KindFilter = GameLogic::Items::AuctionCategory::All;
    std::uint8_t m_CurrencyFilter = GameLogic::Items::AuctionCurrencyAny;
    GameLogic::Items::AuctionQuality m_Quality = GameLogic::Items::AuctionQuality::Any;
    // The search by name: the text of the field as last seen, when it last changed, and the folded
    // text of the searches sent (GameLogic::Items::FoldSearchText).
    Rml::String m_SearchField;
    std::chrono::steady_clock::time_point m_SearchChangedAt;
    std::wstring m_Search;
    // The item types of m_SearchTypesFor, found once for the pages of a search.
    std::wstring m_SearchTypesFor;
    std::vector<std::uint16_t> m_SearchTypes;
    std::uint16_t m_ServerPage = 0;
    std::uint8_t m_SellCurrency = GameLogic::Items::AuctionCurrencyZen;
    // The part of the list on screen: its first row.
    int m_FirstRow = 0;
    // The previous page of the server was asked for: show its last part when it comes.
    bool m_ShowLastRows = false;

    // A request waits for the server's answer: a page or a result, or a timeout when it refused
    // without them.
    bool m_Busy = false;
    GameLogic::Items::AuctionAction m_RequestAction = GameLogic::Items::AuctionAction::Browse;
    // A confirmed request which came while another waited.
    std::optional<Network::Auction::Request> m_QueuedRequest;
    // The item of the last confirmed sale, which a queued sale still needs in its slot.
    DWORD m_QueuedItemKey = 0;
    unsigned m_RequestPageVersion = 0;
    std::chrono::steady_clock::time_point m_RequestTime;
    unsigned m_HandledResultVersion = 0;

    // The items of the page of the server, parsed from its entries, by entry.
    unsigned m_ItemsPageVersion = 0;
    std::vector<ITEM> m_PageItems;
    // The items of the main bag for the Vender tab, and the keys of the bag they were read from.
    std::vector<SellItem> m_SellItems;
    std::vector<DWORD> m_InventoryKeys;
    unsigned m_SellRevision = 0;

    // What the model's rows show.
    unsigned m_ShownPageVersion = 0;
    unsigned m_ShownSellRevision = 0;
    Tab m_ShownTab = Tab::Browse;
    GameLogic::Items::AuctionCategory m_ShownKindFilter = GameLogic::Items::AuctionCategory::All;
    GameLogic::Items::AuctionQuality m_ShownQuality = GameLogic::Items::AuctionQuality::Any;
    std::uint8_t m_ShownCurrencyFilter = GameLogic::Items::AuctionCurrencyAny;
    std::uint8_t m_ShownSellCurrency = GameLogic::Items::AuctionCurrencyZen;
    int m_ShownFirstRow = -1;
    bool m_ShownBusy = false;

    // The row under the mouse, whose item gets its tooltip.
    int m_HoveredRow = -1;

    std::array<std::unique_ptr<UI::Items::ItemCameraTarget>, GameLogic::Items::AuctionRowsPerPage> m_RowTargets;

public:
    CAuctionWindow();
    virtual ~CAuctionWindow();

    bool Create(CManager* pNewUIMng);
    Rml::ElementDocument* GetFillDocument() const override { return m_RmlView.Document(); }
    Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }
    void Release();

    bool UpdateMouseEvent();
    bool UpdateKeyEvent();
    bool Update();
    bool Render();

    void OpeningProcess();
    void ClosingProcess();

    float GetLayerDepth(); //. 5.0f

    // The search field takes the typing while it has the focus.
    bool TakesTypingFrom(const Rml::ElementDocument* document) const override;

private:
    void HandleClicks();
    void HandleRowClick(int row);
    void StepRows(int delta);
    void SendRequest(GameLogic::Items::AuctionAction action, std::uint8_t arg1 = 0, std::uint8_t arg2 = 0,
                     std::int64_t value = 0, std::uint32_t price = 0);
    void Send(const std::uint8_t* data, std::size_t size);
    // Sends the search again when the text of the field rests, half a second after the typing.
    void UpdateSearch();
    void RequestBrowse(std::uint16_t page);
    void RequestCurrentView();

    // Whether the page of the server is the one of the tab: right after a switch it's the old one.
    bool PageFitsTab() const;
    // How many rows the list of the tab has, all its parts together.
    int RowCount() const;
    void RebuildPageItems();
    bool SlotHolds(std::uint8_t serverSlot, DWORD key) const;
    bool HasInventoryChanged() const;
    void RebuildSellItems();

    // The item of a row on screen, null for an empty row.
    const ITEM* RowItem(int row) const;
    void RenderRowItem(int row, const Rml::Vector2f& offset, const Rml::Vector2f& size);
    void SyncRowIcons();
    void DisableRowIcons();

    void BuildRmlUi();
    void SyncRmlModel();
};
} // namespace mu::ui::window
