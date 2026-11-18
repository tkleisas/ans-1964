// ANS 1964 - an independent emulation of the 1964 five-disc ANS photoelectronic
// synthesizer, built for Akai MPC standalone hardware and desktop VST2 hosts.
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Signal flow:
//   720 optical sine tracks (1/72-octave grid)
//     -> score scanner (drawn strokes or a .ansscore image)
//     -> 20 photomultiplier channels (36 tracks each)
//     -> 6N2P-style tube preamp -> manipulator -> two-stage 0.5-octave twin-T filter
//     -> 20 channel level presets -> line 1 / line 2 -> master.
//
// The coder is reproduced as a 16-harmonic live voice with a formant template.
// See README.md and research/ANS_TECHNICAL_REPORT.md for what is measured,
// what is inferred, and what is deliberately approximated.

#include "mpc_plugin.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <thread>
#include <utility>
#include <vector>

#include "ANSDsp.h"

enum ParamIndex
{
    kVolume = 0,
    kScore, kRun, kSpeed, kReverse, kLoop, kScanPos, kScoreLevel,
    kCh1,
    kCh20 = kCh1 + 19,
    kCoderLevel, kHarmonics, kFormant, kFrmtShift, kCoderDecay, kCoderNote, kCoderGate,
    kWowDepth, kWowRate, kTrackNoise, kBlur, kHfLoss, kHum,
    kDynMode, kDynAmount, kDynRate,
    kDrive, kLineMode, kWidth,
    kEditX, kEditY, kEditLevel, kEditWidth, kEditHarm,
    kEditDraw, kEditErase, kEditClear, kEditAudition, kEditFlipY,
    kNumParams
};

// A vocal-style 20-channel preset: low-mid emphasis, rolled-off top. The live ANS
// recordings have most energy between ~100 Hz and 2 kHz, with a gentle top.
static const float kVocalCurve[ans64::kChannels] = {
    30.0f, 40.0f, 50.0f, 60.0f, 70.0f,
    90.0f, 100.0f, 100.0f, 100.0f,
    100.0f, 100.0f, 70.0f,
    30.0f, 35.0f, 30.0f,
    25.0f, 22.0f, 18.0f, 14.0f, 10.0f
};

class ANS1964 : public mpc::Plugin
{
public:
    ANS1964() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;

        addParam(floatParam("volume", "Volume", -40.0f, 6.0f, -6.0f, "dB", 1));

        addParam(choiceParam("score", "Score",
                             { "Vocal A", "Vocal O", "Solaris", "Harm Glide",
                               "Micro Arp", "Noise Swell", "Bell Garden", "Empty", "User Score" },
                             ans64::kScoreEmpty));
        addParam(boolParam ("run", "Run", false));
        addParam(floatParam("speed", "Speed", 0.0f, 24.0f, 12.0f, "mm/s", 1));
        addParam(boolParam ("reverse", "Backward", false));
        addParam(choiceParam("loop", "Loop", { "One-shot", "Loop", "Ping-Pong" }, 1));
        addParam(floatParam("scan_pos", "Scan Pos", 0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("score_gain", "Score Level", -40.0f, 6.0f, 0.0f, "dB", 1));

        for (int c = 0; c < ans64::kChannels; ++c)
        {
            char id[12], name[12];
            std::snprintf(id, sizeof(id), "ch%d", c + 1);
            std::snprintf(name, sizeof(name), "Ch %d", c + 1);
            addParam(floatParam(id, name, 0.0f, 100.0f, 80.0f, "%", 0));
        }

        addParam(floatParam("coder_level", "Coder Level", -40.0f, 6.0f, -6.0f, "dB", 1));
        addParam(intParam  ("coder_harm", "Harmonics", 1, 16, 16));
        addParam(choiceParam("coder_formant", "Formant",
                             { "Flat", "A", "E", "I", "O", "U", "Bright", "Dark" }, 1));
        addParam(floatParam("coder_shift", "Frmt Shift", 0.5f, 2.0f, 1.0f, "x", 2));
        addParam(logParam  ("coder_decay", "Coder Decay", 20.0f, 5000.0f, 400.0f, "ms", 0));
        addParam(intParam  ("coder_note", "Coder Note", 24, 96, 60));
        addParam(boolParam ("coder_gate", "Coder Gate", false));

        addParam(floatParam("wow_depth", "Wow Depth", 0.0f, 100.0f, 10.0f, "%", 0));
        addParam(logParam  ("wow_rate", "Wow Rate", 0.05f, 8.0f, 0.7f, "Hz", 2));
        addParam(floatParam("track_noise", "Track Noise", 0.0f, 100.0f, 10.0f, "%", 0));
        addParam(floatParam("blur", "Blur", 0.0f, 100.0f, 40.0f, "%", 0));
        addParam(floatParam("hf_loss", "HF Loss", 0.0f, 100.0f, 30.0f, "%", 0));
        addParam(floatParam("hum", "Hum", 0.0f, 100.0f, 0.0f, "%", 0));

        addParam(choiceParam("dyn_mode", "Dyn Mode", { "Open", "Dynamics", "Modulation" }, 0));
        addParam(floatParam("dyn_amount", "Dyn Amount", 0.0f, 100.0f, 50.0f, "%", 0));
        addParam(logParam  ("dyn_rate", "Dyn Rate", 0.05f, 8.0f, 1.2f, "Hz", 2));

        addParam(floatParam("drive", "Drive", 0.0f, 100.0f, 20.0f, "%", 0));
        addParam(choiceParam("line_mode", "Line Mode", { "Mono", "Split", "Stereo" }, 0));
        addParam(floatParam("width", "Width", 0.0f, 100.0f, 50.0f, "%", 0));

        // EDIT page: on-device XYPad drawing onto the user score image.
        addParam(floatParam("edit_x", "Edit X", 0.0f, 100.0f, 50.0f, "%", 1));
        addParam(floatParam("edit_y", "Edit Y", 0.0f, 100.0f, 50.0f, "%", 1));
        addParam(floatParam("edit_level", "Brush Lvl", 0.0f, 100.0f, 80.0f, "%", 0));
        addParam(intParam  ("edit_width", "Brush Wd", 1, 128, 8));
        addParam(intParam  ("edit_harm", "Edit Harm", 1, 16, 1));
        addParam(boolParam ("edit_draw", "Draw", false));
        addParam(boolParam ("edit_erase", "Erase", false));
        addParam(boolParam ("edit_clear", "Clear", false));
        addParam(boolParam ("edit_audition", "Audition", false));
        addParam(boolParam ("edit_flip_y", "Flip Y", false));

        if (numParams() != kNumParams)
            mpc::log("ANS1964: parameter count mismatch (%d vs %d)", numParams(), (int)kNumParams);

        auto program = [this](const char* name, std::initializer_list<std::pair<int, float>> overrides)
        {
            std::vector<float> v((size_t)numParams());
            for (int i = 0; i < numParams(); ++i) v[(size_t)i] = param(i).def;
            for (const auto& o : overrides) v[(size_t)o.first] = o.second;
            addProgram(name, std::move(v));
        };

        auto programCurve = [this](const char* name, const float (&curve)[ans64::kChannels],
                                   std::initializer_list<std::pair<int, float>> overrides)
        {
            std::vector<float> v((size_t)numParams());
            for (int i = 0; i < numParams(); ++i) v[(size_t)i] = param(i).def;
            for (const auto& o : overrides) v[(size_t)o.first] = o.second;
            for (int ch = 0; ch < ans64::kChannels; ++ch) v[(size_t)(kCh1 + ch)] = curve[ch];
            addProgram(name, std::move(v));
        };

        // 0: silent score, live coder ready for MIDI.
        program("Init", {});
        // The built-in scores are original ANS-idiom studies, not copies.
        programCurve("Vocal A", kVocalCurve, {
            { kScore, (float)ans64::kScoreVocalA }, { kRun, 1 }, { kSpeed, 12.0f },
            { kLoop, 1 }, { kCoderLevel, -12.0f }, { kScoreLevel, 4.0f } });
        program("Solaris", {
            { kScore, (float)ans64::kScoreSolaris }, { kRun, 1 }, { kSpeed, 6.0f },
            { kLoop, 1 }, { kWowDepth, 20.0f }, { kBlur, 70.0f }, { kTrackNoise, 20.0f },
            { kDynMode, 2 }, { kDynAmount, 30.0f }, { kDynRate, 0.3f }, { kScoreLevel, 6.0f } });
        program("Harm Glide", {
            { kScore, (float)ans64::kScoreHarmGlide }, { kRun, 1 }, { kSpeed, 12.0f },
            { kLoop, 1 }, { kCoderLevel, -18.0f }, { kScoreLevel, 4.0f } });
        program("Micro Arp", {
            { kScore, (float)ans64::kScoreMicroArp }, { kRun, 1 }, { kSpeed, 18.0f },
            { kLoop, 1 }, { kScoreLevel, 6.0f } });
        program("Noise Swell", {
            { kScore, (float)ans64::kScoreNoiseSwell }, { kRun, 1 }, { kSpeed, 8.0f },
            { kLoop, 1 }, { kBlur, 60.0f }, { kDynMode, 2 }, { kDynAmount, 50.0f },
            { kDynRate, 0.2f }, { kScoreLevel, -26.0f } });
        program("Bell Garden", {
            { kScore, (float)ans64::kScoreBellGarden }, { kRun, 1 }, { kSpeed, 10.0f },
            { kLoop, 1 }, { kBlur, 30.0f }, { kScoreLevel, 4.0f } });
        program("Coder U", {
            { kScore, (float)ans64::kScoreEmpty }, { kRun, 0 }, { kCoderLevel, 0.0f },
            { kHarmonics, 16 }, { kFormant, 5 }, { kCoderDecay, 900.0f } });
    }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        (void)maxBlockSize;
        engine_.setUserScorePath(mpc::contentPath("Scores/user.ansscore"));
        engine_.prepare(sampleRate);
    }

    void reset() override
    {
        if (stagedScoreValid_)
        {
            engine_.setUserScore(std::move(stagedScore_), stagedScoreWidth_, stagedScorePx_);
            stagedScoreValid_ = false;
        }
        engine_.reset();
    }

    void process(mpc::ProcessContext& ctx) override
    {
        ans64::Settings s;

        s.scoreIndex   = getInt(kScore);
        s.run          = getBool(kRun);
        s.reverse      = getBool(kReverse);
        s.loop         = getInt(kLoop);
        s.speed        = get(kSpeed);
        s.scanPos01    = get(kScanPos) * 0.01f;
        s.scoreGain    = mpc::dsp::dbToGain(get(kScoreLevel));
        s.volumeGain   = mpc::dsp::dbToGain(get(kVolume));
        for (int c = 0; c < ans64::kChannels; ++c)
            s.chLevel[c] = get(kCh1 + c) * 0.01f;

        s.coderGain    = mpc::dsp::dbToGain(get(kCoderLevel));
        s.harmonics    = getInt(kHarmonics);
        s.formant      = getInt(kFormant);
        s.formantShift = get(kFrmtShift);
        s.coderDecayMs = get(kCoderDecay);
        s.coderNote    = getInt(kCoderNote);
        s.testGate     = getBool(kCoderGate);

        s.wowDepth     = get(kWowDepth) * 0.01f;
        s.wowRate      = get(kWowRate);
        s.trackNoise   = get(kTrackNoise) * 0.01f;
        s.blur         = get(kBlur) * 0.01f;
        s.hfLoss       = get(kHfLoss) * 0.01f;
        s.hum          = get(kHum) * 0.01f;

        s.dynMode      = getInt(kDynMode);
        s.dynAmount    = get(kDynAmount) * 0.01f;
        s.dynRate      = get(kDynRate);

        s.drive        = get(kDrive) * 0.01f;
        s.lineMode     = getInt(kLineMode);
        s.width        = get(kWidth) * 0.01f;

        s.editX01      = get(kEditX) * 0.01f;
        s.editY01      = get(kEditY) * 0.01f;
        s.editLevel    = get(kEditLevel) * 0.01f;
        s.editWidth    = getInt(kEditWidth);
        s.editHarm     = getInt(kEditHarm);
        s.editDraw     = getBool(kEditDraw);
        s.editErase    = getBool(kEditErase);
        s.editClear    = getBool(kEditClear);
        s.editAudition = getBool(kEditAudition);
        s.editFlipY    = getBool(kEditFlipY);

        engine_.setSettings(s);

        float* outL = ctx.outputs[0];
        float* outR = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;

        int pos = 0;
        for (int e = 0; e < ctx.numMidiEvents; ++e)
        {
            const mpc::MidiEvent& ev = ctx.midi[e];
            const int off = mpc::clamp(ev.sampleOffset, 0, ctx.numFrames);
            if (off > pos)
            {
                engine_.render(outL + pos, outR ? outR + pos : nullptr, off - pos);
                pos = off;
            }
            handleMidi(ev);
        }
        if (pos < ctx.numFrames)
            engine_.render(outL + pos, outR ? outR + pos : nullptr, ctx.numFrames - pos);
    }

    void saveExtraState(std::vector<uint8_t>& out) override
    {
        engine_.freezeEdits(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::vector<uint8_t> score;
        int w = 0, px = 0;
        const bool ok = engine_.copyUserScore(score, w, px);
        engine_.freezeEdits(false);
        if (!ok || w <= 0 || score.size() > 0xFFFFFFFFu) return;

        const size_t base = out.size();
        out.resize(base + 16 + score.size());
        uint8_t* p = out.data() + base;
        p[0] = 'A'; p[1] = 'N'; p[2] = 'S'; p[3] = 'X';
        p[4] = 1; p[5] = 0;                       // version 1
        p[6] = (uint8_t)(w & 0xff); p[7] = (uint8_t)(w >> 8);
        p[8] = (uint8_t)(px & 0xff); p[9] = (uint8_t)(px >> 8);
        p[10] = 0; p[11] = 0;                     // reserved
        const uint32_t n = (uint32_t)score.size();
        p[12] = (uint8_t)(n & 0xff); p[13] = (uint8_t)((n >> 8) & 0xff);
        p[14] = (uint8_t)((n >> 16) & 0xff); p[15] = (uint8_t)((n >> 24) & 0xff);
        std::memcpy(p + 16, score.data(), score.size());
    }

    void loadExtraState(const uint8_t* data, size_t size) override
    {
        if (data == nullptr || size < 16) return;
        if (std::memcmp(data, "ANSX", 4) != 0) return;
        const uint16_t version = rd16(data + 4);
        const uint16_t w = rd16(data + 6);
        const uint16_t px = rd16(data + 8);
        const uint32_t bytes = rd32(data + 12);
        if (version != 1 || w == 0 || w > 16384 || bytes != (uint32_t)w * 720u) return;
        if (size < 16u + (size_t)bytes) return;
        stagedScore_.assign(data + 16, data + 16 + (size_t)bytes);
        stagedScoreWidth_ = w;
        stagedScorePx_ = px > 0 ? px : 10;
        stagedScoreValid_ = true;
    }

private:
    static uint16_t rd16(const uint8_t* p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
    static uint32_t rd32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo info;
        info.name         = "ANS 1964";
        info.vendor       = "bokontep";
        info.uniqueId     = MPC_FOURCC('B', 'y', 'L', 'm');
        info.version      = 1102;
        info.isInstrument = true;
        info.numInputs    = 0;
        info.numOutputs   = 2;
        info.tailSamples  = 0;
        return info;
    }

    void handleMidi(const mpc::MidiEvent& e)
    {
        if (e.isNoteOn())
            engine_.noteOn(e.note(), e.velocity());
        else if (e.isNoteOff())
            engine_.noteOff(e.note());
        else if (e.isAllNotesOff())
            engine_.allNotesOff();
    }

    ans64::Engine engine_;
    std::vector<uint8_t> stagedScore_;
    int stagedScoreWidth_ = 0;
    int stagedScorePx_ = 10;
    bool stagedScoreValid_ = false;
};

MPC_PLUGIN_ENTRY(ANS1964)
