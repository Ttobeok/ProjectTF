# Bakes the navmesh into the CQB level and saves it.
#
# Must run inside the full editor, not a commandlet: commandlets hold a navigation build lock
# ("Navigation NOT building because navigation build is locked"), so nothing is ever generated.
#
#   UnrealEditor.exe <project>.uproject -ExecutePythonScript="Scripts/BakeNavigation.py"
#
# Building tiles takes several editor ticks, so the work is driven from a tick callback rather
# than straight line code, and the level is saved once a test point projects onto the navmesh.

import unreal

LEVEL_PATH = "/Game/CQB/Lvl_CQB"
QUIT_WHEN_DONE = True

# points that must land on the navmesh: both rooms and the flanking corridor
TEST_POINTS = [
    ("roomA", unreal.Vector(-400.0, 0.0, 20.0)),
    ("roomB", unreal.Vector(600.0, 0.0, 20.0)),
    ("corridor", unreal.Vector(0.0, -600.0, 20.0)),
]

MAX_TICKS = 1200

state = {"ticks": 0, "handle": None, "done": False}

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
unreal.log("BAKENAV loaded %s" % LEVEL_PATH)

unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
unreal.log("BAKENAV requested RebuildNavigation")


def projected_count():
    """How many of the test points currently sit on the navmesh."""
    hits = 0
    for _, point in TEST_POINTS:
        try:
            if unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None) is not None:
                hits += 1
        except Exception:
            pass
    return hits


def finish(success):
    if state["done"]:
        return
    state["done"] = True

    if state["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(state["handle"])

    for label, point in TEST_POINTS:
        try:
            result = unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None)
        except Exception as error:
            result = "err " + str(error)
        unreal.log("BAKENAV project %s -> %s" % (label, result))

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    unreal.log("BAKENAV success=%s saved=%s ticks=%d" % (success, saved, state["ticks"]))

    if QUIT_WHEN_DONE:
        unreal.SystemLibrary.execute_console_command(world, "QUIT_EDITOR")


def on_tick(delta_seconds):
    state["ticks"] += 1

    # give the generator a moment before believing an empty result
    if state["ticks"] < 30:
        return

    if projected_count() == len(TEST_POINTS):
        finish(True)
    elif state["ticks"] > MAX_TICKS:
        finish(False)


state["handle"] = unreal.register_slate_post_tick_callback(on_tick)
