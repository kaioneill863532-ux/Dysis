# 看一眼关卡里和“天”有关的东西是怎么设的（天光、雾、云、音乐音量），写到 Saved/probe_level_sky.txt
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = []
def props(obj, names):
    r = []
    for n in names:
        try: r.append("%s=%s" % (n, obj.get_editor_property(n)))
        except Exception as e: r.append("%s=?" % n)
    return ", ".join(r)
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    cls = a.get_class().get_name()
    if cls in ("SkyLight", "ExponentialHeightFog", "VolumetricCloud", "SkyAtmosphere", "PostProcessVolume", "DysisMusicManager", "DysisSkyActor", "DirectionalLight"):
        line = "%s  %s" % (cls, a.get_actor_label())
        if cls == "SkyLight":
            c = a.light_component
            line += "  " + props(c, ["real_time_capture", "intensity", "source_type", "mobility", "lower_hemisphere_is_black", "lower_hemisphere_color", "cast_shadows"])
        elif cls == "ExponentialHeightFog":
            c = a.component
            line += "  " + props(c, ["fog_density", "fog_height_falloff", "fog_inscattering_luminance", "sky_atmosphere_ambient_contribution_color_scale", "directional_inscattering_luminance", "volumetric_fog", "volumetric_fog_scattering_distribution", "volumetric_fog_extinction_scale", "start_distance", "fog_max_opacity"])
        elif cls == "PostProcessVolume":
            s = a.settings
            line += "  unbound=%s  " % a.unbound + props(s, ["override_auto_exposure_method", "auto_exposure_method", "override_auto_exposure_bias", "auto_exposure_bias", "override_auto_exposure_min_brightness", "auto_exposure_min_brightness", "auto_exposure_max_brightness", "override_bloom_intensity", "bloom_intensity"])
        elif cls == "DysisMusicManager":
            line += "  " + props(a, ["music_volume", "day_music", "night_music"])
        elif cls == "DysisSkyActor":
            line += "  " + props(a, ["night_dome_material", "star_material", "night_fill_lux", "night_sky_gain", "star_gain", "moon_lux_per_greybox_unit"])
        out.append(line)
path = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "probe_level_sky.txt"
open(path, "w", encoding="utf-8").write("\n".join(out))
print("\n".join(out))
