#pragma once
#include <JuceHeader.h>

class OrbitalReverbProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    OrbitalReverbProcessor();
    ~OrbitalReverbProcessor() override { stopTimer(); }
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Orbital Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outLevel { 0.0f };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void rebuildIR();
    double sr = 44100.0;
    std::atomic<bool> prepared { false };
    juce::String irKey;
    juce::dsp::Convolution conv;
    juce::dsp::StateVariableTPTFilter<float> tone;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> pre { 48000 };
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> dryS, wetS, outS, bypS, pdS;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitalReverbProcessor)
};
