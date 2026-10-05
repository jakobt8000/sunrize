#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour ink        { 0xff111111 };
    const juce::Colour needleInk  { 0xff2a2a2a };
    const juce::Colour labelInk   { 0xff333333 };
    const juce::Colour softInk    { 0xff9a9a98 };
    const juce::Colour panel      { 0xfff7f7f7 };   // white with a 3 % black overlay
    const juce::Colour bigRing    { 0xffa3a3a0 };

    const char* knobIds[]    = { "tone", "weather", "width", "birds", "bloom",
                                 "afterglow", "drift", "dust", "light", "drops" };
    const char* knobLabels[] = { "TONE", "WEATHER", "WIDTH", "BIRDS", "BLOOM",
                                 "AFTERGLOW", "DRIFT", "DUST", "LIGHT", "DROPS" };

    // layout (matches the final design at 1620 x 280, 30 px padding everywhere)
    const juce::Rectangle<int> leftPanel  { 0, 0, 280, 280 };
    const juce::Rectangle<int> bigKnob    { 30, 30, 220, 220 };
    const juce::Rectangle<int> scopeArea  { 310, 30, 600, 182 };
    const juce::Rectangle<int> navArea    { 310, 218, 600, 32 };
    const int gridX = 940, gridW = 505;
    const juce::Rectangle<int> rightPanel { 1475, 0, 145, 280 };
}

//==============================================================================
void SunrizeLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const bool big = s.getProperties()["big"];
    const bool onPanel = s.getProperties()["panel"];
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto circle = bounds.withSizeKeepingCentre (size, size);
    const auto centre = circle.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (big)
    {
        g.setColour (panel);
        g.fillEllipse (circle);
        g.setColour (bigRing);
        g.drawEllipse (circle.reduced (3.0f), 6.0f);
        const float inner = size - 12.0f;
        const float len = inner * 0.284f, top = inner * 0.15f;
        juce::Path needle;
        needle.addRoundedRectangle (-4.0f, -inner * 0.5f + top, 8.0f, len, 4.0f);
        g.setColour (needleInk);
        g.fillPath (needle, juce::AffineTransform::rotation (angle).translated (centre));
        return;
    }

    g.setColour (onPanel ? panel : juce::Colours::white);
    g.fillEllipse (circle);
    g.setColour (ink);
    g.drawEllipse (circle, 1.0f);
    const float len = size * 0.35f, top = size * 0.12f;
    juce::Path needle;
    needle.addRectangle (-1.5f, -size * 0.5f + top, 3.0f, len);
    g.fillPath (needle, juce::AffineTransform::rotation (angle).translated (centre));
}

void NavArrow::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    juce::Path p;
    if (next) { p.startNewSubPath (c.x - 4, c.y - 10); p.lineTo (c.x + 6, c.y); p.lineTo (c.x - 4, c.y + 10); }
    else      { p.startNewSubPath (c.x + 4, c.y - 10); p.lineTo (c.x - 6, c.y); p.lineTo (c.x + 4, c.y + 10); }
    g.setColour (over ? labelInk : ink);
    g.strokePath (p, juce::PathStrokeType (3.0f));
}

void InfoButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto c = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (panel);
    g.fillEllipse (c);
    g.setColour (ink);
    g.drawEllipse (c, 1.0f);
    g.setColour (over ? labelInk : ink);
    g.setFont (font);
    g.drawText ("i", getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
void CloseX::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (12.0f, 12.0f);
    juce::Path p;
    p.startNewSubPath (r.getTopLeft()); p.lineTo (r.getBottomRight());
    p.startNewSubPath (r.getTopRight()); p.lineTo (r.getBottomLeft());
    g.setColour (over ? labelInk : ink);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

InfoContent::InfoContent (juce::Typeface::Ptr t) : mono (t)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    addAndMakeVisible (closeBtn);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    setSize (juce::roundToInt (702 * scale), 280);
}

void InfoContent::resized()
{
    // The 12 px cross sits 16 design-px from the top and right edge, like the text.
    const int s = juce::roundToInt (36 * scale * 0.6f);
    const float edge = pad * scale + 6.0f;   // centre of the cross
    closeBtn.setBounds (juce::roundToInt (getWidth() - edge - s * 0.5f),
                        juce::roundToInt (edge - s * 0.5f), s, s);
}

void InfoContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
    g.addTransform (juce::AffineTransform::scale (scale));   // design is drawn at 702 x 165
    if (logo.isValid())
    {
        const float lw = 420.0f, lh = lw * (float) logo.getHeight() / (float) logo.getWidth();
        g.drawImage (logo, juce::Rectangle<float> (16.0f, (165.0f - lh) * 0.5f, lw, lh));
    }
    const juce::Rectangle<int> right (452, 0, 250, 165);
    g.setColour (panel);
    g.fillRect (right);

    const char* rows[][2] = { { "PART OF", "REZONANZA" }, { "TYPE", "PAD SYNTH" }, { "SOUNDS", "10" },
                              { "KNOBS", "12" }, { "OSCILLATORS", "28" }, { "TUNING", "A = 440 HZ" },
                              { "VERSION", "0.1" }, { "BIRDS", "INCLUDED" }, { "WEATHER", "VARIABLE" } };
    // Same 16 px padding as the logo on every side, measured to the visible
    // letters: top of the capitals in the first row, baseline of the last row.
    const float fontH = 11.0f;
    g.setFont (juce::Font (juce::FontOptions (mono).withHeight (fontH).withKerningFactor (0.09f)));
    const float capH = fontH / 1.3f * 0.698f;          // IBM Plex Mono cap height
    const auto area = right.toFloat().reduced (pad);
    const int count = 9;
    const float first = area.getY() + capH, last = area.getBottom();
    const float step = (last - first) / (float) (count - 1);
    for (int i = 0; i < count; ++i)
    {
        const int base = juce::roundToInt (first + i * step);
        g.setColour (softInk);
        g.drawSingleLineText (rows[i][0], juce::roundToInt (area.getX()), base);
        g.setColour (labelInk);
        g.drawSingleLineText (rows[i][1], juce::roundToInt (area.getX() + 112.0f), base);
    }
}

//==============================================================================
SunrizeAudioProcessorEditor::SunrizeAudioProcessorEditor (SunrizeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    monoFace = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf,
                                                        BinaryData::IBMPlexMonoRegular_ttfSize);
    infoBtn.font = mono (22.0f, 0.0f);

    auto setupKnob = [this] (juce::Slider& s, const char* id, bool big, bool onPanel)
    {
        s.setLookAndFeel (&look);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * -0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        s.setMouseDragSensitivity (big ? 170 : 160);
        s.setVelocityBasedMode (false);
        s.getProperties().set ("big", big);
        s.getProperties().set ("panel", onPanel);
        s.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->getDefaultValue());
        s.setTitle (id);
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
    };

    setupKnob (sun, "sun", true, true);
    for (size_t i = 0; i < knobs.size(); ++i) setupKnob (knobs[i], knobIds[i], false, false);
    setupKnob (output, "output", false, true);

    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    addAndMakeVisible (infoBtn);
    prevBtn.onClick = [this] { stepSound (-1); };
    nextBtn.onClick = [this] { stepSound (1); };
    infoBtn.onClick = [this]
    {
        if (infoWindow != nullptr && infoWindow->isOnDesktop())
        {
            infoWindow->removeFromDesktop();
            return;
        }
        if (infoWindow == nullptr)
        {
            infoWindow = std::make_unique<InfoContent> (monoFace);
            infoWindow->onClose = [this] { if (infoWindow != nullptr) infoWindow->removeFromDesktop(); };
        }
        // pop up just under the synth, centred on it
        const auto screen = getScreenBounds();
        infoWindow->setTopLeftPosition (screen.getCentreX() - infoWindow->getWidth() / 2, screen.getBottom());
        infoWindow->setAlwaysOnTop (true);
        infoWindow->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
        infoWindow->setVisible (true);
        infoWindow->toFront (false);
    };

    setSize (W, H);
    startTimerHz (30);
}

SunrizeAudioProcessorEditor::~SunrizeAudioProcessorEditor()
{
    stopTimer();
    if (infoWindow != nullptr && infoWindow->isOnDesktop()) infoWindow->removeFromDesktop();
    infoWindow = nullptr;
    sun.setLookAndFeel (nullptr);
    output.setLookAndFeel (nullptr);
    for (auto& k : knobs) k.setLookAndFeel (nullptr);
}

juce::Font SunrizeAudioProcessorEditor::mono (float size, float kerning) const
{
    return juce::Font (juce::FontOptions (monoFace).withHeight (size).withKerningFactor (kerning));
}

void SunrizeAudioProcessorEditor::stepSound (int delta)
{
    auto* param = proc.apvts.getParameter ("sound");
    const int count = (int) SunrizeAudioProcessor::sounds().size();
    const int current = (int) proc.apvts.getRawParameterValue ("sound")->load();
    const int next = ((current + delta) % count + count) % count;
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
    param->endChangeGesture();
    repaint (navArea);
}

void SunrizeAudioProcessorEditor::resized()
{
    sun.setBounds (bigKnob);

    const int step = (gridW - knobSize) / 4;
    for (int i = 0; i < 10; ++i)
    {
        const int col = i % 5, row = i / 5;
        knobs[(size_t) i].setBounds (gridX + col * step, 30 + row * 115, knobSize, knobSize);
    }
    output.setBounds (rightPanel.getX() + 30, 30, knobSize, knobSize);
    infoBtn.setBounds (rightPanel.getX() + 30, 145, knobSize, knobSize);

    prevBtn.setBounds (navArea.getX() + 30, navArea.getY(), 44, navArea.getHeight());
    nextBtn.setBounds (navArea.getRight() - 74, navArea.getY(), 44, navArea.getHeight());
}

void SunrizeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
    g.setColour (panel);
    g.fillRect (leftPanel);
    g.fillRect (rightPanel);

    // knob labels
    g.setFont (mono (11.0f, 0.09f));
    g.setColour (labelInk);
    for (int i = 0; i < 10; ++i)
    {
        auto kb = knobs[(size_t) i].getBounds();
        g.drawText (knobLabels[i], kb.getX() - 20, kb.getBottom() + 6, kb.getWidth() + 40, 14, juce::Justification::centred);
    }
    auto ob = output.getBounds();
    g.drawText ("OUTPUT", ob.getX() - 20, ob.getBottom() + 6, ob.getWidth() + 40, 14, juce::Justification::centred);

    // sound name
    const auto& snds = SunrizeAudioProcessor::sounds();
    const int idx = juce::jlimit (0, (int) snds.size() - 1, (int) proc.apvts.getRawParameterValue ("sound")->load());
    g.setFont (mono (19.0f, 0.21f));
    g.setColour (ink);
    g.drawText (juce::String (idx + 1).paddedLeft ('0', 2) + "  " + snds[(size_t) idx].name,
                navArea, juce::Justification::centred);

    // scope: one thin pixelated line
    const float mid = (float) scopeArea.getCentreY();
    const float amp = scopeArea.getHeight() * 0.5f - 4.0f;
    const int cols = (int) trace.size();
    const float colW = scopeArea.getWidth() / (float) cols;
    juce::Path line;
    for (int c = 0; c < cols; ++c)
    {
        const float v = trace[(size_t) c];
        const float a = juce::jmin (1.0f, std::pow (std::abs (v) / 0.22f, 0.85f)) * (v < 0 ? -1.0f : 1.0f);
        float yy = std::round ((mid - a * amp) / 4.0f) * 4.0f;
        const float x0 = scopeArea.getX() + c * colW, x1 = x0 + colW;
        if (c == 0) line.startNewSubPath (x0, yy);
        else line.lineTo (x0, yy);
        line.lineTo (x1, yy);
    }
    g.setColour (ink);
    g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

void SunrizeAudioProcessorEditor::timerCallback()
{
    proc.copyScope (scopeData.data(), (int) scopeData.size());
    int s0 = 0;
    for (int i = 1; i < 1024; ++i)
        if (scopeData[(size_t) i - 1] < 0.0f && scopeData[(size_t) i] >= 0.0f) { s0 = i; break; }
    const float span = 900.0f, stepS = span / (float) trace.size();
    for (size_t c = 0; c < trace.size(); ++c)
    {
        const auto idx = (size_t) juce::jmin (2047, s0 + (int) (c * stepS));
        trace[c] = trace[c] * 0.3f + scopeData[idx] * 0.7f;
    }
    repaint (scopeArea.expanded (4));

    const int idx = (int) proc.apvts.getRawParameterValue ("sound")->load();
    if (idx != soundIndex) { soundIndex = idx; repaint (navArea); }
}
