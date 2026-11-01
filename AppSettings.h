#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

// Application-level options (as opposed to the audio device setup, which the
// HostEngine saves itself). One instance is shared through
// juce::SharedResourcePointer<AppSettings>. It broadcasts a change message
// whenever an option changes, so the app can react (e.g. show or hide the tray icon).
class AppSettings final : public juce::ChangeBroadcaster
{
public:
    AppSettings()
    {
        const auto folder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("LightRebirth");
        folder.createDirectory();

        juce::PropertiesFile::Options options;
        options.applicationName = "LightRebirth";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.millisecondsBeforeSaving = 0;   // write straight away on every change

        file = std::make_unique<juce::PropertiesFile>(folder.getChildFile("settings.xml"), options);
    }

    // Closing or minimising the window hides it to the system tray instead of
    // quitting or minimising to the taskbar.
    bool getKeepInTray() const                 { return file->getBoolValue("keepInTray", false); }

    void setKeepInTray(bool shouldKeep)
    {
        if (shouldKeep == getKeepInTray())
            return;

        file->setValue("keepInTray", shouldKeep);
        file->saveIfNeeded();
        sendChangeMessage();
    }

    // Start with Windows. The truth lives in the registry (the user can also
    // change it from Task Manager > Startup), so this reads it back from there
    // instead of keeping a copy that could go stale.
    static bool isLaunchAtStartupSupported()
    {
       #if JUCE_WINDOWS
        return true;
       #else
        return false;
       #endif
    }

    bool getLaunchAtStartup() const
    {
       #if JUCE_WINDOWS
        return juce::WindowsRegistry::valueExists(getStartupRegistryPath());
       #else
        return false;
       #endif
    }

    // Returns false if Windows refused the change.
    bool setLaunchAtStartup(bool shouldLaunch)
    {
       #if JUCE_WINDOWS
        const auto path = getStartupRegistryPath();
        bool ok = true;

        if (shouldLaunch)
        {
            // "--startup" lets the app know Windows started it, so it can come up
            // hidden in the tray instead of throwing a window at you at sign-in.
            const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                 .getFullPathName();
            ok = juce::WindowsRegistry::setValue(path, "\"" + exe + "\" --startup");
        }
        else if (juce::WindowsRegistry::valueExists(path))
        {
            ok = juce::WindowsRegistry::deleteValue(path);
        }

        sendChangeMessage();
        return ok;
       #else
        juce::ignoreUnused(shouldLaunch);
        return false;
       #endif
    }

private:
   #if JUCE_WINDOWS
    static juce::String getStartupRegistryPath()
    {
        return "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\LightRebirth";
    }
   #endif

    std::unique_ptr<juce::PropertiesFile> file;
};
