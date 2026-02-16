#pragma once

#include "PluginProcessor.h"
#include "OfflineConvolver.h"

class ConvolutionPluginEditor : public juce::AudioProcessorEditor,
                                 public juce::FileDragAndDropTarget,
                                 private juce::Timer
{
public:
    explicit ConvolutionPluginEditor(ConvolutionPluginProcessor&);
    ~ConvolutionPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

private:
    void timerCallback() override;

    bool isDraggingOver = false;

    ConvolutionPluginProcessor& processorRef;

    // Real-time mode controls
    juce::TextButton loadIRButton { "Load IR" };
    juce::Label irFileLabel;
    juce::Slider dryWetSlider;
    juce::Slider gainSlider;
    juce::Label dryWetLabel { {}, "Dry/Wet" };
    juce::Label gainLabel { {}, "Gain (dB)" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    // Offline mode controls
    juce::TextButton loadSampleAButton { "Load Sample A" };
    juce::TextButton loadSampleBButton { "Load Sample B" };
    juce::TextButton convolveButton { "Convolve & Export" };
    juce::Label sampleALabel;
    juce::Label sampleBLabel;
    juce::Label offlineStatusLabel;

    juce::File sampleAFile, sampleBFile;
    OfflineConvolver offlineConvolver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConvolutionPluginEditor)
};
