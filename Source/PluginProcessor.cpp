#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kTruePeakMarginDb = -0.1f;   // headroom for inter-sample peaks after downsampling

    juce::String dbText (float v, int)  { return juce::String (v, 1) + " dB"; }
    juce::String msText (float v, int)  { return v < 10.0f ? juce::String (v, 2) + " ms" : juce::String (v, 0) + " ms"; }
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
}

MasterLimiterAudioProcessor::MasterLimiterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    outPeak[0].store (0.0f);
    outPeak[1].store (0.0f);

    pGain      = apvts.getRawParameterValue ("gain");
    pCeiling   = apvts.getRawParameterValue ("ceiling");
    pAttack    = apvts.getRawParameterValue ("attack");
    pRelease   = apvts.getRawParameterValue ("release");
    pLookahead = apvts.getRawParameterValue ("lookahead");
    pLink      = apvts.getRawParameterValue ("link");
    pStyle     = apvts.getRawParameterValue ("style");
    pOversamp  = apvts.getRawParameterValue ("oversampling");
    pTruePeak  = apvts.getRawParameterValue ("truepeak");
    pDither    = apvts.getRawParameterValue ("dither");
    pUnity     = apvts.getRawParameterValue ("unity");
    pDelta     = apvts.getRawParameterValue ("delta");
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterLimiterAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 0.0f);
        r.setSkewForCentre (centre);
        return r;
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
        NormalisableRange<float> (0.0f, 30.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Output Ceiling",
        NormalisableRange<float> (-12.0f, 0.0f, 0.01f), -1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
        skewed (0.01f, 10.0f, 1.5f), 2.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
        skewed (1.0f, 2000.0f, 120.0f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lookahead", 1 }, "Lookahead",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (msText)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "link", 1 }, "Stereo Link",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

    StringArray styleNames;
    for (const auto& s : kStyles) styleNames.add (s.name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", styleNames, 0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "oversampling", 1 }, "Oversampling",
        StringArray { "Off", "2x", "4x", "8x", "16x" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "truepeak", 1 }, "True Peak", true));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "dither", 1 }, "Dither",
        StringArray { "Off", "16-bit", "24-bit" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "unity", 1 }, "Unity Gain", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "delta", 1 }, "Delta", false));

    return layout;
}

bool MasterLimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MasterLimiterAudioProcessor::prepareToPlay (double sampleRate, int)
{
    baseRate    = sampleRate;
    numChannels = std::max (1, std::max (getTotalNumInputChannels(), getTotalNumOutputChannels()));

    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, i + 1,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,    // max quality
            true);   // integer latency so we can report it exactly
        oversamplers[i]->initProcessing ((size_t) kSubBlock);
    }

    engine.prepare (sampleRate * 16.0, 10.0, numChannels);
    loudness.prepare (sampleRate, numChannels);

    inputGain.reset (sampleRate, 0.05);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    frameLen = std::max (1, (int) (sampleRate * 0.02));   // one history frame every 20 ms
    frameCount = 0; frameIn = frameOut = frameGr = 0.0f;

    currentOs = currentLook = -1;
    updateConfiguration();
}

void MasterLimiterAudioProcessor::updateConfiguration()
{
    int os = (int) pOversamp->load();              // 0 = off, 1..4 = 2x..16x
    if (pTruePeak->load() > 0.5f && os < 2) os = 2; // true-peak mode uses at least 4x

    const int look = (int) std::lround (pLookahead->load() * 0.001 * baseRate);

    if (os == currentOs && look == currentLook)
        return;

    currentOs = os;
    currentLook = look;

    const int factor = 1 << os;
    engine.configure (baseRate * factor, look * factor, numChannels);

    int latency = look;
    if (os > 0)
    {
        auto& o = *oversamplers[(size_t) os - 1];
        o.reset();
        latency += (int) std::lround (o.getLatencyInSamples());
    }
    setLatencySamples (latency);
}

float MasterLimiterAudioProcessor::nextRandom() noexcept
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float) (rng >> 8) * (1.0f / 16777216.0f);
}

void MasterLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int n        = buffer.getNumSamples();

    for (int c = totalIn; c < totalOut; ++c)
        buffer.clear (c, 0, n);

    const int nCh = std::min ({ buffer.getNumChannels(), totalOut, numChannels, LimiterEngine::kMaxChannels });
    if (nCh <= 0 || n <= 0) return;

    updateConfiguration();

    const bool  truePeak   = pTruePeak->load() > 0.5f;
    const bool  delta      = pDelta->load() > 0.5f;
    const bool  unity      = pUnity->load() > 0.5f;
    const float ceilingLin = juce::Decibels::decibelsToGain (pCeiling->load());
    const float engineCeil = ceilingLin * (truePeak ? juce::Decibels::decibelsToGain (kTruePeakMarginDb) : 1.0f);
    const int   ditherIdx  = (int) pDither->load();
    const int   ditherBits = ditherIdx == 1 ? 16 : (ditherIdx == 2 ? 24 : 0);

    engine.setParameters (engineCeil, pAttack->load(), pRelease->load(),
                          pLink->load() * 0.01f, (int) pStyle->load(), delta);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    float* ptrs[LimiterEngine::kMaxChannels] = {};
    float* upPtrs[LimiterEngine::kMaxChannels] = {};

    for (int start = 0; start < n; start += kSubBlock)
    {
        const int len = std::min (kSubBlock, n - start);
        for (int c = 0; c < nCh; ++c)
            ptrs[c] = buffer.getWritePointer (c, start);

        // Input gain + input metering
        float inPeak = 0.0f;
        for (int i = 0; i < len; ++i)
        {
            const float g = inputGain.getNextValue();
            gainScratch[(size_t) i] = g;
            for (int c = 0; c < nCh; ++c)
            {
                ptrs[c][i] *= g;
                inPeak = std::max (inPeak, std::abs (ptrs[c][i]));
            }
        }

        // Limiting (optionally oversampled)
        juce::dsp::AudioBlock<float> block (ptrs, (size_t) nCh, (size_t) len);
        if (currentOs > 0)
        {
            auto& os = *oversamplers[(size_t) currentOs - 1];
            auto up = os.processSamplesUp (block);
            for (int c = 0; c < nCh; ++c)
                upPtrs[c] = up.getChannelPointer ((size_t) c);
            engine.process (upPtrs, nCh, (int) up.getNumSamples());
            os.processSamplesDown (block);
        }
        else
        {
            engine.process (ptrs, nCh, len);
        }

        // Final sample-peak safety at the base rate
        if (! delta)
            for (int c = 0; c < nCh; ++c)
                juce::FloatVectorOperations::clip (ptrs[c], ptrs[c], -ceilingLin, ceilingLin, len);

        // Output metering (real output level, before unity-gain monitoring)
        float outPeakBlock = 0.0f;
        for (int c = 0; c < nCh; ++c)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (ptrs[c], len);
            const float pk = std::max (std::abs (range.getStart()), std::abs (range.getEnd()));
            outPeakBlock = std::max (outPeakBlock, pk);
            if (c < 2) outPeak[c].store (std::max (outPeak[c].load(), pk));
        }
        if (nCh == 1) outPeak[1].store (outPeak[0].load());

        loudness.process (ptrs, nCh, len);

        // Unity gain monitoring: remove the input gain to judge the tone, not the loudness
        if (unity && ! delta)
            for (int i = 0; i < len; ++i)
            {
                const float inv = 1.0f / gainScratch[(size_t) i];
                for (int c = 0; c < nCh; ++c) ptrs[c][i] *= inv;
            }

        // TPDF dither + quantisation
        if (ditherBits > 0 && ! delta)
        {
            const float q = 1.0f / (float) (1 << (ditherBits - 1));
            const float invQ = 1.0f / q;
            for (int c = 0; c < nCh; ++c)
                for (int i = 0; i < len; ++i)
                {
                    const float v = ptrs[c][i] + (nextRandom() - nextRandom()) * q;
                    ptrs[c][i] = std::round (v * invQ) * q;
                }
        }

        // History for the scrolling display
        const float gr = engine.getAndResetMaxGrDb();
        grMeter.store (std::max (grMeter.load(), gr));
        frameIn  = std::max (frameIn, inPeak);
        frameOut = std::max (frameOut, outPeakBlock);
        frameGr  = std::max (frameGr, gr);
        frameCount += len;
        if (frameCount >= frameLen)
        {
            pushFrame ({ juce::Decibels::gainToDecibels (frameIn, -100.0f),
                         juce::Decibels::gainToDecibels (frameOut, -100.0f),
                         frameGr });
            frameIn = frameOut = frameGr = 0.0f;
            frameCount = 0;
        }
    }
}

void MasterLimiterAudioProcessor::pushFrame (const HistoryFrame& f)
{
    int s1, z1, s2, z2;
    historyFifo.prepareToWrite (1, s1, z1, s2, z2);
    if (z1 > 0)      historyBuf[(size_t) s1] = f;
    else if (z2 > 0) historyBuf[(size_t) s2] = f;
    historyFifo.finishedWrite (z1 + z2);
}

int MasterLimiterAudioProcessor::popHistory (HistoryFrame* dest, int maxFrames)
{
    int s1, z1, s2, z2;
    historyFifo.prepareToRead (maxFrames, s1, z1, s2, z2);
    for (int i = 0; i < z1; ++i) dest[i]      = historyBuf[(size_t) (s1 + i)];
    for (int i = 0; i < z2; ++i) dest[z1 + i] = historyBuf[(size_t) (s2 + i)];
    historyFifo.finishedRead (z1 + z2);
    return z1 + z2;
}

void MasterLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MasterLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MasterLimiterAudioProcessor::createEditor()
{
    return new MasterLimiterAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterLimiterAudioProcessor();
}
