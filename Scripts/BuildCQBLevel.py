# Builds the CQB sample level: two rooms, a choke point door, and a flanking loop.
#
# Run from the editor:      Tools > Execute Python Script...
# Run from the command line:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="Scripts/BuildCQBLevel.py"
#
# Layout seen from above. X runs left to right, Y runs bottom to top. 100 uu = 1 m.
#
#                    +---------------+       +---------------+
#                    |               |       |               |
#   +------+         |    ROOM A     |       |    ROOM B     |
#   |SPAWN |=========|   8m x 8m     |=======|   8m x 8m     |
#   +------+ corridor|   2 covers    |       | 3 covers, 3AI |
#                    |               |       |               |
#                    +----+     +----+       +----+     +----+
#                         |     |                 |     |
#                         |     +--- flank corridor ----+
#                         +---------------------------+
#
# The flank corridor is the point of the level: it gives the Flanker role somewhere to go,
# so the AI comes around into room A behind the player instead of walking in frontally.

import unreal

LEVEL_PATH = "/Game/CQB/Lvl_CQB"

CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
MATERIAL_PATH = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"

WALL_HEIGHT = 320.0        # 3.2 m ceiling feel
WALL_THICKNESS = 40.0
FLOOR_THICKNESS = 20.0

editor_actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

cube_mesh = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
basic_material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)


# ---------------------------------------------------------------- primitives

def spawn_box(name, center_x, center_y, center_z, size_x, size_y, size_z):
    """One engine cube scaled to the given size, centred on the given point."""
    actor = editor_actor.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(center_x, center_y, center_z), unreal.Rotator(0, 0, 0))
    if actor is None:
        unreal.log_error("failed to spawn " + name)
        return None

    actor.set_actor_label(name)
    actor.set_actor_scale3d(unreal.Vector(size_x / 100.0, size_y / 100.0, size_z / 100.0))

    component = actor.static_mesh_component
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    component.set_static_mesh(cube_mesh)
    if basic_material:
        component.set_material(0, basic_material)

    return actor


def solid_spans(span_min, span_max, holes):
    """The parts of a wall that stay solid once the doorways are cut out of it."""
    spans = []
    cursor = span_min

    for hole_min, hole_max in sorted(holes):
        if hole_min > cursor:
            spans.append((cursor, hole_min))
        cursor = max(cursor, hole_max)

    if cursor < span_max:
        spans.append((cursor, span_max))

    return spans


def build_region(prefix, min_x, max_x, min_y, max_y, walls, floor=True, floor_drop=0.0, ceiling=True):
    """
    Floor slab plus the requested walls for one rectangular region.

    walls maps a side to the list of openings on it:
      "N" and "S" take openings as X ranges, "W" and "E" as Y ranges.
    A side left out of the dict gets no wall at all, which is how regions join up.

    Floor slabs run under the walls, so neighbouring regions overlap by the wall thickness.
    floor_drop sinks a slab by a couple of centimetres to keep those overlaps from z fighting;
    the resulting step is far below the character step height and cannot be felt.
    """
    t = WALL_THICKNESS

    if floor:
        spawn_box(prefix + "_Floor",
                  (min_x + max_x) * 0.5, (min_y + max_y) * 0.5, -FLOOR_THICKNESS * 0.5 - floor_drop,
                  (max_x - min_x) + t * 2, (max_y - min_y) + t * 2, FLOOR_THICKNESS)

    if ceiling:
        # Rooms read as interiors rather than open pits, and it keeps the AI debug text from
        # being lost against the sky. Sits just above the walls.
        spawn_box(prefix + "_Ceiling",
                  (min_x + max_x) * 0.5, (min_y + max_y) * 0.5, WALL_HEIGHT + FLOOR_THICKNESS * 0.5 + floor_drop,
                  (max_x - min_x) + t * 2, (max_y - min_y) + t * 2, FLOOR_THICKNESS)

    for side, holes in walls.items():
        if side in ("N", "S"):
            wall_y = (max_y + t * 0.5) if side == "N" else (min_y - t * 0.5)
            # run a little past the corners so they close up
            for index, (x0, x1) in enumerate(solid_spans(min_x - t, max_x + t, holes)):
                spawn_box("%s_Wall_%s%d" % (prefix, side, index),
                          (x0 + x1) * 0.5, wall_y, WALL_HEIGHT * 0.5,
                          x1 - x0, t, WALL_HEIGHT)
        else:
            wall_x = (max_x + t * 0.5) if side == "E" else (min_x - t * 0.5)
            for index, (y0, y1) in enumerate(solid_spans(min_y, max_y, holes)):
                spawn_box("%s_Wall_%s%d" % (prefix, side, index),
                          wall_x, (y0 + y1) * 0.5, WALL_HEIGHT * 0.5,
                          t, y1 - y0, WALL_HEIGHT)


# ---------------------------------------------------------------- the level

def build_geometry():
    # The openings are deliberately staggered: the entry corridor meets room A on the south
    # side while the door to room B sits on the north side. Without that offset there is a
    # straight sight line from the spawn closet into room B and the enemies open fire before
    # the player has even entered, which kills the corner clearing the level is built around.

    # spawn closet, so the player walks in rather than starting in the fight
    build_region("Spawn", -1400.0, -1000.0, -450.0, -50.0,
                 {"N": [], "S": [], "W": [], "E": [(-350.0, -150.0)]})

    # approach corridor. Open at both ends, so only the side walls are built.
    build_region("Entry", -1000.0, -800.0, -350.0, -150.0,
                 {"N": [], "S": []}, floor_drop=2.0)

    # room A, where the player pie slices the corner before pushing the door
    build_region("RoomA", -800.0, 0.0, -400.0, 400.0,
                 {"N": [],
                  "S": [(-700.0, -500.0)],          # flank corridor comes up here
                  "W": [(-350.0, -150.0)],          # from the entry corridor
                  "E": [(150.0, 350.0)]})           # door to room B, offset to the north

    # the door itself: a 2 m hole in the wall, 2 m deep. The choke point.
    build_region("Door", 0.0, 200.0, 150.0, 350.0,
                 {"N": [], "S": []}, floor_drop=2.0)

    # room B, held by the enemies
    build_region("RoomB", 200.0, 1000.0, -400.0, 400.0,
                 {"N": [],
                  "S": [(700.0, 900.0)],            # flank corridor drops out here
                  "E": [],
                  "W": [(150.0, 350.0)]})

    # flank loop: room B -> south -> west -> north -> room A
    build_region("FlankStubB", 700.0, 900.0, -500.0, -400.0, {"W": [], "E": []}, floor_drop=2.0)
    build_region("FlankStubA", -700.0, -500.0, -500.0, -400.0, {"W": [], "E": []}, floor_drop=2.0)
    build_region("FlankMain", -700.0, 900.0, -700.0, -500.0,
                 {"S": [],
                  "N": [(-700.0, -500.0), (700.0, 900.0)],   # the two stubs
                  "W": [],
                  "E": []}, floor_drop=4.0)


def build_cover():
    # (name, x, y, size_x, size_y, height). 110 to 120 cm: crouch behind it, shoot over it.
    cover_boxes = [
        # room A: one to break the sight line from the door, one to fall back to
        ("Cover_A1", -480.0, 120.0, 260.0, 100.0, 110.0),
        ("Cover_A2", -180.0, -180.0, 100.0, 260.0, 120.0),
        # room B: one per enemy
        ("Cover_B1", 360.0, 140.0, 100.0, 260.0, 120.0),
        ("Cover_B2", 880.0, 190.0, 100.0, 260.0, 120.0),
        ("Cover_B3", 620.0, -130.0, 260.0, 100.0, 110.0),
    ]

    for name, x, y, size_x, size_y, height in cover_boxes:
        spawn_box(name, x, y, height * 0.5, size_x, size_y, height)


def setup_light(actor, label, component_class, intensity):
    """Light actors expose their component under different names, so look it up by class."""
    if actor is None:
        return

    actor.set_actor_label(label)

    component = actor.get_component_by_class(component_class)
    if component is None:
        return

    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity", intensity)


def spawn_room_light(name, x, y, intensity=5000.0, radius=1400.0):
    """Ceilings block the sun, so each space gets its own lamp."""
    light = editor_actor.spawn_actor_from_class(
        unreal.PointLight, unreal.Vector(x, y, WALL_HEIGHT - 60.0), unreal.Rotator(0, 0, 0))
    if light is None:
        return None

    light.set_actor_label(name)

    component = light.get_component_by_class(unreal.PointLightComponent)
    if component:
        component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity", intensity)
        component.set_editor_property("attenuation_radius", radius)
        component.set_editor_property("cast_shadows", False)

    return light


def build_interior_lights():
    spawn_room_light("Light_RoomA_1", -550.0, 150.0)
    spawn_room_light("Light_RoomA_2", -200.0, -200.0)
    spawn_room_light("Light_RoomB_1", 400.0, 150.0)
    spawn_room_light("Light_RoomB_2", 800.0, -150.0)
    spawn_room_light("Light_Spawn", -1200.0, -250.0, 3000.0, 900.0)
    spawn_room_light("Light_Entry", -900.0, -250.0, 2500.0, 700.0)
    spawn_room_light("Light_Door", 100.0, 250.0, 2500.0, 700.0)
    spawn_room_light("Light_Flank_1", -300.0, -600.0, 3500.0, 1100.0)
    spawn_room_light("Light_Flank_2", 500.0, -600.0, 3500.0, 1100.0)


def build_lighting():
    sun = editor_actor.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 900), unreal.Rotator(-50, 0, -30))
    setup_light(sun, "Sun", unreal.DirectionalLightComponent, 3.5)

    sky_light = editor_actor.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600), unreal.Rotator(0, 0, 0))
    setup_light(sky_light, "SkyLight", unreal.SkyLightComponent, 2.0)

    sky = editor_actor.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if sky:
        sky.set_actor_label("SkyAtmosphere")


def build_gameplay_actors():
    player_start = editor_actor.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-1200.0, -250.0, 100.0), unreal.Rotator(0, 0, 0))
    if player_start:
        player_start.set_actor_label("PlayerStart")

    spawner_class = unreal.load_class(None, "/Script/ProjectTF.EnemySpawner")
    if not spawner_class:
        unreal.log_error("EnemySpawner class not found: build the editor target first")
        return

    spawner = editor_actor.spawn_actor_from_class(spawner_class, unreal.Vector(600.0, 0.0, 20.0), unreal.Rotator(0, 0, 180))
    if not spawner:
        return

    spawner.set_actor_label("EnemySpawner")

    # one enemy next to each piece of cover in room B, relative to the spawner at (600, 0)
    # note the spawner faces -X, so the offsets are rotated with it
    spawner.set_editor_property("spawn_offsets", [
        unreal.Vector(170.0, -80.0, 0.0),    # -> world (430, 80)   next to Cover_B1
        unreal.Vector(-330.0, -150.0, 0.0),  # -> world (930, 150)  next to Cover_B2
        unreal.Vector(-50.0, 250.0, 0.0),    # -> world (650, -250) next to Cover_B3
    ])


def build_nav_bounds(center, extent):
    """
    NavMesh bounds volume covering the whole playable area.

    Python does not expose the brush builders, so the volume is spawned and then scaled
    to the wanted size using whatever brush geometry it came with.
    """
    volume = editor_actor.spawn_actor_from_class(unreal.NavMeshBoundsVolume, center, unreal.Rotator(0, 0, 0))
    if volume is None:
        unreal.log_error("failed to spawn the NavMeshBoundsVolume")
        return None

    volume.set_actor_label("NavMeshBounds")

    origin, current_extent = volume.get_actor_bounds(False)
    unreal.log("NAVBOUNDS_RAW extent=%s" % str(current_extent))

    if current_extent.x > 1.0 and current_extent.y > 1.0 and current_extent.z > 1.0:
        scale = volume.get_actor_scale3d()
        volume.set_actor_scale3d(unreal.Vector(
            scale.x * extent.x / current_extent.x,
            scale.y * extent.y / current_extent.y,
            scale.z * extent.z / current_extent.z))

        origin, final_extent = volume.get_actor_bounds(False)
        unreal.log("NAVBOUNDS_FINAL extent=%s" % str(final_extent))
    else:
        unreal.log_warning("NAVBOUNDS_EMPTY the spawned volume has no brush geometry")

    return volume



def configure_navmesh():
    """
    Switch the level navmesh to dynamic generation.

    The editor creates a RecastNavMesh actor for the bounds volume and saves it with no tile
    data, because nothing ever ran Build Paths on it. Keeping the actor matters: the navigation
    system decides at world init whether to keep a geometry octree at all, and it only does so
    when nav data that supports rebuilding already exists. Dynamic generation gives it both.
    """
    for actor in editor_actor.get_all_level_actors():
        if actor.get_class().get_name() != "RecastNavMesh":
            continue

        actor.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
        unreal.log("NAVMESH runtime_generation=" + str(actor.get_editor_property("runtime_generation")))
        return actor

    unreal.log_warning("NAVMESH no RecastNavMesh actor found")
    return None


def main():
    unreal.log("=== building " + LEVEL_PATH + " ===")

    # start from a blank map. new_blank_map works in a commandlet, unlike the level editor subsystem.
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        unreal.log_error("could not create a blank map")
        return

    build_geometry()
    build_cover()
    build_gameplay_actors()

    try:
        build_interior_lights()
    except Exception as error:
        unreal.log_error("interior lights failed: " + str(error))

    try:
        build_lighting()
    except Exception as error:
        unreal.log_error("lighting failed: " + str(error))

    try:
        # covers both rooms, the spawn closet and the flank loop
        build_nav_bounds(unreal.Vector(-200.0, -150.0, 140.0), unreal.Vector(1400.0, 800.0, 400.0))
    except Exception as error:
        unreal.log_error("nav bounds failed: " + str(error))

    try:
        configure_navmesh()
    except Exception as error:
        unreal.log_error("navmesh config failed: " + str(error))

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH):
        unreal.log_error("=== failed to save " + LEVEL_PATH + " ===")
        return

    # The navigation system only creates the RecastNavMesh actor once the level is loaded,
    # so reopen the level we just wrote and configure it there before saving again.
    reloaded = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if reloaded and configure_navmesh():
        unreal.EditorLoadingAndSavingUtils.save_map(reloaded, LEVEL_PATH)

    unreal.log("=== saved " + LEVEL_PATH + " ===")


main()
