"""诊断脚本：确认地平线黑带是不是 ExponentialHeightFog 造成的。

顺序：
  A. 默认机位截图（应有黑带）
  B. 把高度雾的密度降到 0 → 再截一张
  C. 把高度雾恢复原值 → 再截一张
对比 A/B/C 即可定位。
"""

import traceback

import unreal

TAG = "[FOGPROBE]"
_state = {"frame": 0, "saved_density": None, "done": False}


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def warn(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def fog_component():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() == "ExponentialHeightFog":
            for c in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
                return c
    return None


def describe():
    comp = fog_component()
    if comp is None:
        log("关卡里没有 ExponentialHeightFog")
        return
    for name in ("fog_density", "fog_height_falloff", "start_distance",
                 "fog_inscattering_color", "fog_inscattering_luminance",
                 "enable_volumetric_fog", "volumetric_fog_density"):
        try:
            log("  %s = %s" % (name, comp.get_editor_property(name)))
        except Exception:
            pass


def set_density(value):
    comp = fog_component()
    if comp is None:
        return
    try:
        comp.set_editor_property("fog_density", value)
        log("fog_density -> %s" % value)
    except Exception:
        warn("设置 fog_density 失败: %s" % traceback.format_exc())


def shot(tag):
    unreal.SystemLibrary.execute_console_command(
        unreal.EditorLevelLibrary.get_editor_world(), "HighResShot 1600x900")
    log("截图 %s" % tag)


def on_tick(delta_seconds):
    _state["frame"] += 1
    f = _state["frame"]
    try:
        if f == 30:
            describe()
        elif f == 40:
            shot("A_with_fog")
        elif f == 100:
            comp = fog_component()
            if comp is not None:
                _state["saved_density"] = comp.get_editor_property("fog_density")
            set_density(0.0)
        elif f == 150:
            shot("B_no_fog")
        elif f == 210:
            if _state["saved_density"] is not None:
                set_density(_state["saved_density"])
        elif f == 260:
            shot("C_restored")
        elif f == 320 and not _state["done"]:
            _state["done"] = True
            log("探测结束，退出")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("雾诊断脚本已加载")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.register_slate_post_tick_callback(on_tick)
