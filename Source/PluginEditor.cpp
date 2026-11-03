#include "PluginEditor.h"

SlipknotDrummerAudioProcessorEditor::SlipknotDrummerAudioProcessorEditor(SlipknotDrummerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 650);
    title.setText("SLIPKNOT DRUMMER", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffff4b35));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);
    subtitle.setText("NU-METAL DRUM SAMPLER  /  FREE SAMPLE EDITION", juce::dontSendNotification);
    subtitle.setFont(juce::Font(juce::FontOptions(11.0f)));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffb5b5bd));
    addAndMakeVisible(subtitle);
    for (int i = 0; i < SlipknotDrummerAudioProcessor::slotCount; ++i)
    {
        auto& button = sampleButtons[static_cast<size_t>(i)];
        button.setButtonText(juce::String(SlipknotDrummerAudioProcessor::slotNames[static_cast<size_t>(i)]) + "\nLOAD WAV");
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff25252d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffeeeeee));
        if (loadedName != "Load WAV") button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b2927));
        button.onClick = [this, i] { chooseSample(i); };
        addAndMakeVisible(button);
    }
    setupSlider(masterSlider, masterLabel, "MASTER");
    setupSlider(snareSlider, snareLabel, "SNARE HYBRID");
    setupSlider(roomSlider, roomLabel, "ROOM");
    setupSlider(pitchSlider, pitchLabel, "PITCH");
    auto& state = processor.getParameters();
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "master", masterSlider);
    snareAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "snareBlend", snareSlider);
    roomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "room", roomSlider);
    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "pitch", pitchSlider);
    folderButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff76251f));
    folderButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    folderButton.onClick = [] { juce::URL("https://github.com/bxrockrack-cmd/slipknot-/blob/main/SAMPLES.md").launchInDefaultBrowser(); };
    addAndMakeVisible(folderButton);
}
void SlipknotDrummerAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name)
{
    label.setText(name, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd7d7dd));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff4b35));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff44444e));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffb09f));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff202027));
    addAndMakeVisible(slider);
}
void SlipknotDrummerAudioProcessorEditor::chooseSample(int slot)
{
    fileChooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, slot](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile() && processor.loadSample(slot, file))
            {
                auto& button = sampleButtons[static_cast<size_t>(slot)];
                button.setButtonText(juce::String(SlipknotDrummerAudioProcessor::slotNames[static_cast<size_t>(slot)]) + "\n" + processor.getSampleName(slot));
                button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b2927));
            }
        });
}
void SlipknotDrummerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111116));
    g.setColour(juce::Colour(0xff292932));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 12.0f);
    g.setColour(juce::Colour(0xffff4b35));
    g.fillRect(18, 18, 4, 48);
    g.setColour(juce::Colour(0xff3b3b45));
    g.drawHorizontalLine(82, 20.0f, 740.0f);
}
void SlipknotDrummerAudioProcessorEditor::resized()
{
    title.setBounds(34, 20, 480, 34);
    subtitle.setBounds(36, 53, 520, 20);
    folderButton.setBounds(580, 28, 145, 34);
    const int left = 28, top = 105, gap = 9, w = 170, h = 76;
    for (int i = 0; i < SlipknotDrummerAudioProcessor::slotCount; ++i)
        sampleButtons[static_cast<size_t>(i)].setBounds(left + (i % 4) * (w + gap), top + (i / 4) * (h + gap), w, h);
    const int knobTop = 475, knobWidth = 120, startX = 50;
    juce::Slider* sliders[] = { &masterSlider, &snareSlider, &roomSlider, &pitchSlider };
    juce::Label* labels[] = { &masterLabel, &snareLabel, &roomLabel, &pitchLabel };
    for (int i = 0; i < 4; ++i)
    {
        const int x = startX + i * 175;
        labels[i]->setBounds(x, knobTop, knobWidth, 20);
        sliders[i]->setBounds(x, knobTop + 18, knobWidth, 112);
    }
}