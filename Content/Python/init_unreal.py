# Editor startup hook. Runs BakeNavigation.py only when a trigger file is present, so the
# editor opens normally the rest of the time.
import os
import unreal

TRIGGER = os.path.join(unreal.Paths.project_dir(), "Saved", "BakeNavigation.trigger")

if os.path.exists(TRIGGER):
    try:
        os.remove(TRIGGER)
    except OSError:
        pass

    script = os.path.join(unreal.Paths.project_dir(), "Scripts", "BakeNavigation.py")
    unreal.log("BAKENAV startup hook running %s" % script)
    exec(compile(open(script).read(), script, "exec"), {"__name__": "__main__"})
