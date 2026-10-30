#include "../shared/HexLibrary.h"
#include "../plugins/hex/HexEngine.h"
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
using Patch = triad::HexLibrary::Patch;
void require (bool value, const char* message)
{
    if (! value) throw std::runtime_error (message);
}

std::vector<unsigned char> single (const Patch& patch)
{
    std::vector<unsigned char> result (163);
    const unsigned char header[] = { 0xf0, 0x43, 0, 0, 1, 0x1b };
    std::copy_n (header, 6, result.begin());
    std::copy (patch.begin(), patch.end(), result.begin() + 6);
    unsigned sum = 0;
    for (auto byte : patch) sum += byte;
    result[161] = (unsigned char) ((0u - sum) & 127u);
    result[162] = 0xf7;
    return result;
}

Patch named (Patch patch, const juce::String& name)
{
    std::fill (patch.begin() + 145, patch.end(), ' ');
    const auto ascii = name.toRawUTF8();
    for (int i = 0; i < juce::jmin (10, name.length()); ++i) patch[145 + i] = (unsigned char) ascii[i];
    return patch;
}

std::vector<unsigned char> bank (const std::vector<Patch>& patches)
{
    std::vector<unsigned char> result (4104);
    const unsigned char header[] = { 0xf0, 0x43, 0, 9, 0x20, 0 };
    std::copy_n (header, 6, result.begin());
    for (int voice = 0; voice < 32; ++voice)
    {
        const auto& p = patches[(size_t) voice];
        auto* b = result.data() + 6 + voice * 128;
        for (int op = 0; op < 6; ++op)
        {
            const int u = op * 21, v = op * 17;
            std::copy_n (p.begin() + u, 11, b + v);
            b[v + 11] = p[u + 11] | (p[u + 12] << 2);
            b[v + 12] = p[u + 13] | (p[u + 20] << 3);
            b[v + 13] = p[u + 14] | (p[u + 15] << 2);
            b[v + 14] = p[u + 16];
            b[v + 15] = p[u + 17] | (p[u + 18] << 1);
            b[v + 16] = p[u + 19];
        }
        std::copy_n (p.begin() + 126, 9, b + 102);
        b[111] = p[135] | (p[136] << 3);
        std::copy_n (p.begin() + 137, 4, b + 112);
        b[116] = p[141] | (p[142] << 1) | (p[143] << 4);
        std::copy_n (p.begin() + 144, 11, b + 117);
    }
    unsigned sum = 0;
    for (size_t i = 6; i < 4102; ++i) sum += result[i];
    result[4102] = (unsigned char) ((0u - sum) & 127u);
    result[4103] = 0xf7;
    return result;
}

void write (const juce::File& file, const std::vector<unsigned char>& data)
{
    require (file.getParentDirectory().createDirectory().wasOk(), "create source directory");
    require (file.replaceWithData (data.data(), data.size()), "write source SysEx");
}
}

int main()
{
    auto temporary = juce::File::createTempFile ("hex-library-test");
    struct Cleanup { juce::File path; ~Cleanup() { path.deleteRecursively(); } } cleanup { temporary };
    try
    {
        require (temporary.createDirectory().wasOk(), "create test directory");
        const auto sources = temporary.getChildFile ("Sources");
        const auto store = temporary.getChildFile ("Library");
        triad::HexEngine engine;
        auto one = named (engine.patch(), "SAME NAME");
        auto two = one;
        two[135] = 3;
        auto three = named (engine.patch(), "DEEP BELL");
        write (sources.getChildFile ("Keys/One.SYX"), single (one));
        write (sources.getChildFile ("Keys/Two.sysex"), single (two));
        write (sources.getChildFile ("Nested/Deep/Bell.syx"), single (three));
        std::vector<Patch> bankPatches;
        for (int i = 0; i < 32; ++i)
        {
            auto patch = named (engine.patch(), "SLOT " + juce::String (i + 1));
            patch[134] = (unsigned char) i;
            patch[135] = (unsigned char) (i % 8);
            bankPatches.push_back (patch);
        }
        const auto bankData = bank (bankPatches);
        require (triad::HexLibrary::decode (bankData.data(), bankData.size()) == bankPatches,
                 "all 32 packed bank slots must decode exactly");
        write (sources.getChildFile ("Banks/Collection.sysex"), bankData);
        auto broken = single (one);
        broken[161] ^= 1;
        write (sources.getChildFile ("Bad checksum.syx"), broken);
        write (sources.getChildFile ("Readme.txt"), { 1, 2, 3 });
        const auto link = sources.getChildFile ("Loop");
        const bool madeLink = sources.createSymbolicLink (link, false);

        triad::HexLibrary library (store);
        require (library.entries().empty(), "constructor should not scan disk");
        const auto report = library.importFolder (sources);
        require (report.error.isEmpty(), "folder import should succeed");
        require (report.added == 35 && library.entries().size() == 35, "three singles plus all32 bank voices");
        require (report.skipped == (madeLink ? 2 : 1), "malformed SysEx and symlink must be skipped");
        int sameNames = 0, banks = 0;
        bool foundDeep = false;
        std::set<juce::String> ids;
        for (const auto& entry : library.entries())
        {
            require (ids.insert (entry.id).second, "voice IDs must be unique");
            if (entry.name == "SAME NAME") ++sameNames;
            if (entry.folder == "Sources/Banks/Collection") ++banks;
            if (entry.name == "DEEP BELL" && entry.folder == "Sources/Nested/Deep") foundDeep = true;
        }
        require (sameNames == 2, "different patches with the same name must coexist");
        require (banks == 32 && foundDeep, "bank and nested folder hierarchy should be preserved");
        const auto repeated = library.importFolder (sources);
        require (repeated.added == 0 && repeated.duplicates == 35, "repeat import must not duplicate sounds");

        const auto identicalBank = temporary.getChildFile ("Identical bank.syx");
        write (identicalBank, bank (std::vector<Patch> (32, one)));
        triad::HexLibrary bankLibrary (temporary.getChildFile ("Other Library"));
        const auto identicalReport = bankLibrary.importFiles ({ identicalBank });
        require (identicalReport.added == 32 && bankLibrary.entries().size() == 32,
                 "identical bank slots must remain independently browsable");
        require (bankLibrary.entries().front().name.startsWith ("01 - ")
                     && bankLibrary.entries().back().name.startsWith ("32 - "),
                 "bank slot prefixes must preserve original order");
        require (bankLibrary.importFiles ({ identicalBank }).duplicates == 32,
                 "reimporting identical slots must deduplicate by slot");

        auto combined = single (one);
        combined.push_back (0);
        combined.insert (combined.end(), bankData.begin(), bankData.end());
        const auto concatenated = triad::HexLibrary::decode (combined.data(), combined.size());
        require (concatenated.size() == 33 && concatenated[0] == one && concatenated[32] == bankPatches[31],
                 "concatenated SysEx messages should retain all voices");
        auto malformedBank = bankData;
        malformedBank[4102] ^= 1;
        require (triad::HexLibrary::decode (malformedBank.data(), malformedBank.size()).empty(),
                 "bad bank checksum must be rejected");
        auto invalidPatch = one;
        invalidPatch[134] = 100;
        const auto invalidSingle = single (invalidPatch);
        require (triad::HexLibrary::decode (invalidSingle.data(), invalidSingle.size()).empty(),
                 "out-of-range patch fields must be rejected");
        require (triad::HexLibrary::decode (nullptr, 163).empty(), "null input should be rejected");

        triad::HexLibrary cancelled (temporary.getChildFile ("Cancelled Library"));
        cancelled.requestCancel();
        require (! cancelled.reload() && cancelled.importFiles ({ identicalBank }).added == 0
                     && ! cancelled.root().exists(),
                 "cancelled library operations must stop before disk writes");

        auto next = named (engine.patch(), "NEW SOUND");
        const auto standalone = temporary.getChildFile ("Standalone.SYSEX");
        write (standalone, single (next));
        triad::HexLibrary otherInstance (store);
        const auto files = otherInstance.importFiles ({ standalone });
        require (files.error.isEmpty() && files.added == 1, "individual file import");
        require (library.reload() && library.entries().size() == 36, "reload sees another instance's import");
        const auto saved = library.entries();
        require (sources.deleteRecursively() && standalone.deleteFile(), "remove original sources");
        triad::HexLibrary reopened (store);
        require (reopened.reload() && reopened.entries().size() == saved.size(),
                 "library must survive source deletion and reopening");
        for (size_t i = 0; i < saved.size(); ++i)
        {
            const auto& a = saved[i];
            const auto& b = reopened.entries()[i];
            require (a.id == b.id && a.folder == b.folder && a.name == b.name && a.patch == b.patch,
                     "persistent metadata and exact voice payload must roundtrip");
        }
        std::cout << "Hex library: 32-bank decode, nested imports, deduplication, validation, source deletion, and cross-instance reload passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Hex library test failed: " << error.what() << '\n';
        return 1;
    }
}
