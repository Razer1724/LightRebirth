#include "HostEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace
{
    constexpr int kAudioChannels = 2;
}

class HostEngine::MeterProcessor final : public juce::AudioProcessor
{
public:
    explicit MeterProcessor(const juce::String& processorName)
        : juce::AudioProcessor(BusesProperties()
                                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          name(processorName)
    {
    }

    const juce::String getName() const override { return name; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        float blockPeak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            blockPeak = juce::jmax(blockPeak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

        float previous = peak.load(std::memory_order_relaxed);
        while (blockPeak > previous
               && !peak.compare_exchange_weak(previous, blockPeak,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed))
        {
        }
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    float takePeak() noexcept { return peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    juce::String name;
    std::atomic<float> peak { 0.0f };
};

// Two of these bracket every plugin in the graph. The chain is processed in
// order on one thread, so the time between the "start" and "end" probe is the
// time the plugin spent in processBlock, expressed as a share of the block.
class HostEngine::TimingProbe final : public juce::AudioProcessor
{
public:
    enum class Role { start, end };

    TimingProbe(Role roleIn, std::shared_ptr<PluginTiming> timingIn)
        : juce::AudioProcessor(BusesProperties()
                                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          role(roleIn), timing(std::move(timingIn))
    {
    }

    const juce::String getName() const override { return "Timing Probe"; }
    void prepareToPlay(double newSampleRate, int) override { rate = newSampleRate; }
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const auto now = juce::Time::getHighResolutionTicks();

        if (role == Role::start)
        {
            timing->startTicks.store(now, std::memory_order_relaxed);
            return;
        }

        const auto start = timing->startTicks.load(std::memory_order_relaxed);
        if (start == 0 || rate <= 0.0 || buffer.getNumSamples() == 0)
            return;

        const double elapsed = juce::Time::highResolutionTicksToSeconds(now - start);
        const double budget = static_cast<double>(buffer.getNumSamples()) / rate;
        const float ratio = static_cast<float>(elapsed / budget);
        const float previous = timing->load.load(std::memory_order_relaxed);
        timing->load.store(previous + 0.1f * (ratio - previous), std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    Role role;
    std::shared_ptr<PluginTiming> timing;
    double rate = 0.0;
};

class HostEngine::ScanJob final : public juce::ThreadPoolJob
{
public:
    ScanJob(HostEngine& ownerIn, std::function<void()> callbackIn)
        : juce::ThreadPoolJob("VST3 scanner"), owner(ownerIn), callback(std::move(callbackIn))
    {
    }

    JobStatus runJob() override
    {
        if (shouldExit())
            return jobHasFinished;

        owner.scanForPlugins([this] { return shouldExit(); });
        if (shouldExit())
            return jobHasFinished;

        auto finished = std::move(callback);
        juce::MessageManager::callAsync([finished = std::move(finished)]() mutable
        {
            if (finished)
                finished();
        });
        return jobHasFinished;
    }

private:
    HostEngine& owner;
    std::function<void()> callback;
};

HostEngine::HostEngine()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
#endif

    // The app used to be called ToneChain. Carry its saved audio settings and
    // plugin cache over the first time the renamed app runs.
    const auto dataDirectory = getApplicationDataDirectory();
    const auto oldDataDirectory = dataDirectory.getSiblingFile("ToneChain");
    for (const char* fileName : { "audio-settings.xml", "plugins.xml" })
    {
        const auto oldFile = oldDataDirectory.getChildFile(fileName);
        const auto newFile = dataDirectory.getChildFile(fileName);
        if (oldFile.existsAsFile() && !newFile.exists())
        {
            dataDirectory.createDirectory();
            oldFile.copyFileTo(newFile);
        }
    }

    loadPluginCache();
}

HostEngine::~HostEngine()
{
    lifetimeToken.reset();
    shutdown();
    scanPool.removeAllJobs(true, 30000);
    savePluginCache();
}

juce::String HostEngine::initialise()
{
    if (initialised)
        return {};

    // Restore the last-used device/sample rate/buffer size if we have them.
    // If that device is gone, the device manager falls back to the default.
    std::unique_ptr<juce::XmlElement> savedAudioState;
    const auto settingsFile = getAudioSettingsFile();
    if (settingsFile.existsAsFile())
        savedAudioState = juce::XmlDocument::parse(settingsFile);

    const auto error = deviceManager.initialise(kAudioChannels, kAudioChannels,
                                                savedAudioState.get(), true);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = device->getCurrentSampleRate();
        bufferSize = device->getCurrentBufferSizeSamples();
    }

    graph.setPlayConfigDetails(kAudioChannels, kAudioChannels, sampleRate, bufferSize);
    buildGraph();

    audioPlayer.setProcessor(&graph);
    deviceManager.addAudioCallback(&audioPlayer);
    deviceManager.addChangeListener(this);
    saveAudioSettings();
    initialised = true;
    return error;
}

void HostEngine::shutdown()
{
    if (!initialised)
        return;

    deviceManager.removeChangeListener(this);
    onDeviceChanged = nullptr;
    deviceManager.removeAudioCallback(&audioPlayer);
    audioPlayer.setProcessor(nullptr);
    initialised = false;
}

void HostEngine::scanForPluginsAsync(std::function<void()> onFinished)
{
    scanPool.addJob(new ScanJob(*this, std::move(onFinished)), true);
}

void HostEngine::scanForPlugins(const std::function<bool()>& shouldStop)
{
#if JUCE_PLUGINHOST_VST3
    auto* format = formatManager.getFormat(0);
    if (format == nullptr)
        return;

    const auto folder = getApplicationDataDirectory();
    folder.createDirectory();

    // The scanner records each file in the "dead man's pedal" file before loading
    // it and clears it afterwards. If a plugin crashes the app mid-scan, the next
    // scan finds it still listed and blacklists it instead of crashing again.
    juce::PluginDirectoryScanner scanner(knownPlugins, *format,
                                         format->getDefaultLocationsToSearch(),
                                         true,
                                         folder.getChildFile("scan-crash-guard.txt"),
                                         true);

    juce::String scannedName;
    bool completed = false;

    for (;;)
    {
        if (shouldStop && shouldStop())
            break;

        scannedName.clear();
        const bool moreToScan = scanner.scanNextFile(true, scannedName);

        // Only set when a file was actually loaded (not skipped as up to date).
        // Save as we go so a crash later in the scan doesn't lose earlier results.
        if (scannedName.isNotEmpty())
            savePluginCache();

        if (!moreToScan)
        {
            completed = true;
            break;
        }
    }

    if (completed)
    {
        // Drop entries for plugins that have been uninstalled since the last scan.
        // getTypes() returns a copy, so removing while iterating is safe.
        for (const auto& description : knownPlugins.getTypes())
            if (!format->doesPluginStillExist(description))
                knownPlugins.removeType(description);

        savePluginCache();
    }
#else
    juce::ignoreUnused(shouldStop);
#endif
}

juce::Array<juce::PluginDescription> HostEngine::getAvailablePlugins() const
{
    juce::Array<juce::PluginDescription> result;

    // This first release is deliberately a stereo audio-effects host. Hide
    // instruments and plugins whose scanned default layout cannot accept and
    // produce stereo audio, rather than presenting unsupported choices.
    for (const auto& description : knownPlugins.getTypes())
    {
        if (!description.isInstrument
            && description.numInputChannels >= kAudioChannels
            && description.numOutputChannels >= kAudioChannels)
        {
            result.add(description);
        }
    }

    std::sort(result.begin(), result.end(), [](const juce::PluginDescription& a,
                                                const juce::PluginDescription& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    return result;
}

void HostEngine::addPlugin(const juce::PluginDescription& description,
                           std::function<void(const juce::String&)> completion)
{
    if (!initialised)
    {
        if (completion)
            completion("The audio engine is not running.");
        return;
    }

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = device->getCurrentSampleRate();
        bufferSize = device->getCurrentBufferSizeSamples();
    }

    const std::weak_ptr<int> weakLifetime = lifetimeToken;
    formatManager.createPluginInstanceAsync(
        description, sampleRate, bufferSize,
        [this, weakLifetime, description, completion = std::move(completion)]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable
        {
            if (weakLifetime.expired())
                return;

            auto instanceHolder = std::make_shared<std::unique_ptr<juce::AudioPluginInstance>>(std::move(instance));
            juce::MessageManager::callAsync(
                [this, weakLifetime, description, instanceHolder, error,
                 completion = std::move(completion)]() mutable
                {
                    if (weakLifetime.expired())
                        return;

                    juce::String result = error;
                    if (*instanceHolder != nullptr)
                    {
                        auto node = graph.addNode(std::move(*instanceHolder));
                        if (node != nullptr)
                        {
                            auto timing = std::make_shared<PluginTiming>();
                            auto startProbe = graph.addNode(std::make_unique<TimingProbe>(TimingProbe::Role::start, timing));
                            auto endProbe = graph.addNode(std::make_unique<TimingProbe>(TimingProbe::Role::end, timing));
                            plugins.push_back({ node, description, false, startProbe, endProbe, timing });
                            rebuildConnections();
                            result.clear();
                        }
                        else
                        {
                            result = "JUCE could not add this plugin to the audio graph.";
                        }
                    }
                    else if (result.isEmpty())
                    {
                        result = "The plugin could not be loaded.";
                    }

                    if (completion)
                        completion(result);
                });
        });
}

int HostEngine::getNumPlugins() const
{
    return static_cast<int>(plugins.size());
}

HostEngine::PluginInfo HostEngine::getPluginInfo(int index) const
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(plugins.size())))
        return {};

    const auto& slot = plugins[static_cast<size_t>(index)];
    return { slot.description.name,
             slot.description.manufacturerName,
             slot.description.pluginFormatName,
             slot.bypassed,
             slot.node != nullptr ? slot.node->nodeID.uid : 0 };
}

juce::AudioProcessor* HostEngine::getPluginProcessor(int index) const
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(plugins.size())))
        return nullptr;

    const auto& node = plugins[static_cast<size_t>(index)].node;
    return node != nullptr ? node->getProcessor() : nullptr;
}

void HostEngine::setPluginBypassed(int index, bool shouldBypass)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(plugins.size())))
        return;

    auto& slot = plugins[static_cast<size_t>(index)];
    if (slot.bypassed == shouldBypass)
        return;

    slot.bypassed = shouldBypass;
    rebuildConnections();
}

void HostEngine::movePlugin(int from, int to)
{
    if (!juce::isPositiveAndBelow(from, static_cast<int>(plugins.size()))
        || !juce::isPositiveAndBelow(to, static_cast<int>(plugins.size()))
        || from == to)
        return;

    auto item = std::move(plugins[static_cast<size_t>(from)]);
    plugins.erase(plugins.begin() + from);
    plugins.insert(plugins.begin() + to, std::move(item));
    rebuildConnections();
}

void HostEngine::removePlugin(int index)
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(plugins.size())))
        return;

    const auto& slot = plugins[static_cast<size_t>(index)];
    const auto nodeId = slot.node->nodeID;
    if (slot.startProbe != nullptr)
        graph.removeNode(slot.startProbe->nodeID);
    if (slot.endProbe != nullptr)
        graph.removeNode(slot.endProbe->nodeID);
    graph.removeNode(nodeId);
    plugins.erase(plugins.begin() + index);
    rebuildConnections();
}

void HostEngine::setChainEnabled(bool shouldBeEnabled)
{
    if (chainEnabled == shouldBeEnabled)
        return;

    chainEnabled = shouldBeEnabled;
    rebuildConnections();
}

float HostEngine::takeInputPeak() noexcept
{
    return inputMeter != nullptr ? inputMeter->takePeak() : 0.0f;
}

float HostEngine::takeOutputPeak() noexcept
{
    return outputMeter != nullptr ? outputMeter->takePeak() : 0.0f;
}

double HostEngine::getRoundTripLatencyMs() const
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return -1.0;

    const double rate = device->getCurrentSampleRate();
    if (rate <= 0.0)
        return -1.0;

    double samples = static_cast<double>(device->getInputLatencyInSamples()
                                         + device->getOutputLatencyInSamples());

    if (chainEnabled)
        for (const auto& slot : plugins)
            if (!slot.bypassed && slot.node != nullptr && slot.node->getProcessor() != nullptr)
                samples += static_cast<double>(slot.node->getProcessor()->getLatencySamples());

    return samples * 1000.0 / rate;
}

double HostEngine::getPluginLatencyMs(int index) const
{
    auto* processor = getPluginProcessor(index);
    if (processor == nullptr || sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(processor->getLatencySamples()) * 1000.0 / sampleRate;
}

float HostEngine::getPluginLoad(int index) const noexcept
{
    if (!juce::isPositiveAndBelow(index, static_cast<int>(plugins.size())))
        return 0.0f;

    const auto& slot = plugins[static_cast<size_t>(index)];
    if (slot.bypassed || !chainEnabled || slot.timing == nullptr)
        return 0.0f;

    return slot.timing->load.load(std::memory_order_relaxed);
}

double HostEngine::getCpuUsage() const
{
    return deviceManager.getCpuUsage();
}

juce::String HostEngine::getCurrentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();

    return "No audio device";
}

juce::String HostEngine::getDeviceSummary() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const juce::CharPointer_UTF8 separator("  \xc2\xb7  ");
        return device->getName()
             + juce::String(separator)
             + juce::String(device->getCurrentSampleRate() / 1000.0, 1) + " kHz"
             + juce::String(separator)
             + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    }

    return "No audio device";
}

void HostEngine::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = device->getCurrentSampleRate();
        bufferSize = device->getCurrentBufferSizeSamples();
    }

    saveAudioSettings();

    // The channel layout may have changed along with the device, so re-wire the
    // chain against the graph as it is now.
    rebuildConnections();

    if (onDeviceChanged)
        onDeviceChanged();
}

void HostEngine::saveAudioSettings() const
{
    if (auto xml = deviceManager.createStateXml())
    {
        const auto file = getAudioSettingsFile();
        file.getParentDirectory().createDirectory();
        file.replaceWithText(xml->toString());
    }
}

juce::File HostEngine::getAudioSettingsFile() const
{
    return getApplicationDataDirectory().getChildFile("audio-settings.xml");
}

void HostEngine::loadPluginCache()
{
    const auto cache = getApplicationDataDirectory().getChildFile("plugins.xml");
    if (!cache.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(cache))
        knownPlugins.recreateFromXml(*xml);
}

void HostEngine::savePluginCache() const
{
    const auto folder = getApplicationDataDirectory();
    folder.createDirectory();

    if (auto xml = knownPlugins.createXml())
        folder.getChildFile("plugins.xml").replaceWithText(xml->toString());
}

juce::File HostEngine::getApplicationDataDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("LightRebirth");
}

void HostEngine::buildGraph()
{
    graph.clear();
    plugins.clear();

    using IOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

    inputNode = graph.addNode(std::make_unique<IOProcessor>(IOProcessor::audioInputNode));
    inputMeterNode = graph.addNode(std::make_unique<MeterProcessor>("Input Meter"));
    outputMeterNode = graph.addNode(std::make_unique<MeterProcessor>("Output Meter"));
    outputNode = graph.addNode(std::make_unique<IOProcessor>(IOProcessor::audioOutputNode));

    inputMeter = static_cast<MeterProcessor*>(inputMeterNode->getProcessor());
    outputMeter = static_cast<MeterProcessor*>(outputMeterNode->getProcessor());

    rebuildConnections();
}

void HostEngine::rebuildConnections()
{
    const auto existingConnections = graph.getConnections();
    for (const auto& connection : existingConnections)
        graph.removeConnection(connection, juce::AudioProcessorGraph::UpdateKind::none);

    // AudioProcessorGraph renders every node it owns, connected or not. Simply
    // leaving a bypassed plugin (or the whole chain, when it is switched off)
    // unconnected therefore does NOT stop it running: it keeps calling
    // processBlock on silence every callback. That inflated the CPU readout with
    // work the UI claims isn't happening, and because a
    // disconnected node can be scheduled anywhere, it could also land between an
    // active plugin's two timing probes and be billed to that plugin.
    // Suspended processors are skipped by the graph (their buffer is just cleared).
    for (auto& slot : plugins)
    {
        const bool active = chainEnabled && !slot.bypassed;

        for (const auto& node : { slot.node, slot.startProbe, slot.endProbe })
            if (node != nullptr && node->getProcessor() != nullptr)
                node->getProcessor()->suspendProcessing(!active);

        if (!active && slot.timing != nullptr)
        {
            slot.timing->startTicks.store(0, std::memory_order_relaxed);
            slot.timing->load.store(0.0f, std::memory_order_relaxed);
        }
    }

    auto previous = inputNode;
    if (inputMeterNode != nullptr)
    {
        connectAudioNodes(previous, inputMeterNode);
        previous = inputMeterNode;
    }

    if (chainEnabled)
    {
        for (auto& slot : plugins)
        {
            if (slot.bypassed || slot.node == nullptr)
                continue;

            if (slot.startProbe != nullptr && slot.endProbe != nullptr)
            {
                connectAudioNodes(previous, slot.startProbe);
                connectAudioNodes(slot.startProbe, slot.node);
                connectAudioNodes(slot.node, slot.endProbe);
                previous = slot.endProbe;
            }
            else
            {
                connectAudioNodes(previous, slot.node);
                previous = slot.node;
            }
        }
    }

    if (outputMeterNode != nullptr)
    {
        connectAudioNodes(previous, outputMeterNode);
        previous = outputMeterNode;
    }

    if (outputNode != nullptr)
        connectAudioNodes(previous, outputNode);

    graph.rebuild();
}

void HostEngine::connectAudioNodes(const juce::AudioProcessorGraph::Node::Ptr& source,
                                   const juce::AudioProcessorGraph::Node::Ptr& destination)
{
    if (source == nullptr || destination == nullptr)
        return;

    // This first version is intentionally a stereo effects host. Matching the
    // two-channel graph wiring used in JUCE's cascading-effects tutorial also
    // handles the special graph input/output nodes consistently.
    for (int channel = 0; channel < kAudioChannels; ++channel)
    {
        graph.addConnection({ { source->nodeID, channel }, { destination->nodeID, channel } },
                             juce::AudioProcessorGraph::UpdateKind::none);
    }
}