/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TrackDistanceAudioProcessorEditor::TrackDistanceAudioProcessorEditor (TrackDistanceAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (230, 460);

    // Distance Slider
    distanceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 25);
    distanceSlider.setTextValueSuffix(" ft");
    distanceSlider.setRange(1.0, 100.0);
    // Read the current processor value so reopening the editor doesn't reset the slider.
    distanceSlider.setValue(audioProcessor.distance.load(), juce::dontSendNotification);
    distanceSlider.addListener(this);
    addAndMakeVisible(distanceSlider);  

    // Feature Buttons
    // setClickingTogglesState makes the button stay visually toggled after release,
    // matching the appearance of the doppler mode buttons.
    // No setRadioGroupId here — all three can be active simultaneously (checkbox behaviour).
    // setToggleState restores the correct visual state when the editor is reopened.
    reverbButton.setButtonText("Enable Reverb");
    reverbButton.setClickingTogglesState(true);
    reverbButton.setToggleState(audioProcessor.isReverbEnabled(), juce::dontSendNotification);
    reverbButton.addListener(this);
    addAndMakeVisible(reverbButton);

    delayButton.setButtonText("Enable Speed of Sound Delay");
    delayButton.setClickingTogglesState(true);
    delayButton.setToggleState(audioProcessor.isDelayEnabled(), juce::dontSendNotification);
    delayButton.addListener(this);
    addAndMakeVisible(delayButton);

    freqAttenuationButton.setButtonText("Enable Frequency Attenuation");
    freqAttenuationButton.setClickingTogglesState(true);
    freqAttenuationButton.setToggleState(audioProcessor.isFreqAttenuationEnabled(), juce::dontSendNotification);
    freqAttenuationButton.addListener(this);
    addAndMakeVisible(freqAttenuationButton);

    // Doppler mode toggle — radio group so only one can be active at a time.
    // setClickingTogglesState lets TextButtons behave as toggle buttons.
    // setRadioGroupId ties them together so selecting one deselects the other.
    naturalDopplerButton.setButtonText("Natural Doppler");
    naturalDopplerButton.setClickingTogglesState(true);
    naturalDopplerButton.setRadioGroupId(1);
    // Restore toggle state from processor so reopening the panel preserves the selection.
    bool customMode = audioProcessor.isUsingCustomSmoothing();
    naturalDopplerButton.setToggleState(!customMode, juce::dontSendNotification);
    naturalDopplerButton.addListener(this);
    addAndMakeVisible(naturalDopplerButton);

    customSmoothingButton.setButtonText("Custom Smoothing");
    customSmoothingButton.setClickingTogglesState(true);
    customSmoothingButton.setRadioGroupId(1);
    customSmoothingButton.setToggleState(customMode, juce::dontSendNotification);
    customSmoothingButton.addListener(this);
    addAndMakeVisible(customSmoothingButton);

    // Smoothing ramp time slider — restore persisted value from processor.
    smoothingSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    smoothingSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 70, 20);
    smoothingSlider.setTextValueSuffix(" ms");
    smoothingSlider.setRange(10.0, 500.0);
    smoothingSlider.setValue(audioProcessor.smoothingRampMs.load(), juce::dontSendNotification);
    smoothingSlider.addListener(this);
    addAndMakeVisible(smoothingSlider);

    // Set initial enabled state based on current processor state (handles preset load).
    updateDopplerControls();
}

TrackDistanceAudioProcessorEditor::~TrackDistanceAudioProcessorEditor()
{
}

//==============================================================================
void TrackDistanceAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    //g.setColour (juce::Colours::white);
    //g.setFont (juce::FontOptions (15.0f));
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void TrackDistanceAudioProcessorEditor::resized()
{
    const int b       = 20;  // margin used throughout
    const int sliderW = 100;
    const int buttonW = 70;  // fills the right panel (window width - sliderW - 3*b = 70)
    const int buttonH = 30;
    const int rightX  = sliderW + b * 2; // x origin of the right panel

    // Distance slider spans the full usable height on the left.
    distanceSlider.setBounds(b, b, sliderW, getLocalBounds().getHeight() - b * 2);

    // Feature enable buttons — evenly spaced with margin b between each.
    reverbButton.setBounds         (rightX, b,                         buttonW, buttonH);
    delayButton.setBounds          (rightX, buttonH + b * 2,           buttonW, buttonH);
    freqAttenuationButton.setBounds(rightX, buttonH * 2 + b * 3,       buttonW, buttonH);

    // Doppler mode toggle pair — grouped tightly (2px gap) to signal they're related,
    // separated from the feature buttons above by the standard margin b.
    int toggleY = buttonH * 3 + b * 4;
    naturalDopplerButton.setBounds (rightX, toggleY,              buttonW, buttonH);
    customSmoothingButton.setBounds(rightX, toggleY + buttonH + 2, buttonW, buttonH);

    // Smoothing slider fills the remaining right-panel height below the toggle group.
    int smoothingY = toggleY + buttonH * 2 + b + 2;
    smoothingSlider.setBounds(rightX, smoothingY, buttonW,
                              getLocalBounds().getHeight() - b - smoothingY);
}

void TrackDistanceAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &distanceSlider)
        audioProcessor.distance.store((float)distanceSlider.getValue());
    else if (slider == &smoothingSlider)
        audioProcessor.smoothingRampMs.store((float)smoothingSlider.getValue());
}

void TrackDistanceAudioProcessorEditor::buttonStateChanged(juce::Button* button)
{}

void TrackDistanceAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &reverbButton)
        audioProcessor.toggleReverb();
    else if (button == &delayButton)
    {
        audioProcessor.toggleDelay();
        updateDopplerControls(); // grey out / restore the doppler mode group
    }
    else if (button == &freqAttenuationButton)
        audioProcessor.toggleFreq();
    else if (button == &naturalDopplerButton || button == &customSmoothingButton)
    {
        // Write the chosen mode to the processor so it survives editor close/reopen.
        audioProcessor.setUseCustomSmoothing(customSmoothingButton.getToggleState());
        updateDopplerControls();
    }
}

void TrackDistanceAudioProcessorEditor::updateDopplerControls()
{
    bool delayOn = audioProcessor.isDelayEnabled();

    // Grey out both toggle buttons when delay is off — the doppler mode
    // choice is meaningless without the delay feature active.
    naturalDopplerButton.setEnabled(delayOn);
    customSmoothingButton.setEnabled(delayOn);

    // Smoothing slider is only interactive when delay is on AND custom mode is chosen.
    bool customMode = delayOn && customSmoothingButton.getToggleState();
    smoothingSlider.setEnabled(customMode);
}