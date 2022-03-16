/*
  ==============================================================================

    Copyright (c) 2022 - Gordon Webb
 
    This file is part of Wind4Unity2.

    Wind4Unity2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Wind4Unity2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wind4Unity2.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/
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
//   addParameter(dstBPCutoffFreq = new juce::AudioParameterFloat(
//                       "DstCoff", "DistantIntensty", 0.004f, 1000.0f, 10.0f));
//   addParameter(dstBPQ = new juce::AudioParameterFloat(
//                       "DstQ", "DistantQ", 1.0f, 100.0f, 10.0f));
   addParameter(dstAmplitude = new juce::AudioParameterFloat(
                       "DstAmp", "DistantAmpltd", 0.0001f, 1.5f, 0.75f));
    
//  Wind Speed Parameters
    addParameter(windForce = new juce::AudioParameterInt("Wind Force", "Wind Force", 0, 12, 3));
    addParameter(gustActive = new juce::AudioParameterBool("Gust Active", "Gust Active", false));
    addParameter(gustDepth = new juce::AudioParameterFloat("Gust Depth", "Gust Depth", 0.0f, 1.0f, 0.5f));
    addParameter(gustInterval = new juce::AudioParameterFloat("Gust Interval", "Gust Interval", 0.0f, 1.0f, 0.5f));
    addParameter(squallActive = new juce::AudioParameterBool("Squall Active", "Squall Active", false));
    addParameter(squallDepth = new juce::AudioParameterFloat("Squall Depth", "Squall Depth", 0.0f, 1.0f, 0.5f));
    
    //  Seed RNG
    int seed = int(std::chrono::system_clock::now().time_since_epoch().count());
    generator.seed(seed);
        
    //  Set Initial Wind Speed
    windSpeedSet();
    
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
    currentSpec = spec;
    
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
    
    if (++currentWSComponentCounter > targetWSComponentCount)
        windSpeedSet();
     else
        currentWindSpeed += deltaWindSpeed;
    if (gustActive->get() || gustStatus == Closing)
    {
        computeGust();
    } else if (gustStatus == Active)
    {
        gustClose();
    }
    
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
//    dstNoise1.prepare(spec);
    
    //    Prepare Filter
    dstBPF.prepare(spec);
    dstBPF.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
//    dstBPF.setCutoffFrequency(dstBPCutoffFreq->get());
//    dstBPF.setResonance(dstBPQ->get());
    dstBPF.setCutoffFrequency(10.0f);
    dstBPF.setResonance(1.0f);
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
            float output = r.nextFloat() * 2.0f - 1.0f;
            output = dstBPF.processSample(ch, output);
            buffer.addSample(ch, s, output * dstFrameAmp);
        }
    }
    
}

void Wind4Unity2AudioProcessor::dstUpdateSettings()
{
    //    Update Filter Settings
//    dstBPF.setCutoffFrequency(dstBPCutoffFreq->get());
//    dstBPF.setResonance(dstBPQ->get());
    if (currentWindSpeed < 0)
        currentWindSpeed = 0;
    
    dstBPF.setCutoffFrequency(juce::jlimit(0.004f, 1500.0f, (currentWindSpeed + currentGust) * 30.0f));
    dstBPF.setResonance(1.0f + log(juce::jmax(1.0f, (currentWindSpeed + currentGust) * 0.1f)));
}

float Wind4Unity2AudioProcessor::randomNormal()
{
    return distribution(generator);
}

void Wind4Unity2AudioProcessor::windSpeedSet()
{
    int force = windForce->get();
    if (force == 0)
    {
        targetWindSpeed = 0.0f;
        targetWSComponentCount = (int) (currentSpec.sampleRate / currentSpec.maximumBlockSize);
    }
    else
    {
        targetWindSpeed = meanWS[force] + sdWS[force] * randomNormal();
        targetWSComponentCount = (int) ((10.0f + 2.0f * randomNormal()) / (force / 2.0f) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
    }
    
    currentWSComponentCounter = 0;
    deltaWindSpeed = (targetWindSpeed - currentWindSpeed) / (float) targetWSComponentCount;
}

void Wind4Unity2AudioProcessor::computeGust()
{
    // If first time set interval
    if (!gustWasActive)
    {
        gustIntervalSet();
        return;
    }
    // If Waiting, if end of Interval set gust/squall component and length
    if (gustStatus == Waiting)
    {
        if (++currentGustIntervalCounter > targetGustIntervalCount)
        {
            if (squallActive->get())
            {
                squallSet();
                squallLengthSet();
            }
            else
            {
                gustSet();
                gustLengthSet();
            }
        }
        return;
    }
    // If Active, Close if at end of length, set new gust/squall component if at
    //    end of component, else increment gust speed
    if (gustStatus == Active)
    {
        if (++currentGustLengthCounter > targetGustLengthCount)
        {
            gustClose();
            return;
        }
        
        if (++currentGustComponentCounter > targetGustComponentCount)
        {
            if (squallActive->get())
            {
                squallSet();
            }
            else
            {
                gustSet();
            }
        }
        else
        {
            currentGust += deltaGust;
        }
        return;
    }
    // If Closing and end of component set gust speed zero, set wait length and return
    //    else increment gust speed
    if (gustStatus == Closing)
    {
        if (++currentGustComponentCounter > targetGustComponentCount)
        {
            gustWasActive = false;
            gustStatus = Off;
            currentGust = 0.0f;
            return;
        }
        else
        {
            currentGust += deltaGust;
        }
    }

}

void Wind4Unity2AudioProcessor::gustSet()
{
    int force = windForce->get();
    if (force < 3)
    {
        gustClose();
        return;
    }
    else
    {
        targetGust = 15.0f * gustDepth->get() + 2.0f * sdWS[force] * randomNormal();
        targetGustComponentCount = (int) ((1.0f + 0.25f * randomNormal()) / (force / 2.0f) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
        juce::jmax(targetGustComponentCount, 1);
    }
    
    currentGustComponentCounter = 0;
    deltaGust = (targetGust - currentGust) / (float) targetGustComponentCount;
    gustStatus = Active;
}

void Wind4Unity2AudioProcessor::squallSet()
{
    int force = windForce->get();
    if (force < 3)
    {
        gustClose();
        return;
    }
    else
    {
        targetGust = 20.0f * squallDepth->get() + 2.0f * sdWS[force] * randomNormal();
        targetGustComponentCount = (int) (((0.75f + 0.25f * randomNormal()) / (force / 2.0f)) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
        juce::jmax(targetGustComponentCount, 1);
    }
    
    currentGustComponentCounter = 0;
    deltaGust = (targetGust - currentGust) / (float) targetGustComponentCount;
    gustStatus = Active;
}

void Wind4Unity2AudioProcessor::gustClose()
{
    targetGust = 0.0f;
    targetGustComponentCount = (int) (2 * currentSpec.sampleRate / currentSpec.maximumBlockSize);
    currentGustComponentCounter = 0;
    deltaGust = (targetGust - currentGust) / (float) targetGustComponentCount;
    gustStatus = Closing;
}

void Wind4Unity2AudioProcessor::gustIntervalSet()
{
    currentGustIntervalCounter = 0;
    targetGustIntervalCount = (int) ((5.0f + gustInterval->get() * 100.0f + 10.0f * randomNormal()) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
    juce::jmax(targetGustIntervalCount, 1);
    gustStatus = Waiting;
    gustWasActive = true;
}

void Wind4Unity2AudioProcessor::gustLengthSet()
{
    currentGustLengthCounter = 0;
    targetGustLengthCount = (int) ((5.0f + 1.5f * randomNormal()) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
    juce::jmax(targetGustLengthCount, 1);
    gustStatus = Active;
}

void Wind4Unity2AudioProcessor::squallLengthSet()
{
    currentGustLengthCounter = 0;
    targetGustLengthCount = (int) (((30.0f + 5.0f * randomNormal())) * currentSpec.sampleRate / currentSpec.maximumBlockSize);
    juce::jmax(targetGustLengthCount, 1);
    gustStatus = Active;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Wind4Unity2AudioProcessor();
}
