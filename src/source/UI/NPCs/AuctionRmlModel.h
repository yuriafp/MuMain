#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One row of a list: the item drawn in 3D at its left (the <img> #auction_item_<index>), its name in
// the color of its tooltip, and the columns of the tab: the price and its currency, the seller or the
// state, the time left. A click buys, takes back or selects it.
struct AuctionRow
{
    // The row on screen, 0 .. AuctionRowsPerPage - 1.
    int index = 0;
    Rml::String name;
    // The name's size: the native one, shrunk to fit its box as RenderText() did.
    float namePx = 0.f;
    // The class of the name's color (auction.rcss): q-normal, q-blue, q-yellow, q-exc, q-ancient...
    Rml::String nameClass;
    Rml::String price;
    float pricePx = 0.f;
    Rml::String currency;
    // Comprar: the seller; Meus itens: the state.
    Rml::String detail;
    Rml::String time;
    bool mine = false;
    bool ended = false;
    bool pending = false;
    bool clickable = true;
    // Vender: the item of the panel.
    bool selected = false;
};

// A kind of the sidebar of Comprar.
struct AuctionKindRow
{
    int kind = 0;
    Rml::String label;
    bool selected = false;
};

// A name which the search field suggests while the player types.
struct AuctionSuggestion
{
    int index = 0;
    Rml::String name;
};

struct AuctionRmlModel
{
    float textPx = 0.f;     // native normal text size in physical px (RmlNativeTextSize.h)
    float boldTextPx = 0.f; // native bold text size
    // The buttons' labels centred on the native line height, in physical px.
    float labelLinePx = 0.f;

    Rml::String title;
    Rml::String tabBuyText;
    Rml::String tabMineText;
    Rml::String tabSellText;
    // 0 Comprar, 1 Meus itens, 2 Vender.
    int activeTab = 0;
    // How many items the account has with Otto, over the tabs.
    Rml::String accountItemsText;

    // Locked from a request until the server answers it.
    bool busy = false;

    // Comprar: the sidebar (the kinds, the currency and the quality, and a reset of the filters).
    Rml::String categoryTitle;
    std::vector<AuctionKindRow> kinds;
    Rml::String currencyTitle;
    Rml::String filterCurrencyText;
    Rml::String qualityTitle;
    Rml::String qualityText;
    Rml::String clearFiltersText;
    bool canClearFilters = false;
    // Comprar: the search by name, which the field writes back (data-value), and its suggestions.
    Rml::String searchText;
    Rml::String searchHint;
    std::vector<AuctionSuggestion> suggestions;
    bool showSuggestions = false;
    Rml::String resultCountText;
    // The heads of the columns, with the arrow of the order on the sorted one: 0 Item (the newest
    // first), 1 Preço, 2 Tempo.
    Rml::String columnItemText;
    Rml::String columnPriceText;
    Rml::String columnTimeText;
    Rml::String columnStateText;
    int sortColumn = 0;

    std::vector<AuctionRow> rows;
    Rml::String emptyText;
    bool empty = true;

    Rml::String pageText;
    bool canPrevPage = false;
    bool canNextPage = false;

    // The line under the list: the fees of the tab.
    Rml::String footerText;
    // Meus itens: the zen of the sales and the button which takes it, and the button of each row.
    Rml::String zenToClaimText;
    Rml::String claimButtonText;
    bool canClaimZen = false;
    Rml::String returnButtonText;

    // Vender: the bag, and the panel of the selected item: its name, the currency of the price, the
    // lowest price of the same item for sale, the fees and Anunciar.
    Rml::String bagTitle;
    bool hasSellItem = false;
    Rml::String sellChooseText;
    Rml::String sellItemName;
    float sellItemNamePx = 0.f;
    Rml::String sellItemClass;
    Rml::String sellCurrencyText;
    Rml::String lowestTitle;
    Rml::String lowestText;
    bool lowestFound = false;
    Rml::String sellFeeListingText;
    Rml::String sellFeeDaysText;
    Rml::String sellFeeCancelText;
    Rml::String listButtonText;
    bool canList = false;
};
} // namespace mu::ui::window
