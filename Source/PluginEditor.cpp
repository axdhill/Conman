#include "PluginEditor.h"

ConvolutionPluginEditor::ConvolutionPluginEditor(ConvolutionPluginProcessor& p)
    : AudioProcessorEditor(p), processorRef(p)
{
    setOpaque(true);

    if (! isStandalone())
    {
        // --- Plugin mode: real-time convolution ---

        addAndMakeVisible(loadImprintButton);
        loadImprintButton.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select Imprint", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");

            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                    {
                        processorRef.loadImpulseResponse(file);
                        imprintFileLabel.setText(file.getFileName(), juce::dontSendNotification);
                    }
                });
        };

        addAndMakeVisible(imprintFileLabel);
        imprintFileLabel.setJustificationType(juce::Justification::centredLeft);
        auto imprintName = processorRef.getIRFileName();
        imprintFileLabel.setText(imprintName.isNotEmpty() ? imprintName : "No imprint loaded", juce::dontSendNotification);

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

        setSize(500, 200);
    }
    else
    {
        // --- Standalone mode: offline convolution ---

        addAndMakeVisible(loadSampleAButton);
        loadSampleAButton.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select Sample A", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");

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
                "Select Sample B", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");

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
                "Save Convolved Output", juce::File{}, "*.wav");

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

        setSize(500, 240);
    }
}

ConvolutionPluginEditor::~ConvolutionPluginEditor()
{
    stopTimer();
    dryWetAttachment.reset();
    gainAttachment.reset();
}

bool ConvolutionPluginEditor::isStandalone() const
{
    return processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
}

void ConvolutionPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    if (! isStandalone() && isDraggingOver)
    {
        g.setColour(juce::Colour(0x30ffffff));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colours::white);
        g.drawRect(getLocalBounds(), 2);
        g.setFont(20.0f);
        g.drawText("Drop audio file to load as imprint",
                    getLocalBounds(), juce::Justification::centred);
    }
}

void ConvolutionPluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    if (! isStandalone())
    {
        // Plugin mode layout
        auto irRow = area.removeFromTop(30);
        loadImprintButton.setBounds(irRow.removeFromLeft(120));
        irRow.removeFromLeft(10);
        imprintFileLabel.setBounds(irRow);

        area.removeFromTop(10);

        auto dwRow = area.removeFromTop(30);
        dryWetLabel.setBounds(dwRow.removeFromLeft(80));
        dryWetSlider.setBounds(dwRow);

        area.removeFromTop(10);

        auto gainRow = area.removeFromTop(30);
        gainLabel.setBounds(gainRow.removeFromLeft(80));
        gainSlider.setBounds(gainRow);
    }
    else
    {
        // Standalone mode layout
        auto saRow = area.removeFromTop(30);
        loadSampleAButton.setBounds(saRow.removeFromLeft(130));
        saRow.removeFromLeft(10);
        sampleALabel.setBounds(saRow);

        area.removeFromTop(10);

        auto sbRow = area.removeFromTop(30);
        loadSampleBButton.setBounds(sbRow.removeFromLeft(130));
        sbRow.removeFromLeft(10);
        sampleBLabel.setBounds(sbRow);

        area.removeFromTop(10);

        auto convRow = area.removeFromTop(30);
        convolveButton.setBounds(convRow.removeFromLeft(150));

        area.removeFromTop(10);
        offlineStatusLabel.setBounds(area.removeFromTop(25));
    }
}

bool ConvolutionPluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (isStandalone())
        return false;

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

    if (isStandalone())
        return;

    for (auto& path : files)
    {
        juce::File file(path);
        auto ext = file.getFileExtension().toLowerCase();
        if (file.existsAsFile() && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"))
        {
            processorRef.loadImpulseResponse(file);
            imprintFileLabel.setText(file.getFileName(), juce::dontSendNotification);
            break;
        }
    }
}

void ConvolutionPluginEditor::timerCallback()
{
    if (isStandalone())
    {
        auto status = offlineConvolver.getStatus();
        offlineStatusLabel.setText(offlineConvolver.getStatusMessage(), juce::dontSendNotification);

        if (status == OfflineConvolver::Status::Done || status == OfflineConvolver::Status::Error)
            stopTimer();
    }
}
