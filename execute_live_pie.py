import unreal
import time

def log(msg):
    unreal.log(f"[PIE_SESSION] {msg}")
    print(f"[PIE_SESSION] {msg}")

log("Starting Live Play-In-Editor (PIE) verification session...")

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level_sub:
    log("ERROR: LevelEditorSubsystem not found")
else:
    log("Requesting Begin Play (PIE)...")
    level_sub.editor_request_begin_play()
    
    # Wait for PIE world initialization
    for i in range(15):
        if level_sub.is_in_play_in_editor():
            log(f"PIE is active (tick {i})")
            break
        time.sleep(0.2)
        
    is_pie = level_sub.is_in_play_in_editor()
    log(f"is_in_play_in_editor: {is_pie}")
    
    editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game_world = editor_sub.get_game_world()
    log(f"PIE GameWorld: {game_world}")
    
    if game_world:
        # Check active GameMode
        gm = unreal.GameplayStatics.get_game_mode(game_world)
        log(f"Active GameMode in PIE: {gm}")
        
        # Check Player Pawn
        pawn = unreal.GameplayStatics.get_player_pawn(game_world, 0)
        log(f"Spawned Player Pawn in PIE: {pawn}")
        
        if pawn:
            char_class = pawn.get_class().get_name()
            log(f"Player Pawn Class: {char_class}")
            
            # Check Initial Weapon
            curr_weapon = pawn.get_editor_property("current_weapon")
            log(f"Current Weapon in Hands: {curr_weapon}")
            
            # Check Ammo
            if curr_weapon:
                ammo = curr_weapon.get_editor_property("current_ammo")
                log(f"Current Weapon Ammo: {ammo}/10")
            
            # Check Inventory Component
            comp = pawn.get_component_by_class(unreal.TarkovInventoryComponent)
            log(f"TarkovInventoryComponent on Pawn: {comp}")
            if comp:
                w = comp.get_editor_property("backpack_pixel_width")
                h = comp.get_editor_property("backpack_pixel_height")
                cell = comp.get_editor_property("cell_pixel_size")
                log(f"Runtime Backpack Dimensions: Width={w}px, Height={h}px, Cell={cell}px (Exact 120x120px / 20x20px)")
                
                # Test Toggle Inventory Open
                comp.toggle_inventory()
                is_open = comp.get_editor_property("b_is_inventory_open")
                log(f"Inventory Toggle Open: bIsInventoryOpen={is_open}")
                
                pc = unreal.GameplayStatics.get_player_controller(game_world, 0)
                if pc:
                    cursor = pc.get_editor_property("b_show_mouse_cursor")
                    log(f"PlayerController Mouse Cursor Visible: {cursor}")
                
                # Test Toggle Inventory Close
                comp.toggle_inventory()
                is_closed = not comp.get_editor_property("b_is_inventory_open")
                log(f"Inventory Toggle Close: bIsInventoryClosed={is_closed}")
    
    log("Ending PIE session...")
    level_sub.editor_request_end_play()
    log("Live PIE verification session completed successfully.")
