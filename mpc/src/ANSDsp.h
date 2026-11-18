// ANSDsp.h - the ANS 1964 sound engine.
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Circuit/spec sources (research, not copied code):
//   - USSR author certificate 118695 (Murzin, 1957): score + optical slit + 16-cutter coder
//   - Habr 961642 (2025): reader, channel preamp, manipulator and twin-T filter schematics
//   - asmir.info/lib/murzin0.htm: 720 tones, 5 discs, speeds, disc geometry
//   - Kreichi/Krivnova/Egorov, Speech Technologies 2013: 0.3 mm tracks, score width/level
//
// The engine is deliberately split into the same blocks as the instrument:
//   720 optical partials -> 20 half-octave photomultiplier channels ->
//   tube preamp -> manipulator -> twin-T bandpass -> channel level -> line 1/2.
//
// The 720 partials are exact 1/72-octave tones, but only the tracks that are lit by
// the score (plus live coder voices) run. Dense sections are thinned to kMaxPartials
// with a power-preserving gain, so an MPC can render a full "white noise" scratch.

#ifndef ANS1964_DSP_H
#define ANS1964_DSP_H

#include "dsp/math.h"
#include "dsp/biquad.h"
#include "dsp/onepole.h"
#include "dsp/noise.h"
#include "dsp/oscillator.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ans64 {

using namespace mpc::dsp;

constexpr int   kTracks          = 720;
constexpr int   kChannels        = 20;
constexpr int   kTracksPerChannel = 36;
constexpr float kF0              = 21.09f;   // first track, Hz (asmir/museum figure)
constexpr int   kCtrl            = 16;       // control interval in samples
constexpr int   kMaxPartials     = 192;      // density cap on the slowest MPCs
constexpr int   kNoiseThreshold   = 20;       // tracks/channel above which the channel becomes band noise
constexpr int   kMaxCoderVoices  = 8;
constexpr int   kMaxHarmonics    = 16;
constexpr int   kNumBuiltInScores = 8;       // indices 0..7, 8 = user image
constexpr float kPartialScale    = 1.5f;    // headroom before the 20 channel sums

// Keep this order in sync with the "Score" choice parameter in ANS1964.cpp.
// 0 Vocal A, 1 Vocal O, 2 Solaris, 3 Harm Glide, 4 Micro Arp,
// 5 Noise Swell, 6 Bell Garden, 7 Empty, 8 User Score.
enum ScoreIndex
{
    kScoreVocalA = 0,
    kScoreVocalO,
    kScoreSolaris,
    kScoreHarmGlide,
    kScoreMicroArp,
    kScoreNoiseSwell,
    kScoreBellGarden,
    kScoreEmpty,
    kScoreUser
};

struct Settings
{
    int   scoreIndex   = kScoreEmpty;
    bool  run          = false;
    bool  reverse      = false;
    int   loop         = 1;          // 0 off, 1 loop, 2 ping-pong
    float speed        = 12.0f;      // mm/s, 0..24
    float scanPos01    = 0.0f;       // manual scrub, 0..1
    float scoreGain    = 1.0f;       // linear
    float volumeGain   = 0.5f;       // linear
    float chLevel[kChannels] = {};
    float coderGain    = 0.5f;       // linear
    int   harmonics    = 8;
    int   formant      = 1;          // Flat/A/E/I/O/U/Bright/Dark
    float formantShift = 1.0f;
    float coderDecayMs = 400.0f;
    int   coderNote    = 60;
    bool  testGate     = false;
    float wowDepth     = 0.1f;
    float wowRate      = 0.7f;
    float trackNoise   = 0.1f;
    float blur         = 0.4f;
    float hfLoss       = 0.3f;
    float hum          = 0.0f;
    int   dynMode      = 0;          // 0 open, 1 dynamics, 2 modulation
    float dynAmount    = 0.5f;
    float dynRate      = 1.2f;
    float drive        = 0.2f;
    int   lineMode     = 0;          // 0 mono, 1 split, 2 stereo
    float width        = 0.5f;

    // On-device XYPad editor
    float editX01      = 0.5f;
    float editY01      = 0.5f;
    float editLevel    = 0.8f;
    int   editWidth    = 8;
    int   editHarm     = 1;
    bool  editDraw     = false;
    bool  editErase    = false;
    bool  editClear    = false;
    bool  editAudition = false;
    bool  editFlipY    = false;
};

struct Stroke
{
    float t0 = 0.0f, t1 = 0.0f;
    float p0 = 0.0f, p1 = 0.0f;      // track positions (0..719, fractional)
    float w = 0.6f;                  // optical line width in tracks
    float level = 0.5f;
    float fade = 0.0f;               // seconds
    float decay = 0.0f;              // seconds, exponential
};

struct Score
{
    std::vector<Stroke> strokes;
    float duration = 1.0f;
};

// Coder formant templates (F1/F2/F3, amplitudes, bandwidths in Hz).
struct FormantPreset
{
    float f[3];
    float a[3];
    float b[3];
};

inline const FormantPreset& formantPreset(int index)
{
    static const FormantPreset kPresets[8] = {
        // Flat: index 0 is handled separately
        {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}},
        // A
        {{800, 1150, 2900}, {1.00f, 0.55f, 0.30f}, {90, 110, 170}},
        // E
        {{400, 1600, 2700}, {1.00f, 0.60f, 0.30f}, {80, 110, 160}},
        // I
        {{300, 2300, 3000}, {1.00f, 0.50f, 0.30f}, {70, 130, 170}},
        // O
        {{450, 800, 2830}, {1.00f, 0.60f, 0.20f}, {80, 90, 160}},
        // U
        {{325, 700, 2530}, {1.00f, 0.50f, 0.15f}, {70, 90, 160}},
        // Bright
        {{1800, 3200, 6000}, {0.80f, 0.70f, 0.60f}, {300, 500, 900}},
        // Dark
        {{300, 600, 1200}, {1.00f, 0.60f, 0.30f}, {100, 150, 300}}
    };
    return kPresets[clamp(index, 0, 7)];
}

inline float formantGain(float hz, int preset, float shift)
{
    if (preset <= 0) return 1.0f;
    const FormantPreset& p = formantPreset(preset);
    // The mechanical coder can leave the fundamental and non-formant overtones
    // open; the template shapes them rather than deleting them.
    float g = 0.18f;
    for (int k = 0; k < 3; ++k)
    {
        const float x = (hz - p.f[k] * shift) / p.b[k];
        g += p.a[k] * std::exp(-0.5f * x * x);
    }
    return g;
}

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr_ = (float)sampleRate;

        for (int n = 0; n < kTracks; ++n)
        {
            baseInc_[n] = kF0 * std::pow(2.0f, (float)n / 72.0f) / sr_;
            channelOf_[n] = (uint8_t)(n / kTracksPerChannel);
            discOf_[n] = (uint8_t)(n / 144);
            phase_[n] = 0.0f;
        }

        for (int c = 0; c < kChannels; ++c)
        {
            // 36 tracks = half an octave. Two cascaded 2nd-order stages with
            // Q ~1.85 give a combined -3 dB bandwidth close to the documented
            // 0.5-octave twin-T response.
            const float hz = kF0 * std::pow(2.0f, 0.25f + 0.5f * (float)c);
            bpCoeff_[c] = BiquadCoeffs::bandpass(sampleRate, hz, 1.85f);
            bp1_[c].setCoeffs(bpCoeff_[c]);
            bp2_[c].setCoeffs(bpCoeff_[c]);
            chGain_[c] = 0.8f;
            targetChGain_[c] = 0.8f;

            // Calibrate the band-noise fallback so a fully lit 36-track channel has
            // the same RMS as the equivalent bank of 36 sine partials.
            noiseBp1_[c].setCoeffs(bpCoeff_[c]);
            noiseBp2_[c].setCoeffs(bpCoeff_[c]);
            Biquad s1, s2, m1, m2;
            s1.setCoeffs(bpCoeff_[c]);
            s2.setCoeffs(bpCoeff_[c]);
            m1.setCoeffs(bpCoeff_[c]);
            m2.setCoeffs(bpCoeff_[c]);
            WhiteNoise wn(0x51F0A000u + (uint32_t)c);
            double p = 0.0;
            const int kCal = 8192;
            for (int i = 0; i < kCal; ++i)
            {
                float y = wn.process();
                y = s1.process(y);
                y = s2.process(y);
                y = m1.process(y);
                y = m2.process(y);
                p += (double)y * (double)y;
            }
            const float rms = (float)std::sqrt(p / (double)kCal);
            noiseCal_[c] = rms > 1.0e-9f ? 1.0f / rms : 1.0f;
            noiseSrc_[c].reseed(0xA11CE000u + (uint32_t)c);
        }

        for (int h = 0; h < kMaxHarmonics; ++h)
            harmOffset_[h] = (int)std::lround(72.0f * std::log2((float)(h + 1)));
        for (int h = 0; h < kMaxHarmonics; ++h)
            harmRolloff_[h] = std::pow((float)(h + 1), -0.5f);

        envCoef_ = 1.0f - std::exp(-(float)kCtrl / (sr_ * 0.010f));   // ~10 ms follower
        ampCoef_ = 1.0f - std::exp(-(float)kCtrl / (sr_ * 0.0014f));  // ~1.4 ms optical edge
        coderAttack_ = 1.0f - std::exp(-(float)kCtrl / (sr_ * 0.005f));

        dcL_.prepare(sampleRate);
        dcR_.prepare(sampleRate);
        hfL_.setCutoff(16000.0f, sampleRate);
        hfR_.setCutoff(16000.0f, sampleRate);

        (void)sineTable();   // build the shared sine table off the audio thread
        buildScores();
        loadUserScore();
        if (userData_.empty())
        {
            userWidth_ = 1024;
            userHeight_ = kTracks;
            userPxPerMm_ = 10;
            userData_.assign((size_t)userWidth_ * (size_t)userHeight_, 0);
            std::fprintf(stderr, "ANS 1964: no user score; created a blank 1024x720 surface\n");
        }
        reset();
    }

    void reset()
    {
        for (int n = 0; n < kTracks; ++n)
        {
            phase_[n] = 0.0f;
            amp_[n] = 0.0f;
            target_[n] = 0.0f;
            tn_[n] = 0.0f;
        }
        // Deterministic arbitrary phases, like the five continuously spinning discs.
        Xorshift32 rng(0x414E5331u);
        for (int n = 0; n < kTracks; ++n) phase_[n] = rng.nextFloat();

        for (int c = 0; c < kChannels; ++c)
        {
            bp1_[c].reset();
            bp2_[c].reset();
            env_[c] = 0.0f;
            dynLfo_[c] = 0.0f;
            lfoPhase_[c] = (float)c * 0.137f;
            noiseGain_[c] = 0.0f;
            noiseCh_[c] = false;
            noiseBp1_[c].reset();
            noiseBp2_[c].reset();
            noiseSrc_[c].reseed(0xA11CE000u + (uint32_t)c);
        }

        for (int v = 0; v < kMaxCoderVoices; ++v) voices_[v] = CoderVoice();

        activeCount_ = 0;
        ctrlCounter_ = 0;
        noiseCounter_ = 0;
        humPhase_ = 0.0f;
        realTime_ = 0.0;
        scoreTime_ = 0.0;
        scoreDur_ = 0.0f;
        scanDir_ = 1;
        lastScoreIndex_ = -1;
        lastScanPos_ = 0.0f;
        lastReverse_ = false;
        lastFormant_ = -1;
        lastFormantShift_ = 0.0f;
        lastBlur_ = -1.0f;
        coderDirty_ = true;
        initialized_ = false;
        testGatePrev_ = false;
        testNotePrev_ = -1;
        lastEditClear_ = false;
        lastEditCol_ = -1;
        lastEditTrack_ = -1;
        lastEditActive_ = false;
        voiceOrder_ = 0;
        wowPhase_ = 0.0f;
        wowNoise_ = 0.0f;
        scoreGainSm_ = 1.0f;
        coderGainSm_ = 1.0f;
        masterGainSm_ = 1.0f;
        dcL_.reset();
        dcR_.reset();
        hfL_.reset();
        hfR_.reset();
        blurW_[0] = 0.0f; blurW_[1] = 0.25f; blurW_[2] = 0.25f;
    }

    void setSettings(const Settings& s)
    {
        const bool scoreChanged = (s.scoreIndex != s_.scoreIndex);
        const bool reverseChanged = (s.reverse != s_.reverse);
        const bool formantChanged = (s.formant != s_.formant) ||
                                    (std::fabs(s.formantShift - s_.formantShift) > 1.0e-4f);
        const bool blurChanged = (std::fabs(s.blur - s_.blur) > 1.0e-4f);

        s_ = s;

        if (!initialized_)
        {
            scoreGainSm_ = s.scoreGain;
            coderGainSm_ = s.coderGain;
            masterGainSm_ = s.volumeGain;
            for (int c = 0; c < kChannels; ++c)
                chGain_[c] = targetChGain_[c] = clamp(s.chLevel[c], 0.0f, 1.0f);
            selectScore(s.scoreIndex);
            initialized_ = true;
        }
        else if (scoreChanged)
        {
            selectScore(s.scoreIndex);
        }
        if (reverseChanged) scanDir_ = s.reverse ? -1 : 1;
        if (formantChanged) coderDirty_ = true;

        for (int c = 0; c < kChannels; ++c)
            targetChGain_[c] = clamp(s.chLevel[c], 0.0f, 1.0f);

        // Precompute the line 1/2 routing so the audio loop never calls cos/sin.
        for (int c = 0; c < kChannels; ++c)
        {
            if (s.lineMode == 0)      { panL_[c] = 1.0f; panR_[c] = 1.0f; }
            else if (s.lineMode == 1) { panL_[c] = (c < 10) ? 1.0f : 0.0f;
                                        panR_[c] = (c < 10) ? 0.0f : 1.0f; }
            else equalPowerPan(((c < 10) ? -1.0f : 1.0f) * clamp(s.width, 0.0f, 1.0f),
                               panL_[c], panR_[c]);
        }

        if (blurChanged)
        {
            const float sigma = 0.15f + 1.35f * clamp(s.blur, 0.0f, 1.0f);
            float w[5];
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k)
            {
                const float t = (float)k / sigma;
                w[k + 2] = std::exp(-0.5f * t * t);
                sum += w[k + 2];
            }
            const float inv = sum > 0.0f ? 1.0f / sum : 1.0f;
            blurW_[0] = w[0] * inv;
            blurW_[1] = w[1] * inv;
            blurW_[2] = w[2] * inv;
            lastBlur_ = s.blur;
        }

        driveGain_ = 1.0f + 2.5f * clamp(s.drive, 0.0f, 1.0f);
        driveBias_ = 0.06f * driveGain_;
        // "Coder Decay" is the time to fall by 60 dB, not an asymptotic time constant.
        coderDecay_ = 1.0f - std::exp(-6.9078f * (float)kCtrl /
                                      (sr_ * clamp(s.coderDecayMs, 20.0f, 5000.0f) * 0.001f));

        const float hf = 20000.0f - 12000.0f * clamp(s.hfLoss, 0.0f, 1.0f);
        hfL_.setCutoff(hf, sr_);
        hfR_.setCutoff(hf, sr_);
    }

    // Render n frames into outL/outR. outR may be null for a mono host.
    void render(float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (ctrlCounter_ == 0) updateControl();
            if (++ctrlCounter_ >= kCtrl) ctrlCounter_ = 0;

            float ch[kChannels];
            for (int c = 0; c < kChannels; ++c) ch[c] = 0.0f;

            for (int a = 0; a < activeCount_; ++a)
            {
                const int idx = active_[a];
                float p = phase_[idx] + inc_[idx];
                if (p >= 1.0f) p -= 1.0f;
                phase_[idx] = p;
                ch[channelOf_[idx]] += sinLut(p) * activeGain_[a];
            }

            float busL = 0.0f, busR = 0.0f;

            for (int c = 0; c < kChannels; ++c)
            {
                float x = ch[c];
                if (noiseCh_[c])
                {
                    float n = noiseSrc_[c].process();
                    n = noiseBp1_[c].process(n);
                    n = noiseBp2_[c].process(n);
                    x += n * noiseGain_[c];
                }

                // 6N2P channel preamp, switchable in the original. A small bias
                // adds even harmonics; normalising keeps small signals clean.
                const float b = driveBias_;
                x = (fastTanh(x * driveGain_ + b) - fastTanh(b)) * (1.0f / driveGain_);

                // Manipulator: envelope follower -> expander or animation.
                env_[c] += envCoef_ * (std::fabs(x) - env_[c]);
                float g = 1.0f;
                if (s_.dynMode == 1)
                {
                    g = 1.0f + s_.dynAmount * 2.0f * (env_[c] - 0.08f);
                    g = clamp(g, 0.15f, 2.5f);
                }
                else if (s_.dynMode == 2)
                {
                    g = 1.0f + s_.dynAmount * 0.5f * dynLfo_[c];
                    g = clamp(g, 0.0f, 1.75f);
                }
                x *= g;

                // Two-stage half-octave twin-T-style channel filter.
                x = bp1_[c].process(x);
                x = bp2_[c].process(x);
                x *= chGain_[c];

                // Line 1 / line 2 routing (Mono, Split, Stereo).
                busL += x * panL_[c];
                busR += x * panR_[c];
            }

            float l = hfL_.process(busL);
            float r = hfR_.process(busR);
            l = dcL_.process(l);
            r = dcR_.process(r);

            if (s_.hum > 0.0f)
            {
                const float hum = s_.hum * 0.010f * fastSin01(humPhase_);
                l += hum;
                r += hum;
            }
            humPhase_ += 100.0f / sr_;
            if (humPhase_ >= 1.0f) humPhase_ -= 1.0f;

            l = softClip(l * masterGainSm_);
            r = softClip(r * masterGainSm_);
            l = flushDenormal(l);
            r = flushDenormal(r);

            if (outR != nullptr)
            {
                outL[i] = l;
                outR[i] = r;
            }
            else
            {
                outL[i] = 0.5f * (l + r);
            }
        }
    }

    void noteOn(int note, int velocity)
    {
        int slot = findVoice();
        if (slot < 0) return;

        CoderVoice& v = voices_[slot];
        v = CoderVoice();
        v.note = note;
        v.target = 1.0f;
        v.env = 0.0f;
        v.velGain = clamp((float)velocity / 127.0f, 0.0f, 1.0f);
        v.order = ++voiceOrder_;
        v.isTest = false;

        const float hz = midiToHz((float)note);
        const int n0 = (int)std::lround(72.0f * std::log2(hz / kF0));
        const int maxH = clamp(s_.harmonics, 1, kMaxHarmonics);

        for (int h = 1; h <= maxH; ++h)
        {
            const int tr = n0 + harmOffset_[h - 1];
            if (tr < 0) continue;
            if (tr >= kTracks) break;
            v.track[v.count] = (uint16_t)tr;
            v.gain[v.count] = 1.0f;
            ++v.count;
        }
        if (v.count == 0)
        {
            v.note = -1;
            return;
        }
        computeCoderGains(v);
    }

    void noteOff(int note)
    {
        for (int i = 0; i < kMaxCoderVoices; ++i)
            if (voices_[i].note == note && !voices_[i].isTest)
                voices_[i].target = 0.0f;
    }

    void allNotesOff()
    {
        for (int i = 0; i < kMaxCoderVoices; ++i) voices_[i].target = 0.0f;
    }

    // User-score drawing buffer, shared with the MPC project state.
    bool copyUserScore(std::vector<uint8_t>& out, int& width, int& pxPerMm) const
    {
        if (userData_.empty()) return false;
        width = userWidth_;
        pxPerMm = userPxPerMm_;
        out = userData_;
        return true;
    }

    void setUserScore(std::vector<uint8_t>&& data, int width, int pxPerMm)
    {
        if (width <= 0 || width > 16384 || data.size() != (size_t)width * kTracks) return;
        userData_ = std::move(data);
        userWidth_ = width;
        userHeight_ = kTracks;
        userPxPerMm_ = pxPerMm > 0 ? pxPerMm : 10;
    }

    void freezeEdits(bool frozen)
    {
        editsFrozen_.store(frozen, std::memory_order_relaxed);
    }

    void setUserScorePath(const std::string& path) { userScorePath_ = path; }
    int userScoreWidth() const { return userWidth_; }
    int userScorePxPerMm() const { return userPxPerMm_; }
    uint8_t* userScoreData() { return userData_.empty() ? nullptr : userData_.data(); }
    const uint8_t* userScoreData() const { return userData_.empty() ? nullptr : userData_.data(); }
    float scorePosition01() const
    {
        return scoreDur_ > 0.0f ? (float)(scoreTime_ / (double)scoreDur_) : 0.0f;
    }
    float scoreDuration() const { return scoreDur_; }

    // Called by the UI test gate (a parameter, not MIDI).
    void setTestVoice(bool on, int note)
    {
        if (on && !testGatePrev_)
        {
            releaseTestVoices();
            startTestVoice(note);
        }
        else if (!on && testGatePrev_)
        {
            releaseTestVoices();
        }
        else if (on && note != testNotePrev_)
        {
            releaseTestVoices();
            startTestVoice(note);
        }
        testGatePrev_ = on;
        testNotePrev_ = note;
    }

private:
    struct CoderVoice
    {
        int note = -1;
        int count = 0;
        uint16_t track[kMaxHarmonics] = {};
        float gain[kMaxHarmonics] = {};
        float env = 0.0f;
        float target = 0.0f;
        float velGain = 1.0f;
        uint32_t order = 0;
        bool isTest = false;
    };

    static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    float trackOf(float hz) const
    {
        const float t = 72.0f * std::log2(hz / kF0);
        return clamp(t, 0.0f, (float)(kTracks - 1));
    }

    void selectScore(int index)
    {
        lastScoreIndex_ = index;
        scoreTime_ = 0.0;
        scanDir_ = s_.reverse ? -1 : 1;
        lastScanPos_ = s_.scanPos01;

        if (index >= 0 && index < kNumBuiltInScores)
            scoreDur_ = scores_[index].duration;
        else
            scoreDur_ = userWidth_ > 0 ? (float)userWidth_ / ((float)userPxPerMm_ * 24.0f) : 0.0f;
    }

    void updateControl()
    {
        realTime_ += (double)kCtrl / (double)sr_;

        if (std::fabs(s_.scanPos01 - lastScanPos_) > 1.0e-4f)
        {
            scoreTime_ = (double)clamp(s_.scanPos01, 0.0f, 1.0f) * (double)scoreDur_;
            lastScanPos_ = s_.scanPos01;
        }

        if (s_.editAudition && scoreDur_ > 0.0f)
        {
            scoreTime_ = (double)clamp(s_.editX01, 0.0f, 1.0f) * (double)scoreDur_;
            lastScanPos_ = s_.scanPos01;
        }
        else if (s_.run && scoreDur_ > 0.0f)
        {
            scoreTime_ += (double)scanDir_ * ((double)s_.speed / 24.0) * ((double)kCtrl / (double)sr_);
            if (s_.loop == 1)
            {
                while (scoreTime_ >= scoreDur_) scoreTime_ -= scoreDur_;
                while (scoreTime_ < 0.0) scoreTime_ += scoreDur_;
            }
            else if (s_.loop == 2)
            {
                if (scoreTime_ >= scoreDur_)
                {
                    scoreTime_ = 2.0 * scoreDur_ - scoreTime_;
                    scanDir_ = -1;
                }
                else if (scoreTime_ < 0.0)
                {
                    scoreTime_ = -scoreTime_;
                    scanDir_ = 1;
                }
            }
            else
            {
                scoreTime_ = (double)clamp((float)scoreTime_, 0.0f, scoreDur_);
            }
        }

        applyEdits();
        for (int n = 0; n < kTracks; ++n) target_[n] = 0.0f;

        if (lastScoreIndex_ >= 0 && scoreDur_ > 0.0f) evalScore();

        updateCoder();
        applyTrackNoiseAndBlur();

        for (int n = 0; n < kTracks; ++n)
            amp_[n] += ampCoef_ * (target_[n] - amp_[n]);

        scoreGainSm_ += 0.25f * (s_.scoreGain - scoreGainSm_);
        coderGainSm_ += 0.25f * (s_.coderGain - coderGainSm_);
        masterGainSm_ += 0.12f * (s_.volumeGain - masterGainSm_);
        for (int c = 0; c < kChannels; ++c)
            chGain_[c] += 0.25f * (targetChGain_[c] - chGain_[c]);

        wowPhase_ += (float)((double)s_.wowRate * ((double)kCtrl / (double)sr_));
        if (wowPhase_ >= 1.0f) wowPhase_ -= 1.0f;
        wowNoise_ = 0.99f * wowNoise_ + 0.01f * rng_.nextBipolar();

        if (s_.dynMode == 2)
        {
            for (int c = 0; c < kChannels; ++c)
            {
                lfoPhase_[c] += (float)((double)s_.dynRate * ((double)kCtrl / (double)sr_));
                if (lfoPhase_[c] >= 1.0f) lfoPhase_[c] -= 1.0f;
                dynLfo_[c] = fastSin01(lfoPhase_[c]);
            }
        }

        const float wow = 1.0f + 0.0020f * s_.wowDepth *
                          (0.7f * fastSin01(wowPhase_) + 0.3f * wowNoise_);
        float discMod[5];
        static const float kDiscRate[5] = { 0.09375f, 0.375f, 1.5f, 6.0f, 24.0f };
        for (int d = 0; d < 5; ++d)
            discMod[d] = 1.0f + 0.0008f * s_.wowDepth *
                         fastSin01((float)(realTime_ * (double)kDiscRate[d]) + 0.17f * (float)d);

        for (int n = 0; n < kTracks; ++n)
            inc_[n] = baseInc_[n] * wow * discMod[discOf_[n]];

        buildActiveList();
    }

    void applyEdits()
    {
        if (editsFrozen_.load(std::memory_order_relaxed)) return;
        if (userData_.empty() || userWidth_ <= 0) return;

        const int w = userWidth_;
        const int col = clampi((int)std::lround(clamp(s_.editX01, 0.0f, 1.0f) * (float)(w - 1)),
                               0, w - 1);
        float y = clamp(s_.editY01, 0.0f, 1.0f);
        if (s_.editFlipY) y = 1.0f - y;
        const int track = clampi((int)std::lround(y * (float)(kTracks - 1)), 0, kTracks - 1);

        if (s_.editClear && !lastEditClear_)
        {
            std::memset(userData_.data(), 0, userData_.size());
            lastEditActive_ = false;
        }
        lastEditClear_ = s_.editClear;

        const bool painting = s_.editDraw || s_.editErase;
        if (painting)
        {
            const bool erase = s_.editErase;
            const int level = erase ? 0
                : (int)std::lround(clamp(s_.editLevel, 0.0f, 1.0f) * 255.0f);
            if (lastEditActive_ && lastEditCol_ >= 0 &&
                (col != lastEditCol_ || track != lastEditTrack_))
            {
                const int steps = std::max(1, std::abs(col - lastEditCol_));
                for (int i = 1; i <= steps; ++i)
                {
                    const int c = lastEditCol_ + (col - lastEditCol_) * i / steps;
                    const int t = lastEditTrack_ + (track - lastEditTrack_) * i / steps;
                    paintEdit(c, t, level, erase);
                }
            }
            else
            {
                paintEdit(col, track, level, erase);
            }
            lastEditCol_ = col;
            lastEditTrack_ = track;
            lastEditActive_ = true;
        }
        else
        {
            lastEditActive_ = false;
        }
    }

    void paintEdit(int col, int track, int level, bool erase)
    {
        const int harm = clamp(s_.editHarm, 1, 16);
        const float width = (float)clamp(s_.editWidth, 1, 128);
        for (int h = 1; h <= harm; ++h)
        {
            const int tr = track + (int)std::lround(72.0f * std::log2((float)h));
            if (tr < 0) continue;
            if (tr >= kTracks) break;
            const int lev = (int)std::lround((float)level * std::pow((float)h, -0.5f));
            paintTrack(col, tr, lev, width, erase);
        }
    }

    void paintTrack(int col, int track, int level, float width, bool erase)
    {
        if (col < 0 || col >= userWidth_) return;
        const float half = width * 0.5f;
        int lo = (int)std::floor((float)track - half);
        int hi = (int)std::ceil((float)track + half);
        lo = clampi(lo, 0, kTracks - 1);
        hi = clampi(hi, 0, kTracks - 1);
        for (int t = lo; t <= hi; ++t)
        {
            const float ov = std::fmin((float)t + 0.5f, (float)track + half) -
                             std::fmax((float)t - 0.5f, (float)track - half);
            if (ov <= 0.0f) continue;
            const int row = kTracks - 1 - t;
            uint8_t& cell = userData_[(size_t)row * (size_t)userWidth_ + (size_t)col];
            if (erase)
            {
                cell = 0;
            }
            else
            {
                const int v = (int)std::lround((float)level * std::min(1.0f, ov));
                if (v > (int)cell) cell = (uint8_t)std::min(255, v);
            }
        }
    }

    void evalScore()
    {
        if (lastScoreIndex_ < kNumBuiltInScores)
        {
            const Score& sc = scores_[lastScoreIndex_];
            const float t = (float)scoreTime_;
            for (size_t i = 0; i < sc.strokes.size(); ++i)
            {
                const Stroke& st = sc.strokes[i];
                if (t < st.t0 || t > st.t1) continue;
                const float dur = st.t1 - st.t0;
                if (dur <= 0.0f) continue;

                const float u = (t - st.t0) / dur;
                const float p = st.p0 + (st.p1 - st.p0) * u;
                float env = 1.0f;
                if (st.fade > 0.0f)
                {
                    float f = st.fade;
                    if (f > 0.5f * dur) f = 0.5f * dur;
                    if (f > 0.0f)
                    {
                        if (t < st.t0 + f) env *= 0.5f - 0.5f * std::cos(kPi * (t - st.t0) / f);
                        if (t > st.t1 - f) env *= 0.5f - 0.5f * std::cos(kPi * (st.t1 - t) / f);
                    }
                }
                if (st.decay > 0.0f) env *= std::exp(-(t - st.t0) / st.decay);
                if (env < 1.0e-5f) continue;
                deposit(p, st.w, st.level * env * scoreGainSm_);
            }
        }
        else if (lastScoreIndex_ == kScoreUser && !userData_.empty())
        {
            const float u = scoreDur_ > 0.0f ? (float)(scoreTime_ / (double)scoreDur_) : 0.0f;
            const float x = clamp(u, 0.0f, 1.0f) * (float)(userWidth_ - 1);
            const int x0 = clampi((int)x, 0, userWidth_ - 1);
            const int x1 = clampi(x0 + 1, 0, userWidth_ - 1);
            const float fx = x - (float)x0;
            const uint8_t* data = userData_.data();
            for (int n = 0; n < kTracks; ++n)
            {
                const int row = kTracks - 1 - n;   // row 0 is the highest track
                if (row < 0 || row >= userHeight_) continue;
                const float a = (float)data[(size_t)row * userWidth_ + (size_t)x0];
                const float b = (float)data[(size_t)row * userWidth_ + (size_t)x1];
                const float v = (a + (b - a) * fx) * (1.0f / 255.0f);
                target_[n] += v * scoreGainSm_;
            }
        }
    }

    void deposit(float center, float width, float level)
    {
        const float half = 0.5f * width;
        int lo = (int)std::floor(center - half - 0.5f);
        int hi = (int)std::ceil(center + half + 0.5f);
        lo = clampi(lo, 0, kTracks - 1);
        hi = clampi(hi, 0, kTracks - 1);
        for (int i = lo; i <= hi; ++i)
        {
            const float ov = std::fmin((float)i + 0.5f, center + half) -
                             std::fmax((float)i - 0.5f, center - half);
            if (ov > 0.0f) target_[i] += level * ov;
        }
    }

    void applyTrackNoiseAndBlur()
    {
        if (s_.trackNoise > 0.0f)
        {
            if (++noiseCounter_ >= 8)
            {
                noiseCounter_ = 0;
                for (int n = 0; n < kTracks; ++n)
                    tn_[n] = 0.98f * tn_[n] + 0.02f * rng_.nextBipolar();
            }
            for (int n = 0; n < kTracks; ++n)
            {
                const float mul = clamp(1.0f + 2.0f * s_.trackNoise * tn_[n], 0.1f, 2.0f);
                target_[n] *= mul;
            }
        }
        else
        {
            noiseCounter_ = 0;
        }

        // Reader/aperture blur across the 720-track pitch axis.
        const float w0 = blurW_[0], w1 = blurW_[1], w2 = blurW_[2];
        if (w1 <= 0.0f && w2 <= 0.0f) return;

        blurTmp_[0] = w0 * target_[0] + w1 * target_[1] + w2 * target_[2];
        blurTmp_[1] = w0 * target_[1] + w1 * (target_[0] + target_[2]) + w2 * target_[3];
        for (int n = 2; n < kTracks - 2; ++n)
            blurTmp_[n] = w0 * target_[n] + w1 * (target_[n - 1] + target_[n + 1]) +
                          w2 * (target_[n - 2] + target_[n + 2]);
        blurTmp_[kTracks - 2] = w0 * target_[kTracks - 2] +
                                w1 * (target_[kTracks - 3] + target_[kTracks - 1]) +
                                w2 * target_[kTracks - 4];
        blurTmp_[kTracks - 1] = w0 * target_[kTracks - 1] +
                                w1 * target_[kTracks - 2] + w2 * target_[kTracks - 3];
        for (int n = 0; n < kTracks; ++n) target_[n] = blurTmp_[n];
    }

    void updateCoder()
    {
        setTestVoice(s_.testGate, s_.coderNote);

        for (int i = 0; i < kMaxCoderVoices; ++i)
        {
            CoderVoice& v = voices_[i];
            if (v.note < 0) continue;

            const float coef = (v.target > v.env) ? coderAttack_ : coderDecay_;
            v.env += coef * (v.target - v.env);
            if (v.target <= 0.0f && v.env < 1.0e-4f)
            {
                v.note = -1;
                continue;
            }

            if (coderDirty_) computeCoderGains(v);

            const float base = v.env * v.velGain * coderGainSm_;
            for (int h = 0; h < v.count; ++h)
                target_[v.track[h]] += base * v.gain[h];
        }
        if (coderDirty_) coderDirty_ = false;
    }

    void computeCoderGains(CoderVoice& v)
    {
        const float hz = midiToHz((float)v.note);
        float maxG = 1.0e-6f;
        for (int h = 0; h < v.count; ++h)
        {
            const float g = formantGain(hz * (float)(h + 1), s_.formant, s_.formantShift) *
                            harmRolloff_[h];
            v.gain[h] = g;
            if (g > maxG) maxG = g;
        }
        const float inv = 1.0f / maxG;
        for (int h = 0; h < v.count; ++h) v.gain[h] *= inv;
    }

    int findVoice()
    {
        for (int i = 0; i < kMaxCoderVoices; ++i)
            if (voices_[i].note < 0) return i;

        int best = -1;
        for (int i = 0; i < kMaxCoderVoices; ++i)
        {
            if (voices_[i].target > 0.0f) continue;
            if (best < 0 || voices_[i].env < voices_[best].env) best = i;
        }
        if (best >= 0) return best;

        best = 0;
        for (int i = 1; i < kMaxCoderVoices; ++i)
            if (voices_[i].order < voices_[best].order) best = i;
        return best;
    }

    void startTestVoice(int note)
    {
        int slot = findVoice();
        if (slot < 0) return;
        CoderVoice& v = voices_[slot];
        v = CoderVoice();
        v.note = note;
        v.target = 1.0f;
        v.velGain = 0.8f;
        v.order = ++voiceOrder_;
        v.isTest = true;

        const float hz = midiToHz((float)note);
        const int n0 = (int)std::lround(72.0f * std::log2(hz / kF0));
        const int maxH = clamp(s_.harmonics, 1, kMaxHarmonics);
        for (int h = 1; h <= maxH; ++h)
        {
            const int tr = n0 + harmOffset_[h - 1];
            if (tr < 0) continue;
            if (tr >= kTracks) break;
            v.track[v.count] = (uint16_t)tr;
            v.gain[v.count] = 1.0f;
            ++v.count;
        }
        if (v.count == 0) v.note = -1;
        else computeCoderGains(v);
    }

    void releaseTestVoices()
    {
        for (int i = 0; i < kMaxCoderVoices; ++i)
            if (voices_[i].isTest) voices_[i].target = 0.0f;
    }

    void buildActiveList()
    {
        for (int c = 0; c < kChannels; ++c)
        {
            const bool wasNoise = noiseCh_[c];
            const int base = c * kTracksPerChannel;
            int count = 0;
            float sumSq = 0.0f;
            for (int i = 0; i < kTracksPerChannel; ++i)
            {
                const float a = amp_[base + i];
                if (a > 1.0e-5f)
                {
                    ++count;
                    sumSq += a * a;
                }
            }
            // Hysteresis avoids rapid oscillator/noise switching on the threshold.
            if (count > kNoiseThreshold) noiseCh_[c] = true;
            else if (count < kNoiseThreshold - 4) noiseCh_[c] = false;
            else noiseCh_[c] = wasNoise;

            noiseGain_[c] = noiseCh_[c]
                ? std::sqrt(0.5f * sumSq) * kPartialScale * noiseCal_[c] : 0.0f;
        }

        activeCount_ = 0;
        for (int n = 0; n < kTracks; ++n)
            if (!noiseCh_[channelOf_[n]] && amp_[n] > 1.0e-5f)
                active_[activeCount_++] = (uint16_t)n;

        if (activeCount_ > kMaxPartials)
        {
            const int stride = (activeCount_ + kMaxPartials - 1) / kMaxPartials;
            const float gain = std::sqrt((float)stride) * kPartialScale;
            int k = 0;
            for (int i = 0; i < activeCount_; i += stride)
            {
                active_[k] = active_[i];
                activeGain_[k] = amp_[active_[i]] * gain;
                ++k;
            }
            activeCount_ = k;
        }
        else
        {
            for (int i = 0; i < activeCount_; ++i)
                activeGain_[i] = amp_[active_[i]] * kPartialScale;
        }
    }

    void addLine(Score& s, float t0, float t1, float hz0, float hz1,
                 float level, float width, float fade, float decay)
    {
        Stroke st;
        st.t0 = t0;
        st.t1 = t1;
        st.p0 = trackOf(hz0);
        st.p1 = trackOf(hz1);
        st.w = width;
        st.level = level;
        st.fade = fade;
        st.decay = decay;
        s.strokes.push_back(st);
        if (t1 > s.duration) s.duration = t1;
    }

    void addBand(Score& s, float t0, float t1, float level, float fade)
    {
        Stroke st;
        st.t0 = t0;
        st.t1 = t1;
        st.p0 = st.p1 = 360.0f;
        st.w = (float)kTracks;
        st.level = level;
        st.fade = fade;
        s.strokes.push_back(st);
        if (t1 > s.duration) s.duration = t1;
    }

    void addTone(Score& s, float t0, float t1, float f0, float level,
                 int harmonics, int formant, float shift, float width,
                 float fade, float decay)
    {
        for (int h = 1; h <= harmonics; ++h)
        {
            const float f = f0 * (float)h;
            if (f > 20500.0f) break;
            const float g = formantGain(f, formant, shift) * std::pow((float)h, -0.5f);
            if (g < 1.0e-4f) continue;
            addLine(s, t0, t1, f, f, level * g, width, fade, decay);
        }
    }

    void buildScores()
    {
        // 0: Vocal A - five vowel chords, 12 harmonics + sub-octave.
        {
            Score s; s.duration = 16.0f;
            static const int vowels[5] = { 1, 2, 3, 4, 5 };   // A E I O U
            for (int i = 0; i < 5; ++i)
            {
                const float t = (float)i * 3.2f;
                addTone(s, t, t + 2.7f, 110.0f, 0.62f, 28, vowels[i], 1.0f, 0.55f, 0.06f, 0.0f);
            }
            s.duration = 16.0f;
            scores_[kScoreVocalA] = std::move(s);
        }

        // 1: Vocal O - lower, darker chord cycle.
        {
            Score s; s.duration = 14.0f;
            for (int i = 0; i < 4; ++i)
            {
                const float t = (float)i * 3.5f;
                addTone(s, t, t + 3.0f, 98.0f, 0.66f, 24, 4, 1.0f, 0.55f, 0.08f, 0.0f);
            }
            s.duration = 14.0f;
            scores_[kScoreVocalO] = std::move(s);
        }

        // 2: Solaris - low drone, slow high clusters, drifting glissandi.
        {
            Score s; s.duration = 32.0f;
            addTone(s, 0.0f, 32.0f, 55.0f, 0.30f, 8, 0, 1.0f, 0.7f, 6.0f, 0.0f);
            Xorshift32 rng(0x5A17C0DEu);
            for (int i = 0; i < 14; ++i)
            {
                const float f0 = 700.0f * std::pow(2.0f, 0.23f * (float)i);
                const float f1 = f0 * (1.02f + 0.10f * rng.nextFloat());
                const float t0 = 1.2f * (float)i + 0.4f * rng.nextFloat();
                const float t1 = t0 + 7.0f + 2.0f * rng.nextFloat();
                const float lvl = 0.12f + 0.06f * rng.nextFloat();
                const float w = 1.0f + 2.5f * rng.nextFloat();
                addLine(s, t0, t1, f0, f1, lvl, w, 2.5f, 0.0f);
            }
            for (int i = 0; i < 8; ++i)
            {
                const float f = 3200.0f * std::pow(2.0f, 0.19f * (float)i);
                const float t0 = 3.0f * (float)i;
                addLine(s, t0, t0 + 6.0f, f, f * 1.06f, 0.075f, 2.0f, 2.0f, 0.0f);
            }
            s.duration = 32.0f;
            scores_[kScoreSolaris] = std::move(s);
        }

        // 3: Harm Glide - eight harmonics glissando, staggered.
        {
            Score s; s.duration = 12.0f;
            addLine(s, 0.0f, 12.0f, 55.0f, 55.0f, 0.18f, 0.7f, 0.2f, 0.0f);
            for (int h = 1; h <= 8; ++h)
            {
                const float t0 = 0.3f * (float)h;
                addLine(s, t0, 10.0f + 0.3f * (float)h, 110.0f * (float)h, 220.0f * (float)h,
                        0.5f * harmRolloff_[h - 1], 0.6f, 0.15f, 0.0f);
            }
            s.duration = 13.0f;
            scores_[kScoreHarmGlide] = std::move(s);
        }

        // 4: Micro Arp - 144 steps at the 1/72-octave grid (2 octaves).
        {
            Score s; s.duration = 17.3f;
            for (int i = 0; i < 144; ++i)
            {
                const int track = 144 + i;
                const float hz = kF0 * std::pow(2.0f, (float)track / 72.0f);
                const float t = 0.12f * (float)i;
                addLine(s, t, t + 0.10f, hz, hz, 0.50f, 0.45f, 0.004f, 0.0f);
                if ((i % 4) == 0)
                    addLine(s, t, t + 0.10f, hz * 2.0f, hz * 2.0f, 0.22f, 0.45f, 0.004f, 0.0f);
                if ((i % 12) == 0)
                    addLine(s, t, t + 0.40f, hz * 0.5f, hz * 0.5f, 0.28f, 0.55f, 0.01f, 0.0f);
            }
            s.duration = 17.28f;
            scores_[kScoreMicroArp] = std::move(s);
        }

        // 5: Noise Swell - full-height scratches (all 720 tones) plus a tone.
        {
            Score s; s.duration = 16.0f;
            addBand(s, 0.0f, 8.0f, 0.32f, 2.5f);
            addBand(s, 8.0f, 16.0f, 0.48f, 3.0f);
            addLine(s, 0.0f, 16.0f, 220.0f, 220.0f, 0.10f, 0.8f, 1.0f, 0.0f);
            addLine(s, 0.0f, 16.0f, 880.0f, 880.0f, 0.06f, 0.8f, 1.0f, 0.0f);
            s.duration = 16.0f;
            scores_[kScoreNoiseSwell] = std::move(s);
        }

        // 6: Bell Garden - inharmonic partials with exponential decays.
        {
            Score s; s.duration = 16.0f;
            static const float ratio[5] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f };
            static const float lvl[5] = { 0.55f, 0.36f, 0.26f, 0.18f, 0.12f };
            const float starts[8] = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f };
            const float roots[8] = { 220.0f, 330.0f, 220.0f, 330.0f, 220.0f, 330.0f, 220.0f, 330.0f };
            for (int b = 0; b < 8; ++b)
            {
                for (int k = 0; k < 5; ++k)
                    addLine(s, starts[b], starts[b] + 3.8f, roots[b] * ratio[k], roots[b] * ratio[k],
                            lvl[k], 0.5f, 0.002f, 1.1f);
            }
            s.duration = 18.0f;
            scores_[kScoreBellGarden] = std::move(s);
        }

        // 7: Empty - used by the Init program (live coder only).
        {
            Score s; s.duration = 1.0f;
            scores_[kScoreEmpty] = std::move(s);
        }
    }

    void loadUserScore()
    {
        userData_.clear();
        userWidth_ = 0;
        userHeight_ = 0;
        userPxPerMm_ = 10;

        if (userScorePath_.empty()) return;
        std::ifstream file(userScorePath_, std::ios::binary);
        if (!file) return;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        if (data.size() < 16) return;
        if (std::memcmp(data.data(), "ANS1", 4) != 0) return;

        const uint16_t version = rd16(data.data() + 4);
        const uint16_t w = rd16(data.data() + 6);
        const uint16_t h = rd16(data.data() + 8);
        const uint16_t px = rd16(data.data() + 10);
        const uint32_t bytes = rd32(data.data() + 12);
        if (version != 1 || w == 0 || h != kTracks) return;
        if (bytes != (uint32_t)w * (uint32_t)h) return;
        if (data.size() < 16u + (size_t)bytes) return;

        userWidth_ = w;
        userHeight_ = h;
        userPxPerMm_ = px > 0 ? px : 10;
        userData_.assign(data.begin() + 16, data.begin() + 16 + (size_t)bytes);
        std::fprintf(stderr, "ANS 1964: user score loaded (%u x %u, %u px/mm)\n",
                     (unsigned)w, (unsigned)h, (unsigned)userPxPerMm_);
    }

    static uint16_t rd16(const uint8_t* p)
    {
        return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
    }
    static uint32_t rd32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static const float* sineTable()
    {
        static float tab[2049];
        static bool built = false;
        if (!built)
        {
            for (int i = 0; i <= 2048; ++i)
                tab[i] = std::sin(kTwoPi * (float)i / 2048.0f);
            built = true;
        }
        return tab;
    }

    static float sinLut(float p)
    {
        const float* tab = sineTable();
        const float x = p * 2048.0f;
        const int i = (int)x;
        const float f = x - (float)i;
        return tab[i] + f * (tab[i + 1] - tab[i]);
    }

    // --- constants/state ----------------------------------------------------
    float sr_ = 44100.0f;
    float baseInc_[kTracks] = {};
    float inc_[kTracks] = {};
    float phase_[kTracks] = {};
    float amp_[kTracks] = {};
    float target_[kTracks] = {};
    float blurTmp_[kTracks] = {};
    float tn_[kTracks] = {};
    uint8_t channelOf_[kTracks] = {};
    uint8_t discOf_[kTracks] = {};
    uint16_t active_[kTracks] = {};
    float activeGain_[kTracks] = {};
    int activeCount_ = 0;

    BiquadCoeffs bpCoeff_[kChannels];
    Biquad bp1_[kChannels];
    Biquad bp2_[kChannels];
    float chGain_[kChannels] = {};
    float targetChGain_[kChannels] = {};
    float panL_[kChannels] = {};
    float panR_[kChannels] = {};
    float env_[kChannels] = {};
    float dynLfo_[kChannels] = {};
    float lfoPhase_[kChannels] = {};
    float noiseGain_[kChannels] = {};
    float noiseCal_[kChannels] = {};
    bool noiseCh_[kChannels] = {};
    WhiteNoise noiseSrc_[kChannels];
    Biquad noiseBp1_[kChannels];
    Biquad noiseBp2_[kChannels];

    OnePoleLP hfL_, hfR_;
    DCBlocker dcL_, dcR_;

    Score scores_[kNumBuiltInScores];
    std::string userScorePath_;
    std::vector<uint8_t> userData_;
    int userWidth_ = 0;
    int userHeight_ = 0;
    int userPxPerMm_ = 10;

    CoderVoice voices_[kMaxCoderVoices];
    int harmOffset_[kMaxHarmonics] = {};
    float harmRolloff_[kMaxHarmonics] = {};

    Settings s_;
    bool initialized_ = false;
    bool coderDirty_ = true;
    bool lastEditClear_ = false;
    int lastEditCol_ = -1;
    int lastEditTrack_ = -1;
    bool lastEditActive_ = false;
    std::atomic<bool> editsFrozen_{false};
    bool testGatePrev_ = false;
    int testNotePrev_ = -1;
    uint32_t voiceOrder_ = 0;
    Xorshift32 rng_{0x9E3779B9u};

    int ctrlCounter_ = 0;
    int noiseCounter_ = 0;
    double realTime_ = 0.0;
    double scoreTime_ = 0.0;
    float scoreDur_ = 0.0f;
    int scanDir_ = 1;
    int lastScoreIndex_ = -1;
    float lastScanPos_ = 0.0f;
    bool lastReverse_ = false;

    int lastFormant_ = -1;
    float lastFormantShift_ = 0.0f;
    float lastBlur_ = -1.0f;
    float blurW_[3] = { 0.0f, 0.25f, 0.25f };

    float envCoef_ = 0.03f;
    float ampCoef_ = 0.25f;
    float coderAttack_ = 0.07f;
    float coderDecay_ = 0.01f;
    float driveGain_ = 1.0f;
    float driveBias_ = 0.06f;
    float scoreGainSm_ = 1.0f;
    float coderGainSm_ = 0.5f;
    float masterGainSm_ = 0.5f;

    float wowPhase_ = 0.0f;
    float wowNoise_ = 0.0f;
    float humPhase_ = 0.0f;
};

} // namespace ans64

#endif // ANS1964_DSP_H
