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
    void toggleDelay()  { delayEnabled = !delayEnabled; }
    void toggleFreq()   { freqAttenuationEnabled = !freqAttenuationEnabled; }

    // Read-only access for the editor — the bools stay private so only the
    // toggle methods can write them, keeping write control on the processor side.
    bool isDelayEnabled()           const { return delayEnabled.load(); }
    bool isReverbEnabled()          const { return reverbEnabled.load(); }
    bool isFreqAttenuationEnabled() const { return freqAttenuationEnabled.load(); }

    // Doppler mode: private with getter + setter because the editor sets it to a
    // specific value (not a flip), and nothing outside the processor should write it freely.
    bool isUsingCustomSmoothing() const { return useCustomSmoothing.load(); }
    void setUseCustomSmoothing(bool customMode) { useCustomSmoothing.store(customMode); }

    // Registered with the host via addParameter() so Ableton/other DAWs can
    // automate it. The host reads/writes the normalised value [0,1] internally;
    // distanceParam->get() returns the unnormalised ft value for audio math.
    juce::AudioParameterFloat* distanceParam = nullptr;
    const float defaultDist = 4.0f; // reference distance: plugin is calibrated assuming input starts this far away

    // Public like distance — set directly by the smoothing slider on the UI thread.
    std::atomic<float> smoothingRampMs { 50.0f };

private:
    juce::AudioBuffer<float> delayedBuffer;
    int dp = 0; // write position in the circular delay buffer

    // Pre-allocated in prepareToPlay so processBlock never calls malloc on the audio thread.
    std::vector<float*> channelData;

    // Stored from prepareToPlay so processBlock can compute delay in samples correctly.
    // Formula: delaySamples = distance / speedOfSound * sampleRate (NOT * numSamples).
    double currentSampleRate = 44100.0;

    // SmoothedValue ramps each parameter gradually over 50ms instead of jumping
    // instantly when the slider moves, which would cause a waveform discontinuity
    juce::SmoothedValue<float> smoothedDelaySamples;
    juce::SmoothedValue<float> smoothedGain;

    // Cached so we only call updateReverbParams() when distance actually changes,
    // not unconditionally on every processBlock call.
    float lastDistance = 4.0f;

    // --- Natural Doppler mode state ---
    // Tracks the current delay position under velocity limiting, independently of
    // SmoothedValue so the two modes can be switched without a position jump.
    float naturalDelaySamples = 0.0f;

    // Pre-smooths the raw targetDelay before the velocity limiter sees it.
    // Without this, every discrete slider tick causes a full-speed velocity burst
    // (max pitch shift for a few samples then silence), which sounds like a trill.
    // With it, small ticks are absorbed smoothly; only large sudden changes saturate
    // the velocity limit and produce the intended Doppler pitch shift.
    juce::SmoothedValue<float> naturalTarget;

    // --- Custom smoothing mode state ---
    // Cached so reset() is only called when the slider actually moves, not every block.
    // Calling reset() every block would continuously snap the ramp to the current value.
    float lastSmoothingRampMs = 50.0f;

    // Cached to detect mode switches so the inactive tracker can be synced from the
    // active one, preventing a delay position jump at the moment of switching.
    bool lastUseCustomSmoothing = false;

    juce::Reverb reverb;
    juce::Reverb::Parameters params;

    // Frequency attenuation: one biquad low-pass filter per channel.
    // Cutoff falls exponentially with distance, inspired by the frequency-dependent
    // absorption curves in ISO 9613-1. At 1-100 ft the physical effect is <0.1 dB,
    // so the curve is artistically scaled to be audible: 20kHz at 4ft, ~2kHz at 100ft.
    juce::IIRFilter freqFilter[2];
    float lastFreqCutoff = 20000.0f;

    // Atomic so the compiler cannot cache these in a register and serve stale
    // values to the audio thread after the UI thread has toggled them.
    std::atomic<bool> reverbEnabled { false };
    std::atomic<bool> freqAttenuationEnabled { false };
    std::atomic<bool> delayEnabled { false }; // delay based on speed of sound in normal air
    std::atomic<bool> useCustomSmoothing { false }; // false = natural doppler, true = custom ramp time

    void updateReverbParams();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDistanceAudioProcessor)
};
