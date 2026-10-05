#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    inline float polyBlep (double t, double dt)
    {
        if (t < dt)        { t /= dt;               return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline double wrap (double p) { return p - std::floor (p); }

    inline float timeCoef (float seconds, double sr)
    {
        return 1.0f - std::exp (-1.0f / (float) juce::jmax (1.0, seconds * sr));
    }

    const char* paramIds[] = { "attack", "weather", "width", "birds", "bloom",
                               "afterglow", "drift", "dust", "light", "drops" };
}

//==============================================================================
const std::array<SunrizeSound, 10>& SunrizeAudioProcessor::sounds()
{
    static const std::array<SunrizeSound, 10> s {{
        { "MORNING DEW",     2, 1, 0.0f },
        { "SEA BREEZE",      1, 0, 0.0f },
        { "CATHEDRAL",       3, 1, 0.3f },
        { "GLASSHOUSE",      0, 1, 0.0f },
        { "FOG",             2, 2, 0.1f },
        { "NORTHERN LIGHTS", 3, 0, 0.0f },
        { "DUSK",            2, 1, 0.2f },
        { "STILL WATER",     0, 0, 0.0f },
        { "WARM AIR",        1, 3, 0.2f },
        { "DISTANT RADIO",   3, 3, 0.0f }
    }};
    return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout SunrizeAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto add = [&] (const char* id, const char* name, float def)
    {
        ps.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name,
                                           juce::NormalisableRange<float> (0.0f, 1.0f), def));
    };
    add ("sun",       "Sun",       0.30f);
    add ("attack",    "Attack",    0.33f);
    add ("weather",   "Weather",   0.25f);
    add ("width",     "Width",     0.40f);
    add ("birds",     "Birds",     0.30f);
    add ("bloom",     "Bloom",     0.55f);
    add ("afterglow", "Afterglow", 0.40f);
    add ("drift",     "Drift",     0.40f);
    add ("dust",      "Dust",      0.20f);
    add ("light",     "Light",     0.45f);
    add ("drops",     "Drops",     0.00f);
    add ("output",    "Output",    0.75f);

    juce::StringArray names;
    for (auto& s : sounds()) names.add (s.name);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sound", 1 }, "Sound", names, 0));
    return { ps.begin(), ps.end() };
}

SunrizeAudioProcessor::SunrizeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUNRIZE", createLayout())
{
    juce::ignoreUnused (paramIds);
}

bool SunrizeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void SunrizeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const auto block = (juce::uint32) juce::jmax (1, samplesPerBlock);
    juce::dsp::ProcessSpec spec { sr, block, 2 };

    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    chorus.prepare (spec);
    chorus.setRate (0.45f);
    chorus.setCentreDelay (18.0f);
    chorus.setFeedback (0.0f);
    chorus.setMix (1.0f);
    reverb.prepare (spec);
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.92f; rp.damping = 0.35f; rp.width = 1.0f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
    reverb.setParameters (rp);
    comp.prepare (spec);
    comp.setThreshold (-18.0f); comp.setRatio (3.0f); comp.setAttack (10.0f); comp.setRelease (300.0f);
    limiter.prepare (spec);
    limiter.setThreshold (-0.8f); limiter.setRelease (100.0f);

    juce::dsp::ProcessSpec mono { sr, block, 1 };
    airL.prepare (mono); airR.prepare (mono); hissBP.prepare (mono); breezeBP.prepare (mono);
    airL.coefficients = airR.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 6000.0, 0.7, 1.0f);
    hissBP.coefficients   = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 3200.0, 0.6);
    breezeBP.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, juce::jmin (8500.0, sr * 0.45), 1.2);

    delayL.assign ((size_t) (sr * 2.0) + 1, 0.0f);
    delayR.assign ((size_t) (sr * 2.0) + 1, 0.0f);
    delayPos = 0; echoLpL = echoLpR = 0;

    padBuf.setSize (2, (int) block); choBuf.setSize (2, (int) block);
    revBuf.setSize (2, (int) block); dryBuf.setSize (2, (int) block);

    for (auto* s : { &sCut, &sToneA, &sToneB, &sSub, &sQ, &sWow, &sSpread, &sWet, &sDry,
                     &sShim, &sFb, &sEcho, &sChorus, &sHiss, &sLight, &sBreeze, &sOut })
        s->reset (sr, 0.05);
    sCut.reset (sr, 0.08);

    for (auto& v : voices) v = Voice();
    for (auto& b : blips) b = Blip();
    cloudFactor = cloudTarget = 1.0f;
}

//==============================================================================
void SunrizeAudioProcessor::noteOn (int note, float velocity)
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr)  // steal the oldest
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age < target->age) target = &v;
    }
    const bool wasActive = target->active;
    *target = Voice();
    target->active = true;
    target->note = note;
    target->freq = (float) juce::MidiMessage::getMidiNoteInHertz (note);
    target->velocity = velocity;
    target->env = wasActive ? 0.2f : 0.0f;
    target->age = ++noteCounter;
    target->lightRate = 0.04f + (float) (note % 4) * 0.027f;
    for (int i = 0; i < 8; ++i) target->ph[i] = rng.nextDouble();
    target->lightLfo = rng.nextDouble();
}

void SunrizeAudioProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && ! v.releasing && v.note == note)
            v.releasing = true;
}

int SunrizeAudioProcessor::pickHeldNote()
{
    int candidates[8]; int n = 0;
    for (auto& v : voices)
        if (v.active && ! v.releasing) candidates[n++] = v.note;
    if (n == 0) return -1;
    return candidates[rng.nextInt (n)];
}

SunrizeAudioProcessor::Blip* SunrizeAudioProcessor::freeBlip()
{
    for (auto& b : blips) if (! b.active) return &b;
    return nullptr;
}

void SunrizeAudioProcessor::triggerBird()
{
    const int note = pickHeldNote();
    auto* b = freeBlip();
    if (note < 0 || b == nullptr) return;
    float f = (float) juce::MidiMessage::getMidiNoteInHertz (note) * (rng.nextBool() ? 4.0f : 8.0f);
    while (f > 9000.0f) f *= 0.5f;
    *b = Blip();
    b->active = true;
    b->f0 = f; b->f1 = f * (1.08f + rng.nextFloat() * 0.2f); b->f2 = b->f1;
    b->sweep = 0.1f; b->attack = 0.01f; b->decay = 0.18f + rng.nextFloat() * 0.12f;
    b->amp = 0.05f; b->pan = rng.nextFloat() * 1.6f - 0.8f;
}

void SunrizeAudioProcessor::triggerDrop()
{
    const int count = 1 + (rng.nextFloat() < 0.35f ? 1 + rng.nextInt (2) : 0);
    float startDelay = 0.0f;
    for (int c = 0; c < count; ++c)
    {
        const int note = pickHeldNote();
        auto* b = freeBlip();
        if (note < 0 || b == nullptr) return;
        float f = (float) juce::MidiMessage::getMidiNoteInHertz (note) * (rng.nextBool() ? 2.0f : 4.0f);
        while (f > 6000.0f) f *= 0.5f;
        *b = Blip();
        b->active = true;
        b->t = -startDelay;
        b->f0 = f * 0.45f; b->f1 = f; b->f2 = f * 1.04f;
        b->sweep = 0.025f + rng.nextFloat() * 0.035f;
        b->attack = 0.004f; b->decay = 0.12f + rng.nextFloat() * 0.12f;
        b->amp = 0.09f; b->pan = rng.nextFloat() * 1.2f - 0.6f;
        startDelay += 0.06f + rng.nextFloat() * 0.18f;
    }
}

void SunrizeAudioProcessor::triggerCrackle()
{
    const float dust = apvts.getRawParameterValue ("dust")->load();
    crackleAmp = (0.02f + rng.nextFloat() * rng.nextFloat() * 0.25f) * (0.4f + dust);
    crackleEnv = 1.0f;
}

float SunrizeAudioProcessor::osc (int wave, double t, double dt)
{
    switch (wave)
    {
        case 0:  return (float) std::sin (t * twoPi);
        case 1:  return (float) (4.0 * std::abs (t - 0.5) - 1.0);
        case 2:  return (float) (2.0 * t - 1.0) - polyBlep (t, dt);
        default:
        {
            float s = t < 0.5 ? 1.0f : -1.0f;
            s += polyBlep (t, dt);
            s -= polyBlep (wrap (t + 0.5), dt);
            return s;
        }
    }
}

//==============================================================================
void SunrizeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (padBuf.getNumSamples() < n)
    {
        padBuf.setSize (2, n, false, false, true); choBuf.setSize (2, n, false, false, true);
        revBuf.setSize (2, n, false, false, true); dryBuf.setSize (2, n, false, false, true);
    }

    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float sun = p ("sun"), attack = p ("attack"), weather = p ("weather"), width = p ("width"),
                birds = p ("birds"), bloom = p ("bloom"), glow = p ("afterglow"), drift = p ("drift"),
                dust = p ("dust"), light = p ("light"), drops = p ("drops"), out = p ("output");
    const auto& snd = sounds()[(size_t) juce::jlimit (0, 9, (int) p ("sound"))];

    // targets ---------------------------------------------------------------
    const float o = sun;
    const float tone = 0.35f;   // fixed colour of the pad (the knob is now ATTACK)
    sCut.setTargetValue (120.0f + o * o * 9000.0f);
    sToneA.setTargetValue (tone * 0.65f);
    sToneB.setTargetValue ((1.0f - tone) * 1.1f);
    sSub.setTargetValue (juce::jmax (0.0f, tone - 0.5f) * 0.5f + snd.subLevel * 0.3f);
    sQ.setTargetValue (0.6f + tone * tone * 5.0f);
    sWow.setTargetValue (weather * weather * 40.0f);
    sSpread.setTargetValue (2.0f + width * width * 43.0f);
    sWet.setTargetValue (bloom * 0.9f);
    sDry.setTargetValue (1.0f - bloom * 0.5f);
    sShim.setTargetValue (bloom * bloom * 0.22f);
    sFb.setTargetValue (glow * 0.6f);
    sEcho.setTargetValue (glow * 0.55f);
    sChorus.setTargetValue (drift * 0.7f);
    sHiss.setTargetValue (dust * dust * 0.07f);
    sLight.setTargetValue (light * light * 0.11f);
    sBreeze.setTargetValue (light * light * 0.05f);
    sOut.setTargetValue (out * out * 1.6f);

    chorus.setDepth (0.25f + drift * 0.5f);
    {
        if (std::abs (lastLight - light) > 0.01f)
        {
            lastLight = light;
            airL.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 6000.0, 0.7, juce::Decibels::decibelsToGain (light * 4.0f));
            *airR.coefficients = *airL.coefficients;
        }
    }

    const float attackT = 0.005f + attack * attack * 8.0f;   // 5 ms .. 8 s
    const float releaseT = 0.5f + glow * glow * 14.0f;
    const float kAtt = timeCoef (attackT / 3.0f, sr);
    const float kRel = timeCoef (releaseT / 4.0f, sr);
    const float kCloudDown = timeCoef (0.06f, sr), kCloudUp = timeCoef (0.3f, sr);
    const int tickLen = (int) (sr * 0.033);
    const int delaySamps = (int) (sr * 0.42);
    const float echoLpCoef = 1.0f - std::exp (-twoPi * 2500.0 / sr);

    auto midiIt = midi.cbegin();
    auto* padL = padBuf.getWritePointer (0); auto* padR = padBuf.getWritePointer (1);
    auto* revL = revBuf.getWritePointer (0); auto* revR = revBuf.getWritePointer (1);
    auto* dryL = dryBuf.getWritePointer (0); auto* dryR = dryBuf.getWritePointer (1);

    // pass 1: voices, events, filter ------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto m = (*midiIt).getMessage();
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v.releasing = true;
            ++midiIt;
        }

        // ticks for random events (≈ every 33 ms, like the prototype)
        if (++eventCounter >= tickLen)
        {
            eventCounter = 0;
            if (rng.nextFloat() < birds * birds * 0.35f) triggerBird();
            if (rng.nextFloat() < drops * drops * 0.12f) triggerDrop();
            if (rng.nextFloat() < dust * dust * 0.9f) triggerCrackle();
            if (cloudHold <= 0 && rng.nextFloat() < weather * weather * 0.06f)
            {
                cloudTarget = 0.05f + rng.nextFloat() * 0.2f;
                cloudHold = (int) ((0.2f + rng.nextFloat() * 0.8f) * sr);
            }
        }
        if (cloudHold > 0 && --cloudHold == 0) cloudTarget = 1.0f;
        cloudFactor += (cloudTarget - cloudFactor) * (cloudTarget < cloudFactor ? kCloudDown : kCloudUp);

        const float toneA = sToneA.getNextValue(), toneB = sToneB.getNextValue(), sub = sSub.getNextValue();
        const float wow = sWow.getNextValue(), spread = sSpread.getNextValue();
        const float shimG = sShim.getNextValue(), lightG = sLight.getNextValue();
        const float cut = sCut.getNextValue(), q = sQ.getNextValue();

        wowPhase = wrap (wowPhase + 0.55 / sr);
        const double wowRatio = std::exp2 (wow * std::sin (wowPhase * twoPi) / 1200.0);
        const double spreadRatio = std::exp2 (spread / 1200.0);

        float L = 0, R = 0, shim = 0, lightSum = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.releasing) { v.env += (0.0f - v.env) * kRel; if (v.env < 1.0e-4f) { v.active = false; continue; } }
            else             v.env += (1.0f - v.env) * kAtt;

            const double f = v.freq * wowRatio;
            const double fl = f / spreadRatio, fr = f * spreadRatio;
            const double dl = fl / sr, dr = fr / sr;
            const float aL = osc (snd.waveA, v.ph[0], dl), aR = osc (snd.waveA, v.ph[1], dr);
            const float bL = osc (snd.waveB, v.ph[2], dl), bR = osc (snd.waveB, v.ph[3], dr);
            v.ph[0] = wrap (v.ph[0] + dl); v.ph[1] = wrap (v.ph[1] + dr);
            v.ph[2] = wrap (v.ph[2] + dl); v.ph[3] = wrap (v.ph[3] + dr);
            const double ds = f * 0.5 / sr;
            const float subS = osc (3, v.ph[4], ds); v.ph[4] = wrap (v.ph[4] + ds);
            const float shS = (float) std::sin (v.ph[5] * twoPi); v.ph[5] = wrap (v.ph[5] + f * 2.0 / sr);
            const float liS = (float) std::sin (v.ph[6] * twoPi); v.ph[6] = wrap (v.ph[6] + f * 4.0 / sr);
            v.lightLfo = wrap (v.lightLfo + v.lightRate / sr);
            const float lfo = 0.5f + 0.5f * (float) std::sin (v.lightLfo * twoPi);

            const float g = v.env * (0.6f + 0.4f * v.velocity) * 0.07f;
            const float left  = aL * toneA + bL * toneB;
            const float right = aR * toneA + bR * toneB;
            L += g * (left * 0.75f + right * 0.25f + subS * sub);
            R += g * (right * 0.75f + left * 0.25f + subS * sub);
            shim += g * shS * shimG * 4.0f;
            lightSum += g * liS * lfo * lightG * 6.0f;
        }

        if ((i & 15) == 0)
        {
            filter.setCutoffFrequency (juce::jlimit (60.0f, (float) (sr * 0.45), cut * cloudFactor));
            filter.setResonance (q);
        }
        padL[i] = filter.processSample (0, L);
        padR[i] = filter.processSample (1, R);

        // one-shot birds / drops
        float bl = 0, br = 0;
        for (auto& b : blips)
        {
            if (! b.active) continue;
            b.t += (float) (1.0 / sr);
            if (b.t < 0) continue;
            float fr;
            if (b.t < b.sweep) fr = b.f0 * std::pow (b.f1 / b.f0, b.t / b.sweep);
            else fr = b.f1 + (b.f2 - b.f1) * (1.0f - std::exp (-(b.t - b.sweep) / 0.05f));
            b.phase = wrap (b.phase + fr / sr);
            float env = b.t < b.attack ? b.t / b.attack : std::exp (-(b.t - b.attack) / (b.decay / 9.2f));
            if (b.t > b.attack + b.decay) { b.active = false; continue; }
            const float s = (float) std::sin (b.phase * twoPi) * env * b.amp;
            bl += s * (1.0f - b.pan) * 0.5f;
            br += s * (1.0f + b.pan) * 0.5f;
        }

        // dust & breeze noise
        const float noise1 = rng.nextFloat() * 2.0f - 1.0f;
        const float noise2 = rng.nextFloat() * 2.0f - 1.0f;
        const float hiss = hissBP.processSample (noise1) * sHiss.getNextValue();
        const float breeze = breezeBP.processSample (noise2) * sBreeze.getNextValue();
        crackleEnv *= 0.985f;
        const float crackle = (rng.nextFloat() * 2.0f - 1.0f) * crackleEnv * crackleAmp;

        // stash for pass 2
        revL[i] = shim + bl + lightSum + breeze;
        revR[i] = shim + br + lightSum + breeze;
        dryL[i] = bl + lightSum + hiss + crackle;
        dryR[i] = br + lightSum + hiss + crackle;
    }

    // chorus (DRIFT) on a copy of the pad --------------------------------------
    choBuf.copyFrom (0, 0, padBuf, 0, 0, n);
    choBuf.copyFrom (1, 0, padBuf, 1, 0, n);
    {
        juce::dsp::AudioBlock<float> blk (choBuf.getArrayOfWritePointers(), 2, (size_t) n);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }
    auto* choL = choBuf.getReadPointer (0); auto* choR = choBuf.getReadPointer (1);

    // pass 2: echo (AFTERGLOW) and sends --------------------------------------
    const int dlen = (int) delayL.size();
    for (int i = 0; i < n; ++i)
    {
        const float chG = sChorus.getNextValue(), fb = sFb.getNextValue(), echoG = sEcho.getNextValue(), dryG = sDry.getNextValue();
        int rp = delayPos - delaySamps; if (rp < 0) rp += dlen;
        echoLpL += (delayL[(size_t) rp] - echoLpL) * echoLpCoef;
        echoLpR += (delayR[(size_t) rp] - echoLpR) * echoLpCoef;
        // delay input: the filtered pad, fed back through a soft low-pass
        delayL[(size_t) delayPos] = padL[i] + echoLpL * fb;
        delayR[(size_t) delayPos] = padR[i] + echoLpR * fb;
        if (++delayPos >= dlen) delayPos = 0;

        const float cL = choL[i] * chG, cR = choR[i] * chG;
        revL[i] = padL[i] + cL * 0.5f + revL[i] + echoLpL * 0.5f;
        revR[i] = padR[i] + cR * 0.5f + revR[i] + echoLpR * 0.5f;
        dryL[i] = padL[i] * dryG + cL + dryL[i] + echoLpL * echoG;
        dryR[i] = padR[i] * dryG + cR + dryR[i] + echoLpR * echoG;
    }

    // reverb (BLOOM) ----------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> blk (revBuf.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // mix, air, glue, output --------------------------------------------------
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float wet = sWet.getNextValue();
        outL[i] = airL.processSample (dryL[i] + revBuf.getSample (0, i) * wet);
        outR[i] = airR.processSample (dryR[i] + revBuf.getSample (1, i) * wet);
    }
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        comp.process (ctx);
    }
    for (int i = 0; i < n; ++i)
    {
        const float g = sOut.getNextValue();
        outL[i] *= g; outR[i] *= g;
    }
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        limiter.process (ctx);
    }

    // scope
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        scope[(size_t) w] = 0.5f * (outL[i] + outR[i]);
        w = (w + 1) & (scopeSize - 1);
    }
    scopeWrite.store (w, std::memory_order_release);
}

void SunrizeAudioProcessor::copyScope (float* dest, int num) const
{
    num = juce::jmin (num, scopeSize);
    int r = scopeWrite.load (std::memory_order_acquire) - num;
    if (r < 0) r += scopeSize;
    for (int i = 0; i < num; ++i)
        dest[i] = scope[(size_t) ((r + i) & (scopeSize - 1))];
}

//==============================================================================
void SunrizeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SunrizeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SunrizeAudioProcessor::createEditor()
{
    return new SunrizeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SunrizeAudioProcessor();
}
