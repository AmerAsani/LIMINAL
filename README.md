# LIMINAL 🟨🚪

**LIMINAL** is a Backrooms-style first-person exploration game set in an endless procedurally generated world.

Explore office corridors, concrete halls, underground tunnels, endless hotel hallways, giant empty rooms, and narrow maintenance passages. Manage your **Health and Sanity**, search for rare supplies, read the notes other wanderers left behind, and find the **emergency exits** that lead somewhere else. Just don't trust everything you see when your Sanity runs low.

LIMINAL started as my very first game project in **Python** and evolved into a completely custom **C++ game with its own renderer and engine**.

## ⬇️ Download

Ready to play, no installation: download a ZIP, unzip it and start `LIMINAL.exe`.

| Version | Download |
|---|---|
| **V7 (latest)** | [`downloads/LIMINAL_V7.zip`](downloads/LIMINAL_V7.zip) |
| V6 | [`downloads/LIMINAL_V6.zip`](downloads/LIMINAL_V6.zip) |
| V5 | [`downloads/LIMINAL_V5.zip`](downloads/LIMINAL_V5.zip) |
| V1–V4 (Python / terminal) | `LIMINAL V1.zip` … `LIMINAL V4.zip` |

The source code of every version is in the folders `LIMINAL V1` … `LIMINAL V7`. To build V5–V7 yourself, see [Build It Yourself](#build-it-yourself).

---

# Version History

## LIMINAL V1

The beginning of LIMINAL.

* Added the **Core Engine**
* Added the first playable version of the game
* Added the first rendering system
* Fully rendered the game directly in the **CMD terminal**
* Added the basic gameplay loop

V1 was the foundation of the entire project.

---

# LIMINAL V2

The first major expansion of the game.

* Added an **in-game menu**
* Added a **minimap**
* Fixed several small rendering bugs
* Removed `Q` and `E` inputs
* Replaced them with **mouse controls**
* Improved the overall game structure and controls

---

# LIMINAL V3

V3 introduced the main survival systems and a completely new camera system.

## Camera & Rendering

* Added a **true first-person camera perspective**
* The world is rendered as a panoramic view first
* The panorama is then reprojected into a correctly tilted camera view for every pixel
* Walls and pillars remain correctly proportioned when looking up or down
* Vertical edges converge correctly, similar to a real camera
* Maximum deviation from an ideal camera is approximately **1 pixel**
* Vertical camera movement of up to approximately **52°** in both directions

### Projection

With a wide field of view above 90°, the `Auto` projection can use **Panini projection** to reduce extreme stretching near the edges.

Available projection modes:

* `Auto`
* `Pinhole`
* `Panini`

The image proportions can also be adjusted to compensate for the character height of the terminal.

## Items

Added two rare items:

### Energy Bar

* **+20% movement speed** for 60 seconds
* **+20% Sanity**

### Almond Water

* **+35% Sanity**

Items do not randomly spawn on the floor.

Instead, they can be found inside small **supply rooms**. Each supply room contains a red glowing vending machine whose light also illuminates the nearby corridor.

* Approximately **1 in every 20 rooms** is a supply room
* Around **4 items per 96 × 96 m** on average

On the map (`M`):

* Vending machines are marked **magenta**
* Items are marked **yellow**

When you get close to a supply room containing items, a message appears:

> “Ganz in der Nähe summt ein Automat …”

Collected items remain permanently removed, even after loading a save.

## Inventory

Added a Minecraft-style inventory system.

### Hotbar

* 9 slots
* Located at the bottom of the screen

### Inventory

* 27 additional slots
* Opened with `Tab`

Items of the same type can be stacked.

## Health & Sanity

Added the **Health** and **Sanity** systems.

* Sanity decreases by **15% per minute**
* At **0% Sanity**, the screen begins turning red around the edges
* A subtle heartbeat effect starts
* A death countdown appears
* After **15 seconds**, the player dies

After death, the player can load the last save or start a new world.

Loading the previous save restores:

* **Full Health**
* At least **30% Sanity**

## Autosave

Added the first save system.

The game automatically saves:

* Every **3 minutes of playtime**
* When exiting the game

The following data is saved:

* World Seed
* Position
* Looking direction
* Health
* Sanity
* Active effects
* Inventory
* Collected items
* Distance travelled
* Playtime

The game first writes to a temporary file and then replaces the previous save.

The previous save is kept as a `.bak` backup.

If a save file becomes corrupted, the backup can automatically be loaded.

---

# LIMINAL V4

V4 focused on menus, difficulty settings, multiple saves, and usability.

## Main Menu

Added a complete main menu:

* New Game
* Continue Game
* Settings
* Exit

While the menu is open, the game world slowly rotates in the background.

## New Game

When creating a new game, you can enter a name and choose a difficulty.

### Easy

* Sanity: **−10%/min**
* **25 seconds** until death at 0% Sanity
* Significantly more items

### Medium

* Sanity: **−15%/min**
* **15 seconds** until death
* Normal item amount

### Hard

* Sanity: **−22%/min**
* **10 seconds** until death
* Fewer items

Health regeneration also depends on the selected difficulty.

## Continue Game

The Continue Game menu displays all available save files with:

* Preview image
* Save name
* Difficulty
* Date
* Playtime
* Player statistics

Press `Enter` to load a save.

Press `Delete` twice to remove a save.

Existing **V3 save files** can automatically appear in the save list.

## Pause Menu

Press `ESC` to open the pause menu.

Added:

* Save Game
* Graphics & Display
* Return to Main Menu

Autosave continues to use the same save file.

## Fullscreen

When the game starts, the console window can automatically:

* Maximize
* Enter fullscreen mode similar to `Alt + Enter`

Both options can be disabled under **Graphics & Display**.

## Command Line Options

Start a new world directly:

```bash
LIMINAL.exe --new
```

Start a specific world using a seed:

```bash
LIMINAL.exe --seed 4242
```

Start directly with a seed, name, and difficulty without opening the main menu:

```bash
LIMINAL.exe --seed N --name X --difficulty hard
```

---

# BIG UPDATE — LIMINAL V5 🚨

## Completely Rewritten in C++

V5 is the biggest technical change in LIMINAL so far.

The entire game was **rewritten from Python to C++20** and now runs on its own engine with a **Direct3D 11 renderer** instead of the terminal.

The world itself stayed the same: world generation was ported **bit-for-bit** from V4, so the same seed creates exactly the same rooms. **V4 and V3 saves can be continued in V5.**

### Graphics

* Deferred renderer with physically based materials (**PBR**)
* Tiled lighting with hundreds of lamps at once; every lamp is a real area light
* **Soft shadows for every lamp**: no light leaks through walls, stairs cast correct shadows
* Ambient occlusion (SSAO), reflections on glossy floors (SSR) and volumetric light glow in the haze
* Bloom, automatic exposure, AgX tone mapping, film grain, vignette and chromatic aberration
* Temporal anti-aliasing (TAA)
* Procedural PBR textures up to **2048 × 2048** (wallpaper, carpet, tiles, concrete, bricks, panels, metal grates)
* The old terminal look is still there: press `V` to switch between **Modern, Terminal, ASCII and Monochrome**
* Quality presets **Low / Medium / High / Ultra**, plus every option individually adjustable

### Performance

* About **2.3 ms per frame (~440 FPS)** at 1920 × 1080 with all effects enabled (RTX 3070)
* World streaming runs on multiple CPU cores
* No Python runtime anymore; a native, statically linked executable

### New Content

* **New level: Level 1 — "Das Tiefgeschoss"**: basement corridors, concrete cellars and a technical floor. Darker, narrower, with more sunken rooms and fewer items. Choose the level under *New Game*.
* Dripping water echoes through the basement rooms
* Levels, zones, room types, lighting styles, items, difficulties, key bindings and entities are defined in JSON files under `data/`, so **new levels can be added without changing the engine**

### Sound

* All sounds are generated procedurally: humming fluorescent lights, vending machines, footsteps for each floor type, heartbeat and ambience for each zone
* Positional stereo sound with low latency (WASAPI)

### Controls & Comfort

* **Gamepad support** (XInput)
* Freely rebindable keys (`data/input.json`)
* The game pauses automatically when you switch windows (`Alt + Tab`)
* Built-in help (`H` / `F1`)

### Saves

* Multiple save slots with preview images
* Crash-safe saving (temporary file + `.bak` backup)
* V4 and V3 saves appear in the list and can be continued; the old files are never changed

### Launcher

`LIMINAL.exe` checks your PC before the game starts:

* Windows version
* Direct3D 11 graphics support (software mode as a fallback)
* Microsoft C runtime
* Completeness of the game folder

If something is missing, it explains what to do and, where possible, offers the **official Microsoft installer**. It only downloads from Microsoft, verifies the digital signature and installs nothing without asking you first.

If the game ever crashes, a crash report is saved to `%LOCALAPPDATA%\LIMINAL\crash`.

## V5 Fixes

* Menus opened with `ESC`, `Tab` or `H` no longer close again immediately
* Fixed flickering, blocky artifacts on ceilings and walls in very large halls
* Softer, more even lighting in tall halls (no harsh bright spots on the walls)
* No leftover geometry or shadows from the previous world after loading a save
* `Alt + Enter` now stays in sync with the fullscreen setting
* `ESC` in the main menu no longer quits instantly; press it twice to exit
* Player names with accented letters (ä, é, ç, ñ, ł …) are displayed correctly
* Two games started within the same second no longer overwrite each other's save
* The gamepad `Start` button now also closes the pause menu

---

# LIMINAL V6 — Bigger, Deeper, Louder 🔊

V6 kept the same engine, but almost every system was extended or rebuilt.

## A Bigger, More Open World

The V5 world often consisted of many very small rooms packed tightly together. New games now use a much more spacious layout:

* **Bigger rooms**, wider and taller corridors, far fewer tiny rooms
* **Passages** along the main axes: colonnades with alcoves, tall halls with scattered pillars, or wide plain corridors
* **Open transitions**: corridors flow into each other, large halls open wide towards the corridor
* **Light courts** 10–18 m high, with more haze in large areas, so it is often unclear how far a space really goes

| Measured (Level 0, 5 seeds × 25 sectors) | V5 | V6 |
|---|---|---|
| Rooms per hectare | 99 | 51 |
| Median room size | 36 m² | 80 m² |
| Floor area in tiny rooms (< 40 m²) | 15 % | 2.5 % |
| Spacious floor area (≥ 3 m from a wall) | 33 % | 48 % |
| Longest line of sight (average) | 20 m | 30 m |

Old saves keep their **bit-identical world**. The new layout is a data overlay in the level files.

## Spatial Sound

* **Every fluorescent lamp is its own 3D sound source**: the 100 Hz hum of a magnetic ballast with overtones, a fine hiss and slow fluctuations. Industrial lamps hum deeper, emergency lights rasp, flickering tubes click in sync with the picture.
* **Room acoustics**: the reverb time is calculated from the room's volume, surface area and materials. Carpet absorbs, concrete and tiles echo.
* **Spatial hearing**: time difference between the ears, head shadow for sounds behind you, air absorption, and walls that make sounds quieter and duller
* **Distant sounds**: rare, matching the zone (a door, metal, rumbling, knocking, steam, creaking). They come more often as your Sanity drops.

## New Graphics Generation

* **GTAO** ambient occlusion instead of SSAO
* **Indirect light** (one bounce in screen space)
* **Volumetric light shafts with shadows** through doors and between pillars, with drifting haze
* **Contact shadows** for items, your body and fine edges
* **Parallax occlusion mapping**: real depth in tile joints, mortar, grates and ceiling grids
* **Wear and weathering**: water stains, dirt along the walls, dust in corners, scratches, soot and damp carpet
* Fixed streaks in floor reflections, new color grading with cool shadows and warm lights
* A believable **vending machine**: lit sign, LED display, keypad, coin slot, scratches and rust. Behind the glass front are shelves of bottles, cans and snacks with real depth.

## Performance That Adapts to Your PC

V6 is not tuned for one specific computer. It adapts to the hardware it runs on:

* **Automatic quality**: the game estimates the GPU, then measures the real GPU time in the main menu and picks the highest preset that keeps 60 FPS. A new GPU or resolution triggers a new measurement.
* **Laptops with two GPUs** now use the powerful graphics card instead of the integrated one
* **Dynamic resolution** and new options for lighting quality, particles, post-processing and window size
* **Texture compression** (BC1/BC5): 183 MB instead of 333 MB of video memory with 1024 textures

| GPU time per frame, 1920 × 1080 | Low | Medium | High | Ultra |
|---|---|---|---|---|
| NVIDIA GeForce RTX 5070 Laptop | – | – | 4.4 ms | 7.6 ms |
| Intel Arc 140T (integrated) | 4.1 ms | 9.5 ms | 11.4 ms | – |

Ultra renders internally at 2400 × 1350.

## Gameplay

* **Your own body**: an animated stick figure in first person. It walks, sprints, jumps, falls and lands, and its hands reach for items. Press `F5` for a shoulder camera.
* **Round minimap** that only shows what you have actually explored, plus a big map (`M`). Explored areas are saved.
* **Jumping** onto counters, crates and platforms
* **Stamina**: sprinting drains it, and when it is empty you are out of breath
* **Drop items** with `Q`. They stay on the floor and are saved.
* **Energy Bars** are only about a third as common, but stronger: **+30% Sanity**, **+30% speed** for 75 seconds, full stamina and cheaper sprinting

---

# LIMINAL V7 — A Way Out 🚪

Until V6, the world was endless and aimless. V7 gives exploring a goal: there is a way out, or at least a way somewhere else. You also find things along the way.

> „Es gibt Ausgänge. Grüne Schilder, NOTAUSGANG. Aber nie in der Nähe des Anfangs.“
> — a note found on the floor

## Flashlight 🔦

* Press `T` to switch it on. Your stick figure holds it in the **left hand**.
* The beam is a real light: a soft spotlight with its own **shadows** and a **visible beam in the haze**
* A full battery lasts about **5 minutes**. A battery indicator appears next to the hotbar.
* Below 15%, the light starts to **flicker**
* **Batteries** are rare, most often found in dark rooms. Using one recharges **50%**.

## Emergency Exits & Level 2 — "Das Hotel"

* Far from the start, steel doors with a glowing green **NOTAUSGANG** sign appear in the walls. The sign casts green light on the walls and floor around it.
* Exits you have seen are marked **green** on the minimap and the big map. If an exit is outside the map, a marker on the edge shows its direction.
* Press `E` to open the door. The screen fades to black, and you arrive on the **next level**:

  **Level 0** (offices) → **Level 1** (basement) → **Level 2 "Das Hotel"** → back to **Level 0** (a new world)

* Your inventory, Health, Sanity, flashlight, notebook and run log come with you
* **Level 2 — "Das Hotel"**:
  * Endless carpeted hallways with wallpaper and warm wall lamps that don't hum
  * Hotel rooms, suites, ballrooms, lobbies and dining halls
  * A staff area with a laundry and storage rooms, plus stairwells and rooms with ceilings that are far too high
  * Faint, warped **elevator music** plays somewhere behind the walls. In the staff area, a washing machine rumbles.

## Notes & Notebook 📝

* Sometimes a **note** lies on the floor, left by another wanderer. Press `E` to pick it up and read it.
* **36 different notes**, some only appear on certain levels. You mostly find notes you haven't read yet.
* All notes you have read are collected in the **notebook** (`ESC` → Notizbuch)

## Hallucinations 👁️

The lower your Sanity, the less you can trust what you see and hear:

| Sanity | What happens |
|---|---|
| below 55% | **Footsteps behind you**, in the same rhythm as your own. Turn around, and they stop. |
| below 35% | **Blackout**: the lights in your room twitch, go dark for a few seconds and stutter back on. Their humming falls silent too. |
| below 25% | **"Der Andere"**: after a blackout, a figure stands at the end of the hallway. A stick figure like you, only taller, and it is looking at you. Get closer or stare too long, and the lights flicker. It's gone. |

Hallucinations are purely atmospheric and never hurt you. They can be turned off under **Graphics & Display → Hallucinations**.

## Run Log 📖

The pause menu and the death screen show your run:

* Playtime and distance travelled
* Rooms entered
* Levels visited and emergency exits used
* Notes found
* Hallucinations experienced

The run log is saved and travels with you to the next level.

## Under the Hood

* Notes, batteries and exits are created only from the room data, without using the world generator's random numbers. **Old saves (V3–V6) keep their bit-identical world**, and every find is in the same place after loading.
* Everything is still procedural: the flashlight, battery, notes, exit door and sign are modeled and textured in code, and all new sounds are synthesized (flashlight click, paper, panic bar, power failure, elevator music).
* V6 saves appear in the list and can be continued in V7. V6 settings are imported on the first start.

---

## Controls (V7)

| Input | Action |
|---|---|
| `W A S D` | Move |
| Mouse / Arrow keys | Look around |
| `Shift` | Sprint (uses stamina) |
| `Space` | Jump |
| `+` / `-` | Walking speed |
| `E` | Pick up item, read note, open emergency exit |
| `T` | Flashlight on / off |
| `F` | Use selected item |
| `Q` / `Ctrl + Q` | Drop selected item / whole stack |
| `1–9` / Mouse wheel | Select hotbar slot |
| `Tab` | Inventory |
| `M` | Big map (the minimap is always visible) |
| `F5` | First person / shoulder camera |
| `ESC` | Menu (notebook, run log, settings, help, save) |
| `H` / `F1` | Help |
| `F3` / `I` | Debug overlay |
| `V` | Display mode (Modern / Terminal / ASCII / Monochrome) |
| `L` | Performance mode |
| `B` | Head bobbing |
| `P` / `F12` | Screenshot |
| `Alt + Enter` | Fullscreen / window |

**Gamepad (XInput)**

| Input | Action |
|---|---|
| Left stick | Move |
| Right stick | Look around |
| `L3` | Sprint |
| `B` | Jump |
| `A` | Pick up / read / open |
| `X` | Use item |
| D-pad up | Flashlight |
| D-pad down | Drop item |
| `Y` | Inventory |
| `LB` / `RB` | Hotbar |
| `R3` | First person / shoulder camera |
| `Start` | Menu |
| `Back` | Map |

All keys can be changed in `data/input.json`.

---

## Requirements

* Windows 10 or 11 (64-bit)
* Graphics card with Direct3D 11 and an up-to-date driver
* No Python, no extra DLLs, no installation: unzip and run `LIMINAL.exe`

---

## Command Line Options

Start a new game directly with a seed, name, difficulty and level:

```bash
LIMINAL.exe --seed 4242 --name X --difficulty hard --level level2
```

| Option | Effect |
|---|---|
| `--windowed --size 1600x900` | Window mode and size |
| `--quality 0..3` | Fixed quality preset for this start (turns off automatic quality) |
| `--gpu sparsam` | On laptops with two GPUs, use the power-saving GPU instead of the faster one |
| `--mode terminal` / `ascii` / `mono` | Start in a retro display mode |
| `--set key=value` | Change a setting for this start only, e.g. `--set hallucinations=false` |
| `--warp` | Software rendering (without a graphics card) |
| `--benchmark 3000` | Automatic camera run, result written to the log |

---

# ⚠️ Antivirus / Security Warnings

Because LIMINAL is a **native C++ application** and the executables are currently **not digitally signed with a code-signing certificate**, some antivirus software or Windows security features may show warnings or potentially flag the `.exe`.

* **Windows SmartScreen** may show "Windows protected your PC". Click **More info → Run anyway**.
* **Smart App Control** (Windows 11) can block unsigned programs while it is turned on.

This can happen with smaller or independently developed native applications, especially when they are unsigned.

LIMINAL is **fully open source**.

The complete source code can be inspected at any time, allowing anyone to review what the program actually does.

You can also compile the game yourself from the source code instead of using the provided executable.

**Don't blindly trust the executable. Inspect the source code and verify it yourself.** 👍

## Build It Yourself

No Visual Studio required. The build script downloads a portable toolchain (llvm-mingw / Clang, CMake, Ninja) into the project folder; nothing is installed on your system.

```bash
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

To create the ready-to-play release folder and ZIP:

```bash
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

---

# From V1 to V7

**V1**: Core engine and first terminal renderer
**V2**: Menu, minimap, mouse controls and bug fixes
**V3**: Camera system, projections, items, inventory, Health, Sanity and saving
**V4**: Main menu, difficulty system, multiple saves, fullscreen and improved save management
**V5**: Complete rewrite in C++20, custom engine, Direct3D 11 renderer with PBR graphics, a new level, procedural sound and gamepad support
**V6**: A bigger, more open world, spatial sound, a new graphics generation that adapts to your PC, a stick-figure body, an exploration minimap, jumping and stamina
**V7**: Flashlight, emergency exits between levels, Level 2 "Das Hotel", notes and a notebook, hallucinations and a run log

From a small Python experiment in the terminal...

**to a fully custom C++ game.**

🟨🚪 **Welcome to LIMINAL.**

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/7ebd8269-a8c6-4494-910d-01c45929833e" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d8a26f14-0d9b-4a21-b5e1-5d009923790b" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d9c81ddd-e7e6-4dd3-84ee-536294332f7e" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/0cb8ff9e-7d24-480b-b829-ffc714aeaceb" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/b358c8d6-7611-474a-9f7a-1ea2302ad7fb" />
