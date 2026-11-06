#include "PluginProcessor.h"
#include "PluginEditor.h"

using P = juce::AudioParameterFloat;
using R = juce::NormalisableRange<float>;

namespace
{
    struct Planet { float len, br, sw; bool er; };
    // Moon, Mercury, Jupiter, Black Hole (same character as the HTML version)
    const Planet planets[] = { { 0.5f, 0.75f, 0.0f, true }, { 1.6f, 0.6f, 0.0f, false },
                               { 3.6f, 0.4f, 0.08f, false }, { 6.5f, 0.3f, 0.5f, false } };
}

juce::AudioProcessorValueTreeState::ParameterLayout OrbitalReverbProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto pct = juce::AudioParameterFloatAttributes().withLabel ("%");
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pl", 1 }, "Planet",
        juce::StringArray { "Moon", "Mercury", "Jupiter", "Black Hole" }, 1));
    p.push_back (std::make_unique<P> (juce::ParameterID { "sz", 1 }, "Size", R (60.f, 150.f, 1.f), 100.f, pct));
    p.push_back (std::make_unique<P> (juce::ParameterID { "pd", 1 }, "Pre-delay", R (0.f, 200.f, 1.f), 30.f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "tone", 1 }, "Tone", R (800.f, 16000.f, 1.f, 0.3f), 7566.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return v >= 1000.f ? juce::String (v / 1000.f, 1) + " kHz" : juce::String ((int) v) + " Hz"; })));
    p.push_back (std::make_unique<P> (juce::ParameterID { "dc", 1 }, "Decay", R (0.f, 100.f, 1.f), 50.f, pct));
    p.push_back (std::make_unique<P> (juce::ParameterID { "mix", 1 }, "Mix", R (0.f, 100.f, 1.f), 35.f, pct));
    p.push_back (std::make_unique<P> (juce::ParameterID { "out", 1 }, "Output", R (-48.f, 24.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return v <= -47.9f ? juce::String ("-inf dB") : (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "byp", 1 }, "Bypass", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sk", 1 }, "Style", juce::StringArray { "Galaxy", "Neon" }, 0));
    return { p.begin(), p.end() };
}

OrbitalReverbProcessor::OrbitalReverbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    startTimer (120);
}

bool OrbitalReverbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto o = l.getMainOutputChannelSet();
    if (o != juce::AudioChannelSet::mono() && o != juce::AudioChannelSet::stereo()) return false;
    return o == l.getMainInputChannelSet();
}

void OrbitalReverbProcessor::rebuildIR()
{
    const auto& T = planets[juce::jlimit (0, 3, (int) apvts.getRawParameterValue ("pl")->load())];
    const float sz = apvts.getRawParameterValue ("sz")->load() / 100.f;
    const float dc = apvts.getRawParameterValue ("dc")->load() / 100.f;

    // Natural exponential decay (RT60) instead of a curve that collapses at the end.
    const float rt60 = T.len * sz * 0.55f * std::pow (2.2f, 2.f * dc - 1.f);
    const float lenSec = juce::jmin (10.f, rt60 * 1.5f + 0.05f);
    const int L = juce::jmax (64, (int) (lenSec * sr));
    const int fadeLen = juce::jmax (1, (int) (L * 0.15f));
    juce::AudioBuffer<float> ir (2, L);
    juce::Random rnd;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = ir.getWritePointer (c);
        float y = 0.f;
        for (int i = 0; i < L; ++i)
        {
            const float t = (float) i / (float) L, sec = (float) i / (float) sr;
            const float a = juce::jmax (0.04f, T.br * (1.f - 0.75f * t));      // darker as it decays
            y += a * ((rnd.nextFloat() * 2.f - 1.f) - y);
            float env = std::pow (10.f, -3.f * sec / rt60);                     // -60 dB at rt60
            if (T.sw > 0.f) env *= juce::jmin (1.f, sec / T.sw);                // slow swell
            const int fromEnd = L - 1 - i;
            if (fromEnd < fadeLen) env *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) fromEnd / (float) fadeLen);
            d[i] = y * env;
        }
        if (T.er)
            for (int k = 0; k < 8; ++k)
            {
                int p = (int) ((0.004f + k * 0.007f + rnd.nextFloat() * 0.004f) * sr);
                if (p < L) d[p] += (0.9f - k * 0.08f) * (rnd.nextBool() ? 1.f : -1.f);
            }
    }
    conv.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes,
                              juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
}

void OrbitalReverbProcessor::timerCallback()
{
    if (! prepared) return;
    juce::String key = juce::String ((int) apvts.getRawParameterValue ("pl")->load()) + "|"
                     + juce::String ((int) apvts.getRawParameterValue ("sz")->load()) + "|"
                     + juce::String ((int) apvts.getRawParameterValue ("dc")->load()) + "|" + juce::String (sr);
    if (key != irKey) { irKey = key; rebuildIR(); }
}

void OrbitalReverbProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    const auto ch = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, ch };
    conv.prepare (spec); tone.prepare (spec); pre.prepare (spec);
    tone.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    pre.setMaximumDelayInSamples ((int) (sr * 0.25) + 8);
    wet.setSize ((int) ch, blockSize);
    for (auto* s : { &dryS, &wetS, &outS, &bypS }) s->reset (sr, 0.03);
    pdS.reset (sr, 0.08);
    pdS.setCurrentAndTargetValue (apvts.getRawParameterValue ("pd")->load() * 0.001f * (float) sr);
    bypS.setCurrentAndTargetValue (apvts.getRawParameterValue ("byp")->load() > 0.5f ? 1.f : 0.f);
    irKey = {};
    prepared = true;
    timerCallback();
}

void OrbitalReverbProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int nCh = getTotalNumInputChannels(), n = buf.getNumSamples();
    for (int c = nCh; c < getTotalNumOutputChannels(); ++c) buf.clear (c, 0, n);
    if (wet.getNumSamples() < n || wet.getNumChannels() < nCh) wet.setSize (juce::jmax (nCh, 1), n, false, false, true);

    const float m = apvts.getRawParameterValue ("mix")->load() / 100.f * juce::MathConstants<float>::halfPi;
    dryS.setTargetValue (std::cos (m));
    wetS.setTargetValue (std::sin (m) * 1.4f);
    const float outDb = apvts.getRawParameterValue ("out")->load();
    outS.setTargetValue (outDb <= -47.9f ? 0.f : juce::Decibels::decibelsToGain (outDb));
    bypS.setTargetValue (apvts.getRawParameterValue ("byp")->load() > 0.5f ? 1.f : 0.f);
    pdS.setTargetValue (apvts.getRawParameterValue ("pd")->load() * 0.001f * (float) sr);
    tone.setCutoffFrequency (apvts.getRawParameterValue ("tone")->load());

    for (int c = 0; c < nCh; ++c) wet.copyFrom (c, 0, buf, c, 0, n);
    for (int i = 0; i < n; ++i)
    {
        const float d = pdS.getNextValue();
        pre.setDelay (d);
        for (int c = 0; c < nCh; ++c)
        {
            pre.pushSample (c, wet.getSample (c, i));
            wet.setSample (c, i, pre.popSample (c));
        }
    }
    juce::dsp::AudioBlock<float> wb (wet.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (wb);
    conv.process (ctx);
    tone.process (ctx);

    float peak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float dg = dryS.getNextValue(), wg = wetS.getNextValue(), o = outS.getNextValue(), b = bypS.getNextValue();
        for (int c = 0; c < nCh; ++c)
        {
            const float x = buf.getSample (c, i);
            float v = (x * dg + wet.getSample (c, i) * wg) * o;
            v = juce::jlimit (-1.5f, 1.5f, v);
            v = v * (1.f - b) + x * b;
            buf.setSample (c, i, v);
            peak = std::max (peak, std::abs (v));
        }
    }
    outLevel = peak;
}

juce::AudioProcessorEditor* OrbitalReverbProcessor::createEditor() { return new OrbitalReverbEditor (*this); }

void OrbitalReverbProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto x = apvts.copyState().createXml()) copyXmlToBinary (*x, d);
}
void OrbitalReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size))
        if (x->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*x));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OrbitalReverbProcessor(); }
