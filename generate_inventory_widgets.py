"""
Unreal Engine Python Script: Tarkov-Style Inventory UMG Widget Generation
===========================================================================

This script generates UMG widgets for the Tarkov-style grid inventory system.

Widgets Created:
- WBP_TarkovInventory: Main inventory overlay (dark tactical styling)
- WBP_BackpackGrid: 6x6 grid (120x120px total, 20x20px per cell)
- WBP_EquippedSlot: Dedicated weapon slot
- WBP_InventoryItem: 20x20px item widget with drag/drop support

Requirements:
- Unreal Engine 5.7 with Python scripting enabled
- Run from the Unreal Editor Scripting Console

Usage:
1. Open Unreal Editor for GameJam_Shift_2026
2. Window → Developer Tools → Output Log → Python tab
3. Paste this script and press Enter
   OR
   File → Run Python Script... and select this file
"""

import unreal

# =============================================================================
# Configuration Constants
# =============================================================================

# Grid dimensions
GRID_COLUMNS = 6
GRID_ROWS = 6
CELL_SIZE = 20.0  # pixels per cell
TOTAL_GRID_SIZE = GRID_COLUMNS * CELL_SIZE  # 120x120px

# Colors (Slate Gray tactical theme)
COLOR_BACKGROUND = unreal.LinearColor(0.102, 0.114, 0.125, 1.0)  # #1A1D20
COLOR_CELL_EMPTY = unreal.LinearColor(0.15, 0.15, 0.15, 1.0)
COLOR_CELL_HOVER_VALID = unreal.LinearColor(0.2, 0.4, 0.2, 1.0)  # Green
COLOR_CELL_HOVER_INVALID = unreal.LinearColor(0.4, 0.2, 0.2, 1.0)  # Red
COLOR_BORDER = unreal.LinearColor(0.3, 0.3, 0.3, 1.0)
COLOR_TEXT = unreal.LinearColor(0.9, 0.9, 0.9, 1.0)

# Font
FONT_SIZE_TITLE = 14
FONT_SIZE_NORMAL = 10

# Widget paths
UI_PACKAGE_PATH = "/Game/UI"


# =============================================================================
# Helper Functions
# =============================================================================

def create_package(package_path):
    """Ensure a package path exists."""
    try:
        package = unreal.load_asset(package_path)
        if package:
            return package
    except:
        pass
    
    # Create the package
    package_name = package_path.split("/")[-1]
    package_dir = "/".join(package_path.split("/")[:-1])
    
    if package_dir:
        factory = unreal.PackageFactory()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        try:
            package = asset_tools.create_asset(
                asset_name=package_name,
                package_path=package_dir,
                asset_class=unreal.Package,
                factory=factory
            )
            print(f"  ✓ Created package: {package_path}")
            return package
        except Exception as e:
            print(f"  ℹ Package creation skipped (may already exist): {e}")
    
    return None


def create_widget_blueprint(widget_name, parent_class=unreal.UserWidget):
    """Create a new Widget Blueprint asset."""
    full_path = f"{UI_PACKAGE_PATH}/{widget_name}"
    
    # Check if already exists
    existing = unreal.load_asset(full_path)
    if existing:
        print(f"  ⊘ Widget exists: {widget_name}")
        return existing
    
    # Create factory
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    
    # Create the widget blueprint
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    try:
        widget_bp = asset_tools.create_asset(
            asset_name=widget_name,
            package_path=UI_PACKAGE_PATH,
            asset_class=unreal.WidgetBlueprint,
            factory=factory
        )
        print(f"  ✓ Created widget: {widget_name}")
        return widget_bp
    except Exception as e:
        print(f"  ✗ Error creating {widget_name}: {e}")
        return None


def create_color_brush(color):
    """Create a SlateColorBrush from LinearColor."""
    return unreal.SlateColor(
        specified_color=color,
        color_use_mode=unreal.SlateColorStylingMode.USE_COLOR_SPECIFIED
    )


def create_margin(value=0):
    """Create an FMargin."""
    return unreal.Margin(left=value, top=value, right=value, bottom=value)


# =============================================================================
# Widget: WBP_InventoryItem (20x20px Item Widget)
# =============================================================================

def create_inventory_item_widget():
    """Create the WBP_InventoryItem widget - represents a single inventory item."""
    print("\nCreating WBP_InventoryItem...")
    
    widget_bp = create_widget_blueprint("WBP_InventoryItem")
    if not widget_bp:
        return None
    
    # Get the widget class generated from this blueprint
    widget_class = widget_bp.generated_class
    widget_tree = widget_bp.widget_tree
    
    # Create root border (handles drag visual)
    root_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    root_border.set_editor_property("name", "RootBorder")
    root_border.set_editor_property("width_override", 20.0)
    root_border.set_editor_property("height_override", 20.0)
    root_border.set_editor_property("brush", unreal.SlateBrush())
    root_border.set_editor_property("brush_color", COLOR_CELL_EMPTY)
    
    # Set as root
    widget_tree.root_widget = root_border
    
    # Add image for item icon (centered)
    icon_image = unreal.WidgetLibrary.add_image(widget_tree, unreal.Image, root_border)
    icon_image.set_editor_property("name", "ItemIcon")
    icon_image.set_editor_property("width_override", 16.0)
    icon_image.set_editor_property("height_override", 16.0)
    icon_image.set_editor_property("brush", unreal.SlateBrush())
    
    # Add hover/valid-invalid indicator border (thin, inside)
    indicator_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    indicator_border.set_editor_property("name", "IndicatorBorder")
    indicator_border.set_editor_property("brush_color", unreal.LinearColor(0, 0, 0, 0))
    indicator_border.set_editor_property("brush_thickness", unreal.Margin(1, 1, 1, 1))
    
    # Add to vertical box under root
    v_box = unreal.WidgetLibrary.add_vertical_box(widget_tree, unreal.VerticalBox, root_border)
    unreal.WidgetLibrary.add_child(v_box, icon_image)
    unreal.WidgetLibrary.add_child(v_box, indicator_border)
    
    # Save the widget
    unreal.EditorAssetLibrary.save_loaded_asset(widget_bp)
    
    print(f"  ✓ WBP_InventoryItem created (20x20px)")
    return widget_bp


# =============================================================================
# Widget: WBP_EquippedSlot (Weapon Slot)
# =============================================================================

def create_equipped_slot_widget():
    """Create the WBP_EquippedSlot widget - shows equipped weapon."""
    print("\nCreating WBP_EquippedSlot...")
    
    widget_bp = create_widget_blueprint("WBP_EquippedSlot")
    if not widget_bp:
        return None
    
    widget_tree = widget_bp.widget_tree
    
    # Create root border (larger slot for weapon)
    root_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    root_border.set_editor_property("name", "RootBorder")
    root_border.set_editor_property("width_override", 64.0)
    root_border.set_editor_property("height_override", 64.0)
    root_border.set_editor_property("brush_color", COLOR_BACKGROUND)
    root_border.set_editor_property("brush_thickness", unreal.Margin(2, 2, 2, 2))
    root_border.set_editor_property("border_background_color", COLOR_BORDER)
    
    widget_tree.root_widget = root_border
    
    # Add text label
    label = unreal.WidgetLibrary.add_text_block(widget_tree, unreal.TextBlock, root_border)
    label.set_editor_property("name", "SlotLabel")
    label.set_editor_property("text", unreal.Text.from_string("EQUIPPED"))
    label.set_editor_property("font_size", 8)
    label.set_editor_property("color_and_opacity", COLOR_TEXT)
    label.set_editor_property("justification", unreal.TextJustify.CENTER)
    
    # Add image for weapon icon
    weapon_icon = unreal.WidgetLibrary.add_image(widget_tree, unreal.Image, root_border)
    weapon_icon.set_editor_property("name", "WeaponIcon")
    weapon_icon.set_editor_property("width_override", 48.0)
    weapon_icon.set_editor_property("height_override", 48.0)
    
    # Add to layout
    v_box = unreal.WidgetLibrary.add_vertical_box(widget_tree, unreal.VerticalBox, root_border)
    unreal.WidgetLibrary.add_child(v_box, label)
    unreal.WidgetLibrary.add_child(v_box, weapon_icon)
    
    unreal.EditorAssetLibrary.save_loaded_asset(widget_bp)
    
    print(f"  ✓ WBP_EquippedSlot created (64x64px)")
    return widget_bp


# =============================================================================
# Widget: WBP_BackpackGrid (6x6 Grid, 120x120px)
# =============================================================================

def create_backpack_grid_widget():
    """Create the WBP_BackpackGrid widget - the main inventory grid."""
    print("\nCreating WBP_BackpackGrid...")
    
    widget_bp = create_widget_blueprint("WBP_BackpackGrid")
    if not widget_bp:
        return None
    
    widget_tree = widget_bp.widget_tree
    
    # Create root border (the grid container)
    root_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    root_border.set_editor_property("name", "GridBorder")
    root_border.set_editor_property("width_override", TOTAL_GRID_SIZE)  # 120px
    root_border.set_editor_property("height_override", TOTAL_GRID_SIZE)  # 120px
    root_border.set_editor_property("brush_color", COLOR_BACKGROUND)
    root_border.set_editor_property("border_background_color", COLOR_BORDER)
    root_border.set_editor_property("brush_thickness", unreal.Margin(1, 1, 1, 1))
    
    widget_tree.root_widget = root_border
    
    # Create uniform grid panel (6x6 cells)
    grid_panel = unreal.WidgetLibrary.add_uniform_grid_panel(widget_tree, unreal.UniformGridPanel, root_border)
    grid_panel.set_editor_property("name", "CellGrid")
    grid_panel.set_editor_property("slot_padding", unreal.Margin(0, 0, 0, 0))
    
    # Add 36 cells (6x6)
    cells = []
    for row in range(GRID_ROWS):
        for col in range(GRID_COLUMNS):
            cell_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
            cell_border.set_editor_property("name", f"Cell_{row}_{col}")
            cell_border.set_editor_property("width_override", CELL_SIZE)  # 20px
            cell_border.set_editor_property("height_override", CELL_SIZE)  # 20px
            cell_border.set_editor_property("brush_color", COLOR_CELL_EMPTY)
            
            # Add to grid
            grid_slot = unreal.WidgetLibrary.add_child_to_uniform_grid(grid_panel, cell_border)
            grid_slot.set_editor_property("row", row)
            grid_slot.set_editor_property("column", col)
            
            cells.append(cell_border)
    
    unreal.EditorAssetLibrary.save_loaded_asset(widget_bp)
    
    print(f"  ✓ WBP_BackpackGrid created ({int(TOTAL_GRID_SIZE)}x{int(TOTAL_GRID_SIZE)}px, {GRID_COLUMNS}x{GRID_ROWS} cells)")
    return widget_bp


# =============================================================================
# Widget: WBP_TarkovInventory (Main Overlay)
# =============================================================================

def create_tarkov_inventory_widget():
    """Create the WBP_TarkovInventory main overlay widget."""
    print("\nCreating WBP_TarkovInventory...")
    
    widget_bp = create_widget_blueprint("WBP_TarkovInventory")
    if not widget_bp:
        return None
    
    widget_tree = widget_bp.widget_tree
    
    # Create root overlay
    root_overlay = unreal.WidgetLibrary.add_overlay(widget_tree, unreal.Overlay)
    widget_tree.root_widget = root_overlay
    
    # Background (full screen, semi-transparent dark)
    bg_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    bg_border.set_editor_property("name", "Background")
    bg_border.set_editor_property("brush_color", unreal.LinearColor(0, 0, 0, 0.7))
    
    overlay_slot = unreal.WidgetLibrary.add_child_to_overlay(root_overlay, bg_border)
    overlay_slot.set_editor_property("horizontal_alignment", unreal.HorizontalAlignment.H_ALIGN_FILL)
    overlay_slot.set_editor_property("vertical_alignment", unreal.VerticalAlignment.V_ALIGN_FILL)
    
    # Main content box (centered)
    main_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    main_border.set_editor_property("name", "MainPanel")
    main_border.set_editor_property("width_override", 400.0)
    main_border.set_editor_property("height_override", 300.0)
    main_border.set_editor_property("brush_color", COLOR_BACKGROUND)
    main_border.set_editor_property("border_background_color", COLOR_BORDER)
    main_border.set_editor_property("brush_thickness", unreal.Margin(2, 2, 2, 2))
    
    overlay_slot = unreal.WidgetLibrary.add_child_to_overlay(root_overlay, main_border)
    overlay_slot.set_editor_property("horizontal_alignment", unreal.HorizontalAlignment.H_ALIGN_CENTER)
    overlay_slot.set_editor_property("vertical_alignment", unreal.VerticalAlignment.V_ALIGN_CENTER)
    
    # Vertical layout inside main panel
    v_box = unreal.WidgetLibrary.add_vertical_box(widget_tree, unreal.VerticalBox, main_border)
    
    # Title bar
    title_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    title_border.set_editor_property("name", "TitleBar")
    title_border.set_editor_property("height_override", 30.0)
    title_border.set_editor_property("brush_color", unreal.LinearColor(0.2, 0.2, 0.2, 1.0))
    
    title_text = unreal.WidgetLibrary.add_text_block(widget_tree, unreal.TextBlock, title_border)
    title_text.set_editor_property("name", "TitleText")
    title_text.set_editor_property("text", unreal.Text.from_string("INVENTORY / BACKPACK"))
    title_text.set_editor_property("font_size", FONT_SIZE_TITLE)
    title_text.set_editor_property("color_and_opacity", COLOR_TEXT)
    title_text.set_editor_property("justification", unreal.TextJustify.CENTER)
    
    unreal.WidgetLibrary.add_child(v_box, title_border)
    
    # Content area (horizontal: equipped slot + grid)
    content_h_box = unreal.WidgetLibrary.add_horizontal_box(widget_tree, unreal.HorizontalBox, v_box)
    
    # Equipped slot section
    equipped_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    equipped_border.set_editor_property("name", "EquippedSection")
    equipped_border.set_editor_property("width_override", 100.0)
    equipped_border.set_editor_property("padding", create_margin(10))
    
    equipped_label = unreal.WidgetLibrary.add_text_block(widget_tree, unreal.TextBlock, equipped_border)
    equipped_label.set_editor_property("text", unreal.Text.from_string("EQUIPPED"))
    equipped_label.set_editor_property("font_size", FONT_SIZE_NORMAL)
    equipped_label.set_editor_property("color_and_opacity", COLOR_TEXT)
    
    unreal.WidgetLibrary.add_child_to_horizontal_box(content_h_box, equipped_border)
    
    # Grid section
    grid_border = unreal.WidgetLibrary.add_border(widget_tree, unreal.Border)
    grid_border.set_editor_property("name", "GridSection")
    grid_border.set_editor_property("padding", create_margin(10))
    
    grid_label = unreal.WidgetLibrary.add_text_block(widget_tree, unreal.TextBlock, grid_border)
    grid_label.set_editor_property("text", unreal.Text.from_string("BACKPACK"))
    grid_label.set_editor_property("font_size", FONT_SIZE_NORMAL)
    grid_label.set_editor_property("color_and_opacity", COLOR_TEXT)
    
    unreal.WidgetLibrary.add_child_to_horizontal_box(content_h_box, grid_border)
    
    unreal.EditorAssetLibrary.save_loaded_asset(widget_bp)
    
    print(f"  ✓ WBP_TarkovInventory created (400x300px main panel)")
    return widget_bp


# =============================================================================
# Drag Drop Operation Class
# =============================================================================

def create_drag_drop_operation():
    """Create a Blueprint for the drag-drop operation."""
    print("\nCreating BPD_DragDropItem...")
    
    full_path = f"{UI_PACKAGE_PATH}/BPD_DragDropItem"
    
    # Check if exists
    existing = unreal.load_asset(full_path)
    if existing:
        print(f"  ⊘ DragDrop exists: BPD_DragDropItem")
        return existing
    
    # Create factory for blueprint
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.DragDropOperation)
    
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    try:
        bp = asset_tools.create_asset(
            asset_name="BPD_DragDropItem",
            package_path=UI_PACKAGE_PATH,
            asset_class=unreal.Blueprint,
            factory=factory
        )
        
        # Add variables to the blueprint
        # (This would normally be done in the blueprint editor)
        
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        
        print(f"  ✓ BPD_DragDropItem created")
        return bp
    except Exception as e:
        print(f"  ℹ DragDrop creation skipped: {e}")
        return None


# =============================================================================
# Main Execution
# =============================================================================

def main():
    """Generate all inventory widgets."""
    print("=" * 60)
    print("Tarkov Inventory Widget Generator")
    print("=" * 60)
    print(f"\nConfiguration:")
    print(f"  Grid: {GRID_COLUMNS}x{GRID_ROWS} cells")
    print(f"  Cell Size: {CELL_SIZE}x{CELL_SIZE}px")
    print(f"  Total Grid: {int(TOTAL_GRID_SIZE)}x{int(TOTAL_GRID_SIZE)}px")
    print(f"  Output Package: {UI_PACKAGE_PATH}")
    
    # Ensure package exists
    create_package(UI_PACKAGE_PATH)
    
    # Create all widgets
    widgets = {}
    
    widgets["InventoryItem"] = create_inventory_item_widget()
    widgets["EquippedSlot"] = create_equipped_slot_widget()
    widgets["BackpackGrid"] = create_backpack_grid_widget()
    widgets["TarkovInventory"] = create_tarkov_inventory_widget()
    widgets["DragDrop"] = create_drag_drop_operation()
    
    # Summary
    print("\n" + "=" * 60)
    print("Generation Summary")
    print("=" * 60)
    
    created_count = 0
    for name, widget in widgets.items():
        if widget:
            print(f"  ✓ {name}")
            created_count += 1
        else:
            print(f"  ✗ {name} (failed)")
    
    print(f"\nTotal: {created_count}/{len(widgets)} widgets created")
    print("=" * 60)
    
    if created_count == len(widgets):
        print("\n✓ All widgets generated successfully!")
        print("\nNext steps:")
        print("1. Open each widget in the UMG Editor")
        print("2. Bind events for drag/drop functionality")
        print("3. Connect to UTarkovInventoryComponent")
    else:
        print("\n⚠ Some widgets failed. Check errors above.")
    
    return created_count == len(widgets)


if __name__ == "__main__":
    main()