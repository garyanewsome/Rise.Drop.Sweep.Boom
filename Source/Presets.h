#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

//==============================================================================
// Factory and user presets. A preset is a list of {parameter id, real value};
// anything it does not mention resets to the parameter default, so patches
// never inherit settings from the previous sound. Master gain is never touched.
//==============================================================================
namespace Presets
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    // mode: 0 riser, 1 downlifter.  boom_trigger_mode: 0 auto, 1 MIDI key.
    // boom_position: 0 at drop, 1 at start, 2 at end, 3 start + end.
    // noise_type: 0 white, 1 pink.  noise_filter_mode / filter_mode: 0 band-pass|low-pass, 1 high-pass.
    // arp_rate: 0 1/8, 1 1/16, 2 1/32.  arp_pattern: 0 up, 1 down, 2 up-down, 3 octaves.
    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> presets = {
            { "Init", {} },

            { "8-Bar Riser",
              { { "duration_bars", 8.0f }, { "warp_k", 2.5f },
                { "tonal_volume", 0.5f }, { "tonal_pitch_start", 36.0f }, { "tonal_pitch_end", 84.0f },
                { "tonal_unison", 5 }, { "tonal_detune", 0.02f },
                { "noise_volume", 0.4f }, { "noise_cutoff_start", 200.0f }, { "noise_cutoff_end", 15000.0f } } },

            { "4-Bar Tension",
              { { "duration_bars", 4.0f }, { "warp_k", 3.0f },
                { "tonal_volume", 0.45f }, { "tonal_pitch_start", 40.0f }, { "tonal_pitch_end", 76.0f },
                { "tonal_unison", 7 }, { "tonal_detune", 0.035f },
                { "pumper_depth", 0.6f }, { "pumper_rate_start", 1.0f }, { "pumper_rate_end", 6.0f },
                { "noise_volume", 0.3f }, { "noise_filter_mode", 1 }, { "noise_cutoff_start", 400.0f },
                { "noise_cutoff_end", 12000.0f }, { "noise_filter_q", 1.5f } } },

            { "2-Bar Lift",
              { { "duration_bars", 2.0f }, { "warp_k", 1.8f },
                { "tonal_volume", 0.4f }, { "tonal_pitch_start", 48.0f }, { "tonal_pitch_end", 88.0f },
                { "arp_enable", 1 }, { "arp_rate", 1 }, { "arp_pattern", 0 },
                { "noise_volume", 0.45f }, { "reverb_mix", 0.25f } } },

            { "Downlifter",
              { { "mode", 1 }, { "duration_bars", 4.0f }, { "tonal_volume", 0.45f }, { "tonal_pitch_start", 36.0f }, { "tonal_pitch_end", 84.0f },
                { "noise_volume", 0.35f }, { "noise_filter_mode", 1 }, { "reverb_mix", 0.35f },
                { "filter_enable", 1 }, { "filter_mode", 0 }, { "filter_resonance", 0.4f } } },

            { "Downlifter + Landing",
              { { "mode", 1 }, { "duration_bars", 8.0f }, { "boom_position", 3 }, { "warp_k", 2.0f },
                { "tonal_volume", 0.4f }, { "tonal_pitch_start", 30.0f }, { "tonal_pitch_end", 80.0f },
                { "tonal_unison", 5 }, { "noise_volume", 0.35f }, { "noise_filter_mode", 1 },
                { "reverb_mix", 0.4f }, { "boom_sub_decay", 4.0f } } },

            { "Impact Only",
              { { "tonal_volume", 0.0f }, { "noise_volume", 0.0f },
                { "boom_volume", 0.8f }, { "boom_trigger_mode", 1 }, { "boom_sub_decay", 3.5f },
                { "reverb_mix", 0.3f } } },

            { "Noise Whoosh",
              { { "duration_bars", 1.0f }, { "warp_k", 1.5f },
                { "tonal_volume", 0.0f }, { "noise_volume", 0.7f }, { "noise_filter_mode", 1 },
                { "noise_cutoff_start", 300.0f }, { "noise_cutoff_end", 16000.0f }, { "noise_filter_q", 0.8f },
                { "boom_volume", 0.0f } } },

            { "Dark Siren",
              { { "duration_bars", 8.0f }, { "warp_k", 3.5f },
                { "tonal_volume", 0.55f }, { "tonal_pitch_start", 24.0f }, { "tonal_pitch_end", 60.0f },
                { "tonal_unison", 7 }, { "tonal_detune", 0.05f },
                { "saturation_drive", 3.0f }, { "phaser_enable", 1 }, { "phaser_mix", 0.5f },
                { "phaser_feedback", 0.75f }, { "noise_volume", 0.2f } } },

            { "Bright Sweep",
              { { "duration_bars", 2.0f }, { "tonal_volume", 0.4f }, { "tonal_pitch_start", 60.0f }, { "tonal_pitch_end", 108.0f },
                { "tonal_unison", 3 }, { "noise_volume", 0.5f }, { "noise_cutoff_start", 500.0f },
                { "noise_cutoff_end", 18000.0f }, { "saturation_drive", 1.5f }, { "boom_volume", 0.5f } } },
        };
        return presets;
    }

    // Master gain and sync mode describe how you use the plugin, not the sound,
    // so presets never change them.
    inline bool isPerformanceParam (const juce::String& id) { return id == "master_volume" || id == "sync_mode"; }

    inline void applyValues (juce::AudioProcessor& proc,
                             const std::vector<std::pair<juce::String, float>>& values)
    {
        for (auto* p : proc.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr || isPerformanceParam (ranged->getParameterID()))
                continue;

            float target = ranged->getDefaultValue();
            for (const auto& kv : values)
                if (kv.first == ranged->getParameterID())
                    target = ranged->convertTo0to1 (kv.second);

            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (target);
            ranged->endChangeGesture();
        }
    }

    inline void applyFactory (juce::AudioProcessor& proc, int index)
    {
        const auto& list = factory();
        if (index < 0 || index >= (int) list.size())
            return;
        std::vector<std::pair<juce::String, float>> v;
        for (const auto& kv : list[(size_t) index].values)
            v.emplace_back (kv.first, kv.second);
        applyValues (proc, v);
    }

    inline juce::File userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RDSB").getChildFile ("Presets");
    }

    inline juce::Array<juce::File> userPresetFiles()
    {
        juce::Array<juce::File> files;
        userDirectory().findChildFiles (files, juce::File::findFiles, false, "*.rdsbpreset");
        files.sort();
        return files;
    }

    inline bool saveUser (juce::AudioProcessor& proc, const juce::String& name)
    {
        auto dir = userDirectory();
        if (! dir.createDirectory().wasOk())
            return false;

        juce::XmlElement xml ("RDSBPreset");
        xml.setAttribute ("name", name);
        for (auto* p : proc.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isPerformanceParam (ranged->getParameterID()))
                {
                    auto* e = xml.createNewChildElement ("PARAM");
                    e->setAttribute ("id", ranged->getParameterID());
                    e->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
                }

        return xml.writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".rdsbpreset"));
    }

    inline bool loadUser (juce::AudioProcessor& proc, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("RDSBPreset"))
            return false;

        std::vector<std::pair<juce::String, float>> values;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        applyValues (proc, values);
        return true;
    }
} // namespace Presets
