#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SlipknotDrummerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SlipknotDrummerAudioProcessorEditor(SlipknotDrummerAudioProcessor&);
    ~SlipknotDrummerAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    SlipknotDrummerAudioProcessor& processor;
    juce::Label title, subtitle;
    std::array<juce::TextButton, SlipknotDrummerAudioProcessor::slotCount> sampleButtons;
    juce::Slider masterSlider, snareSlider, roomSlider, pitchSlider;
    juce::Label masterLabel, snareLabel, roomLabel, pitchLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment, snareAttachment, roomAttachment, pitchAttachment;
    juce::TextButton folderButton { "SAMPLE GUIDE" };
    juce::FileChooser fileChooser { "Load a free / CC0 drum sample" };
    void chooseSample(int slot);
    void setupSlider(juce::Slider&, juce::Label&, const juce::String&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlipknotDrummerAudioProcessorEditor)
};