#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

class SlipknotDrummerAudioProcessor final : public juce::AudioProcessor
{
public:
    SlipknotDrummerAudioProcessor();
    ~SlipknotDrummerAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Slipknot Drummer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static constexpr int slotCount = 14;
    static const std::array<const char*, slotCount> slotNames;
    static const std::array<int, slotCount> slotNotes;
    bool loadSample(int slot, const juce::File& file);
    juce::String getSampleName(int slot) const;
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
private:
    struct SampleSlot
    {
        std::array<std::shared_ptr<const juce::AudioBuffer<float>>, 4> takes {};
        std::atomic<int> writeIndex { 0 };
        std::atomic<int> readIndex { 0 };
        juce::String fileNames[4];
    };
    struct Voice
    {
        std::shared_ptr<const juce::AudioBuffer<float>> sample;
        double position = 0.0;
        double rate = 1.0;
        float gain = 0.0f;
        int delay = 0;
        bool active = false;
    };
    void triggerSlot(int slot, float velocity, int delay, float layerGain = 1.0f);
    void triggerNote(int note, float velocity, int delay);
    void renderVoices(juce::AudioBuffer<float>& buffer);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::array<SampleSlot, slotCount> slots;
    std::array<Voice, 96> voices;
    int nextVoice = 0;
    double currentSampleRate = 44100.0;
    juce::AudioFormatManager formatManager;
    juce::Reverb roomReverb;
    juce::AudioProcessorValueTreeState parameters;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlipknotDrummerAudioProcessor)
};