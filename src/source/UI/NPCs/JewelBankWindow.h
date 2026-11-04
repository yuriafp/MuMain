#pragma once

#include "GameLogic/Items/JewelBank.h"
#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/NPCs/JewelBankRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The window of Ella, the Banqueira of our server, docked right next to the inventory, opened when
// the server answers the talk to her with the jewels which the account keeps
// (GameLogic/Items/JewelBank.h). jewel_bank.rml draws it; C++ keeps the selected jewel, the
// requests to the server (one at a time), Escape and the close of the NPC dialog.
class CJewelBankWindow : public CObject
{
private:
    CManager* m_pNewUIMng = nullptr;

    void BindRmlModel(Rml::DataModelConstructor& c, JewelBankRmlModel& model);
    UI::RmlBridge::ThemedView<JewelBankRmlModel> m_RmlView{"jewel_bank",
        [this](Rml::DataModelConstructor& c, JewelBankRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/jewel_bank.rml"}}};

    // The clicks RmlUi reported, handled in Update().
    int m_PendingSelect = -1;
    bool m_PendingDeposit = false;
    bool m_PendingDepositAll = false;
    int m_PendingWithdraw = 0;

    int m_Selected = 0;
    // A request waits for the server's answer: the jewels it sends next, or a timeout when it
    // refused without them.
    bool m_Busy = false;
    unsigned m_RequestVersion = 0;
    std::chrono::steady_clock::time_point m_RequestTime;
    // The jewels and the selection whose rows the model has.
    unsigned m_ShownVersion = 0;
    int m_ShownSelected = -1;
    bool m_ShownBusy = false;

public:
    CJewelBankWindow() = default;
    virtual ~CJewelBankWindow();

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

private:
    void HandleClicks();
    void SendRequest(GameLogic::Items::JewelBankAction action, int jewelIndex, int amount);
    void BuildRmlUi();
    void SyncRmlModel();
};
} // namespace mu::ui::window
