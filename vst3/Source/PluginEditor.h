#pragma once

#include <JuceHeader.h>
#include <memory>

class ANS1964AudioProcessor;
class ANS1964ScoreCanvas;
class ANS1964EditPanel;
class ANS1964ParamsPanel;

class ANS1964AudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    enum class Tool { Draw, Erase, Harmonic };

    explicit ANS1964AudioProcessorEditor(ANS1964AudioProcessor&);
    ~ANS1964AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    ANS1964AudioProcessor& processor() { return proc; }

    Tool tool() const { return currentTool; }
    float brushLevel() const { return currentLevel; }
    int brushWidth() const { return currentWidth; }
    int brushHarmonics() const { return currentHarmonics; }
    void setTool(Tool t) { currentTool = t; }
    void setBrushLevel(float v) { currentLevel = juce::jlimit(0.0f, 1.0f, v); }
    void setBrushWidth(int v) { currentWidth = juce::jlimit(1, 128, v); }
    void setBrushHarmonics(int v) { currentHarmonics = juce::jlimit(1, 16, v); }

    void setStatus(const juce::String& text);
    void refreshCanvasFromProcessor();
    void fitCanvas();
    void setCanvasZoom(float newZoom);

    void openScore();
    void saveScore();
    void importImage();
    void exportImage();
    void newScore();
    void clearScore();
    void undo();
    void redo();

    juce::Viewport& canvasViewport() { return viewport; }

private:
    void timerCallback() override;
    void chooseFile(bool save, bool png, bool exportMode);
    void applyChosenFile(const juce::File&, bool save, bool png, bool exportMode);
    void notifyLoadError(const juce::String& what);

    ANS1964AudioProcessor& proc;
    juce::TextButton openButton { "Open" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton importButton { "Import PNG" };
    juce::TextButton exportButton { "Export PNG" };
    juce::TextButton newButton { "New" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton zoomOutButton { "Zoom -" };
    juce::TextButton zoomInButton { "Zoom +" };
    juce::TextButton fitButton { "Fit" };
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label toolLabel;

    std::unique_ptr<ANS1964ScoreCanvas> canvas;
    std::unique_ptr<ANS1964EditPanel> editPanel;
    std::unique_ptr<ANS1964ParamsPanel> paramsPanel;
    juce::Viewport viewport;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<juce::FileChooser> chooser;

    Tool currentTool = Tool::Draw;
    float currentLevel = 0.8f;
    int currentWidth = 8;
    int currentHarmonics = 1;
    float zoom = 0.8f;
    int lastRevision = -1;
    bool didInitialFit = false;
    double lastTimerTime = 0.0;
    float previewPlayhead = 0.0f;
    float previewDirection = 1.0f;
    bool wasRunning = false;
    bool lastBackward = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ANS1964AudioProcessorEditor)
};
