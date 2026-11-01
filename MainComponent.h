#pragma once

#include "AppSettings.h"
#include "HostEngine.h"

#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <memory>
#include <vector>

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    // Closes plugin editors and dialogs. Used when the main window goes to the tray.
    void closeAuxiliaryWindows();

private:
    class ToneLookAndFeel;
    class PluginRow;
    class ChainView;
    class PluginEditorWindow;
    class PreferencesWindow;
    class SettingsWindow;

    void timerCallback() override;
    void refreshPluginRows();
    void showPluginMenu();
    void showRowMenu(int index);
    void startPluginScan();
    void addPlugin(const juce::PluginDescription& description);
    void openPluginEditor(int index);
    void toggleBypass(int index);
    void movePlugin(int from, int to);
    void removePlugin(int index);
    void showPreferences();
    void showSettings();
    void updateHeader();
    void updateDeviceLine();
    void updateFooterText();
    void setStatus(const juce::String& text);
    int indexOfNode(juce::uint32 nodeId) const;
    void drawMeter(juce::Graphics& graphics, juce::Rectangle<float> bounds,
                   const juce::String& title, float peak) const;

    HostEngine engine;
    std::unique_ptr<ToneLookAndFeel> lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Viewport chainViewport;
    ChainView* chainView = nullptr;

    juce::TextButton addButton { "+ Add plugin" };
    juce::TextButton preferencesButton { "Audio" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton chainButton { "Chain on" };
    juce::Label footerLabel;

    std::map<juce::uint32, std::unique_ptr<PluginEditorWindow>> editorWindows;
    std::unique_ptr<PreferencesWindow> preferencesWindow;
    juce::SharedResourcePointer<AppSettings> appSettings;
    std::unique_ptr<SettingsWindow> settingsWindow;

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    bool scanInProgress = false;
    juce::String statusText;
    int statusTicks = 0;              // timer ticks left to show statusText in the footer
    juce::String latencyText { "-- ms" };
    juce::String cpuText { "0%" };
    float cpuLoad = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};