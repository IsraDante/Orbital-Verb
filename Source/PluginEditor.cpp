#include "PluginEditor.h"

namespace col
{
    const juce::Colour gold (0xff8fd0ff), bg (0xff0b0b0d), track (0xff26262c), text (0xfff2f2f2);
    const juce::Colour pink (0xffff4fd8), cyan (0xff00e5ff), neonBg (0xff0c1536);
    const juce::Colour knobNeon[] = { juce::Colour (0xffff3d81), juce::Colour (0xffffb300), juce::Colour (0xff00e5ff),
                                      juce::Colour (0xffa77bff), juce::Colour (0xff00e676), juce::Colour (0xffff6e40) };
}

void OrbitalLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
    float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    auto ctr = b.getCentre();
    float ang = a0 + pos * (a1 - a0);
    juce::Path tr; tr.addCentredArc (ctr.x, ctr.y, r * 0.84f, r * 0.84f, 0, a0, a1, true);
    g.setColour (neon ? juce::Colour (0xff3d3670) : col::track);
    g.strokePath (tr, juce::PathStrokeType (neon ? 3.f : 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path va; va.addCentredArc (ctr.x, ctr.y, r * 0.84f, r * 0.84f, 0, a0, juce::jmax (ang, a0 + 0.02f), true);
    if (neon) { g.setColour (c.withAlpha (0.35f)); g.strokePath (va, juce::PathStrokeType (9.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (c);
    g.strokePath (va, juce::PathStrokeType (neon ? 4.5f : 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float cr = r * 0.6f;
    if (neon) { g.setColour (juce::Colour (0xff15122f)); g.fillEllipse (ctr.x - cr, ctr.y - cr, cr * 2, cr * 2); }
    g.setColour (neon ? c : col::track);
    g.drawEllipse (ctr.x - cr, ctr.y - cr, cr * 2, cr * 2, neon ? 1.6f : 1.f);
    auto tip = ctr.getPointOnCircumference (cr * 0.8f, ang);
    auto in = ctr.getPointOnCircumference (cr * 0.35f, ang);
    g.setColour (c);
    g.drawLine ({ in, tip }, juce::jmax (3.f, r * 0.12f));
}

static const char* names[] = { "PLANET", "SIZE", "PRE-DELAY", "TONE", "DECAY", "MIX", "OUTPUT" };
static const char* ids[]   = { "pl", "sz", "pd", "tone", "dc", "mix", "out" };

OrbitalReverbEditor::OrbitalReverbEditor (OrbitalReverbProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    for (int i = 0; i < 7; ++i)
    {
        auto& k = knobs[i];
        k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.s.setColour (juce::Slider::textBoxTextColourId, col::text);
        k.s.setDoubleClickReturnValue (true, proc.apvts.getParameter (ids[i])->convertFrom0to1 (proc.apvts.getParameter (ids[i])->getDefaultValue()));
        addAndMakeVisible (k.s);
        k.name.setText (names[i], juce::dontSendNotification);
        k.name.setJustificationType (juce::Justification::centred);
        k.name.setFont (juce::FontOptions (10.f));
        addAndMakeVisible (k.name);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
    }
    bypass.setClickingTogglesState (true);
    bypAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "byp", bypass);
    addAndMakeVisible (bypass);
    style.onClick = [this] {
        auto* sk = proc.apvts.getParameter ("sk");
        sk->setValueNotifyingHost (sk->getValue() > 0.5f ? 0.f : 1.f);
    };
    addAndMakeVisible (style);
    setSize (420, 640);
    startTimerHz (30);
}

OrbitalReverbEditor::~OrbitalReverbEditor() { setLookAndFeel (nullptr); }

void OrbitalReverbEditor::timerCallback()
{
    bool neon = proc.apvts.getRawParameterValue ("sk")->load() > 0.5f;
    bool byp = proc.apvts.getRawParameterValue ("byp")->load() > 0.5f;
    if (neon != lnf.neon)
    {
        lnf.neon = neon;
        for (int i = 0; i < 7; ++i)
        {
            knobs[i].s.setColour (juce::Slider::rotarySliderFillColourId, neon ? col::knobNeon[i % 6] : col::gold);
            knobs[i].name.setColour (juce::Label::textColourId, neon ? col::knobNeon[i % 6] : col::text.withAlpha (0.5f));
        }
    }
    static bool first = true;
    if (first) { first = false; for (int i = 0; i < 7; ++i) { knobs[i].s.setColour (juce::Slider::rotarySliderFillColourId, neon ? col::knobNeon[i % 6] : col::gold); knobs[i].name.setColour (juce::Label::textColourId, neon ? col::knobNeon[i % 6] : col::text.withAlpha (0.5f)); } }
    style.setButtonText (neon ? "STYLE: NEON" : "STYLE: GALAXY");
    bypass.setButtonText (byp ? "O BYPASS" : "* ACTIVE");
    auto accent = neon ? col::cyan : col::gold;
    for (auto* b : { &bypass, &style })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::textColourOffId, accent);
        b->setColour (juce::TextButton::textColourOnId, col::text.withAlpha (0.5f));
        b->setColour (juce::ComboBox::outlineColourId, neon ? col::pink : col::gold.withAlpha (0.4f));
    }
    lv += (juce::jmin (1.f, proc.outLevel.load() * 2.f) - lv) * 0.4f;
    orbit += juce::MathConstants<float>::twoPi / (60.f * 30.f);
    repaint();
}

void OrbitalReverbEditor::paint (juce::Graphics& g)
{
    bool neon = lnf.neon, byp = proc.apvts.getRawParameterValue ("byp")->load() > 0.5f;
    auto accent = neon ? col::pink : col::gold;
    if (neon)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff241050), 0, 0, juce::Colour (0xff07272a), (float) getWidth(), (float) getHeight(), false));
    else g.setColour (col::bg);
    g.fillAll();

    auto box = getLocalBounds().toFloat().reduced (10);
    g.setColour (accent.withAlpha (neon ? 0.9f : 0.85f));
    g.drawRoundedRectangle (box, 28, 1.5f);
    g.setColour (accent.withAlpha (0.28f));
    g.drawRoundedRectangle (box.reduced (7), 22, 1.f);
    for (auto pt : { box.getTopLeft(), box.getTopRight(), box.getBottomLeft(), box.getBottomRight() })
    {
        auto q = pt + juce::Point<float> (pt.x < 100 ? 16.f : -16.f, pt.y < 100 ? 16.f : -16.f);
        g.setColour (accent); g.fillEllipse (q.x - 2, q.y - 2, 4, 4);
    }
    // orbit
    auto c = box.getCentre();
    g.setColour (accent.withAlpha (0.22f));
    g.drawEllipse (c.x - 130, c.y - 130, 260, 260, 0.8f);
    auto pl = c.getPointOnCircumference (130.f, orbit + 0.6f);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.6f), pl.x, pl.y, accent.withAlpha (0.f), pl.x + 24, pl.y, true));
    g.fillEllipse (pl.x - 24, pl.y - 24, 48, 48);
    g.setColour (accent); g.fillEllipse (pl.x - 3.2f, pl.y - 3.2f, 6.4f, 6.4f);

    // title + led
    g.setColour (neon ? juce::Colours::white : col::text.withAlpha (0.7f));
    g.setFont (juce::FontOptions (13.f));
    g.drawFittedText ("O R B I T A L   R E V E R B", 0, 32, getWidth(), 20, juce::Justification::centred, 1);
    juce::Colour led = byp ? juce::Colour (0xff2a2a30) : (neon ? juce::Colour (0xff00e676) : col::gold);
    float lx = getWidth() * 0.5f + 118, ly = 42;
    if (! byp) { g.setColour (led.withAlpha (0.25f + 0.5f * lv)); g.fillEllipse (lx - 8, ly - 8, 16, 16); }
    g.setColour (led); g.fillEllipse (lx - 4.5f, ly - 4.5f, 9, 9);

    // group labels
    g.setFont (juce::FontOptions (9.f));
    g.setColour (neon ? juce::Colours::white : col::text.withAlpha (0.5f));
    for (auto [t, yy] : { std::pair<const char*, int> { "S P A C E", 66 }, { "T A I L", 246 }, { "M I X", 388 } })
        g.drawFittedText (t, 0, yy, getWidth(), 12, juce::Justification::centred, 1);

    // GR meter
    g.setColour (col::text); g.setFont (juce::FontOptions (11.f));
    g.drawText ("OUT", meter.withWidth (24), juce::Justification::centredLeft);
    auto bar = meter.withTrimmedLeft (30).toFloat().withSizeKeepingCentre ((float) meter.getWidth() - 30, 4);
    g.setColour (neon ? juce::Colour (0xff241f4a) : juce::Colour (0xff1c1c21)); g.fillRect (bar);
    auto fill = bar.withWidth (bar.getWidth() * juce::jlimit (0.f, 1.f, byp ? 0.f : lv * 0.8f));
    if (neon) g.setGradientFill (juce::ColourGradient (col::cyan, bar.getX(), 0, col::pink, bar.getRight(), 0, false));
    else g.setColour (col::gold);
    g.fillRect (fill);

    g.setColour (neon ? juce::Colour (0xffffeb3b) : col::gold.withAlpha (0.75f));
    g.setFont (juce::FontOptions ("Georgia", 12.f, juce::Font::italic));
    g.drawFittedText ("by Dante Ramos", 0, getHeight() - 42, getWidth(), 16, juce::Justification::centred, 1);
}

void OrbitalReverbEditor::resized()
{
    auto place = [this] (int i, int cx, int y, int size)
    {
        knobs[i].s.setBounds (cx - size / 2 - 10, y, size + 20, size + 20);
        knobs[i].name.setBounds (cx - 50, y + size + 20, 100, 14);
    };
    int W = getWidth();
    place (0, W / 2 - 108, 82, 112);
    place (1, W / 2 + 14, 96, 84);
    place (2, W / 2 + 122, 96, 84);
    place (3, W / 2 - 57, 262, 89);
    place (4, W / 2 + 57, 262, 89);
    place (5, W / 2 - 59, 404, 94);
    place (6, W / 2 + 59, 404, 94);
    meter = { 40, 548, W - 80, 14 };
    style.setBounds (W / 2 - 130, 572, 125, 26);
    bypass.setBounds (W / 2 + 5, 572, 125, 26);
}

void OrbitalReverbEditor::mouseUp (const juce::MouseEvent&) {}
