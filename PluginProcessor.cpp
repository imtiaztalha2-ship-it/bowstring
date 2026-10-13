#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS  = juce::AudioProcessorValueTreeState;
using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;

//==============================================================================
APVTS::ParameterLayout BowStringProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto skewed = [] (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto addFloat = [&] (const char* id, const char* name, juce::NormalisableRange<float> range,
                         float def, juce::String suffix, int decimals)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [suffix, decimals] (float v, int)
                {
                    return (decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals)) + suffix;
                })));
    };

    auto addPercent = [&] (const char* id, const char* name, float def, float max = 1.0f)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, max), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })));
    };

    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    auto addChoice = [&] (const char* id, const char* name, juce::StringArray choices, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
    };

    // Instrument
    addChoice ("mode",   "Instrument", { "Violin", "Sarangi" }, 0);
    addChoice ("key",    "Sarangi key (Sa)", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    addChoice ("octave", "Octave", { "-1", "0", "+1", "+2" }, 1);
    addPercent ("blend",    "Bowed blend", 0.85f);
    addPercent ("bright",   "Brightness",  0.55f);
    addPercent ("bownoise", "Bow noise",   0.25f);
    addPercent ("sympath",  "Sympathetic strings", 0.20f);

    // Bowing
    addFloat ("attack",   "Bow attack",    skewed (5.0f, 1000.0f, 150.0f), 120.0f, " ms", 0);
    addFloat ("release",  "Bow release",   skewed (20.0f, 2000.0f, 300.0f), 250.0f, " ms", 0);
    addPercent ("dynamics", "Dynamics", 0.5f);
    addFloat ("glide",    "Glide",         skewed (0.0f, 500.0f, 80.0f), 60.0f, " ms", 0);
    addFloat ("vibrate",  "Vibrato rate",  juce::NormalisableRange<float> (2.0f, 9.0f), 5.5f, " Hz", 1);
    addFloat ("vibdepth", "Vibrato depth", juce::NormalisableRange<float> (0.0f, 60.0f), 18.0f, " ct", 0);

    // Pedals
    addBool  ("gate_on", "Gate on", true);
    addFloat ("gate",    "Gate threshold", juce::NormalisableRange<float> (-80.0f, -20.0f), -55.0f, " dB", 0);

    addBool    ("comp_on", "Compressor on", true);
    addPercent ("sustain", "Sustain", 0.4f);

    addBool    ("drive_on", "Drive on", false);
    addPercent ("drive",    "Drive", 0.3f);
    addFloat   ("drivelvl", "Drive level", juce::NormalisableRange<float> (-24.0f, 6.0f), -6.0f, " dB", 1);

    addBool  ("eq_on",  "EQ on", true);
    addFloat ("bass",   "Bass",   juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f, " dB", 1);
    addFloat ("mid",    "Mid",    juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f, " dB", 1);
    addFloat ("treble", "Treble", juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f, " dB", 1);

    addBool    ("chorus_on",   "Chorus on", false);
    addFloat   ("chorusrate",  "Chorus rate", juce::NormalisableRange<float> (0.1f, 5.0f), 0.8f, " Hz", 2);
    addPercent ("chorusdepth", "Chorus depth", 0.3f);
    addPercent ("chorusmix",   "Chorus mix", 0.3f);

    addBool    ("delay_on",  "Delay on", true);
    addFloat   ("delaytime", "Delay time", skewed (20.0f, 1500.0f, 400.0f), 380.0f, " ms", 0);
    addPercent ("delayfb",   "Delay feedback", 0.3f, 0.9f);
    addPercent ("delaymix",  "Delay mix", 0.18f);

    addBool    ("reverb_on", "Reverb on", true);
    addPercent ("revsize",   "Reverb size", 0.7f);
    addPercent ("revdamp",   "Reverb damping", 0.4f);
    addPercent ("revmix",    "Reverb mix", 0.3f);

    addFloat ("output", "Output", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f, " dB", 1);

    return { params.begin(), params.end() };
}

//==============================================================================
BowStringProcessor::BowStringProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    p.mode = get ("mode");         p.key = get ("key");           p.octave = get ("octave");
    p.blend = get ("blend");       p.bright = get ("bright");     p.bowNoise = get ("bownoise");
    p.sympath = get ("sympath");   p.attack = get ("attack");     p.release = get ("release");
    p.dynamics = get ("dynamics"); p.glide = get ("glide");       p.vibRate = get ("vibrate");
    p.vibDepth = get ("vibdepth"); p.gateOn = get ("gate_on");    p.gate = get ("gate");
    p.compOn = get ("comp_on");    p.sustain = get ("sustain");   p.driveOn = get ("drive_on");
    p.drive = get ("drive");       p.driveLvl = get ("drivelvl"); p.eqOn = get ("eq_on");
    p.bass = get ("bass");         p.mid = get ("mid");           p.treble = get ("treble");
    p.chorusOn = get ("chorus_on");     p.chorusRate = get ("chorusrate");
    p.chorusDepth = get ("chorusdepth"); p.chorusMix = get ("chorusmix");
    p.delayOn = get ("delay_on");  p.delayTime = get ("delaytime");
    p.delayFb = get ("delayfb");   p.delayMix = get ("delaymix");
    p.reverbOn = get ("reverb_on"); p.revSize = get ("revsize");
    p.revDamp = get ("revdamp");   p.revMix = get ("revmix");     p.output = get ("output");
}

bool BowStringProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

//==============================================================================
void BowStringProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    tracker.prepare (sr);
    voice.prepare (sr);
    sympathetic.prepare (sr);
    delay.prepare (sr);

    const auto numOut = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec monoSpec   { sr, (juce::uint32) samplesPerBlock, 1 };
    juce::dsp::ProcessSpec stereoSpec { sr, (juce::uint32) samplesPerBlock, numOut };

    for (auto& f : body) { f.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0f); f.reset(); }
    for (auto& f : eq)   { f.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0f); f.reset(); }
    currentMode = -1;
    lastBass = lastMid = lastTreble = -99.0f;

    comp.prepare (monoSpec);
    comp.setAttack (10.0f);
    comp.setRelease (150.0f);

    chorus.prepare (stereoSpec);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.1f);

    reverb.prepare (stereoSpec);
    reverb.reset();

    envAtt  = bow::onePoleCoeff (2.0f,  sr);  envRel  = bow::onePoleCoeff (90.0f,  sr);
    fastAtt = bow::onePoleCoeff (0.5f,  sr);  fastRel = bow::onePoleCoeff (25.0f,  sr);
    slowAtt = bow::onePoleCoeff (25.0f, sr);  slowRel = bow::onePoleCoeff (250.0f, sr);
    gateAtt = bow::onePoleCoeff (3.0f,  sr);  gateRel = bow::onePoleCoeff (60.0f,  sr);

    env = fastEnv = slowEnv = dryGate = 0.0f;
    swell = 1.0f;
    noteActive = hasPitch = false;
    samplesSinceOnset = 0;

    outGain.reset (sr, 0.05);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.output->load()));
    blendSmoothed.reset (sr, 0.05);
    blendSmoothed.setCurrentAndTargetValue (p.blend->load());

    lastDelayOn  = p.delayOn->load()  > 0.5f;
    lastReverbOn = p.reverbOn->load() > 0.5f;

    updateParameters();
}

//==============================================================================
void BowStringProcessor::updateBody (int mode)
{
    struct Band { float freq, q, gainDb; };

    // Violin: air + wood resonances and the "bridge hill" around 2.5-3 kHz
    static constexpr Band violin[5]  = { { 280.0f, 2.5f, 5.0f }, { 470.0f, 3.0f, 4.0f }, { 1000.0f, 1.0f, -3.0f },
                                         { 2700.0f, 1.2f, 6.0f }, { 6500.0f, 0.7f, -6.0f } };
    // Sarangi: skin-covered body, strong nasal formant
    static constexpr Band sarangi[5] = { { 400.0f, 2.0f, 4.0f }, { 950.0f, 2.5f, 7.0f }, { 1900.0f, 2.0f, 5.0f },
                                         { 3200.0f, 1.5f, 4.0f }, { 6000.0f, 0.7f, -8.0f } };

    const auto& bands = mode == 0 ? violin : sarangi;
    const float nyq = (float) sr * 0.45f;

    *body[0].coefficients = Coeffs::makeHighPass (sr, mode == 0 ? 190.0f : 150.0f, 0.7071f);

    for (size_t i = 0; i < 5; ++i)
        *body[i + 1].coefficients = Coeffs::makePeakFilter (sr, juce::jmin (bands[i].freq, nyq), bands[i].q,
                                                            juce::Decibels::decibelsToGain (bands[i].gainDb));
}

void BowStringProcessor::updateParameters()
{
    const int mode = (int) p.mode->load();
    if (mode != currentMode)
    {
        updateBody (mode);
        currentMode = mode;
    }

    sympathetic.setKey ((int) p.key->load());
    octaveMul = std::exp2 ((float) ((int) p.octave->load() - 1));

    voice.setParams (p.attack->load(), p.release->load(), p.glide->load(),
                     p.vibRate->load(), p.vibDepth->load(), p.bowNoise->load(), p.bright->load());

    swellInc     = 1.0f / juce::jmax (1.0f, p.attack->load() * 0.001f * (float) sr);
    dynamicsAmt  = p.dynamics->load();
    sympathAmt   = p.sympath->load();
    gateThreshDb = p.gateOn->load() > 0.5f ? p.gate->load() : -100.0f;
    blendSmoothed.setTargetValue (p.blend->load());

    // Compressor used as a sustainer
    const float sustain = p.sustain->load();
    comp.setThreshold (-10.0f - 30.0f * sustain);
    comp.setRatio (1.0f + 7.0f * sustain);

    // Drive
    driveGain  = juce::Decibels::decibelsToGain (p.drive->load() * 36.0f);
    driveLevel = juce::Decibels::decibelsToGain (p.driveLvl->load());

    // EQ
    const float bass = p.bass->load(), mid = p.mid->load(), treble = p.treble->load();
    if (! juce::exactlyEqual (bass, lastBass))
    {
        *eq[0].coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7071f, juce::Decibels::decibelsToGain (bass));
        lastBass = bass;
    }
    if (! juce::exactlyEqual (mid, lastMid))
    {
        *eq[1].coefficients = Coeffs::makePeakFilter (sr, 900.0f, 0.8f, juce::Decibels::decibelsToGain (mid));
        lastMid = mid;
    }
    if (! juce::exactlyEqual (treble, lastTreble))
    {
        *eq[2].coefficients = Coeffs::makeHighShelf (sr, juce::jmin (3500.0f, (float) sr * 0.45f), 0.7071f,
                                                     juce::Decibels::decibelsToGain (treble));
        lastTreble = treble;
    }

    // Chorus
    chorus.setRate (p.chorusRate->load());
    chorus.setDepth (p.chorusDepth->load());
    chorus.setMix (p.chorusMix->load());

    // Delay
    delay.setParams (p.delayTime->load(), p.delayFb->load(), p.delayMix->load());

    // Reverb
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize  = p.revSize->load();
    rp.damping   = p.revDamp->load();
    rp.wetLevel  = p.revMix->load();
    rp.dryLevel  = 1.0f - 0.5f * p.revMix->load();
    rp.width     = 1.0f;
    reverb.setParameters (rp);

    outGain.setTargetValue (juce::Decibels::decibelsToGain (p.output->load()));
}

//==============================================================================
void BowStringProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (numOut == 0 || n == 0)
        return;

    updateParameters();

    const bool compOn  = p.compOn->load()  > 0.5f;
    const bool driveOn = p.driveOn->load() > 0.5f;
    const bool eqOn    = p.eqOn->load()    > 0.5f;

    const float gateOnLin  = juce::Decibels::decibelsToGain (gateThreshDb);
    const float gateOffLin = juce::Decibels::decibelsToGain (gateThreshDb - 4.0f);
    const int   minOnsetGap = (int) (0.08 * sr);

    float* out0 = buffer.getWritePointer (0);
    const float* in1 = (numIn > 1 && buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float x  = in1 != nullptr ? 0.5f * (out0[i] + in1[i]) : out0[i];
        const float ax = std::abs (x);

        env     = ax + (env     - ax) * (ax > env     ? envAtt  : envRel);
        fastEnv = ax + (fastEnv - ax) * (ax > fastEnv ? fastAtt : fastRel);
        slowEnv = ax + (slowEnv - ax) * (ax > slowEnv ? slowAtt : slowRel);

        tracker.push (x);

        // gate with hysteresis decides when a note is "being bowed"
        bool onset = false;
        if (! noteActive && env > gateOnLin)
        {
            noteActive = true;
            hasPitch = false;
            onset = true;
        }
        else if (noteActive && env < gateOffLin)
        {
            noteActive = false;
        }

        ++samplesSinceOnset;
        if (noteActive && fastEnv > slowEnv * 1.6f && samplesSinceOnset > minOnsetGap)
            onset = true;

        if (onset)
        {
            samplesSinceOnset = 0;
            swell = 0.0f;
            voice.rebow();
        }

        if (noteActive && tracker.isVoiced())
        {
            voice.setTarget (tracker.getPitch() * octaveMul, voice.getAmp() < 0.02f);
            hasPitch = true;
        }

        const float level  = juce::jlimit (0.0f, 1.0f, env * 4.0f);
        const float target = (noteActive && hasPitch) ? 1.0f + dynamicsAmt * (level - 1.0f) : 0.0f;
        voice.setGate (target);

        // bowed synth voice + instrument body
        float v = voice.tick();
        for (auto& f : body)
            v = f.processSample (v);
        if (sympathAmt > 0.0f)
            v += sympathetic.process (v) * sympathAmt;
        v *= 0.5f;

        // the real guitar, with the pick attack removed by a volume swell
        swell = juce::jmin (1.0f, swell + swellInc);
        const float gateTarget = noteActive ? 1.0f : 0.0f;
        dryGate = gateTarget + (dryGate - gateTarget) * (gateTarget > dryGate ? gateAtt : gateRel);
        const float dry = x * swell * swell * dryGate;

        const float b = blendSmoothed.getNextValue();
        float y = v * b + dry * (1.0f - b);

        if (compOn)  y = comp.processSample (0, y);
        if (driveOn) y = std::tanh (y * driveGain) * driveLevel;
        if (eqOn)
            for (auto& f : eq)
                y = f.processSample (y);

        out0[i] = y;
    }

    for (int ch = 1; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n);

    // stereo pedals
    juce::dsp::AudioBlock<float> block (buffer);
    auto outBlock = block.getSubsetChannelBlock (0, (size_t) numOut);
    juce::dsp::ProcessContextReplacing<float> ctx (outBlock);

    if (p.chorusOn->load() > 0.5f)
        chorus.process (ctx);

    const bool delayOn = p.delayOn->load() > 0.5f;
    if (delayOn && ! lastDelayOn)
        delay.reset();
    lastDelayOn = delayOn;

    if (delayOn)
    {
        float* l = buffer.getWritePointer (0);
        float* r = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            float left = l[i], right = r != nullptr ? r[i] : l[i];
            delay.process (left, right);
            if (r != nullptr) { l[i] = left; r[i] = right; }
            else              { l[i] = 0.5f * (left + right); }
        }
    }

    const bool reverbOn = p.reverbOn->load() > 0.5f;
    if (reverbOn && ! lastReverbOn)
        reverb.reset();
    lastReverbOn = reverbOn;

    if (reverbOn)
        reverb.process (ctx);

    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < numOut; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }

    detectedPitch.store (noteActive && tracker.isVoiced() ? tracker.getPitch() * octaveMul : 0.0f);
    inputLevel.store (env);
}

//==============================================================================
juce::AudioProcessorEditor* BowStringProcessor::createEditor()
{
    return new BowStringEditor (*this);
}

void BowStringProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BowStringProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BowStringProcessor();
}
