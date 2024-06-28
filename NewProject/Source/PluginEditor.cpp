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
    setSize (200, 400);

    distanceSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    distanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 25);
    distanceSlider.setTextValueSuffix("ft");
    distanceSlider.setRange(1.0, 64.0);
    distanceSlider.setValue(audioProcessor.defaultDist);
    distanceSlider.addListener(this);
    addAndMakeVisible(distanceSlider);  
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
    distanceSlider.setBounds(getLocalBounds());
}

void TrackDistanceAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &distanceSlider) //future proofing in case more sliders are added
        audioProcessor.distance = distanceSlider.getValue();
}