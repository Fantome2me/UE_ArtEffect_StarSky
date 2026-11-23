"""球面巡览：把视口相机依次指向 6 个主轴方向，各截一张图。

用来定位「天空只有半边渲染」这类问题 —— 如果是某些 cube 面没写进去，
对应方向的截图就是黑的。

用法：
  UnrealEditor.exe <project>.uproject -ExecutePythonScript="<本文件>"
"""

import traceback

import unreal

TAG = "[SURVEY]"
# (标签, pitch, yaw)  —— UE 的 Rotator 是 (roll, pitch, yaw)
DIRECTIONS = [
    ("+X", 0.0, 0.0),
    ("-X", 0.0, 180.0),
    ("+Y", 0.0, 90.0),
    ("-Y", 0.0, -90.0),
    ("+Z_up", 88.0, 0.0),
    ("-Z_down", -88.0, 0.0),
]

CAMERA_LOCATION = unreal.Vector(0.0, 0.0, 2000.0)

_state = {"frame": 0, "index": -1, "done": False}


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def warn(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def aim(pitch, yaw):
    rot = unreal.Rotator(0.0, pitch, yaw)
    aimed = False
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(CAMERA_LOCATION, rot)
            aimed = True
    except Exception:
        pass
    if not aimed:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(CAMERA_LOCATION, rot)


def shot():
    world = unreal.EditorLevelLibrary.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "HighResShot 800x600")


def on_tick(delta_seconds):
    _state["frame"] += 1
    f = _state["frame"]
    try:
        # 每 20 帧推进一个方向：0 帧对准，6 帧后截图。
        # 注意 tick 回调的第一次调用 f 已经是 1，所以用 (f - 1) 做基准，
        # 否则「对准第 1 个方向」那一步永远不执行，第一张会在没对准时盲拍。
        if f < 20 * len(DIRECTIONS) + 40:
            index = (f - 1) // 20
            phase = (f - 1) % 20
            if phase == 0 and index != _state["index"] and index < len(DIRECTIONS):
                _state["index"] = index
                tag, pitch, yaw = DIRECTIONS[index]
                aim(pitch, yaw)
                log("对准 %s (pitch=%.0f yaw=%.0f)" % (tag, pitch, yaw))
            elif phase == 6 and 0 <= index < len(DIRECTIONS):
                shot()
                log("截图 %s" % DIRECTIONS[index][0])
        elif not _state["done"]:
            _state["done"] = True
            log("巡览结束，退出")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("巡览脚本已加载")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
unreal.register_slate_post_tick_callback(on_tick)
