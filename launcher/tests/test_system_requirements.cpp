// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "system_requirements.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace {

// The real contents of /proc/driver/nvidia/version, in both forms the driver writes it.
const char* const kNvidiaProprietary =
    "NVRM version: NVIDIA UNIX x86_64 Kernel Module  550.107.02  Wed Jul 24 20:00:00 UTC 2024\n"
    "GCC version:  gcc version 13.2.0\n";
const char* const kNvidiaOpen =
    "NVRM version: NVIDIA UNIX Open Kernel Module for x86_64  615.71.09  Release Build  (root@)  \n"
    "GCC version:  gcc version 16.2.1 20260810 (GCC) \n";

tuxblox::SystemFacts supportedMachine() {
    tuxblox::SystemFacts facts;
    facts.kernelRelease = "6.14.2-arch1-1";
    facts.nvidiaProcContent = kNvidiaOpen;
    facts.vulkanLoadable = true;
    return facts;
}

} // namespace

int main() {
    using namespace tuxblox;

    // The published floor is Linux 6.7: below it Roblox's own protection stops for the slow way the
    // kernel has to answer "which pages has this program written to".
    assert(kernelIsOlderThan("6.1.0-generic", 6, 7));
    assert(kernelIsOlderThan("5.15.0-91-generic", 6, 7));
    assert(kernelIsOlderThan("6.6.99", 6, 7));
    assert(!kernelIsOlderThan("6.7.0", 6, 7));
    assert(!kernelIsOlderThan("6.7.0-arch1-1", 6, 7));
    assert(!kernelIsOlderThan("6.14.2-arch1-1", 6, 7));
    assert(!kernelIsOlderThan("7.2.6-arch2-1", 6, 7));
    assert(!kernelIsOlderThan("10.0.0", 6, 7));

    // A version that cannot be read must never refuse the machine: being unable to tell is not
    // evidence of being too old, and locking someone out on a guess is the worse outcome.
    assert(!kernelIsOlderThan("", 6, 7));
    assert(!kernelIsOlderThan("not-a-kernel", 6, 7));
    assert(!kernelIsOlderThan("6", 6, 7));

    // Both driver spellings are read, since the open and the proprietary module word the line differently.
    assert(nvidiaDriverVersion(kNvidiaProprietary) == "550.107.02");
    assert(nvidiaDriverVersion(kNvidiaOpen) == "615.71.09");

    // Anything that is not that file yields nothing rather than a wrong answer.
    assert(nvidiaDriverVersion("").empty());
    assert(nvidiaDriverVersion("some other file entirely\n").empty());
    assert(nvidiaDriverVersion("NVRM version: NVIDIA UNIX x86_64 Kernel Module\n").empty());

    // The published NVIDIA floor is 418.49.04, compared part by part rather than as text.
    assert(versionIsOlder("418.49.03", "418.49.04"));
    assert(versionIsOlder("390.157", "418.49.04"));
    assert(versionIsOlder("418.49", "418.49.04"));
    assert(!versionIsOlder("418.49.04", "418.49.04"));
    assert(!versionIsOlder("550.107.02", "418.49.04"));
    assert(!versionIsOlder("615.71.09", "418.49.04"));
    // "9" against "10" must not compare as text, or every two-digit release would read as older.
    assert(!versionIsOlder("1000.1", "999.1"));
    assert(versionIsOlder("999.1", "1000.1"));
    assert(!versionIsOlder("", "418.49.04"));
    assert(!versionIsOlder("garbage", "418.49.04"));

    // A machine that meets everything is not stopped.
    assert(unmetRequirements(supportedMachine()).empty());

    // Each failure is reported on its own, and says what was found so the message is actionable.
    {
        SystemFacts facts = supportedMachine();
        facts.kernelRelease = "6.1.0-generic";
        const std::vector<UnmetRequirement> unmet = unmetRequirements(facts);
        assert(unmet.size() == 1);
        assert(unmet[0].found == "6.1.0-generic");
        assert(!unmet[0].requirement.empty());
    }
    {
        SystemFacts facts = supportedMachine();
        facts.nvidiaProcContent = kNvidiaProprietary;
        assert(unmetRequirements(facts).empty()); // 550 is well past the floor
        facts.nvidiaProcContent =
            "NVRM version: NVIDIA UNIX x86_64 Kernel Module  390.157  Wed Jul 24 20:00:00 UTC 2024\n";
        const std::vector<UnmetRequirement> unmet = unmetRequirements(facts);
        assert(unmet.size() == 1);
        assert(unmet[0].found == "390.157");
    }
    {
        SystemFacts facts = supportedMachine();
        facts.vulkanLoadable = false;
        const std::vector<UnmetRequirement> unmet = unmetRequirements(facts);
        assert(unmet.size() == 1);
        assert(!unmet[0].requirement.empty());
    }

    // A machine with no NVIDIA driver is not judged against the NVIDIA floor. Mesa's version cannot be
    // read without glxinfo installed, so it is never a reason to refuse.
    {
        SystemFacts facts = supportedMachine();
        facts.nvidiaProcContent = "";
        assert(unmetRequirements(facts).empty());
    }

    // Several failures at once are all reported, so a user fixes one and does not meet the next as a surprise.
    {
        SystemFacts facts;
        facts.kernelRelease = "5.4.0";
        facts.nvidiaProcContent = "NVRM version: NVIDIA UNIX x86_64 Kernel Module  390.157  Wed\n";
        facts.vulkanLoadable = false;
        assert(unmetRequirements(facts).size() == 3);
    }

    // The message names every requirement and what was found, and is empty when there is nothing wrong.
    {
        SystemFacts facts = supportedMachine();
        facts.kernelRelease = "6.1.0-generic";
        const std::string message = unsupportedSystemMessage(unmetRequirements(facts));
        assert(message.find("6.1.0-generic") != std::string::npos);
        assert(message.find("6.7") != std::string::npos);
        assert(unsupportedSystemMessage({}).empty());
    }

    // The real machine is read without throwing, whatever it happens to be.
    {
        const SystemFacts facts = collectSystemFacts();
        assert(!facts.kernelRelease.empty());
    }

    printf("system_requirements: all tests passed\n");
    return 0;
}
