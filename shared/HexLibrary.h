#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>

namespace triad
{
// Disk-backed, per-user DX7 library. Call only from a non-audio thread.
// Each instance owns its snapshot; reload() discovers imports by other instances.
class HexLibrary
{
public:
    using Patch = std::array<unsigned char, 155>;
    struct Entry
    {
        juce::String id, name, folder;
        Patch patch {};
    };
    struct ImportReport
    {
        int added = 0, duplicates = 0, skipped = 0;
        juce::String error;
    };

    static juce::File defaultRoot();
    // Constructor performs no disk access. Call reload() to populate entries.
    explicit HexLibrary (juce::File libraryRoot = defaultRoot());
    const std::vector<Entry>& entries() const noexcept { return voices; }
    juce::File root() const { return directory; }
    bool reload (juce::String* error = nullptr);
    ImportReport importFolder (const juce::File& folder);
    ImportReport importFiles (const juce::Array<juce::File>& files);
    // Permanently cancel this instance's pending/current work (for editor close).
    void requestCancel() noexcept { cancellationRequested.store (true, std::memory_order_relaxed); }

    // Supports standard single-voice and 32-voice DX7 SysEx, including files
    // containing several concatenated messages. Malformed messages are skipped.
    static std::vector<Patch> decode (const void*, size_t bytes);
    static juce::String patchName (const Patch&, const juce::String& fallback = "Untitled");

private:
    ImportReport import (const std::vector<std::pair<juce::File, juce::String>>& sources,
                         int alreadySkipped = 0);
    juce::File directory;
    std::vector<Entry> voices;
    std::atomic<bool> cancellationRequested { false };
    bool cancelled() const noexcept { return cancellationRequested.load (std::memory_order_relaxed); }
};
}
