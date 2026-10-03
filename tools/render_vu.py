"""
Renders the VU meter as two layers that sandwich the live needle drawn by the plug-in:

  vu_under.png  the printed card (tools/make_vu_card.py) back-lit by two lamps, with the shadows of the
                well walls and the pivot housing falling on it
  vu_over.png   (transparent) the glass with its reflections and the moulded pivot housing with its
                zero-adjust screw, which sit in front of the needle

    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/render_vu.py -- [lamp=<W>] [key=<W>]

The camera frames exactly the meter face (184 x 166 logical px, the well minus 15 px) at 3x.
"""
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
arg = lambda k, d: float(next((a.split("=")[1] for a in argv if a.startswith(k + "=")), d))
HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "Resources")

W, H = 1.84, 1.66            # 1 unit = 100 logical px
SCALE = 3
LAMP, KEY = arg("lamp", 10.0), arg("key", 380.0)


def reset(transparent):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for dev in prefs.devices:
            dev.use = True
        sc.cycles.device = "GPU"
    except Exception as e:
        print("GPU setup failed:", e)
    sc.cycles.samples = 256
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = int(W * 100 * SCALE), int(H * 100 * SCALE)
    sc.render.film_transparent = transparent
    sc.cycles.film_transparent_glass = transparent
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    sc.view_settings.view_transform = "Standard"

    world = bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    env = world.node_tree.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.join(os.path.dirname(bpy.app.binary_path), "..", "Resources",
                                                  "%d.%d" % bpy.app.version[:2], "datafiles", "studiolights", "world", "studio.exr"))
    world.node_tree.links.new(env.outputs["Color"], world.node_tree.nodes["Background"].inputs["Color"])
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.12

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = W
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0, 0, 10)
    sc.collection.objects.link(cam)
    sc.camera = cam

    # Front studio light from the upper left, like the panel and knob renders
    data = bpy.data.lights.new("Key", "AREA")
    data.energy = KEY
    data.size = 3.0
    key = bpy.data.objects.new("Key", data)
    key.location = (-2.5, 3.0, 5.0)
    key.rotation_euler = (Vector((0, 0, 0)) - key.location).to_track_quat("-Z", "Y").to_euler()
    sc.collection.objects.link(key)
    return sc


def mat(name, base, rough, metallic=0.0, coat=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Coat Weight"].default_value = coat
    return m


def assign(o, m):
    o.data.materials.clear()
    o.data.materials.append(m)


def build(layer):
    """layer: 'under' or 'over'. Everything exists in both so shadows stay identical; visibility differs."""
    under = layer == "under"

    # Card: translucent paper lit from behind + diffuse paper lit from the front
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, 0))
    card = bpy.context.active_object
    card.scale = (W, H, 1)
    m = bpy.data.materials.new("Card")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(RES, "vu_card.png"))
    tl = nt.nodes.new("ShaderNodeBsdfTranslucent")
    df = nt.nodes.new("ShaderNodeBsdfDiffuse")
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = 0.45
    nt.links.new(tex.outputs["Color"], tl.inputs["Color"])
    nt.links.new(tex.outputs["Color"], df.inputs["Color"])
    nt.links.new(tl.outputs[0], mix.inputs[1])
    nt.links.new(df.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    assign(card, m)
    card.visible_camera = under
    card.visible_transmission = under

    # Two warm lamps behind the card, near the bottom corners
    for x in (-0.42, 0.42):
        ld = bpy.data.lights.new("Lamp", "POINT")
        ld.energy = LAMP
        ld.color = (1.0, 0.74, 0.44)
        ld.shadow_soft_size = 0.25
        lo = bpy.data.objects.new("Lamp", ld)
        lo.location = (x, -H / 2 + 0.15, -0.55)
        lo.visible_camera = False
        lo.visible_transmission = False
        lo.visible_glossy = False
        bpy.context.scene.collection.objects.link(lo)

    # Well walls (only their shadows matter; the faceplate render shows them)
    for (x, y, sx, sy) in ((0, H / 2 + 0.05, W + 0.3, 0.1), (0, -H / 2 - 0.05, W + 0.3, 0.1),
                           (-W / 2 - 0.05, 0, 0.1, H + 0.3), (W / 2 + 0.05, 0, 0.1, H + 0.3)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, 0.2))
        wall = bpy.context.active_object
        wall.scale = (sx, sy, 0.4)
        wall.visible_camera = False
        wall.visible_glossy = False

    # Moulded pivot housing with a zero-adjust screw (in front of the needle)
    plastic = mat("Housing", (0.012, 0.011, 0.01), 0.3, coat=0.4)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1, segments=96, ring_count=48, location=(0, -H / 2 - 0.04, 0.0))
    dome = bpy.context.active_object
    dome.scale = (0.368, 0.23, 0.15)
    assign(dome, plastic)
    bpy.ops.object.shade_smooth()
    dome.visible_camera = not under

    steel = mat("Screw", (0.78, 0.77, 0.74), 0.25, 1.0)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.045, depth=0.03, vertices=48, location=(0, -H / 2 + 0.07, 0.135))
    screw = bpy.context.active_object
    assign(screw, steel)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -H / 2 + 0.07, 0.152))
    slot = bpy.context.active_object
    slot.scale = (0.07, 0.011, 0.012)
    slot.rotation_euler.z = math.radians(-14)
    assign(slot, mat("Slot", (0.02, 0.02, 0.02), 0.6))
    for o in (screw, slot):
        o.visible_camera = not under

    # Cover glass (front layer only)
    if not under:
        bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, 0.45))
        glass = bpy.context.active_object
        glass.scale = (W + 0.2, H + 0.2, 1)
        g = mat("Glass", (1, 1, 1), 0.03)
        p = g.node_tree.nodes["Principled BSDF"]
        p.inputs["Transmission Weight"].default_value = 1.0
        p.inputs["IOR"].default_value = 1.5
        assign(glass, g)


for layer in ("under", "over"):
    sc = reset(transparent=(layer == "over"))
    if layer == "under":
        sc.view_settings.exposure = arg("exposure", 0.6)   # photographic exposure for the back-lit card
    build(layer)
    sc.render.filepath = os.path.join(RES, f"vu_{layer}.png")
    bpy.ops.render.render(write_still=True)
    print("Saved", layer)
