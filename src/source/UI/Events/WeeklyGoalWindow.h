#pragma once

#include "GameLogic/Events/WeeklyGoal.h"
#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/WeeklyGoalRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <optional>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The weekly goal of our server, docked right, opened by /meta when the server answers with the
// goal (GameLogic/Events/WeeklyGoal.h). weekly_goal.rml draws it; C++ keeps the corner close and
// Escape.
class CWeeklyGoalWindow : public CObject
{
private:
    CManager* m_pNewUIMng = nullptr;

    void BindRmlModel(Rml::DataModelConstructor& c, WeeklyGoalRmlModel& model);
    UI::RmlBridge::ThemedView<WeeklyGoalRmlModel> m_RmlView{"weekly_goal",
        [this](Rml::DataModelConstructor& c, WeeklyGoalRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/weekly_goal.rml"}}};
    bool m_PendingExit = false;
    // The goal whose texts the model has, so they're built only when the server sends another.
    std::optional<GameLogic::Events::WeeklyGoal> m_ShownGoal;

public:
    CWeeklyGoalWindow() = default;
    virtual ~CWeeklyGoalWindow();

    bool Create(CManager* pNewUIMng);
    Rml::ElementDocument* GetFillDocument() const override { return m_RmlView.Document(); }
    Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }
    void Release();

    bool UpdateMouseEvent();
    bool UpdateKeyEvent();
    bool Update();
    bool Render();

    float GetLayerDepth(); //. 5.0f

private:
    void BuildRmlUi();
    void SyncRmlModel();
};
} // namespace mu::ui::window
