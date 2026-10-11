// Headless engine tests for Rise.Drop.Sweep.Boom (RDSB). Build and run:
//   cmake --build cmake-build --target RDSBTests
//   cmake-build/RDSBTests_artefacts/Release/RDSBTests
// A fake DAW transport drives the real processor offline.
//   RDSBTests --snapshot out.png   renders the editor offscreen.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {

int failures = 0;
void check(bool ok, const juce::String& what) {
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!ok) ++failures;
}

struct FakeHost : juce::AudioPlayHead {
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
    int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;
        p.setBpm(bpm); p.setPpqPosition(ppq); p.setIsPlaying(playing);
        juce::AudioPlayHead::TimeSignature ts; ts.numerator = num; ts.denominator = den;
        p.setTimeSignature(ts);
        return p;
    }
};

using Params = std::vector<std::pair<const char*, float>>;

void setParam(RDSBAudioProcessor& p, const char* id, float v) {
    auto* prm = p.apvts.getParameter(id);
    jassert(prm != nullptr);
    prm->setValueNotifyingHost(prm->convertTo0to1(v));
}

struct Rendered { std::vector<float> l, r; };

// Optional per-block hook lets a test change parameters mid-playback.
Rendered run(int block, int total, bool transport, double startPpq, const Params& params, bool noteOnAtStart,
             double bpm = 120.0, int sigNum = 4,
             const std::function<void(RDSBAudioProcessor&, int)>& hook = {}) {
    RDSBAudioProcessor proc;
    FakeHost host; host.bpm = bpm; host.ppq = startPpq; host.num = sigNum;
    proc.setPlayHead(&host);
    proc.setPlayConfigDetails(0, 2, 48000.0, block);
    proc.prepareToPlay(48000.0, block);
    for (auto& kv : params) setParam(proc, kv.first, kv.second);
    setParam(proc, "sync_mode", transport ? 0.0f : 1.0f);

    Rendered r;
    juce::AudioBuffer<float> buf(2, block);
    for (int s = 0; s < total; s += block) {
        const int n = std::min(block, total - s);
        if (hook) hook(proc, s);
        buf.setSize(2, n, false, false, true);
        juce::MidiBuffer midi;
        if (s == 0 && noteOnAtStart) midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        proc.processBlock(buf, midi);
        for (int i = 0; i < n; ++i) { r.l.push_back(buf.getSample(0, i)); r.r.push_back(buf.getSample(1, i)); }
        host.ppq += (double) n / 48000.0 * host.bpm / 60.0;
    }
    return r;
}

int firstAbove(const std::vector<float>& x, int from, float thr) {
    for (int i = from; i < (int) x.size(); ++i) if (std::abs(x[(size_t) i]) > thr) return i;
    return -1;
}
int lastAbove(const std::vector<float>& x, float thr) {
    for (int i = (int) x.size() - 1; i >= 0; --i) if (std::abs(x[(size_t) i]) > thr) return i;
    return -1;
}
float peakOf(const Rendered& r) {
    float pk = 0;
    for (size_t i = 0; i < r.l.size(); ++i) pk = std::max({pk, std::abs(r.l[i]), std::abs(r.r[i])});
    return pk;
}
double rms(const std::vector<float>& x, int a, int b) {
    double e = 0; for (int i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt(e / std::max(1, b - a));
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc >= 3 && juce::String(argv[1]) == "--snapshot") {
        RDSBAudioProcessor proc;
        proc.setPlayConfigDetails(0, 2, 48000.0, 512);
        proc.prepareToPlay(48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        ed->setVisible(true);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File out(argv[2]);
        out.deleteFile();
        juce::FileOutputStream st(out);
        juce::PNGImageFormat png;
        const bool ok = st.openedOk() && png.writeImageToStream(img, st);
        std::cout << (ok ? "wrote " : "FAILED ") << out.getFullPathName() << " (" << ed->getWidth() << "x"
                  << ed->getHeight() << ")\n";
        return ok ? 0 : 1;
    }

    std::cout << "RDSB engine tests\n";
    const Params boomOnly = { {"tonal_volume", 0.0f}, {"noise_volume", 0.0f}, {"duration_bars", 1.0f},
                              {"reverb_mix", 0.0f}, {"boom_volume", 1.0f} };

    {
        std::cout << "[1] Boom lands on the drop, whatever the host buffer size\n";
        for (int block : {64, 512, 2048}) {
            auto r = run(block, 110000, true, 0.0, boomOnly, false);
            const int at = firstAbove(r.l, 0, 0.0005f);
            check(at >= 96000 && at <= 96002, "block " + juce::String(block) + ": drop at sample " + juce::String(at)
                                                   + " (expected 96000)");
        }
    }

    {
        std::cout << "[2] Riser fills its full length in MIDI mode, then drops on the last sample\n";
        for (int block : {64, 512, 2048}) {
            Params p = { {"tonal_volume", 1.0f}, {"noise_volume", 0.0f}, {"duration_bars", 1.0f},
                         {"reverb_mix", 0.0f}, {"tonal_unison", 1.0f}, {"boom_volume", 0.0f}, {"boom_trigger_mode", 0.0f} };
            auto r = run(block, 120000, false, 0.0, p, true);
            const int end = lastAbove(r.l, 0.001f);
            check(end >= 95990 && end <= 96000, "block " + juce::String(block) + ": riser ends at sample "
                                                    + juce::String(end) + " (expected ~96000)");
        }
        // Boom must follow immediately after the riser.
        Params p = boomOnly; p.push_back({"boom_trigger_mode", 0.0f});
        auto r = run(512, 120000, false, 0.0, p, true);
        const int at = firstAbove(r.l, 0, 0.0005f);
        check(at >= 96000 && at <= 96002, "MIDI-mode boom at sample " + juce::String(at) + " (expected 96000)");
    }

    {
        std::cout << "[3] Output never exceeds full scale\n";
        auto a = run(512, 130000, false, 0.0, { {"duration_bars", 1.0f} }, true);
        check(peakOf(a) <= 1.0f, "default patch peak " + juce::String(peakOf(a), 3) + " (was 1.75)");
        auto b = run(512, 130000, false, 0.0, { {"duration_bars", 1.0f}, {"saturation_drive", 4.0f},
                                                {"noise_volume", 1.0f}, {"tonal_volume", 1.0f}, {"master_volume", 6.0f} }, true);
        check(peakOf(b) <= 1.0f, "everything full + drive + 6 dB peak " + juce::String(peakOf(b), 3) + " (was 2.86)");
    }

    {
        std::cout << "[3b] Default patch headroom (measured 12 dB down, so the limiter is not involved)\n";
        auto a = run(512, 130000, false, 0.0, { {"duration_bars", 1.0f}, {"master_volume", -12.0f}, {"reverb_mix", 0.0f} }, true);
        const double pre = peakOf(a) * std::pow(10.0, 12.0 / 20.0);
        std::cout << "        pre-limiter peak ~ " << pre << " (" << 20.0 * std::log10(pre) << " dBFS)\n";
        check(pre < 1.0, "default patch peaks below full scale before the limiter");
    }

    {
        std::cout << "[4] Boom tail ends without a click\n";
        Params p = boomOnly; p.push_back({"boom_sub_decay", 0.5f}); p.push_back({"boom_trigger_mode", 0.0f});
        auto r = run(512, 96000 + 40000, true, 0.0, p, false);
        // find the end of the tail (~0.5 s after the drop) and measure the biggest step near it
        float maxStep = 0;
        for (int i = 96000 + 20000; i < 96000 + 28000; ++i)
            maxStep = std::max(maxStep, std::abs(r.l[(size_t) i] - r.l[(size_t) i - 1]));
        check(maxStep < 0.002f, "max sample step across the tail's end " + juce::String(maxStep, 5));
    }

    {
        std::cout << "[5] Moving unison/detune/width mid-sweep does not click\n";
        // Low pitch so a saw changes slowly; only a phase re-randomisation could jump.
        Params p = { {"tonal_volume", 1.0f}, {"noise_volume", 0.0f}, {"boom_volume", 0.0f}, {"duration_bars", 4.0f},
                     {"reverb_mix", 0.0f}, {"tonal_unison", 3.0f}, {"tonal_pitch_start", 30.0f},
                     {"tonal_pitch_end", 30.0f}, {"boom_trigger_mode", 1.0f} };
        auto r = run(256, 48000, false, 0.0, p, true, 120.0, 4,
                     [](RDSBAudioProcessor& proc, int s) {
                         if (s == 256 * 40) { setParam(proc, "tonal_detune", 0.06f); setParam(proc, "tonal_pan_spread", 0.2f); }
                     });
        float maxStep = 0;
        const int b = 256 * 40;
        for (int i = b - 8; i < b + 8; ++i) maxStep = std::max(maxStep, std::abs(r.l[(size_t) i] - r.l[(size_t) i - 1]));
        check(maxStep < 0.05f, "largest step at the parameter change " + juce::String(maxStep, 4));
    }

    {
        std::cout << "[6] Riser output does not depend on block size\n";
        Params p = { {"tonal_volume", 1.0f}, {"noise_volume", 0.6f}, {"boom_volume", 0.0f}, {"duration_bars", 1.0f},
                     {"reverb_mix", 0.0f}, {"boom_trigger_mode", 1.0f} };
        auto a = run(64, 100000, false, 0.0, p, true), b = run(2048, 100000, false, 0.0, p, true);
        bool ok = true;
        for (int w = 5000; w < 90000; w += 15000)
            ok &= std::abs(rms(a.l, w, w + 4000) - rms(b.l, w, w + 4000)) < 0.05 * rms(a.l, w, w + 4000);
        check(ok, "RMS of the sweep matches at 64 vs 2048 samples");
    }

    {
        std::cout << "[7] Pumper is tempo-synced (2 pumps per beat at 120 BPM = 4 per second)\n";
        Params p = { {"tonal_volume", 1.0f}, {"noise_volume", 0.0f}, {"boom_volume", 0.0f}, {"duration_bars", 4.0f},
                     {"reverb_mix", 0.0f}, {"tonal_unison", 1.0f}, {"pumper_depth", 1.0f},
                     {"pumper_rate_start", 2.0f}, {"pumper_rate_end", 2.0f}, {"tonal_pitch_start", 60.0f},
                     {"tonal_pitch_end", 60.0f}, {"boom_trigger_mode", 1.0f} };
        auto r = run(512, 96000, false, 0.0, p, true);
        // count how many times the 5 ms RMS envelope dips below 20% of its peak
        std::vector<double> env;
        for (int i = 0; i + 240 <= 96000; i += 240) env.push_back(rms(r.l, i, i + 240));
        double peak = 0; for (double e : env) peak = std::max(peak, e);
        int dips = 0; bool below = false;
        for (size_t i = 10; i < env.size(); ++i) {
            if (env[i] < 0.2 * peak && !below) { ++dips; below = true; }
            if (env[i] > 0.5 * peak) below = false;
        }
        check(dips >= 7 && dips <= 9, "dips in 2 s: " + juce::String(dips) + " (expected 8)");
        // a different tempo should change the pump rate
        auto r2 = run(512, 96000, false, 0.0, p, true, 60.0);
        std::vector<double> env2;
        for (int i = 0; i + 240 <= 96000; i += 240) env2.push_back(rms(r2.l, i, i + 240));
        double peak2 = 0; for (double e : env2) peak2 = std::max(peak2, e);
        int dips2 = 0; below = false;
        for (size_t i = 10; i < env2.size(); ++i) {
            if (env2[i] < 0.2 * peak2 && !below) { ++dips2; below = true; }
            if (env2[i] > 0.5 * peak2) below = false;
        }
        check(dips2 >= 4 && dips2 <= 5, "at 60 BPM the pump halves (" + juce::String(dips2) + " dips, expected 4, +1 allowed for the t=2.0 s boundary)");
    }

    {
        std::cout << "[8] Transport loop in 3/4: drop on the bar line\n";
        auto r = run(256, 80000, true, 0.0, boomOnly, false, 120.0, 3);
        const int at = firstAbove(r.l, 0, 0.0005f);
        check(at >= 72000 && at <= 72002, "3/4 bar = 72000 samples, drop at " + juce::String(at));
    }

    std::cout << (failures == 0 ? "\nALL PASSED\n" : "\nFAILURES: " + std::to_string(failures) + "\n");
    return failures == 0 ? 0 : 1;
}
