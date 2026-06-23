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

    void toggleReverb() { reverbEnabled = !reverbEnabled; }
    void toggleDelay() { delayEnabled = !delayEnabled; }
    void toggleFreq() { freqAttenuationEnabled = !freqAttenuationEnabled; }

    // std::atomic<float> ensures the UI thread can safely write (via the slider)
    // while the audio thread reads in processBlock, with no data race.
    // A plain double/float shared across threads is undefined behavior in C++.
    std::atomic<float> distance { 4.0f };
    const float defaultDist = 4.0f; // reference distance: plugin is calibrated assuming input starts this far away

private:
    juce::AudioBuffer<float> delayedBuffer;
    int dp = 0; // write position in the circular delay buffer

    // Pre-allocated in prepareToPlay so processBlock never calls malloc on the audio thread.
    std::vector<float*> channelData;

    // Stored from prepareToPlay so processBlock can compute delay in samples correctly.
    // Formula: delaySamples = distance / speedOfSound * sampleRate (NOT * numSamples).
    double currentSampleRate = 44100.0;

    // SmoothedValue ramps each parameter gradually over 50ms instead of jumping
    // instantly when the slider moves, which would cause a waveform discontinuity (click).
    juce::SmoothedValue<float> smoothedDelaySamples;
    juce::SmoothedValue<float> smoothedGain;

    // Cached so we only call updateReverbParams() when distance actually changes,
    // not unconditionally on every processBlock call.
    float lastDistance = 4.0f;

    juce::Reverb reverb;
    juce::Reverb::Parameters params;

    bool reverbEnabled = false;
    bool freqAttenuationEnabled = false; // Look into ISO 9613-2 for more info on frequency response
    bool delayEnabled = false; // delay based on speed of sound in normal air

    void updateReverbParams();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDistanceAudioProcessor)
};
