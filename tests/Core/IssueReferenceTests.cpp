#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// BUG-03 guard: every issue reference in include/, src/ and tests/ must name its
// tracker. Forbidden: a bare hash-number (no owner/repo before it) and the short
// OSD form. Allowed: Spatial-Media-Lab/OpenSpatialDelay<hash>N, AndrewRahman/SpatialCore<hash>N,
// and the project shorthand SpatialCore<hash>N. The forbidden patterns are built from
// character constants so this file never trips its own scan.

namespace fs = std::filesystem;

namespace
{
constexpr char kHash = '#';

bool isTrackerChar (char c)
{
    return std::isalnum (static_cast<unsigned char> (c)) || c == '/' || c == '-' || c == '_';
}

/** Returns a description of the violation on this line, or empty. */
std::string scanLine (const std::string& line)
{
    const std::string osdShort = std::string ("OSD") + kHash;
    for (size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] != kHash || i + 1 >= line.size()
            || ! std::isdigit (static_cast<unsigned char> (line[i + 1])))
            continue;

        size_t end = i + 1;
        while (end < line.size() && std::isdigit (static_cast<unsigned char> (line[end])))
            ++end;

        // Hex colour such as a hash followed by digits then hex letters: not an issue ref.
        if (end < line.size() && std::isalnum (static_cast<unsigned char> (line[end])))
            continue;

        if (i >= 3 && line.compare (i - 3, 4, osdShort) == 0)
            return "OSD short form";

        if (i == 0 || ! isTrackerChar (line[i - 1]))
            return "bare issue number";
    }
    return {};
}
} // namespace

TEST_CASE ("Issue references in include, src and tests name their tracker", "[bug03]")
{
    const fs::path root (SPATIALCORE_SOURCE_DIR);
    std::vector<std::string> violations;
    int filesScanned = 0;

    for (const char* dir : { "include", "src", "tests" })
    {
        const fs::path base = root / dir;
        REQUIRE (fs::exists (base));
        for (const auto& entry : fs::recursive_directory_iterator (base))
        {
            if (! entry.is_regular_file()) continue;
            const auto ext = entry.path().extension().string();
            if (ext != ".h" && ext != ".cpp") continue;
            if (entry.path().filename() == "IssueReferenceTests.cpp") continue;

            ++filesScanned;
            std::ifstream in (entry.path());
            std::string line;
            int n = 0;
            while (std::getline (in, line))
            {
                ++n;
                const auto what = scanLine (line);
                if (! what.empty())
                    violations.push_back (fs::relative (entry.path(), root).string()
                                          + ":" + std::to_string (n) + " " + what);
            }
        }
    }

    REQUIRE (filesScanned > 50); // the scan really walked the tree

    std::string report;
    for (const auto& v : violations) report += "\n  " + v;
    INFO ("Unqualified issue references:" << report);
    CHECK (violations.empty());
}
