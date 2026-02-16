#include "OfflineConvolver.h"

OfflineConvolver::OfflineConvolver()
    : juce::Thread("OfflineConvolver")
{
}

OfflineConvolver::~OfflineConvolver()
{
    stopThread(5000);
}

void OfflineConvolver::setFiles(const juce::File& sampleA, const juce::File& sampleB, const juce::File& output)
{
    fileA = sampleA;
    fileB = sampleB;
    outputFile = output;
}

void OfflineConvolver::run()
{
    status.store(Status::Processing);
    setStatusMessage("Reading input files...");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Read sample A
    std::unique_ptr<juce::AudioFormatReader> readerA(formatManager.createReaderFor(fileA));
    if (readerA == nullptr)
    {
        setStatusMessage("Error: Could not read Sample A");
        status.store(Status::Error);
        return;
    }

    // Read sample B
    std::unique_ptr<juce::AudioFormatReader> readerB(formatManager.createReaderFor(fileB));
    if (readerB == nullptr)
    {
        setStatusMessage("Error: Could not read Sample B");
        status.store(Status::Error);
        return;
    }

    auto numChannels = std::max(readerA->numChannels, readerB->numChannels);
    auto sampleRate = readerA->sampleRate;
    auto lenA = static_cast<int64_t>(readerA->lengthInSamples);
    auto lenB = static_cast<int64_t>(readerB->lengthInSamples);

    juce::AudioBuffer<float> bufferA(static_cast<int>(numChannels), static_cast<int>(lenA));
    juce::AudioBuffer<float> bufferB(static_cast<int>(numChannels), static_cast<int>(lenB));
    bufferA.clear();
    bufferB.clear();

    readerA->read(&bufferA, 0, static_cast<int>(lenA), 0, true, numChannels > 1);
    readerB->read(&bufferB, 0, static_cast<int>(lenB), 0, true, numChannels > 1);

    if (threadShouldExit()) return;

    setStatusMessage("Convolving...");

    // FFT-based convolution
    int64_t convLen = lenA + lenB - 1;
    int fftOrder = 0;
    int64_t fftSize = 1;
    while (fftSize < convLen)
    {
        fftSize <<= 1;
        fftOrder++;
    }

    juce::dsp::FFT fft(fftOrder);
    auto fftDataSize = fftSize * 2; // complex pairs

    juce::AudioBuffer<float> result(static_cast<int>(numChannels), static_cast<int>(convLen));
    result.clear();

    for (int ch = 0; ch < static_cast<int>(numChannels); ++ch)
    {
        if (threadShouldExit()) return;

        std::vector<float> fftA(static_cast<size_t>(fftDataSize), 0.0f);
        std::vector<float> fftB(static_cast<size_t>(fftDataSize), 0.0f);

        // Copy channel data (or duplicate mono to fill channels)
        int chA = std::min(ch, static_cast<int>(readerA->numChannels) - 1);
        int chB = std::min(ch, static_cast<int>(readerB->numChannels) - 1);
        auto* srcA = bufferA.getReadPointer(chA);
        auto* srcB = bufferB.getReadPointer(chB);

        for (int64_t i = 0; i < lenA; ++i)
            fftA[static_cast<size_t>(i)] = srcA[i];
        for (int64_t i = 0; i < lenB; ++i)
            fftB[static_cast<size_t>(i)] = srcB[i];

        // Forward FFT
        fft.performRealOnlyForwardTransform(fftA.data());
        fft.performRealOnlyForwardTransform(fftB.data());

        // Complex multiplication
        for (int64_t i = 0; i < fftDataSize; i += 2)
        {
            float realA = fftA[static_cast<size_t>(i)];
            float imagA = fftA[static_cast<size_t>(i + 1)];
            float realB = fftB[static_cast<size_t>(i)];
            float imagB = fftB[static_cast<size_t>(i + 1)];

            fftA[static_cast<size_t>(i)]     = realA * realB - imagA * imagB;
            fftA[static_cast<size_t>(i + 1)] = realA * imagB + imagA * realB;
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(fftA.data());

        // Copy result
        auto* dest = result.getWritePointer(ch);
        for (int64_t i = 0; i < convLen; ++i)
            dest[i] = fftA[static_cast<size_t>(i)];
    }

    if (threadShouldExit()) return;

    setStatusMessage("Writing output file...");

    // Normalize to prevent clipping
    float peak = 0.0f;
    for (int ch = 0; ch < result.getNumChannels(); ++ch)
    {
        auto* data = result.getReadPointer(ch);
        for (int i = 0; i < result.getNumSamples(); ++i)
            peak = std::max(peak, std::abs(data[i]));
    }

    if (peak > 1.0f)
        result.applyGain(1.0f / peak);

    // Write WAV
    juce::WavAudioFormat wavFormat;
    auto outputStream = outputFile.createOutputStream();
    if (outputStream == nullptr)
    {
        setStatusMessage("Error: Could not create output file");
        status.store(Status::Error);
        return;
    }

    auto* writer = wavFormat.createWriterFor(outputStream.get(),
                                             sampleRate,
                                             static_cast<unsigned int>(numChannels),
                                             24, {}, 0);

    if (writer == nullptr)
    {
        setStatusMessage("Error: Could not create WAV writer");
        status.store(Status::Error);
        return;
    }

    outputStream.release(); // writer now owns the stream (only after confirmed non-null)

    writer->writeFromAudioSampleBuffer(result, 0, result.getNumSamples());
    delete writer;

    setStatusMessage("Done! Exported to: " + outputFile.getFileName());
    status.store(Status::Done);
}
