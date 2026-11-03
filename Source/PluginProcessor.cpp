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
juce::AudioProcessorValueTreeState::ParameterLayout SlipknotDrummerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("master", "Master", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.85f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("snareBlend", "Snare Hybrid", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("room", "Room", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    return { p.begin(), p.end() };
}
void SlipknotDrummerAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& voice : voices) voice = {};
    roomReverb.reset();
}
bool SlipknotDrummerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}
bool SlipknotDrummerAudioProcessor::loadSample(int slot, const juce::File& file)
{
    if (slot < 0 || slot >= slotCount || !file.existsAsFile()) return false;
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > 30000000) return false;
    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    const int length = static_cast<int>(reader->lengthInSamples);
    const double sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : currentSampleRate;
    auto audio = std::make_shared<juce::AudioBuffer<float>>(channels, length);
    if (!reader->read(audio.get(), 0, length, 0, true, channels > 1)) return false;
    auto& target = slots[static_cast<size_t>(slot)];
    const int index = target.writeIndex.fetch_add(1, std::memory_order_relaxed) % 4;
    std::shared_ptr<const juce::AudioBuffer<float>> immutable = audio;
    std::atomic_store_explicit(&target.takes[static_cast<size_t>(index)], immutable, std::memory_order_release);
    target.sampleRates[static_cast<size_t>(index)].store(sourceRate, std::memory_order_release);
    target.fileNames[index] = file.getFileName();
    target.filePaths[index] = file.getFullPathName();
    return true;
}
juce::String SlipknotDrummerAudioProcessor::getSampleName(int slot) const
{
    if (slot < 0 || slot >= slotCount) return {};
    const auto& target = slots[static_cast<size_t>(slot)];
    const int index = (target.writeIndex.load(std::memory_order_relaxed) - 1 + 4) % 4;
    return target.fileNames[index].isNotEmpty() ? target.fileNames[index] : "Load WAV";
}
bool SlipknotDrummerAudioProcessor::slotHasSample(int slot) const
{
    if (slot < 0 || slot >= slotCount) return false;
    const auto& target = slots[static_cast<size_t>(slot)];
    for (int i = 0; i < 4; ++i)
        if (std::atomic_load_explicit(&target.takes[static_cast<size_t>(i)], std::memory_order_acquire)) return true;
    return false;
}

void SlipknotDrummerAudioProcessor::triggerSlot(int slot, float velocity, int delay, float layerGain)
{
    if (slot < 0 || slot >= slotCount || velocity <= 0.0f || layerGain <= 0.0001f) return;
    auto& target = slots[static_cast<size_t>(slot)];
    const int index = target.readIndex.fetch_add(1, std::memory_order_relaxed) % 4;
    auto sample = std::atomic_load_explicit(&target.takes[static_cast<size_t>(index)], std::memory_order_acquire);
    int chosen = index;
    if (!sample)
        for (int i = 0; i < 4 && !sample; ++i)
        {
            sample = std::atomic_load_explicit(&target.takes[static_cast<size_t>(i)], std::memory_order_acquire);
            if (sample) chosen = i;
        }
    if (!sample || sample->getNumSamples() == 0) return;
    auto& voice = voices[static_cast<size_t>(nextVoice++ % static_cast<int>(voices.size()))];
    voice.sample = std::move(sample);
    voice.position = 0.0;
    const double sourceRate = target.sampleRates[static_cast<size_t>(chosen)].load(std::memory_order_acquire);
    voice.rate = (sourceRate > 0.0 ? sourceRate : currentSampleRate) / currentSampleRate;
    voice.gain = velocity * layerGain;
    voice.gainTarget = voice.gain;
    voice.gainStep = 0.0f;
    voice.slot = slot;
    voice.delay = juce::jmax(0, delay);
    voice.active = true;
}
void SlipknotDrummerAudioProcessor::chokeSlot(int slot, int delay)
{
    const int fadeSamples = juce::jmax(1, static_cast<int>(currentSampleRate * 0.004));
    for (auto& voice : voices)
        if (voice.active && voice.slot == slot)
        {
            voice.delay = juce::jmax(0, delay);
            voice.gainTarget = 0.0f;
            voice.gainStep = -voice.gain / static_cast<float>(fadeSamples);
        }
}

void SlipknotDrummerAudioProcessor::triggerNote(int note, float velocity, int delay)
{
    if (note == 42) chokeSlot(8, delay); // Closed hat chokes the open hat.
    const float blend = parameters.getRawParameterValue("snareBlend")->load();
    if (note == 38)
    {
        const bool bright = slotHasSample(1), tight = slotHasSample(2);
        if (bright && tight)
        {
            triggerSlot(1, velocity, delay, std::sqrt(1.0f - blend));
            triggerSlot(2, velocity, delay, std::sqrt(blend));
        }
        else if (bright) triggerSlot(1, velocity, delay);
        else if (tight) triggerSlot(2, velocity, delay);
        return;
    }
    if (note == 40) { triggerSlot(2, velocity, delay); return; }
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
    const double pitchRate = std::pow(2.0, static_cast<double>(pitch) / 12.0);
    const int channels = buffer.getNumChannels();
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        float left = 0.0f, right = 0.0f;
        for (auto& voice : voices)
        {
            if (!voice.active || !voice.sample) continue;
            if (voice.delay > 0) { --voice.delay; continue; }
            const int length = voice.sample->getNumSamples();
            const int index = static_cast<int>(voice.position);
            if (index >= length) { voice.active = false; continue; }
            if (voice.gainStep != 0.0f)
            {
                voice.gain += voice.gainStep;
                if ((voice.gainStep < 0.0f && voice.gain <= voice.gainTarget) ||
                    (voice.gainStep > 0.0f && voice.gain >= voice.gainTarget))
                {
                    voice.gain = voice.gainTarget;
                    voice.gainStep = 0.0f;
                    if (voice.gainTarget <= 0.0f) { voice.active = false; continue; }
                }
            }
            const int next = juce::jmin(index + 1, length - 1);
            const float fraction = static_cast<float>(voice.position - index);
            const float l0 = voice.sample->getSample(0, index);
            const float l1 = voice.sample->getSample(0, next);
            left += (l0 + (l1 - l0) * fraction) * voice.gain;
            if (voice.sample->getNumChannels() > 1)
            {
                const float r0 = voice.sample->getSample(1, index);
                const float r1 = voice.sample->getSample(1, next);
                right += (r0 + (r1 - r0) * fraction) * voice.gain;
            }
            else right += (l0 + (l1 - l0) * fraction) * voice.gain;
            voice.position += voice.rate * pitchRate;
        }
        if (channels > 0) buffer.addSample(0, frame, left * master);
        if (channels > 1) buffer.addSample(1, frame, right * master);
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
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f;
    rp.damping = 0.42f;
    rp.wetLevel = parameters.getRawParameterValue("room")->load() * 0.28f;
    rp.dryLevel = 1.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    roomReverb.setParameters(rp);
    if (buffer.getNumChannels() > 1)
        roomReverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        roomReverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
}
juce::AudioProcessorEditor* SlipknotDrummerAudioProcessor::createEditor()
{
    return new SlipknotDrummerAudioProcessorEditor(*this);
}
void SlipknotDrummerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    for (int slot = 0; slot < slotCount; ++slot)
        for (int take = 0; take < 4; ++take)
            state.setProperty("samplePath_" + juce::String(slot) + "_" + juce::String(take), slots[static_cast<size_t>(slot)].filePaths[take], nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void SlipknotDrummerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    parameters.replaceState(state);
    for (int slot = 0; slot < slotCount; ++slot)
        for (int take = 0; take < 4; ++take)
        {
            const auto path = state.getProperty("samplePath_" + juce::String(slot) + "_" + juce::String(take)).toString();
            const juce::File file(path);
            if (file.existsAsFile()) loadSample(slot, file);
        }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlipknotDrummerAudioProcessor();
}