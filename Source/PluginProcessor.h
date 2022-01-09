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
*//*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class Wind4Unity2AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Wind4Unity2AudioProcessor();
    ~Wind4Unity2AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override { return false; }
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override {}
    void setStateInformation (const void* data, int sizeInBytes) override {}

private:
    //  Wind Methods
    void dstPrepare(const juce::dsp::ProcessSpec& spec);
    void dstProcess(juce::AudioBuffer<float>& buffer);
    void dstUpdateSettings();
    
    //  Global Parameters
    juce::AudioParameterFloat* gain;
        
    //  Distant Wind Parameters
    juce::AudioParameterFloat* dstBPCutoffFreq;
    juce::AudioParameterFloat* dstBPQ;
    juce::AudioParameterFloat* dstAmplitude;
    
    //  Distant Wind DSP Resources
    juce::dsp::Oscillator<float> dstNoise1
    {[] (float x)
        { juce::Random r; return r.nextFloat() * 2.0f - 1.0f; }
    };
    
    juce::dsp::StateVariableTPTFilter<float> dstBPF;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Wind4Unity2AudioProcessor)
};
