#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "BowDSP.h"

class BowStringProcessor : public juce::AudioProcessor
{
public:
    BowStringProcessor();
    ~BowStringProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BowString"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor for the note display and input meter
    std::atomic<float> detectedPitch { 0.0f };
    std::atomic<float> inputLevel    { 0.0f };

private:
    void updateParameters();
    void updateBody (int mode);

    struct Params
    {
        std::atomic<float> *mode, *key, *octave, *blend, *bright, *bowNoise, *sympath,
                           *attack, *release, *dynamics, *glide, *vibRate, *vibDepth,
                           *gateOn, *gate, *compOn, *sustain, *driveOn, *drive, *driveLvl,
                           *eqOn, *bass, *mid, *treble,
                           *chorusOn, *chorusRate, *chorusDepth, *chorusMix,
                           *delayOn, *delayTime, *delayFb, *delayMix,
                           *reverbOn, *revSize, *revDamp, *revMix, *output;
    } p {};

    bow::PitchTracker       tracker;
    bow::BowedVoice         voice;
    bow::SympatheticStrings sympathetic;
    bow::PingPongDelay      delay;

    std::array<juce::dsp::IIR::Filter<float>, 6> body; // high-pass + 5 body resonances
    std::array<juce::dsp::IIR::Filter<float>, 3> eq;   // bass / mid / treble
    juce::dsp::Compressor<float> comp;
    juce::dsp::Chorus<float>     chorus;
    juce::dsp::Reverb            reverb;

    juce::SmoothedValue<float> outGain, blendSmoothed;

    double sr = 44100.0;
    int    currentMode = -1;
    float  lastBass = -99.0f, lastMid = -99.0f, lastTreble = -99.0f;
    bool   lastDelayOn = false, lastReverbOn = false;

    // per-block cached settings
    float octaveMul = 1.0f, gateThreshDb = -55.0f, dynamicsAmt = 0.5f, sympathAmt = 0.2f;
    float driveGain = 1.0f, driveLevel = 1.0f, swellInc = 0.001f;

    // input analysis state
    float env = 0.0f, fastEnv = 0.0f, slowEnv = 0.0f, swell = 1.0f, dryGate = 0.0f;
    float envAtt = 0.0f, envRel = 0.0f, fastAtt = 0.0f, fastRel = 0.0f, slowAtt = 0.0f, slowRel = 0.0f;
    float gateAtt = 0.0f, gateRel = 0.0f;
    bool  noteActive = false, hasPitch = false;
    int   samplesSinceOnset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BowStringProcessor)
};
