#include "PluginEditor.h"

// ============================================================================
// Helpers
// ============================================================================
void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, float glow)
{
    juce::Path shape;
    shape.addRoundedRectangle (r, corner);

    // Outer neon glow
    juce::DropShadow (Palette::cyan.withAlpha (glow * 0.5f), 18, {}).drawForPath (g, shape);

    // Translucent body (stars and nebula show through)
    g.setColour (Palette::glass);
    g.fillPath (shape);

    // Top sheen
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.35f, false));
    g.fillPath (shape);

    // Edge: cyan on top fading to violet at the bottom
    g.setGradientFill (juce::ColourGradient (Palette::cyan.withAlpha (glow + 0.15f), r.getX(), r.getY(),
                                             Palette::nebulaA.withAlpha (glow), r.getX(), r.getBottom(), false));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

static juce::Font spacedFont (float height, bool bold, float kerning)
{
    auto opts = juce::FontOptions (height);
    if (bold) opts = opts.withStyle ("Bold");
    return juce::Font (opts).withExtraKerningFactor (kerning);
}

// ============================================================================
// Look and feel
// ============================================================================
GalaxyLookAndFeel::GalaxyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Palette::cyan);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  Palette::nebulaA);
    setColour (juce::ComboBox::textColourId,            Palette::text);
    setColour (juce::ComboBox::arrowColourId,           Palette::cyan);
    setColour (juce::PopupMenu::textColourId,           Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::nebulaA.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId,        Palette::text);
    setColour (juce::TextButton::textColourOffId,       Palette::text);
    setColour (juce::TextButton::textColourOnId,        Palette::cyan);
    setColour (juce::Label::textColourId,               Palette::textDim);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::spaceMid);
    setColour (juce::TooltipWindow::textColourId,       Palette::text);
}

void GalaxyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool hover = slider.isMouseOverOrDragging();

    // Dial tick marks: lit up to the current value
    constexpr int numTicks = 41;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = (float) i / (float) (numTicks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const bool lit = a <= angle + 0.001f;
        const bool major = (i % 5 == 0);
        const auto col = lit ? Palette::cyan.interpolatedWith (Palette::magenta, t)
                             : Palette::textDim.withAlpha (0.25f);
        g.setColour (col.withAlpha (lit ? 0.95f : (major ? 0.45f : 0.25f)));
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * (major ? 0.88f : 0.92f), a),
                                       c.getPointOnCircumference (r, a)), major ? 1.6f : 1.0f);
    }

    // Arc track + glowing value arc (cyan -> magenta)
    const float arcR = r * 0.80f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::glassEdge.withAlpha (0.7f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        const juce::ColourGradient grad (Palette::cyan, bounds.getX(), bounds.getBottom(),
                                         Palette::magenta, bounds.getRight(), bounds.getY(), false);
        const float glowBoost = hover ? 1.4f : 1.0f;
        for (auto [width, alpha] : { std::pair { 12.0f, 0.10f }, std::pair { 7.0f, 0.22f }, std::pair { 3.0f, 1.0f } })
        {
            auto gg = grad;
            gg.multiplyOpacity (juce::jmin (1.0f, alpha * glowBoost));
            g.setGradientFill (gg);
            g.strokePath (value, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Knob body: dark metallic sphere
    const float body = r * 0.60f;
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 14, { 0, 4 })
        .drawForPath (g, [&] { juce::Path p; p.addEllipse (c.x - body, c.y - body, body * 2, body * 2); return p; }());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343c78), c.x - body * 0.35f, c.y - body * 0.45f,
                                             juce::Colour (0xff05071a), c.x + body, c.y + body, true));
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);

    // Rim
    g.setColour (Palette::cyan.withAlpha (hover ? 0.55f : 0.3f));
    g.drawEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (c.x - body * 0.8f, c.y - body * 0.8f, body * 1.6f, body * 1.6f, 1.0f);

    // Glowing pointer "star"
    const auto dot = c.getPointOnCircumference (body * 0.70f, angle);
    g.setColour (Palette::cyan.withAlpha (0.25f));
    g.fillEllipse (dot.x - 7.0f, dot.y - 7.0f, 14.0f, 14.0f);
    g.setColour (Palette::cyan.withAlpha (0.55f));
    g.fillEllipse (dot.x - 4.0f, dot.y - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (dot.x - 2.0f, dot.y - 2.0f, 4.0f, 4.0f);
}

void GalaxyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    const bool hover = box.isMouseOver (true);

    g.setColour (Palette::spaceMid.withAlpha (0.85f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setGradientFill (juce::ColourGradient (Palette::cyan.withAlpha (hover ? 0.9f : 0.55f), r.getX(), r.getY(),
                                             Palette::magenta.withAlpha (hover ? 0.9f : 0.55f), r.getRight(), r.getY(), false));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);

    // Chevron
    const float ax = r.getRight() - 16.0f, ay = r.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (Palette::cyan);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font GalaxyLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f));
}

void GalaxyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    // Neon pill switch
    auto sw = bounds.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
    g.setColour (on ? Palette::cyan.withAlpha (0.22f) : Palette::spaceMid);
    g.fillRoundedRectangle (sw, 8.0f);
    g.setColour (on ? Palette::cyan : Palette::glassEdge.brighter (highlighted ? 0.4f : 0.0f));
    g.drawRoundedRectangle (sw, 8.0f, 1.2f);

    const float kx = on ? sw.getRight() - 13.0f : sw.getX() + 3.0f;
    if (on)
    {
        g.setColour (Palette::cyan.withAlpha (0.35f));
        g.fillEllipse (kx - 3.0f, sw.getY() - 1.0f, 16.0f, 16.0f);
    }
    g.setColour (on ? juce::Colours::white : Palette::textDim);
    g.fillEllipse (kx, sw.getY() + 3.0f, 10.0f, 10.0f);

    g.setColour (on ? Palette::text : Palette::textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (b.getButtonText(), bounds.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

void GalaxyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::spaceMid.withAlpha (down ? 1.0f : 0.8f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Palette::magenta.withAlpha (highlighted ? 0.9f : 0.5f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
}

void GalaxyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Palette::spaceMid);
    g.setColour (Palette::cyan.withAlpha (0.4f));
    g.drawRect (0, 0, width, height, 1);
}

// ============================================================================
// History display
// ============================================================================
void HistoryDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();

    juce::Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion ([&] { juce::Path p; p.addRoundedRectangle (b.reduced (1.0f), 10.0f); return p; }());

    // Radar-style grid
    g.setFont (juce::FontOptions (11.0f));
    for (float db = 0.0f; db >= kBottomDb; db -= 6.0f)
    {
        const float yy = dbToY (db, h);
        g.setColour (Palette::cyan.withAlpha (db == 0.0f ? 0.18f : 0.07f));
        g.drawHorizontalLine ((int) yy, 0.0f, w);
        g.setColour (Palette::textDim.withAlpha (0.8f));
        g.drawText (juce::String ((int) db), 8, (int) yy - 13, 40, 12, juce::Justification::left);
    }
    for (float xx = w; xx > 0.0f; xx -= 100.0f)
    {
        g.setColour (Palette::cyan.withAlpha (0.04f));
        g.drawVerticalLine ((int) xx, 0.0f, h);
    }

    const float px = 2.0f;
    const int visible = juce::jmin (count, (int) (w / px) + 1);

    if (visible > 1)
    {
        auto frameAt = [this] (int k) -> const HistoryFrame& { return frames[(size_t) ((writePos - 1 - k + kLen) % kLen)]; };

        juce::Path inPath, outFill, outLine, grLine;
        inPath.startNewSubPath (w, h);
        outFill.startNewSubPath (w, h);
        const float grScale = h / (kTopDb - kBottomDb);

        for (int k = 0; k < visible; ++k)
        {
            const auto& f = frameAt (k);
            const float xx = w - (float) k * px;
            const float yo = dbToY (f.outDb, h);
            const float yg = juce::jmin (h, f.grDb * grScale);
            inPath.lineTo (xx, dbToY (f.inDb, h));
            outFill.lineTo (xx, yo);
            if (k == 0) { outLine.startNewSubPath (xx, yo); grLine.startNewSubPath (xx, yg); }
            else        { outLine.lineTo (xx, yo);          grLine.lineTo (xx, yg); }
        }

        const float leftX = w - (float) (visible - 1) * px;
        inPath.lineTo (leftX, h);  inPath.closeSubPath();
        outFill.lineTo (leftX, h); outFill.closeSubPath();

        // Input: violet haze
        g.setGradientFill (juce::ColourGradient (Palette::input.withAlpha (0.55f), 0.0f, 0.0f,
                                                 Palette::input.withAlpha (0.08f), 0.0f, h, false));
        g.fillPath (inPath);

        // Output: cyan plasma with a bright edge
        g.setGradientFill (juce::ColourGradient (Palette::output.withAlpha (0.45f), 0.0f, 0.0f,
                                                 Palette::nebulaB.withAlpha (0.10f), 0.0f, h, false));
        g.fillPath (outFill);
        g.setColour (Palette::output.withAlpha (0.25f));
        g.strokePath (outLine, juce::PathStrokeType (4.0f));
        g.setColour (Palette::output);
        g.strokePath (outLine, juce::PathStrokeType (1.2f));

        // Gain reduction: glowing pink beam from the top
        g.setColour (Palette::reduction.withAlpha (0.2f));
        g.strokePath (grLine, juce::PathStrokeType (6.0f));
        g.setColour (Palette::reduction);
        g.strokePath (grLine, juce::PathStrokeType (1.6f));
    }

    // Ceiling line
    const float cy = dbToY (ceilingDb, h);
    const float dashes[] = { 8.0f, 5.0f };
    g.setColour (Palette::magenta.withAlpha (0.25f));
    g.fillRect (0.0f, cy - 2.0f, w, 4.0f);
    g.setColour (Palette::magenta);
    g.drawDashedLine (juce::Line<float> (0.0f, cy, w, cy), dashes, 2, 1.2f);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Ceiling " + juce::String (ceilingDb, 1) + " dB",
                juce::Rectangle<float> (w - 130.0f, cy - 16.0f, 122.0f, 14.0f), juce::Justification::right);
}

// ============================================================================
// Editor
// ============================================================================
MasterLimiterAudioProcessorEditor::MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    addAndMakeVisible (display);

    const char* ids[6]   = { "gain", "ceiling", "attack", "release", "lookahead", "link" };
    const char* names[6] = { "Gain", "Output ceiling", "Attack", "Release", "Lookahead", "Stereo link" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (spacedFont (13.0f, false, 0.08f));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, ids[i], k.slider);
    }
    knobs[0].slider.setColour (juce::Slider::textBoxTextColourId, Palette::magenta);

    setupCombo (styleBox,  styleLabel,  "style",        "Style");
    setupCombo (osBox,     osLabel,     "oversampling", "Oversampling");
    setupCombo (ditherBox, ditherLabel, "dither",       "Dither");
    styleAttachment  = std::make_unique<ComboBoxAttachment> (proc.apvts, "style",        styleBox);
    osAttachment     = std::make_unique<ComboBoxAttachment> (proc.apvts, "oversampling", osBox);
    ditherAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "dither",       ditherBox);

    for (auto* b : { &truePeakButton, &unityButton, &deltaButton })
        addAndMakeVisible (*b);
    truePeakAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "truepeak", truePeakButton);
    unityAttachment    = std::make_unique<ButtonAttachment> (proc.apvts, "unity",    unityButton);
    deltaAttachment    = std::make_unique<ButtonAttachment> (proc.apvts, "delta",    deltaButton);

    truePeakButton.setTooltip ("Detects inter-sample peaks using at least 4x oversampling");
    unityButton.setTooltip ("Monitor with the input gain removed, to judge tone rather than loudness");
    deltaButton.setTooltip ("Hear only what the limiter removes");

    styleDescription.setFont (juce::FontOptions (13.0f));
    styleDescription.setColour (juce::Label::textColourId, Palette::textDim.brighter (0.3f));
    addAndMakeVisible (styleDescription);

    resetButton.onClick = [this] { proc.loudness.requestReset(); };
    addAndMakeVisible (resetButton);

    setSize (1000, 660);
    startTimerHz (30);
}

MasterLimiterAudioProcessorEditor::~MasterLimiterAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MasterLimiterAudioProcessorEditor::setupCombo (juce::ComboBox& box, juce::Label& label,
                                                    const juce::String& paramId, const juce::String& labelText)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);

    addAndMakeVisible (box);
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (label);
}

// Starfield + nebula, rendered once per resize at 2x for sharp HiDPI
void MasterLimiterAudioProcessorEditor::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;

    constexpr float scale = 2.0f;
    background = juce::Image (juce::Image::RGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (scale));

    const float fw = (float) w, fh = (float) h;

    // Deep space base
    g.setGradientFill (juce::ColourGradient (Palette::spaceMid, 0.0f, 0.0f, Palette::space, 0.0f, fh, false));
    g.fillAll();

    // Nebula clouds (layered radial glows)
    auto nebula = [&] (juce::Colour col, float cx, float cy, float radius, float alpha)
    {
        g.setGradientFill (juce::ColourGradient (col.withAlpha (alpha), cx, cy,
                                                 col.withAlpha (0.0f), cx + radius, cy, true));
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    };
    nebula (Palette::nebulaA, fw * 0.22f, fh * 0.30f, fw * 0.45f, 0.35f);
    nebula (Palette::nebulaB, fw * 0.70f, fh * 0.18f, fw * 0.40f, 0.30f);
    nebula (Palette::nebulaC, fw * 0.55f, fh * 0.55f, fw * 0.30f, 0.18f);
    nebula (Palette::nebulaA, fw * 0.90f, fh * 0.75f, fw * 0.30f, 0.22f);
    nebula (Palette::cyan,    fw * 0.35f, fh * 0.50f, fw * 0.18f, 0.06f);

    // Stars
    juce::Random rng (7919);
    for (int i = 0; i < 520; ++i)
    {
        const float sx = rng.nextFloat() * fw, sy = rng.nextFloat() * fh;
        const float bright = std::pow (rng.nextFloat(), 3.0f);
        const float size = 0.5f + bright * 1.6f;
        const auto tint = rng.nextFloat() < 0.2f ? Palette::cyan : (rng.nextFloat() < 0.15f ? Palette::magenta : juce::Colours::white);
        g.setColour (tint.withAlpha (0.25f + bright * 0.75f));
        g.fillEllipse (sx - size * 0.5f, sy - size * 0.5f, size, size);

        if (bright > 0.75f)   // big stars get a glow and a cross flare
        {
            g.setColour (tint.withAlpha (0.12f));
            g.fillEllipse (sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
            g.setColour (tint.withAlpha (0.35f));
            g.drawLine (sx - 6.0f, sy, sx + 6.0f, sy, 0.6f);
            g.drawLine (sx, sy - 6.0f, sx, sy + 6.0f, 0.6f);
        }
    }

    // Planet horizon glowing along the bottom of the window
    const float pr = fw * 1.1f;
    const float pcx = fw * 0.5f, pcy = fh + pr - 120.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1030), pcx, pcy - pr,
                                             juce::Colour (0xff02030a), pcx, pcy - pr + 200.0f, false));
    g.fillEllipse (pcx - pr, pcy - pr, pr * 2.0f, pr * 2.0f);
    for (auto [width, alpha] : { std::pair { 14.0f, 0.06f }, std::pair { 6.0f, 0.15f }, std::pair { 1.5f, 0.7f } })
    {
        g.setColour (Palette::cyan.withAlpha (alpha));
        g.drawEllipse (pcx - pr, pcy - pr, pr * 2.0f, pr * 2.0f, width);
    }

    // Glass panels are static, so they are baked in here (the glow blur is expensive)
    drawGlassPanel (g, headerArea.toFloat(), 14.0f, 0.30f);
    drawGlassPanel (g, knobPanel.toFloat(), 14.0f, 0.35f);
    drawGlassPanel (g, meterArea.withTrimmedBottom (38).toFloat(), 10.0f, 0.40f);
    drawGlassPanel (g, display.getBounds().toFloat().reduced (1.0f), 10.0f, 0.40f);
}

void MasterLimiterAudioProcessorEditor::drawLogo (juce::Graphics& g)
{
    const auto a = logoArea.toFloat();
    const auto c = juce::Point<float> (a.getX() + 22.0f, a.getCentreY());

    // Planet
    const float pr = 9.0f;
    g.setGradientFill (juce::ColourGradient (Palette::magenta, c.x - 4.0f, c.y - 5.0f,
                                             Palette::nebulaA.darker (0.6f), c.x + pr, c.y + pr, true));
    g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);

    // Tilted orbit ring with a moving moon
    const auto tilt = juce::AffineTransform::rotation (-0.35f, c.x, c.y);
    juce::Path ring;
    ring.addEllipse (c.x - 19.0f, c.y - 6.0f, 38.0f, 12.0f);
    g.setColour (Palette::cyan.withAlpha (0.8f));
    g.strokePath (ring, juce::PathStrokeType (1.2f), tilt);

    auto moon = juce::Point<float> (c.x + 19.0f * std::cos (orbitPhase), c.y + 6.0f * std::sin (orbitPhase)).transformedBy (tilt);
    g.setColour (Palette::cyan.withAlpha (0.3f));
    g.fillEllipse (moon.x - 5.0f, moon.y - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (moon.x - 2.0f, moon.y - 2.0f, 4.0f, 4.0f);

    // Wordmark with glow
    const auto textArea = a.withTrimmedLeft (50.0f);
    g.setFont (spacedFont (22.0f, true, 0.32f));
    g.setColour (Palette::cyan.withAlpha (0.18f));
    for (auto [dx, dy] : { std::pair { -1.5f, 0.0f }, std::pair { 1.5f, 0.0f }, std::pair { 0.0f, -1.5f }, std::pair { 0.0f, 1.5f } })
        g.drawText ("GALAXY", textArea.translated (dx, dy - 7.0f), juce::Justification::centredLeft);
    g.setColour (Palette::text);
    g.drawText ("GALAXY", textArea.translated (0.0f, -7.0f), juce::Justification::centredLeft);

    g.setFont (spacedFont (11.0f, false, 0.55f));
    g.setColour (Palette::cyan);
    g.drawText ("LIMITER", textArea.translated (1.0f, 12.0f), juce::Justification::centredLeft);
}

void MasterLimiterAudioProcessorEditor::timerCallback()
{
    HistoryFrame tmp[128];
    int got = 0;
    while ((got = proc.popHistory (tmp, 128)) > 0)
        for (int i = 0; i < got; ++i)
            display.pushFrame (tmp[i]);

    display.setCeilingDb (proc.apvts.getRawParameterValue ("ceiling")->load());

    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (proc.outPeak[c].exchange (0.0f), -100.0f);
        meterDb[c] = juce::jmax (db, meterDb[c] - 1.5f);
    }
    grDisplayDb = juce::jmax (proc.grMeter.exchange (0.0f), grDisplayDb - 1.0f, 0.0f);

    const int style = (int) proc.apvts.getRawParameterValue ("style")->load();
    if (style != lastStyle && style >= 0 && style < kNumStyles)
    {
        lastStyle = style;
        styleDescription.setText (juce::String (kStyles[style].name) + ": " + kStyles[style].description,
                                  juce::dontSendNotification);
    }

    orbitPhase += 0.035f;
    if (orbitPhase > juce::MathConstants<float>::twoPi) orbitPhase -= juce::MathConstants<float>::twoPi;

    display.repaint();
    repaint (meterArea);
    repaint (logoArea);
}

void MasterLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
    else
        g.fillAll (Palette::space);

    drawLogo (g);
    drawMeters (g);
}

void MasterLimiterAudioProcessorEditor::drawMeters (juce::Graphics& g)
{
    auto area = meterArea.withTrimmedBottom (38).toFloat();

    auto inner = area.reduced (14.0f);
    auto lufsArea = inner.removeFromBottom (88.0f);
    auto labelRow = inner.removeFromTop (16.0f);
    auto valueRow = inner.removeFromBottom (20.0f);

    const float barW = 16.0f, gap = 7.0f;
    const float x0 = inner.getX() + 4.0f;
    auto barRect = [&] (int i) { return juce::Rectangle<float> (x0 + (float) i * (barW + gap), inner.getY(), barW, inner.getHeight()); };

    g.setFont (spacedFont (11.0f, false, 0.2f));
    g.setColour (Palette::textDim);
    g.drawText ("L",  barRect (0).withY (labelRow.getY()).withHeight (16.0f), juce::Justification::centred);
    g.drawText ("R",  barRect (1).withY (labelRow.getY()).withHeight (16.0f), juce::Justification::centred);
    g.drawText ("GR", barRect (3).withY (labelRow.getY()).withHeight (16.0f).expanded (6.0f, 0.0f), juce::Justification::centred);

    // Segmented LED bars
    constexpr int numSeg = 36;
    auto drawSegments = [&] (juce::Rectangle<float> r, float frac, bool fromTop, juce::Colour low, juce::Colour high)
    {
        const float segH = r.getHeight() / (float) numSeg;
        const int lit = juce::roundToInt (frac * numSeg);
        for (int s = 0; s < numSeg; ++s)
        {
            const float t = (float) s / (float) (numSeg - 1);
            const float sy = fromTop ? r.getY() + (float) s * segH : r.getBottom() - (float) (s + 1) * segH;
            const auto seg = juce::Rectangle<float> (r.getX(), sy + 1.0f, r.getWidth(), segH - 2.0f);
            if (s < lit)
            {
                const auto col = low.interpolatedWith (high, t);
                g.setColour (col.withAlpha (0.25f));
                g.fillRoundedRectangle (seg.expanded (2.5f, 1.0f), 2.0f);
                g.setColour (col);
                g.fillRoundedRectangle (seg, 1.5f);
            }
            else
            {
                g.setColour (Palette::glassEdge.withAlpha (0.35f));
                g.fillRoundedRectangle (seg, 1.5f);
            }
        }
    };

    for (int c = 0; c < 2; ++c)
    {
        const float frac = juce::jmap (juce::jlimit (-48.0f, 0.0f, meterDb[c]), -48.0f, 0.0f, 0.0f, 1.0f);
        drawSegments (barRect (c), frac, false, Palette::nebulaB.brighter (0.4f), Palette::cyan.interpolatedWith (juce::Colours::white, 0.3f));
    }
    drawSegments (barRect (3), juce::jlimit (0.0f, 1.0f, grDisplayDb / 24.0f), true, Palette::magenta, Palette::reduction);

    // Scale
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (Palette::textDim);
    const float scaleX = barRect (4).getX();
    for (int db = 0; db >= -48; db -= 6)
    {
        const float yy = juce::jmap ((float) db, 0.0f, -48.0f, inner.getY(), inner.getBottom());
        g.drawText (juce::String (db), juce::Rectangle<float> (scaleX, yy - 6.0f, 34.0f, 12.0f), juce::Justification::centredLeft);
    }

    // Numeric readouts
    const float peak = juce::jmax (meterDb[0], meterDb[1]);
    g.setFont (spacedFont (13.0f, true, 0.05f));
    g.setColour (Palette::cyan);
    g.drawText (peak <= -99.0f ? juce::String ("-inf") : juce::String (peak, 1),
                juce::Rectangle<float> (x0 - 4.0f, valueRow.getY(), barW * 2.0f + gap + 8.0f, 20.0f), juce::Justification::centred);
    g.setColour (Palette::reduction);
    g.drawText (juce::String (-grDisplayDb, 1),
                juce::Rectangle<float> (barRect (3).getX() - 10.0f, valueRow.getY(), barW + 20.0f, 20.0f), juce::Justification::centred);

    // Loudness readouts
    auto lufsText = [] (float v) { return v <= -99.0f ? juce::String ("--") : juce::String (v, 1); };
    const std::pair<const char*, float> rows[] = {
        { "Momentary",  proc.loudness.momentary.load() },
        { "Short-term", proc.loudness.shortTerm.load() },
        { "Integrated", proc.loudness.integrated.load() },
    };

    g.setGradientFill (juce::ColourGradient (Palette::cyan.withAlpha (0.5f), lufsArea.getX(), 0.0f,
                                             Palette::magenta.withAlpha (0.5f), lufsArea.getRight(), 0.0f, false));
    g.fillRect (lufsArea.getX(), lufsArea.getY() + 2.0f, lufsArea.getWidth(), 1.0f);

    auto rowArea = lufsArea.withTrimmedTop (8.0f);
    for (size_t i = 0; i < 3; ++i)
    {
        auto line = rowArea.removeFromTop (26.0f);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (Palette::textDim);
        g.drawText (rows[i].first, line, juce::Justification::centredLeft);
        g.setFont (spacedFont (15.0f, true, 0.04f));
        g.setColour (i == 2 ? Palette::magenta.interpolatedWith (juce::Colours::white, 0.25f) : Palette::text);
        g.drawText (lufsText (rows[i].second) + " LUFS", line, juce::Justification::centredRight);
    }
}

void MasterLimiterAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (14, 12);

    // Header
    headerArea = r.removeFromTop (60);
    auto header = headerArea.reduced (14, 15);
    logoArea = header.removeFromLeft (190).expanded (0, 10);
    header.removeFromLeft (10);
    styleLabel.setBounds (header.removeFromLeft (44));
    header.removeFromLeft (6);
    styleBox.setBounds (header.removeFromLeft (170));
    header.removeFromLeft (14);
    osLabel.setBounds (header.removeFromLeft (90));
    header.removeFromLeft (6);
    osBox.setBounds (header.removeFromLeft (80));
    header.removeFromLeft (16);
    truePeakButton.setBounds (header.removeFromLeft (104));
    header.removeFromLeft (6);
    ditherLabel.setBounds (header.removeFromLeft (50));
    header.removeFromLeft (6);
    ditherBox.setBounds (header.removeFromLeft (90));

    r.removeFromTop (12);

    // Knob panel
    knobPanel = r.removeFromBottom (168);
    auto bottom = knobPanel.reduced (18, 12);
    auto toggles = bottom.removeFromRight (140);
    unityButton.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).withSizeKeepingCentre (130, 28));
    deltaButton.setBounds (toggles.withSizeKeepingCentre (130, 28));

    const int knobW = bottom.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto a = bottom.removeFromLeft (knobW);
        k.label.setBounds (a.removeFromTop (18));
        k.slider.setBounds (a);
    }

    r.removeFromBottom (12);

    // Display + meters
    meterArea = r.removeFromRight (206);
    r.removeFromRight (14);
    styleDescription.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (4);
    display.setBounds (r);

    resetButton.setBounds (meterArea.getX() + 20, meterArea.getBottom() - 30, meterArea.getWidth() - 40, 28);

    renderBackground();
}
