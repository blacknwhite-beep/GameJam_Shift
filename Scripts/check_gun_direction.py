import unreal

gs = unreal.EditorLevelLibrary
for g in ["M1911", "M1911Tactical", "MP5", "MP7", "M4A1", "M16", "SCAR"]:
    sm = unreal.load_asset(f"/Game/Dev_Blueprint/Weapons/Meshes/SM_Gun_{g}")
    if sm:
        bb = sm.get_bounds()
        # Use GeometryScript or vertex positions via StaticMeshDescription to check +Y vs -Y
        desc = sm.get_static_mesh_description(0)
        if desc:
            verts = desc.get_vertices()
            pos_y_z = []
            neg_y_z = []
            for v in verts:
                p = desc.get_vertex_position(v)
                if p.y > bb.origin.y + bb.box_extent.y * 0.5:
                    pos_y_z.append(p.z)
                elif p.y < bb.origin.y - bb.box_extent.y * 0.5:
                    neg_y_z.append(p.z)
            pos_span = (max(pos_y_z) - min(pos_y_z)) if pos_y_z else 0
            neg_span = (max(neg_y_z) - min(neg_y_z)) if neg_y_z else 0
            pos_min_z = min(pos_y_z) if pos_y_z else 0
            neg_min_z = min(neg_y_z) if neg_y_z else 0
            unreal.log_warning(f"[GUN_DIR] {g}: +Y_Z_span={pos_span:.1f} (minZ={pos_min_z:.1f}) | -Y_Z_span={neg_span:.1f} (minZ={neg_min_z:.1f})")
