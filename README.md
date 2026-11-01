# LightRebirth

A small Windows VST3 effects host built with C++ and JUCE. Pick your audio device, stack some effects, and run your mic or instrument through them in real time.

## Using it

1. Start LightRebirth. It scans your VST3 folders in the background.
2. Click **Audio** and choose your input/output device and buffer size.
3. Click **+ Add plugin** and pick an effect.
4. Click a plugin card to open its editor. Use the switch on the card to bypass it.
5. **Chain on/off** bypasses everything at once.
6. **Settings** has two options: keep running in the system tray, and open with Windows.

Use headphones when testing a microphone, otherwise you may get feedback.

## Limitations

- Effects only: no instruments or MIDI, and stereo plugins only
- The loaded chain is not saved yet, so you rebuild it each time you start the app
- Plugins run inside the app, so a plugin crash will close LightRebirth

## Where settings are stored

`%APPDATA%\LightRebirth` holds your audio settings, app settings, and the plugin scan cache. Settings from the old ToneChain name are copied over automatically.
