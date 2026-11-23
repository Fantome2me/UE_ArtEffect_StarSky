"""定机位连拍：判断天空是不是「间歇性变黑」。

相机固定在 (0,-900,400) pitch +25，每隔 40 帧截一张，共 8 张（约 5 秒）。
事后统计每张的黑像素比例：若中间某几张突然全黑，说明是调度/RT 的时序问题，
而不是「某些面没烘」。

用法：
  UnrealEditor.exe <project>.uproject -ExecutePythonScript="<本文件>"
"""

import traceback

import unreal

TAG = "[FLICKER]"
_state = {"f": 0, "shot": 0, "done": False}
SHOT_EVERY = 40
MAX_SHOTS = 8


def log(m):
    unreal.log("%s %s" % (TAG, m))


def warn(m):
    unreal.log_warning("%s %s" % (TAG, m))


def aim():
    loc = unreal.Vector(0.0, -900.0, 400.0)
    rot = unreal.Rotator(0.0, 25.0, 0.0)     # (roll, pitch, yaw)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(loc, rot)
            return
    except Exception:
        pass
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)


def on_tick(dt):
    _state["f"] += 1
    f = _state["f"]
    try:
        if f == 30:
            aim()
            log("相机固定")
        elif f > 60 and (f - 60) % SHOT_EVERY == 0 and _state["shot"] < MAX_SHOTS:
            _state["shot"] += 1
            unreal.SystemLibrary.execute_console_command(
                unreal.EditorLevelLibrary.get_editor_world(), "HighResShot 640x360")
            log("第 %d 张（frame %d）" % (_state["shot"], f))
        elif _state["shot"] >= MAX_SHOTS and not _state["done"]:
            _state["done"] = True
            log("连拍结束")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("连拍脚本已加载")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.register_slate_post_tick_callback(on_tick)
