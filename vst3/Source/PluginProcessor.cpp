#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "ANSFile.h"
#include "ANSPaint.h"
#include "BinaryData.h"

#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <cmath>
#include <cstring>

ANS1964AudioProcessor::ANS1964AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "ANS1964",
             ansvst::createLayout())
{
    // The editor host may create the editor without ever calling prepareToPlay;
    // give it the bundled starting score immediately.
    loadDemoIfNeeded();
}

ANS1964AudioProcessor::~ANS1964AudioProcessor()
{
    if (auto* h = componentHandler_.exchange(nullptr)) h->release();
}

bool ANS1964AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.getMainInputChannelSet().isDisabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() ||
           out == juce::AudioChannelSet::stereo();
}

void ANS1964AudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine_.setUserScorePath({});
    engine_.prepare(sampleRate);
    loadDemoIfNeeded();
}

void ANS1964AudioProcessor::releaseResources()
{
}

void ANS1964AudioProcessor::loadDemoIfNeeded()
{
    if (stateScoreLoaded_ || initialScoreSet_) return;
    initialScoreSet_ = true;

    std::vector<uint8_t> data;
    int w = 0, px = 10;
    if (ansvst::parseScoreBytes(BinaryData::user_ansscore,
                                (size_t)BinaryData::user_ansscoreSize,
                                data, w, px))
    {
        std::lock_guard<std::mutex> lock(scoreMutex_);
        engine_.setUserScore(std::move(data), w, px);
        scoreRevision_.fetch_add(1);
    }
}

float ANS1964AudioProcessor::plain(const char* id) const
{
    if (auto* p = apvts_.getParameter(id))
        return p->convertFrom0to1(p->getValue());
    return 0.0f;
}

ans64::Settings ANS1964AudioProcessor::makeSettings() const
{
    ans64::Settings s;

    s.scoreIndex = (int)std::lround(plain(ansvst::pid::score));
    s.run = plain(ansvst::pid::run) > 0.5f;
    s.speed = plain(ansvst::pid::speed);
    s.reverse = plain(ansvst::pid::backward) > 0.5f;
    s.loop = (int)std::lround(plain(ansvst::pid::loop));
    s.scanPos01 = plain(ansvst::pid::scanPos) * 0.01f;
    s.scoreGain = mpc::dsp::dbToGain(plain(ansvst::pid::scoreGain));
    s.volumeGain = mpc::dsp::dbToGain(plain(ansvst::pid::volume));

    for (int c = 0; c < ans64::kChannels; ++c)
        s.chLevel[c] = juce::jlimit(0.0f, 1.0f,
                                    plain(ansvst::pid::channel(c + 1).toRawUTF8()) * 0.01f);

    s.coderGain = mpc::dsp::dbToGain(plain(ansvst::pid::coderLevel));
    s.harmonics = (int)std::lround(plain(ansvst::pid::coderHarm));
    s.formant = (int)std::lround(plain(ansvst::pid::coderFormant));
    s.formantShift = plain(ansvst::pid::coderShift);
    s.coderDecayMs = plain(ansvst::pid::coderDecay);
    s.coderNote = (int)std::lround(plain(ansvst::pid::coderNote));
    s.testGate = plain(ansvst::pid::coderGate) > 0.5f;

    s.wowDepth = plain(ansvst::pid::wowDepth) * 0.01f;
    s.wowRate = plain(ansvst::pid::wowRate);
    s.trackNoise = plain(ansvst::pid::trackNoise) * 0.01f;
    s.blur = plain(ansvst::pid::blur) * 0.01f;
    s.hfLoss = plain(ansvst::pid::hfLoss) * 0.01f;
    s.hum = plain(ansvst::pid::hum) * 0.01f;

    s.dynMode = (int)std::lround(plain(ansvst::pid::dynMode));
    s.dynAmount = plain(ansvst::pid::dynAmount) * 0.01f;
    s.dynRate = plain(ansvst::pid::dynRate);
    s.drive = plain(ansvst::pid::drive) * 0.01f;
    s.lineMode = (int)std::lround(plain(ansvst::pid::lineMode));
    s.width = plain(ansvst::pid::width) * 0.01f;

    s.editX01 = plain(ansvst::pid::editX) * 0.01f;
    s.editY01 = plain(ansvst::pid::editY) * 0.01f;
    s.editLevel = plain(ansvst::pid::editLevel) * 0.01f;
    s.editWidth = (int)std::lround(plain(ansvst::pid::editWidth));
    s.editHarm = (int)std::lround(plain(ansvst::pid::editHarm));
    s.editDraw = plain(ansvst::pid::editDraw) > 0.5f;
    s.editErase = plain(ansvst::pid::editErase) > 0.5f;
    s.editClear = plain(ansvst::pid::editClear) > 0.5f;
    s.editAudition = plain(ansvst::pid::editAudition) > 0.5f;
    s.editFlipY = plain(ansvst::pid::editFlipY) > 0.5f;

    return s;
}

void ANS1964AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0 || buffer.getNumChannels() <= 0) return;

    std::unique_lock<std::mutex> lock(scoreMutex_, std::try_to_lock);
    if (!lock.owns_lock()) return;

    engine_.setSettings(makeSettings());

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    auto render = [&](int start, int len)
    {
        if (len > 0)
            engine_.render(outL + start, outR != nullptr ? outR + start : nullptr, len);
    };

    int pos = 0;
    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        const int off = juce::jlimit(0, n, (int)meta.samplePosition);
        if (off > pos)
        {
            render(pos, off - pos);
            pos = off;
        }
        if (msg.isNoteOn())
            engine_.noteOn(msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine_.noteOff(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine_.allNotesOff();
    }
    if (pos < n) render(pos, n - pos);
}

juce::AudioProcessorEditor* ANS1964AudioProcessor::createEditor()
{
    return new ANS1964AudioProcessorEditor(*this);
}

ANS1964AudioProcessor::ScoreView ANS1964AudioProcessor::getScoreView() const
{
    ScoreView v;
    std::lock_guard<std::mutex> lock(scoreMutex_);
    v.width = engine_.userScoreWidth();
    v.pxPerMm = engine_.userScorePxPerMm();
    const auto* data = engine_.userScoreData();
    if (data != nullptr && v.width > 0)
        v.data.assign(data, data + (size_t)v.width * 720u);
    return v;
}

void ANS1964AudioProcessor::paintScoreAt(int col, float track, int level,
                                         float brushTracks, bool erase, int harmonics)
{
    std::lock_guard<std::mutex> lock(scoreMutex_);
    ansvst::paintScoreAt(engine_.userScoreData(), engine_.userScoreWidth(),
                         col, track, level, brushTracks, erase, harmonics);
}

void ANS1964AudioProcessor::replaceScore(std::vector<uint8_t> data, int width,
                                         int px, bool notify)
{
    if (width <= 0 || width > 8192 || data.size() != (size_t)width * 720u)
        return;

    {
        std::lock_guard<std::mutex> lock(scoreMutex_);
        engine_.setUserScore(std::move(data), width, px);
    }
    scoreRevision_.fetch_add(1);

    if (notify)
    {
        updateHostDisplay();
        if (auto* h = componentHandler_.load())
            h->restartComponent(Steinberg::Vst::RestartFlags::kParamValuesChanged);
    }
}

void ANS1964AudioProcessor::newBlankScore(int width)
{
    width = juce::jlimit(64, 8192, width);
    std::vector<uint8_t> data((size_t)width * 720u, 0);
    replaceScore(std::move(data), width, 10, true);
}

void ANS1964AudioProcessor::markScoreEdited()
{
    scoreRevision_.fetch_add(1);
    updateHostDisplay();
    if (auto* h = componentHandler_.load())
            h->restartComponent(Steinberg::Vst::RestartFlags::kParamValuesChanged);
}

bool ANS1964AudioProcessor::loadScoreFile(const juce::File& file)
{
    std::vector<uint8_t> data;
    int w = 0, px = 10;
    if (!ansvst::loadScoreFile(file, data, w, px)) return false;
    replaceScore(std::move(data), w, px, true);
    return true;
}

bool ANS1964AudioProcessor::saveScoreFile(const juce::File& file) const
{
    const auto view = getScoreView();
    if (view.data.empty() || view.width <= 0) return false;
    return ansvst::saveScoreFile(file, view.data.data(), view.width, view.pxPerMm);
}

juce::Image ANS1964AudioProcessor::scoreToImage(const uint8_t* data, int width)
{
    juce::Image img(juce::Image::RGB, width, 720, false);
    if (data == nullptr || width <= 0) return {};
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 720; ++y)
    {
        const uint8_t* row = data + (size_t)y * (size_t)width;
        for (int x = 0; x < width; ++x)
        {
            const uint8_t v = row[x];
            bd.setPixelColour(x, y, juce::Colour(v, v, v));
        }
    }
    return img;
}

bool ANS1964AudioProcessor::imageToScore(const juce::Image& image,
                                         std::vector<uint8_t>& out, int& width)
{
    if (!image.isValid()) return false;
    width = image.getWidth();
    const int h = image.getHeight();
    if (width <= 0 || width > 8192 || h != 720) return false;
    out.resize((size_t)width * 720u);

    if (image.getFormat() == juce::Image::SingleChannel)
    {
        juce::Image::BitmapData bd(image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < 720; ++y)
            std::memcpy(out.data() + (size_t)y * (size_t)width,
                        bd.getLinePointer(y), (size_t)width);
    }
    else
    {
        for (int y = 0; y < 720; ++y)
            for (int x = 0; x < width; ++x)
                out[(size_t)y * (size_t)width + (size_t)x] =
                    (uint8_t)juce::jlimit(0, 255, (int)std::lround(
                        image.getPixelAt(x, y).getBrightness() * 255.0f));
    }
    return true;
}

bool ANS1964AudioProcessor::importImageFile(const juce::File& file)
{
    auto img = juce::ImageFileFormat::loadFrom(file);
    if (!img.isValid()) return false;

    int width = (int)std::lround((double)img.getWidth() * 720.0 / (double)img.getHeight());
    width = juce::jlimit(64, 8192, width);
    if (img.getWidth() != width || img.getHeight() != 720)
        img = img.rescaled(width, 720, juce::Graphics::highResamplingQuality);

    std::vector<uint8_t> data;
    int w = 0;
    if (!imageToScore(img, data, w)) return false;
    replaceScore(std::move(data), w, 10, true);
    return true;
}

bool ANS1964AudioProcessor::exportImageFile(const juce::File& file) const
{
    const auto view = getScoreView();
    if (view.data.empty() || view.width <= 0) return false;
    auto img = scoreToImage(view.data.data(), view.width);
    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;
    juce::PNGImageFormat png;
    return png.writeImageToStream(img, out);
}

void ANS1964AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto view = getScoreView();
    const auto tree = apvts_.copyState();
    juce::String xml;
    if (auto x = tree.createXml()) xml = x->toString();

    juce::MemoryOutputStream png;
    if (view.width > 0 && !view.data.empty())
    {
        auto img = scoreToImage(view.data.data(), view.width);
        juce::PNGImageFormat fmt;
        fmt.writeImageToStream(img, png);
    }

    juce::MemoryOutputStream out;
    out.writeInt(0x414E5332); // "ANS2"
    out.writeInt(2);
    out.writeInt(view.width);
    out.writeInt(view.pxPerMm);
    out.writeInt((int)xml.length());
    if (xml.isNotEmpty()) out.write(xml.toRawUTF8(), (size_t)xml.length());
    out.writeInt((int)png.getDataSize());
    if (png.getDataSize() > 0) out.write(png.getData(), png.getDataSize());

    destData.replaceAll(out.getData(), out.getDataSize());
}

void ANS1964AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 24) return;
    juce::MemoryInputStream in(data, (size_t)sizeInBytes, false);
    if (in.readInt() != 0x414E5332) return;
    if (in.readInt() != 2) return;
    const int width = in.readInt();
    const int px = in.readInt();
    const int xmlLen = in.readInt();
    if (xmlLen < 0 || xmlLen > sizeInBytes) return;

    if (xmlLen > 0)
    {
        juce::MemoryBlock xmlBlock((size_t)xmlLen);
        if (in.read(xmlBlock.getData(), (size_t)xmlLen) != xmlLen) return;
        juce::String xml((const char*)xmlBlock.getData(), (size_t)xmlLen);
        auto tree = juce::ValueTree::fromXml(xml);
        if (tree.isValid()) apvts_.replaceState(tree);
    }

    if (in.getNumBytesRemaining() < 4) return;
    const int pngLen = in.readInt();
    if (pngLen < 0 || pngLen > in.getNumBytesRemaining()) return;

    if (pngLen > 0 && width > 0 && width <= 8192)
    {
        juce::MemoryBlock pngBlock((size_t)pngLen);
        if (in.read(pngBlock.getData(), (size_t)pngLen) != pngLen) return;
        juce::MemoryInputStream pngIn(pngBlock.getData(), pngBlock.getSize(), false);
        auto img = juce::ImageFileFormat::loadFrom(pngIn);
        std::vector<uint8_t> score;
        int w = 0;
        if (imageToScore(img, score, w))
        {
            stateScoreLoaded_ = true;
            replaceScore(std::move(score), w, px > 0 ? px : 10, false);
            scoreRevision_.fetch_add(1);
        }
    }
    stateScoreLoaded_ = true;
}

void ANS1964AudioProcessor::setIComponentHandler(Steinberg::FUnknown* handler)
{
    if (auto* old = componentHandler_.exchange(nullptr)) old->release();
    if (handler == nullptr) return;

    Steinberg::Vst::IComponentHandler* h = nullptr;
    if (handler->queryInterface(Steinberg::Vst::IComponentHandler::iid,
                                (void**)&h) == Steinberg::kResultOk)
        componentHandler_.store(h);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ANS1964AudioProcessor();
}
