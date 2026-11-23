"""纯验证脚本：不做任何修改，只打开 Lvl_ThirdPerson、对准天空、截两张图、退出。

用来验证「关掉编辑器再打开」时星空是否依然在 —— 即验证已持久化的关卡状态，
而不是验证安装脚本本身。

用法：
  UnrealEditor.exe <project>.uproject -ExecutePythonScript="<本文件>"
"""

import traceback

import unreal

TAG = "[VERIFY]"
_state = {"frame": 0, "done": False}


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def warn(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def report():
    """把关卡里的天空相关 actor 和参数打出来，作为状态证据。"""
    try:
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        names = sorted(a.get_class().get_name() for a in eas.get_all_level_actors())
        log("关卡 actor 类统计: SkyAtmosphere=%d VolumetricCloud=%d SkyLight=%d AuroraSkyDome=%d"
            % (names.count("SkyAtmosphere"), names.count("VolumetricCloud"),
               names.count("SkyLight"), names.count("AuroraSkyDome")))

        for actor in eas.get_all_level_actors():
            if actor.get_class().get_name() == "AuroraSkyDome":
                s = actor.get_editor_property("sky_settings")
                log("穹顶: %s | material=%s | cube=%s | hz=%s | intensity=%s | time_scale=%s"
                    % (actor.get_name(),
                       s.get_editor_property("sky_material"),
                       s.get_editor_property("cube_resolution"),
                       s.get_editor_property("update_frequency"),
                       s.get_editor_property("sky_intensity"),
                       s.get_editor_property("sky_time_scale")))
    except Exception:
        warn("report 失败: %s" % traceback.format_exc())

    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        sub = unreal.SystemLibrary.get_world_subsystem(  # 可能不存在，失败也无所谓
            world, unreal.AuroraSkySubsystem)
        if sub:
            log("子系统: cube=%s enabled=%s skytime=%s"
                % (sub.get_cube_resolution(), sub.is_sky_enabled(), sub.get_sky_time()))
    except Exception:
        log("(子系统查询不可用，跳过)")


def aim(pitch):
    loc = unreal.Vector(0.0, -900.0, 400.0)
    rot = unreal.Rotator(0.0, pitch, 0.0)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(loc, rot)
            return
    except Exception:
        pass
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)


def shoot():
    world = unreal.EditorLevelLibrary.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")


def on_tick(delta_seconds):
    _state["frame"] += 1
    frame = _state["frame"]
    try:
        if frame == 40:
            report()
            aim(6.0)
            log("视口相机：地平线略上方（看极光带）")
        elif frame == 90:
            shoot()
            log("截图 1（地平线 / 极光）")
        elif frame == 170:
            aim(55.0)
            log("视口相机：抬头（看星云）")
        elif frame == 220:
            shoot()
            log("截图 2（抬头 / 星云）")
        elif frame == 300:
            aim(-80.0)
            log("视口相机：低头（看地面雾）")
        elif frame == 350:
            shoot()
            log("截图 3（低头 / 地面雾）")
        elif frame == 420 and not _state["done"]:
            _state["done"] = True
            log("验证结束，退出")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        warn("tick 出错: %s" % traceback.format_exc())


log("验证脚本已加载")
try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
except Exception:
    warn("keep-alive 失败: %s" % traceback.format_exc())

unreal.register_slate_post_tick_callback(on_tick)
