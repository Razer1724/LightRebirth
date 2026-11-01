#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class HostEngine : private juce::ChangeListener
{
public:
    struct PluginInfo
    {
        juce::String name;
        juce::String manufacturer;
        juce::String format;
        bool bypassed = false;
        juce::uint32 nodeId = 0;
    };

    HostEngine();
    ~HostEngine();

    HostEngine(const HostEngine&) = delete;
    HostEngine& operator=(const HostEngine&) = delete;

    juce::String initialise();
    void shutdown();

    void scanForPluginsAsync(std::function<void()> onFinished);
    juce::Array<juce::PluginDescription> getAvailablePlugins() const;
    void addPlugin(const juce::PluginDescription& description,
                   std::function<void(const juce::String&)> completion);

    int getNumPlugins() const;
    PluginInfo getPluginInfo(int index) const;
    juce::AudioProcessor* getPluginProcessor(int index) const;
    void setPluginBypassed(int index, bool shouldBypass);
    void movePlugin(int from, int to);
    void removePlugin(int index);

    void setChainEnabled(bool shouldBeEnabled);
    bool isChainEnabled() const noexcept { return chainEnabled; }

    float takeInputPeak() noexcept;
    float takeOutputPeak() noexcept;

    // Live statistics, read from the message thread.
    // Round trip = driver-reported input + output latency plus the latency every
    // active plugin reports. Returns a negative value when there is no device.
    double getRoundTripLatencyMs() const;
    double getPluginLatencyMs(int index) const;   // delay this plugin adds on its own
    float getPluginLoad(int index) const noexcept; // share of the audio block time, 0..1
    double getCpuUsage() const;                    // whole audio callback, 0..1

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    const juce::AudioDeviceManager& getDeviceManager() const noexcept { return deviceManager; }
    juce::String getCurrentDeviceName() const;
    juce::String getDeviceSummary() const;   // "name · 48.0 kHz · 256 samples"

    // Called on the message thread after the audio device, sample rate, buffer
    // size or channel setup changed (e.g. from the preferences window).
    std::function<void()> onDeviceChanged;
    double getSampleRate() const noexcept { return sampleRate; }
    int getBufferSize() const noexcept { return bufferSize; }

private:
    class MeterProcessor;
    class TimingProbe;
    class ScanJob;

    // Written by the audio thread (TimingProbe), read by the UI.
    struct PluginTiming
    {
        std::atomic<juce::int64> startTicks { 0 };
        std::atomic<float> load { 0.0f };
    };

    struct PluginSlot
    {
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::PluginDescription description;
        bool bypassed = false;
        juce::AudioProcessorGraph::Node::Ptr startProbe;
        juce::AudioProcessorGraph::Node::Ptr endProbe;
        std::shared_ptr<PluginTiming> timing;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void saveAudioSettings() const;
    juce::File getAudioSettingsFile() const;

    void loadPluginCache();
    void savePluginCache() const;
    void scanForPlugins(const std::function<bool()>& shouldStop);
    void buildGraph();
    void rebuildConnections();
    void connectAudioNodes(const juce::AudioProcessorGraph::Node::Ptr& source,
                           const juce::AudioProcessorGraph::Node::Ptr& destination);
    juce::File getApplicationDataDirectory() const;

    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer audioPlayer;
    juce::AudioProcessorGraph graph;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::ThreadPool scanPool { 1 };
    std::shared_ptr<int> lifetimeToken = std::make_shared<int>(0);

    juce::AudioProcessorGraph::Node::Ptr inputNode;
    juce::AudioProcessorGraph::Node::Ptr outputNode;
    juce::AudioProcessorGraph::Node::Ptr inputMeterNode;
    juce::AudioProcessorGraph::Node::Ptr outputMeterNode;
    MeterProcessor* inputMeter = nullptr;
    MeterProcessor* outputMeter = nullptr;

    std::vector<PluginSlot> plugins;
    bool chainEnabled = true;
    bool initialised = false;
    double sampleRate = 44100.0;
    int bufferSize = 512;
};