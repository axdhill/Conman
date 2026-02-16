#include "PluginEditor.h"

ConvolutionPluginEditor::ConvolutionPluginEditor(ConvolutionPluginProcessor& p)
    : AudioProcessorEditor(p), processorRef(p)
{
    setOpaque(true);

    // --- Real-time mode ---

    addAndMakeVisible(loadIRButton);
    loadIRButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Impulse Response", juce::File{}, "*.wav;*.aif;*.aiff;*.flac", false);

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    processorRef.loadImpulseResponse(file);
                    irFileLabel.setText(file.getFileName(), juce::dontSendNotification);
                }
            });
    };

    addAndMakeVisible(irFileLabel);
    irFileLabel.setJustificationType(juce::Justification::centredLeft);
    auto irName = processorRef.getIRFileName();
    irFileLabel.setText(irName.isNotEmpty() ? irName : "No IR loaded", juce::dontSendNotification);

    addAndMakeVisible(dryWetSlider);
    dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    dryWetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "drywet", dryWetSlider);

    addAndMakeVisible(dryWetLabel);

    addAndMakeVisible(gainSlider);
    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "gain", gainSlider);

    addAndMakeVisible(gainLabel);

    // --- Offline mode ---

    addAndMakeVisible(loadSampleAButton);
    loadSampleAButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Sample A", juce::File{}, "*.wav;*.aif;*.aiff;*.flac", false);

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    sampleAFile = file;
                    sampleALabel.setText(file.getFileName(), juce::dontSendNotification);
                }
            });
    };

    addAndMakeVisible(sampleALabel);
    sampleALabel.setJustificationType(juce::Justification::centredLeft);
    sampleALabel.setText("No file loaded", juce::dontSendNotification);

    addAndMakeVisible(loadSampleBButton);
    loadSampleBButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Sample B", juce::File{}, "*.wav;*.aif;*.aiff;*.flac", false);

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    sampleBFile = file;
                    sampleBLabel.setText(file.getFileName(), juce::dontSendNotification);
                }
            });
    };

    addAndMakeVisible(sampleBLabel);
    sampleBLabel.setJustificationType(juce::Justification::centredLeft);
    sampleBLabel.setText("No file loaded", juce::dontSendNotification);

    addAndMakeVisible(convolveButton);
    convolveButton.onClick = [this]
    {
        if (!sampleAFile.existsAsFile() || !sampleBFile.existsAsFile())
        {
            offlineStatusLabel.setText("Please load both samples first.", juce::dontSendNotification);
            return;
        }

        if (offlineConvolver.isThreadRunning())
        {
            offlineStatusLabel.setText("Already processing...", juce::dontSendNotification);
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Convolved Output", juce::File{}, "*.wav", false);

        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File{})
                {
                    auto outputFile = file.withFileExtension("wav");
                    offlineConvolver.setFiles(sampleAFile, sampleBFile, outputFile);
                    offlineConvolver.startThread();
                    startTimerHz(10);
                }
            });
    };

    addAndMakeVisible(offlineStatusLabel);
    offlineStatusLabel.setJustificationType(juce::Justification::centredLeft);
    offlineStatusLabel.setText("Idle", juce::dontSendNotification);

    setSize(500, 420);
}

ConvolutionPluginEditor::~ConvolutionPluginEditor()
{
    stopTimer();
    dryWetAttachment.reset();
    gainAttachment.reset();
}

void ConvolutionPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    if (isDraggingOver)
    {
        g.setColour(juce::Colour(0x30ffffff));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colours::white);
        g.drawRect(getLocalBounds(), 2);
        g.setFont(20.0f);
        g.drawText("Drop audio file to load as IR", getLocalBounds().removeFromTop(200),
                    juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0.0f, 200.0f, static_cast<float>(getWidth()), 200.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Real-time Convolution", 20, 10, 300, 25, juce::Justification::centredLeft);
    g.drawText("Offline Convolution", 20, 210, 300, 25, juce::Justification::centredLeft);
}

void ConvolutionPluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    // Real-time section
    auto rtArea = area.removeFromTop(170);
    rtArea.removeFromTop(30); // title space

    auto irRow = rtArea.removeFromTop(30);
    loadIRButton.setBounds(irRow.removeFromLeft(100));
    irRow.removeFromLeft(10);
    irFileLabel.setBounds(irRow);

    rtArea.removeFromTop(10);

    auto dwRow = rtArea.removeFromTop(30);
    dryWetLabel.setBounds(dwRow.removeFromLeft(80));
    dryWetSlider.setBounds(dwRow);

    rtArea.removeFromTop(10);

    auto gainRow = rtArea.removeFromTop(30);
    gainLabel.setBounds(gainRow.removeFromLeft(80));
    gainSlider.setBounds(gainRow);

    // Offline section
    area.removeFromTop(20); // separator space
    auto olArea = area;
    olArea.removeFromTop(30); // title space

    auto saRow = olArea.removeFromTop(30);
    loadSampleAButton.setBounds(saRow.removeFromLeft(130));
    saRow.removeFromLeft(10);
    sampleALabel.setBounds(saRow);

    olArea.removeFromTop(10);

    auto sbRow = olArea.removeFromTop(30);
    loadSampleBButton.setBounds(sbRow.removeFromLeft(130));
    sbRow.removeFromLeft(10);
    sampleBLabel.setBounds(sbRow);

    olArea.removeFromTop(10);

    auto convRow = olArea.removeFromTop(30);
    convolveButton.setBounds(convRow.removeFromLeft(150));

    olArea.removeFromTop(10);
    offlineStatusLabel.setBounds(olArea.removeFromTop(25));
}

bool ConvolutionPluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& path : files)
    {
        auto ext = juce::File(path).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac")
            return true;
    }
    return false;
}

void ConvolutionPluginEditor::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag(files))
    {
        isDraggingOver = true;
        repaint();
    }
}

void ConvolutionPluginEditor::fileDragExit(const juce::StringArray&)
{
    isDraggingOver = false;
    repaint();
}

void ConvolutionPluginEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingOver = false;
    repaint();

    for (auto& path : files)
    {
        juce::File file(path);
        auto ext = file.getFileExtension().toLowerCase();
        if (file.existsAsFile() && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"))
        {
            processorRef.loadImpulseResponse(file);
            irFileLabel.setText(file.getFileName(), juce::dontSendNotification);
            break;
        }
    }
}

void ConvolutionPluginEditor::timerCallback()
{
    auto status = offlineConvolver.getStatus();
    offlineStatusLabel.setText(offlineConvolver.getStatusMessage(), juce::dontSendNotification);

    if (status == OfflineConvolver::Status::Done || status == OfflineConvolver::Status::Error)
        stopTimer();
}
