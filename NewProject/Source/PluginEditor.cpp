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
    setSize (230, 400);

    distanceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 25);
    distanceSlider.setTextValueSuffix(" ft");
    distanceSlider.setRange(1.0, 100.0);
    distanceSlider.setValue(audioProcessor.defaultDist);
    distanceSlider.addListener(this);
    addAndMakeVisible(distanceSlider);  

    reverbButton.setButtonText("Enable Reverb");
    reverbButton.addListener(this);
    addAndMakeVisible(reverbButton);

    delayButton.setButtonText("Enable Speed of Sound Delay");
    delayButton.addListener(this);
    addAndMakeVisible(delayButton);

    freqAttenuationButton.setButtonText("Enable Frequency Attenuation");
    freqAttenuationButton.addListener(this);
    addAndMakeVisible(freqAttenuationButton);
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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    const int b = 20; // Boundary size
    const int sliderW = 100;
    const int sliderH = getLocalBounds().getHeight() - b*2;
    const int buttonW = 50;
    const int buttonH = 30;

    distanceSlider.setBounds(b, b, sliderW, sliderH);
    reverbButton.setBounds(sliderW + b * 2, b, buttonW, buttonH);
    delayButton.setBounds(sliderW + b * 2, buttonH + b * 2, buttonW, buttonH);
    freqAttenuationButton.setBounds(sliderW + b * 2, buttonH * 2 + b * 3, buttonW, buttonH);
}

void TrackDistanceAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &distanceSlider) //future proofing in case more sliders are added
        audioProcessor.distance = distanceSlider.getValue();
}

void TrackDistanceAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &reverbButton)
        audioProcessor.reverbEnabled = !(audioProcessor.reverbEnabled);
    else if (button == &delayButton)
        audioProcessor.reverbEnabled = !(audioProcessor.delayEnabled);
    else if (button == &freqAttenuationButton)
        audioProcessor.reverbEnabled = !(audioProcessor.freqAttenuationEnabled);
}