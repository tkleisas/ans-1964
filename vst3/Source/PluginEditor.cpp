#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ANSPaint.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <thread>

namespace {

juce::Colour ansColour(uint8_t v)
{
    struct Stop { int at; uint8_t r, g, b; };
    static const Stop stops[] = {
        { 0, 8, 8, 10 }, { 36, 42, 14, 10 }, { 90, 120, 42, 12 },
        { 150, 210, 105, 24 }, { 210, 255, 185, 70 }, { 255, 255, 248, 220 }
    };
    for (size_t i = 0; i + 1 < std::size(stops); ++i)
    {
        if (v <= stops[i + 1].at)
        {
            const auto& a = stops[i];
            const auto& b = stops[i + 1];
            const float t = b.at == a.at ? 0.0f
                                         : (float)(v - a.at) / (float)(b.at - a.at);
            return juce::Colour((uint8_t)std::lround(a.r + (b.r - a.r) * t),
                                (uint8_t)std::lround(a.g + (b.g - a.g) * t),
                                (uint8_t)std::lround(a.b + (b.b - a.b) * t));
        }
    }
    return juce::Colour(255, 248, 220);
}

juce::Font smallFont(float size = 11.0f)
{
    return juce::Font(size);
}

} // namespace

// ===========================================================================
// Score canvas
// ===========================================================================
class ANS1964ScoreCanvas : public juce::Component
{
public:
    ANS1964ScoreCanvas(ANS1964AudioProcessor& p, ANS1964AudioProcessorEditor& e)
        : proc(p), owner(e)
    {
        setOpaque(true);
        setWantsKeyboardFocus(true);
    }

    void refreshFromProcessor()
    {
        auto v = proc.getScoreView();
        const bool widthChanged = (v.width != width);
        score = std::move(v.data);
        width = v.width;
        pxPerMm = v.pxPerMm;
        if (widthChanged)
        {
            undoStack.clear();
            redoStack.clear();
        }
        rebuildCache();
        setSize(std::max(1, (int)std::lround(width * _zoom)),
                std::max(1, (int)std::lround(720.0 * _zoom)));
        repaint();
    }

    void setZoom(float z)
    {
        _zoom = juce::jlimit(0.2f, 3.0f, z);
        setSize(std::max(1, (int)std::lround(width * _zoom)),
                std::max(1, (int)std::lround(720.0 * _zoom)));
        repaint();
    }

    float getZoom() const { return _zoom; }
    int getScoreWidth() const { return width; }
    int getPxPerMm() const { return pxPerMm; }
    bool isDragging() const { return dragging; }
    void setPlayhead(float columns) { playhead = columns; }

    void undo()
    {
        if (undoStack.empty()) return;
        redoStack.push_back(score);
        score = undoStack.back();
        undoStack.pop_back();
        pushScoreToProcessor();
    }

    void redo()
    {
        if (redoStack.empty()) return;
        undoStack.push_back(score);
        score = redoStack.back();
        redoStack.pop_back();
        pushScoreToProcessor();
    }

    void clearUndoHistory()
    {
        undoStack.clear();
        redoStack.clear();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0b0a08));
        if (cache.isValid() && width > 0)
        {
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImage(cache, getLocalBounds().toFloat());
        }
        else
        {
            g.setColour(juce::Colours::orange);
            g.setFont(smallFont(16.0f));
            g.drawText("No user score", getLocalBounds(), juce::Justification::centred);
            return;
        }

        const float z = _zoom;
        // Channel / octave grid.
        for (int track = 0; track <= 720; track += 36)
        {
            const float y = (float)(719 - std::min(719, track)) * z;
            const bool octave = (track % 72) == 0;
            g.setColour(juce::Colour(octuateColour(octave)));
            g.drawHorizontalLine((int)std::lround(y), 0.0f, (float)getWidth());
            if (octave && y > 12.0f)
            {
                const float hz = 21.09f * std::pow(2.0f, (float)(719 - std::min(719, track)) / 72.0f);
                g.setColour(juce::Colour(0xffd8c69c));
                g.setFont(smallFont(10.0f));
                g.drawText(juce::String((int)std::lround(hz)) + " Hz",
                           4, (int)std::lround(y) - 12, 90, 12,
                           juce::Justification::centredLeft);
            }
        }
        // Time grid: one second at 24 mm/s.
        const float pxPerSecond = std::max(1.0f, (float)pxPerMm * 24.0f);
        for (float s = 0.0f; ; s += 1.0f)
        {
            const float x = s * pxPerSecond * z;
            if (x > (float)getWidth()) break;
            g.setColour(juce::Colour(0x303f382a));
            g.drawVerticalLine((int)std::lround(x), 0.0f, (float)getHeight());
            g.setColour(juce::Colour(0xffbfae8a));
            g.setFont(smallFont(10.0f));
            g.drawText(juce::String(s, 0) + "s", (int)std::lround(x) + 3, 3, 40, 12,
                       juce::Justification::centredLeft);
        }

        if (playhead >= 0.0f && playhead <= (float)width)
        {
            g.setColour(juce::Colour(0xfffff3cc));
            g.drawVerticalLine((int)std::lround(playhead * z), 0.0f, (float)getHeight());
        }

        g.setColour(juce::Colour(0xff3a3327));
        g.drawRect(getLocalBounds(), 1);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (width <= 0 || score.empty()) return;
        pushUndo();
        dragging = true;
        eraseDrag = e.mods.isRightButtonDown() || owner.tool() == ANS1964AudioProcessorEditor::Tool::Erase;
        lastCol = -1;
        lastTrack = 0.0f;
        const auto [col, track] = mapMouse(e);
        strokeTo(col, track);
        updateStatus(col, track);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;
        const auto [col, track] = mapMouse(e);
        const int fromCol = lastCol;
        const float fromTrack = lastTrack;
        if (fromCol < 0)
        {
            strokeTo(col, track);
            return;
        }
        const int steps = std::max(1, std::abs(col - fromCol));
        const int dir = col >= fromCol ? 1 : -1;
        for (int i = 1; i <= steps; ++i)
        {
            const int c = fromCol + dir * i;
            const float t = fromTrack + (track - fromTrack) * ((float)i / (float)steps);
            strokeTo(c, t);
        }
        updateStatus(col, track);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (!dragging) return;
        dragging = false;
        proc.markScoreEdited();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const auto [col, track] = mapMouse(e);
        updateStatus(col, track);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

private:
    float _zoom = 0.8f;

    static juce::uint32 octuateColour(bool octave)
    {
        return octave ? 0x504b3f2e : 0x302e2a22;
    }

    std::pair<int, float> mapMouse(const juce::MouseEvent& e) const
    {
        const float z = std::max(0.01f, _zoom);
        const int col = juce::jlimit(0, std::max(0, width - 1), (int)std::floor((float)e.x / z));
        const float y = juce::jlimit(0.0f, 719.0f, (float)e.y / z);
        return { col, 719.0f - y };
    }

    void updateStatus(int col, float track)
    {
        const float seconds = (float)col / std::max(1.0f, (float)pxPerMm * 24.0f);
        const float hz = 21.09f * std::pow(2.0f, track / 72.0f);
        owner.setStatus("Time " + juce::String(seconds, 2) + " s / "
                        + juce::String((float)width / ((float)pxPerMm * 24.0f), 2)
                        + " s   Track " + juce::String((int)std::lround(track))
                        + "   " + juce::String(hz, 1) + " Hz");
    }

    void pushUndo()
    {
        undoStack.push_back(score);
        if (undoStack.size() > 20) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    void strokeTo(int col, float track)
    {
        if (width <= 0 || col < 0 || col >= width || score.empty()) return;
        const bool erase = eraseDrag;
        const int level = erase ? 0 : (int)std::lround(owner.brushLevel() * 255.0f);
        const float bw = (float)owner.brushWidth();
        const int harm = owner.brushHarmonics();

        if (!erase)
        {
            ansvst::paintScoreAt(score.data(), width, col, track, level, bw, false, harm);
            proc.paintScoreAt(col, track, level, bw, false, harm);
        }
        else
        {
            ansvst::paintScoreAt(score.data(), width, col, track, 0, bw, true, harm);
            proc.paintScoreAt(col, track, 0, bw, true, harm);
        }

        if (lastCol < 0) lastCol = col;
        const int c0 = std::min(lastCol, col);
        const int c1 = std::max(lastCol, col);
        lastCol = col;
        lastTrack = track;
        updateCacheColumns(c0, c1);
        repaint();
    }

    void rebuildCache()
    {
        if (width <= 0 || score.empty())
        {
            cache = {};
            return;
        }
        cache = juce::Image(juce::Image::RGB, width, 720, true);
        juce::Image::BitmapData bd(cache, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 720; ++y)
            for (int x = 0; x < width; ++x)
                bd.setPixelColour(x, y, ansColour(score[(size_t)y * (size_t)width + (size_t)x]));
    }

    void updateCacheColumns(int c0, int c1)
    {
        if (!cache.isValid() || width <= 0) return;
        c0 = juce::jlimit(0, width - 1, c0);
        c1 = juce::jlimit(0, width - 1, c1);
        juce::Image::BitmapData bd(cache, juce::Image::BitmapData::writeOnly);
        for (int x = c0; x <= c1; ++x)
            for (int y = 0; y < 720; ++y)
                bd.setPixelColour(x, y, ansColour(score[(size_t)y * (size_t)width + (size_t)x]));
    }

    void pushScoreToProcessor()
    {
        if (width <= 0 || score.empty()) return;
        proc.replaceScore(score, width, pxPerMm, true);
        rebuildCache();
        repaint();
    }

    ANS1964AudioProcessor& proc;
    ANS1964AudioProcessorEditor& owner;
    std::vector<uint8_t> score;
    int width = 0;
    int pxPerMm = 10;
    juce::Image cache;
    bool dragging = false;
    bool eraseDrag = false;
    int lastCol = -1;
    float lastTrack = 0.0f;
    float playhead = -1.0f;
    std::vector<std::vector<uint8_t>> undoStack, redoStack;
};

// ===========================================================================
// Edit panel
// ===========================================================================
class ANS1964EditPanel : public juce::Component
{
public:
    ANS1964EditPanel(ANS1964AudioProcessor& p, ANS1964AudioProcessorEditor& e)
        : proc(p), editor(e)
    {
        addLabel("SCORE / SCAN", 12, 6);
        addLabel("Score", 12, 26);
        scoreBox.addItemList({ "Vocal A", "Vocal O", "Solaris", "Harm Glide", "Micro Arp",
                               "Noise Swell", "Bell Garden", "Empty", "User Score" }, 1);
        addAndMakeVisible(scoreBox);
        scoreAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.apvts(), ansvst::pid::score, scoreBox);

        useUserButton.setButtonText("Use User Score");
        useUserButton.onClick = [this]
        {
            if (auto* p = proc.apvts().getParameter(ansvst::pid::score))
                p->setValueNotifyingHost(p->convertTo0to1(8.0f));
        };
        addAndMakeVisible(useUserButton);

        runButton.setButtonText("Run");
        backButton.setButtonText("Backward");
        runButton.setClickingTogglesState(true);
        backButton.setClickingTogglesState(true);
        addAndMakeVisible(runButton);
        addAndMakeVisible(backButton);
        runAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts(), ansvst::pid::run, runButton);
        backAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.apvts(), ansvst::pid::backward, backButton);

        addLabel("Loop", 12, 144);
        loopBox.addItemList({ "One-shot", "Loop", "Ping-Pong" }, 1);
        addAndMakeVisible(loopBox);
        loopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.apvts(), ansvst::pid::loop, loopBox);

        addSlider("Speed", speed, 12, 196, ansvst::pid::speed);
        addSlider("Scan Pos", scanPos, 176, 196, ansvst::pid::scanPos);
        addSlider("Score Level", scoreLevel, 12, 250, ansvst::pid::scoreGain);
        addSlider("Volume", volume, 176, 250, ansvst::pid::volume);

        addLabel("TOOLS", 12, 304);
        drawButton.setButtonText("Draw");
        eraseButton.setButtonText("Erase");
        harmButton.setButtonText("Harmonic");
        for (auto* b : { &drawButton, &eraseButton, &harmButton })
        {
            b->setClickingTogglesState(true);
            b->setRadioGroupId(0x616e73);
            addAndMakeVisible(b);
        }
        drawButton.setToggleState(true, juce::dontSendNotification);
        drawButton.onClick = [this] { editor.setTool(ANS1964AudioProcessorEditor::Tool::Draw); };
        eraseButton.onClick = [this] { editor.setTool(ANS1964AudioProcessorEditor::Tool::Erase); };
        harmButton.onClick = [this] { editor.setTool(ANS1964AudioProcessorEditor::Tool::Harmonic); };

        brushLabel.setText("Brush level / width / harmonics", juce::dontSendNotification);
        brushLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcfc0a0));
        addAndMakeVisible(brushLabel);

        brushLevel.setRange(0.0, 100.0, 1.0);
        brushLevel.setValue((double)editor.brushLevel() * 100.0);
        brushLevel.setSliderStyle(juce::Slider::LinearHorizontal);
        brushLevel.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        brushLevel.onValueChange = [this] { editor.setBrushLevel((float)brushLevel.getValue() / 100.0f); };
        addAndMakeVisible(brushLevel);

        brushWidth.setRange(1.0, 128.0, 1.0);
        brushWidth.setValue(editor.brushWidth());
        brushWidth.setSliderStyle(juce::Slider::LinearHorizontal);
        brushWidth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        brushWidth.onValueChange = [this] { editor.setBrushWidth((int)brushWidth.getValue()); };
        addAndMakeVisible(brushWidth);

        brushHarm.setRange(1.0, 16.0, 1.0);
        brushHarm.setValue(editor.brushHarmonics());
        brushHarm.setSliderStyle(juce::Slider::LinearHorizontal);
        brushHarm.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        brushHarm.onValueChange = [this] { editor.setBrushHarmonics((int)brushHarm.getValue()); };
        addAndMakeVisible(brushHarm);
    }

    void resized() override
    {
        const int x0 = 12, x1 = 176, w = 150;
        scoreBox.setBounds(x0, 44, 300, 26);
        useUserButton.setBounds(x0, 78, 140, 24);
        runButton.setBounds(x0, 110, 88, 24);
        backButton.setBounds(104, 110, 120, 24);
        loopBox.setBounds(x0, 160, 140, 26);
        speed.setBounds(x0, 198, w, 26);
        scanPos.setBounds(x1, 198, w, 26);
        scoreLevel.setBounds(x0, 258, w, 26);
        volume.setBounds(x1, 258, w, 26);
        drawButton.setBounds(x0, 324, 88, 24);
        eraseButton.setBounds(104, 324, 88, 24);
        harmButton.setBounds(196, 324, 104, 24);
        brushLabel.setBounds(x0, 358, 300, 18);
        brushLevel.setBounds(x0, 376, 300, 24);
        brushWidth.setBounds(x0, 404, 300, 24);
        brushHarm.setBounds(x0, 432, 300, 24);
    }

private:
    void addLabel(const juce::String& text, int x, int y)
    {
        auto* l = new juce::Label();
        l->setText(text, juce::dontSendNotification);
        l->setColour(juce::Label::textColourId, juce::Colour(0xffd6c49c));
        l->setFont(juce::Font(11.0f, juce::Font::bold));
        l->setBounds(x, y, 310, 16);
        addAndMakeVisible(l);
    }

    void addSlider(const juce::String& name, juce::Slider& slider, int x, int y, const char* id)
    {
        auto* l = new juce::Label();
        l->setText(name, juce::dontSendNotification);
        l->setColour(juce::Label::textColourId, juce::Colour(0xffcfc0a0));
        l->setFont(juce::Font(10.0f));
        l->setBounds(x, y, 150, 14);
        addAndMakeVisible(l);

        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 18);
        slider.setNumDecimalPlacesToDisplay(2);
        slider.setBounds(x, y + 16, 150, 24);
        addAndMakeVisible(slider);
        sliderAttachments.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                proc.apvts(), id, slider));
    }

    ANS1964AudioProcessor& proc;
    ANS1964AudioProcessorEditor& editor;
    juce::ComboBox scoreBox, loopBox;
    juce::TextButton useUserButton, runButton, backButton;
    juce::TextButton drawButton, eraseButton, harmButton;
    juce::Label brushLabel;
    juce::Slider speed, scanPos, scoreLevel, volume;
    juce::Slider brushLevel, brushWidth, brushHarm;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scoreAtt, loopAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAtt, backAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
};

// ===========================================================================
// Generic parameter panel
// ===========================================================================
class ANS1964ParamsPanel : public juce::Component
{
public:
    explicit ANS1964ParamsPanel(ANS1964AudioProcessor& p) : proc(p)
    {
        addAndMakeVisible(view);
        view.setViewedComponent(&content, false);
        view.setScrollBarsShown(true, true);

        const juce::StringArray skip {
            ansvst::pid::score, ansvst::pid::run, ansvst::pid::speed,
            ansvst::pid::backward, ansvst::pid::loop, ansvst::pid::scanPos,
            ansvst::pid::scoreGain, ansvst::pid::volume
        };

        juce::String lastGroup;
        int y = 8;
        int col = 0;
        for (const auto& p : ansvst::paramInfo())
        {
            if (skip.contains(p.id)) continue;
            const auto group = groupFor(p.id);
            if (group != lastGroup)
            {
                if (col != 0) { y += 58; col = 0; }
                addHeader(group, y);
                y += 22;
                lastGroup = group;
            }

            const int x = col == 0 ? 10 : 175;
            addParam(p, x, y);
            ++col;
            if (col == 2) { col = 0; y += 58; }
        }
        contentHeight = y + 70;
        content.setSize(340, contentHeight);
    }

    ~ANS1964ParamsPanel() override
    {
        view.setViewedComponent(nullptr, false);
    }

    void resized() override
    {
        view.setBounds(getLocalBounds());
        content.setSize(std::max(340, view.getWidth() - 2), contentHeight);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff14110d)); }

private:
    static juce::String groupFor(const juce::String& id)
    {
        if (id.startsWith("ch")) return "CHANNELS";
        if (id.startsWith("coder")) return "CODER";
        if (id.startsWith("wow") || id.startsWith("track") || id == "blur" ||
            id == "hf_loss" || id == "hum") return "OPTICS";
        if (id.startsWith("dyn") || id == "drive") return "DYNAMICS";
        if (id == "line_mode" || id == "width") return "OUTPUT";
        if (id.startsWith("edit")) return "EDIT PARAMS";
        return "MORE";
    }

    void addHeader(const juce::String& text, int y)
    {
        auto* l = new juce::Label();
        l->setText(text, juce::dontSendNotification);
        l->setFont(juce::Font(12.0f, juce::Font::bold));
        l->setColour(juce::Label::textColourId, juce::Colour(0xffe1b96a));
        l->setBounds(10, y, 320, 18);
        content.addAndMakeVisible(l);
    }

    void addParam(const ansvst::ParamInfo& info, int x, int y)
    {
        auto* label = new juce::Label();
        label->setText(info.name, juce::dontSendNotification);
        label->setFont(juce::Font(10.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffc9b895));
        label->setBounds(x, y, 155, 14);
        content.addAndMakeVisible(label);

        if (info.type == ansvst::PType::Choice)
        {
            auto* cb = new juce::ComboBox();
            cb->addItemList(info.choices, 1);
            cb->setBounds(x, y + 16, 155, 24);
            content.addAndMakeVisible(cb);
            comboAttachments.push_back(
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                    proc.apvts(), info.id, *cb));
        }
        else if (info.type == ansvst::PType::Bool)
        {
            auto* b = new juce::ToggleButton(info.name);
            b->setBounds(x, y + 16, 155, 24);
            content.addAndMakeVisible(b);
            buttonAttachments.push_back(
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                    proc.apvts(), info.id, *b));
        }
        else
        {
            auto* s = new juce::Slider();
            s->setSliderStyle(juce::Slider::LinearHorizontal);
            s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
            s->setBounds(x, y + 16, 155, 24);
            content.addAndMakeVisible(s);
            sliderAttachments.push_back(
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    proc.apvts(), info.id, *s));
        }
    }

    ANS1964AudioProcessor& proc;
    juce::Viewport view;
    juce::Component content;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    int contentHeight = 800;
};

// ===========================================================================
// Editor
// ===========================================================================
ANS1964AudioProcessorEditor::ANS1964AudioProcessorEditor(ANS1964AudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setOpaque(true);
    setResizable(true, true);
    setWantsKeyboardFocus(true);

    titleLabel.setText("ANS 1964 - SCORE EDITOR", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe2b866));
    addAndMakeVisible(titleLabel);

    for (auto* b : { &openButton, &saveButton, &importButton, &exportButton, &newButton,
                     &undoButton, &redoButton, &zoomOutButton, &zoomInButton, &fitButton })
        addAndMakeVisible(b);

    openButton.onClick = [this] { openScore(); };
    saveButton.onClick = [this] { saveScore(); };
    importButton.onClick = [this] { importImage(); };
    exportButton.onClick = [this] { exportImage(); };
    newButton.onClick = [this] { newScore(); };
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    zoomOutButton.onClick = [this] { setCanvasZoom(zoom / 1.25f); };
    zoomInButton.onClick = [this] { setCanvasZoom(zoom * 1.25f); };
    fitButton.onClick = [this] { fitCanvas(); };

    statusLabel.setFont(juce::Font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb8a98b));
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(viewport);
    canvas = std::make_unique<ANS1964ScoreCanvas>(proc, *this);
    viewport.setViewedComponent(canvas.get(), false);
    viewport.setScrollBarsShown(true, true);

    editPanel = std::make_unique<ANS1964EditPanel>(proc, *this);
    paramsPanel = std::make_unique<ANS1964ParamsPanel>(proc);
    addAndMakeVisible(tabs);
    tabs.setTabBarDepth(28);
    tabs.addTab("EDIT", juce::Colour(0xff1b1712), editPanel.get(), false);
    tabs.addTab("PARAMS", juce::Colour(0xff1b1712), paramsPanel.get(), false);

    canvas->refreshFromProcessor();
    lastRevision = proc.scoreRevision();
    lastTimerTime = juce::Time::getMillisecondCounterHiRes();
    canvas->setZoom(zoom);
    startTimerHz(30);
    setSize(1280, 820);
}

ANS1964AudioProcessorEditor::~ANS1964AudioProcessorEditor()
{
    stopTimer();
    viewport.setViewedComponent(nullptr, false);
    tabs.clearTabs();
}

void ANS1964AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff17130e));
}

void ANS1964AudioProcessorEditor::resized()
{
    if (canvas == nullptr) return;
    auto b = getLocalBounds();
    auto top = b.removeFromTop(44).reduced(8, 6);
    titleLabel.setBounds(top.removeFromLeft(230));
    const int bh = 26;
    auto put = [&](juce::Button& button, int w)
    {
        auto r = top.removeFromLeft(w);
        button.setBounds(r.withSizeKeepingCentre(w, bh));
        top.removeFromLeft(4);
    };
    put(openButton, 60);
    put(saveButton, 60);
    put(importButton, 90);
    put(exportButton, 90);
    put(newButton, 54);
    put(undoButton, 60);
    put(redoButton, 60);
    put(zoomOutButton, 66);
    put(zoomInButton, 66);
    put(fitButton, 44);

    auto bottom = b.removeFromBottom(24).reduced(8, 3);
    statusLabel.setBounds(bottom);

    auto sidebar = b.removeFromRight(360).reduced(4, 0);
    tabs.setBounds(sidebar);
    viewport.setBounds(b.reduced(4, 0));
    canvas->setZoom(zoom);

    if (!didInitialFit && viewport.getWidth() > 10)
    {
        didInitialFit = true;
        fitCanvas();
    }
}

void ANS1964AudioProcessorEditor::setCanvasZoom(float newZoom)
{
    zoom = juce::jlimit(0.2f, 3.0f, newZoom);
    canvas->setZoom(zoom);
}

void ANS1964AudioProcessorEditor::fitCanvas()
{
    const int w = canvas->getScoreWidth();
    if (w <= 0) return;
    const float availW = std::max(100, viewport.getWidth() - 6);
    const float availH = std::max(100, viewport.getHeight() - 6);
    zoom = juce::jlimit(0.2f, 2.0f,
                        std::min(availW / (float)w, availH / 720.0f));
    canvas->setZoom(zoom);
}

void ANS1964AudioProcessorEditor::refreshCanvasFromProcessor()
{
    canvas->refreshFromProcessor();
    lastRevision = proc.scoreRevision();
    repaint();
}

void ANS1964AudioProcessorEditor::setStatus(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void ANS1964AudioProcessorEditor::notifyLoadError(const juce::String& what)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                            "ANS 1964", what);
}

void ANS1964AudioProcessorEditor::applyChosenFile(const juce::File& file,
                                                   bool save, bool png, bool exportMode)
{
    if (file == juce::File()) return;

    bool ok = false;
    if (exportMode) ok = proc.exportImageFile(file);
    else if (save) ok = proc.saveScoreFile(file);
    else if (png) ok = proc.importImageFile(file);
    else ok = proc.loadScoreFile(file);

    if (!ok)
    {
        notifyLoadError("The file could not be read or written.");
        return;
    }
    if (!save && !exportMode)
    {
        canvas->clearUndoHistory();
        refreshCanvasFromProcessor();
        fitCanvas();
    }
}

void ANS1964AudioProcessorEditor::chooseFile(bool save, bool png, bool exportMode)
{
#if JUCE_LINUX
    juce::String title, filter, mode;
    if (exportMode)
    {
        title = "Export ANS score image";
        filter = "PNG image | *.png";
        mode = " --save --confirm-overwrite";
    }
    else if (save)
    {
        title = "Save ANS score";
        filter = "ANS score | *.ansscore";
        mode = " --save --confirm-overwrite";
    }
    else if (png)
    {
        title = "Import image as ANS score";
        filter = "Images | *.png *.jpg *.jpeg *.bmp *.tif *.tiff";
    }
    else
    {
        title = "Open ANS score";
        filter = "ANS score | *.ansscore";
    }

    juce::Component::SafePointer<ANS1964AudioProcessorEditor> safe(this);
    std::thread([safe, title, filter, mode, save, png, exportMode]() mutable
    {
        juce::String cmd = "/usr/bin/zenity --file-selection --title="
                         + juce::String("'") + title + "'"
                         + mode
                         + " --file-filter='" + filter + "' 2>/dev/null";
        juce::String path;
        if (auto* pipe = popen(cmd.toRawUTF8(), "r"))
        {
            char buf[4096];
            if (std::fgets(buf, sizeof(buf), pipe) != nullptr)
                path = juce::String(buf).trim();
            pclose(pipe);
        }
        if (path.isEmpty()) return;
        const juce::File file(path);
        juce::MessageManager::callAsync([safe, file, save, png, exportMode]()
        {
            if (safe == nullptr) return;
            safe->applyChosenFile(file, save, png, exportMode);
        });
    }).detach();
#else
    juce::String title, wildcard;
    int flags = 0;
    if (exportMode) { title = "Export ANS score image"; wildcard = "*.png";
        flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::warnAboutOverwriting; }
    else if (save) { title = "Save ANS score"; wildcard = "*.ansscore";
        flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::warnAboutOverwriting; }
    else if (png) { title = "Import image as ANS score";
        wildcard = "*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff";
        flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles; }
    else { title = "Open ANS score"; wildcard = "*.ansscore";
        flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles; }

    auto initial = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                       .getChildFile(save || exportMode ? "ans1964.ansscore" : "");
    chooser = std::make_unique<juce::FileChooser>(title, initial, wildcard,
                                                   false, false, this);
    chooser->launchAsync(flags, [this, save, png, exportMode](const juce::FileChooser& fc)
    {
        applyChosenFile(fc.getResult(), save, png, exportMode);
    });
#endif
}

void ANS1964AudioProcessorEditor::openScore() { chooseFile(false, false, false); }
void ANS1964AudioProcessorEditor::saveScore() { chooseFile(true, false, false); }
void ANS1964AudioProcessorEditor::importImage() { chooseFile(false, true, false); }
void ANS1964AudioProcessorEditor::exportImage() { chooseFile(false, false, true); }

void ANS1964AudioProcessorEditor::newScore()
{
    proc.newBlankScore(1024);
    canvas->clearUndoHistory();
    refreshCanvasFromProcessor();
    fitCanvas();
}

void ANS1964AudioProcessorEditor::clearScore()
{
    proc.newBlankScore(canvas->getScoreWidth() > 0 ? canvas->getScoreWidth() : 1024);
    canvas->clearUndoHistory();
    refreshCanvasFromProcessor();
}

void ANS1964AudioProcessorEditor::undo()
{
    canvas->undo();
    lastRevision = proc.scoreRevision();
}

void ANS1964AudioProcessorEditor::redo()
{
    canvas->redo();
    lastRevision = proc.scoreRevision();
}

bool ANS1964AudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z') { undo(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Y') { redo(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'S') { saveScore(); return true; }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'O') { openScore(); return true; }
    return false;
}

void ANS1964AudioProcessorEditor::timerCallback()
{
    if (!canvas->isDragging() && proc.scoreRevision() != lastRevision)
        refreshCanvasFromProcessor();

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float)juce::jlimit(0.0, 0.2, (now - lastTimerTime) / 1000.0);
    lastTimerTime = now;

    const bool run = proc.apvts().getRawParameterValue(ansvst::pid::run)->load() > 0.5f;
    const bool back = proc.apvts().getRawParameterValue(ansvst::pid::backward)->load() > 0.5f;
    const int width = canvas->getScoreWidth();
    if (width > 0)
    {
        const float speed = proc.apvts().getRawParameterValue(ansvst::pid::speed)->load();
        const float scan = proc.apvts().getRawParameterValue(ansvst::pid::scanPos)->load();
        const int loop = (int)std::lround(proc.apvts().getRawParameterValue(ansvst::pid::loop)->load());
        const float px = (float)std::max(1, canvas->getPxPerMm());

        if (!run)
        {
            previewPlayhead = (float)(width - 1) * juce::jlimit(0.0f, 1.0f, scan * 0.01f);
            previewDirection = back ? -1.0f : 1.0f;
            wasRunning = false;
        }
        else
        {
            if (!wasRunning)
            {
                previewPlayhead = (float)(width - 1) * juce::jlimit(0.0f, 1.0f, scan * 0.01f);
                previewDirection = back ? -1.0f : 1.0f;
                wasRunning = true;
                lastBackward = back;
            }
            if (back != lastBackward)
            {
                previewDirection = back ? -1.0f : 1.0f;
                lastBackward = back;
            }

            previewPlayhead += previewDirection * speed * px * dt;
            if (loop == 0)
            {
                previewPlayhead = juce::jlimit(0.0f, (float)(width - 1), previewPlayhead);
            }
            else if (loop == 1)
            {
                while (previewPlayhead >= (float)width) previewPlayhead -= (float)width;
                while (previewPlayhead < 0.0f) previewPlayhead += (float)width;
            }
            else
            {
                if (previewPlayhead >= (float)(width - 1))
                {
                    previewPlayhead = (float)(width - 1);
                    previewDirection = -1.0f;
                }
                else if (previewPlayhead <= 0.0f)
                {
                    previewPlayhead = 0.0f;
                    previewDirection = 1.0f;
                }
            }
        }
        canvas->setPlayhead(previewPlayhead);
        canvas->repaint();
    }
}
