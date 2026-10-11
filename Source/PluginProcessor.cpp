#include "PluginProcessor.h"
#include "PluginEditor.h"

RDSBAudioProcessor::RDSBAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

RDSBAudioProcessor::~RDSBAudioProcessor()
{
}

const juce::String RDSBAudioProcessor::getName() const
{
    return "Rise.Drop.Sweep.Boom";
}

bool RDSBAudioProcessor::acceptsMidi() const
{
    return true;
}

bool RDSBAudioProcessor::producesMidi() const
{
    return false;
}

bool RDSBAudioProcessor::isMidiEffect() const
{
    return false;
}

double RDSBAudioProcessor::getTailLengthSeconds() const
{
    return 4.0; // Reverb/Boom tail headroom
}

int RDSBAudioProcessor::getNumPrograms()
{
    return 1;
}

int RDSBAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RDSBAudioProcessor::setCurrentProgram(int)
{
}

const juce::String RDSBAudioProcessor::getProgramName(int)
{
    return "Default";
}

void RDSBAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void RDSBAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    tonalSweeper.prepare(sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    noiseEngine.prepare(spec);
    boomEngine.prepare(spec);
    phaser.prepare(spec);
    ladderFilter.prepare(spec);

    reverb.reset();
    phaser.reset();
    ladderFilter.reset();

    masterPhase = 0.0f;
    isTransitionPlaying = false;
    wasHostPlaying = false;
    boomOnNextBlock = false;
}

void RDSBAudioProcessor::releaseResources()
{
}

bool RDSBAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

namespace {
// Transparent below 0.9; above it eases into a hard ceiling at 1.0, so stacking
// the engines (or +6 dB of master) can never clip the host.
inline float softLimit(float x) {
    constexpr float threshold = 0.9f, room = 1.0f - threshold;
    const float a = std::abs(x);
    if (a <= threshold)
        return x;
    return std::copysign(threshold + room * std::tanh((a - threshold) / room), x);
}
} // namespace

void RDSBAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const double fs = currentSampleRate > 0.0 ? currentSampleRate : getSampleRate();

    buffer.clear();
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // --- parameters (read once per block)
    const bool downlifter = apvts.getRawParameterValue("mode")->load() > 0.5f;
    const bool midiSync = apvts.getRawParameterValue("sync_mode")->load() > 0.5f;
    const bool boomOnKey = apvts.getRawParameterValue("boom_trigger_mode")->load() > 0.5f;
    const float durationBars = apvts.getRawParameterValue("duration_bars")->load();
    const float warpK = apvts.getRawParameterValue("warp_k")->load();

    // --- host timing
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false, haveBeat = false;
    int sigNum = 4, sigDen = 4;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue()) bpm = *pos->getBpm();
            hostPlaying = pos->getIsPlaying();
            if (pos->getPpqPosition().hasValue()) { ppq = *pos->getPpqPosition(); haveBeat = true; }
            if (pos->getTimeSignature().hasValue()) {
                sigNum = juce::jmax(1, pos->getTimeSignature()->numerator);
                sigDen = juce::jmax(1, pos->getTimeSignature()->denominator);
            }
        }
    }
    bpm = juce::jmax(1.0, bpm);
    const double beatsPerSample = bpm / (60.0 * fs);                  // quarter-note beats
    const double barBeats = sigNum * 4.0 / sigDen;                    // quarter notes per bar
    const double transitionBeats = juce::jmax(1.0e-3, durationBars * barBeats);

    // --- plan this block: up to two sweep segments plus sample-accurate boom triggers
    struct Segment { int start, count; double phase, inc; };
    Segment segments[2];
    int numSegments = 0;
    int autoBoomAt = -1;           // sample index of the drop, or -1
    int midiBoomAt = -1;           // sample index of a MIDI-key boom, or -1

    if (boomOnNextBlock) { autoBoomAt = 0; boomOnNextBlock = false; }

    // Note-ons: remember the key (transpose) and sample position.
    int midiStartAt = -1;
    for (const auto metadata : midiMessages) {
        const auto m = metadata.getMessage();
        if (m.isNoteOn()) {
            triggerMidiNote = m.getNoteNumber();
            if (midiSync) midiStartAt = metadata.samplePosition;
            if (boomOnKey && midiBoomAt < 0) midiBoomAt = metadata.samplePosition;
        }
    }

    if (midiSync) {
        // One-shot: runs from the note-on for its length, then (riser) drops.
        const double inc = beatsPerSample / transitionBeats;           // phase per sample
        int from = 0;
        if (midiStartAt >= 0) {
            masterPhase = 0.0f;
            isTransitionPlaying = true;
            tonalSweeper.resetPumper();
            from = midiStartAt;
            if (downlifter && !boomOnKey) autoBoomAt = from;          // downlifters drop at the start
        }
        if (isTransitionPlaying) {
            const double remaining = 1.0 - static_cast<double>(masterPhase);
            const int samplesLeft = static_cast<int>(std::ceil(remaining / inc));
            const int count = juce::jmin(numSamples - from, samplesLeft);
            if (count > 0) {
                segments[numSegments++] = { from, count, static_cast<double>(masterPhase), inc };
                masterPhase = static_cast<float>(juce::jmin(1.0, masterPhase + inc * count));
            }
            if (count >= samplesLeft) {                                // reached the end
                isTransitionPlaying = false;
                masterPhase = 1.0f;
                if (!downlifter && !boomOnKey) {
                    const int at = from + count;
                    if (at < numSamples) autoBoomAt = at; else boomOnNextBlock = true;
                }
            }
        }
        wasHostPlaying = false;
    } else {
        // Transport-synced: the sweep is locked to the host's bar grid.
        if (hostPlaying && haveBeat) {
            const double inc = beatsPerSample / transitionBeats;
            const double startFrac = std::fmod(ppq, transitionBeats) / transitionBeats;

            if (!wasHostPlaying && downlifter && !boomOnKey && startFrac < 1.0e-3)
                autoBoomAt = 0;   // playback began exactly on the loop start

            // Loop boundaries are found by loop *index*, so a boundary that falls
            // exactly on a block edge (or one rounding step either side) still fires once.
            constexpr double eps = 1.0e-9;
            auto loopIndex = [&](double beat) { return static_cast<long long>(std::floor(beat / transitionBeats + eps)); };
            const long long idx0 = loopIndex(ppq);
            const long long idxEnd = loopIndex(ppq + beatsPerSample * numSamples);

            int wrapAt = -1;
            if (haveLoopIndex && idx0 > lastLoopIndex) {
                wrapAt = 0;                                        // boundary was exactly at (or just before) this block
            } else if (idxEnd > idx0) {
                const double boundaryBeat = static_cast<double>(idx0 + 1) * transitionBeats;
                const int at = static_cast<int>(std::ceil((boundaryBeat - ppq) / beatsPerSample - 1.0e-9));
                wrapAt = juce::jlimit(0, numSamples, at);
                if (wrapAt >= numSamples) wrapAt = -1;             // lands on the next block's first sample
            }
            lastLoopIndex = (wrapAt >= 0 && idxEnd > idx0) ? idxEnd : idx0;
            haveLoopIndex = true;

            if (wrapAt > 0) {
                segments[numSegments++] = { 0, wrapAt, startFrac, inc };
                segments[numSegments++] = { wrapAt, numSamples - wrapAt, 0.0, inc };
                masterPhase = static_cast<float>(inc * (numSamples - wrapAt));
            } else if (wrapAt == 0) {
                segments[numSegments++] = { 0, numSamples, 0.0, inc };
                masterPhase = static_cast<float>(std::fmod(inc * numSamples, 1.0));
            } else {
                segments[numSegments++] = { 0, numSamples, startFrac, inc };
                masterPhase = static_cast<float>(std::fmod(startFrac + inc * numSamples, 1.0));
            }
            if (wrapAt >= 0) {
                if (!boomOnKey) autoBoomAt = wrapAt;
                tonalSweeper.resetPumper();
            }
            isTransitionPlaying = true;
            wasHostPlaying = true;
        } else {
            isTransitionPlaying = false;
            wasHostPlaying = false;
            haveLoopIndex = false;
            masterPhase = 0.0f;
        }
    }

    // Boom triggers land on their exact sample.
    if (autoBoomAt >= 0) boomEngine.trigger(autoBoomAt);
    if (midiBoomAt >= 0) boomEngine.trigger(midiBoomAt);

    // --- render the sweep segments
    const int noteOffset = triggerMidiNote - 36;
    const float tonalVol = apvts.getRawParameterValue("tonal_volume")->load();
    const float noiseVol = apvts.getRawParameterValue("noise_volume")->load();
    const float pitchStart = juce::jlimit(0.0f, 127.0f, apvts.getRawParameterValue("tonal_pitch_start")->load() + noteOffset);
    const float pitchEnd = juce::jlimit(0.0f, 127.0f, apvts.getRawParameterValue("tonal_pitch_end")->load() + noteOffset);
    const int unison = static_cast<int>(apvts.getRawParameterValue("tonal_unison")->load());
    const float detune = apvts.getRawParameterValue("tonal_detune")->load();
    const float spread = apvts.getRawParameterValue("tonal_pan_spread")->load();
    const float pumpDepth = apvts.getRawParameterValue("pumper_depth")->load();
    const float pumpStart = apvts.getRawParameterValue("pumper_rate_start")->load();
    const float pumpEnd = apvts.getRawParameterValue("pumper_rate_end")->load();
    const bool arpOn = apvts.getRawParameterValue("arp_enable")->load() > 0.5f;
    const int arpRate = static_cast<int>(apvts.getRawParameterValue("arp_rate")->load());
    const int arpPattern = static_cast<int>(apvts.getRawParameterValue("arp_pattern")->load());
    const int noiseType = static_cast<int>(apvts.getRawParameterValue("noise_type")->load());
    const int noiseFilterMode = static_cast<int>(apvts.getRawParameterValue("noise_filter_mode")->load());
    const float cutoffStart = apvts.getRawParameterValue("noise_cutoff_start")->load();
    const float cutoffEnd = apvts.getRawParameterValue("noise_cutoff_end")->load();
    const float noiseQ = apvts.getRawParameterValue("noise_filter_q")->load();

    for (int i = 0; i < numSegments; ++i) {
        const auto& seg = segments[i];
        // Downlifters sweep the same curve backwards.
        const double phaseStart = downlifter ? 1.0 - seg.phase : seg.phase;
        const double phaseInc = downlifter ? -seg.inc : seg.inc;

        if (tonalVol > 0.0f)
            tonalSweeper.processBlock(buffer, seg.start, seg.count, phaseStart, phaseInc, warpK, tonalVol,
                                      pitchStart, pitchEnd, unison, detune, spread,
                                      pumpDepth, pumpStart, pumpEnd, beatsPerSample, transitionBeats, downlifter,
                                      arpOn, arpRate, arpPattern);
        if (noiseVol > 0.0f)
            noiseEngine.processBlock(buffer, seg.start, seg.count, phaseStart, phaseInc, warpK, noiseVol,
                                     noiseType, noiseFilterMode, cutoffStart, cutoffEnd, noiseQ);
    }

    // --- boom
    if (boomEngine.isActive()) {
        const float freqMultiplier = std::pow(2.0f, static_cast<float>(noteOffset) / 12.0f);
        boomEngine.processBlock(buffer,
                                apvts.getRawParameterValue("boom_volume")->load(),
                                apvts.getRawParameterValue("boom_noise_decay")->load(),
                                apvts.getRawParameterValue("boom_sub_decay")->load(),
                                juce::jlimit(40.0f, 1000.0f, apvts.getRawParameterValue("boom_sub_pitch_start")->load() * freqMultiplier),
                                juce::jlimit(20.0f, 200.0f, apvts.getRawParameterValue("boom_sub_pitch_end")->load() * freqMultiplier));
    }

    // --- FX sweeps: follow the sweep position in short chunks
    auto phaseAt = [&](int sample) -> double {
        for (int i = 0; i < numSegments; ++i) {
            const auto& seg = segments[i];
            if (sample >= seg.start && sample < seg.start + seg.count)
                return seg.phase + seg.inc * (sample - seg.start);
        }
        return static_cast<double>(masterPhase);
    };

    const bool phaserOn = apvts.getRawParameterValue("phaser_enable")->load() > 0.5f;
    const bool filterOn = apvts.getRawParameterValue("filter_enable")->load() > 0.5f;
    if (phaserOn || filterOn) {
        const bool filterHighpass = apvts.getRawParameterValue("filter_mode")->load() > 0.5f;
        if (phaserOn) {
            phaser.setFeedback(apvts.getRawParameterValue("phaser_feedback")->load());
            phaser.setMix(apvts.getRawParameterValue("phaser_mix")->load());
            phaser.setRate(1.0f);
            phaser.setDepth(0.8f);
        }
        if (filterOn) {
            ladderFilter.setMode(filterHighpass ? juce::dsp::LadderFilterMode::HPF24
                                                : juce::dsp::LadderFilterMode::LPF24);
            ladderFilter.setResonance(apvts.getRawParameterValue("filter_resonance")->load());
        }

        constexpr int chunk = 64;
        for (int offset = 0; offset < numSamples; offset += chunk) {
            const int n = juce::jmin(chunk, numSamples - offset);
            const double raw = juce::jlimit(0.0, 1.0, phaseAt(offset + n / 2));
            const float active = static_cast<float>(downlifter ? 1.0 - raw : raw);
            const float warped = std::pow(active, warpK);

            float* chans[2] = { buffer.getWritePointer(0) + offset, buffer.getWritePointer(1) + offset };
            juce::dsp::AudioBlock<float> block(chans, 2, static_cast<size_t>(n));
            juce::dsp::ProcessContextReplacing<float> context(block);

            if (phaserOn) {
                phaser.setCentreFrequency(400.0f + 1600.0f * warped);
                phaser.process(context);
            }
            if (filterOn) {
                const float cutoff = filterHighpass ? 20000.0f - (20000.0f - 30.0f) * warped
                                                    : 100.0f + (20000.0f - 100.0f) * warped;
                ladderFilter.setCutoffFrequencyHz(juce::jlimit(20.0f, 20000.0f, cutoff));
                ladderFilter.process(context);
            }
        }
    }

    // --- saturation, master gain, reverb, safety limiter
    const float drive = apvts.getRawParameterValue("saturation_drive")->load();
    if (drive > 1.0f) {
        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);
        const float scaleComp = 1.0f / std::tanh(drive);
        for (int i = 0; i < numSamples; ++i) {
            left[i] = std::tanh(drive * left[i]) * scaleComp;
            right[i] = std::tanh(drive * right[i]) * scaleComp;
        }
    }

    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("master_volume")->load()));

    const float revMix = apvts.getRawParameterValue("reverb_mix")->load();
    if (revMix > 0.0f) {
        juce::Reverb::Parameters revParams;
        revParams.roomSize = apvts.getRawParameterValue("reverb_size")->load();
        revParams.width = apvts.getRawParameterValue("reverb_width")->load();
        revParams.wetLevel = revMix;
        revParams.dryLevel = 1.0f - revMix;
        revParams.damping = 0.5f;
        reverb.setParameters(revParams);
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);
    for (int i = 0; i < numSamples; ++i) {
        outL[i] = softLimit(outL[i]);
        outR[i] = softLimit(outR[i]);
    }
}

void RDSBAudioProcessor::triggerTransition()
{
    // Called from the UI TRIGGER button: start a transition on the next block.
    masterPhase = 0.0f;
    isTransitionPlaying = true;
    tonalSweeper.resetPumper();

    const bool downlifter = apvts.getRawParameterValue("mode")->load() > 0.5f;
    const bool boomOnKey = apvts.getRawParameterValue("boom_trigger_mode")->load() > 0.5f;
    if (downlifter && !boomOnKey)
        boomOnNextBlock = true;
}

bool RDSBAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* RDSBAudioProcessor::createEditor()
{
    return new RDSBAudioProcessorEditor(*this);
}

void RDSBAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RDSBAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout RDSBAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global Params
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray("Riser", "Downlifter"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("sync_mode", 1), "Sync Mode", juce::StringArray("Transport Synced", "MIDI Triggered"), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("duration_bars", 1), "Bar Length", 1.0f, 32.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("warp_k", 1), "Warp Curve (k)", juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("master_volume", 1), "Master Gain (dB)", juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f));

    // Tonal Sweeper Params
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tonal_volume", 1), "Tonal Volume", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tonal_pitch_start", 1), "Tonal Start Pitch (MIDI)", 24.0f, 108.0f, 36.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tonal_pitch_end", 1), "Tonal End Pitch (MIDI)", 24.0f, 108.0f, 84.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("tonal_unison", 1), "Tonal Unison Voices", 1, 7, 5));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tonal_detune", 1), "Tonal Detune", 0.0f, 0.1f, 0.02f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tonal_pan_spread", 1), "Tonal Stereo Width", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pumper_depth", 1), "Pumper Depth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pumper_rate_start", 1), "Pumper Rate Start (per beat)", 0.25f, 8.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pumper_rate_end", 1), "Pumper Rate End (per beat)", 0.25f, 8.0f, 4.0f));

    // Arpeggiator Params
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_enable", 1), "Arpeggiator Enable", juce::StringArray("Off", "On"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_rate", 1), "Arpeggiator Rate", juce::StringArray("1/8", "1/16", "1/32"), 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_pattern", 1), "Arpeggiator Pattern", juce::StringArray("Up", "Down", "Up-Down", "Octaves"), 0));

    // Noise Engine Params
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise_volume", 1), "Noise Volume", 0.0f, 1.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("noise_type", 1), "Noise Type", juce::StringArray("White Noise", "Pink Noise"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("noise_filter_mode", 1), "Noise Filter Mode", juce::StringArray("Bandpass", "Highpass"), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise_cutoff_start", 1), "Noise Cutoff Start (Hz)", 20.0f, 20000.0f, 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise_cutoff_end", 1), "Noise Cutoff End (Hz)", 20.0f, 20000.0f, 15000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise_filter_q", 1), "Noise Filter Q", 0.1f, 10.0f, 1.0f));

    // Boom Engine Params
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boom_volume", 1), "Boom Volume", 0.0f, 1.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("boom_trigger_mode", 1), "Boom Trigger Mode", juce::StringArray("Auto (Drop)", "MIDI Key"), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boom_noise_decay", 1), "Boom Punch Decay (s)", 0.05f, 0.5f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boom_sub_decay", 1), "Boom Rumble Decay (s)", 0.5f, 5.0f, 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boom_sub_pitch_start", 1), "Boom Sub Start (Hz)", 100.0f, 300.0f, 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boom_sub_pitch_end", 1), "Boom Sub End (Hz)", 20.0f, 60.0f, 35.0f));

    // FX Params
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("saturation_drive", 1), "Drive (Saturation)", 1.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverb_mix", 1), "Reverb Mix", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverb_size", 1), "Reverb Size", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverb_width", 1), "Reverb Width", 0.0f, 1.0f, 0.5f));

    // Phaser Params
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("phaser_enable", 1), "Phaser Enable", juce::StringArray("Off", "On"), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("phaser_feedback", 1), "Phaser Feedback", 0.0f, 0.95f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("phaser_mix", 1), "Phaser Mix", 0.0f, 1.0f, 0.5f));

    // Ladder Filter Params
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("filter_enable", 1), "Filter Enable", juce::StringArray("Off", "On"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("filter_mode", 1), "Filter Mode", juce::StringArray("Lowpass", "Highpass"), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filter_resonance", 1), "Filter Resonance", 0.0f, 0.95f, 0.5f));

    return { params.begin(), params.end() };
}

// Boilerplate creator function for VST/AU entry points
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RDSBAudioProcessor();
}
