/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================
Wind4Unity2AudioProcessor::Wind4Unity2AudioProcessor()
: AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      )
{
   //    Global Parameters
   addParameter(gain = new juce::AudioParameterFloat(
                       "Master Gain", "Master Gain", 0.0f, 1.0f, 0.5f));
   
   //    Distant Rain Parameters
   addParameter(dstBPCutoffFreq = new juce::AudioParameterFloat(
                       "DstCoff", "DistantIntensty", 0.004f, 1000.0f, 10.0f));
   addParameter(dstBPQ = new juce::AudioParameterFloat(
                       "DstQ", "DistantQ", 1.0f, 100.0f, 10.0f));
   addParameter(dstAmplitude = new juce::AudioParameterFloat(
                       "DstAmp", "DistantAmpltd", 0.0001f, 1.5f, 0.75f));
}

Wind4Unity2AudioProcessor::~Wind4Unity2AudioProcessor()
{
}

//==============================================================================


void Wind4Unity2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //    Create DSP Spec
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;
    
    //    Prepare Distant Wind
    dstPrepare(spec);
}

void Wind4Unity2AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void Wind4Unity2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
//    auto totalNumInputChannels  = getTotalNumInputChannels();
//    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear();
    
    dstUpdateSettings();
    dstProcess(buffer);

    buffer.applyGain(gain->get());

}

//==============================================================================
bool Wind4Unity2AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Wind4Unity2AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================

void Wind4Unity2AudioProcessor::dstPrepare(const juce::dsp::ProcessSpec &spec)
{
    //    Prepare Noise Source
    dstNoise1.prepare(spec);
    
    //    Prepare Filter
    dstBPF.prepare(spec);
    dstBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    dstBPF.setCutoffFrequency(dstBPCutoffFreq->get());
    dstBPF.setResonance(dstBPQ->get());
    dstBPF.reset();
}

void Wind4Unity2AudioProcessor::dstProcess(juce::AudioBuffer<float> &buffer)
{
    //    Get Buffer info
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    float dstFrameAmp = dstAmplitude->get();
    
    //    Distant Wind DSP Loop
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float output = dstBPF.processSample(ch, dstNoise1.processSample(0.0f));
            buffer.addSample(ch, s, output * dstFrameAmp);
        }
    }
    
}

void Wind4Unity2AudioProcessor::dstUpdateSettings()
{
    //    Update Filter Settings
    dstBPF.setCutoffFrequency(dstBPCutoffFreq->get());
    dstBPF.setResonance(dstBPQ->get());
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Wind4Unity2AudioProcessor();
}
