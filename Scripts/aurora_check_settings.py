"""确认关卡里**已保存**的星空参数（不是内存里临时设的）。

只读：加载关卡 → 找到穹顶 → 打印 SkySettings 的关键字段 → 退出。
"""

import unreal

TAG = "[CHECK]"
LEVEL_PATH = "/Game/ThirdPerson/Lvl_ThirdPerson"


def log(m):
    unreal.log("%s %s" % (TAG, m))


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(LEVEL_PATH)
    log("已加载 %s" % LEVEL_PATH)

    aes = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in aes.get_all_level_actors():
        if actor.get_class().get_name() != "AuroraSkyDome":
            continue
        s = actor.get_editor_property("sky_settings")
        log("穹顶: %s" % actor.get_name())
        for key in ("cube_resolution", "update_frequency", "sky_intensity",
                    "sky_time_scale", "sky_time_offset"):
            try:
                log("   %-18s = %s" % (key, s.get_editor_property(key)))
            except Exception as exc:
                log("   %-18s 读不到: %s" % (key, exc))
        return

    log("没有找到 AuroraSkyDome")


main()
unreal.SystemLibrary.quit_editor()
