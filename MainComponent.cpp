#include "MainComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    // ---- palette ----------------------------------------------------------
    const juce::Colour backgroundColour { 0xff0e1217 };
    const juce::Colour toolbarColour    { 0xff121820 };
    const juce::Colour surfaceColour    { 0xff171e27 };
    const juce::Colour surfaceHover     { 0xff1d2631 };
    const juce::Colour surfaceActive    { 0xff243040 };
    const juce::Colour borderColour     { 0xff263140 };
    const juce::Colour primaryText      { 0xffe8edf2 };
    const juce::Colour secondaryText    { 0xff8d9aa8 };
    const juce::Colour dimText          { 0xff627080 };
    const juce::Colour accentColour     { 0xff2bd0c4 };
    const juce::Colour onAccentColour   { 0xff062421 };
    const juce::Colour warnColour       { 0xfff2b84b };
    const juce::Colour dangerColour     { 0xffef5b5b };
    const juce::Colour trackColour      { 0xff222c38 };
    const juce::Colour switchOffColour  { 0xff2c3745 };

    // ---- layout (pixels) -----------------------------------------------------
    constexpr int kMargin       = 24;

    // header
    constexpr int kHeaderTop    = 18;
    constexpr int kLogoSize     = 38;
    constexpr int kLogoGap      = 12;
    constexpr int kStatH        = 48;
    constexpr int kLatencyW     = 118;
    constexpr int kCpuW         = 64;
    constexpr int kStatGap      = 22;

    // level meters
    constexpr int kMeterTop     = 86;
    constexpr int kMeterH       = 44;

    // signal chain
    constexpr int kSectionTop   = 146;
    constexpr int kSectionH     = 28;
    constexpr int kListTop      = 180;
    constexpr int kCardH        = 64;
    constexpr int kCardGap      = 8;
    constexpr int kCardRadius   = 12;
    constexpr int kCardPitch    = kCardH + kCardGap;   // distance from one card to the next
    constexpr int kEmptyH       = 150;

    // bottom toolbar
    constexpr int kBarHeight    = 72;
    constexpr int kButtonH      = 40;

    // plugin card columns
    constexpr int kSwitchW      = 36;
    constexpr int kSwitchH      = 20;
    constexpr int kColDelay     = 76;
    constexpr int kColCpu       = 52;
    constexpr int kColMenu      = 44;

    juce::Rectangle<int> getLogoBounds()
    {
        return { kMargin, kHeaderTop + 2, kLogoSize, kLogoSize };
    }

    juce::Rectangle<int> getCpuBounds(int componentWidth)
    {
        return { componentWidth - kMargin - kCpuW, kHeaderTop, kCpuW, kStatH };
    }

    juce::Rectangle<int> getLatencyBounds(int componentWidth)
    {
        return { getCpuBounds(componentWidth).getX() - kStatGap - kLatencyW, kHeaderTop, kLatencyW, kStatH };
    }

    juce::Rectangle<int> getMeterCardBounds(int componentWidth)
    {
        return { kMargin, kMeterTop, componentWidth - 2 * kMargin, kMeterH };
    }

    juce::Rectangle<float> getMeterBounds(int componentWidth, bool output)
    {
        const auto inner = getMeterCardBounds(componentWidth).toFloat().reduced(18.0f, 0.0f);
        const float gap = 28.0f;
        const float half = (inner.getWidth() - gap) * 0.5f;
        return { inner.getX() + (output ? half + gap : 0.0f), inner.getY(), half, inner.getHeight() };
    }

    juce::Rectangle<int> getSectionBounds(int componentWidth)
    {
        return { kMargin, kSectionTop, componentWidth - 2 * kMargin, kSectionH };
    }

    // ---- text helpers ---------------------------------------------------------
    juce::String glyph(juce::juce_wchar codePoint)
    {
        return juce::String::charToString(codePoint);
    }

    juce::Font sansFont(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(height).withStyle(bold ? "Bold" : "Regular"));
    }

    juce::Font monoFont(float height, bool bold = true, float kerning = 0.0f)
    {
        return juce::Font(juce::FontOptions(height)
                              .withName("Consolas")
                              .withStyle(bold ? "Bold" : "Regular")
                              .withKerningFactor(kerning));
    }

    float textWidth(const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText(font, text, 0.0f, 0.0f);
        return arrangement.getBoundingBox(0, -1, true).getWidth();
    }

    juce::String formatMs(double ms)
    {
        return juce::String(ms, 1) + " ms";
    }

    juce::String formatPercent(double fraction)
    {
        const double percent = fraction * 100.0;
        if (percent < 0.05)
            return "0%";
        if (percent < 1.0)
            return "<1%";
        return juce::String(juce::roundToInt(percent)) + "%";
    }

    // A big number with a tiny caption under it, right-aligned (header stats).
    void drawStat(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& value,
                  const juce::String& caption, juce::Colour colour)
    {
        g.setColour(colour);
        g.setFont(monoFont(24.0f, true));
        g.drawText(value, area.withHeight(30), juce::Justification::centredRight);

        g.setColour(dimText);
        g.setFont(monoFont(10.0f, true, 0.14f));
        g.drawText(caption, area.withTrimmedTop(32).withHeight(14), juce::Justification::centredRight);
    }

    // The on/off switch used for plugin bypass and in the settings window.
    void drawSwitch(juce::Graphics& g, juce::Rectangle<float> bounds, bool on, bool enabled, bool highlighted)
    {
        const float alpha = enabled ? 1.0f : 0.4f;
        auto track = on ? accentColour : switchOffColour;
        if (highlighted)
            track = track.brighter(0.15f);

        g.setColour(track.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);

        const float inset = 3.0f;
        const float diameter = bounds.getHeight() - 2.0f * inset;
        const float x = on ? bounds.getRight() - inset - diameter : bounds.getX() + inset;
        g.setColour((on ? onAccentColour : secondaryText).withMultipliedAlpha(alpha));
        g.fillEllipse(x, bounds.getY() + inset, diameter, diameter);
    }

    // ---- button styles ------------------------------------------------------------
    // ToneLookAndFeel draws buttons from one property:
    //   primary = filled accent, ghost = outlined (default), pill = on/off chip,
    //   switch  = toggle drawn as a sliding switch.
    const juce::Identifier kindProperty { "tcKind" };

    void setupButton(juce::TextButton& button, const juce::String& kind)
    {
        button.getProperties().set(kindProperty, kind);
        button.setClickingTogglesState(false);
        button.setWantsKeyboardFocus(false);
    }

    juce::String kindOf(const juce::Component& component)
    {
        return component.getProperties().getWithDefault(kindProperty, "ghost").toString();
    }

    // ---- plugin card geometry (shared by painting and hit testing) ------------------
    struct RowGeometry
    {
        juce::Rectangle<int> toggle, toggleHit, text, delay, cpu, menu;
    };

    RowGeometry computeRowGeometry(int width, int height)
    {
        RowGeometry geo;
        geo.toggle    = { 18, (height - kSwitchH) / 2, kSwitchW, kSwitchH };
        geo.toggleHit = { 0, 0, geo.toggle.getRight() + 12, height };
        geo.menu      = { width - kColMenu, 0, kColMenu, height };
        geo.cpu       = { geo.menu.getX() - kColCpu, 0, kColCpu, height };
        geo.delay     = { geo.cpu.getX() - kColDelay, 0, kColDelay, height };

        const int textLeft = geo.toggleHit.getRight() + 4;
        geo.text = { textLeft, 0, juce::jmax(40, geo.delay.getX() - 16 - textLeft), height };
        return geo;
    }
}

class MainComponent::ToneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ToneLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, backgroundColour);
        setColour(juce::Label::textColourId, primaryText);

        setColour(juce::PopupMenu::backgroundColourId, surfaceColour);
        setColour(juce::PopupMenu::textColourId, primaryText);
        setColour(juce::PopupMenu::headerTextColourId, secondaryText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentColour);
        setColour(juce::PopupMenu::highlightedTextColourId, onAccentColour);

        // Used by the audio device selector in the Audio window.
        setColour(juce::ComboBox::backgroundColourId, surfaceColour);
        setColour(juce::ComboBox::outlineColourId, borderColour);
        setColour(juce::ComboBox::textColourId, primaryText);
        setColour(juce::ComboBox::arrowColourId, secondaryText);
        setColour(juce::ToggleButton::textColourId, primaryText);
        setColour(juce::ToggleButton::tickColourId, accentColour);
        setColour(juce::ToggleButton::tickDisabledColourId, dimText);

        setColour(juce::ScrollBar::thumbColourId, borderColour.brighter(0.3f));
        setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool isMouseOver, bool isButtonDown) override
    {
        const auto kind = kindOf(button);
        const auto area = button.getLocalBounds().toFloat().reduced(0.5f);
        const float alpha = button.isEnabled() ? 1.0f : 0.4f;

        if (kind == "primary")
        {
            auto fill = accentColour;
            if (isButtonDown)      fill = fill.darker(0.25f);
            else if (isMouseOver)  fill = fill.brighter(0.12f);

            g.setColour(fill.withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(area, 10.0f);
        }
        else if (kind == "pill")
        {
            const bool on = button.getToggleState();
            const float radius = area.getHeight() * 0.5f;

            g.setColour(on ? accentColour.withAlpha(isMouseOver ? 0.22f : 0.14f)
                           : (isMouseOver ? surfaceHover : surfaceColour));
            g.fillRoundedRectangle(area, radius);

            g.setColour(on ? accentColour.withAlpha(0.55f) : borderColour);
            g.drawRoundedRectangle(area, radius, 1.0f);

            g.setColour(on ? accentColour : dimText);
            g.fillEllipse(area.getX() + 12.0f, area.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        else
        {
            g.setColour((isButtonDown ? surfaceActive : isMouseOver ? surfaceHover : surfaceColour)
                            .withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(area, 10.0f);

            g.setColour(borderColour.withMultipliedAlpha(alpha));
            g.drawRoundedRectangle(area, 10.0f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        const auto kind = kindOf(button);
        const float alpha = button.isEnabled() ? 1.0f : 0.4f;
        auto area = button.getLocalBounds();

        if (kind == "primary")
        {
            g.setColour(onAccentColour.withMultipliedAlpha(alpha));
            g.setFont(sansFont(14.5f, true));
        }
        else if (kind == "pill")
        {
            g.setColour((button.getToggleState() ? accentColour : secondaryText).withMultipliedAlpha(alpha));
            g.setFont(sansFont(12.5f, true));
            area = area.withTrimmedLeft(14);   // room for the status dot
        }
        else
        {
            g.setColour(primaryText.withMultipliedAlpha(alpha));
            g.setFont(sansFont(juce::jmin(14.5f, static_cast<float>(button.getHeight()) * 0.55f)));
        }

        g.drawText(button.getButtonText(), area, juce::Justification::centred, false);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (kindOf(button) != "switch")
        {
            juce::LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted,
                                                   shouldDrawButtonAsDown);
            return;
        }

        const auto bounds = button.getLocalBounds();
        const auto switchArea = juce::Rectangle<int>(0, (bounds.getHeight() - kSwitchH) / 2, kSwitchW, kSwitchH);
        drawSwitch(g, switchArea.toFloat(), button.getToggleState(), button.isEnabled(),
                   shouldDrawButtonAsHighlighted);

        g.setColour(button.findColour(juce::ToggleButton::textColourId)
                          .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));
        g.setFont(sansFont(15.0f, true));
        g.drawText(button.getButtonText(), bounds.withTrimmedLeft(kSwitchW + 16),
                   juce::Justification::centredLeft, true);
    }
};

// One plugin in the chain, drawn as a card: bypass switch, name and maker,
// delay and CPU, and a menu button. Click the switch to bypass, click the
// card to open the editor, click the dots (or right-click) for move / remove.
class MainComponent::PluginRow final : public juce::Component
{
public:
    PluginRow(const HostEngine::PluginInfo& info, int pluginIndex,
              std::function<void(int)> editCallback,
              std::function<void(int)> bypassCallback,
              std::function<void(int)> menuCallback)
        : pluginName(info.name), nodeId(info.nodeId), index(pluginIndex), bypassed(info.bypassed),
          edit(std::move(editCallback)), toggleBypass(std::move(bypassCallback)),
          menu(std::move(menuCallback))
    {
        subtitle = info.manufacturer;
        if (info.format.isNotEmpty())
        {
            if (subtitle.isNotEmpty())
                subtitle += " " + glyph(0x00B7) + " ";
            subtitle += info.format;
        }

        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTitle(pluginName);
    }

    juce::uint32 getNodeId() const noexcept { return nodeId; }

    // Set by ChainView. onDragMove returns false when dragging is not possible
    // (e.g. only one plugin), in which case the card simply stays put.
    std::function<bool(PluginRow&, const juce::MouseEvent&)> onDragMove;
    std::function<void(PluginRow&)> onDragEnd;

    void setLive(bool isBypassed, const juce::String& delay, const juce::String& cpu)
    {
        if (bypassed == isBypassed && delayText == delay && cpuText == cpu)
            return;

        bypassed = isBypassed;
        delayText = delay;
        cpuText = cpu;
        repaint();
    }

    void setEditing(bool isEditing)
    {
        if (editing == isEditing)
            return;

        editing = isEditing;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const int w = getWidth();
        const int h = getHeight();
        const auto geo = computeRowGeometry(w, h);
        const auto card = getLocalBounds().toFloat().reduced(0.5f);
        const float radius = static_cast<float>(kCardRadius);

        g.setColour(dragging ? surfaceActive : hovered ? surfaceHover : surfaceColour);
        g.fillRoundedRectangle(card, radius);

        g.setColour(dragging ? accentColour : editing ? accentColour.withAlpha(0.65f) : borderColour);
        g.drawRoundedRectangle(card, radius, 1.0f);

        // Bypass switch: on = the plugin is processing audio.
        drawSwitch(g, geo.toggle.toFloat(), !bypassed, true, false);

        // Name and maker
        g.setColour(bypassed ? dimText : primaryText);
        g.setFont(sansFont(15.0f, true));
        g.drawText(pluginName, geo.text.withHeight(20).withY(h / 2 - 21),
                   juce::Justification::centredLeft, true);

        g.setColour(dimText);
        g.setFont(sansFont(12.5f));
        g.drawText(subtitle, geo.text.withHeight(16).withY(h / 2 + 1),
                   juce::Justification::centredLeft, true);

        // Delay and CPU, or a single "bypassed" note.
        if (bypassed)
        {
            g.setColour(dimText);
            g.setFont(monoFont(11.0f, true, 0.14f));
            g.drawText("BYPASSED", geo.delay.getUnion(geo.cpu).withTrimmedRight(12),
                       juce::Justification::centredRight);
        }
        else
        {
            drawValue(g, geo.delay.withTrimmedRight(12), delayText, "DELAY", h);
            drawValue(g, geo.cpu.withTrimmedRight(12), cpuText, "CPU", h);
        }

        // Menu dots
        g.setColour(hovered ? primaryText : dimText);
        const float cx = static_cast<float>(geo.menu.getCentreX());
        const float cy = static_cast<float>(h) * 0.5f;
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse(cx + static_cast<float>(i) * 6.0f - 1.75f, cy - 1.75f, 3.5f, 3.5f);
    }

    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovered = false; repaint(); }

    // Pressing on the switch or the menu dots never starts a drag; anywhere else on
    // the card does (after the mouse has moved a few pixels).
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto geo = computeRowGeometry(getWidth(), getHeight());
        canDrag = !e.mods.isPopupMenu()
               && !geo.toggleHit.contains(e.getPosition())
               && !geo.menu.contains(e.getPosition());
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!canDrag || !onDragMove)
            return;

        if (!dragging && e.getDistanceFromDragStart() < 6)
            return;

        if (!onDragMove(*this, e))
            return;

        if (!dragging)
        {
            dragging = true;
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (dragging)
        {
            dragging = false;
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            repaint();
            if (onDragEnd)
                onDragEnd(*this);
            return;
        }

        if (!contains(e.getPosition()) || e.mouseWasDraggedSinceMouseDown())
            return;

        if (e.mods.isPopupMenu())
        {
            if (menu) menu(index);
            return;
        }

        const auto geo = computeRowGeometry(getWidth(), getHeight());
        if (geo.toggleHit.contains(e.getPosition()))
        {
            if (toggleBypass) toggleBypass(index);
        }
        else if (geo.menu.contains(e.getPosition()))
        {
            if (menu) menu(index);
        }
        else if (edit)
        {
            edit(index);
        }
    }

private:
    // Value on top, small caption below.
    void drawValue(juce::Graphics& g, juce::Rectangle<int> column, const juce::String& value,
                   const juce::String& caption, int height) const
    {
        g.setColour(secondaryText);
        g.setFont(monoFont(13.0f, false));
        g.drawText(value, column.withHeight(18).withY(height / 2 - 17), juce::Justification::centredRight);

        g.setColour(dimText);
        g.setFont(monoFont(9.5f, true, 0.14f));
        g.drawText(caption, column.withHeight(14).withY(height / 2 + 2), juce::Justification::centredRight);
    }

    juce::String pluginName;
    juce::String subtitle;
    juce::uint32 nodeId = 0;
    int index = 0;
    bool bypassed = false;
    bool editing = false;
    bool hovered = false;
    bool canDrag = false;
    bool dragging = false;
    juce::String delayText { "0.0 ms" };
    juce::String cpuText { "0%" };

    std::function<void(int)> edit;
    std::function<void(int)> toggleBypass;
    std::function<void(int)> menu;
};

class MainComponent::ChainView final : public juce::Component
{
public:
    // Called after a card was dropped on a new position (from and to are chain indexes).
    std::function<void(int from, int to)> onMove;

    void rebuild(const HostEngine& host,
                 std::function<void(int)> editCallback,
                 std::function<void(int)> bypassCallback,
                 std::function<void(int)> menuCallback)
    {
        animator.cancelAllAnimations(false);
        dragRow = nullptr;
        layoutLocked = false;
        rows.clear();

        const int count = host.getNumPlugins();
        for (int i = 0; i < count; ++i)
        {
            auto row = std::make_unique<PluginRow>(host.getPluginInfo(i), i,
                                                   editCallback, bypassCallback, menuCallback);
            row->onDragMove = [this](PluginRow& r, const juce::MouseEvent& e) { return dragMoved(r, e); };
            row->onDragEnd  = [this](PluginRow& r) { dragEnded(r); };
            addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }

        if (count > 0)
            updateLive(host);

        resized();
        repaint();
    }

    // Pulls the current bypass state, delay and CPU of every row from the engine.
    void updateLive(const HostEngine& host)
    {
        const bool chainOn = host.isChainEnabled();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const int index = static_cast<int>(i);
            const bool bypassed = host.getPluginInfo(index).bypassed;

            if (bypassed)
                rows[i]->setLive(true, "bypass", "0%");
            else if (!chainOn)
                rows[i]->setLive(false, "--", "--");
            else
                rows[i]->setLive(false, formatMs(host.getPluginLatencyMs(index)),
                                 formatPercent(host.getPluginLoad(index)));
        }
    }

    void resized() override
    {
        // While a card is being dragged (or settling into its new place) the
        // positions are driven by the drag, not by the plain list layout.
        if (!layoutLocked)
            for (size_t i = 0; i < rows.size(); ++i)
                rows[i]->setBounds(kMargin, static_cast<int>(i) * kCardPitch, getRowWidth(), kCardH);

        const int contentHeight = rows.empty() ? kEmptyH
                                               : static_cast<int>(rows.size()) * kCardPitch + 4;
        setSize(getWidth(), contentHeight);
    }

    // Empty state: a dashed outline with a hint.
    void paint(juce::Graphics& g) override
    {
        if (!rows.empty())
            return;

        const auto area = juce::Rectangle<int>(kMargin, 0, getWidth() - 2 * kMargin, kEmptyH)
                              .toFloat().reduced(0.5f);

        juce::Path outline;
        outline.addRoundedRectangle(area, static_cast<float>(kCardRadius));

        juce::Path dashed;
        const float dashes[] = { 5.0f, 5.0f };
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
        g.setColour(borderColour.brighter(0.1f));
        g.fillPath(dashed);

        // A little "+" in a circle.
        const auto icon = juce::Rectangle<float>(0.0f, 0.0f, 36.0f, 36.0f)
                              .withCentre({ area.getCentreX(), area.getY() + 40.0f });
        g.setColour(dimText);
        g.drawEllipse(icon, 1.5f);
        g.drawLine(icon.getCentreX() - 6.0f, icon.getCentreY(), icon.getCentreX() + 6.0f, icon.getCentreY(), 1.5f);
        g.drawLine(icon.getCentreX(), icon.getCentreY() - 6.0f, icon.getCentreX(), icon.getCentreY() + 6.0f, 1.5f);

        const auto box = area.toNearestInt();
        g.setColour(primaryText);
        g.setFont(sansFont(17.0f, true));
        g.drawText("No plugins yet", box.withTrimmedTop(76).withHeight(24), juce::Justification::centred);

        g.setColour(secondaryText);
        g.setFont(sansFont(14.0f));
        g.drawText("Press \"Add plugin\" to build your signal chain.",
                   box.withTrimmedTop(102).withHeight(20), juce::Justification::centred);
    }

    PluginRow* getRow(int index) const
    {
        return juce::isPositiveAndBelow(index, static_cast<int>(rows.size()))
                   ? rows[static_cast<size_t>(index)].get() : nullptr;
    }

private:
    int getRowWidth() const { return juce::jmax(100, getWidth() - 2 * kMargin); }

    int indexOf(const PluginRow& row) const
    {
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].get() == &row)
                return static_cast<int>(i);

        return -1;
    }

    // The dragged card follows the mouse; the other cards slide out of the way
    // so there is always a gap where it would land.
    bool dragMoved(PluginRow& row, const juce::MouseEvent& e)
    {
        const int count = static_cast<int>(rows.size());
        if (count < 2)
            return false;

        if (dragRow == nullptr)
        {
            const int from = indexOf(row);
            if (from < 0)
                return false;

            dragRow = &row;
            dragFrom = from;
            dragTarget = from;
            grabOffset = e.getMouseDownPosition().y;   // where inside the card it was grabbed
            layoutLocked = true;
            animator.cancelAllAnimations(false);
            row.toFront(false);
        }

        if (dragRow != &row)
            return false;

        const int y = juce::jlimit(0, (count - 1) * kCardPitch,
                                   e.getEventRelativeTo(this).getPosition().y - grabOffset);
        row.setTopLeftPosition(kMargin, y);

        const int target = juce::jlimit(0, count - 1, (y + kCardPitch / 2) / kCardPitch);
        if (target != dragTarget)
        {
            dragTarget = target;
            slideOthers();
        }

        // Scroll the list when the card is held near its top or bottom edge.
        if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        {
            const auto p = e.getEventRelativeTo(viewport).getPosition();
            viewport->autoScroll(p.x, p.y, 30, 10);
        }

        return true;
    }

    void slideOthers()
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].get() == dragRow)
                continue;

            const int index = static_cast<int>(i);
            int slot = index;
            if (dragFrom < dragTarget && index > dragFrom && index <= dragTarget)
                slot = index - 1;
            else if (dragFrom > dragTarget && index >= dragTarget && index < dragFrom)
                slot = index + 1;

            animator.animateComponent(rows[i].get(),
                                      { kMargin, slot * kCardPitch, getRowWidth(), kCardH },
                                      1.0f, 140, false, 1.0, 1.0);
        }
    }

    void dragEnded(PluginRow& row)
    {
        if (dragRow != &row)
            return;

        dragRow = nullptr;
        const int from = dragFrom;
        const int to = dragTarget;

        // Settle the card into its slot.
        animator.animateComponent(&row, { kMargin, to * kCardPitch, getRowWidth(), kCardH },
                                  1.0f, 100, false, 1.0, 1.0);

        if (from == to)
        {
            layoutLocked = false;
            return;
        }

        // Rebuilding the list destroys this card, so wait until the mouse handler
        // has returned (and the settle animation has played) before reordering.
        juce::Timer::callAfterDelay(120, [safeThis = juce::Component::SafePointer<ChainView>(this), from, to]
        {
            if (safeThis != nullptr && safeThis->onMove)
                safeThis->onMove(from, to);
        });
    }

    std::vector<std::unique_ptr<PluginRow>> rows;
    juce::ComponentAnimator animator;
    PluginRow* dragRow = nullptr;
    int dragFrom = 0;
    int dragTarget = 0;
    int grabOffset = 0;
    bool layoutLocked = false;
};

class MainComponent::PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& title, juce::AudioProcessorEditor* editor,
                       std::function<void()> onCloseCallback)
        : juce::DocumentWindow(title, surfaceColour, juce::DocumentWindow::closeButton),
          onClose(std::move(onCloseCallback))
    {
        setUsingNativeTitleBar(true);
        if (editor != nullptr)
        {
            // The window owns the editor, so the editor is always deleted
            // before the processor it belongs to (see MainComponent::removePlugin
            // and the destructor, which destroy windows before the engine).
            // resizeToFit sizes the window to the editor, including the title
            // bar, so we just re-centre at whatever size the window ended up.
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);
            centreWithSize(getWidth(), getHeight());
        }
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onClose)
            onClose();
    }

private:
    std::function<void()> onClose;
};

// The sound played by the "Test" button in the Audio window. It runs as a second
// audio callback next to the plugin chain, so it comes out of whatever output
// device is selected. Change the notes, the gain or the decay below to taste.
class TestChime final : public juce::AudioIODeviceCallback
{
public:
    void trigger() noexcept { restartRequested.store(true); }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
        position = -1;
    }

    void audioDeviceStopped() override { position = -1; }

    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
                                          int numOutputChannels, int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        if (restartRequested.exchange(false))
            position = 0;

        const juce::int64 length = static_cast<juce::int64>(sampleRate * 1.4);

        for (int i = 0; i < numSamples; ++i)
        {
            float value = 0.0f;

            if (position >= 0)
            {
                value = chime(static_cast<double>(position) / sampleRate);
                if (++position >= length)
                    position = -1;
            }

            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (output[channel] != nullptr)
                    output[channel][i] = value;
        }
    }

private:
    // One soft bell note: quick attack, exponential decay, a faint overtone.
    static double note(double t, double frequency)
    {
        if (t < 0.0)
            return 0.0;

        const double w = juce::MathConstants<double>::twoPi * frequency;
        const double envelope = (1.0 - std::exp(-t * 400.0)) * std::exp(-t * 6.0);
        const double overtone = 0.2 * std::sin(w * 2.76 * t) * std::exp(-t * 12.0);
        return envelope * (0.8 * std::sin(w * t) + overtone);
    }

    static float chime(double t)
    {
        const double gain = 0.22;                              // overall volume
        return static_cast<float>(gain * (note(t, 659.25)            // E5
                                          + 0.8 * note(t - 0.16, 987.77)));   // B5, a little later
    }

    std::atomic<bool> restartRequested { false };
    double sampleRate = 44100.0;
    juce::int64 position = -1;   // samples played so far, or -1 while silent
};

class MainComponent::PreferencesWindow final : public juce::DocumentWindow,
                                               private juce::Timer
{
public:
    PreferencesWindow(juce::AudioDeviceManager& manager, juce::LookAndFeel& style)
        : juce::DocumentWindow("Audio", backgroundColour, juce::DocumentWindow::closeButton),
          deviceManager(manager)
    {
        setLookAndFeel(&style);
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        auto* selector = new juce::AudioDeviceSelectorComponent(
            manager, 0, 2, 0, 2, false, false, true, false);
        setContentOwned(selector, true);
        centreWithSize(540, 620);

        deviceManager.addAudioCallback(&testChime);

        // JUCE's selector always plays its own beep from the Test button, so the
        // button's click is swapped for ours. The selector rebuilds that button
        // when the device changes, hence the regular re-check.
        useOwnTestSound(*this);
        startTimer(250);
    }

    ~PreferencesWindow() override
    {
        stopTimer();
        deviceManager.removeAudioCallback(&testChime);
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    void timerCallback() override { useOwnTestSound(*this); }

    void useOwnTestSound(juce::Component& parent)
    {
        static const juce::Identifier patched { "tcOwnTestSound" };

        for (auto* child : parent.getChildren())
        {
            if (auto* button = dynamic_cast<juce::TextButton*>(child))
            {
                if (!button->getProperties().contains(patched) && button->getButtonText() == TRANS("Test"))
                {
                    button->getProperties().set(patched, true);
                    button->setTooltip("Plays a short chime");
                    button->onClick = [this] { testChime.trigger(); };
                }
            }

            useOwnTestSound(*child);
        }
    }

    juce::AudioDeviceManager& deviceManager;
    TestChime testChime;
};

class MainComponent::SettingsWindow final : public juce::DocumentWindow
{
public:
    SettingsWindow(AppSettings& settings, juce::LookAndFeel& style)
        : juce::DocumentWindow("Settings", backgroundColour, juce::DocumentWindow::closeButton)
    {
        setLookAndFeel(&style);
        setUsingNativeTitleBar(true);
        setContentOwned(new Content(settings), true);
        centreWithSize(getWidth(), getHeight());
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    class Content final : public juce::Component
    {
    public:
        explicit Content(AppSettings& settingsIn) : settings(settingsIn)
        {
            configure(trayToggle, trayNote, "Keep running in the system tray",
                      "Closing or minimising the window hides it to the tray so audio keeps "
                      "running. Click the tray icon to bring it back.");

            configure(startupToggle, startupNote, "Open with Windows",
                      AppSettings::isLaunchAtStartupSupported()
                          ? "Start LightRebirth when you sign in. If the tray option is on, "
                            "it starts hidden in the tray."
                          : "Only available on Windows.");

            trayToggle.setToggleState(settings.getKeepInTray(), juce::dontSendNotification);
            trayToggle.onClick = [this] { settings.setKeepInTray(trayToggle.getToggleState()); };

            startupToggle.setEnabled(AppSettings::isLaunchAtStartupSupported());
            startupToggle.setToggleState(settings.getLaunchAtStartup(), juce::dontSendNotification);
            startupToggle.onClick = [this]
            {
                settings.setLaunchAtStartup(startupToggle.getToggleState());

                // Show what Windows actually ended up with, in case it refused.
                startupToggle.setToggleState(settings.getLaunchAtStartup(), juce::dontSendNotification);
            };

            setSize(480, 250);
        }

        void paint(juce::Graphics& g) override { g.fillAll(backgroundColour); }

        void resized() override
        {
            auto area = getLocalBounds().reduced(28, 24);

            const auto place = [&area](juce::ToggleButton& toggle, juce::Label& note)
            {
                toggle.setBounds(area.removeFromTop(28));
                note.setBounds(area.removeFromTop(52).withTrimmedLeft(kSwitchW + 16));   // lines up with the toggle's text
                area.removeFromTop(18);
            };

            place(trayToggle, trayNote);
            place(startupToggle, startupNote);
        }

    private:
        void configure(juce::ToggleButton& toggle, juce::Label& note,
                       const juce::String& title, const juce::String& description)
        {
            toggle.setButtonText(title);
            toggle.getProperties().set(kindProperty, "switch");
            toggle.setWantsKeyboardFocus(false);
            addAndMakeVisible(toggle);

            note.setText(description, juce::dontSendNotification);
            note.setFont(sansFont(13.5f));
            note.setJustificationType(juce::Justification::topLeft);
            note.setBorderSize(juce::BorderSize<int>(0));
            note.setColour(juce::Label::textColourId, secondaryText);
            addAndMakeVisible(note);
        }

        AppSettings& settings;
        juce::ToggleButton trayToggle, startupToggle;
        juce::Label trayNote, startupNote;
    };
};

MainComponent::MainComponent()
{
    lookAndFeel = std::make_unique<ToneLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setSize(640, 560);

    titleLabel.setText("LightRebirth", juce::dontSendNotification);
    titleLabel.setFont(sansFont(22.0f, true));
    titleLabel.setBorderSize(juce::BorderSize<int>(0));
    titleLabel.setColour(juce::Label::textColourId, primaryText);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(monoFont(12.0f, false));
    subtitleLabel.setBorderSize(juce::BorderSize<int>(0));
    subtitleLabel.setColour(juce::Label::textColourId, secondaryText);
    addAndMakeVisible(subtitleLabel);

    chainView = new ChainView();
    chainView->onMove = [this](int from, int to) { movePlugin(from, to); };
    chainViewport.setViewedComponent(chainView, true);
    chainViewport.setScrollBarsShown(true, false);
    chainViewport.setScrollBarThickness(8);
    addAndMakeVisible(chainViewport);

    addAndMakeVisible(addButton);
    setupButton(addButton, "primary");
    addButton.onClick = [this] { showPluginMenu(); };

    addAndMakeVisible(preferencesButton);
    setupButton(preferencesButton, "ghost");
    preferencesButton.onClick = [this] { showPreferences(); };

    addAndMakeVisible(settingsButton);
    setupButton(settingsButton, "ghost");
    settingsButton.onClick = [this] { showSettings(); };

    addAndMakeVisible(chainButton);
    setupButton(chainButton, "pill");
    chainButton.setClickingTogglesState(true);
    chainButton.setToggleState(true, juce::dontSendNotification);
    chainButton.onClick = [this]
    {
        engine.setChainEnabled(chainButton.getToggleState());
        chainButton.setButtonText(chainButton.getToggleState() ? "Chain on" : "Chain off");
        chainView->updateLive(engine);
        repaint(getLatencyBounds(getWidth()));
    };

    footerLabel.setFont(sansFont(13.0f));
    footerLabel.setBorderSize(juce::BorderSize<int>(0));
    footerLabel.setJustificationType(juce::Justification::centredRight);
    footerLabel.setColour(juce::Label::textColourId, secondaryText);
    addAndMakeVisible(footerLabel);

    engine.onDeviceChanged = [safeThis = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safeThis == nullptr)
            return;

        safeThis->updateDeviceLine();
        if (!safeThis->scanInProgress)
            safeThis->setStatus("Audio device changed");
    };

    const auto error = engine.initialise();
    if (error.isNotEmpty())
        setStatus("Audio device: " + error);

    refreshPluginRows();
    updateDeviceLine();
    updateHeader();
    startPluginScan();
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    setLookAndFeel(nullptr);
    preferencesWindow.reset();
    settingsWindow.reset();
    editorWindows.clear();
    engine.shutdown();
}

void MainComponent::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    g.fillAll(backgroundColour);

    // ---- header: logo tile (same look as the tray icon), then the live numbers ----
    const auto logo = getLogoBounds().toFloat();
    g.setColour(surfaceColour);
    g.fillRoundedRectangle(logo, 11.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(logo.reduced(0.5f), 11.0f, 1.0f);
    g.setColour(accentColour);
    g.fillEllipse(logo.withSizeKeepingCentre(14.0f, 14.0f));

    const auto cpuColour = cpuLoad > 0.85f ? dangerColour : cpuLoad > 0.6f ? warnColour : primaryText;
    drawStat(g, getLatencyBounds(w), latencyText, "ROUND TRIP", accentColour);
    drawStat(g, getCpuBounds(w), cpuText, "CPU", cpuColour);

    // ---- level meters ----
    const auto card = getMeterCardBounds(w).toFloat().reduced(0.5f);
    g.setColour(surfaceColour);
    g.fillRoundedRectangle(card, static_cast<float>(kCardRadius));
    g.setColour(borderColour);
    g.drawRoundedRectangle(card, static_cast<float>(kCardRadius), 1.0f);

    drawMeter(g, getMeterBounds(w, false), "IN", inputPeak);
    drawMeter(g, getMeterBounds(w, true), "OUT", outputPeak);

    // ---- section title ----
    const auto section = getSectionBounds(w);
    const auto sectionFont = monoFont(11.0f, true, 0.14f);
    const juce::String sectionTitle = "SIGNAL CHAIN";
    g.setFont(sectionFont);
    g.setColour(secondaryText);
    g.drawText(sectionTitle, section, juce::Justification::centredLeft);

    const int count = engine.getNumPlugins();
    g.setColour(dimText);
    g.drawText((juce::String(count) + (count == 1 ? " PLUGIN" : " PLUGINS")),
               section.withTrimmedLeft(juce::roundToInt(textWidth(sectionFont, sectionTitle)) + 16),
               juce::Justification::centredLeft);

    // ---- bottom toolbar ----
    const int barTop = h - kBarHeight;
    g.setColour(toolbarColour);
    g.fillRect(0, barTop, w, kBarHeight);
    g.setColour(borderColour);
    g.fillRect(0, barTop, w, 1);
}

void MainComponent::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    const int titleLeft = kMargin + kLogoSize + kLogoGap;
    const int statsWidth = kLatencyW + kStatGap + kCpuW + kStatGap;
    const int headerWidth = juce::jmax(60, w - kMargin - titleLeft - statsWidth);
    titleLabel.setBounds(titleLeft, kHeaderTop, headerWidth, 26);
    subtitleLabel.setBounds(titleLeft, kHeaderTop + 28, headerWidth, 18);

    chainButton.setBounds(getSectionBounds(w).removeFromRight(104));

    const int barTop = h - kBarHeight;
    const int buttonTop = barTop + (kBarHeight - kButtonH) / 2;
    addButton.setBounds(kMargin, buttonTop, 144, kButtonH);
    preferencesButton.setBounds(addButton.getRight() + 10, buttonTop, 84, kButtonH);
    settingsButton.setBounds(preferencesButton.getRight() + 8, buttonTop, 96, kButtonH);

    const int footerLabelLeft = settingsButton.getRight() + 16;
    footerLabel.setBounds(footerLabelLeft, buttonTop, juce::jmax(60, w - kMargin - footerLabelLeft), kButtonH);

    chainViewport.setBounds(0, kListTop, w, juce::jmax(80, barTop - kListTop));
    if (chainView != nullptr)
    {
        chainView->setSize(chainViewport.getWidth(), chainView->getHeight());
        chainView->resized();
    }
}

void MainComponent::timerCallback()
{
    // Snap to exactly zero once the decay is far below anything the meters can
    // display (the readout floors at -60 dB), so an idle app stops repainting.
    const auto decay = [](float current, float incoming)
    {
        const float value = juce::jmax(incoming, current * 0.88f);
        return value < 1.0e-4f ? 0.0f : value;
    };

    const float previousInput = inputPeak;
    const float previousOutput = outputPeak;
    inputPeak = decay(inputPeak, engine.takeInputPeak());
    outputPeak = decay(outputPeak, engine.takeOutputPeak());

    if (inputPeak != previousInput || outputPeak != previousOutput)
        repaint(getMeterCardBounds(getWidth()));

    const double latency = engine.getRoundTripLatencyMs();
    const auto newLatencyText = latency < 0.0 ? juce::String("-- ms") : formatMs(latency);
    if (newLatencyText != latencyText)
    {
        latencyText = newLatencyText;
        repaint(getLatencyBounds(getWidth()));
    }

    const double cpu = engine.getCpuUsage();
    const auto newCpuText = formatPercent(cpu);
    cpuLoad = static_cast<float>(cpu);
    if (newCpuText != cpuText)
    {
        cpuText = newCpuText;
        repaint(getCpuBounds(getWidth()));
    }

    if (chainView != nullptr)
        chainView->updateLive(engine);

    if (statusTicks > 0)
        --statusTicks;
    updateFooterText();
}

void MainComponent::refreshPluginRows()
{
    if (chainView == nullptr)
        return;

    chainView->rebuild(
        engine,
        [this](int index) { openPluginEditor(index); },
        [this](int index) { toggleBypass(index); },
        [this](int index) { showRowMenu(index); });

    chainView->setSize(chainViewport.getWidth(), chainView->getHeight());
    chainView->resized();

    // Rows are recreated on every refresh, so restore the "editing" tag for any
    // plugin whose editor window is currently showing.
    for (int i = 0; i < engine.getNumPlugins(); ++i)
    {
        const auto window = editorWindows.find(engine.getPluginInfo(i).nodeId);
        if (window != editorWindows.end() && window->second->isVisible())
            if (auto* row = chainView->getRow(i))
                row->setEditing(true);
    }

    updateHeader();
}

void MainComponent::showPluginMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(lookAndFeel.get());
    menu.addItem(1, scanInProgress ? "Scanning VST3 plugins..." : "Rescan VST3 folders", !scanInProgress);
    menu.addSeparator();

    const auto available = engine.getAvailablePlugins();
    for (int i = 0; i < available.size(); ++i)
    {
        const auto& plugin = available.getReference(i);
        auto label = plugin.name;
        if (plugin.manufacturerName.isNotEmpty())
            label += "  " + glyph(0x2014) + "  " + plugin.manufacturerName;
        menu.addItem(i + 100, label);
    }

    if (available.isEmpty())
        menu.addItem(2, "No VST3 plugins found yet", false);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton),
        [safeThis = juce::Component::SafePointer<MainComponent>(this), available](int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            if (result == 1)
            {
                safeThis->startPluginScan();
                return;
            }

            const int pluginIndex = result - 100;
            if (juce::isPositiveAndBelow(pluginIndex, available.size()))
                safeThis->addPlugin(available.getReference(pluginIndex));
        });
}

void MainComponent::showRowMenu(int index)
{
    const auto info = engine.getPluginInfo(index);
    if (info.nodeId == 0)
        return;

    const bool hasEditorOpen = editorWindows.count(info.nodeId) != 0;
    const int last = engine.getNumPlugins() - 1;

    juce::PopupMenu menu;
    menu.setLookAndFeel(lookAndFeel.get());
    menu.addItem(1, hasEditorOpen ? "Show editor" : "Open editor");
    menu.addItem(2, info.bypassed ? "Enable" : "Bypass");
    menu.addSeparator();
    menu.addItem(3, "Move up", index > 0);
    menu.addItem(4, "Move down", index < last);
    menu.addSeparator();
    menu.addItem(5, "Remove");

    // The chain can change while the menu is open, so look the plugin up again by node id.
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [safeThis = juce::Component::SafePointer<MainComponent>(this), nodeId = info.nodeId](int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            const int current = safeThis->indexOfNode(nodeId);
            if (current < 0)
                return;

            switch (result)
            {
                case 1: safeThis->openPluginEditor(current); break;
                case 2: safeThis->toggleBypass(current); break;
                case 3: safeThis->movePlugin(current, current - 1); break;
                case 4: safeThis->movePlugin(current, current + 1); break;
                case 5: safeThis->removePlugin(current); break;
                default: break;
            }
        });
}

int MainComponent::indexOfNode(juce::uint32 nodeId) const
{
    for (int i = 0; i < engine.getNumPlugins(); ++i)
        if (engine.getPluginInfo(i).nodeId == nodeId)
            return i;

    return -1;
}

void MainComponent::startPluginScan()
{
    if (scanInProgress)
        return;

    scanInProgress = true;
    updateFooterText();

    engine.scanForPluginsAsync([safeThis = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safeThis == nullptr)
            return;

        safeThis->scanInProgress = false;
        const int count = safeThis->engine.getAvailablePlugins().size();
        safeThis->setStatus("Found " + juce::String(count) + " VST3 plugin(s)");
    });
}

void MainComponent::addPlugin(const juce::PluginDescription& description)
{
    setStatus("Loading " + description.name + "...");

    engine.addPlugin(description, [safeThis = juce::Component::SafePointer<MainComponent>(this),
                                   pluginName = description.name](const juce::String& error)
    {
        if (safeThis == nullptr)
            return;

        if (error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                    "Plugin could not be loaded", error);
            safeThis->setStatus("Could not load " + pluginName);
        }
        else
        {
            safeThis->setStatus("Loaded " + pluginName);
            safeThis->refreshPluginRows();
        }
    });
}

void MainComponent::openPluginEditor(int index)
{
    const auto info = engine.getPluginInfo(index);
    if (info.nodeId == 0)
        return;

    const auto existing = editorWindows.find(info.nodeId);
    if (existing != editorWindows.end())
    {
        existing->second->setVisible(true);
        existing->second->toFront(true);
        if (auto* row = chainView->getRow(index))
            row->setEditing(true);
        return;
    }

    auto* processor = engine.getPluginProcessor(index);
    if (processor == nullptr)
        return;

    if (!processor->hasEditor())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                "No plugin editor",
                                                "This plugin does not provide a custom editor.");
        return;
    }

    // Returns nullptr if the plugin already has an active editor.
    auto* editor = processor->createEditorAndMakeActive();
    if (editor == nullptr)
        return;

    const auto nodeId = info.nodeId;
    auto window = std::make_unique<PluginEditorWindow>(
        info.name, editor,
        [safeThis = juce::Component::SafePointer<MainComponent>(this), nodeId]
        {
            if (safeThis == nullptr || safeThis->chainView == nullptr)
                return;

            const int current = safeThis->indexOfNode(nodeId);
            if (current >= 0)
                if (auto* row = safeThis->chainView->getRow(current))
                    row->setEditing(false);

            // Destroy the closed window (and with it the plugin's editor) so the
            // plugin can release its GUI resources. Deferred, because this runs
            // inside the window's own closeButtonPressed().
            juce::MessageManager::callAsync([safeThis, nodeId]
            {
                if (safeThis == nullptr)
                    return;

                const auto it = safeThis->editorWindows.find(nodeId);
                if (it != safeThis->editorWindows.end() && !it->second->isVisible())
                    safeThis->editorWindows.erase(it);
            });
        });
    window->toFront(true);
    if (auto* row = chainView->getRow(index))
        row->setEditing(true);
    editorWindows.emplace(info.nodeId, std::move(window));
}

void MainComponent::toggleBypass(int index)
{
    const auto info = engine.getPluginInfo(index);
    if (info.nodeId == 0)
        return;

    engine.setPluginBypassed(index, !info.bypassed);
    chainView->updateLive(engine);
    updateHeader();
}

void MainComponent::movePlugin(int from, int to)
{
    if (to < 0 || to >= engine.getNumPlugins())
        return;

    engine.movePlugin(from, to);
    refreshPluginRows();
}

void MainComponent::removePlugin(int index)
{
    const auto info = engine.getPluginInfo(index);
    const auto editor = editorWindows.find(info.nodeId);
    if (editor != editorWindows.end())
        editorWindows.erase(editor);

    engine.removePlugin(index);
    refreshPluginRows();
}

void MainComponent::showPreferences()
{
    if (preferencesWindow == nullptr)
        preferencesWindow = std::make_unique<PreferencesWindow>(engine.getDeviceManager(), *lookAndFeel);

    preferencesWindow->setVisible(true);
    preferencesWindow->toFront(true);
}

void MainComponent::showSettings()
{
    // Rebuilt each time it is opened so the toggles always show the current state.
    if (settingsWindow == nullptr || !settingsWindow->isVisible())
        settingsWindow = std::make_unique<SettingsWindow>(appSettings.get(), *lookAndFeel);

    settingsWindow->setVisible(true);
    settingsWindow->toFront(true);
}

void MainComponent::closeAuxiliaryWindows()
{
    preferencesWindow.reset();
    settingsWindow.reset();
    editorWindows.clear();

    // Rows are rebuilt so no plugin is still tagged "EDITING".
    refreshPluginRows();
}

void MainComponent::updateDeviceLine()
{
    juce::String line = "No audio device";
    if (auto* device = engine.getDeviceManager().getCurrentAudioDevice())
    {
        const double rate = device->getCurrentSampleRate();
        const double kHz = rate / 1000.0;
        const bool wholeKHz = std::abs(kHz - std::round(kHz)) < 0.001;
        const auto separator = " " + glyph(0x00B7) + " ";
        line = device->getName() + separator
             + juce::String(kHz, wholeKHz ? 0 : 1) + " kHz" + separator
             + juce::String(device->getCurrentBufferSizeSamples()) + " smp";
    }

    subtitleLabel.setText(line, juce::dontSendNotification);
}

void MainComponent::updateHeader()
{
    const bool on = engine.isChainEnabled();
    chainButton.setButtonText(on ? "Chain on" : "Chain off");
    chainButton.setToggleState(on, juce::dontSendNotification);

    repaint(getLatencyBounds(getWidth()));
    repaint(getSectionBounds(getWidth()));   // plugin count
}

void MainComponent::setStatus(const juce::String& text)
{
    statusText = text;
    statusTicks = 30 * 5;
    updateFooterText();
}

// The toolbar shows what's happening right now (scan, load, errors) for a few
// seconds and is otherwise empty; the live CPU load is in the header.
void MainComponent::updateFooterText()
{
    juce::String text;

    if (scanInProgress)
        text = "Scanning VST3 folders...";
    else if (statusTicks > 0 && statusText.isNotEmpty())
        text = statusText;

    footerLabel.setText(text, juce::dontSendNotification);
}

// One meter: label on the left, level bar in the middle, dB readout on the right.
// The bar runs teal, then amber from -15 dB, then red towards 0 dB.
void MainComponent::drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                              const juce::String& title, float peak) const
{
    const float db = peak > 0.000001f ? juce::Decibels::gainToDecibels(peak) : -60.0f;

    auto area = bounds;
    const auto labelArea = area.removeFromLeft(30.0f);
    const auto readoutArea = area.removeFromRight(58.0f);

    g.setColour(dimText);
    g.setFont(monoFont(11.0f, true, 0.14f));
    g.drawText(title, labelArea, juce::Justification::centredLeft);

    const bool silent = db <= -59.5f;
    g.setColour(db > -1.0f ? dangerColour : secondaryText);
    g.setFont(monoFont(11.5f, false));
    g.drawText(silent ? juce::String("-inf dB") : juce::String(juce::roundToInt(db)) + " dB",
               readoutArea, juce::Justification::centredRight);

    const auto track = area.reduced(8.0f, 0.0f).withSizeKeepingCentre(area.getWidth() - 16.0f, 6.0f);
    g.setColour(trackColour);
    g.fillRoundedRectangle(track, 3.0f);

    const float normalized = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    if (normalized <= 0.0f)
        return;

    juce::ColourGradient gradient(accentColour, track.getX(), 0.0f,
                                  dangerColour, track.getRight(), 0.0f, false);
    gradient.addColour(0.75, accentColour);
    gradient.addColour(0.90, warnColour);

    g.setGradientFill(gradient);
    g.fillRoundedRectangle(track.withWidth(track.getWidth() * normalized), 3.0f);
}