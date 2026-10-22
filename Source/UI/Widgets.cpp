#include "Widgets.h"

namespace dicey::ui
{
using namespace juce;

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------
Font panelFont (float height, bool bold, bool italic)
{
    int style = Font::plain;
    if (bold) style |= Font::bold;
    if (italic) style |= Font::italic;

   #if JUCE_MAC
    Font f (FontOptions ("Helvetica Neue", height, style));
   #else
    Font f (FontOptions (height, style));
   #endif
    f.setHorizontalScale (0.86f);
    return f;
}

void drawLabel (Graphics& g, const String& text, float cx, float cy, float height,
                Colour colour, bool bold, bool italic)
{
    g.setColour (colour);
    g.setFont (panelFont (height, bold, italic));
    const float w = jmax (40.0f, height * 0.75f * (float) text.length() + 20.0f);
    g.drawText (text, Rectangle<float> (cx - w * 0.5f, cy - height, w, height * 2.0f), Justification::centred, false);
}

// ---------------------------------------------------------------------------
// LookAndFeel
// ---------------------------------------------------------------------------
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, colours::black.withAlpha (0.92f));
    setColour (BubbleComponent::outlineColourId, colours::accent);
    setColour (TooltipWindow::textColourId, colours::ink);
}

void PanelLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider&)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c      = bounds.getCentre();
    const float r     = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Shadow
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, r * 0.06f)));

    // Skirt
    ColourGradient skirt (colours::cream, c.x - r * 0.4f, c.y - r * 0.6f,
                          colours::creamShade, c.x + r * 0.5f, c.y + r * 0.8f, true);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.2f);

    // Seven-sided cap that turns with the knob
    Path cap;
    const float pr = r * 0.80f;
    for (int i = 0; i < 7; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 7.0f;
        const Point<float> p (c.x + pr * std::sin (a), c.y - pr * std::cos (a));
        if (i == 0) cap.startNewSubPath (p); else cap.lineTo (p);
    }
    cap.closeSubPath();
    cap = cap.createPathWithRoundedCorners (r * 0.12f);
    ColourGradient top (colours::cream.brighter (0.05f), c.x, c.y - pr,
                        colours::cream.darker (0.04f), c.x, c.y + pr, false);
    g.setGradientFill (top);
    g.fillPath (cap);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.strokePath (cap, PathStrokeType (jmax (1.0f, r * 0.022f)));

    // Pointer
    const float len = r * 0.78f;
    const Point<float> tip (c.x + len * std::sin (angle), c.y - len * std::cos (angle));
    const Point<float> tail (c.x + r * 0.05f * std::sin (angle + MathConstants<float>::pi),
                             c.y - r * 0.05f * std::cos (angle + MathConstants<float>::pi));
    g.setColour (colours::black);
    g.drawLine (Line<float> (tail, tip), jmax (2.0f, r * 0.075f));
}

void PanelLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float, float, Slider::SliderStyle, Slider& s)
{
    const float cx    = (float) x + (float) w * 0.5f;
    const float slotW = 18.0f;
    const auto  slot  = Rectangle<float> (cx - slotW * 0.5f, (float) y + 4.0f, slotW, (float) h - 8.0f);

    ColourGradient grad (colours::slotHi, slot.getX(), 0.0f, colours::slotLo, slot.getRight(), 0.0f, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (slot.withSizeKeepingCentre (4.0f, slot.getHeight() - 8.0f), 2.0f);

    // Cap
    const bool lit = (bool) s.getProperties().getWithDefault ("lit", false);
    const auto capR = Rectangle<float> (cx - 11.0f, sliderPos - faderCapHeight * 0.5f, 22.0f, faderCapHeight);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (capR.translated (0.0f, 2.0f), 3.0f);
    g.setColour (Colour (0xff1a1a1b));
    g.fillRoundedRectangle (capR, 3.0f);
    g.setColour (Colour (0xff3a3a3c));
    g.drawRoundedRectangle (capR, 3.0f, 1.0f);

    const auto window = capR.reduced (4.0f, 9.0f);
    if (lit)
    {
        g.setColour (colours::ledRed.withAlpha (0.35f));
        g.fillRoundedRectangle (window.expanded (5.0f), 5.0f);
    }
    g.setColour (lit ? colours::ledRed : Colour (0xff7a0c1a));
    g.fillRoundedRectangle (window, 2.0f);
    g.setColour (Colours::white.withAlpha (lit ? 0.55f : 0.15f));
    g.fillRect (window.withHeight (2.0f).translated (0.0f, window.getHeight() * 0.5f - 1.0f));
}

// ---------------------------------------------------------------------------
// Knob / Fader
// ---------------------------------------------------------------------------
Knob::Knob (bool centreDetent) : detent (centreDetent)
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
}

double Knob::snapValue (double attempted, DragMode)
{
    if (detent && std::abs (attempted) < 0.05 * (getMaximum() - getMinimum()) * 0.5)
        return 0.0;
    return attempted;
}

Fader::Fader()
{
    setSliderStyle (Slider::LinearVertical);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setMouseDragSensitivity (300);
}

void Fader::setLit (bool shouldBeLit)
{
    if (lit == shouldBeLit)
        return;
    lit = shouldBeLit;
    getProperties().set ("lit", lit);
    repaint();
}

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
void Led::setLevel (float newLevel)
{
    newLevel = jlimit (0.0f, 1.0f, newLevel);
    if (std::abs (newLevel - level) > 0.01f)
    {
        level = newLevel;
        repaint();
    }
}

void Led::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.25f;

    if (level > 0.0f)
    {
        ColourGradient glow (colour.withAlpha (0.55f * level), c.x, c.y,
                             colour.withAlpha (0.0f), c.x + r * 2.0f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (b);
    }

    const Colour off = colour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.32f);
    const Colour face = off.interpolatedWith (colour, level);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (c));
    ColourGradient lens (face.brighter (0.4f * level + 0.1f), c.x - r * 0.4f, c.y - r * 0.4f,
                         face, c.x + r * 0.6f, c.y + r * 0.6f, true);
    g.setGradientFill (lens);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.25f + 0.35f * level));
    g.fillEllipse (Rectangle<float> (r * 0.6f, r * 0.45f).withCentre (c.translated (-r * 0.3f, -r * 0.4f)));
}

// ---------------------------------------------------------------------------
// RoundButton
// ---------------------------------------------------------------------------
void RoundButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // Bezel
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, 2.0f)));
    ColourGradient bezel (Colour (0xffd8d8d8), c.x, c.y - r, Colour (0xff6c6c6c), c.x, c.y + r, false);
    g.setGradientFill (bezel);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

    Colour face = style == Style::black ? colours::black : (style == Style::tan ? colours::tan : colours::cream);
    if (pressed)
        face = face.darker (style == Style::black ? 0.0f : 0.18f).brighter (style == Style::black ? 0.15f : 0.0f);

    const float fr = r * (pressed ? 0.84f : 0.87f);
    ColourGradient faceGrad (face.brighter (0.08f), c.x - fr * 0.3f, c.y - fr * 0.5f,
                             face.darker (0.10f), c.x + fr * 0.4f, c.y + fr * 0.7f, true);
    g.setGradientFill (faceGrad);
    g.fillEllipse (Rectangle<float> (fr * 2.0f, fr * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (fr * 2.0f, fr * 2.0f).withCentre (c), 1.0f);
}

void RoundButton::mouseDown (const MouseEvent&)
{
    pressed   = true;
    longFired = false;
    if (onLongPress)
        startTimer (600);
    repaint();
}

void RoundButton::mouseUp (const MouseEvent& e)
{
    stopTimer();
    const bool inside = getLocalBounds().contains (e.getPosition());
    pressed = false;
    repaint();
    if (! longFired && inside && onClick)
        onClick();
}

void RoundButton::timerCallback()
{
    stopTimer();
    longFired = true;
    if (onLongPress)
        onLongPress();
}

// ---------------------------------------------------------------------------
// StepRing
// ---------------------------------------------------------------------------
void StepRing::paint (Graphics& g)
{
    const auto c     = centre();
    const int  first = getFirst ? getFirst() : 0;
    const int  last  = getLast ? getLast() : 15;
    const int  step  = getStep ? getStep() : -1;
    const bool fl    = firstLatched && firstLatched();
    const bool ll    = lastLatched && lastLatched();

    for (int i = 0; i < 16; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 16.0f;
        const Point<float> p (c.x + ledRadius * std::sin (a), c.y - ledRadius * std::cos (a));
        const Point<float> n (c.x + numberRadius * std::sin (a), c.y - numberRadius * std::cos (a));

        drawLabel (g, String (i + 1), n.x, n.y, 15.0f, colours::ink);

        const bool inWindow = i >= first && i <= last;
        const bool beat     = (i % 4) == 0;
        Colour col          = beat ? colours::ledRed : colours::ledGreen;
        float  level        = inWindow ? 0.42f : 0.0f;

        if (i == step)
            level = 1.0f;

        if ((fl && i == first) || (ll && i == last))
        {
            col   = colours::ledYellow;
            level = blink ? 1.0f : 0.35f;
        }

        const float r = 10.0f;
        const Colour off  = col.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.32f);
        const Colour face = off.interpolatedWith (col, level);

        if (level > 0.5f)
        {
            ColourGradient glow (col.withAlpha (0.5f * level), p.x, p.y, col.withAlpha (0.0f), p.x + r * 2.4f, p.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (Rectangle<float> (r * 4.8f, r * 4.8f).withCentre (p));
        }
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (p));
        ColourGradient lens (face.brighter (0.4f * level + 0.1f), p.x - r * 0.4f, p.y - r * 0.4f, face,
                             p.x + r * 0.6f, p.y + r * 0.6f, true);
        g.setGradientFill (lens);
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
        g.setColour (Colours::white.withAlpha (0.2f + 0.35f * level));
        g.fillEllipse (Rectangle<float> (r * 0.6f, r * 0.45f).withCentre (p.translated (-r * 0.3f, -r * 0.4f)));
    }

    // Encoder
    const float er = encoderRadius;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (er * 2.0f + 6.0f, er * 2.0f + 6.0f).withCentre (c.translated (0.0f, 2.0f)));
    ColourGradient body (Colour (0xff2b2b2d), c.x - er * 0.5f, c.y - er * 0.7f, Colour (0xff050506), c.x + er * 0.5f, c.y + er, true);
    g.setGradientFill (body);
    g.fillEllipse (Rectangle<float> (er * 2.0f, er * 2.0f).withCentre (c));

    // Knurled edge, turning with the encoder
    g.setColour (Colour (0xff3c3c3f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = encoderAngle + (float) i * MathConstants<float>::twoPi / 36.0f;
        g.drawLine (c.x + (er - 5.0f) * std::sin (a), c.y - (er - 5.0f) * std::cos (a),
                    c.x + (er - 1.0f) * std::sin (a), c.y - (er - 1.0f) * std::cos (a), 1.4f);
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillEllipse (Rectangle<float> (er * 1.1f, er * 0.7f).withCentre (c.translated (-er * 0.15f, -er * 0.35f)));
}

void StepRing::mouseDown (const MouseEvent& e)
{
    dragging   = overEncoder (e.position);
    moved      = false;
    dragStartY = e.position.y;
    dragAccum  = 0;
}

void StepRing::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;

    const int detents = (int) ((dragStartY - e.position.y) / 12.0f);
    if (detents != dragAccum)
    {
        const int delta = detents - dragAccum;
        dragAccum = detents;
        moved = true;
        encoderAngle += (float) delta * MathConstants<float>::twoPi / 24.0f;
        if (onTurn) onTurn (delta);
        repaint();
    }
}

void StepRing::mouseUp (const MouseEvent& e)
{
    if (dragging && ! moved && overEncoder (e.position) && onPush)
        onPush();
    dragging = false;
}

void StepRing::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    wheelAccum += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 8.0f;
    const int detents = (int) wheelAccum;
    if (detents != 0)
    {
        wheelAccum -= (float) detents;
        encoderAngle += (float) detents * MathConstants<float>::twoPi / 24.0f;
        if (onTurn) onTurn (detents);
        repaint();
    }
}

// ---------------------------------------------------------------------------
// OptionTable
// ---------------------------------------------------------------------------
Rectangle<float> OptionTable::cellBounds (int row, int col) const
{
    const auto b = getLocalBounds().toFloat();
    const float rowH  = rows.empty() ? 0.0f : (b.getHeight() - headerH) / (float) rows.size();
    const float cellW = (b.getWidth() - indexW - labelW) / 4.0f;
    const float x = b.getX() + indexW + labelW + cellW * (float) col;
    const float y = b.getY() + headerH + rowH * (float) row;
    return { x, y, cellW, rowH };
}

std::pair<int, int> OptionTable::hitCell (Point<float> p) const
{
    for (int r = 0; r < (int) rows.size(); ++r)
        for (int c = 0; c < 4; ++c)
            if (cellBounds (r, c).contains (p) && rows[(size_t) r].cells[(size_t) c].onClick)
                return { r, c };
    return { -1, -1 };
}

void OptionTable::paint (Graphics& g)
{
    const auto b     = getLocalBounds().toFloat();
    const float cellW = (b.getWidth() - indexW - labelW) / 4.0f;
    const float left  = b.getX() + indexW + labelW;

    // Header
    const auto header = Rectangle<float> (left, b.getY(), cellW * 4.0f, headerH);
    g.setColour (colours::ink);
    g.fillRect (header);
    const char* letters[] = { "A", "B", "C", "D" };
    for (int c = 0; c < 4; ++c)
        drawLabel (g, letters[c], left + cellW * ((float) c + 0.5f), header.getCentreY(), 19.0f, colours::panel);

    // Rows
    for (int r = 0; r < (int) rows.size(); ++r)
    {
        const auto& row  = rows[(size_t) r];
        const auto  rb   = cellBounds (r, 0);
        const float rowY = rb.getY();
        const float rowH = rb.getHeight();

        drawLabel (g, String (r + 1), b.getX() + indexW * 0.5f, rowY + rowH * 0.5f, 14.0f);
        g.setColour (colours::ink);
        g.setFont (panelFont (14.0f));
        g.drawText (row.label, Rectangle<float> (b.getX() + indexW + 5.0f, rowY, labelW - 6.0f, rowH),
                    Justification::centredLeft, false);

        for (int c = 0; c < 4; ++c)
        {
            const auto& cell = row.cells[(size_t) c];
            const auto  cb   = cellBounds (r, c);
            const bool  active = cell.isActive && cell.isActive();
            const bool  hot    = hover == std::pair<int, int> { r, c };

            if (active)
            {
                g.setColour (colours::accent);
                g.fillRect (cb.reduced (2.0f));
            }
            else if (hot)
            {
                g.setColour (colours::ink.withAlpha (0.12f));
                g.fillRect (cb.reduced (2.0f));
            }

            if (cell.text.isNotEmpty())
                drawLabel (g, cell.text, cb.getCentreX(), cb.getCentreY(), 14.0f,
                           active ? colours::panel : colours::ink, true, true);
        }
    }

    // Grid
    g.setColour (colours::ink);
    const float top    = b.getY() + headerH;
    const float bottom = b.getBottom() - 1.0f;
    g.drawRect (Rectangle<float> (b.getX() + 1.0f, top, b.getWidth() - 2.0f, bottom - top), 2.0f);
    for (int r = 1; r < (int) rows.size(); ++r)
    {
        const float y = cellBounds (r, 0).getY();
        g.drawLine (b.getX() + 1.0f, y, b.getRight() - 1.0f, y, 1.2f);
    }
    g.drawLine (b.getX() + indexW, top, b.getX() + indexW, bottom, 1.2f);
    g.drawLine (left, b.getY(), left, bottom, 1.6f);
    for (int c = 1; c < 4; ++c)
        g.drawLine (left + cellW * (float) c, b.getY(), left + cellW * (float) c, bottom, 1.2f);
}

void OptionTable::mouseUp (const MouseEvent& e)
{
    const auto [r, c] = hitCell (e.position);
    if (r >= 0)
    {
        rows[(size_t) r].cells[(size_t) c].onClick();
        repaint();
    }
}

void OptionTable::mouseMove (const MouseEvent& e)
{
    const auto h = hitCell (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h.first >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
        repaint();
    }
}

void OptionTable::mouseExit (const MouseEvent&)
{
    hover = { -1, -1 };
    repaint();
}

// ---------------------------------------------------------------------------
// ChoiceDisplay
// ---------------------------------------------------------------------------
void ChoiceDisplay::refresh()
{
    if (getText != nullptr && getText() != shown)
        repaint();
}

void ChoiceDisplay::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colour (0xff0b0f0e));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (isMouseOver() ? colours::accent.withAlpha (0.85f) : Colour (0xff4a4a4c));
    g.drawRoundedRectangle (r, 5.0f, 1.5f);

    if (getText != nullptr)
        shown = getText();
    g.setColour (colours::accent);
    g.setFont (panelFont (r.getHeight() * 0.56f));
    g.drawText (shown, r.withTrimmedRight (14.0f), Justification::centred, false);

    // Drop-down hint
    const float sz = 4.5f;
    const float cx = r.getRight() - 11.0f, cy = r.getCentreY();
    Path tri;
    tri.addTriangle (cx - sz, cy - sz * 0.5f, cx + sz, cy - sz * 0.5f, cx, cy + sz * 0.7f);
    g.setColour (colours::inkDim);
    g.fillPath (tri);
}

void ChoiceDisplay::mouseDown (const MouseEvent&)
{
    if (! buildMenu)
        return;
    PopupMenu m;
    buildMenu (m);
    auto safe = Component::SafePointer<ChoiceDisplay> (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                     [safe] (int result)
                     {
                         if (safe != nullptr && result != 0 && safe->onChoose)
                             safe->onChoose (result);
                     });
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void Display::setText (const String& big, const String& small)
{
    if (big != main || small != sub)
    {
        main = big;
        sub  = small;
        repaint();
    }
}

void Display::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colour (0xff0b0f0e));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (Colour (0xff4a4a4c));
    g.drawRoundedRectangle (b, 5.0f, 1.5f);

    g.setColour (colours::accent);
    g.setFont (panelFont (b.getHeight() * 0.48f));
    g.drawText (main, b.withTrimmedBottom (b.getHeight() * 0.32f), Justification::centred, false);
    g.setColour (colours::accent.withAlpha (0.6f));
    g.setFont (panelFont (b.getHeight() * 0.2f, true));
    g.drawText (sub, b.withTrimmedTop (b.getHeight() * 0.62f).withTrimmedBottom (3.0f), Justification::centred, false);
}

} // namespace dicey::ui
