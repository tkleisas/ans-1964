#include "ANSParams.h"

#include <cmath>

namespace ansvst {

const std::vector<ParamInfo>& paramInfo()
{
    static const std::vector<ParamInfo> info = []
    {
        std::vector<ParamInfo> p;

        auto F = [&](const char* id, const char* name, float lo, float hi, float def,
                     bool log = false)
        {
            p.push_back({ id, name, PType::Float, lo, hi, def, log, {} });
        };
        auto I = [&](const char* id, const char* name, int lo, int hi, int def)
        {
            p.push_back({ id, name, PType::Int, (float)lo, (float)hi, (float)def, false, {} });
        };
        auto B = [&](const char* id, const char* name, bool def)
        {
            p.push_back({ id, name, PType::Bool, 0.0f, 1.0f, def ? 1.0f : 0.0f, false, {} });
        };
        auto C = [&](const char* id, const char* name, juce::StringArray choices, int def)
        {
            p.push_back({ id, name, PType::Choice, 0.0f,
                          (float)juce::jmax(0, choices.size() - 1), (float)def, false,
                          std::move(choices) });
        };

        F(pid::volume, "Volume", -40.0f, 6.0f, -6.0f);
        // VST3 default: the user score is the point of the visual editor.
        C(pid::score, "Score",
          { "Vocal A", "Vocal O", "Solaris", "Harm Glide", "Micro Arp",
            "Noise Swell", "Bell Garden", "Empty", "User Score" }, 8);
        B(pid::run, "Run", false);
        F(pid::speed, "Speed", 0.0f, 24.0f, 12.0f);
        B(pid::backward, "Backward", false);
        C(pid::loop, "Loop", { "One-shot", "Loop", "Ping-Pong" }, 1);
        F(pid::scanPos, "Scan Pos", 0.0f, 100.0f, 0.0f);
        F(pid::scoreGain, "Score Level", -40.0f, 6.0f, 0.0f);

        for (int i = 1; i <= 20; ++i)
        {
            const auto id = pid::channel(i);
            const auto name = "Ch " + juce::String(i);
            p.push_back({ id, name, PType::Float, 0.0f, 100.0f, 80.0f, false, {} });
        }

        F(pid::coderLevel, "Coder Level", -40.0f, 6.0f, -6.0f);
        I(pid::coderHarm, "Harmonics", 1, 16, 16);
        C(pid::coderFormant, "Formant",
          { "Flat", "A", "E", "I", "O", "U", "Bright", "Dark" }, 1);
        F(pid::coderShift, "Frmt Shift", 0.5f, 2.0f, 1.0f);
        F(pid::coderDecay, "Coder Decay", 20.0f, 5000.0f, 400.0f, true);
        I(pid::coderNote, "Coder Note", 24, 96, 60);
        B(pid::coderGate, "Coder Gate", false);

        F(pid::wowDepth, "Wow Depth", 0.0f, 100.0f, 10.0f);
        F(pid::wowRate, "Wow Rate", 0.05f, 8.0f, 0.7f, true);
        F(pid::trackNoise, "Track Noise", 0.0f, 100.0f, 10.0f);
        F(pid::blur, "Blur", 0.0f, 100.0f, 40.0f);
        F(pid::hfLoss, "HF Loss", 0.0f, 100.0f, 30.0f);
        F(pid::hum, "Hum", 0.0f, 100.0f, 0.0f);
        C(pid::dynMode, "Dyn Mode", { "Open", "Dynamics", "Modulation" }, 0);
        F(pid::dynAmount, "Dyn Amount", 0.0f, 100.0f, 50.0f);
        F(pid::dynRate, "Dyn Rate", 0.05f, 8.0f, 1.2f, true);
        F(pid::drive, "Drive", 0.0f, 100.0f, 20.0f);
        C(pid::lineMode, "Line Mode", { "Mono", "Split", "Stereo" }, 0);
        F(pid::width, "Width", 0.0f, 100.0f, 50.0f);

        F(pid::editX, "Edit X", 0.0f, 100.0f, 50.0f);
        F(pid::editY, "Edit Y", 0.0f, 100.0f, 50.0f);
        F(pid::editLevel, "Brush Lvl", 0.0f, 100.0f, 80.0f);
        I(pid::editWidth, "Brush Wd", 1, 128, 8);
        I(pid::editHarm, "Edit Harm", 1, 16, 1);
        B(pid::editDraw, "Draw", false);
        B(pid::editErase, "Erase", false);
        B(pid::editClear, "Clear", false);
        B(pid::editAudition, "Audition", false);
        B(pid::editFlipY, "Flip Y", false);

        return p;
    }();
    return info;
}

static juce::NormalisableRange<float> makeRange(const ParamInfo& p)
{
    if (p.logarithmic && p.min > 0.0f && p.max > p.min)
    {
        return juce::NormalisableRange<float>(
            p.min, p.max,
            [](float a, float b, float t) { return a * std::pow(b / a, t); },
            [](float a, float b, float v)
            {
                return std::log(v / a) / std::log(b / a);
            },
            [](float a, float b, float v) { return juce::jlimit(a, b, v); });
    }
    return juce::NormalisableRange<float>(p.min, p.max);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : paramInfo())
    {
        const juce::ParameterID id { p.id, 1 };
        switch (p.type)
        {
            case PType::Float:
            {
                auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [](float value, int) { return juce::String(value, 2); });
                layout.add(std::make_unique<juce::AudioParameterFloat>(
                    id, p.name, makeRange(p), p.def, attrs));
                break;
            }
            case PType::Int:
                layout.add(std::make_unique<juce::AudioParameterInt>(
                    id, p.name, (int)p.min, (int)p.max, (int)p.def));
                break;
            case PType::Bool:
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    id, p.name, p.def >= 0.5f));
                break;
            case PType::Choice:
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    id, p.name, p.choices, (int)p.def));
                break;
        }
    }
    return layout;
}

} // namespace ansvst
