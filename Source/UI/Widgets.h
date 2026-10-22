// Widgets.h — panel controls for Dicey: knobs, faders, round buttons, LEDs,
// the 16-step ring encoder, the options table and the note display.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace dicey::ui
{
// ---------------------------------------------------------------------------
// Palette & type
// ---------------------------------------------------------------------------
namespace colours
{
    inline const juce::Colour panel      { 0xff2a2a2c };
    inline const juce::Colour panelEdge  { 0xff1c1c1e };
    inline const juce::Colour ink        { 0xfff3f0e6 };
    inline const juce::Colour inkDim     { 0xff9a978e };
    inline const juce::Colour accent     { 0xff4cc7b8 }; // Dicey teal
    inline const juce::Colour cream      { 0xfff7f1dc };
    inline const juce::Colour creamShade { 0xffd9d1b4 };
    inline const juce::Colour tan        { 0xffcfc6a0 };
    inline const juce::Colour black      { 0xff111112 };
    inline const juce::Colour slotHi     { 0xff9a9a9a };
    inline const juce::Colour slotLo     { 0xff5c5c5e };
    inline const juce::Colour ledRed     { 0xffff2b3d };
    inline const juce::Colour ledGreen   { 0xff3fe060 };
    inline const juce::Colour ledYellow  { 0xffffd21f };
}

juce::Font panelFont (float height, bool bold = true, bool italic = false);

void drawLabel (juce::Graphics& g, const juce::String& text, float cx, float cy, float height,
                juce::Colour colour = colours::ink, bool bold = true, bool italic = false);

// ---------------------------------------------------------------------------
// Look and feel for knobs & faders
// ---------------------------------------------------------------------------
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider& s) override
    {
        return s.isVertical() ? (int) (faderCapHeight * 0.5f) : LookAndFeel_V4::getSliderThumbRadius (s);
    }

    juce::Font getSliderPopupFont (juce::Slider&) override { return panelFont (15.0f); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }

    static constexpr float faderCapHeight = 44.0f;
};

// Knob with an optional centre detent (for bipolar VARIATION).
class Knob final : public juce::Slider
{
public:
    explicit Knob (bool centreDetent = false);
    double snapValue (double attempted, DragMode mode) override;

private:
    bool detent;
};

// Fader whose cap has an LED window that lights while its note plays.
class Fader final : public juce::Slider
{
public:
    Fader();
    void setLit (bool shouldBeLit);
    bool isLit() const noexcept { return lit; }

private:
    bool lit = false;
};

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
class Led final : public juce::Component
{
public:
    explicit Led (juce::Colour c) : colour (c) { setInterceptsMouseClicks (false, false); }
    void setLevel (float newLevel);
    void setColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    float level = 0.0f;
};

// ---------------------------------------------------------------------------
// Round push button (with optional long-press)
// ---------------------------------------------------------------------------
class RoundButton final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Style { cream, black, tan };

    explicit RoundButton (Style s) : style (s) {}

    std::function<void()> onClick;
    std::function<void()> onLongPress; // set to enable long-press (~0.6 s)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    Style style;
    bool  pressed = false;
    bool  longFired = false;
};

// ---------------------------------------------------------------------------
// 16-step ring with the central encoder
// ---------------------------------------------------------------------------
class StepRing final : public juce::Component, public juce::SettableTooltipClient
{
public:
    // Supplied by the editor.
    std::function<int()>  getFirst;    // 0..15
    std::function<int()>  getLast;     // 0..15
    std::function<int()>  getStep;     // -1 when stopped
    std::function<bool()> firstLatched;
    std::function<bool()> lastLatched;
    std::function<void (int)> onTurn;  // encoder detents
    std::function<void()> onPush;      // click without turning

    void setBlinkPhase (bool on) { if (on != blink) { blink = on; repaint(); } }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr float ledRadius = 86.0f;
    static constexpr float numberRadius = 112.0f;
    static constexpr float encoderRadius = 42.0f;

private:
    juce::Point<float> centre() const { return getLocalBounds().toFloat().getCentre(); }
    bool overEncoder (juce::Point<float> p) const { return p.getDistanceFrom (centre()) <= encoderRadius + 6.0f; }

    float encoderAngle = 0.0f;
    int   dragAccum = 0;
    float dragStartY = 0.0f;
    bool  dragging = false;
    bool  moved = false;
    bool  blink = false;
    float wheelAccum = 0.0f;
};

// ---------------------------------------------------------------------------
// Options table: mute mode, VARIATION values, scale presets, pattern length
// ---------------------------------------------------------------------------
class OptionTable final : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Cell
    {
        juce::String text;
        std::function<bool()> isActive;
        std::function<void()> onClick;
    };

    struct Row
    {
        juce::String label;
        std::array<Cell, 4> cells;
    };

    void setRows (std::vector<Row> r) { rows = std::move (r); repaint(); }
    void refresh() { repaint(); }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr float indexW  = 28.0f;
    static constexpr float labelW  = 92.0f;
    static constexpr float headerH = 34.0f;

private:
    juce::Rectangle<float> cellBounds (int row, int col) const;
    std::pair<int, int> hitCell (juce::Point<float>) const;

    std::vector<Row> rows;
    std::pair<int, int> hover { -1, -1 };
};

// ---------------------------------------------------------------------------
// Click-to-choose value display (same idea as the sibling plugins' rail)
// ---------------------------------------------------------------------------
class ChoiceDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::String()> getText;
    std::function<void (juce::PopupMenu&)> buildMenu;
    std::function<void (int)> onChoose;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    // Repaints only when the shown text changes; cheap to call from a timer.
    void refresh();

private:
    juce::String shown;
};

// ---------------------------------------------------------------------------
// Small LCD-style readout
// ---------------------------------------------------------------------------
class Display final : public juce::Component
{
public:
    void setText (const juce::String& big, const juce::String& small);
    void paint (juce::Graphics&) override;

private:
    juce::String main, sub;
};

} // namespace dicey::ui
