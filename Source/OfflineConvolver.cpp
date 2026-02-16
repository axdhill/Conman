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
    statusMessage = "Reading input files...";

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Read sample A
    std::unique_ptr<juce::AudioFormatReader> readerA(formatManager.createReaderFor(fileA));
    if (readerA == nullptr)
    {
        statusMessage = "Error: Could not read Sample A";
        status.store(Status::Error);
        return;
    }

    // Read sample B
    std::unique_ptr<juce::AudioFormatReader> readerB(formatManager.createReaderFor(fileB));
    if (readerB == nullptr)
    {
        statusMessage = "Error: Could not read Sample B";
        status.store(Status::Error);
        return;
    }

    auto numChannels = std::max(readerA->numChannels, readerB->numChannels);
    auto sampleRate = readerA->sampleRate;
    auto lenA = static_cast<int>(readerA->lengthInSamples);
    auto lenB = static_cast<int>(readerB->lengthInSamples);

    juce::AudioBuffer<float> bufferA(static_cast<int>(numChannels), lenA);
    juce::AudioBuffer<float> bufferB(static_cast<int>(numChannels), lenB);
    bufferA.clear();
    bufferB.clear();

    readerA->read(&bufferA, 0, lenA, 0, true, numChannels > 1);
    readerB->read(&bufferB, 0, lenB, 0, true, numChannels > 1);

    if (threadShouldExit()) return;

    statusMessage = "Convolving...";

    // FFT-based convolution
    int convLen = lenA + lenB - 1;
    int fftOrder = 0;
    int fftSize = 1;
    while (fftSize < convLen)
    {
        fftSize <<= 1;
        fftOrder++;
    }

    juce::dsp::FFT fft(fftOrder);
    auto fftDataSize = fftSize * 2; // complex pairs

    juce::AudioBuffer<float> result(static_cast<int>(numChannels), convLen);
    result.clear();

    for (int ch = 0; ch < static_cast<int>(numChannels); ++ch)
    {
        if (threadShouldExit()) return;

        std::vector<float> fftA(static_cast<size_t>(fftDataSize), 0.0f);
        std::vector<float> fftB(static_cast<size_t>(fftDataSize), 0.0f);

        // Copy channel data (or duplicate mono to fill channels)
        auto* srcA = bufferA.getReadPointer(std::min(ch, static_cast<int>(readerA->numChannels) - 1));
        auto* srcB = bufferB.getReadPointer(std::min(ch, static_cast<int>(readerB->numChannels) - 1));

        for (int i = 0; i < lenA; ++i)
            fftA[static_cast<size_t>(i)] = srcA[i];
        for (int i = 0; i < lenB; ++i)
            fftB[static_cast<size_t>(i)] = srcB[i];

        // Forward FFT
        fft.performRealOnlyForwardTransform(fftA.data());
        fft.performRealOnlyForwardTransform(fftB.data());

        // Complex multiplication
        for (int i = 0; i < fftDataSize; i += 2)
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
        for (int i = 0; i < convLen; ++i)
            dest[i] = fftA[static_cast<size_t>(i)];
    }

    if (threadShouldExit()) return;

    statusMessage = "Writing output file...";

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
    std::unique_ptr<juce::FileOutputStream> outputStream(outputFile.createOutputStream());
    if (outputStream == nullptr)
    {
        statusMessage = "Error: Could not create output file";
        status.store(Status::Error);
        return;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(),
                                  sampleRate,
                                  static_cast<unsigned int>(numChannels),
                                  24, {}, 0));

    if (writer == nullptr)
    {
        statusMessage = "Error: Could not create WAV writer";
        status.store(Status::Error);
        return;
    }

    outputStream.release(); // writer now owns the stream

    writer->writeFromAudioSampleBuffer(result, 0, result.getNumSamples());
    writer.reset();

    statusMessage = "Done! Exported to: " + outputFile.getFileName();
    status.store(Status::Done);
}
