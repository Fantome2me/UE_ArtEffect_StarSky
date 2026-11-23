"""从高处俯视，把「地平线以下」的半球露出来，用来对比地面雾开关前后的效果。

相机放到 z=3000（30 m），俯视 25°/45°，视线会越过关卡边缘落到下半球天空上，
这样着色器里 else 分支（镜像 + 地面雾）才真正可见。

用法：
  UnrealEditor.exe <project>.uproject -ExecutePythonScript="<本文件>"
"""

import traceback

import unreal

TAG = "[DOWN]"
_state = {"f": 0, "done": False}


def log(m):
    unreal.log("%s %s" % (TAG, m))


def warn(m):
    unreal.log_warning("%s %s" % (TAG, m))


def aim(pitch, loc=(0.0, 0.0, 3000.0)):
    rot = unreal.Rotator(0.0, pitch, 0.0)   # (roll, pitch, yaw)
    v = unreal.Vector(*loc)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(v, rot)
            return
    except Exception:
        pass
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(v, rot)


def shot():
    unreal.SystemLibrary.execute_console_command(
        unreal.EditorLevelLibrary.get_editor_world(), "HighResShot 800x600")


def report_fog():
    """确认关卡那盏 ExponentialHeightFog 的状态（它不是着色器里的地面雾）。"""
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class().get_name() != "ExponentialHeightFog":
            continue
        for c in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
            for name in ("fog_density", "fog_inscattering_luminance", "start_distance"):
                try:
                    log("  关卡雾 %s = %s" % (name, c.get_editor_property(name)))
                except Exception:
                    pass


def on_tick(dt):
    _state["f"] += 1
    f = _state["f"]
    try:
        if f == 20:
            report_fog()
        elif f == 30:
            aim(-25.0)
            log("俯视 25°")
        elif f == 45:
            shot()
            log("截图 A：-25°")
        elif f == 80:
            aim(-45.0)
            log("俯视 45°")
        elif f == 95:
            shot()
            log("截图 B：-45°")
        elif f == 150 and not _state["done"]:
            _state["done"] = True
            log("结束")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("俯视探测脚本已加载")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.register_slate_post_tick_callback(on_tick)
