#include "PluginProcessor.h"
#include "PluginEditor.h"

ConvolutionPluginProcessor::ConvolutionPluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

ConvolutionPluginProcessor::~ConvolutionPluginProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout ConvolutionPluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drywet", 1}, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void ConvolutionPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    convolution.prepare(spec);
    dryBuffer.setSize(static_cast<int>(spec.numChannels), samplesPerBlock);
}

void ConvolutionPluginProcessor::releaseResources() {}

bool ConvolutionPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ConvolutionPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    float dryWet = apvts.getRawParameterValue("drywet")->load();
    float gainDb = apvts.getRawParameterValue("gain")->load();
    float gainLinear = juce::Decibels::decibelsToGain(gainDb);

    // Keep a copy of the dry signal (pre-allocated buffer, no heap allocation)
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    jassert(dryBuffer.getNumChannels() >= numChannels && dryBuffer.getNumSamples() >= numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Process wet signal through convolution
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);

    // Mix dry and wet
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            wet[i] = (dry[i] * (1.0f - dryWet) + wet[i] * dryWet) * gainLinear;
    }
}

void ConvolutionPluginProcessor::loadImpulseResponse(const juce::File& file)
{
    if (file.existsAsFile())
    {
        irFilePath = file.getFullPathName();
        convolution.loadImpulseResponse(file,
                                        juce::dsp::Convolution::Stereo::yes,
                                        juce::dsp::Convolution::Trim::yes,
                                        0);
    }
}

void ConvolutionPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("irFilePath", irFilePath, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ConvolutionPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        juce::String path = apvts.state.getProperty("irFilePath", "");
        if (path.isNotEmpty())
            loadImpulseResponse(juce::File(path));
    }
}

juce::AudioProcessorEditor* ConvolutionPluginProcessor::createEditor()
{
    return new ConvolutionPluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConvolutionPluginProcessor();
}
