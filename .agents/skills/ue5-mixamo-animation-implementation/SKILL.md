---
name: ue5-mixamo-animation-implementation
description: >-
  End-to-end procedure for importing and retargeting Mixamo FBX animations onto
  UE 5.7 skeletal meshes (such as SKM_Farmer_male), playing them without T-Pose
  or invisible-body bugs, and wiring synchronized Third-Person action cameras
  (modeled on the exact GameJam_Shift_2026 planting animation implementation).
  Use this skill whenever implementing a Mixamo animation, retargeting FBX clips
  in UE 5.7, fixing T-Pose/A-Pose or invisible Third-Person character meshes, or
  building action-synced Third-Person cameras.
---

# UE 5.7 Mixamo Animation & Third-Person Action Implementation

This skill captures the exact, verified pipeline used to implement `"Plant A Plant.fbx"` (`Anim_Farmer_Plant`) on `SKM_Farmer_male` in `GameJam_Shift_2026` (UE 5.7, commit `1538f30`).

For full copy-pasteable C++ reference snippets from `ShooterCharacter.cpp`, see [references/planting_implementation_reference.md](./references/planting_implementation_reference.md).
For the reusable Python FBX import & IK retargeting script, see [scripts/import_mixamo_animation_template.py](./scripts/import_mixamo_animation_template.py).

---

## Step 1: Import & Retarget Mixamo FBX onto the Target Character (Python)

1. **Never use `-run=pythonscript` with `IKRetargetBatchOperation.duplicate_and_retarget`**:
   - `DuplicateAndRetarget` invokes Slate `NotifyUserOfResults`, which asserts `CurrentApplication.IsValid()` under `-run=pythonscript`.
   - Always execute via **`-ExecutePythonScript`**:
     ```powershell
     & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
       "C:\UnrealProjects\GameJam_Shift_2026\GameJam_Shift_2026.uproject" `
       -ExecutePythonScript="C:\UnrealProjects\GameJam_Shift_2026\Scripts\import_plant_animation.py" `
       -nopause -unattended -nosound
     ```
2. **Legacy FBX import flags**:
   - Disable Interchange FBX before importing Mixamo FBX files:
     `Interchange.FeatureFlags.Import.FBX 0` and `Interchange.FeatureFlags.Import.Texture 0`.
3. **Build IK Rigs & Retargeter (`T-Pose -> A-Pose` alignment)**:
   - Import a Mixamo bind-pose mesh (`SKM_Mixamo_Source`) once, then import the Mixamo animation FBX (`import_mesh=False`, `import_animations=True`, `animation_length=FBXALIT_EXPORTED_TIME`) onto `SKM_Mixamo_Source_Skeleton`.
   - Build `IK_Mixamo_Source` and `IK_Farmer_Target` with `ctrl.apply_auto_generated_retarget_definition()`, then clear all chain IK goals (`ctrl.set_retarget_chain_goal(cname, "None")`) so retargeting uses clean aligned FK + Pelvis without unconfigured FBIK pinning.
   - Build `RTG_MixamoToFarmer` with:
     - `rctrl.remove_all_ops()` -> `rctrl.add_default_ops()`
     - `rctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)`
     - **`rctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)`** (aligns Mixamo T-Pose shoulders, arms, forearms, hands, legs, and all 10 fingers to the Farmer's A-Pose!)
     - `rctrl.snap_bone_to_ground("ball_l", unreal.RetargetSourceOrTarget.TARGET)`
   - Run `unreal.IKRetargetBatchOperation.duplicate_and_retarget(...)` and save the resulting `UAnimSequence` (e.g. `/Game/Dev_Blueprint/Characters/Farmer/Animations/Anim_Farmer_Plant`).
4. **Verify Component-Space Bone Poses Before Writing C++**:
   - Log Component-Space positions (`pelvis`, `head`, `hand_l`, `hand_r`, `foot_l`, `foot_r`) across timestamps (`0.0s..end`) using `AnimationLibrary.get_bone_pose_for_time(...)` composed up each chain.
   - Confirm arms/hands reach the exact target action zone (for `Anim_Farmer_Plant`: at `t = 3.0..6.0s`, `pelvis ≈ (0, -14, 43)`, `hand_l ≈ (+15.5, +43.6, 11.7)`, `hand_r ≈ (-8.1, +47.0, 12.8)` — both hands cup the soil `+45..+52 cm` forward!).

---

## Step 2: Runtime Skeletal Mesh Swap & Zero-T-Pose Playback (C++)

1. **Swap mesh with `bReinitPose = true`**:
   - Never use `SetSkeletalMeshAsset(NewMesh)` (it calls `SetSkeletalMesh(NewMesh, false)` and leaves stale pose/skeleton state after replacing `SKM_Manny_Simple` / `ABP_Unarmed`).
   - Always call:
     ```cpp
     BodyMesh->SetAnimInstanceClass(nullptr);
     BodyMesh->SetSkeletalMesh(FarmerMesh, true);
     BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
     BodyMesh->bEnableUpdateRateOptimizations = false;
     BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
     BodyMesh->SetAnimInstanceClass(UZombieAnimInstance::StaticClass());
     ```
2. **Drive sequences through `UZombieAnimInstance` (NOT `AnimationSingleNode`)**:
   - `UAnimSingleNodeInstance` silently no-ops into T-Pose if `SetAnimInstanceClass(nullptr)` is ever called while `AnimationMode == AnimationSingleNode`.
   - `UZombieAnimInstance` (`Variant_Shooter/AI/ZombieAnimInstance.h`) evaluates any `UAnimSequence` directly in `FAnimInstanceProxy::Evaluate()` via `ActiveSequence->GetAnimationPose(...)`. With `SetHeadLookAngles(false, false, 0.0f, 0.0f)`, it is 100% skeleton-agnostic and evaluates every frame even when `bRecentlyRendered == false`.
   - **Critical rules**:
     - Guard `HandleOnDimensionShifted` so `SetAnimInstanceClass(nullptr)` is **never** called on `GetMesh()` when `bUsingFarmerMesh == true`.
     - For one-shot animations (`bLooping = false`), call `PlayZombieSequence(Anim, false, PlayRate)` **once** when the action begins — do **not** re-call it every tick (or `SequenceTime` resets to `0`). On locomotion ticks, only update speed via `SetZombiePlayRate(PlayRate)`.

---

## Step 3: Fix Invisible Third-Person Mesh (`WorldSpaceRepresentation` Trap)

In UE 5.7 (`PrimitiveComponent.h`), `EFirstPersonPrimitiveType::WorldSpaceRepresentation` **implicitly forces `bOwnerNoSee = true` and `bCastHiddenShadow = false` inside the scene proxy**, overriding `SetOwnerNoSee(false)`. If left set during Third-Person, the character's 3D body is 100% invisible and only the shadow renders.

Always toggle visibility via a single helper (`ApplyFarmerViewVisibility`):

- **Third-Person (`bShowBodyToOwner = true`, e.g., Planting or `[V]` toggle)**:
  - `Body->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);`
  - `Body->SetOwnerNoSee(false);`
  - `Body->SetOnlyOwnerSee(false);`
  - `Body->SetHiddenInGame(false, false);`
  - `Body->SetVisibility(true, false);`
  - `Body->SetCastShadow(true);`
  - `Body->bCastHiddenShadow = true;`
  - `Body->UnHideBoneByName(FName("Head"));`
  - `Body->MarkRenderStateDirty();`
- **First-Person (`bShowBodyToOwner = false`)**:
  - `Body->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);`
  - `Body->SetOwnerNoSee(true);`
  - `Body->SetCastShadow(true);`
  - `Body->UnHideBoneByName(FName("Head"));`
  - `Body->MarkRenderStateDirty();`
- **Auxiliary character meshes (`FirstPersonMesh`, etc.)**:
  - Iterate `TInlineComponentArray<USkeletalMeshComponent*> CharSkelMeshes(this)` and disable any non-`BodyMesh` component owned by `this` using **`bPropagateToChildren = false`** (`SetVisibility(false, false)`, `SetHiddenInGame(true, false)`, `SetFirstPersonPrimitiveType(None)`, `SetCastShadow(false)`, `bCastHiddenShadow = false`, `SetAnimInstanceClass(nullptr)`). Never pass `bPropagateToChildren = true` on `FirstPersonMesh`, or you will hide `FirstPersonCameraComponent` children (`TacticalFlashlight`, FP weapons).

---

## Step 4: Synchronized Planting Positioning, Sprout Reveal & 3/4 Camera

1. **Action Start (`DoStartFiring`)**:
   - Lock `DoMove`, `DoAim`, and `DoJumpStart` while `ActionState.bIsPlanting == true`; call `GetCharacterMovement()->StopMovementImmediately()`.
   - Compute `DesiredKneelLoc = PlotLoc - ToPlotDir * 48.0f` (matching the verified `+45..+52 cm` forward hand reach of `Anim_Farmer_Plant`), floor-snapped via `ECC_WorldStatic`.
   - Hide the newly spawned `Mesh_Sprout` / `CropMesh` at `t = 0`, then reveal it at **`Progress >= 0.45f`** right between both hands.
   - Call `BeginPlantThirdPerson(...)` (forces Third-Person visibility and remembers prior view), then `PlayFarmerOneShotAnim(PlantAnim, 1.65f)`.
2. **Unobstructed Inside-the-Fence 3/4 Camera (`UpdateFarmerAnimation`)**:
   - Smoothly lerp actor location & yaw to `DesiredKneelLoc` / `PlantTargetYaw` over the first `0.30s`.
   - Anchor `KneelBase` at `PlantTargetActorLoc` XY so the tripod never slides during step-in, and focus on `ActionFocusPoint = KneelBase + Fwd * 24.0f + Z * (PelvisHeightAboveGround + 4.0f)`.
   - Test **4 closer inside-the-fence 3/4 candidates** (`TraceOrigin = KneelBase + Fwd * 24 + Z * 54`):
     - `0`: `TraceOrigin - Fwd * 110 + Right * 122 + Z * 62` (Rear-Right 3/4)
     - `1`: `TraceOrigin - Fwd * 110 - Right * 122 + Z * 62` (Rear-Left 3/4)
     - `2`: `TraceOrigin + Fwd * 92 + Right * 130 + Z * 60` (Front-Right 3/4)
     - `3`: `TraceOrigin + Fwd * 92 - Right * 130 + Z * 60` (Front-Left 3/4)
   - Sweep each candidate with a `15 cm` sphere (`SweepSingleByChannel`) on **`ECC_Visibility`** (not just `ECC_Camera`, so low wooden fences/rails are detected), ignoring `this` and all `AFarmPlotBase` actors (so plot `InteractionBox` colliders don't falsely trigger collision). Lock `ChosenCamSideIndex` for the duration of the clip.
   - Blend camera out over `0.28s` and back over `0.35s`; hide `"Head"` only during `Elapsed < 0.05f` and `Remaining < 0.08f` so the camera never clips the hat/face at the eye-transition endpoints.
   - On completion (or dimension shift), call `EndPlantThirdPerson` + `RestoreFarmerActionCamera` + `RevealPendingPlantSprout`.

---

## Step 5: Validation Checklist

1. Stop `UnrealEditor*` processes, compile with `Build.bat ... -WaitMutex`, confirm `Result: Succeeded`.
2. Launch PIE and check `Saved/Logs/GameJam_Shift_2026.log` for:
   `Equipped Animated Cubic Farmer (SKM_Farmer_male, AnimInst=ZombieAnimInstance_..., PlantAnim=Anim_Farmer_Plant)`.
3. Verify in PIE:
   - First-Person ground shadow animates `Idle` / `Walk` / `Run` cleanly (zero T-Pose).
   - Pressing **`[V]`** shows the full visible, animated 3D Farmer.
   - Planting on an empty plot smoothly pulls out inside the fence, frames the visible kneeling Farmer + both hands + soil plot, pops the sprout at ~45%, and smoothly returns to First-Person.
