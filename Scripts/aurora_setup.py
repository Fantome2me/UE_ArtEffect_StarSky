"""动态星空极光天空 —— 一键落地脚本（UE 5.8 / Python）

用 UnrealEditor.exe -ExecutePythonScript="<本文件>" 运行。

UE 5.8 的两个坑（脚本已处理）：
  1. -ExecutePythonScript 默认「跑完就 QUIT_EDITOR」→ 必须 set_keep_python_script_alive(True)
  2. Sky 不再是 Material Domain（MD_Sky 已从 EMaterialDomain 移除）→
     改成 UMaterial::bIsSky 开关（Python: is_sky），要求 Surface + Unlit + Opaque

执行顺序：模块级同步做完材质 + 关卡，再靠 post-tick 回调做「对准相机 → 截图 → 退出」。
每一步都单独 try/except 并打完整 traceback，避免静默失败。
"""

import traceback

import unreal

TAG = "[AURORA]"

MAT_DIR = "/Game/ArtEffect/StarSky"
MAT_NAME = "M_AuroraSky"
MAT_PATH = MAT_DIR + "/" + MAT_NAME
LEVEL_PATH = "/Game/ThirdPerson/Lvl_ThirdPerson"

_state = {"frame": 0, "level_ok": False, "finished": False}


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def warn(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def step(name, fn, *args):
    """执行一步，成功返回结果，失败打完整 traceback 并返回 None。"""
    try:
        result = fn(*args)
        log("STEP OK: %s" % name)
        return result
    except Exception:
        warn("STEP FAIL: %s\n%s" % (name, traceback.format_exc()))
        return None


def resolve_enum(enum_type, *candidates):
    for name in candidates:
        try:
            return getattr(enum_type, name)
        except Exception:
            continue
    return None


def set_prop(obj, value, *names):
    last = None
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception as exc:
            last = exc
    warn("设置属性失败 %s: %s" % (names, last))
    return False


# ---------------------------------------------------------------------------
# 1. 材质
# ---------------------------------------------------------------------------

def connect_expr(mel, src, src_pins, dst, dst_pins, label):
    for sp in src_pins:
        for dp in dst_pins:
            try:
                if mel.connect_material_expressions(src, sp, dst, dp):
                    log("连接 %s: %s[%s] -> %s[%s]" % (label, src.get_name(), sp, dst.get_name(), dp))
                    return True
            except Exception:
                continue
    warn("连接失败 %s: %s %s -> %s %s" % (label, src.get_name(), src_pins, dst.get_name(), dst_pins))
    return False


def create_material():
    mel = unreal.MaterialEditingLibrary

    if not unreal.EditorAssetLibrary.does_directory_exist(MAT_DIR):
        unreal.EditorAssetLibrary.make_directory(MAT_DIR)

    # 已存在就直接复用：UE 5.8 的 delete_all_material_expressions / delete_asset
    # 在资产已被引用时会崩编辑器或删不掉，重跑脚本不需要重建节点图。
    if unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH):
        material = unreal.EditorAssetLibrary.load_asset(MAT_PATH)
        if material is not None:
            try:
                log("复用已有材质 %s (is_sky=%s, shading=%s, two_sided=%s)"
                    % (MAT_PATH,
                       material.get_editor_property("is_sky"),
                       material.get_editor_property("shading_model"),
                       material.get_editor_property("two_sided")))
            except Exception:
                warn("读取材质属性失败（继续）:%s" % traceback.format_exc())
            return material
        warn("材质存在但加载失败，尝试重建")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    log("create_asset -> %s" % material)

    if material is None:
        return None

    # UE 5.8：Sky 材质 = Surface + Unlit + Opaque + bIsSky
    unlit = resolve_enum(unreal.MaterialShadingModel, "MSM_UNLIT", "UNLIT")
    set_prop(material, unlit, "shading_model")
    set_prop(material, True, "two_sided")
    sky_ok = set_prop(material, True, "is_sky", "b_is_sky")
    log("is_sky 设置结果 = %s" % sky_ok)

    cam = mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1000, -100)
    tex = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameterCube, -720, -100)
    set_prop(tex, "AuroraSkyCube", "parameter_name")

    intensity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -720, 220)
    set_prop(intensity, "SkyIntensity", "parameter_name")
    set_prop(intensity, 1.0, "default_value")

    mul = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -380, 0)

    connect_expr(mel, cam, [""], tex, ["Coordinates", "UVs", "UV"], "CameraVector->CubeUV")
    connect_expr(mel, tex, ["RGB", "RGBA", ""], mul, ["A"], "Cube->Color")
    connect_expr(mel, intensity, [""], mul, ["B"], "Intensity")

    emissive = resolve_enum(unreal.MaterialProperty, "MP_EMISSIVE_COLOR", "EMISSIVE_COLOR")
    try:
        mel.connect_material_property(mul, "", emissive)
        log("EmissiveColor 已连接")
    except Exception:
        warn("连接 EmissiveColor 失败: %s" % traceback.format_exc())

    # 不主动 recompile_material：材质在被使用时自然会编译，避免同步编译引发的问题
    saved = unreal.EditorAssetLibrary.save_asset(MAT_PATH, False)
    log("save_asset(%s) -> %s" % (MAT_PATH, saved))
    return material


# ---------------------------------------------------------------------------
# 2. 关卡
# ---------------------------------------------------------------------------

def hide_actor(actor, reason):
    """SkyAtmosphere / VolumetricCloud 都是 USceneComponent，没有 SetVisibility，
    只能删除。方案文档 P3 明确建议「二选一，直接删掉天空大气」。"""
    name = actor.get_class().get_name()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    try:
        eas.destroy_actor(actor)
        log("已删除 %s（%s）" % (name, reason))
    except Exception:
        warn("删除 %s 失败: %s" % (name, traceback.format_exc()))


def setup_skylight(actor):
    try:
        actor.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception:
        pass

    comp = None
    try:
        comp = actor.get_editor_property("light_component")
    except Exception:
        pass
    if comp is None:
        for c in actor.get_components_by_class(unreal.SkyLightComponent):
            comp = c
            break
    if comp is None:
        warn("找不到 SkyLightComponent")
        return

    set_prop(comp, True, "real_time_capture")
    for fn in ("recapture_sky", "set_capture_is_dirty"):
        if hasattr(comp, fn):
            try:
                getattr(comp, fn)()
                log("SkyLight: %s() 成功" % fn)
                break
            except Exception:
                continue


def setup_height_fog():
    """高度雾必须关掉（密度 0），否则「天空只有半边」。

    原因（实测 A/B/C 对照确认）：
      UE 5.8 的 bIsSky 只跳过 aerial perspective，**高度雾照旧作用**在天空穹顶上。
      穹顶在 1 km 处，而 FogHeightFalloff = 0.2 使向下的光线雾密度指数增长：
        向上看 → 密度骤降 → 几乎无雾
        向下看 → 光深饱和 → 整个下半球被雾涂成一片蓝灰
      于是地平线处出现硬阶跃，看起来就像「天空只渲染了一半」。

      另外模板里 fog_inscattering_luminance 是纯黑，靠天空大气提供颜色；
      大气删掉后地平线会先被涂黑再被涂蓝，两种坏情况都试过了。

    对关卡本身几乎没有影响：密度 0.0005 时在 100 m 尺度上光深仅 0.05，本来就看不见；
    真正被它影响的只有 1 km 外的穹顶。想恢复大气感就加一个 start_distance > 1500 m 的雾。
    """
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    night_color = unreal.LinearColor(0.045, 0.06, 0.10, 1.0)
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() != "ExponentialHeightFog":
            continue
        for comp in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
            # 颜色保留夜蓝，这样以后想加一点点雾也不会是黑的
            set_prop(comp, night_color, "fog_inscattering_luminance")
            set_prop(comp, night_color, "fog_inscattering_color")
            set_prop(comp, 0.0, "fog_density")
            log("高度雾已关闭（密度 0，inscattering 保持夜蓝），避免下半球被雾涂成蓝灰")


def log_post_process(actor):
    """把 PostProcessVolume 的曝光参数打出来，方便判断天空亮度是不是被自动曝光放大的。"""
    try:
        s = actor.get_editor_property("settings")
        log("PP volume '%s': unbound=%s | auto_exp_method=%s (override=%s) | bias=%s (override=%s) | min_brt=%s (override=%s) | max_brt=%s (override=%s)"
            % (actor.get_name(),
               actor.get_editor_property("unbound"),
               s.get_editor_property("auto_exposure_method"), s.get_editor_property("override_auto_exposure_method"),
               s.get_editor_property("auto_exposure_bias"), s.get_editor_property("override_auto_exposure_bias"),
               s.get_editor_property("auto_exposure_min_brightness"), s.get_editor_property("override_auto_exposure_min_brightness"),
               s.get_editor_property("auto_exposure_max_brightness"), s.get_editor_property("override_auto_exposure_max_brightness")))
    except Exception:
        warn("读取 PostProcessVolume 失败: %s" % traceback.format_exc())


def fix_level(material):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    try:
        loaded = les.load_level(LEVEL_PATH)
        log("load_level(%s) -> %s" % (LEVEL_PATH, loaded))
    except Exception:
        warn("加载关卡失败: %s" % traceback.format_exc())
        return False

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    log("关卡 actor 数：%d" % len(actors))

    has_skylight = False
    dome = None
    for actor in actors:
        name = actor.get_class().get_name()
        if name == "SkyAtmosphere":
            hide_actor(actor, "与星空叠加会发白，方案 P3：直接删掉内置大气")
        elif name == "VolumetricCloud":
            hide_actor(actor, "体积云会遮挡星星，且依赖天空大气")
        elif name == "SkyLight":
            setup_skylight(actor)
            has_skylight = True
        elif name == "AuroraSkyDome":
            dome = actor
        elif name == "PostProcessVolume":
            log_post_process(actor)

    if not has_skylight:
        warn("关卡里没有 SkyLight，星空不会被捕获成环境光")

    setup_height_fog()

    if dome is not None:
        log("复用已有穹顶: %s" % dome.get_name())
    else:
        try:
            dome = eas.spawn_actor_from_class(unreal.AuroraSkyDome, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            log("已放置 AAuroraSkyDome: %s" % dome)
        except Exception:
            warn("放置穹顶失败: %s" % traceback.format_exc())

    if dome is not None:
        try:
            settings = dome.get_editor_property("sky_settings")
            if material:
                settings.set_editor_property("sky_material", material)
            settings.set_editor_property("cube_resolution", 512)
            settings.set_editor_property("update_frequency", 12.0)
            settings.set_editor_property("sky_intensity", 1.0)
            dome.set_editor_property("sky_settings", settings)
            dome.set_editor_property("dome_radius_meters", 1000.0)
            dome.set_editor_property("follow_camera", True)
            if hasattr(dome, "update_dome_scale"):
                dome.update_dome_scale()
            log("穹顶参数已设置（radius=1000m, cube=512, 12Hz, intensity=1.0）")
        except Exception:
            warn("设置穹顶参数失败: %s" % traceback.format_exc())

    try:
        les.save_current_level()
        log("关卡已保存")
    except Exception:
        warn("保存关卡失败: %s" % traceback.format_exc())

    return True


# ---------------------------------------------------------------------------
# 3. 视口 / 截图 / 退出
# ---------------------------------------------------------------------------

def aim_viewport_up():
    loc = unreal.Vector(0.0, -900.0, 400.0)
    rot = unreal.Rotator(0.0, 30.0, 0.0)  # 注意：unreal.Rotator 的参数顺序是 (roll, pitch, yaw)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if hasattr(les, "set_level_viewport_camera_info"):
            les.set_level_viewport_camera_info(loc, rot)
            log("视口相机已指向天空")
            return
    except Exception:
        pass
    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
        log("视口相机已指向天空（EditorLevelLibrary）")
    except Exception:
        warn("设置视口相机失败: %s" % traceback.format_exc())


def take_shot(tag):
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
        log("已触发截图 #%s" % tag)
    except Exception:
        warn("截图失败: %s" % traceback.format_exc())


def set_debug_mode(mode):
    """切换穹顶的 DebugMode：0=完整 1=仅星云 2=仅星星 3=仅极光"""
    try:
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        for actor in eas.get_all_level_actors():
            if actor.get_class().get_name() == "AuroraSkyDome":
                s = actor.get_editor_property("sky_settings")
                s.set_editor_property("debug_mode", mode)
                actor.set_editor_property("sky_settings", s)
                log("DebugMode -> %d" % mode)
                return
        warn("找不到穹顶，无法切 DebugMode")
    except Exception:
        warn("设置 DebugMode 失败: %s" % traceback.format_exc())


def on_tick(delta_seconds):
    _state["frame"] += 1
    frame = _state["frame"]

    if not _state["level_ok"]:
        return

    # 三张完整天空，间隔约 2 秒，用来同时验证「好看」和「在动」
    if frame == 60:
        aim_viewport_up()
    elif frame == 110:
        take_shot("t0")
    elif frame == 230:
        take_shot("t1")
    elif frame == 350:
        take_shot("t2")
    elif frame == 440 and not _state["finished"]:
        _state["finished"] = True
        log("全部完成，关闭编辑器")
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            warn("quit_editor 失败: %s" % traceback.format_exc())


# ---------------------------------------------------------------------------
# 主流程（模块级同步执行，异常可见）
# ---------------------------------------------------------------------------

log("脚本已加载")

try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    log("已开启 keep-alive，编辑器不会在脚本结束后自动关闭")
except Exception:
    warn("开启 keep-alive 失败:\n%s" % traceback.format_exc())

material = step("create_material", create_material)
_state["level_ok"] = step("fix_level", fix_level, material) or False

_handle = step("register_tick", unreal.register_slate_post_tick_callback, on_tick)
log("主流程结束：material=%s level_ok=%s" % (material is not None, _state["level_ok"]))
