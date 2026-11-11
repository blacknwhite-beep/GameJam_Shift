# GameJam_Shift_2026: Team Guide & Ownership

Welcome to **GameJam_Shift_2026**! This document defines the division of labor, folder structure, and Git rules for our two-person development team.

---

## 🎯 The Core Concept
- **World 1 (Combat / Guns)**: Fast-paced shooter combat, enemy encounters, Tarkov-style inventory, weapon loot. Located at `Z = 0`.
- **World 2 (Farming / Tranquil)**: Peaceful farming, planting seeds, watering crops, tranquil countryside. Located at `Z = -20,000`.
- **The Core Mechanic**: Pressing **Shift** instantly teleports the player between both realities!
- **The Synergy**: Crops grown on the farm yield ammo, medkits, and upgrades for combat; combat yields rare seeds and fertilizer.

---

## 👥 The Division of Labor

```
┌────────────────────────────────────────────────────────┐
│                      THE TEAM                          │
├──────────────────────────┬─────────────────────────────┤
│   C++ Developer (You)    │   Blueprint Developer       │
│                          │                             │
│   • Core Systems & Math  │   • Visuals, Audio & Polish │
│   • Dimension Teleport   │   • Level Design & Maps     │
│   • Soil State Machines  │   • UMG User Interfaces     │
│   • Inventory Integration│   • Crop & Weapon Variants  │
│   • Input Action Contexts│   • Camera Shakes & VFX     │
└──────────────────────────┴─────────────────────────────┘
```

---

## 📁 Workspace Folders (Zero Git Conflicts!)

To prevent binary `.uasset` / `.umap` merge conflicts, work in your designated folders:

```
Repository/
├── Source/GameJam_Shift_2026/   <── C++ Dev Territory (Source code)
│    ├── Dimension/              (Teleportation & subsystem)
│    ├── Farming/                (C++ crop base classes & tools)
│    └── InterWorld/             (Economy bridge)
│
├── Content/
│    ├── Dev_Blueprint/          <── Blueprint Dev Territory (Assets & levels)
│    │    ├── Maps/              (Lvl_Farm.umap, Lvl_Combat.umap)
│    │    ├── Blueprints/        (BP_Character, BP_Weapons)
│    │    ├── Crops/             (BP_ShotgunCorn, BP_RifleWheat)
│    │    ├── UI/                (WBP_FarmingHUD, WBP_AmmoHUD)
│    │    ├── VFX/               (Niagara particles, shift portals)
│    │    └── Audio/             (Sound cues)
│    │
│    └── Dev_CPP/                <── C++ Dev Test Assets (Dummy test maps)
│
└── Team/
     ├── README.md               (This overview file)
     ├── CPP_Dev/CHECKLIST.md    (C++ Dev task checklist)
     └── Blueprint_Dev/CHECKLIST.md (Blueprint Dev task checklist)
```

---

## 🚨 Git Golden Rules for Collaboration

1. **Pull Before Opening Unreal**: Always run `git pull` before launching Unreal Engine so your editor loads fresh assets.
2. **Never Edit the Same `.uasset` or `.umap` Simultaneously**: Binary files cannot be merged. If one person is working on `BP_ShooterCharacter`, notify the other on Discord.
3. **Sublevel Isolation**:
   - Combat world is authored in `Dev_Blueprint/Maps/Lvl_Combat.umap`.
   - Farm world is authored in `Dev_Blueprint/Maps/Lvl_Farm.umap`.
   - Both are loaded into `Lvl_MasterPersistent.umap`.
   - Never edit the other person's level file.
4. **C++ Exposes, Blueprint Implements**:
   - C++ developer creates base classes with `BlueprintCallable` functions and `BlueprintImplementableEvent` hooks.
   - Blueprint developer subclasses them and styles them in the editor.
