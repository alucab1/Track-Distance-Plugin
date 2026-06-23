/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TrackDistanceAudioProcessor::TrackDistanceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    delayedBuffer.setSize(2, 192000, true, false, true);
    delayedBuffer.applyGain(0);
}

TrackDistanceAudioProcessor::~TrackDistanceAudioProcessor()
{
}

//==============================================================================
const juce::String TrackDistanceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TrackDistanceAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TrackDistanceAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TrackDistanceAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TrackDistanceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TrackDistanceAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TrackDistanceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TrackDistanceAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TrackDistanceAudioProcessor::getProgramName (int index)
{
    return {};
}

void TrackDistanceAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TrackDistanceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    reverb.setSampleRate(sampleRate);

    float dist = distance.load();

    // reset() configures the ramp length (50ms here) and sets internal state to
    // the current sampleRate. setCurrentAndTargetValue() primes the smoother at
    // the current distance so there's no unwanted ramp-from-zero on the first block.
    smoothedDelaySamples.reset(sampleRate, 0.05);
    smoothedGain.reset(sampleRate, 0.05);
    smoothedDelaySamples.setCurrentAndTargetValue(dist / 1125.0f * (float)sampleRate);
    smoothedGain.setCurrentAndTargetValue(defaultDist / dist);

    // Allocate once here so processBlock never touches the heap.
    channelData.resize(getTotalNumInputChannels());

    updateReverbParams();
}

void TrackDistanceAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TrackDistanceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TrackDistanceAudioProcessor::updateReverbParams()
{
    float dist = distance.load(); // atomic load — safe to call from either thread

    //params.roomSize = size;   // can be set by user
    params.damping = 0.92f;
    params.wetLevel = 0.015f * dist;  // relative to distance
    params.dryLevel = 0.7f;
    params.width = 0.006f * dist;    // relative to distance

    reverb.setParameters(params);
}

void TrackDistanceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    int  numSamples            = buffer.getNumSamples();

    // Atomically snapshot the distance value the UI thread may have just updated.
    float dist = distance.load();

    // Tell each smoother where it should ramp to over the next ~50ms.
    // The corrected formula is distance / speedOfSound * sampleRate — NOT * numSamples,
    // which would make the delay time vary with buffer size.
    smoothedDelaySamples.setTargetValue(dist / 1125.0f * (float)currentSampleRate);
    smoothedGain.setTargetValue(defaultDist / dist);

    // Recompute reverb parameters only when distance has actually changed,
    // not unconditionally on every block.
    if (dist != lastDistance)
    {
        updateReverbParams();
        lastDistance = dist;
    }

    // Collect writable pointers for all channels up front.
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        channelData[ch] = buffer.getWritePointer(ch);

    if (delayEnabled)
    {
        // Samples are the OUTER loop and channels are the INNER loop so that dp
        // (the circular buffer write position) advances exactly once per sample.
        // Previously channels were outer, which made dp advance numChannels*numSamples
        // per block — a stereo bug that offset channel 1's data in the ring buffer.
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // getNextValue() steps each smoother by one sample toward its target,
            // so the delay time and gain change gradually rather than jumping.
            float currentDelaySamples = smoothedDelaySamples.getNextValue();
            float currentGain         = smoothedGain.getNextValue();

            // Linear interpolation between the two adjacent delay-buffer samples
            // that straddle the fractional read position. Without this, the read
            // pointer would snap to integer positions, causing subtle pitch artifacts
            // as the delay time slowly changes.
            int   d0     = (int)currentDelaySamples;
            float frac   = currentDelaySamples - d0;
            int   bufLen = delayedBuffer.getNumSamples();

            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                // Write the incoming sample into the circular ring buffer.
                delayedBuffer.setSample(ch, dp, channelData[ch][sample]);

                // Read back from d0 samples ago, wrapping around the ring boundary.
                int op0 = (dp - d0 + bufLen) % bufLen;
                int op1 = (op0 - 1 + bufLen) % bufLen; // one sample further back for interpolation

                float x = delayedBuffer.getSample(ch, op0) * (1.0f - frac)
                        + delayedBuffer.getSample(ch, op1) * frac;

                // Apply smoothed gain (amplitude falls off with distance).
                channelData[ch][sample] = x * currentGain;
            }

            // Advance the shared write position once — after all channels are written.
            if (++dp >= bufLen)
                dp = 0;
        }
    }
    else
    {
        // No delay: just apply the smoothed gain attenuation per sample.
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentGain = smoothedGain.getNextValue();
            for (int ch = 0; ch < totalNumInputChannels; ++ch)
                channelData[ch][sample] *= currentGain;
        }
    }

    // Apply reverb after delay/gain so the wet signal reflects attenuated audio.
    if (reverbEnabled)
    {
        if (totalNumInputChannels == 1)
            reverb.processMono(channelData[0], numSamples);
        else
            reverb.processStereo(channelData[0], channelData[1], numSamples);
    }
}

//==============================================================================
bool TrackDistanceAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TrackDistanceAudioProcessor::createEditor()
{
    return new TrackDistanceAudioProcessorEditor (*this);
}

//==============================================================================
void TrackDistanceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TrackDistanceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrackDistanceAudioProcessor();
}
