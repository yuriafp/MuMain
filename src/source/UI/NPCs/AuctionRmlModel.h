#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One row of the list: the item drawn in 3D at its left (the <img> #auction_item_<index>), its name,
// and under it the price and the seller or the state; a click buys, takes back or sells it.
struct AuctionRow
{
    // The row on screen, 0 .. AuctionRowsPerPage - 1.
    int index = 0;
    Rml::String name;
    // The name's size: the native one, shrunk to fit its box as RenderText() did.
    float namePx = 0.f;
    Rml::String price;
    Rml::String detail;
    bool mine = false;
    bool ended = false;
    bool pending = false;
    bool clickable = true;
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

    // Locked from a request until the server answers it.
    bool busy = false;

    // Comprar: the kind, currency and quality selectors, and the search by name, which the field
    // writes back (data-value).
    Rml::String kindText;
    Rml::String filterCurrencyText;
    Rml::String qualityText;
    Rml::String searchText;
    Rml::String searchHint;
    // Vender: the currency of the price.
    Rml::String sellCurrencyText;
    // Meus itens: how many items the account has with Otto.
    Rml::String accountItemsText;

    std::vector<AuctionRow> rows;
    Rml::String emptyText;
    bool empty = true;

    Rml::String pageText;
    bool canPrevPage = false;
    bool canNextPage = false;

    // The line under the list: the fees of the tab.
    Rml::String footerText;
    // Meus itens: the zen of the sales and the button which takes it.
    Rml::String zenToClaimText;
    Rml::String claimButtonText;
    bool canClaimZen = false;
};
} // namespace mu::ui::window
