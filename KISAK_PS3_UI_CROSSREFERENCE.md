# CS:GO Menu & Navigation Cross-Reference: PS3 Scaleform UI vs Kisak-Strike (PC/PS4)

## 1. Sources surveyed

**PS3/PS4 side** — `/Volumes/Untitled/Counter Strike Global Offensive/`
- `gfx_scripts/` — 3,519 decompiled ActionScript files from 140 `.swf`/`.gfx` movies (JPEXS FFDEC export)
- `gfx_decompiled/SCALEFORM_UI_LAYOUT.md` — architecture summary
- `pc_ui_extracted/` — 9 VGUI `.res` files shipped alongside the console build (`basechat`, `classmenu`, `controllerdialog`, `l4d360ui_audio`, `l4d360ui_controller`, `l4d360ui_controlleroptions`, `mainbuymenu`, `scoreboard`, `settingsdialog`)

**Kisak-Strike side** — `./Kisak-Strike/`
- `game/client/cstrike15/Scaleform/` — Scaleform menu/HUD C++ drivers
- `game/client/cstrike15/gameui/` and `gameui/cstrike15/` — VGUI GameUI
- `game/client/cstrike15/RocketUI/` — libRocket HTML fallback UI
- `vgui2/vgui_controls/ControllerMap.cpp` — controller key table
- `ps4/scaleform_gfx_manager.cpp` — PS4 Scaleform movie manager + boot state machine
- `ps4/scaleform_menu_actions.h` — PS4 menu command → action map
- `KISAK_PS4_PORT.md` — port policy and boot-sequence plan

---

## 2. UI framework choice

| Platform | Primary UI | Secondary | Asset format |
|---|---|---|---|
| **PS3 (authentic)** | Scaleform 4.x (AS2) | VGUI `.res` for a few legacy panels | `.gfx`/`.swf` + a handful of `.res` |
| **Kisak PC** | VGUI2 (`.res`) | Scaleform (gated by `INCLUDE_SCALEFORM`), RocketUI (`INCLUDE_ROCKETUI`) | `.res` (GameMenu.res) + `.gfx` |
| **Kisak PS4 (target)** | Scaleform via OpenGNM HAL | RocketUI as dev fallback | `.gfx`/`.swf` mounted from `resource/flash` |

The port policy in `KISAK_PS4_PORT.md` (lines 787–807) is explicit: **PS4 keeps `IsPC()==1` and `IsGameConsole()==0`** so it can read Kisak's little-endian PC VPK/VTF/BSP, but it forces **PS3 presentation mode only inside `scaleformui`** by injecting `PlatformCode=2`. `IsPlatformConsoleUI()` is the new predicate for controller-first behavior; the legacy `IsGameConsole()` is deliberately left false.

The injection is confirmed live in `ps4/scaleform_gfx_manager.cpp:1972-1974`:
```cpp
Scaleform::GFx::Value platformCode;
platformCode.SetNumber( 2 ); // PS3/PS4 console ActionScript convention.
global.SetMember( "PlatformCode", platformCode );
```
plus `wantControllerShown=true` and the full Valve key table, all installed before the movie's first `Advance(0.0f)`. This is exactly what the PS3 root movies expect (`_global.IsPS3()` returns true → all console branches fire).

---

## 3. Boot sequence: Legals → StartScreen → MainMenu

| Stage | PS3 movie | PS3 driver (C++) | Kisak PS4 status |
|---|---|---|---|
| 1. Legals | `legals.swf` | `CCreateLegalAnimScaleform::CreateIntroMovie()` (`cstrike15basepanel.cpp:1441`) | **Implemented** — `kBootLegalsLoading/Playing`, `AnimationCompleted` callback, `GetRatingsBoardForLegals`→"ESRB", controller skip |
| 2. StartScreen | `startscreen.swf` | `CCreateStartScreenScaleform::LoadDialog()` (`createstartscreen_scaleform.cpp:46`) | **Implemented** — `kBootStartScreenLoading/Waiting`, `ShowStartLogo()`, controller-confirm `CompleteStartScreen()` |
| 3. MainMenu | `mainmenu.swf` | `CCreateMainMenuScreenScaleform::LoadDialog()` | **Implemented** — `kBootMainMenuLoading`, `MainMenu` element request, `OnReady` callback |

The PS4 manager (`scaleform_gfx_manager.cpp:974-981`) defines the exact state machine the port doc requires: `kBootLegalsLoading → kBootLegalsPlaying → kBootStartScreenLoading → kBootStartScreenWaiting → kBootMainMenuLoading → Ready`. Transitions fire only on real AS callbacks (`AnimationCompleted`, `OnReady`, controller confirm). The earlier "skip straight to MainMenu" path is now the dev-only fallback (`KISAK_PS4_SCALEFORM_DIRECT_MENU`).

**Gap:** The port doc notes Legals/StartScreen assets (JPEGs, `valve_logo_music.mp3`, vector logos) must be *mounted*, not committed. `ps4/prepare_scaleform_swf.py` and `scaleform_asset_path.cpp` exist for this, but the audio callback currently does a **silent fallback** (`kCallbackPlayAudio` just logs once) — `valve_logo_music.mp3` is not yet played.

---

## 4. Main menu top bar (PS3 `mainmenuselectpanel.swf`)

The PS3 main menu is a horizontal top bar (`MainMenuTopBar`) with dropdown sub-panels. From `DefineSprite_58_select-panel/frame_1/DoAction.as`:

| Top-bar button | Label token | Dropdown | Dropdown items (Type → `BasePanelRunCommand`) |
|---|---|---|---|
| **Play** | `#SFUI_MainMenu_PlayButton` | `PlayDropdown` (`DefineSprite_18`) | Custom→`OpenCreateMultiplayerGameDialog`, Friends:Competitive→`ShowJoinPartyUI:competitive`, Friends:Wingman→`ShowJoinPartyUI:scrimcomp2v2`, Browser→`OpenServerBrowser`, Offline→`OpenCreateSinglePlayerGameDialog`, Training→`LaunchTraining` |
| **Learn** | `#SFUI_MainMenu_LearnButton` | `LearnDropdown` | HowPlay→`OpenHowToPlayDialog`, TrainMaps→`OpenCreateTrainingGameDialog` (defined in `OpenMenu` switch) |
| **Inventory** | `#SFUI_MainMenu_Inventory` | (no dropdown) | `OnInventoryPressed()` → `InventoryPanel.InitInventoryPanelMaster()` |
| **Watch** | `#SFUI_MainMenu_Watch` | (no dropdown) | `OnWatchPressed()` → `WatchPanel.InitWatchPanel()` |
| **Awards** | `#SFUI_MainMenu_My_Awards` | `AwardsDropdown` (`DefineSprite_14`) | Stats→`OpenStatsDialog`, Medals→`OpenMedalsDialog`, Leaderboards→`OpenLeaderboardsDialog` |
| **Options** | `#SFUI_MainMenu_HelpButton` | `OptionsDropdown` (`DefineSprite_12`) | Controller→`OpenControllerDialog`, Mouse→`OpenMouseDialog`, Settings→`OpenSettingsDialog`, Video→`OpenVideoSettingsDialog`, Audio→`OpenAudioSettingsDialog`, Credits→`PlayCreditsVideo` |
| **Home** | — | (no dropdown) | `OnHomePressed()` → show floating panels |
| **Quit** | — | (no dropdown) | `OnQuitGamePressed()` → `Quit` |
| **Alert** | — | (no dropdown) | `OnAlertButtonPressed()` → survey/mapvote |

**Kisak PC counterpart:** `CCreateMainMenuScreenScaleform::LoadDialog()` requests the `MainMenu` element and exposes `BasePanelRunCommand`, `IsMultiplayerPrivilegeEnabled`, `LaunchTraining`, `ViewMapInWorkshop`, `GetPreviousLevel` to ActionScript. The same AS code runs unchanged on PS4 because the same `.gfx` movies are mounted.

**PS4 wiring status (`scaleform_gfx_manager.cpp`):**
- `BasePanelRunCommand` is intercepted and routed through `KisakPs4ScaleformMenuActionForCommand()`.
- **Only one command is currently wired to an action**: `OpenCreateSinglePlayerGameDialog` (and its `_AcceptNotConnectedToLive` variant) → `kKisakPs4ScaleformMenuActionStartSinglePlayer` → `"StartSinglePlayer"` element (`scaleform_menu_actions.h`).
- Every other `BasePanelRunCommand` is **logged but not actioned** (first 24 logged via `m_menuCommandsLogged`).
- `IsMultiplayerPrivilegeEnabled` hard-returns `true` (line 837), `GetPreviousLevel`→`-1`, `GetTrialTimeRemaining`→`-1` (full unlock).

**Gap (the big one):** Of the ~20 distinct `BasePanelRunCommand` strings the PS3 main menu can emit, only `OpenCreateSinglePlayerGameDialog` has a PS4 handler. `OpenOptionsDialog`, `OpenSettingsDialog`, `OpenControllerDialog`, `OpenAudioSettingsDialog`, `OpenVideoSettingsDialog`, `OpenMouseDialog`, `OpenHowToPlayDialog`, `OpenLeaderboardsDialog`, `OpenStatsDialog`, `OpenMedalsDialog`, `OpenServerBrowser`, `ShowJoinPartyUI`, `OpenCreateMultiplayerGameDialog`, `PlayCreditsVideo`, `Quit`, `ResumeGame`, `Disconnect`, `SwitchTeams`, `OpenCallVoteDialog`, `ShowInviteFriendsUI` all still need PS4 action handlers. Each currently falls through to the log-only path.

---

## 5. Options/Settings dialogs

PS3 presents options as a **single scrollable Scaleform `optionsmenu.swf`** with a widget-slot system (`DefineSprite_51`, 766 lines): `Init(nSlots, nMode)` attaches `OptionControlPanel` clips, a scrollbar, and a `NavigationMaster` with `PCButtons` (EditKey/ClearKey/UseDefaults/Back) and a `ControllerNavl` text field. The C++ side (`COptionsScaleform` in Kisak) drives it via `DialogType_e`:

| DialogType_e | PS3 OpenMenu type | Notes |
|---|---|---|
| `DIALOG_TYPE_MOUSE` / `KEYBOARD` | `Mouse` | Mouse/keyboard tab |
| `DIALOG_TYPE_CONTROLLER` | `Controller` | Controller mapping (uses `controllerdialog.res` / `l4d360ui_controlleroptions.res`) |
| `DIALOG_TYPE_SETTINGS` | `Settings` | Spray paint + general (uses `settingsdialog.res`) |
| `DIALOG_TYPE_MOTION_CONTROLLER` | — | PC-only, disabled |
| `DIALOG_TYPE_MOTION_CONTROLLER_MOVE` | — | **PS3-only** (Move) |
| `DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER` | — | **PS3-only** (Sharpshooter) |
| `DIALOG_TYPE_VIDEO` / `VIDEO_ADVANCED` | `Video` | |
| `DIALOG_TYPE_AUDIO` | `Audio` | Uses `l4d360ui_audio.res` |
| `DIALOG_TYPE_SCREENSIZE` | — | Screen-size calibration |

The PS3 pause menu's Help submenu (`pausemenu.swf` DefineSprite_27) branches on platform: `IsPS3()` adds **MotionControllerMove** and **MotionControllerSharpshooter** entries; `IsPC()` adds **Video** and **Audio**; Xbox gets a disabled MotionController placeholder. Because PS4 injects `PlatformCode=2`, the PS3 Move/Sharpshooter entries will appear in the PS4 pause menu's Help list — these are meaningless on DualShock 4 and should be filtered (either a new `IsPS4()` branch in the AS, or a PS4-specific options type that the C++ `COptionsScaleform` rejects).

**Kisak PC VGUI path** (`optionsdialog.cpp`) uses a tabbed `PropertyDialog` with six pages: Keyboard, Mouse, Audio, Video, Voice, Multiplayer. This is the **PC-only** path and is not what runs on PS4.

**Gap:** The Scaleform `optionsmenu.swf` widget system is mounted and the `COptionsScaleform` enum exists, but the PS4 `scaleform_gfx_manager` does not yet route `OpenOptionsDialog`/`OpenSettingsDialog`/etc. to actual `COptionsScaleform` instances. The PS3 Motion-Controller entries need PS4 gating.

---

## 6. Pause menu (`pausemenu.swf`)

PS3 pause menu (`DefineSprite_27`, 435 lines) builds its button list dynamically in `InitButtons()`:

| # | Label token | Condition | Action |
|---|---|---|---|
| 1 | `#SFUI_PauseMenu_ResumeGameButton` | always | `ResumeGame` |
| 2 | `#SFUI_PauseMenu_OpenLoadout` | purchased | `OnOpenLoadout` (inventory) |
| 3 | `#SFUI_PauseMenu_OpenLoadoutGift` | inv>0 && !loadout-allowed && MP | `OnOpenLoadoutGift` |
| 4 | `#SFUI_PauseMenu_SwitchTeamsButton` | !training && !queued && !gotv | `SwitchTeams` |
| 5 | `#SFUI_PauseMenu_CallVoteButton` | (!training && (MP‖PC)) && !gotv | `OpenCallVoteDialog` |
| 6 | `#SFUI_PlayMenu_BrowseServersButton` | !perfectworld && !queued && !gotv | `OpenServerBrowser` |
| 7 | `#SFUI_PauseMenu_InviteFriendsButton` / ReportServer | `NeedsInviteFriends()` ‖ MP | `ShowInviteFriendsUI` / `OpenPlayerDetailsPanel("ServerReport")` |
| 8 | `#SFUI_PlayMenu_OpenWorkshopMap` | workshop map | `ViewMapInWorkshop` |
| 9 | `#SFUI_MainMenu_My_Awards` | always | → Awards submenu (Stats/Medals/Leaderboards) |
| 10 | `#SFUI_PauseMenu_HelpButton` | always | → Help submenu (HowToPlay/Controls/Settings/MouseKeyboard/+PS3 Motion) |
| 11 | `#SFUI_PauseMenu_ExitGameButton` | always | `Disconnect` |

Navigation: 9-button circular `NavLayout` (UP/DOWN wrap), `CANCEL` and `KEY_XBUTTON_START` both resume. `ForceHighlightOnPop(true)` restores last highlight.

**Kisak PC:** The Scaleform pause menu is **stubbed** in `cstrike15basepanel.cpp:833-857` — `DismissPauseMenu`, `RestorePauseMenu`, `ShowScaleformPauseMenu` are all `/* Removed for partner depot */`. The VGUI `CBaseModPanel::DismissPauseMenu` just toggles the main menu. The **RocketUI** pause menu (`rkhud_pausemenu.cpp`) is the only live PS4 pause path: `OnOpenPauseMenu`→`ShowRocketPauseMenu(true)`→`RocketPauseMenuDocument::ShowPanel`.

**Gap:** If the goal is "PS3 UI on Kisak," the Scaleform `pausemenu.swf` is the authentic console pause menu, but its C++ driver is stubbed on the partner depot side. Either (a) re-implement `ShowScaleformPauseMenu`/`DismissPauseMenu` in `cstrike15basepanel.cpp` to request the `PauseMenu` Scaleform element, or (b) keep RocketUI as the pause host. Right now PS4 uses RocketUI for pause while using Scaleform for the main menu — a mismatch.

---

## 7. Team selection (`chooseteam.swf`) & class (`classmenu.res`)

PS3 `chooseteam.swf` (`frame_1/DoAction.as`, 206 lines) exposes `selectCounterTerrorists`, `selectTerrorists`, `ShowSpectatorButton`, `showPreMatchOverlay`, `setTeamsFull`, `SetBackgroundJpg`. **PS3 branch:** `IsPS3()` maps `KEY_XBUTTON_Y` to "Show Scoreboard" (per `SCALEFORM_UI_LAYOUT.md`).

Kisak Scaleform: `CCSTeamMenuScaleform` (`teammenu_scaleform.h`) exposes `OnOk/OnCancel/OnSpectate/OnAutoSelect/OnShowScoreboard/OnTeamHighlight/IsInitialTeamMenu/IsQueuedMatchmaking` — a clean match to the AS callbacks. `CChooseClassScaleform` mirrors it for class selection.

**Status:** Team/class Scaleform drivers exist and align with the PS3 AS interface. No PS4-specific gap beyond the general "menu commands not yet routed" issue.

---

## 8. Buy menu

| Path | Asset | Status |
|---|---|---|
| PS3 Scaleform | `buy-menu.swf` | Mounted; integrated into team menu flow |
| PC VGUI | `mainbuymenu.res` (CT-side categories: pistols/shotguns/submachineguns/rifles/machineguns/primaryammo/secammo/equipment + Autobuy/Rebuy) | Active on PC |
| PS4 RocketUI | `rkhud_buymenu.cpp` | Active fallback |

**Gap:** Same Scaleform-vs-RocketUI split as pause. The PS3 `buy-menu.swf` is the console-authentic path; RocketUI is the current PS4 fallback.

---

## 9. Scoreboard

PS3 `scoreboard.swf` gates auto-show on gamepad (`!IsPC()`). PC VGUI `scoreboard.res` is a `CClientScoreBoardDialog` with `ServerName` + `SectionedListPanel`. Kisak has both Scaleform (`OnShowScoreboard` in `teammenu_scaleform.cpp`) and RocketUI (`rkhud_scoreboard.cpp`) paths.

---

## 10. Navigation & input

**PS3 Scaleform** uses `Lib.NavLayout` + `Lib.NavManager` (in `sharedlib.swf`, 450 scripts):
- `AddTabOrder`, `AddNavForObject` with `{UP,DOWN}` maps, `AddCancelKeyHandlers`, `AddKeyHandlerTable`
- `Lib.NavLayout.AddDirectionKeyHandlersToObject` maps UP/DOWN/LEFT/RIGHT across **keyboard `KEY_*`, D-pad `KEY_XBUTTON_*`, and analog `KEY_XSTICK1_*`** simultaneously
- `ResizeManager` applies safezone scaling (convar `safezonex/y`, default 0.85)
- `SFKey` translates Valve key codes

**Kisak PC** has `vgui2/vgui_controls/ControllerMap.cpp` with the full `KEY_XBUTTON_*` table (A/B/X/Y, START/BACK, sticks, shoulders, triggers, D-pad) and `OnKeyCodeTyped` → command dispatch. **No `NavLayout`/`NavManager` equivalent in C++** — that logic lives inside the Scaleform AS, which is why mounting the `.gfx` movies is the path of least resistance.

**PS4** feeds `libScePad` → Source `InputEvent_t` → Scaleform (`scaleform_gfx_manager.cpp:1149` packs `m_nData2` into AS args; `KEY_XBUTTON_*` table at line 1085). The PS3 AS NavLayout then drives controller navigation natively.

**Status:** Controller input chain is wired end-to-end on PS4. The PS3 AS navigation grid works unmodified because `PlatformCode=2` + `wantControllerShown=true` are injected.

---

## 11. Platform branches in the PS3 movies

Six `IsPS3()` branches exist in the authentic movies (per `SCALEFORM_UI_LAYOUT.md`):

| Movie | Branch effect | PS4 desirability |
|---|---|---|
| `leaderboards.swf` | Hides "Show Profile" button, changes button layout | OK — PS4 has no Steam profile |
| `chooseteam.swf` | `KEY_XBUTTON_Y` → Show Scoreboard | OK — DualShock 4 has Y/triangle |
| `pausemenu.swf` | Adds Motion Controller Move + Sharpshooter settings | **Wrong on PS4** — no Move/Sharpshooter; needs gating |
| `medalstatsscreen.swf` | Changes medal display layout | OK |
| `hudvoicestatus.swf` | Local player voice handling | OK |
| `scoreboard.swf` | `!IsPC()` gates auto-show on gamepad | OK |

**The one actionable platform-branch gap:** `pausemenu.swf`'s Help submenu will render Move/Sharpshooter entries on PS4 because `IsPS3()` is true. Options: (a) patch the AS to also check a new `_global.IsPS4()` and skip those two entries, or (b) inject `PlatformCode=4` and add `IsPS4()` to the root movies, or (c) have the C++ `COptionsScaleform` reject `DIALOG_TYPE_MOTION_CONTROLLER_MOVE/SHARPSHOOTER` on PS4 so the dialog never opens. Option (c) is least invasive.

---

## 12. Summary of gaps for "PS3 UI on Kisak PS4"

| Area | Status | Gap |
|---|---|---|
| Scaleform boot (Legals→StartScreen→MainMenu) | **Done** | Legals audio silent; assets must be mounted not committed |
| `PlatformCode=2` + `wantControllerShown` injection | **Done** | — |
| Controller input (`libScePad`→Scaleform) | **Done** | — |
| Main menu top bar + dropdowns render | **Done** (AS runs unchanged) | — |
| `BasePanelRunCommand` routing | **1 of ~20 wired** | Only `OpenCreateSinglePlayerGameDialog` → action. Need handlers for Options/Settings/Controller/Audio/Video/Mouse/HowToPlay/Leaderboards/Stats/Medals/ServerBrowser/ShowJoinParty/Quit/Resume/Disconnect/SwitchTeams/CallVote/InviteFriends/Credits |
| Options dialogs (`optionsmenu.swf`) | C++ enum exists, AS mounted | PS4 manager doesn't yet request `Options`/`Settings`/etc. Scaleform elements |
| Pause menu | **Stubbed in Scaleform**; RocketUI active | Re-implement `ShowScaleformPauseMenu` or accept RocketUI/Scaleform split |
| Buy menu | Scaleform + RocketUI both present | Choose one; PS3 `buy-menu.swf` is console-authentic |
| Team/class selection | Scaleform drivers match AS | No PS4-specific gap |
| Scoreboard | Both paths present | No PS4-specific gap |
| PS3 Motion-Controller branches | Will appear on PS4 | Gate `DIALOG_TYPE_MOTION_CONTROLLER_MOVE/SHARPSHOOTER` in C++ or patch AS |
| Localization `[$PS3]` | Treated as true on PS4 | Confirmed correct per port doc |
| `IsGameConsole()` | Correctly false on PS4 | PS4 uses `IsPS4()`/`IsPlatformConsoleUI()` — do not flip `IsGameConsole()` |

**Bottom line:** The PS3 Scaleform UI is already running on Kisak PS4 at the movie/AS/navigation level — `PlatformCode=2` injection, the three-stage boot state machine, and the controller input chain are all implemented. The remaining work is **C++ action routing**: the main menu and pause menu emit `BasePanelRunCommand` strings that the PS4 `scaleform_gfx_manager` currently only logs. Wiring those ~20 commands to real `COptionsScaleform`/`CCreateMainMenuScreenScaleform`/pause handlers is the critical path to a fully functional PS3-style menu on Kisak. The Motion-Controller pause entries and the Scaleform-vs-RocketUI pause/buy split are secondary cleanups.
