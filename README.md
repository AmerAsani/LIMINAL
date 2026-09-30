# LIMINAL 🟨🚪

**LIMINAL** is a Backrooms-style first-person exploration game set in an endless procedurally generated world.

Explore office corridors, concrete halls, underground tunnels, giant empty rooms, and narrow maintenance passages. Manage your **Health, Energy, and Sanity**, search for rare supplies, and try to survive the endless world.

LIMINAL started as my very first game project in **Python** and evolved into a completely custom **C++ game with its own renderer and engine**.

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

---

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

---

## Inventory

Added a Minecraft-style inventory system.

### Hotbar

* 9 slots
* Located at the bottom of the screen

### Inventory

* 27 additional slots
* Opened with `Tab`

Items of the same type can be stacked.

---

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

---

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

---

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

---

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

---

## Pause Menu

Press `ESC` to open the pause menu.

Added:

* Save Game
* Graphics & Display
* Return to Main Menu

Autosave continues to use the same save file.

---

## Fullscreen

When the game starts, the console window can automatically:

* Maximize
* Enter fullscreen mode similar to `Alt + Enter`

Both options can be disabled under **Graphics & Display**.

---

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

The entire game was **rewritten from Python to C++**.

This means V5 is no longer based on the old Python runtime or rendering system.

### V5 includes:

* Complete rewrite from **Python to C++**
* Completely new **C++ codebase**
* Fully custom **renderer**
* Custom **game engine**
* Completely remade graphics
* Completely redesigned UI
* Improved rendering performance
* Improved stability
* New rendering architecture
* No Python installation required

The old Python-based terminal renderer has been completely replaced.

**LIMINAL V5 is now a native C++ game with its own engine and renderer.**

---

# ⚠️ Antivirus / Security Warnings

Because LIMINAL V5 is a **native C++ application** and the executable is currently **not digitally signed with a code-signing certificate**, some antivirus software or Windows security features may show warnings or potentially flag the `.exe`.

This can happen with smaller or independently developed native applications, especially when they are unsigned.

LIMINAL is **fully open source**.

The complete source code can be inspected at any time, allowing anyone to review what the program actually does.

You can also compile the game yourself from the source code instead of using the provided executable.

**Don't blindly trust the executable — inspect the source code and verify it yourself.** 👍

---

# From V1 to V5

**V1** — Core engine and first terminal renderer
**V2** — Menu, minimap, mouse controls and bug fixes
**V3** — Camera system, projections, items, inventory, Health, Sanity and saving
**V4** — Main menu, difficulty system, multiple saves, fullscreen and improved save management
**V5** — Complete rewrite in C++, custom engine, custom renderer and completely remade graphics

From a small Python experiment in the terminal...

**to a fully custom C++ game.**

🟨🚪 **Welcome to LIMINAL.**

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/7ebd8269-a8c6-4494-910d-01c45929833e" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d8a26f14-0d9b-4a21-b5e1-5d009923790b" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d9c81ddd-e7e6-4dd3-84ee-536294332f7e" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/0cb8ff9e-7d24-480b-b829-ffc714aeaceb" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/b358c8d6-7611-474a-9f7a-1ea2302ad7fb" />


