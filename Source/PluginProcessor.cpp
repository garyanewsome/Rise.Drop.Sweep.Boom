#include "PluginProcessor.h"
#include "PluginEditor.h"

TransitionEngineAudioProcessor::TransitionEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

TransitionEngineAudioProcessor::~TransitionEngineAudioProcessor()
{
}

const juce::String TransitionEngineAudioProcessor::getName() const
{
    return "Rise.Drop.Sweep.Boom";
}

bool TransitionEngineAudioProcessor::acceptsMidi() const
{
    return true;
}

bool TransitionEngineAudioProcessor::producesMidi() const
{
    return false;
}

bool TransitionEngineAudioProcessor::isMidiEffect() const
{
    return false;
}

double TransitionEngineAudioProcessor::getTailLengthSeconds() const
{
    return 4.0; // Reverb/Boom tail headroom
}

int TransitionEngineAudioProcessor::getNumPrograms()
{
    return 1;
}

int TransitionEngineAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TransitionEngineAudioProcessor::setCurrentProgram(int)
{
}

const juce::String TransitionEngineAudioProcessor::getProgramName(int)
{
    return "Default";
}

void TransitionEngineAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void TransitionEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
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
}

void TransitionEngineAudioProcessor::releaseResources()
{
}

bool TransitionEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void TransitionEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    int numSamples = buffer.getNumSamples();

    // 1. Clear the output buffer (we generate sound from scratch)
    buffer.clear();

    // Feed on-screen keyboard midi notes into the buffer
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // Get parameters
    auto mode = apvts.getRawParameterValue("mode")->load(); // 0 = Riser, 1 = Downlifter
    auto syncMode = apvts.getRawParameterValue("sync_mode")->load(); // 0 = Transport, 1 = MIDI
    auto durationBars = apvts.getRawParameterValue("duration_bars")->load();
    auto boomTrigger = apvts.getRawParameterValue("boom_trigger_mode")->load(); // 0 = Auto, 1 = MIDI

    // Parse incoming MIDI to trigger transitions or boom
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            triggerMidiNote = message.getNoteNumber();

            if (syncMode == 1.0f) // MIDI Triggered Mode
            {
                triggerTransition();
            }
            else // Transport Synced Mode
            {
                if (boomTrigger == 1.0f) // Manual MIDI Boom
                {
                    boomEngine.trigger();
                }
            }

            // Note-on always triggers boom in MIDI Boom mode regardless of sync mode
            if (boomTrigger == 1.0f)
            {
                boomEngine.trigger();
            }
        }
    }

    // Get host tempo and timing details
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        auto posInfo = ph->getPosition();
        if (posInfo.hasValue() && posInfo->getBpm().hasValue())
        {
            bpm = *posInfo->getBpm();
        }
    }

    // 2. Phase accumulation & transport sync logic
    if (syncMode == 1.0f) // MIDI Triggered
    {
        if (isTransitionPlaying)
        {
            // Advance phase based on sample duration and sample rate
            double fs = getSampleRate();
            double deltaPhi = bpm / (240.0 * fs * durationBars);
            
            masterPhase += static_cast<float>(deltaPhi * numSamples);

            if (masterPhase >= 1.0f)
            {
                masterPhase = 1.0f;
                isTransitionPlaying = false;

                // Riser auto-detonates boom at terminal capacity
                if (mode == 0.0f && boomTrigger == 0.0f)
                {
                    boomEngine.trigger();
                }
            }
        }
    }
    else // Transport Synced
    {
        if (auto* ph = getPlayHead())
        {
            auto posInfo = ph->getPosition();
            if (posInfo.hasValue())
            {
                bool hostIsPlaying = posInfo->getIsPlaying();
                auto optBeat = posInfo->getPpqPosition();

                if (hostIsPlaying && optBeat.hasValue())
                {
                    double currentBeat = *optBeat;
                    double beatsPerBar = 4.0;
                    if (posInfo->getTimeSignature().hasValue())
                    {
                        beatsPerBar = posInfo->getTimeSignature()->numerator;
                    }

                    double transitionBeats = durationBars * beatsPerBar;
                    double rawPhase = std::fmod(currentBeat, transitionBeats) / transitionBeats;
                    if (rawPhase < 0.0) rawPhase += 1.0;

                    float newPhase = static_cast<float>(rawPhase);

                    // Check for drop events
                    if (wasHostPlaying)
                    {
                        float diff = newPhase - masterPhase;
                        if (diff < -0.5f) // Wrap-around loop boundary
                        {
                            if (boomTrigger == 0.0f)
                            {
                                boomEngine.trigger(); // Trigger for both risers (at end) and downlifters (at start of next loop)
                            }
                        }
                    }
                    else
                    {
                        // Playback just started: Trigger Downlifter boom immediately
                        if (mode == 1.0f && boomTrigger == 0.0f)
                        {
                            boomEngine.trigger();
                        }
                    }

                    masterPhase = newPhase;
                    isTransitionPlaying = true;
                    wasHostPlaying = hostIsPlaying;
                }
                else
                {
                    isTransitionPlaying = false;
                    wasHostPlaying = false;
                    masterPhase = 0.0f;
                }
            }
        }
        else
        {
            isTransitionPlaying = false;
            wasHostPlaying = false;
            masterPhase = 0.0f;
        }
    }

    // Determine active phase based on Mode (Riser = masterPhase, Downlifter = 1.0 - masterPhase)
    float activePhase = (mode == 1.0f) ? (1.0f - masterPhase) : masterPhase;
    auto warpK = apvts.getRawParameterValue("warp_k")->load();

    // Calculate sample-accurate phase start and increment for this block
    double fs = getSampleRate();
    double deltaPhi = (isTransitionPlaying && fs > 0.0) ? (bpm / (240.0 * fs * durationBars)) : 0.0;
    if (syncMode == 0.0f && (!wasHostPlaying || !isTransitionPlaying)) {
        deltaPhi = 0.0;
    }

    double activePhaseStart = (mode == 1.0f) ? (1.0f - masterPhase) : masterPhase;
    double activePhaseIncrement = (mode == 1.0f) ? -deltaPhi : deltaPhi;

    // 3. Render Tonal Sweeper
    if (isTransitionPlaying)
    {
        auto tonalVol = apvts.getRawParameterValue("tonal_volume")->load();
        auto pitchStart = apvts.getRawParameterValue("tonal_pitch_start")->load();
        auto pitchEnd = apvts.getRawParameterValue("tonal_pitch_end")->load();

        // Calculate pitch shift relative to default C2 (MIDI 36)
        int noteOffset = triggerMidiNote - 36;
        float shiftedPitchStart = juce::jlimit(0.0f, 127.0f, pitchStart + noteOffset);
        float shiftedPitchEnd = juce::jlimit(0.0f, 127.0f, pitchEnd + noteOffset);

        auto unison = apvts.getRawParameterValue("tonal_unison")->load();
        auto detune = apvts.getRawParameterValue("tonal_detune")->load();
        auto spread = apvts.getRawParameterValue("tonal_pan_spread")->load();
        auto pumpDepth = apvts.getRawParameterValue("pumper_depth")->load();
        auto pumpStart = apvts.getRawParameterValue("pumper_rate_start")->load();
        auto pumpEnd = apvts.getRawParameterValue("pumper_rate_end")->load();

        // Arpeggiator params
        auto arpEnable = apvts.getRawParameterValue("arp_enable")->load() == 1.0f;
        auto arpRate = static_cast<int>(apvts.getRawParameterValue("arp_rate")->load());
        auto arpPattern = static_cast<int>(apvts.getRawParameterValue("arp_pattern")->load());

        tonalSweeper.processBlock(buffer, activePhaseStart, activePhaseIncrement, warpK, tonalVol,
                                  shiftedPitchStart, shiftedPitchEnd, unison, detune, spread,
                                  pumpDepth, pumpStart, pumpEnd, durationBars,
                                  arpEnable, arpRate, arpPattern);
    }

    // 4. Render Noise Engine
    if (isTransitionPlaying)
    {
        auto noiseVol = apvts.getRawParameterValue("noise_volume")->load();
        auto noiseType = apvts.getRawParameterValue("noise_type")->load();
        auto filterMode = apvts.getRawParameterValue("noise_filter_mode")->load();
        auto cutoffStart = apvts.getRawParameterValue("noise_cutoff_start")->load();
        auto cutoffEnd = apvts.getRawParameterValue("noise_cutoff_end")->load();
        auto filterQ = apvts.getRawParameterValue("noise_filter_q")->load();

        noiseEngine.processBlock(buffer, activePhase, warpK, noiseVol,
                                 static_cast<int>(noiseType), static_cast<int>(filterMode),
                                 cutoffStart, cutoffEnd, filterQ);
    }

    // 5. Render Boom Engine
    if (boomEngine.isActive())
    {
        auto boomVol = apvts.getRawParameterValue("boom_volume")->load();
        auto noiseDecay = apvts.getRawParameterValue("boom_noise_decay")->load();
        auto subDecay = apvts.getRawParameterValue("boom_sub_decay")->load();
        auto subPitchStart = apvts.getRawParameterValue("boom_sub_pitch_start")->load();
        auto subPitchEnd = apvts.getRawParameterValue("boom_sub_pitch_end")->load();

        // Sub-bass pitch tracks the key pressed (relative to C2 / note 36)
        int noteOffset = triggerMidiNote - 36;
        float freqMultiplier = std::pow(2.0f, static_cast<float>(noteOffset) / 12.0f);
        float shiftedSubPitchStart = juce::jlimit(40.0f, 1000.0f, subPitchStart * freqMultiplier);
        float shiftedSubPitchEnd = juce::jlimit(20.0f, 200.0f, subPitchEnd * freqMultiplier);

        boomEngine.processBlock(buffer, boomVol, noiseDecay, subDecay, shiftedSubPitchStart, shiftedSubPitchEnd);
    }

    // 5.5. Render Phaser & Ladder Filter FX
    auto phaserEnabled = apvts.getRawParameterValue("phaser_enable")->load();
    if (phaserEnabled == 1.0f)
    {
        auto phaserFeedback = apvts.getRawParameterValue("phaser_feedback")->load();
        auto phaserMix = apvts.getRawParameterValue("phaser_mix")->load();

        phaser.setFeedback(phaserFeedback);
        phaser.setMix(phaserMix);
        phaser.setRate(1.0f);
        phaser.setDepth(0.8f);
        phaser.setCentreFrequency(400.0f + 1600.0f * std::pow(activePhase, warpK));

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        phaser.process(context);
    }

    auto filterEnabled = apvts.getRawParameterValue("filter_enable")->load();
    if (filterEnabled == 1.0f)
    {
        auto filterMode = apvts.getRawParameterValue("filter_mode")->load();
        auto filterResonance = apvts.getRawParameterValue("filter_resonance")->load();

        if (filterMode == 1.0f)
        {
            ladderFilter.setMode(juce::dsp::LadderFilterMode::HPF24);
            float cutoff = 20000.0f - (20000.0f - 30.0f) * std::pow(activePhase, warpK);
            cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
            ladderFilter.setCutoffFrequencyHz(cutoff);
        }
        else
        {
            ladderFilter.setMode(juce::dsp::LadderFilterMode::LPF24);
            float cutoff = 100.0f + (20000.0f - 100.0f) * std::pow(activePhase, warpK);
            cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
            ladderFilter.setCutoffFrequencyHz(cutoff);
        }

        ladderFilter.setResonance(filterResonance);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        ladderFilter.process(context);
    }

    // 6. Waveshaper (Soft-Clipper)
    auto drive = apvts.getRawParameterValue("saturation_drive")->load();
    if (drive > 1.0f)
    {
        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);
        float scaleComp = 1.0f / std::tanh(drive);

        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = std::tanh(drive * left[i]) * scaleComp;
            right[i] = std::tanh(drive * right[i]) * scaleComp;
        }
    }

    // Apply master volume gain before spatial reverb
    auto masterVol = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("master_volume")->load());
    buffer.applyGain(masterVol);

    // 7. Spatial Reverb Routing
    auto revMix = apvts.getRawParameterValue("reverb_mix")->load();
    if (revMix > 0.0f)
    {
        auto revSize = apvts.getRawParameterValue("reverb_size")->load();
        auto revWidth = apvts.getRawParameterValue("reverb_width")->load();

        juce::Reverb::Parameters revParams;
        revParams.roomSize = revSize;
        revParams.width = revWidth;
        revParams.wetLevel = revMix;
        revParams.dryLevel = 1.0f - revMix;
        revParams.damping = 0.5f;
        reverb.setParameters(revParams);

        float* leftData = buffer.getWritePointer(0);
        float* rightData = buffer.getWritePointer(1);
        reverb.processStereo(leftData, rightData, numSamples);
    }
}

void TransitionEngineAudioProcessor::triggerTransition()
{
    masterPhase = 0.0f;
    isTransitionPlaying = true;

    auto mode = apvts.getRawParameterValue("mode")->load();
    auto boomTrigger = apvts.getRawParameterValue("boom_trigger_mode")->load();

    // Downlifter drops immediately at phase 0.0
    if (mode == 1.0f && boomTrigger == 0.0f)
    {
        boomEngine.trigger();
    }
}

bool TransitionEngineAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TransitionEngineAudioProcessor::createEditor()
{
    return new TransitionEngineAudioProcessorEditor(*this);
}

void TransitionEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TransitionEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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

juce::AudioProcessorValueTreeState::ParameterLayout TransitionEngineAudioProcessor::createParameterLayout()
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
        juce::ParameterID("tonal_volume", 1), "Tonal Volume", 0.0f, 1.0f, 0.7f));
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
        juce::ParameterID("pumper_rate_start", 1), "Pumper Rate Start (Hz)", 1.0f, 32.0f, 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pumper_rate_end", 1), "Pumper Rate End (Hz)", 1.0f, 32.0f, 16.0f));

    // Arpeggiator Params
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_enable", 1), "Arpeggiator Enable", juce::StringArray("Off", "On"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_rate", 1), "Arpeggiator Rate", juce::StringArray("1/8", "1/16", "1/32"), 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("arp_pattern", 1), "Arpeggiator Pattern", juce::StringArray("Up", "Down", "Up-Down", "Octaves"), 0));

    // Noise Engine Params
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise_volume", 1), "Noise Volume", 0.0f, 1.0f, 0.5f));
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
        juce::ParameterID("boom_volume", 1), "Boom Volume", 0.0f, 1.0f, 0.8f));
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
    return new TransitionEngineAudioProcessor();
}
