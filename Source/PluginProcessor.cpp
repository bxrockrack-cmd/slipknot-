#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<const char*, SlipknotDrummerAudioProcessor::slotCount>
SlipknotDrummerAudioProcessor::slotNames {
    "Kick", "Snare Bright", "Snare Tight", "Floor Tom", "Low Tom",
    "Mid Tom", "High Tom", "Closed Hat", "Open Hat", "Ride",
    "Crash 1", "Crash 2", "China", "Splash"
};

const std::array<int, SlipknotDrummerAudioProcessor::slotCount>
SlipknotDrummerAudioProcessor::slotNotes {
    36, 38, 38, 43, 45, 47, 50, 42, 46, 51, 49, 57, 52, 55
};

SlipknotDrummerAudioProcessor::SlipknotDrummerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout
SlipknotDrummerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "master", "Master", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.85f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "snareBlend", "Snare Hybrid", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "room", "Room", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pitch", "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    return { p.begin(), p.end() };
}

void SlipknotDrummerAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& voice : voices)
        voice = {};
}

bool SlipknotDrummerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

bool SlipknotDrummerAudioProcessor::loadSample(int slot, const juce::File& file)
{
    if (slot < 0 || slot >= slotCount || !file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > 30000000)
        return false;

    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    const int length = static_cast<int>(reader->lengthInSamples);
    auto audio = std::make_shared<juce::AudioBuffer<float>>(channels, length);
    reader->read(audio.get(), 0, length, 0, true, channels > 1);

    auto& target = slots[static_cast<size_t>(slot)];
    const int index = target.writeIndex.fetch_add(1, std::memory_order_relaxed) % 4;
    std::shared_ptr<const juce::AudioBuffer<float>> immutable = audio;
    std::atomic_store_explicit(&target.takes[static_cast<size_t>(index)], immutable, std::memory_order_release);
    target.fileNames[index] = file.getFileName();
    return true;
}

juce::String SlipknotDrummerAudioProcessor::getSampleName(int slot) const
{
    if (slot < 0 || slot >= slotCount)
        return {};
    const auto& target = slots[static_cast<size_t>(slot)];
    const int index = (target.writeIndex.load(std::memory_order_relaxed) - 1 + 4) % 4;
    return target.fileNames[index].isNotEmpty() ? target.fileNames[index] : "Load WAV";
}

void SlipknotDrummerAudioProcessor::triggerSlot(int slot, float velocity, int delay, float layerGain)
{
    if (slot < 0 || slot >= slotCount || velocity <= 0.0f || layerGain <= 0.0001f)
        return;

    auto& target = slots[static_cast<size_t>(slot)];
    const int index = target.readIndex.fetch_add(1, std::memory_order_relaxed) % 4;
    auto sample = std::atomic_load_explicit(&target.takes[static_cast<size_t>(index)], std::memory_order_acquire);
    if (!sample)
    {
        for (int i = 0; i < 4 && !sample; ++i)
            sample = std::atomic_load_explicit(&target.takes[static_cast<size_t>(i)], std::memory_order_acquire);
    }
    if (!sample || sample->getNumSamples() == 0)
        return;

    auto& voice = voices[static_cast<size_t>(nextVoice++ % static_cast<int>(voices.size()))];
    voice.sample = std::move(sample);
    voice.position = 0.0;
    voice.rate = 1.0;
    voice.gain = velocity * layerGain;
    voice.delay = juce::jmax(0, delay);
    voice.active = true;
}

void SlipknotDrummerAudioProcessor::triggerNote(int note, float velocity, int delay)
{
    const float blend = parameters.getRawParameterValue("snareBlend")->load();
    if (note == 38)
    {
        triggerSlot(1, velocity, delay, std::sqrt(1.0f - blend));
        triggerSlot(2, velocity, delay, std::sqrt(blend));
        return;
    }
    if (note == 40)
    {
        triggerSlot(2, velocity, delay);
        return;
    }

    for (int i = 0; i < slotCount; ++i)
        if (slotNotes[static_cast<size_t>(i)] == note && i != 2)
        {
            triggerSlot(i, velocity, delay);
            break;
        }
}

void SlipknotDrummerAudioProcessor::renderVoices(juce::AudioBuffer<float>& buffer)
{
    const float master = parameters.getRawParameterValue("master")->load();
    const float pitch = parameters.getRawParameterValue("pitch")->load();
    const float pitchRate = std::pow(2.0, static_cast<double>(pitch) / 12.0);
    const float room = parameters.getRawParameterValue("room")->load();
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    for (int frame = 0; frame < samples; ++frame)
    {
        float left = 0.0f, right = 0.0f;
        for (auto& voice : voices)
        {
            if (!voice.active || !voice.sample)
                continue;
            if (voice.delay > 0)
            {
                --voice.delay;
                continue;
            }

            const int length = voice.sample->getNumSamples();
            const int index = static_cast<int>(voice.position);
            if (index >= length)
            {
                voice.active = false;
                voice.sample.reset();
                continue;
            }

            const int next = juce::jmin(index + 1, length - 1);
            const float fraction = static_cast<float>(voice.position - index);
            const float l0 = voice.sample->getSample(0, index);
            const float l1 = voice.sample->getSample(0, next);
            const float l = (l0 + (l1 - l0) * fraction) * voice.gain;
            float r = l;
            if (voice.sample->getNumChannels() > 1)
            {
                const float r0 = voice.sample->getSample(1, index);
                const float r1 = voice.sample->getSample(1, next);
                r = (r0 + (r1 - r0) * fraction) * voice.gain;
            }
            left += l;
            right += r;
            voice.position += voice.rate * pitchRate;
        }
        const float roomMix = 1.0f + room * 0.12f;
        if (channels > 0) buffer.addSample(0, frame, left * master * roomMix);
        if (channels > 1) buffer.addSample(1, frame, right * master * roomMix);
    }
}

void SlipknotDrummerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            triggerNote(message.getNoteNumber(), message.getFloatVelocity(),
                        juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition));
    }
    renderVoices(buffer);
}

juce::AudioProcessorEditor* SlipknotDrummerAudioProcessor::createEditor()
{
    return new SlipknotDrummerAudioProcessorEditor(*this);
}

void SlipknotDrummerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    for (int i = 0; i < slotCount; ++i)
        state.setProperty("sample" + juce::String(i), getSampleName(i), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SlipknotDrummerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlipknotDrummerAudioProcessor();
}
