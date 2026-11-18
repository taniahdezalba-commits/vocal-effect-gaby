#include "PluginEditor.h"

using namespace gabyui;

// =============================================================================
//  LookAndFeel
// =============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto sq = bounds.withSizeKeepingCentre (size, size);
    const auto c = sq.getCentre();
    const float r = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Pista del arco
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2a36));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arco de valor con brillo morado
    juce::Path value;
    value.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, angle, true);
    g.setColour (col::purple.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (col::violet, sq.getBottomLeft(), col::purple.brighter (0.3f), sq.getTopRight(), false));
    g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cuerpo de la perilla
    const float kr = r * 0.70f;
    auto knob = juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (knob.translated (0.0f, 3.0f).expanded (2.0f));

    juce::ColourGradient body (juce::Colour (0xff4a4a56), knob.getX(), knob.getY(),
                               juce::Colour (0xff0e0e14), knob.getRight(), knob.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (knob);

    auto inner = knob.reduced (kr * 0.12f);
    juce::ColourGradient face (juce::Colour (0xff2c2c36), inner.getCentreX(), inner.getY(),
                               juce::Colour (0xff15151c), inner.getCentreX(), inner.getBottom(), false);
    g.setGradientFill (face);
    g.fillEllipse (inner);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (knob, 1.0f);

    // Indicador
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr * 0.85f, 3.0f, kr * 0.42f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillPath (pointer);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float corner = 8.0f;

    if (b.getToggleState())
    {
        g.setColour (col::purple.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
        g.setGradientFill (juce::ColourGradient (col::violet.withAlpha (0.85f), r.getX(), r.getY(),
                                                 juce::Colour (0xff3b1d6e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (col::purple.brighter (0.4f));
        g.drawRoundedRectangle (r, corner, 1.4f);
    }
    else
    {
        g.setColour (highlighted ? col::panelHi.brighter (0.1f) : col::panelHi);
        g.fillRoundedRectangle (r, corner);
        g.setColour (col::border);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }

    if (down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r, corner);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : col::text.withAlpha (0.8f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, col::textDim);
    l->setFont (juce::FontOptions (12.0f));
    return l;
}

// =============================================================================
//  Medidor
// =============================================================================
void LevelMeter::setLevel (float linearPeak)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
    levelDb = juce::jmax (db, levelDb - 1.8f); // caída suave
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (0.0f, 7.0f);
    auto leds = area.removeFromLeft (22.0f).reduced (2.0f, 0.0f);

    constexpr int numSeg = 16;
    const float segH = leds.getHeight() / numSeg;
    const float minDb = -48.0f;

    for (int i = 0; i < numSeg; ++i)
    {
        const float frac = (float) (i + 1) / numSeg;          // 0 = abajo, 1 = arriba
        const float segDb = minDb * (1.0f - frac);
        auto seg = juce::Rectangle<float> (leds.getX(), leds.getBottom() - (i + 1) * segH, leds.getWidth(), segH).reduced (0.0f, 1.2f);

        juce::Colour c = frac > 0.88f ? juce::Colour (0xffef4444)
                       : frac > 0.72f ? juce::Colour (0xfff59e0b)
                       : frac > 0.55f ? juce::Colour (0xffeab308)
                                      : juce::Colour (0xff22c55e);

        const bool lit = levelDb >= segDb - 48.0f / numSeg;
        if (lit)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (seg.expanded (1.5f, 0.5f), 2.0f);
        }
        g.setColour (lit ? c : c.withAlpha (0.12f));
        g.fillRoundedRectangle (seg, 1.5f);
    }

    // Escala
    g.setColour (col::textDim);
    g.setFont (juce::FontOptions (11.0f));
    const int marks[] = { 0, -6, -12, -18, -24, -36, -48 };
    for (int m : marks)
    {
        const float yy = leds.getY() + leds.getHeight() * ((float) m / minDb);
        g.drawText (juce::String (m == 0 ? "-0" : juce::String (m)),
                    juce::Rectangle<float> (area.getX() + 4.0f, yy - 6.0f, area.getWidth() - 4.0f, 12.0f),
                    juce::Justification::centredLeft);
    }
}

// =============================================================================

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float,
                                    float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto track = r.withSizeKeepingCentre (8.0f, r.getHeight() - 8.0f);
    g.setColour (juce::Colour (0xff0b0b12));
    g.fillRoundedRectangle (track, 4.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (track, 4.0f, 1.0f);

    // marca del 100 %
    const float midY = track.getBottom() - track.getHeight() * (float) s.valueToProportionOfLength (100.0);
    g.setColour (col::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) midY, track.getX() - 8.0f, track.getRight() + 8.0f);

    auto fill = track.withTop (pos);
    g.setColour (col::purple.withAlpha (0.25f));
    g.fillRoundedRectangle (fill.expanded (3.0f, 0.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient (col::purple.brighter (0.3f), fill.getX(), fill.getY(),
                                             col::violet, fill.getX(), fill.getBottom(), false));
    g.fillRoundedRectangle (fill, 4.0f);

    auto thumb = juce::Rectangle<float> (26.0f, 12.0f).withCentre ({ track.getCentreX(), pos });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a56), thumb.getX(), thumb.getY(),
                                             juce::Colour (0xff15151c), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawHorizontalLine ((int) thumb.getCentreY(), thumb.getX() + 6.0f, thumb.getRight() - 6.0f);
}

// =============================================================================
//  Espectro
// =============================================================================
void SpectrumDisplay::update (const VocalEffectGabyProcessor& p)
{
    for (int b = 0; b < gabyvx::kDisplayBands; ++b)
    {
        auto smooth = [] (float& v, float target) { v = target > v ? target : v + (target - v) * 0.25f; };
        smooth (in[(size_t) b], p.specIn[(size_t) b].load());
        smooth (out[(size_t) b], p.specOut[(size_t) b].load());
        noise[(size_t) b] += (p.specNoise[(size_t) b].load() - noise[(size_t) b]) * 0.2f;
    }
    voice = p.voiceActive.load();
    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0b16), r.getX(), r.getY(),
                                             juce::Colour (0xff06060c), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto plot = r.reduced (14.0f, 12.0f).withTrimmedBottom (14.0f);
    const float minDb = -100.0f, maxDb = -10.0f;
    auto xOfFreq = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / 30.0) / std::log (600.0)); };
    auto yOf = [&] (float db) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };

    // cuadrícula
    g.setFont (juce::FontOptions (10.0f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float xx = xOfFreq (f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) xx, plot.getY(), plot.getBottom());
        g.setColour (col::textDim);
        g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (xx - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    for (float db = -90.0f; db <= -20.0f; db += 10.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
    }

    auto makePath = [&] (const std::array<float, gabyvx::kDisplayBands>& v, bool closed)
    {
        juce::Path p;
        for (int b = 0; b < gabyvx::kDisplayBands; ++b)
        {
            const float xx = plot.getX() + plot.getWidth() * (b + 0.5f) / gabyvx::kDisplayBands;
            const float yy = yOf (v[(size_t) b]);
            if (b == 0) p.startNewSubPath (closed ? plot.getX() : xx, closed ? plot.getBottom() : yy);
            if (b == 0 && closed) p.lineTo (xx, yy);
            else if (b > 0) p.lineTo (xx, yy);
        }
        if (closed)
        {
            p.lineTo (plot.getRight(), plot.getBottom());
            p.closeSubPath();
        }
        return p;
    };

    // entrada (cian)
    auto inFill = makePath (in, true);
    g.setGradientFill (juce::ColourGradient (col::cyan.withAlpha (0.25f), 0, plot.getY(), col::cyan.withAlpha (0.02f), 0, plot.getBottom(), false));
    g.fillPath (inFill);
    g.setColour (col::cyan.withAlpha (0.7f));
    g.strokePath (makePath (in, false), juce::PathStrokeType (1.2f));

    // salida (morado con brillo)
    auto outFill = makePath (out, true);
    g.setGradientFill (juce::ColourGradient (col::purple.withAlpha (0.45f), 0, plot.getY(), col::violet.withAlpha (0.05f), 0, plot.getBottom(), false));
    g.fillPath (outFill);
    auto outLine = makePath (out, false);
    g.setColour (col::purple.withAlpha (0.3f));
    g.strokePath (outLine, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::purple.brighter (0.4f));
    g.strokePath (outLine, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ruido estimado (línea punteada)
    juce::Path dashed;
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, makePath (noise, false), dashes, 2);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillPath (dashed);

    // leyenda e indicador de voz
    auto legend = r.reduced (16.0f, 10.0f).removeFromTop (14.0f);
    g.setFont (juce::FontOptions (11.0f));
    auto item = [&] (juce::Colour c, const juce::String& t, float w)
    {
        g.setColour (c);
        g.fillEllipse (legend.getX(), legend.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (col::textDim);
        g.drawText (t, legend.withTrimmedLeft (10.0f).withWidth (w), juce::Justification::centredLeft);
        legend.removeFromLeft (w + 16.0f);
    };
    item (col::cyan, "Entrada", 60.0f);
    item (col::purple, "Voz limpia", 70.0f);
    item (juce::Colours::white.withAlpha (0.5f), "Ruido detectado", 100.0f);

    auto vr = r.reduced (16.0f, 9.0f).removeFromTop (16.0f).removeFromRight (70.0f);
    g.setColour (voice ? col::green : col::textDim.withAlpha (0.4f));
    g.fillEllipse (vr.getX(), vr.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (voice ? col::text : col::textDim);
    g.drawText ("VOZ", vr.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
}

// =============================================================================
//  Botón de encendido
// =============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const bool on = getToggleState();
    const auto c = on ? col::purple.brighter (0.3f) : col::textDim.withAlpha (highlighted ? 0.9f : 0.6f);

    if (on)
    {
        g.setColour (col::purple.withAlpha (0.18f));
        g.fillEllipse (r);
    }

    auto icon = r.reduced (r.getWidth() * 0.22f);
    juce::Path arc;
    arc.addCentredArc (icon.getCentreX(), icon.getCentreY(), icon.getWidth() * 0.5f, icon.getHeight() * 0.5f,
                       0.0f, juce::MathConstants<float>::pi * 0.22f, juce::MathConstants<float>::pi * 1.78f, true);
    g.setColour (c);
    g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (icon.getCentreX(), icon.getY() - 1.0f, icon.getCentreX(), icon.getCentreY(), 2.2f);
}

// =============================================================================
//  Editor
// =============================================================================
VocalEffectGabyEditor::VocalEffectGabyEditor (VocalEffectGabyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (powerButton);

    setupKnob (clarityKnob, clarityLabel, "CLARIDAD", " %");
    setupKnob (ambienceKnob, ambienceLabel, "AMBIENTE", " %");
    setupKnob (gateKnob, gateLabel, "COMPUERTA", " dB");
    setupKnob (gainKnob, gainLabel, "GANANCIA", " dB");
    setupKnob (volumeKnob, volumeLabel, "VOLUMEN", " dB");
    clarityLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));

    clarityAtt  = std::make_unique<SliderAtt> (proc.apvts, "clarity", clarityKnob);
    ambienceAtt = std::make_unique<SliderAtt> (proc.apvts, "ambience", ambienceKnob);
    gateAtt     = std::make_unique<SliderAtt> (proc.apvts, "gate", gateKnob);
    gainAtt     = std::make_unique<SliderAtt> (proc.apvts, "inGain", gainKnob);
    volumeAtt   = std::make_unique<SliderAtt> (proc.apvts, "outGain", volumeKnob);
    powerAtt    = std::make_unique<ButtonAtt> (proc.apvts, "power", powerButton);

    const char* bandNames[] = { "GRAVES", "MEDIOS", "PRESENCIA", "AIRE" };
    for (int b = 0; b < 4; ++b)
    {
        auto& s = bandSliders[b];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxTextColourId, col::textDim);
        s.setTextValueSuffix (" %");
        s.setNumDecimalPlacesToDisplay (0);
        addAndMakeVisible (s);
        bandAtt[b] = std::make_unique<SliderAtt> (proc.apvts, "band" + juce::String (b), s);

        bandLabels[b].setText (bandNames[b], juce::dontSendNotification);
        bandLabels[b].setJustificationType (juce::Justification::centred);
        bandLabels[b].setFont (juce::FontOptions (11.0f, juce::Font::bold));
        bandLabels[b].setColour (juce::Label::textColourId, col::text);
        addAndMakeVisible (bandLabels[b]);
    }

    voiceButton.setButtonText ("VOZ");
    ambienceButton.setButtonText ("AMBIENTE");
    voiceButton.onClick = [this] { setMode (0); };
    ambienceButton.onClick = [this] { setMode (1); };
    addAndMakeVisible (voiceButton);
    addAndMakeVisible (ambienceButton);

    setSize (940, 600);
    timerCallback();
    startTimerHz (30);
}

VocalEffectGabyEditor::~VocalEffectGabyEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VocalEffectGabyEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId, col::textDim);
    s.setColour (juce::Slider::textBoxHighlightColourId, col::violet);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, col::text);
    addAndMakeVisible (l);
}

void VocalEffectGabyEditor::setMode (int m)
{
    if (auto* param = proc.apvts.getParameter ("mode"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) m));
        param->endChangeGesture();
    }
    timerCallback();
}

void VocalEffectGabyEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    auto r = area.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1b25), r.getX(), r.getY(),
                                             juce::Colour (0xff121219), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    if (title.isNotEmpty())
    {
        auto t = area.reduced (14, 10).removeFromTop (18);
        g.setColour (col::purple);
        g.fillRoundedRectangle ((float) t.getX(), (float) t.getCentreY() - 4.0f, 16.0f, 8.0f, 4.0f);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, t.withTrimmedLeft (24), juce::Justification::centredLeft);
    }
}

void VocalEffectGabyEditor::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::bgTop, 0.0f, 0.0f, col::bgBottom, 0.0f, full.getBottom(), false));
    g.fillAll();

    auto frame = full.reduced (3.0f);
    g.setColour (col::purple.withAlpha (0.15f));
    g.drawRoundedRectangle (frame, 16.0f, 5.0f);
    g.setColour (col::purple.withAlpha (0.55f));
    g.drawRoundedRectangle (frame, 16.0f, 1.2f);

    // Encabezado
    drawPanel (g, headerArea, {});
    {
        auto h = headerArea.reduced (16, 0).toFloat();
        const float heights[] = { 0.35f, 0.65f, 1.0f, 0.65f, 0.35f };
        float bx = h.getX();
        for (float hh : heights)
        {
            const float bh = 26.0f * hh;
            juce::Rectangle<float> bar (bx, h.getCentreY() - bh * 0.5f, 3.5f, bh);
            g.setGradientFill (juce::ColourGradient (col::cyan, bar.getX(), bar.getY(), col::purple, bar.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 1.75f);
            bx += 6.5f;
        }
        g.setColour (col::text);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText ("Vocal Effect Gaby", juce::Rectangle<float> (bx + 8.0f, h.getY(), 240.0f, h.getHeight()), juce::Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("G A B Y   R E C O R D S", juce::Rectangle<float> (bx + 200.0f, h.getY() + 3.0f, 160.0f, h.getHeight()), juce::Justification::centredLeft);

        auto box = voiceButton.getBounds().getUnion (ambienceButton.getBounds()).toFloat().expanded (6.0f, 4.0f);
        g.setColour (juce::Colour (0xff0b0b12));
        g.fillRoundedRectangle (box, 9.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (box, 9.0f, 1.0f);
    }

    drawPanel (g, inPanel, "ENTRADA");
    drawPanel (g, outPanel, "SALIDA");
    drawPanel (g, ambPanel, "AMBIENTE");
    drawPanel (g, redPanel, juce::String (juce::CharPointer_UTF8 ("REDUCCI\xc3\x93N")));
    drawPanel (g, clarityPanel, "LIMPIEZA");
    drawPanel (g, bandPanel, "MULTIBANDA");

    // Ayudas
    g.setColour (col::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("Ambiente = reflexiones\nCompuerta = pausas")),
                      ambPanel.reduced (10, 12).removeFromBottom (32), juce::Justification::centred, 2);
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("Gira hasta que el ruido desaparezca")),
                      clarityPanel.reduced (10, 10).removeFromBottom (20), juce::Justification::centred, 1);
    g.drawFittedText ("<250 Hz      250-2k      2k-6k      >6k",
                      bandPanel.reduced (14, 10).removeFromBottom (18), juce::Justification::centred, 1);

    // Medidor de reducción
    {
        auto r = redPanel.reduced (16, 0).withTrimmedTop (44).withHeight (60);
        g.setColour (col::text);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText (juce::String (shownReduction, 1) + " dB", r, juce::Justification::centred);

        auto bar = redPanel.reduced (20, 0).withTrimmedTop (116).withHeight (10).toFloat();
        g.setColour (juce::Colour (0xff0b0b12));
        g.fillRoundedRectangle (bar, 5.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, -shownReduction / 40.0f);
        auto fill = bar.withWidth (bar.getWidth() * frac);
        g.setGradientFill (juce::ColourGradient (col::violet, fill.getX(), 0, col::purple.brighter (0.3f), fill.getRight(), 0, false));
        g.fillRoundedRectangle (fill, 5.0f);

        auto led = redPanel.reduced (20, 0).withTrimmedTop (144).withHeight (20).toFloat();
        g.setColour (shownVoice ? col::green : col::textDim.withAlpha (0.35f));
        g.fillEllipse (led.getX(), led.getCentreY() - 5.0f, 10.0f, 10.0f);
        g.setColour (shownVoice ? col::text : col::textDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (shownVoice ? "VOZ DETECTADA" : "SIN VOZ", led.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    }
}

void VocalEffectGabyEditor::resized()
{
    headerArea   = { 15, 12, 910, 52 };
    inPanel      = { 15, 78, 170, 280 };
    ambPanel     = { 15, 370, 170, 215 };
    outPanel     = { 755, 78, 170, 280 };
    redPanel     = { 755, 370, 170, 215 };
    clarityPanel = { 200, 280, 250, 305 };
    bandPanel    = { 460, 280, 280, 305 };

    voiceButton.setBounds (headerArea.getCentreX() - 60, headerArea.getY() + 11, 110, 30);
    ambienceButton.setBounds (headerArea.getCentreX() + 54, headerArea.getY() + 11, 110, 30);
    powerButton.setBounds (headerArea.getRight() - 50, headerArea.getY() + 8, 36, 36);

    inMeter.setBounds (inPanel.getX() + 52, inPanel.getY() + 34, 80, 148);
    gainLabel.setBounds (inPanel.getX(), inPanel.getY() + 184, inPanel.getWidth(), 16);
    gainKnob.setBounds (inPanel.getX() + 35, inPanel.getY() + 198, 100, 78);

    outMeter.setBounds (outPanel.getX() + 52, outPanel.getY() + 34, 80, 148);
    volumeLabel.setBounds (outPanel.getX(), outPanel.getY() + 184, outPanel.getWidth(), 16);
    volumeKnob.setBounds (outPanel.getX() + 35, outPanel.getY() + 198, 100, 78);

    ambienceLabel.setBounds (ambPanel.getX() + 4, ambPanel.getY() + 40, 81, 16);
    ambienceKnob.setBounds (ambPanel.getX() + 4, ambPanel.getY() + 58, 81, 100);
    gateLabel.setBounds (ambPanel.getX() + 85, ambPanel.getY() + 40, 81, 16);
    gateKnob.setBounds (ambPanel.getX() + 85, ambPanel.getY() + 58, 81, 100);

    spectrum.setBounds (200, 78, 540, 190);

    clarityLabel.setBounds (clarityPanel.getX(), clarityPanel.getY() + 40, clarityPanel.getWidth(), 18);
    clarityKnob.setBounds (clarityPanel.getX() + 35, clarityPanel.getY() + 60, 180, 200);

    const int colW = (bandPanel.getWidth() - 28) / 4;
    for (int b = 0; b < 4; ++b)
    {
        const int x = bandPanel.getX() + 14 + b * colW;
        bandLabels[b].setBounds (x, bandPanel.getY() + 40, colW, 16);
        bandSliders[b].setBounds (x + 4, bandPanel.getY() + 60, colW - 8, 205);
    }
}

void VocalEffectGabyEditor::timerCallback()
{
    inMeter.setLevel (proc.inputPeak.exchange (0.0f));
    outMeter.setLevel (proc.outputPeak.exchange (0.0f));
    spectrum.update (proc);

    shownReduction += (juce::jmin (0.0f, proc.reductionDb.load()) - shownReduction) * 0.2f;
    shownVoice = proc.voiceActive.load();
    repaint (redPanel);

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    voiceButton.setToggleState (mode == 0, juce::dontSendNotification);
    ambienceButton.setToggleState (mode == 1, juce::dontSendNotification);
}
