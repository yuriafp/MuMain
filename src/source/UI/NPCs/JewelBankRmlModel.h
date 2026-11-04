#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One jewel of the list: its item name and what the account keeps of it; a click selects it.
struct JewelBankRow
{
    int index = 0;
    Rml::String name;
    // The name's size: the native one, shrunk to fit its 108-unit box as RenderText() did.
    float namePx = 0.f;
    // The jewel's icon (an RmlUi image source; empty without one), right after the drawn name at
    // iconLeft, reference px from the row's left.
    Rml::String icon;
    float iconLeft = 0.f;
    Rml::String amount;
    bool selected = false;
    bool empty = true;
};

// One withdrawal button: what it takes out of the selected jewel, enabled while the account
// keeps that much of it.
struct JewelBankWithdrawButton
{
    int amount = 0;
    Rml::String label;
    bool enabled = false;
};

struct JewelBankRmlModel
{
    float textPx = 0.f;     // native normal text size in physical px (RmlNativeTextSize.h)
    float boldTextPx = 0.f; // native bold text size
    // The buttons' labels centred on the native line height, in physical px.
    float labelLinePx = 0.f;

    Rml::String title;
    Rml::String depositFee;
    Rml::String withdrawalFee;
    std::vector<JewelBankRow> rows;
    // Locked from a request until the server answers it.
    bool busy = false;
    Rml::String depositText;
    Rml::String depositAllText;
    Rml::String withdrawTitle;
    std::vector<JewelBankWithdrawButton> withdrawButtons;
    Rml::String hint;
};
} // namespace mu::ui::window
