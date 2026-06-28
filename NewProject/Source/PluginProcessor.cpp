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
    // Register with the host so the parameter appears in Ableton's automation lanes.
    addParameter(distanceParam = new juce::AudioParameterFloat(
        "distance", "Distance",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.01f),
        4.0f
    ));

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

    float dist = distanceParam->get();

    // reset() configures the ramp length (50ms here) and sets internal state to
    // the current sampleRate. setCurrentAndTargetValue() primes the smoother at
    // the current distance so there's no unwanted ramp-from-zero on the first block.
    smoothedDelaySamples.reset(sampleRate, 0.05);
    smoothedGain.reset(sampleRate, 0.05);
    float initialDelay = dist / 1125.0f * (float)sampleRate;
    smoothedDelaySamples.setCurrentAndTargetValue(initialDelay);
    smoothedGain.setCurrentAndTargetValue(defaultDist / dist);

    // Prime the natural doppler tracker at the same position so neither mode
    // starts with a ramp-from-zero on the first block.
    naturalDelaySamples    = initialDelay;
    naturalTarget.reset(sampleRate, 0.03);  // 30ms pre-smoother to absorb slider ticks
    naturalTarget.setCurrentAndTargetValue(initialDelay);
    lastSmoothingRampMs    = smoothingRampMs.load();
    lastUseCustomSmoothing = useCustomSmoothing.load();

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
    float dist = distanceParam->get();

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

    float dist            = distanceParam->get();
    bool  customSmoothing = useCustomSmoothing.load();
    float targetDelay     = dist / 1125.0f * (float)currentSampleRate;

    smoothedGain.setTargetValue(defaultDist / dist);

    if (dist != lastDistance)
    {
        updateReverbParams();
        lastDistance = dist;
    }

    // When the user switches modes, sync the about-to-become-active tracker from
    // the one that was just active so there's no delay position jump at the switch.
    if (customSmoothing != lastUseCustomSmoothing)
    {
        if (customSmoothing)
        {
            // Entering custom mode: snap SmoothedValue to where the natural tracker is.
            smoothedDelaySamples.setCurrentAndTargetValue(naturalDelaySamples);
            // Also sync smoothedGain — natural mode derives gain directly from
            // naturalDelaySamples (never calling getNextValue()), so its internal
            // ramp position is stale. Without this snap, switching to custom mode
            // would produce a gain jump on the first block.
            float naturalDist = juce::jmax(naturalDelaySamples / (float)currentSampleRate * 1125.0f, 0.1f);
            smoothedGain.setCurrentAndTargetValue(defaultDist / naturalDist);
        }
        else
        {
            // Entering natural mode: snap both trackers to where SmoothedValue is.
            naturalDelaySamples = smoothedDelaySamples.getCurrentValue();
            naturalTarget.setCurrentAndTargetValue(naturalDelaySamples);
        }

        lastUseCustomSmoothing = customSmoothing;
    }

    if (customSmoothing)
    {
        // Only rebuild the ramp when the slider has actually moved.
        // Calling reset() every block would snap the current value each time.
        float rampMs = smoothingRampMs.load();
        if (rampMs != lastSmoothingRampMs)
        {
            float curr = smoothedDelaySamples.getCurrentValue();
            smoothedDelaySamples.reset(currentSampleRate, rampMs / 1000.0f);
            // Preserve position across the ramp-time change so there's no jump.
            smoothedDelaySamples.setCurrentAndTargetValue(curr);
            lastSmoothingRampMs = rampMs;
        }
        smoothedDelaySamples.setTargetValue(targetDelay);
    }
    else
    {
        naturalTarget.setTargetValue(targetDelay);
    }

    // Collect writable pointers for all channels up front.
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        channelData[ch] = buffer.getWritePointer(ch);

    if (delayEnabled)
    {
        // Max source velocity: 50 ft/s (~34 mph).
        // At 100 ft/s the pitch shift was ~1.6 semitones — clearly audible but too
        // large for music production. 50 ft/s gives ~0.77 semitones: unmistakably
        // Doppler but not overwhelming. Hermite cubic handles 4.4% shift cleanly.
        const float maxStepPerSample = 50.0f / 1125.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentDelaySamples;
            float currentGain;
            if (customSmoothing)
            {
                // Custom mode: SmoothedValue ramps to targetDelay over the user-set time.
                currentDelaySamples = smoothedDelaySamples.getNextValue();
                // Just use default smoothing of 50ms for gain.
                currentGain = smoothedGain.getNextValue();
            }
            else
            {
                // Natural Doppler mode: velocity limiting. naturalTarget pre-smooths
                // the raw slider value (30ms ramp) so small ticks don't each cause a
                // full-speed burst. The velocity limit only saturates for large, fast
                // changes — which is when the Doppler pitch shift is actually wanted.
                float diff = naturalTarget.getNextValue() - naturalDelaySamples;
                naturalDelaySamples += juce::jlimit(-maxStepPerSample, maxStepPerSample, diff);
                currentDelaySamples = naturalDelaySamples;
                // Derive gain from the same velocity-limited position so amplitude and
                // pitch shift stay physically coherent (both tied to actual source distance).
                float currentDist = juce::jmax(naturalDelaySamples / (float)currentSampleRate * 1125.0f, 0.1f);
                currentGain = defaultDist / currentDist;
            }

            // Hermite cubic interpolation across the four samples surrounding the
            // fractional read position. Uses 4 points vs. linear's 2, which lets
            // it handle pitch shifts up to ~10% without audible aliasing — necessary
            // for the 100 ft/s velocity limit (~8.9% shift at that speed).
            int   d0     = (int)currentDelaySamples;
            float t      = currentDelaySamples - d0;  // fractional part [0, 1)
            int   bufLen = delayedBuffer.getNumSamples();

            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                delayedBuffer.setSample(ch, dp, channelData[ch][sample]);

                // Four read positions: one before the target, two after.
                int i0 = (dp - d0     + bufLen) % bufLen;  // d0   samples ago
                int im = (dp - d0 + 1 + bufLen) % bufLen;  // d0-1 samples ago
                int i1 = (dp - d0 - 1 + bufLen) % bufLen;  // d0+1 samples ago
                int i2 = (dp - d0 - 2 + bufLen) % bufLen;  // d0+2 samples ago

                float p0 = delayedBuffer.getSample(ch, im);
                float p1 = delayedBuffer.getSample(ch, i0);
                float p2 = delayedBuffer.getSample(ch, i1);
                float p3 = delayedBuffer.getSample(ch, i2);

                // Catmull-Rom / Hermite coefficients (evaluates p1..p2 at t).
                float c1 = 0.5f * (p2 - p0);
                float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
                float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
                float x  = ((c3 * t + c2) * t + c1) * t + p1;

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
    // Serialize all user-facing parameters to XML so the host (Ableton) can save
    // them with the project and restore them on reload.
    juce::XmlElement xml ("PluginState");
    xml.setAttribute ("distance",                (double) distanceParam->get());
    xml.setAttribute ("reverbEnabled",           (bool)   reverbEnabled.load());
    xml.setAttribute ("delayEnabled",            (bool)   delayEnabled.load());
    xml.setAttribute ("freqAttenuationEnabled",  (bool)   freqAttenuationEnabled.load());
    xml.setAttribute ("useCustomSmoothing",      (bool)   useCustomSmoothing.load());
    xml.setAttribute ("smoothingRampMs",         (double) smoothingRampMs.load());
    copyXmlToBinary (xml, destData);
}

void TrackDistanceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Called by the host when loading a saved project. Restore all parameters
    // and update any derived state (reverb params, lastDistance cache).
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName ("PluginState"))
    {
        float dist = (float) xml->getDoubleAttribute ("distance", defaultDist);
        *distanceParam = dist; // set without notifying the host (restoring saved state, not a user gesture)
        lastDistance   = dist; // keep cache in sync so processBlock doesn't re-fire updateReverbParams needlessly

        reverbEnabled.store          (xml->getBoolAttribute ("reverbEnabled",          false));
        delayEnabled.store           (xml->getBoolAttribute ("delayEnabled",           false));
        freqAttenuationEnabled.store (xml->getBoolAttribute ("freqAttenuationEnabled", false));
        useCustomSmoothing.store     (xml->getBoolAttribute ("useCustomSmoothing",     false));
        smoothingRampMs.store        ((float) xml->getDoubleAttribute ("smoothingRampMs", 50.0));

        updateReverbParams();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrackDistanceAudioProcessor();
}
