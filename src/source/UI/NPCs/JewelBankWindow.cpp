#include "stdafx.h"
#include "UI/NPCs/JewelBankWindow.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/JewelBank.h"
#include "Network/Server/JewelBankPacket.h"
#include "UI/Core/WindowSystem.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"
#include "UI/RmlBridge/RmlSyncField.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlWindowClose.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"

#include <RmlUi/Core/ElementDocument.h>

#include <string>
#include <utility>

using namespace mu::ui::window;
using GameLogic::Items::JewelBankAction;

namespace
{
// How long a request waits for the server's answer before the buttons unlock: the server refuses
// some requests (a trade open, a failed save) with a message and no jewels.
constexpr auto AnswerTimeout = std::chrono::seconds(3);

// The width of a jewel's name in its row, after its icon (jewel_bank.rcss), reference px.
constexpr int NameBoxWidth = 88;

// The icon of each jewel, at the left of its row: Interface/JewelBank/<file>.OZT, rendered from the jewel's
// model (scratchpad scripts/render-item-icons.py). By the server's group and number.
struct JewelIcon
{
    int group;
    int number;
    const char* file;
};
constexpr JewelIcon JewelIcons[] = {
    {14, 13, "bless"},    {14, 14, "soul"},     {14, 16, "life"},     {12, 15, "chaos"},
    {14, 22, "creation"}, {14, 31, "guardian"}, {14, 42, "harmony"},  {14, 41, "gemstone"},
    {14, 43, "lower_refining_stone"},           {14, 44, "higher_refining_stone"},
};

// The RmlUi source of a jewel's icon, like the other images of the windows (crywolf.rml); empty for
// a jewel without one.
std::string JewelIconSource(const GameLogic::Items::JewelBankJewel& jewel)
{
    for (const JewelIcon& icon : JewelIcons)
    {
        if (icon.group == jewel.group && icon.number == jewel.number)
            return std::string("../../../JewelBank/") + icon.file + ".tga";
    }
    return {};
}

// A number with the client's thousands separators, as zen is shown.
std::string GroupedNumber(int value)
{
    wchar_t text[32] = {};
    ConvertGold(static_cast<double>(value), text);
    return StringUtils::WideToNarrow(text);
}

std::string FeeText(const wchar_t* format, int fee)
{
    wchar_t line[128] = {};
    mu_swprintf(line, format, StringUtils::NarrowToWide(GroupedNumber(fee)).c_str());
    return StringUtils::WideToNarrow(line);
}

// The client's name of the single jewel, e.g. "Jewel of Bless".
std::wstring JewelName(const GameLogic::Items::JewelBankJewel& jewel)
{
    return GetItemName(jewel.group * MAX_ITEM_INDEX + jewel.number, 0);
}

// The width of `text` in the normal font, reference px.
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
} // namespace

CJewelBankWindow::~CJewelBankWindow()
{
    Release();
}

bool CJewelBankWindow::Create(CManager* pNewUIMng)
{
    if (nullptr == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_JEWEL_BANK, this);

    BuildRmlUi();

    Show(false);

    return true;
}

void CJewelBankWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

bool CJewelBankWindow::UpdateMouseEvent()
{
    // The list and the buttons are RmlUi's (see Update()).
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CJewelBankWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_JEWEL_BANK) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_JEWEL_BANK);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

bool CJewelBankWindow::Update()
{
    if (m_Busy && (GameLogic::Items::CurrentJewelBankVersion() != m_RequestVersion ||
                   std::chrono::steady_clock::now() - m_RequestTime > AnswerTimeout))
    {
        m_Busy = false;
    }

    HandleClicks();
    SyncRmlModel();
    return true;
}

bool CJewelBankWindow::Render()
{
    // Nothing native: the frame, the list and the buttons are RmlUi. Kept because CObject requires
    // the override.
    return true;
}

void CJewelBankWindow::OpeningProcess()
{
    m_Selected = 0;
    m_Busy = false;
    m_ShownSelected = -1;
}

void CJewelBankWindow::ClosingProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();
}

float CJewelBankWindow::GetLayerDepth()
{
    return 5.0f;
}

void CJewelBankWindow::HandleClicks()
{
    const int select = std::exchange(m_PendingSelect, -1);
    const bool deposit = std::exchange(m_PendingDeposit, false);
    const bool depositAll = std::exchange(m_PendingDepositAll, false);
    const int withdraw = std::exchange(m_PendingWithdraw, 0);
    if (!IsVisible())
        return;

    const GameLogic::Items::JewelBankContents& bank = GameLogic::Items::CurrentJewelBank();
    if (select >= 0 && select < static_cast<int>(bank.jewels.size()))
        m_Selected = select;

    if (deposit)
        SendRequest(JewelBankAction::Deposit, m_Selected, 0);
    else if (depositAll)
        SendRequest(JewelBankAction::DepositAll, 0, 0);
    else if (withdraw > 0 && GameLogic::Items::CanWithdraw(bank, m_Selected, withdraw))
        SendRequest(JewelBankAction::Withdraw, m_Selected, withdraw);
}

void CJewelBankWindow::SendRequest(JewelBankAction action, int jewelIndex, int amount)
{
    if (m_Busy || SocketClient == nullptr)
        return;

    const Network::JewelBank::Request request = Network::JewelBank::MakeRequest(action, jewelIndex, amount);
    SocketClient->Send(reinterpret_cast<const BYTE*>(&request), sizeof(request));
    m_Busy = true;
    m_RequestVersion = GameLogic::Items::CurrentJewelBankVersion();
    m_RequestTime = std::chrono::steady_clock::now();
}

void CJewelBankWindow::BindRmlModel(Rml::DataModelConstructor& c, JewelBankRmlModel& model)
{
    UI::RmlBridge::BindWindowClose(c, mu::ui::window::INTERFACE_JEWEL_BANK);
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("label_line_px", &model.labelLinePx);
    c.Bind("title", &model.title);
    c.Bind("deposit_fee", &model.depositFee);
    c.Bind("withdrawal_fee", &model.withdrawalFee);
    c.Bind("busy", &model.busy);
    c.Bind("deposit_text", &model.depositText);
    c.Bind("deposit_all_text", &model.depositAllText);
    c.Bind("withdraw_title", &model.withdrawTitle);
    c.Bind("hint", &model.hint);

    auto row = c.RegisterStruct<JewelBankRow>();
    row.RegisterMember("index", &JewelBankRow::index);
    row.RegisterMember("name", &JewelBankRow::name);
    row.RegisterMember("name_px", &JewelBankRow::namePx);
    row.RegisterMember("icon", &JewelBankRow::icon);
    row.RegisterMember("amount", &JewelBankRow::amount);
    row.RegisterMember("selected", &JewelBankRow::selected);
    row.RegisterMember("empty", &JewelBankRow::empty);
    c.RegisterArray<std::vector<JewelBankRow>>();
    c.Bind("rows", &model.rows);

    auto button = c.RegisterStruct<JewelBankWithdrawButton>();
    button.RegisterMember("amount", &JewelBankWithdrawButton::amount);
    button.RegisterMember("label", &JewelBankWithdrawButton::label);
    button.RegisterMember("enabled", &JewelBankWithdrawButton::enabled);
    c.RegisterArray<std::vector<JewelBankWithdrawButton>>();
    c.Bind("withdraw_buttons", &model.withdrawButtons);

    c.BindEventCallback("jewel_bank_select",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
                        {
                            if (arguments.size() == 1)
                                m_PendingSelect = arguments[0].Get<int>(-1);
                        });
    c.BindEventCallback("jewel_bank_deposit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingDeposit = true; });
    c.BindEventCallback("jewel_bank_deposit_all",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingDepositAll = true; });
    c.BindEventCallback("jewel_bank_withdraw",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
                        {
                            if (arguments.size() == 1)
                                m_PendingWithdraw = arguments[0].Get<int>(0);
                        });

    model.title = StringUtils::WideToNarrow(I18N::Game::JewelBankTitle);
    model.depositText = StringUtils::WideToNarrow(I18N::Game::JewelBankKeep);
    model.depositAllText = StringUtils::WideToNarrow(I18N::Game::JewelBankKeepAll);
    model.withdrawTitle = StringUtils::WideToNarrow(I18N::Game::JewelBankTakeOut);
    model.hint = StringUtils::WideToNarrow(I18N::Game::JewelBankHint);
    model.withdrawButtons.clear();
    for (const int amount : GameLogic::Items::JewelBankWithdrawalAmounts)
    {
        JewelBankWithdrawButton entry;
        entry.amount = amount;
        entry.label = std::to_string(amount);
        model.withdrawButtons.push_back(std::move(entry));
    }
}

void CJewelBankWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void CJewelBankWindow::SyncRmlModel()
{
    BuildRmlUi();
    if (!m_RmlView.Document())
        return;

    // Layer depth 5: over the HUD like the other panels.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());
    auto& binder = m_RmlView.Binder();
    SyncField(binder, &JewelBankRmlModel::boldTextPx, "bold_text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold));
    SyncField(binder, &JewelBankRmlModel::labelLinePx, "label_line_px",
              CUIRenderTextSDLTtf::LineHeightPx(UI::Scaling::FontRole::Normal));
    SyncField(binder, &JewelBankRmlModel::busy, "busy", m_Busy);

    // The rows and the withdrawal buttons change only with the jewels, the selection or the lock.
    const unsigned version = GameLogic::Items::CurrentJewelBankVersion();
    if (m_ShownVersion == version && m_ShownSelected == m_Selected && m_ShownBusy == m_Busy)
        return;
    m_ShownVersion = version;
    m_ShownSelected = m_Selected;
    m_ShownBusy = m_Busy;

    const GameLogic::Items::JewelBankContents& bank = GameLogic::Items::CurrentJewelBank();
    SyncField(binder, &JewelBankRmlModel::depositFee, "deposit_fee",
              FeeText(I18N::Game::JewelBankDepositFee, bank.depositFeePerJewel));
    SyncField(binder, &JewelBankRmlModel::withdrawalFee, "withdrawal_fee",
              FeeText(I18N::Game::JewelBankWithdrawalFee, bank.withdrawalFeePerJewel));

    JewelBankRmlModel& model = m_RmlView.GetModel();
    model.rows.clear();
    for (int i = 0; i < static_cast<int>(bank.jewels.size()); ++i)
    {
        const GameLogic::Items::JewelBankJewel& jewel = bank.jewels[static_cast<std::size_t>(i)];
        JewelBankRow entry;
        entry.index = i;
        const std::wstring name = JewelName(jewel);
        entry.name = StringUtils::WideToNarrow(name.c_str());
        entry.namePx = TextPxInBox(name, NameBoxWidth);
        entry.icon = JewelIconSource(jewel);
        entry.amount = GroupedNumber(jewel.amount);
        entry.selected = i == m_Selected;
        entry.empty = jewel.amount <= 0;
        model.rows.push_back(std::move(entry));
    }
    m_RmlView.MarkDirty("rows");

    for (JewelBankWithdrawButton& button : model.withdrawButtons)
        button.enabled = !m_Busy && GameLogic::Items::CanWithdraw(bank, m_Selected, button.amount);
    m_RmlView.MarkDirty("withdraw_buttons");
}
