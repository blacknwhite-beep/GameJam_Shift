# Blueprint Developer Checklist & Creative Guide

**Owner**: Blueprint Developer (Your Teammate)  
**Location**: `Content/Dev_Blueprint/`  
**Goal**: Bring the game to life with level design, visual effects, sounds, crop blueprints, UI/HUD, and game feel!

---

## 📁 Your Workspace Folders

All your assets, blueprints, maps, and audio should live inside `Content/Dev_Blueprint/`:

```
Content/Dev_Blueprint/
├── Maps/          <── Your levels (Lvl_Farm.umap, Lvl_Combat.umap)
├── Blueprints/    <── Game Blueprints (BP_Character, BP_Weapons)
├── Crops/         <── Crop variants (BP_ShotgunCorn, BP_RifleWheat)
├── UI/            <── User interfaces (WBP_FarmingHUD, WBP_AmmoHUD)
├── VFX/           <── Particles, Niagara systems, dimension portals
└── Audio/         <── Gunshots, ambient music, crop pop sounds
```

---

## 📋 Master Task List

### Phase 1: The "Shift" Dimension Transition (Game Feel)
When the player presses Shift, C++ instantly teleports the character. **Your job is to make it look and feel amazing!**

- [ ] **1.1 Camera Shake**:
  - Right-click in `VFX/` -> **Blueprint Class** -> Select `CameraShakeBase`.
  - Name it `CS_DimensionShift`.
  - Add an oscillation: Pitch amplitude `5.0`, FOV kick `10.0` for duration `0.2s`.
- [ ] **1.2 Audio Stinger**:
  - Add a dimensional whoosh / electric crackle sound cue in `Audio/` (`Cue_DimensionShift`).
- [ ] **1.3 Post-Process Glitch Effect**:
  - Create a Post-Process Material (or Material Parameter Collection) with slight chromatic aberration / color pulse.
- [ ] **1.4 Connect in Blueprint**:
  - Open `BP_ShooterCharacter` (in `Blueprints/`).
  - Right-click in Event Graph -> Add node: **`Event On Dimension Shifted`** (provided by C++).
  - Connect:
    1. **Play World Camera Shake** -> select `CS_DimensionShift`.
    2. **Play Sound 2D** -> select `Cue_DimensionShift`.
    3. **Spawn System at Location** -> spawn portal sparkle Niagara at player feet.
    4. Set Post-Process Material weight from `1.0` -> timeline fade to `0.0` in 0.2s.

---

### Phase 2: Building the Farming World (`Lvl_Farm.umap`)
Build the tranquil, beautiful agricultural dimension.

- [ ] **2.1 Create Level**:
  - In `Maps/`, create a new level named `Lvl_Farm.umap`.
  - **Important**: This level will be placed at `Z = -20,000` inside `Lvl_MasterPersistent`.
- [ ] **2.2 Terrain & Environment**:
  - Sculpt green rolling hills, a dirt path, and a small river or pond.
  - Place wooden fences, hay bales, and a rustic farmhouse or barn.
- [ ] **2.3 Atmosphere & Lighting**:
  - Directional Light: Warm golden sunlight (warm yellow/orange).
  - Skylight: Soft blue sky.
  - Ambient Sound: Bird chirping, gentle wind, rustling leaves.
- [ ] **2.4 Soil Layout**:
  - Flatten a central plot area for tilled soil rows where `BP_FarmPlot` actors will sit.

---

### Phase 3: Creating Crop Blueprints
C++ gives you `FarmPlotBase`. You use it to make as many fun crops as you want!

- [ ] **3.1 Create "Shotgun Corn" (`BP_Crop_ShotgunCorn`)**:
  - In `Crops/`, right-click -> **Create Blueprint Class** -> Choose `FarmPlotBase` as parent.
  - In the Components panel, add 3 Static Mesh components:
    - `Mesh_Dirt`: Tilled soil mound.
    - `Mesh_Sprout`: Small green leaf sprout.
    - `Mesh_Mature`: Corn stalk with red shotgun shells for cobs!
  - In Details panel, tune settings:
    - `Stage Duration`: `10.0` seconds.
    - `Yield Item ID`: `"Item_Ammo_Shotgun"`.
    - `Yield Quantity`: `16`.
  - In Event Graph, implement:
    - **`Event On Stage Changed`**:
      - If `Empty` -> show only `Mesh_Dirt`.
      - If `Planted` or `Growing` -> show `Mesh_Sprout`.
      - If `Mature` -> show `Mesh_Mature` and spawn sparkle particle!
    - **`Event On Watered`**:
      - Change dirt material to a darker, wet soil texture.
      - Spawn water splash particle.
    - **`Event On Harvested`**:
      - Play pop sound effect.
      - Spawn floating text: `"+16 Shotgun Shells"`.
- [ ] **3.2 Create "Rifle Wheat" (`BP_Crop_RifleWheat`)**:
  - Same steps, but set `Yield Item ID` to `"Item_Ammo_Rifle"` and `Yield Quantity` to `30`.
- [ ] **3.3 Create "Medicinal Herb" (`BP_Crop_MedHerb`)**:
  - Same steps, but yields `"Item_Medkit"` to restore player health in combat!

---

### Phase 4: User Interface (UMG Widgets)
Build the player's on-screen HUDs in `UI/`.

- [x] **4.1 Farming HUD (`WBP_FarmingHUD`)**:
  - Add an icon displaying the currently selected tool: Hoe, Watering Can, or Harvester.
  - Add a seed pouch counter: e.g. `Corn Seeds: x4`, `Wheat Seeds: x2`.
  - Wire mouse scroll wheel to call C++ node **`Cycle Tool`**.
- [x] **4.2 Crop Hover Prompt (`WBP_CropPrompt`)**:
  - A small 3D widget placed above each farm plot.
  - If unwatered -> show blue water drop icon.
  - If mature -> show green `"Press [E] to Harvest"` prompt.
- [x] **4.3 HUD Swapping on Shift**:
  - In `BP_ShooterCharacter`:
    - When shifting to **Farming**: Show `WBP_FarmingHUD`, hide crosshair and weapon ammo counter.
    - When shifting to **Combat**: Show crosshair and weapon ammo counter, hide `WBP_FarmingHUD`.

---

### Phase 5: Combat World Level & Tuning (`Lvl_Combat.umap`)
Build the intense combat dimension at `Z = 0`.

- [ ] **5.1 Create Level**:
  - In `Maps/`, create `Lvl_Combat.umap` at coordinates `(0, 0, 0)`.
- [ ] **5.2 Environment & Obstacles**:
  - Place broken concrete walls, sandbags, metal barriers, and ruined buildings.
  - Align landmarks with the farm: e.g., the ruined fortress in combat sits right above the farm barn below!
- [ ] **5.3 Atmosphere**:
  - Dark overcast sky, combat smoke, red emergency lights, flickering lamps.
- [ ] **5.4 Enemies & Pickups**:
  - Place `BP_ShooterNPCSpawner` actors behind cover to spawn enemy waves.
  - Place weapon pickup crates containing pistols, shotguns, and seed pouches.

---

## 🎨 Creative Tips for Game Jam Success
1. **Juice Everything**: Add sounds to button presses, footsteps, crop harvesting, and gun reloads. Sound makes a game feel 10x more polished!
2. **Clear Colors**: Use high contrast between worlds — bright, warm, cozy colors for the Farm; desaturated, gritty, high-contrast colors for Combat.
3. **Keep It Simple**: Start with 2 crops (Ammo Corn & Health Herb) and get the complete loop working before making 10 crops.

---

## 🤝 Rules of Engagement with the C++ Developer
1. **Never edit files in `Source/`**: All C++ code lives in `Source/`. If you need a new variable, function, or event hook, ask the C++ dev to add it!
2. **Work inside `Content/Dev_Blueprint/`**: Keeping your assets in your folder guarantees you won't get Git conflicts with C++ test levels.
3. **Always `git pull` before starting work**: Ensures you have the latest compiled C++ code and nodes available in your editor.
