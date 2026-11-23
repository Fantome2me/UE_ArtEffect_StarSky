"""定位「地平线以下一片蓝灰」的来源。

依次：
  1. 相机水平朝 -X → 截图（基线，下半屏是蓝灰）
  2. fog_density = 0 → 截图（若下半屏变暗 = 是高度雾）
  3. 顺便把 AuroraSkyDome 隐藏 → 截图（看看背后到底是什么）
  4. 全部恢复，退出
"""

import traceback

import unreal

TAG = "[PROBE]"
_state = {"f": 0, "density": None, "done": False}


def log(m):
    unreal.log("%s %s" % (TAG, m))


def warn(m):
    unreal.log_warning("%s %s" % (TAG, m))


def aim(pitch=0.0, yaw=180.0):
    loc = unreal.Vector(0.0, 0.0, 2000.0)
    rot = unreal.Rotator(0.0, pitch, yaw)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(loc, rot)
            return
    except Exception:
        pass
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)


def shot():
    unreal.SystemLibrary.execute_console_command(
        unreal.EditorLevelLibrary.get_editor_world(), "HighResShot 640x480")


def fog_comp():
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class().get_name() == "ExponentialHeightFog":
            for c in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
                return c
    return None


def set_fog(value):
    c = fog_comp()
    if c is None:
        warn("没有雾")
        return
    if _state["density"] is None:
        _state["density"] = c.get_editor_property("fog_density")
    c.set_editor_property("fog_density", value)
    log("fog_density -> %s（原值 %s）" % (value, _state["density"]))


def set_dome_visibility(visible):
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class().get_name() == "AuroraSkyDome":
            for c in actor.get_components_by_class(unreal.StaticMeshComponent):
                c.set_visibility(visible, True)
            log("AuroraSkyDome visible -> %s" % visible)


def on_tick(dt):
    _state["f"] += 1
    f = _state["f"]
    try:
        if f == 20:
            aim()
            log("相机对准 -X 水平")
        elif f == 30:
            c = fog_comp()
            if c:
                for name in ("fog_density", "fog_height_falloff", "fog_height",
                             "fog_inscattering_luminance", "fog_inscattering_color",
                             "start_distance", "enable_volumetric_fog"):
                    try:
                        log("  %s = %s" % (name, c.get_editor_property(name)))
                    except Exception:
                        pass
        elif f == 40:
            shot()
            log("截图 A 基线")
        elif f == 70:
            set_fog(0.0)
        elif f == 85:
            shot()
            log("截图 B 无雾")
        elif f == 115:
            set_dome_visibility(False)
        elif f == 130:
            shot()
            log("截图 C 无雾 + 隐藏穹顶")
        elif f == 160:
            set_dome_visibility(True)
            set_fog(_state["density"] if _state["density"] is not None else 0.0005)
        elif f == 180 and not _state["done"]:
            _state["done"] = True
            log("探测结束")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("探测脚本已加载")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.register_slate_post_tick_callback(on_tick)
