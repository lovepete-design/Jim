#include "HexLibrary.h"
#include "../plugins/hex/HexEngine.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>

namespace triad
{
namespace
{
constexpr int maxVoices = 50000;
constexpr int maxSourceFiles = 10000;
constexpr int maxVisitedItems = 100000;
constexpr juce::int64 maxSourceBytes = 1024 * 1024;

juce::String identity (const juce::String& folder, const juce::String& name,
                       const HexLibrary::Patch& patch)
{
    // Two independent stable hashes. IDs are filenames, not security tokens.
    std::uint64_t a = UINT64_C(14695981039346656037), b = UINT64_C(7809847782465536322);
    const auto consume = [&] (unsigned char byte)
    {
        a = (a ^ byte) * UINT64_C(1099511628211);
        b = (b ^ byte) * UINT64_C(14029467366897019727);
    };
    const auto text = folder + "\n" + name + "\n";
    const auto utf = text.toRawUTF8();
    for (size_t i = 0; i < text.getNumBytesAsUTF8(); ++i) consume ((unsigned char) utf[i]);
    for (const auto byte : patch) consume (byte);
    return juce::String::toHexString ((juce::int64) a).paddedLeft ('0', 16)
         + juce::String::toHexString ((juce::int64) b).paddedLeft ('0', 16);
}

bool validatePatch (const HexLibrary::Patch& patch)
{
    std::array<unsigned char, 163> syx {{ 0xf0, 0x43, 0, 0, 1, 0x1b }};
    std::copy (patch.begin(), patch.end(), syx.begin() + 6);
    unsigned sum = 0;
    for (auto byte : patch) sum += byte;
    syx[161] = (unsigned char) ((0u - sum) & 127u);
    syx[162] = 0xf7;
    HexLibrary::Patch checked {};
    return HexEngine::parseSysex (syx.data(), syx.size(), checked);
}

bool supported (const juce::File& file)
{
    const auto extension = file.getFileExtension();
    return extension.equalsIgnoreCase (".syx") || extension.equalsIgnoreCase (".sysex");
}

bool decodeRecord (const juce::File& file, HexLibrary::Entry& entry)
{
    if (file.isSymbolicLink() || file.getSize() <= 0 || file.getSize() > 16384) return false;
    const auto data = juce::JSON::parse (file);
    if (! data.isObject() || (int) data["version"] != 1) return false;
    entry.id = data["id"].toString();
    entry.name = data["name"].toString();
    entry.folder = data["folder"].toString();
    if (entry.id.isEmpty() || entry.id.length() > 80 || entry.name.isEmpty()
        || entry.name.length() > 256 || entry.folder.length() > 2048) return false;
    if (entry.id != file.getFileNameWithoutExtension()) return false;
    juce::MemoryBlock bytes;
    if (! bytes.fromBase64Encoding (data["patch"].toString()) || bytes.getSize() != entry.patch.size())
        return false;
    std::memcpy (entry.patch.data(), bytes.getData(), entry.patch.size());
    return validatePatch (entry.patch);
}

juce::String encodeRecord (const HexLibrary::Entry& entry)
{
    auto* object = new juce::DynamicObject;
    object->setProperty ("version", 1);
    object->setProperty ("id", entry.id);
    object->setProperty ("name", entry.name);
    object->setProperty ("folder", entry.folder);
    object->setProperty ("patch", juce::MemoryBlock (entry.patch.data(), entry.patch.size()).toBase64Encoding());
    return juce::JSON::toString (juce::var (object), true);
}

void sortEntries (std::vector<HexLibrary::Entry>& entries)
{
    std::sort (entries.begin(), entries.end(), [] (const auto& a, const auto& b)
    {
        const auto folderOrder = a.folder.compareNatural (b.folder);
        if (folderOrder != 0) return folderOrder < 0;
        const auto nameOrder = a.name.compareNatural (b.name);
        return nameOrder != 0 ? nameOrder < 0 : a.id < b.id;
    });
}
}

juce::File HexLibrary::defaultRoot()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Triad Audio/Hex MkI/Sound Library");
}

HexLibrary::HexLibrary (juce::File libraryRoot) : directory (std::move (libraryRoot))
{
}

bool HexLibrary::reload (juce::String* error)
{
    if (error != nullptr) error->clear();
    if (cancelled())
    {
        if (error != nullptr) *error = "Sound library operation cancelled.";
        return false;
    }
    const auto storage = directory.getChildFile ("Voices");
    if (! storage.exists())
    {
        voices.clear();
        return true;
    }
    if (! storage.isDirectory() || storage.isSymbolicLink())
    {
        if (error != nullptr) *error = "The sound library's Voices folder is unavailable.";
        return false;
    }
    std::vector<Entry> found;
    int visited = 0;
    for (const auto& item : juce::RangedDirectoryIterator (storage, false, "*.hexvoice",
                                                          juce::File::findFiles))
    {
        if (cancelled())
        {
            if (error != nullptr) *error = "Sound library operation cancelled.";
            return false;
        }
        if (++visited > maxVisitedItems) break;
        Entry entry;
        if (decodeRecord (item.getFile(), entry)) found.push_back (std::move (entry));
        if ((int) found.size() >= maxVoices) break;
    }
    sortEntries (found);
    voices = std::move (found);
    return true;
}

juce::String HexLibrary::patchName (const Patch& patch, const juce::String& fallback)
{
    juce::String name;
    for (int i = 145; i < 155; ++i)
        name += juce::String::charToString (patch[(size_t) i] >= 32 && patch[(size_t) i] <= 126
                                              ? patch[(size_t) i] : ' ');
    name = name.trim();
    return name.isNotEmpty() ? name : fallback;
}

std::vector<HexLibrary::Patch> HexLibrary::decode (const void* source, size_t bytes)
{
    std::vector<Patch> result;
    if (source == nullptr || bytes == 0 || bytes > (size_t) maxSourceBytes) return result;
    const auto* data = static_cast<const unsigned char*> (source);
    size_t cursor = 0;
    while (cursor < bytes)
    {
        while (cursor < bytes && data[cursor] != 0xf0) ++cursor;
        if (cursor == bytes) break;
        const auto start = cursor++;
        while (cursor < bytes && data[cursor] != 0xf0 && data[cursor] != 0xf7) ++cursor;
        if (cursor == bytes) break;
        if (data[cursor] == 0xf0) continue;
        const auto length = ++cursor - start;
        const auto* message = data + start;
        if (length == 163)
        {
            Patch patch {};
            if (HexEngine::parseSysex (message, length, patch)) result.push_back (patch);
            continue;
        }
        if (length != 4104 || message[1] != 0x43 || message[2] > 0x0f
            || message[3] != 9 || message[4] != 0x20 || message[5] != 0) continue;
        unsigned sum = 0;
        bool valid = true;
        for (size_t i = 6; i < 4103; ++i)
        {
            if (message[i] > 127) valid = false;
            sum += message[i];
        }
        if (! valid || (sum & 127u) != 0) continue;
        std::vector<Patch> bankVoices;
        for (int voice = 0; voice < 32; ++voice)
        {
            const auto* b = message + 6 + 128 * voice;
            Patch patch {};
            for (int op = 0; op < 6; ++op)
            {
                const int u = op * 21, v = op * 17;
                std::copy_n (b + v, 11, patch.begin() + u);
                patch[u + 11] = b[v + 11] & 3;
                patch[u + 12] = (b[v + 11] >> 2) & 3;
                patch[u + 13] = b[v + 12] & 7;
                patch[u + 20] = (b[v + 12] >> 3) & 15;
                patch[u + 14] = b[v + 13] & 3;
                patch[u + 15] = (b[v + 13] >> 2) & 7;
                patch[u + 16] = b[v + 14];
                patch[u + 17] = b[v + 15] & 1;
                patch[u + 18] = (b[v + 15] >> 1) & 31;
                patch[u + 19] = b[v + 16];
            }
            std::copy_n (b + 102, 9, patch.begin() + 126);
            patch[135] = b[111] & 7;
            patch[136] = (b[111] >> 3) & 1;
            std::copy_n (b + 112, 4, patch.begin() + 137);
            patch[141] = b[116] & 1;
            patch[142] = (b[116] >> 1) & 7;
            patch[143] = (b[116] >> 4) & 7;
            std::copy_n (b + 117, 11, patch.begin() + 144);
            if (validatePatch (patch)) bankVoices.push_back (patch);
        }
        // A malformed bank is rejected as a whole so slots never shift silently.
        if (bankVoices.size() == 32) result.insert (result.end(), bankVoices.begin(), bankVoices.end());
    }
    return result;
}

HexLibrary::ImportReport HexLibrary::importFolder (const juce::File& folder)
{
    if (cancelled()) return { 0, 0, 0, "Import cancelled." };
    if (! folder.isDirectory() || folder.isSymbolicLink())
        return { 0, 0, 0, "Choose a folder containing DX7 .syx or .sysex sounds." };
    std::vector<std::pair<juce::File, juce::String>> sources;
    struct Pending { juce::File folder; juce::String relative; int depth; };
    std::vector<Pending> pending {{ folder, folder.getFileName(), 0 }};
    int visited = 0, skipped = 0;
    bool limited = false;
    while (! pending.empty() && ! limited)
    {
        auto current = std::move (pending.back());
        pending.pop_back();
        for (const auto& item : juce::RangedDirectoryIterator (current.folder, false, "*",
                 juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
        {
            if (cancelled()) return { 0, 0, skipped, "Import cancelled." };
            if (++visited > maxVisitedItems || (int) sources.size() >= maxSourceFiles)
            {
                limited = true;
                break;
            }
            const auto file = item.getFile();
            if (file.isSymbolicLink()) { ++skipped; continue; }
            if (file.isDirectory())
            {
                if (current.depth >= 32) { ++skipped; continue; }
                const auto relative = current.relative + "/" + file.getFileName();
                if (relative.length() > 2048) { ++skipped; continue; }
                pending.push_back ({ file, relative, current.depth + 1 });
            }
            else if (supported (file)) sources.emplace_back (file, current.relative);
        }
    }
    std::sort (sources.begin(), sources.end(), [] (const auto& a, const auto& b)
    {
        return a.first.getFullPathName().compareNatural (b.first.getFullPathName()) < 0;
    });
    auto report = import (sources, skipped);
    if (limited && report.error.isEmpty())
        report.error = "Import stopped at the folder-size limit. Import smaller subfolders to add the rest.";
    return report;
}

HexLibrary::ImportReport HexLibrary::importFiles (const juce::Array<juce::File>& files)
{
    std::vector<std::pair<juce::File, juce::String>> sources;
    for (const auto& file : files)
    {
        if (cancelled()) return { 0, 0, 0, "Import cancelled." };
        sources.emplace_back (file, "Imported sounds");
        if ((int) sources.size() >= maxSourceFiles) break;
    }
    auto report = import (sources);
    if (files.size() > maxSourceFiles && report.error.isEmpty())
        report.error = "Import stopped after 10,000 files. Select fewer files to add the rest.";
    return report;
}

HexLibrary::ImportReport HexLibrary::import (
    const std::vector<std::pair<juce::File, juce::String>>& sources, int alreadySkipped)
{
    ImportReport report;
    report.skipped = alreadySkipped;
    if (cancelled()) { report.error = "Import cancelled."; return report; }
    const auto storage = directory.getChildFile ("Voices");
    const auto created = storage.createDirectory();
    if (created.failed() || storage.isSymbolicLink())
    {
        report.error = "Could not create the sound library: " + created.getErrorMessage();
        return report;
    }
    if (! reload (&report.error)) return report;
    std::set<juce::String> known;
    for (const auto& entry : voices) known.insert (identity (entry.folder, entry.name, entry.patch));
    for (const auto& source : sources)
    {
        if (cancelled()) { report.error = "Import cancelled."; break; }
        const auto& file = source.first;
        if (! supported (file) || file.isSymbolicLink() || ! file.existsAsFile()
            || file.getSize() <= 0 || file.getSize() > maxSourceBytes)
        {
            ++report.skipped;
            continue;
        }
        juce::MemoryBlock bytes;
        if (! file.loadFileAsData (bytes)) { ++report.skipped; continue; }
        const auto patches = decode (bytes.getData(), bytes.getSize());
        if (patches.empty()) { ++report.skipped; continue; }
        const auto bankFolder = patches.size() > 1
            ? source.second + "/" + file.getFileNameWithoutExtension() : source.second;
        if (bankFolder.length() > 2048) { ++report.skipped; continue; }
        int slot = 0;
        for (const auto& patch : patches)
        {
            if (cancelled())
            {
                report.error = "Import cancelled.";
                sortEntries (voices);
                return report;
            }
            ++slot;
            Entry entry;
            entry.folder = bankFolder;
            entry.name = patchName (patch, file.getFileNameWithoutExtension()
                                           + (patches.size() > 1 ? " " + juce::String (slot) : "")).substring (0, 256);
            // Keep all bank slots, including identical INIT voices, in their
            // original order. Slot prefixes also distinguish repeated names.
            if (patches.size() > 1)
                entry.name = (juce::String (slot).paddedLeft ('0', 2) + " - " + entry.name).substring (0, 256);
            entry.patch = patch;
            const auto key = identity (entry.folder, entry.name, entry.patch);
            if (known.count (key) != 0) { ++report.duplicates; continue; }
            if ((int) voices.size() >= maxVoices)
            {
                report.error = "The sound library holds up to 50,000 sounds.";
                sortEntries (voices);
                return report;
            }
            entry.id = key;
            auto target = storage.getChildFile (entry.id + ".hexvoice");
            int collision = 0;
            bool duplicate = false;
            while (target.exists())
            {
                Entry existing;
                if (decodeRecord (target, existing) && existing.folder == entry.folder
                    && existing.name == entry.name && existing.patch == entry.patch)
                {
                    duplicate = true;
                    break;
                }
                entry.id = key + "-" + juce::String (++collision);
                target = storage.getChildFile (entry.id + ".hexvoice");
            }
            if (duplicate) { ++report.duplicates; known.insert (key); continue; }
            const auto json = encodeRecord (entry);
            if (! target.replaceWithData (json.toRawUTF8(), json.getNumBytesAsUTF8()))
            {
                report.error = "Could not save a sound. Check the sound-library folder is writable and the disk has space.";
                sortEntries (voices);
                return report;
            }
            known.insert (key);
            voices.push_back (std::move (entry));
            ++report.added;
        }
    }
    sortEntries (voices);
    return report;
}
}
