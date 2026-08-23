#include "net/ShellBridge.h"

#include "game/Ledger.h"

#include <atomic>
#include <fmt/format.h>

namespace
{
    constexpr std::uint32_t kMsgType = 'LGTL';  // Lorkhan Grid TeLemetry
    constexpr int           kProtoVersion = 1;

    // Per-second dispatch cap. Telemetry must never become the load it is
    // supposed to measure: a take-all burst or a resync storm collapses into
    // a dropped-count line rather than a message flood.
    constexpr int kMaxPerSecond = 120;

    std::atomic<std::uint64_t> g_seq{ 0 };
    std::atomic<int>           g_windowCount{ 0 };
    std::atomic<std::uint64_t> g_windowStamp{ 0 };
    std::atomic<std::uint64_t> g_dropped{ 0 };

    // Minimal JSON string escaping for the few free-text fields (who/slot).
    std::string Escaped(const char* a_text)
    {
        std::string out;
        if (!a_text) {
            return out;
        }
        for (const char* p = a_text; *p; ++p) {
            const char c = *p;
            if (c == '"' || c == '\\') {
                out.push_back('\\');
                out.push_back(c);
            } else if (static_cast<unsigned char>(c) >= 0x20) {
                out.push_back(c);
            }
        }
        return out;
    }

    // Game thread only (callers marshal). Dispatches one JSON line, capped.
    void DispatchNow(std::string a_json)
    {
        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) {
            return;
        }
        // One-second window keyed on the seq clock of dispatches themselves:
        // no wall-clock dependency, no extra OS calls on the hot path.
        const auto now = static_cast<std::uint64_t>(std::time(nullptr));
        auto       stamp = g_windowStamp.load(std::memory_order_relaxed);
        if (stamp != now) {
            g_windowStamp.store(now, std::memory_order_relaxed);
            const int dropped = g_windowCount.exchange(0, std::memory_order_relaxed);
            if (dropped > kMaxPerSecond) {
                g_dropped.fetch_add(static_cast<std::uint64_t>(dropped - kMaxPerSecond),
                                    std::memory_order_relaxed);
            }
        }
        if (g_windowCount.fetch_add(1, std::memory_order_relaxed) >= kMaxPerSecond) {
            return;  // counted above on the next window flip
        }
        // +1 for the terminating NUL: listeners on the other side of the ABI
        // get a plain C string they can parse without a length dance.
        messaging->Dispatch(kMsgType, a_json.data(),
                            static_cast<std::uint32_t>(a_json.size() + 1), nullptr);
    }

    // Everything funnels through here: copy the line, hop to the game thread,
    // dispatch. Container events arrive on arbitrary threads (B0's five), and
    // SKSE messaging makes no thread promise — same rule as every consumer in
    // main.cpp.
    void Emit(std::string a_json)
    {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([line = std::move(a_json)]() { DispatchNow(line); });
        }
    }

    std::uint64_t NextSeq()
    {
        return g_seq.fetch_add(1, std::memory_order_relaxed);
    }
}

namespace FUI::ShellBridge
{
    void Init()
    {
        Emit(fmt::format(R"({{"t":"hello","proto":{},"seq":{}}})", kProtoVersion, NextSeq()));
    }

    void NoteSubmit(std::uint32_t a_form, std::int32_t a_delta, const char* a_who,
                    std::uint16_t a_uid, std::uint16_t a_sig, const std::string& a_slot)
    {
        Emit(fmt::format(
            R"({{"t":"submit","seq":{},"form":{},"delta":{},"who":"{}","uid":{},"sig":{},"slot":"{}"}})",
            NextSeq(), a_form, a_delta, Escaped(a_who), a_uid, a_sig, Escaped(a_slot.c_str())));
    }

    void NoteLanded(const FUI::Ledger::Expired& a_rec)
    {
        Emit(fmt::format(
            R"({{"t":"confirm","seq":{},"form":{},"delta":{},"who":"{}"}})",
            NextSeq(), a_rec.form, a_rec.delta, Escaped(a_rec.who)));
    }

    void NoteExpired(const FUI::Ledger::Expired& a_rec)
    {
        Emit(fmt::format(
            R"({{"t":"expire","seq":{},"form":{},"delta":{},"who":"{}","slot":"{}"}})",
            NextSeq(), a_rec.form, a_rec.delta, Escaped(a_rec.who), Escaped(a_rec.slot.c_str())));
    }

    void NoteOutsideDelta(std::uint32_t a_form, std::int32_t a_delta)
    {
        Emit(fmt::format(
            R"({{"t":"outside","seq":{},"form":{},"delta":{},"droppedSoFar":{}}})",
            NextSeq(), a_form, a_delta, g_dropped.load(std::memory_order_relaxed)));
    }
}
