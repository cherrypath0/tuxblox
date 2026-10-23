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

#include "update_decision.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

InstallState coherent() {
    InstallState s;
    s.releaseRecorded = true;
    s.recordedRelease = "3.6.0.0";
    s.recordedVersions = {{"launcher", "3.5.0"}, {"installer", "3.1.0"}, {"studio-mcp", "2.0.1"},
                          {"compat", "3.3.0"}, {"libtuxblox", "3.1.0"}};
    s.installedVersions = s.recordedVersions;
    return s;
}

void test_a_coherent_install_on_the_named_release_does_nothing() {
    UpdateDecision d = decideUpdate(coherent(), "3.6.0.0", false);
    assert(!d.update);
    assert(!d.forced);
    assert(d.incoherent.empty());
    printf("  a coherent install on the named release does nothing\n");
}

// Five different component versions is the NORMAL state now, not a half-applied update.
void test_differing_component_versions_are_not_a_mismatch() {
    InstallState s = coherent();
    assert(!decideUpdate(s, "3.6.0.0", false).update);
    printf("  differing component versions are not a mismatch\n");
}

void test_a_stale_install_updates_but_is_not_forced() {
    UpdateDecision d = decideUpdate(coherent(), "3.6.0.1", false);
    assert(d.update);
    assert(!d.forced);
    printf("  a stale install updates but is not forced\n");
}

// Rollback: the channel moved back to an earlier release and the install has to follow it.
// "recorded != named" is the whole comparison, so there is no second direction to reason about.
void test_a_release_behind_the_recorded_one_is_an_ordinary_update() {
    UpdateDecision d = decideUpdate(coherent(), "3.5.0.0", false);
    assert(d.update);
    assert(!d.forced);
    printf("  a release behind the recorded one is an ordinary update\n");
}

void test_a_component_at_the_wrong_version_is_forced() {
    InstallState s = coherent();
    s.installedVersions["compat"] = "3.2.0";          // an update that died midway
    UpdateDecision d = decideUpdate(s, "3.6.0.0", false);
    assert(d.update);
    assert(d.forced);
    assert(d.incoherent == std::vector<std::string>{"compat"});
    printf("  a component at the wrong version is forced\n");
}

void test_a_component_the_release_names_but_the_install_lacks_is_forced() {
    InstallState s = coherent();
    s.installedVersions.erase("studio-mcp");
    UpdateDecision d = decideUpdate(s, "3.6.0.0", false);
    assert(d.update);
    assert(d.forced);
    assert(d.incoherent == std::vector<std::string>{"studio-mcp"});
    printf("  a component the release names but the install lacks is forced\n");
}

// Review Focus 5. stage.sh replaces components one at a time, so a locally built install is
// incoherent by construction; noRollback is what stops it being repaired over the top of itself.
void test_norollback_suppresses_the_forced_path_only() {
    InstallState s = coherent();
    s.installedVersions["compat"] = "3.2.0";
    UpdateDecision blocked = decideUpdate(s, "3.6.0.0", true);
    assert(!blocked.update);
    assert(!blocked.incoherent.empty());             // still reported, so it can be logged
    assert(!blocked.reason.empty());

    // Staleness is NOT suppressed: the install is still offered the named release, which
    // Auto-Update then gates. A local build with Auto-Update on can still be replaced.
    UpdateDecision stale = decideUpdate(coherent(), "3.6.0.1", true);
    assert(stale.update);
    assert(!stale.forced);
    printf("  norollback suppresses the forced path only\n");
}

// The repo owner's decision, 2026-10-23: an install that predates the release file is OFFERED
// an update, not forced into one. Nobody is updated without being asked.
void test_an_install_with_no_record_is_offered_an_ordinary_update() {
    InstallState s;                                   // releaseRecorded = false
    s.installedVersions = {{"launcher", "3.5.0"}, {"compat", "3.3.0"}};
    UpdateDecision d = decideUpdate(s, "3.6.0.0", false);
    assert(d.update);
    assert(!d.forced);
    assert(d.incoherent.empty());                     // nothing to be incoherent WITH
    assert(d.reason.find("record") != std::string::npos);
    printf("  an install with no record is offered an ordinary update\n");
}

void test_an_install_with_no_record_and_nothing_published_does_nothing() {
    InstallState s;
    UpdateDecision d = decideUpdate(s, "", false);
    assert(!d.update);
    printf("  an install with no record and nothing published does nothing\n");
}

void test_a_component_the_release_does_not_name_is_ignored() {
    InstallState s = coherent();
    s.installedVersions["leftover"] = "1.0.0";        // e.g. a file an older release placed
    assert(!decideUpdate(s, "3.6.0.0", false).update);
    printf("  a component the release does not name is ignored\n");
}

// The order the pieces are reported in is the fixed component order, not whatever a map iterates
// in: the reason is printed to a person, and "compat, launcher" reading differently from run to
// run is the kind of thing that makes a bug report hard to compare against another one.
void test_several_incoherent_components_are_reported_in_component_order() {
    InstallState s = coherent();
    s.installedVersions["libtuxblox"] = "3.0.0";
    s.installedVersions["compat"] = "3.2.0";
    s.installedVersions.erase("launcher");
    UpdateDecision d = decideUpdate(s, "3.6.0.0", false);
    assert(d.forced);
    assert((d.incoherent == std::vector<std::string>{"launcher", "compat", "libtuxblox"}));
    printf("  several incoherent components are reported in component order\n");
}

} // namespace

int main() {
    test_a_coherent_install_on_the_named_release_does_nothing();
    test_differing_component_versions_are_not_a_mismatch();
    test_a_stale_install_updates_but_is_not_forced();
    test_a_release_behind_the_recorded_one_is_an_ordinary_update();
    test_a_component_at_the_wrong_version_is_forced();
    test_a_component_the_release_names_but_the_install_lacks_is_forced();
    test_norollback_suppresses_the_forced_path_only();
    test_an_install_with_no_record_is_offered_an_ordinary_update();
    test_an_install_with_no_record_and_nothing_published_does_nothing();
    test_a_component_the_release_does_not_name_is_ignored();
    test_several_incoherent_components_are_reported_in_component_order();
    printf("update_decision: all tests passed\n");
    return 0;
}
