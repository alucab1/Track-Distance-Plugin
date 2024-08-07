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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    reverb.setSampleRate(sampleRate);
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
    //params.roomSize = size;   // can be set by user
    params.damping = 0.92f;
    params.wetLevel = 0.015f * distance;    // relative to distance
    params.dryLevel = 0.7f;
    params.width = 0.006f * distance;     //relative to distance

    reverb.setParameters(params);
}

void TrackDistanceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    int numSamples = buffer.getNumSamples();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    updateReverbParams();

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    std::vector <float*> channelData(2);
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        //float* channelData = buffer.getWritePointer (channel);
        channelData[channel] = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; sample++)
        {
            channelData[channel][sample] = buffer.getSample(channel, sample) * (defaultDist/distance);  // amplitude inversely proportional to distance
        }
        if (totalNumInputChannels == 1 && reverbEnabled)
            reverb.processMono(channelData[channel], numSamples);
    }
    if (totalNumInputChannels == 2 && reverbEnabled)
        reverb.processStereo(channelData[0], channelData[1], numSamples);
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
