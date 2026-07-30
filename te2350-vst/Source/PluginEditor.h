#pragma once

#include <JuceHeader.h>
#include <functional>
#include "PluginProcessor.h"
#include "Presets/UserPresetManager.h"

class TE2350AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TE2350AudioProcessorEditor(TE2350AudioProcessor&);
    ~TE2350AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class OrbitalLookAndFeel;
    class KnobTile;
    class FaderTile;
    class ComboTile;
    class ToggleTile;
    class GroupLabel;
    class SectionPanel;
    class MeterStrip;
    class GravityMeter;
    class LogoMark;

    juce::Slider& addSlider(juce::Component& parent,
                            std::vector<juce::Component*>& group,
                            const juce::String& parameterID,
                            const juce::String& title,
                            const juce::String& subtitle,
                            juce::Colour accent,
                            bool large = false,
                            const juce::String& suffix = {},
                            double displayScale = 1.0,
                            int decimalPlaces = 1);
    juce::Slider& addFader(juce::Component& parent,
                           std::vector<juce::Component*>& group,
                           const juce::String& parameterID,
                           const juce::String& title,
                           const juce::String& subtitle,
                           juce::Colour accent,
                           const juce::String& suffix = {},
                           double displayScale = 1.0,
                           int decimalPlaces = 1);
    juce::ComboBox& addCombo(juce::Component& parent,
                             std::vector<juce::Component*>& group,
                             const juce::String& parameterID,
                             const juce::String& title,
                             const juce::String& subtitle);
    juce::Button& addToggle(juce::Component& parent,
                            std::vector<juce::Component*>& group,
                            const juce::String& parameterID,
                            const juce::String& title,
                            juce::Colour accent,
                            std::function<bool()> isMomentary = {});
    void layoutGrid(juce::Rectangle<int> area, const std::vector<juce::Component*>& group, int columns);
    void layoutMacroControls(juce::Rectangle<int> area);
    void layoutCoreControls(juce::Rectangle<int> area);
    void layoutAdvancedControls(juce::Rectangle<int> area);
    void resetParametersToDefault();
    void toggleAB();
    void applySnapshot(const juce::ValueTree& snapshot);
    void rebuildPresetSelector();
    void showPresetActionsMenu();
    void promptToSaveUserPreset();
    void confirmDeleteUserPreset();
    void loadUserPreset(int index);
    void showPresetError(const juce::String& message);
    bool hasPresetChanges() const;
    void updatePresetStatus();
    void updateDependentControls();
    static void setControlAvailable(juce::Component* component, bool available);
    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;

    TE2350AudioProcessor& processor;
    te2350::UserPresetManager userPresetManager;
    OrbitalLookAndFeel* orbitalLookAndFeel = nullptr;

    SectionPanel* macroPanel = nullptr;
    SectionPanel* corePanel = nullptr;
    SectionPanel* advancedPanel = nullptr;
    SectionPanel* utilityPanel = nullptr;
    GravityMeter* gravityMeter = nullptr;
    MeterStrip* inputMeter = nullptr;
    MeterStrip* outputMeter = nullptr;
    MeterStrip* feedbackMeter = nullptr;
    GroupLabel* corePerformLabel = nullptr;
    GroupLabel* coreColorLabel = nullptr;
    GroupLabel* advancedMotionLabel = nullptr;
    GroupLabel* advancedTextureLabel = nullptr;
    LogoMark* logoMark = nullptr;

    juce::Component macroLayer;
    juce::Component coreLayer;
    juce::Component advancedLayer;
    juce::Component utilityLayer;

    juce::ComboBox presetSelector;
    juce::Label presetCaption;
    juce::TextButton presetActionsButton { "..." };
    juce::TextButton abButton { "A/B  A" };
    juce::TextButton resetButton { "RESET" };
    juce::TextButton advancedToggle { "SCULPT" };
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::TextButton mutateButton { "MUTATE" };
    juce::TooltipWindow tooltipWindow { this, 700 };
    std::unique_ptr<juce::AlertWindow> savePresetDialog;

    std::vector<std::unique_ptr<juce::Component>> ownedComponents;
    std::vector<juce::Component*> macroControls;
    std::vector<juce::Component*> corePrimaryControls;
    std::vector<juce::Component*> coreSecondaryControls;
    std::vector<juce::Component*> advancedMotionControls;
    std::vector<juce::Component*> advancedTextureControls;
    std::vector<juce::Component*> utilityControls;
    std::vector<juce::Component*> macroPerformanceControls;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    juce::ValueTree snapshotA;
    juce::ValueTree snapshotB;
    juce::ValueTree loadedPresetSnapshot;
    juce::Component* timeControl = nullptr;
    juce::Component* shimmerFeedbackControl = nullptr;
    juce::Component* shimmerIntervalControl = nullptr;
    juce::Component* duckThresholdControl = nullptr;
    bool showingSnapshotA = true;
    bool advancedExpanded = false;
    bool presetDirty = false;
    bool rebuildingPresetSelector = false;
    int selectedUserPreset = -1;
    int observedProgramIndex = 0;
    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TE2350AudioProcessorEditor)
};
