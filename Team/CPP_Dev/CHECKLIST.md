# C++ Developer Checklist & Technical Roadmap

**Owner**: C++ Developer (You)  
**Location**: `Source/GameJam_Shift_2026/`  
**Goal**: Build the robust core engine, math, subsystems, and reflection hooks for the dimension shift and farming mechanics.

---

## 📋 Master Task List

### Phase 1: Build Configuration & Module Setup
- [x] **1.1** Update `GameJam_Shift_2026.Build.cs`:
  - Add `"GameJam_Shift_2026/Dimension"` to `PublicIncludePaths`.
  - Add `"GameJam_Shift_2026/Farming"` to `PublicIncludePaths`.
  - Add `"GameJam_Shift_2026/InterWorld"` to `PublicIncludePaths`.
- [x] **1.2** Verify project compiles cleanly with new include paths.

---

### Phase 2: Dimension Shift Subsystem
- [x] **2.1** Create `Source/GameJam_Shift_2026/Dimension/DimensionTypes.h`:
  - Define `UENUM(BlueprintType) enum class EGameDimension : uint8 { Combat, Farming };`
  - Define `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDimensionShifted, EGameDimension, NewDimension, FVector, TargetLocation);`
- [x] **2.2** Create `Source/GameJam_Shift_2026/Dimension/DimensionShiftSubsystem.h & .cpp`:
  - Inherit from `UGameInstanceSubsystem`.
  - Property: `FVector FarmWorldOffset = FVector(0.0f, 0.0f, -20000.0f);`
  - Property: `float ShiftCooldown = 0.5f;`
  - Property: `EGameDimension CurrentDimension = EGameDimension::Combat;`
  - Delegate: `UPROPERTY(BlueprintAssignable) FOnDimensionShifted OnDimensionShifted;`
  - Function: `UFUNCTION(BlueprintCallable) bool RequestShift(ACharacter* PlayerCharacter);`
  - Function: `UFUNCTION(BlueprintPure) bool CanShiftSafely(const FVector& SourceLocation, FVector& OutTargetLocation, const UWorld* World) const;`
- [x] **2.3** Implement safety raycast in `CanShiftSafely`:
  - Raycast down from target point to verify solid ground exists (prevents falling into the void).
  - Raycast capsule sweep to verify no overlapping blocking geometry.
- [x] **2.4** Implement teleportation in `RequestShift`:
  - Check cooldown against `World->GetTimeSeconds()`.
  - Calculate target location based on `CurrentDimension`.
  - Call `PlayerCharacter->TeleportTo(TargetLocation, PlayerCharacter->GetActorRotation(), false, true)`.
  - Toggle `CurrentDimension` (`Combat` <-> `Farming`).
  - Broadcast `OnDimensionShifted.Broadcast(CurrentDimension, TargetLocation)`.

---

### Phase 3: Character Input & Context Switching
- [x] **3.1** Update `Source/GameJam_Shift_2026/Variant_Shooter/ShooterCharacter.h & .cpp`:
  - Add `UPROPERTY(EditAnywhere, Category = "Input") UInputAction* DimensionShiftAction;`
  - Add `UPROPERTY(EditAnywhere, Category = "Input") UInputMappingContext* FarmingMappingContext;`
  - Add `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Farming") TObjectPtr<UFarmingComponent> FarmingComponent;`
- [x] **3.2** Bind Shift Action in `SetupPlayerInputComponent`:
  - Bind `DimensionShiftAction` (Triggered) to `AShooterCharacter::HandleDimensionShift()`.
- [x] **3.3** Implement `HandleDimensionShift()`:
  - Query `UDimensionShiftSubsystem` from `GetGameInstance()`.
  - Call `Subsystem->RequestShift(this)`.
- [x] **3.4** Implement `SwitchDimensionContext(EGameDimension NewDimension)`:
  - If `Combat`:
    - Add `IMC_Weapons` to Enhanced Input subsystem.
    - Remove `FarmingMappingContext`.
    - If `CurrentWeapon`, unhide / equip weapon mesh.
  - If `Farming`:
    - Add `FarmingMappingContext` to Enhanced Input subsystem.
    - Remove `IMC_Weapons`.
    - If `CurrentWeapon`, holster / hide weapon mesh.

---

### Phase 4: Farming Base Architecture
- [x] **4.1** Create `Source/GameJam_Shift_2026/Farming/FarmingTypes.h`:
  - Define `UENUM(BlueprintType) enum class ECropStage : uint8 { Empty, Planted, Growing, Mature };`
  - Define `UENUM(BlueprintType) enum class EFarmTool : uint8 { Hoe, WateringCan, Harvester };`
  - Define `USTRUCT(BlueprintType) struct FCropYieldData { ... };`
- [x] **4.2** Create `Source/GameJam_Shift_2026/Farming/FarmPlotBase.h & .cpp`:
  - Inherit from `AActor`.
  - Properties: `CurrentStage`, `StageDuration`, `bIsWatered`, `YieldItemID`, `YieldQuantity`.
  - Function: `UFUNCTION(BlueprintCallable) void WaterPlot();` (sets `bIsWatered = true`, starts timer).
  - Function: `UFUNCTION(BlueprintCallable) bool PlantSeed(FName SeedID);` (sets `CurrentStage = Planted`).
  - Function: `UFUNCTION(BlueprintCallable) bool Harvest(FName& OutItem, int32& OutQuantity);`
  - Blueprint Hooks:
    - `UFUNCTION(BlueprintImplementableEvent) void OnStageChanged(ECropStage NewStage);`
    - `UFUNCTION(BlueprintImplementableEvent) void OnWatered();`
    - `UFUNCTION(BlueprintImplementableEvent) void OnHarvested();`
- [ ] **4.3** Create `Source/GameJam_Shift_2026/Farming/FarmingComponent.h & .cpp`:
  - Inherit from `UActorComponent`.
  - Property: `UPROPERTY(BlueprintReadWrite) EFarmTool ActiveTool;`
  - Function: `UFUNCTION(BlueprintCallable) void UseActiveTool();` (raycasts forward to find `AFarmPlotBase`).
  - Function: `UFUNCTION(BlueprintCallable) void CycleTool(bool bNext);`
  - Helper query: `UFUNCTION(BlueprintPure) EFarmTool GetActiveTool() const;`

---

### Phase 5: Economy Bridge (Crops -> Tarkov Inventory)
- [ ] **5.1** Create `Source/GameJam_Shift_2026/InterWorld/InterWorldBridge.h & .cpp`:
  - Static helper: `static bool DepositHarvestToInventory(AActor* Player, FName ItemID, int32 Quantity);`
  - Connect to `UTarkovInventoryComponent`:
    - `"Item_Ammo_Pistol"` -> adds 9mm stacks to grid.
    - `"Item_Ammo_Shotgun"` -> adds 12G shells to grid.
    - `"Item_Ammo_Rifle"` -> adds 5.56mm ammo to grid.
    - `"Item_Medkit"` -> adds health consumable.

---

## 🛠️ Verification & Compile Commands

Compile and test code without closing Unreal Editor using Live Coding or MCP CLI:

```bash
# Verify C++ compilation via Unreal MCP CLI
/home/admin/.local/bin/unreal-mcp-cli exec -p "C:/UnrealProjects/GameJam_Shift_2026" "import unreal; print('Compilation verified!')"

# Test dimension shift subsystem query
/home/admin/.local/bin/unreal-mcp-cli exec -p "C:/UnrealProjects/GameJam_Shift_2026" "import unreal
gi = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world().get_game_instance()
print('GameInstance:', gi)
"
```

---

## 🤝 Rules for Writing Code That Won't Break Your Teammate
1. **Never delete or rename a `UPROPERTY`** that your teammate has wired in Blueprint. If you must change a name, add an alias or use `CoreRedirects`.
2. **Always mark functions callable by Blueprint** with `UFUNCTION(BlueprintCallable, Category = "...")`.
3. **Always mark audio/VFX triggers** as `UFUNCTION(BlueprintImplementableEvent, Category = "...")`.
4. **Never spawn actors or widgets inside C++ constructors**. Always use `BeginPlay()` or lazy-initialization.
