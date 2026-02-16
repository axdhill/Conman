#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

class OfflineConvolver : public juce::Thread
{
public:
    OfflineConvolver();
    ~OfflineConvolver() override;

    void setFiles(const juce::File& sampleA, const juce::File& sampleB, const juce::File& output);
    void run() override;

    enum class Status { Idle, Processing, Done, Error };
    Status getStatus() const { return status.load(); }
    juce::String getStatusMessage() const
    {
        const juce::ScopedLock sl(messageLock);
        return statusMessage;
    }

private:
    void setStatusMessage(const juce::String& msg)
    {
        const juce::ScopedLock sl(messageLock);
        statusMessage = msg;
    }

    juce::File fileA, fileB, outputFile;
    std::atomic<Status> status { Status::Idle };
    juce::CriticalSection messageLock;
    juce::String statusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OfflineConvolver)
};
