#include "stdafx.h"
#include "UI/Events/WeeklyGoalWindow.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Events/WeeklyGoal.h"
#include "GameLogic/Items/ItemExpiry.h"
#include "UI/Core/WindowSystem.h"

#include "Core/Utilities/StringUtils.h"
#include "UI/RmlBridge/RmlSyncField.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"

#include <RmlUi/Core/ElementDocument.h>

#include <optional>
#include <string>
#include <vector>

using namespace mu::ui::window;

namespace
{
// Room for one line of the window: the goal's name is at most 64 bytes.
constexpr int LineLength = 160;

template <typename... Args> std::wstring FormatWide(const wchar_t* format, Args... args)
{
    wchar_t line[LineLength] = {};
    mu_swprintf(line, format, args...);
    return line;
}

template <typename... Args> std::string Format(const wchar_t* format, Args... args)
{
    return StringUtils::WideToNarrow(FormatWide(format, args...).c_str());
}

// The goal in the client's language, e.g. "10 Chaos Castle entries"; the server's name for a kind
// this client doesn't know.
std::string GoalText(const GameLogic::Events::WeeklyGoal& goal)
{
    using GameLogic::Events::WeeklyGoalKind;
    switch (goal.kind)
    {
    case WeeklyGoalKind::BloodCastle:
        return Format(I18N::Game::WeeklyGoalBloodCastle, goal.target);
    case WeeklyGoalKind::DevilSquare:
        return Format(I18N::Game::WeeklyGoalDevilSquare, goal.target);
    case WeeklyGoalKind::ChaosCastle:
        return Format(I18N::Game::WeeklyGoalChaosCastle, goal.target);
    case WeeklyGoalKind::Resets:
        return Format(I18N::Game::WeeklyGoalResets, goal.target);
    case WeeklyGoalKind::TreasureChests:
        return Format(I18N::Game::WeeklyGoalTreasureChests, goal.target);
    }
    return Format(I18N::Game::WeeklyGoalGoal, goal.target, StringUtils::NarrowToWide(goal.name).c_str());
}

// The reward with the client's item names, e.g. "1 Jewel of Bless, 1 Box of Kundun +3 and 100
// Goblin Points".
std::string RewardText(const GameLogic::Events::WeeklyGoal& goal)
{
    std::vector<std::wstring> parts;
    const auto add = [&parts](int count, const std::wstring& name)
    {
        if (count > 0)
            parts.push_back(FormatWide(I18N::Game::WeeklyGoalRewardPart, count, name.c_str()));
    };
    add(goal.jewelsOfBless, GetItemName(ITEM_JEWEL_OF_BLESS, 0));
    add(goal.jewelsOfSoul, GetItemName(ITEM_JEWEL_OF_SOUL, 0));
    if (goal.boxOfKundunPlus > 0)
        add(1, GetItemName(ITEM_BOX_OF_LUCK, GameLogic::Events::BoxOfKundunLevel(goal.boxOfKundunPlus)));
    add(goal.goblinPoints, I18N::Game::GoblinPoints);
    return StringUtils::WideToNarrow(
        GameLogic::Events::JoinList(parts, I18N::Game::WeeklyGoalListNext, I18N::Game::WeeklyGoalListLast).c_str());
}

// When the week turns, in the player's local time.
std::string WeekEndText(const GameLogic::Events::WeeklyGoal& goal)
{
    const std::optional<std::tm> weekEnd = GameLogic::Items::LocalExpiryTime(goal.weekEnd);
    if (!weekEnd)
        return {};
    return Format(I18N::Game::WeeklyGoalWeekEnd, weekEnd->tm_mday, weekEnd->tm_mon + 1, weekEnd->tm_hour,
                  weekEnd->tm_min);
}
} // namespace

CWeeklyGoalWindow::~CWeeklyGoalWindow()
{
    Release();
}

bool CWeeklyGoalWindow::Create(CManager* pNewUIMng)
{
    if (nullptr == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_WEEKLY_GOAL, this);

    BuildRmlUi();

    Show(false);

    return true;
}

void CWeeklyGoalWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

bool CWeeklyGoalWindow::UpdateMouseEvent()
{
    // The exit buttons are RmlUi's (see Update()).
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CWeeklyGoalWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_WEEKLY_GOAL) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_WEEKLY_GOAL);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

bool CWeeklyGoalWindow::Update()
{
    const bool exit = m_PendingExit;
    m_PendingExit = false;
    if (IsVisible() && exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_WEEKLY_GOAL);
    }

    SyncRmlModel();
    return true;
}

bool CWeeklyGoalWindow::Render()
{
    // Nothing native: the frame, the texts and the bar are RmlUi. Kept because CObject requires
    // the override.
    return true;
}

float CWeeklyGoalWindow::GetLayerDepth()
{
    return 5.0f;
}

void CWeeklyGoalWindow::BindRmlModel(Rml::DataModelConstructor& c, WeeklyGoalRmlModel& model)
{
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("title", &model.title);
    c.Bind("goal", &model.goal);
    c.Bind("bar_percent", &model.barPercent);
    c.Bind("reached", &model.reached);
    c.Bind("progress", &model.progress);
    c.Bind("status", &model.status);
    c.Bind("contribution", &model.contribution);
    c.Bind("reward_title", &model.rewardTitle);
    c.Bind("reward", &model.reward);
    c.Bind("rewarded", &model.rewarded);
    c.Bind("rewarded_text", &model.rewardedText);
    c.Bind("week_end", &model.weekEnd);
    c.Bind("exit_tooltip", &model.exitTooltip);
    c.BindEventCallback("weekly_goal_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_PendingExit = true; });
}

void CWeeklyGoalWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void CWeeklyGoalWindow::SyncRmlModel()
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
    SyncField(binder, &WeeklyGoalRmlModel::boldTextPx, "bold_text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold));

    // The goal as the server last sent it: its name and target, the bar and the count, then
    // whether it's reached, what the player's account counted, the reward and the week's end.
    // The texts change only with it.
    const GameLogic::Events::WeeklyGoal& goal = GameLogic::Events::CurrentWeeklyGoal();
    if (m_ShownGoal == goal)
        return;
    m_ShownGoal = goal;

    const int percent = GameLogic::Events::ProgressPercent(goal);
    SyncField(binder, &WeeklyGoalRmlModel::title, "title", StringUtils::WideToNarrow(I18N::Game::WeeklyGoalTitle));
    SyncField(binder, &WeeklyGoalRmlModel::goal, "goal", GoalText(goal));
    SyncField(binder, &WeeklyGoalRmlModel::barPercent, "bar_percent", percent);
    SyncField(binder, &WeeklyGoalRmlModel::reached, "reached", goal.reached);
    SyncField(binder, &WeeklyGoalRmlModel::progress, "progress",
              Format(I18N::Game::WeeklyGoalProgress, goal.count, goal.target, percent));
    SyncField(binder, &WeeklyGoalRmlModel::status, "status",
              goal.reached ? Format(I18N::Game::WeeklyGoalReached, goal.experienceBonusPercent)
                           : Format(I18N::Game::WeeklyGoalRemaining, GameLogic::Events::Remaining(goal)));
    SyncField(binder, &WeeklyGoalRmlModel::contribution, "contribution",
              Format(I18N::Game::WeeklyGoalContribution, goal.contribution));
    SyncField(binder, &WeeklyGoalRmlModel::rewardTitle, "reward_title",
              StringUtils::WideToNarrow(I18N::Game::WeeklyGoalRewardTitle));
    SyncField(binder, &WeeklyGoalRmlModel::reward, "reward", RewardText(goal));
    SyncField(binder, &WeeklyGoalRmlModel::rewarded, "rewarded", goal.rewarded);
    SyncField(binder, &WeeklyGoalRmlModel::rewardedText, "rewarded_text",
              StringUtils::WideToNarrow(I18N::Game::WeeklyGoalRewarded));
    SyncField(binder, &WeeklyGoalRmlModel::weekEnd, "week_end", WeekEndText(goal));
    SyncField(binder, &WeeklyGoalRmlModel::exitTooltip, "exit_tooltip", StringUtils::WideToNarrow(I18N::Game::Close388));
}
