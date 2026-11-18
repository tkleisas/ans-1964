#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <vector>

#include "ANSDsp.h"
#include "ANSParams.h"

namespace Steinberg { namespace Vst { struct IComponentHandler; } }

class ANS1964AudioProcessor : public juce::AudioProcessor,
                              public juce::VST3ClientExtensions
{
public:
    struct ScoreView
    {
        std::vector<uint8_t> data;
        int width = 0;
        int pxPerMm = 10;
    };

    ANS1964AudioProcessor();
    ~ANS1964AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ANS 1964"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // VST3 extension: the editor uses setDirty so hosts with process-isolated
    // editors (YAWN) pull the updated score/state back to the audio instance.
    void setIComponentHandler(Steinberg::FUnknown* handler) override;

    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }

    ScoreView getScoreView() const;
    int scoreRevision() const { return scoreRevision_.load(); }
    float scorePosition01() const { return engine_.scorePosition01(); }
    float scoreDuration() const { return engine_.scoreDuration(); }

    void paintScoreAt(int col, float track, int level, float brushTracks,
                      bool erase, int harmonics);
    void replaceScore(std::vector<uint8_t> data, int width, int px, bool notify);
    void newBlankScore(int width);
    void markScoreEdited();

    bool loadScoreFile(const juce::File& file);
    bool saveScoreFile(const juce::File& file) const;
    bool importImageFile(const juce::File& file);
    bool exportImageFile(const juce::File& file) const;

    static juce::Image scoreToImage(const uint8_t* data, int width);
    static bool imageToScore(const juce::Image& image, std::vector<uint8_t>& out, int& width);

private:
    float plain(const char* id) const;
    ans64::Settings makeSettings() const;
    void loadDemoIfNeeded();

    juce::AudioProcessorValueTreeState apvts_;
    ans64::Engine engine_;
    mutable std::mutex scoreMutex_;
    std::atomic<int> scoreRevision_ { 0 };
    std::atomic<Steinberg::Vst::IComponentHandler*> componentHandler_ { nullptr };
    bool stateScoreLoaded_ = false;
    bool initialScoreSet_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ANS1964AudioProcessor)
};
