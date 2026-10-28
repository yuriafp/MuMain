#pragma once

#include <RmlUi/Core/Types.h>

namespace mu::ui::window
{
struct WeeklyGoalRmlModel
{
    float textPx = 0.f;     // native normal text size in physical px (RmlNativeTextSize.h)
    float boldTextPx = 0.f; // native bold text size

    Rml::String title;
    Rml::String goal;
    // The fill of the progress bar, 0 to 100.
    int barPercent = 0;
    bool reached = false;
    Rml::String progress;
    Rml::String status;
    Rml::String contribution;
    Rml::String rewardTitle;
    Rml::String reward;
    bool rewarded = false;
    Rml::String rewardedText;
    Rml::String weekEnd;
    Rml::String exitTooltip;
};
} // namespace mu::ui::window
