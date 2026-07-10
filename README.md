# AutoMyAim

**v1.0.4**

A PoeFixer plugin for **Path of Exile 2** that moves your cursor onto the best nearby
monster so your skills aim where they should. Hold a key (or toggle it on) and it scans
enemies in range, checks **line-of-sight**, weights them by distance / rarity / HP, and
points the cursor at the top target. It only moves the cursor — **you still cast**.

Unofficial third-party game tool. Maintainer: Ömer Faruk ARPA.

![AutoMyAim](https://i.ibb.co/x8SBFHR1/fixer-FTV6f-Sp-Os2.jpg)

## Features

- **Smart target pick** — weights nearby hostile monsters by distance, rarity (prefer
  Magic/Rare/Unique) and optionally HP (prefer low or high), and aims at the best one.
- **Line-of-sight** — skips monsters behind walls using the terrain grid (toggle + a
  tunable threshold).
- **Aim key + toggle** — hold a key to aim, or toggle always-on. Bind any key or mouse
  button (click the button, press the key).
- **Cursor control** — optionally confine the cursor to a circle around your character,
  and add a little randomization.
- **Safety** — stops while an item panel (inventory/stash/vendor) is open. Only runs
  while the game is focused.
- **Plays nice with PickMyLoot** — pauses aiming while PickMyLoot is picking up loot, so
  the two never fight over the cursor.
- **Range circle** — optional ground ring at your aim range for easy calibration.
- **Target diagnostics (for reporting)** — an optional overlay that prints the current
  target's targetable flags and active buff names. If the cursor ever sticks to a monster
  that can't be killed (an invulnerable Delirium / Essence mob, a buff that makes it immortal
  for a few seconds), turn this on, aim at it, and send the readout so that exact case can be
  filtered.

## Install

1. Download `AutoMyAim.dll` from a release (or build it, see below).
2. Copy it to `…\fixer\Plugins\AutoMyAim\AutoMyAim.dll`.
3. Enable **AutoMyAim** in the PoeFixer plugin list, bind your aim key, and play. Run the
   game in fullscreen / borderless for correct cursor targeting.

## Build

Requires MSVC (v14x toolset, C++20), x64. From the plugin folder:

```
MSBuild.exe AutoMyAim.sln -p:Configuration=Release -p:Platform=x64 -m
```

Output: `bin\Release\AutoMyAim.dll`. The `sdk/`, `imgui/` and `third_party/` folders are
vendored so the plugin builds standalone.

## Notes

Line-of-sight uses the walkable terrain grid; if targets behind walls are kept or dropped
incorrectly, adjust **LoS block below** or turn line-of-sight off. Everything is optional
and configurable in the plugin settings.
