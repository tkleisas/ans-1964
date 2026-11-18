#pragma once

#include <JuceHeader.h>
#include <vector>

namespace ansvst {

enum class PType { Float, Int, Bool, Choice };

struct ParamInfo
{
    juce::String id;
    juce::String name;
    PType type = PType::Float;
    float min = 0.0f;
    float max = 1.0f;
    float def = 0.0f;
    bool logarithmic = false;
    juce::StringArray choices;
};

const std::vector<ParamInfo>& paramInfo();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

namespace pid {
inline constexpr const char* volume = "volume";
inline constexpr const char* score = "score";
inline constexpr const char* run = "run";
inline constexpr const char* speed = "speed";
inline constexpr const char* backward = "reverse";
inline constexpr const char* loop = "loop";
inline constexpr const char* scanPos = "scan_pos";
inline constexpr const char* scoreGain = "score_gain";
inline constexpr const char* coderLevel = "coder_level";
inline constexpr const char* coderHarm = "coder_harm";
inline constexpr const char* coderFormant = "coder_formant";
inline constexpr const char* coderShift = "coder_shift";
inline constexpr const char* coderDecay = "coder_decay";
inline constexpr const char* coderNote = "coder_note";
inline constexpr const char* coderGate = "coder_gate";
inline constexpr const char* wowDepth = "wow_depth";
inline constexpr const char* wowRate = "wow_rate";
inline constexpr const char* trackNoise = "track_noise";
inline constexpr const char* blur = "blur";
inline constexpr const char* hfLoss = "hf_loss";
inline constexpr const char* hum = "hum";
inline constexpr const char* dynMode = "dyn_mode";
inline constexpr const char* dynAmount = "dyn_amount";
inline constexpr const char* dynRate = "dyn_rate";
inline constexpr const char* drive = "drive";
inline constexpr const char* lineMode = "line_mode";
inline constexpr const char* width = "width";
inline constexpr const char* editX = "edit_x";
inline constexpr const char* editY = "edit_y";
inline constexpr const char* editLevel = "edit_level";
inline constexpr const char* editWidth = "edit_width";
inline constexpr const char* editHarm = "edit_harm";
inline constexpr const char* editDraw = "edit_draw";
inline constexpr const char* editErase = "edit_erase";
inline constexpr const char* editClear = "edit_clear";
inline constexpr const char* editAudition = "edit_audition";
inline constexpr const char* editFlipY = "edit_flip_y";

inline juce::String channel(int oneBased)
{
    return "ch" + juce::String(oneBased);
}
} // namespace pid

} // namespace ansvst
