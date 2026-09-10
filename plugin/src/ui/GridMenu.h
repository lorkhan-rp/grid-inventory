// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Smooth <skypia0147-dev@users.noreply.github.com>
// SPDX-FileCopyrightText: 2026 Lorkhan RP
// Portions SPDX-FileCopyrightText: patchulidev (Modex / ModExplorerMenu)
// Additional permissions under GPL-3.0 section 7 apply - see EXCEPTIONS.txt.

#pragma once

#include <imgui.h>

namespace FUI
{
    // Ported from ModExplorerMenu (Modex) by patchulidev — ModexGUIMenu.
    // https://github.com/patchulidev/ModExplorerMenu (GPL-3.0 with Modding Exception)
    //
    // The kCustomRendering menu whose PostDisplay is the game's UI render
    // pass — the only stage where Inventory3DManager::Render() actually
    // draws (MODEX_TRANSITION_PLAN §2 root cause). Also relays Scaleform
    // input events into ImGui.
    class GridInventoryMenu : public RE::IMenu
    {
    public:
        static constexpr std::string_view MENU_NAME = "GridInventoryMenu";

        static void RegisterMenu();
        static void FlushInputState();

        // F1: the titlebar ✕ plays MenuClose itself before UIRoot::Close();
        // this suppresses OnHide's fallback so the sound doesn't double up.
        static void MarkCloseSfxPlayed();

        // Lorkhan (serveur RP) : mode SANS PAUSE. Le monde et les autres
        // joueurs continuent d'exister pendant qu'on est dans son sac — un
        // menu qui fige l'ecran local n'a pas de sens en multijoueur. Defaut
        // ON dans ce fork ; "!nopause = 0" dans GridInventory_ui.ini rend le
        // comportement solo d'origine (utile pour bisecter un rapport).
        static void SetNoPause(bool a_on);
        [[nodiscard]] static bool NoPauseEnabled();

        void PostDisplay() override;
        void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;
        RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

    private:
        void OnShow();
        void OnHide();

        static RE::IMenu* Creator();

        static void ProcessScaleformEvent(const RE::BSUIScaleformData* a_data);
        static void OnMouseEvent(RE::GFxEvent* a_event, bool a_down);
        static void OnMouseWheelEvent(RE::GFxEvent* a_event);
        static void OnKeyEvent(RE::GFxEvent* a_event, bool a_down);
        static void OnCharEvent(RE::GFxEvent* a_event);
        static void ForceCursor();

        static ImGuiKey GFxKeyToImGuiKey(RE::GFxKey::Code a_keyCode);
    };
}
