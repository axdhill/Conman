#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class ConvolutionPluginProcessor : public juce::AudioProcessor
{
public:
    ConvolutionPluginProcessor();
    ~ConvolutionPluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadImpulseResponse(const juce::File& file);
    juce::String getIRFileName() const { return irFilePath.isNotEmpty() ? juce::File(irFilePath).getFileName() : juce::String(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> dryBuffer;
    juce::String irFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConvolutionPluginProcessor)
};
