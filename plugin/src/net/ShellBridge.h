// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Lorkhan RP
// Additional permissions under GPL-3.0 section 7 apply - see EXCEPTIONS.txt.

#pragma once

#include <cstdint>
#include <string>

namespace FUI::Ledger
{
    struct Expired;
}

// Lorkhan server-shell, phase 1 — TELEMETRY ONLY. Changes nothing about how
// the plugin behaves: every note below is a passive copy of something the
// ledger/sinks already decided, broadcast over SKSE messaging for a host
// plugin (SkyrimPlatform on the Lorkhan pack) to forward to the server.
//
// Why this exists before any read-write sync (SERVER-SHELL-PLAN.md §8, phase
// 1): the whole shell design rests on unproven runtime questions — does the
// SkyMP inventory resync raise TESContainerChangedEvents ("outside" deltas
// below)? how often does the FIFO Confirm mis-match under resyncs? what is
// the real consumable double-event rate? These numbers calibrate the
// protocol before a single engine call changes hands.
//
// Contract:
//   - Broadcast SKSE messages, type 'LGTL', data = UTF-8 JSON, sender
//     "GridInventory". No listener = cheap no-op; the fork stays fully
//     functional solo.
//   - Notes may be called from ARBITRARY threads (container events); the
//     bridge marshals everything onto the game thread via the task interface
//     before dispatching, so listeners always receive on the game thread.
//   - Rate-capped; overflow is counted and reported, never blocking.
namespace FUI::ShellBridge
{
    // Called once at kDataLoaded (after forms resolve). Broadcasts a hello.
    void Init();

    // A request was written into the ledger, right before its engine call.
    void NoteSubmit(std::uint32_t a_form, std::int32_t a_delta, const char* a_who,
                    std::uint16_t a_uid, std::uint16_t a_sig, const std::string& a_slot);

    // A request landed (delivered from Ledger::Tick, game thread).
    void NoteLanded(const FUI::Ledger::Expired& a_rec);

    // A request expired unconfirmed (same delivery).
    void NoteExpired(const FUI::Ledger::Expired& a_rec);

    // A player-container delta the ledger did NOT match — a genuine outside
    // change (console, script, or a SkyMP server resync: THE measurement).
    // Arbitrary thread; POD only.
    void NoteOutsideDelta(std::uint32_t a_form, std::int32_t a_delta);
}
