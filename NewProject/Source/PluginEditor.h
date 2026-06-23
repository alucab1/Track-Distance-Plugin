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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDistanceAudioProcessorEditor)
};
