#include "AppSettings.h"
#include "MainComponent.h"

namespace
{
    // Simple generated icon (dark tile, teal dot, matching the app's colours).
    // Swap in a real icon image here whenever you have one.
    juce::Image makeTrayIcon()
    {
        juce::Image image(juce::Image::ARGB, 64, 64, true);
        juce::Graphics g(image);
        g.setColour(juce::Colour(0xff101419));
        g.fillRoundedRectangle(2.0f, 2.0f, 60.0f, 60.0f, 14.0f);
        g.setColour(juce::Colour(0xff2bd0c4));
        g.fillEllipse(18.0f, 18.0f, 28.0f, 28.0f);
        return image;
    }
}

// The icon in the Windows notification area. Click to bring the window back,
// right-click for Open / Quit.
class TrayIcon final : public juce::SystemTrayIconComponent
{
public:
    TrayIcon(std::function<void()> openCallback, std::function<void()> quitCallback)
        : onOpen(std::move(openCallback)), onQuit(std::move(quitCallback))
    {
        const auto icon = makeTrayIcon();
        setIconImage(icon, icon);
        setIconTooltip("LightRebirth");
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!e.mods.isPopupMenu())
        {
            if (onOpen)
                onOpen();
            return;
        }

        juce::PopupMenu menu;
        menu.addItem(1, "Open LightRebirth");
        menu.addSeparator();
        menu.addItem(2, "Quit");

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [safeThis = juce::Component::SafePointer<TrayIcon>(this)](int result)
            {
                if (safeThis == nullptr)
                    return;

                if (result == 1 && safeThis->onOpen)
                    safeThis->onOpen();
                else if (result == 2 && safeThis->onQuit)
                    safeThis->onQuit();
            });
    }

private:
    std::function<void()> onOpen;
    std::function<void()> onQuit;
};

class LightRebirthApplication final : public juce::JUCEApplication,
                                      private juce::ChangeListener
{
public:
    const juce::String getApplicationName() override { return "LightRebirth"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    // With the tray option on there is only ever one instance: launching the app
    // again brings the hidden window back rather than starting a second host that
    // would fight the first one for the audio device.
    bool moreThanOneInstanceAllowed() override { return !settings->getKeepInTray(); }

    void initialise(const juce::String& commandLine) override
    {
        settings->addChangeListener(this);

        // Windows passes --startup when it launches us at sign-in.
        const bool startHidden = settings->getKeepInTray()
                              && commandLine.containsIgnoreCase("--startup");

        mainWindow = std::make_unique<MainWindow>(getApplicationName(), startHidden,
                                                  [this] { closeRequested(); },
                                                  [this] { minimiseRequested(); });
        updateTrayIcon();
    }

    void shutdown() override
    {
        settings->removeChangeListener(this);
        trayIcon.reset();
        mainWindow.reset();
    }

    // Also what Windows calls when it is logging off or shutting down, so this
    // always really quits.
    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override
    {
        restoreFromTray();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, bool startHidden,
                   std::function<void()> onClose, std::function<void()> onMinimise)
            : juce::DocumentWindow(name, juce::Colour(0xff101419),
                                   juce::DocumentWindow::allButtons),
              closeCallback(std::move(onClose)), minimiseCallback(std::move(onMinimise))
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(600, 420, 1000, 1600);
            setContentOwned(new MainComponent(), true);
            centreWithSize(640, 560);
            setVisible(!startHidden);
        }

        void closeButtonPressed() override
        {
            if (closeCallback)
                closeCallback();
        }

        void minimiseButtonPressed() override
        {
            if (minimiseCallback)
                minimiseCallback();
        }

        // Plugin editors and dialogs are separate windows, so they have to be
        // closed explicitly when this one goes to the tray.
        void closeAuxiliaryWindows()
        {
            if (auto* content = dynamic_cast<MainComponent*>(getContentComponent()))
                content->closeAuxiliaryWindows();
        }

    private:
        std::function<void()> closeCallback;
        std::function<void()> minimiseCallback;
    };

    void closeRequested()
    {
        if (settings->getKeepInTray())
        {
            updateTrayIcon();
            hideToTray();
        }
        else
        {
            systemRequestedQuit();
        }
    }

    void minimiseRequested()
    {
        if (settings->getKeepInTray())
        {
            updateTrayIcon();
            hideToTray();
        }
        else if (mainWindow != nullptr)
        {
            mainWindow->setMinimised(true);
        }
    }

    void hideToTray()
    {
        if (mainWindow == nullptr)
            return;

        mainWindow->closeAuxiliaryWindows();
        mainWindow->setVisible(false);
    }

    void restoreFromTray()
    {
        if (mainWindow == nullptr)
            return;

        mainWindow->setVisible(true);
        mainWindow->setMinimised(false);
        mainWindow->toFront(true);
    }

    void updateTrayIcon()
    {
        if (settings->getKeepInTray())
        {
            if (trayIcon == nullptr)
                trayIcon = std::make_unique<TrayIcon>([this] { restoreFromTray(); },
                                                      [this] { systemRequestedQuit(); });
        }
        else
        {
            trayIcon.reset();

            // Without the icon there would be no way back to a hidden window.
            if (mainWindow != nullptr && !mainWindow->isVisible())
                restoreFromTray();
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        updateTrayIcon();
    }

    juce::SharedResourcePointer<AppSettings> settings;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(LightRebirthApplication)