/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
/**
*/
class TrackDistanceAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    TrackDistanceAudioProcessor();
    ~TrackDistanceAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool reverbEnabled = false;
    bool freqAttenuationEnabled = false; // Look into ISO 9613-2 for more info on frequency response
    bool delayEnabled = false; //delay based on speed of sound in normal air

    double distance = 4.0;
    const double defaultDist = 4.0;   //for reference, how far away input audio is considered to be
private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;

    void updateReverbParams();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDistanceAudioProcessor)
};
