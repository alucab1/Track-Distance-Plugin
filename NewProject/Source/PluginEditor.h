/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <iostream>

//==============================================================================
/**
*/
class TrackDistanceAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Slider::Listener, public juce::Button::Listener
{
public:
    TrackDistanceAudioProcessorEditor (TrackDistanceAudioProcessor&);
    ~TrackDistanceAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;
    void buttonStateChanged(juce::Button* button) override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    TrackDistanceAudioProcessor& audioProcessor;

    juce::Slider distanceSlider;

    juce::TextButton reverbButton;
    juce::TextButton freqAttenuationButton;
    juce::TextButton delayButton;
    juce::TextButton zeroButton;

    // Radio group: only one of these two can be toggled at a time.
    // Both are disabled when the speed-of-sound delay feature is off.
    juce::TextButton naturalDopplerButton;
    juce::TextButton customSmoothingButton;

    // Only interactable when customSmoothingButton is selected and delay is on.
    juce::Slider smoothingSlider;

    // Enables/disables the doppler mode controls based on delay toggle state.
    void updateDopplerControls();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDistanceAudioProcessorEditor)
};
