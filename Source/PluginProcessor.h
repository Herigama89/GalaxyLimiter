#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>

#include "LimiterEngine.h"
#include "LoudnessMeter.h"

struct HistoryFrame
{
    float inDb  = -100.0f;
    float outDb = -100.0f;
    float grDb  = 0.0f;
};

class MasterLimiterAudioProcessor final : public juce::AudioProcessor
{
public:
    MasterLimiterAudioProcessor();
    ~MasterLimiterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // UI access (lock-free)
    int popHistory (HistoryFrame* dest, int maxFrames);

    juce::AudioProcessorValueTreeState apvts;
    LoudnessMeter loudness;
    std::atomic<float> outPeak[2];
    std::atomic<float> grMeter { 0.0f };

private:
    void updateConfiguration();
    void pushFrame (const HistoryFrame& f);
    float nextRandom() noexcept;

    static constexpr int kSubBlock = 256;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers; // 2x, 4x, 8x, 16x
    LimiterEngine engine;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain;
    std::array<float, kSubBlock> gainScratch {};

    double baseRate = 44100.0;
    int numChannels = 2;
    int currentOs = -1, currentLook = -1;

    juce::AbstractFifo historyFifo { 2048 };
    std::array<HistoryFrame, 2048> historyBuf;
    float frameIn = 0.0f, frameOut = 0.0f, frameGr = 0.0f;
    int frameCount = 0, frameLen = 882;

    uint32_t rng = 0x9E3779B9u;

    std::atomic<float>* pGain      = nullptr;
    std::atomic<float>* pCeiling   = nullptr;
    std::atomic<float>* pAttack    = nullptr;
    std::atomic<float>* pRelease   = nullptr;
    std::atomic<float>* pLookahead = nullptr;
    std::atomic<float>* pLink      = nullptr;
    std::atomic<float>* pStyle     = nullptr;
    std::atomic<float>* pOversamp  = nullptr;
    std::atomic<float>* pTruePeak  = nullptr;
    std::atomic<float>* pDither    = nullptr;
    std::atomic<float>* pUnity     = nullptr;
    std::atomic<float>* pDelta     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessor)
};
