#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

//==============================================================================
// SUNRIZE – a dreamy pad synth.
// The big SUN knob opens the filter (night -> noon). Ten colour knobs add
// weather, birds, bloom, echoes, drift, dust, light and drops around the pad.
//==============================================================================

struct SunrizeSound
{
    const char* name;
    int waveA;   // 0 sine, 1 triangle, 2 saw, 3 square  (the "bright" layer)
    int waveB;   // the "soft" layer
    float subLevel; // extra sub character for the sound
};

class SunrizeAudioProcessor : public juce::AudioProcessor
{
public:
    SunrizeAudioProcessor();
    ~SunrizeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SUNRIZE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static const std::array<SunrizeSound, 10>& sounds();

    // Scope: the editor reads the most recent output samples from here.
    static constexpr int scopeSize = 2048;
    void copyScope (float* dest, int num) const;

private:
    //==========================================================================
    struct Voice
    {
        bool active = false, releasing = false;
        int note = -1;
        float freq = 220.0f, velocity = 1.0f;
        float env = 0.0f;
        double ph[8] {};      // 0..3 main pair layers, 4 sub, 5 shimmer, 6 light
        double lightLfo = 0.0;
        float lightRate = 0.05f;
        uint32_t age = 0;
    };

    struct Blip  // one-shot birds / drops
    {
        bool active = false;
        double phase = 0.0;
        float f0 = 0, f1 = 0, f2 = 0;   // start, target, drift
        float sweep = 0.05f;           // seconds from f0 to f1
        float t = 0, attack = 0.01f, decay = 0.2f, amp = 0.05f;
        float pan = 0.0f;
    };

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void triggerBird();
    void triggerDrop();
    void triggerCrackle();
    int pickHeldNote();
    Blip* freeBlip();

    static float osc (int wave, double phase, double dt);

    double sr = 44100.0;
    std::array<Voice, 8> voices;
    std::array<Blip, 32> blips;
    uint32_t noteCounter = 0;
    juce::Random rng;

    // global modulation
    double wowPhase = 0.0;
    float cloudFactor = 1.0f, cloudTarget = 1.0f;
    int cloudHold = 0;
    float crackleEnv = 0.0f, crackleAmp = 0.0f;
    int eventCounter = 0;
    float lastLight = -1.0f;

    // smoothed parameters
    juce::SmoothedValue<float> sCut, sToneA, sToneB, sSub, sQ, sWow, sSpread,
        sWet, sDry, sShim, sFb, sEcho, sChorus, sHiss, sLight, sBreeze, sOut;

    // processing
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Limiter<float> limiter;
    juce::dsp::IIR::Filter<float> airL, airR, hissBP, breezeBP;
    std::vector<float> delayL, delayR;
    int delayPos = 0;
    float echoLpL = 0, echoLpR = 0;

    juce::AudioBuffer<float> padBuf, choBuf, revBuf, dryBuf;

    // scope ring
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SunrizeAudioProcessor)
};
