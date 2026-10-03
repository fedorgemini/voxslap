"""
Renders the plugin's knobs, toggle and keys in Blender (Cycles) as transparent PNG frames.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/render_controls.py -- <outdir> [only=<name>] [test]

Every control is modelled from primitives here, lit like the panel (key light from the upper left)
and rendered top-down with an orthographic camera. Knobs are rendered at FRAMES angles over a
full turn; tools/pack_controls.py packs the frames into sprite sheets for the plugin.

Units: knob radius = 1. The camera covers +-PAD, matching imagePad in AnalogLookAndFeel.h.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else "/tmp/controls"
ONLY = next((a.split("=", 1)[1] for a in argv if a.startswith("only=")), None)
TEST = "test" in argv

FRAMES = 120
PAD = 1.45
HDRI = os.path.join(os.path.dirname(bpy.app.binary_path), "..", "Resources",
                    "%d.%d" % bpy.app.version[:2], "datafiles", "studiolights", "world", "studio.exr")


# ---------------------------------------------------------------- scene

def reset_scene(res_x, res_y, ortho_scale, samples=128):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for d in prefs.devices:
            d.use = True
        scene.cycles.device = "GPU"
    except Exception as e:
        print("GPU setup failed, using CPU:", e)
    scene.cycles.samples = 24 if TEST else samples
    scene.cycles.use_denoising = True
    scene.render.film_transparent = True
    scene.render.resolution_x, scene.render.resolution_y = res_x, res_y
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"

    # Environment for reflections (hidden from camera by the transparent film)
    world = bpy.data.worlds.new("World")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.abspath(HDRI))
    nt.links.new(env.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 0.8

    # Orthographic top-down camera
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho_scale
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0, 0, 20)
    scene.collection.objects.link(cam)
    scene.camera = cam

    # Key light from the upper left (shadows fall to the lower right, as in the UI)
    add_light("Key", "AREA", (-5.0, 6.0, 9.0), 950, size=4.0)
    add_light("Fill", "AREA", (6.0, -3.0, 7.0), 180, size=6.0)

    # Shadow catcher floor
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
    floor = bpy.context.active_object
    floor.is_shadow_catcher = True
    return scene


def add_light(name, kind, loc, energy, size=1.0):
    data = bpy.data.lights.new(name, kind)
    data.energy = energy
    if kind == "AREA":
        data.size = size
    obj = bpy.data.objects.new(name, data)
    obj.location = loc
    direction = Vector((0, 0, 0)) - Vector(loc)
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(obj)


# ---------------------------------------------------------------- materials

def material(name, base, metallic=0.0, rough=0.4, coat=0.0, coat_rough=0.05, aniso=0.0,
             radial_tangent=False, emission=None, emission_strength=0.0, subsurface=0.0, ring_bump=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    p = nt.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1.0)
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Roughness"].default_value = rough
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Coat Roughness"].default_value = coat_rough
    if subsurface:
        p.inputs["Subsurface Weight"].default_value = subsurface
    if aniso:
        p.inputs["Anisotropic"].default_value = aniso
        if radial_tangent:
            # Circumferential brushing: tangent = Z x position (object space), symmetric about the axis
            # so the highlight stays put while the knob turns, like real spun aluminium.
            tc = nt.nodes.new("ShaderNodeTexCoord")
            cross = nt.nodes.new("ShaderNodeVectorMath")
            cross.operation = "CROSS_PRODUCT"
            cross.inputs[1].default_value = (0.0, 0.0, 1.0)
            norm = nt.nodes.new("ShaderNodeVectorMath")
            norm.operation = "NORMALIZE"
            to_world = nt.nodes.new("ShaderNodeVectorTransform")
            to_world.vector_type = "VECTOR"
            to_world.convert_from = "OBJECT"
            to_world.convert_to = "WORLD"
            nt.links.new(tc.outputs["Object"], cross.inputs[0])
            nt.links.new(cross.outputs["Vector"], norm.inputs[0])
            nt.links.new(norm.outputs["Vector"], to_world.inputs["Vector"])
            nt.links.new(to_world.outputs["Vector"], p.inputs["Tangent"])
    if emission is not None:
        p.inputs["Emission Color"].default_value = (*emission, 1.0)
        p.inputs["Emission Strength"].default_value = emission_strength
    if ring_bump:
        # Concentric lathe rings
        tc = nt.nodes.new("ShaderNodeTexCoord")
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type = "RINGS"
        wave.inputs["Scale"].default_value = 60.0
        wave.inputs["Distortion"].default_value = 2.0
        wave.inputs["Detail"].default_value = 4.0
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = ring_bump
        bump.inputs["Distance"].default_value = 0.002
        nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
        nt.links.new(wave.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], p.inputs["Normal"])
    return mat


def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def smooth(obj, bevel=0.0, segments=3, limit_angle=35):
    if bevel > 0:
        m = obj.modifiers.new("Bevel", "BEVEL")
        m.width = bevel
        m.segments = segments
        m.limit_method = "ANGLE"
        m.angle_limit = math.radians(limit_angle)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.shade_auto_smooth(angle=math.radians(limit_angle))
    obj.select_set(False)


# ---------------------------------------------------------------- geometry

def prism(name, points, z0, z1):
    """Extrude a closed 2D outline between z0 and z1."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bottom = [bm.verts.new((x, y, z0)) for x, y in points]
    face = bm.faces.new(bottom)
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    for v in [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]:
        v.co.z = z1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def circle_points(r, n=128):
    return [(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def knurl_points(r_outer, r_inner, ridges):
    pts = []
    for i in range(ridges * 2):
        a = 2 * math.pi * i / (ridges * 2)
        r = r_outer if i % 2 == 0 else r_inner
        pts.append((r * math.cos(a), r * math.sin(a)))
    return pts


def cylinder(name, r, z0, z1, n=128):
    return prism(name, circle_points(r, n), z0, z1)


def box(name, x0, x1, y0, y1, z0, z1):
    return prism(name, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z0, z1)


def group(objs):
    root = bpy.data.objects.new("Knob", None)
    bpy.context.scene.collection.objects.link(root)
    for o in objs:
        o.parent = root
    return root


# ---------------------------------------------------------------- controls
# Pointer direction at rotation 0 is +Y (12 o'clock in the image).

def build_alu_knob():
    alu = material("Alu", (0.80, 0.80, 0.80), metallic=1.0, rough=0.30)
    brushed = material("Brushed", (0.86, 0.86, 0.86), metallic=1.0, rough=0.24, aniso=0.7,
                       radial_tangent=True, ring_bump=0.15)
    paint = material("Line", (0.02, 0.02, 0.02), rough=0.5)

    skirt = prism("Skirt", knurl_points(1.0, 0.975, 150), 0.0, 0.34)
    assign(skirt, alu); smooth(skirt, 0.01, 2)
    shoulder = cylinder("Shoulder", 0.86, 0.0, 0.38)
    assign(shoulder, alu); smooth(shoulder, 0.03, 3)
    top = cylinder("Top", 0.80, 0.0, 0.44)
    assign(top, brushed); smooth(top, 0.025, 3)
    line = box("Pointer", -0.022, 0.022, 0.18, 0.78, 0.40, 0.4415)
    assign(line, paint)
    notch = box("Notch", -0.03, 0.03, 0.93, 1.01, 0.20, 0.345)
    assign(notch, paint)
    return group([skirt, shoulder, top, line, notch])


def build_black_knob():
    bakelite = material("Bakelite", (0.012, 0.011, 0.010), rough=0.32, coat=0.6, coat_rough=0.12)
    cap_mat = material("Cap", (0.010, 0.010, 0.009), rough=0.18, coat=1.0, coat_rough=0.04)
    white = material("White", (0.92, 0.90, 0.84), rough=0.4)

    skirt = prism("Skirt", knurl_points(1.0, 0.94, 64), 0.0, 0.24)
    assign(skirt, bakelite); smooth(skirt, 0.015, 2)
    deck = cylinder("Deck", 0.84, 0.0, 0.27)
    assign(deck, bakelite); smooth(deck, 0.03, 3)
    cap = cylinder("Cap", 0.62, 0.0, 0.62)
    assign(cap, cap_mat); smooth(cap, 0.14, 6)
    l1 = box("LineDeck", -0.035, 0.035, 0.64, 0.98, 0.25, 0.2715)
    l2 = box("LineCap", -0.035, 0.035, 0.08, 0.50, 0.60, 0.6215)
    for l in (l1, l2):
        assign(l, white)
    return group([skirt, deck, cap, l1, l2])


def build_chicken_knob():
    red = material("RedBakelite", (0.36, 0.025, 0.02), rough=0.26, coat=0.7, coat_rough=0.08)
    grey = material("Insert", (0.32, 0.31, 0.29), metallic=0.6, rough=0.35)
    white = material("White", (0.95, 0.93, 0.88), rough=0.4)

    # Same outline as the old vector chicken-head, pointing to +Y.
    def quad(p0, p1, p2, n=10):
        return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
                 (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in [i / n for i in range(n)]]
    outline = []
    outline += quad((0.17, 1.08), (0.0, 1.16), (-0.17, 1.08))
    outline += [(-0.17, 1.08), (-0.46, 0.0)]
    outline += quad((-0.30, -0.78), (0.0, -0.90), (0.30, -0.78))
    outline += [(0.30, -0.78), (0.46, 0.0)]
    body = prism("Body", outline, 0.0, 0.40)
    assign(body, red); smooth(body, 0.07, 4)
    hub = cylinder("Hub", 0.66, 0.0, 0.52)
    assign(hub, red); smooth(hub, 0.12, 5)
    insert = cylinder("Insert", 0.36, 0.0, 0.56)
    assign(insert, grey); smooth(insert, 0.04, 3)
    l1 = box("LineHub", -0.03, 0.03, 0.38, 0.62, 0.50, 0.5215)
    l2 = box("LineBody", -0.03, 0.03, 0.62, 1.02, 0.38, 0.4015)
    for l in (l1, l2):
        assign(l, white)
    return group([body, hub, insert, l1, l2])


def build_toggle(tilt_deg, horizontal=False):
    chrome = material("Chrome", (0.95, 0.95, 0.95), metallic=1.0, rough=0.07)
    satin = material("Satin", (0.85, 0.85, 0.84), metallic=1.0, rough=0.28)
    hole = material("Hole", (0.01, 0.01, 0.01), rough=0.6)

    ring = cylinder("Ring", 0.90, 0.0, 0.10)
    assign(ring, satin); smooth(ring, 0.04, 3)
    nut = cylinder("Nut", 0.62, 0.0, 0.30, n=6)
    nut.rotation_euler.z = math.radians(30)
    assign(nut, satin); smooth(nut, 0.03, 2, limit_angle=60)
    bushing = cylinder("Bushing", 0.30, 0.0, 0.42)
    assign(bushing, chrome); smooth(bushing, 0.03, 2)
    inner = cylinder("HoleC", 0.20, 0.0, 0.425)
    assign(inner, hole)

    # Bat-handle lever: tapered shaft + ball tip, tilted towards +Y (on) or -Y (off)
    lever_len = 2.0
    shaft = prism("Shaft", circle_points(1.0, 48), 0.0, lever_len)
    shaft.scale = (0.16, 0.16, 1.0)
    m = shaft.modifiers.new("Taper", "SIMPLE_DEFORM")
    m.deform_method = "TAPER"
    m.factor = 0.6
    m.deform_axis = "Z"
    assign(shaft, chrome); smooth(shaft)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.30, segments=48, ring_count=24, location=(0, 0, lever_len))
    ball = bpy.context.active_object
    assign(ball, chrome); smooth(ball)
    lever = bpy.data.objects.new("Lever", None)
    bpy.context.scene.collection.objects.link(lever)
    lever.location = (0, 0, 0.35)
    shaft.parent = lever
    ball.parent = lever
    if horizontal:
        lever.rotation_euler.y = math.radians(tilt_deg)   # positive = lever to the right
    else:
        lever.rotation_euler.x = math.radians(-tilt_deg)  # positive = lever up
    return group([ring, nut, bushing, inner, lever])


# ---------------------------------------------------------------- render jobs

def render_rotations(name, builder, px):
    scene = reset_scene(px, px, 2 * PAD)
    knob = builder()
    folder = os.path.join(OUT, name)
    os.makedirs(folder, exist_ok=True)
    frames = [0, 15] if TEST else range(FRAMES)
    for i in frames:
        path = os.path.join(folder, "%03d.png" % i)
        if os.path.exists(path) and not TEST:
            continue  # resumable: Cycles on Metal occasionally crashes mid-run
        knob.rotation_euler.z = -2 * math.pi * i / FRAMES  # clockwise seen from above
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)


def render_states(name, builder, states, res, ortho):
    folder = os.path.join(OUT, name)
    os.makedirs(folder, exist_ok=True)
    for i, s in enumerate(states):
        if os.path.exists(os.path.join(folder, "%03d.png" % i)) and not TEST:
            continue
        scene = reset_scene(res[0], res[1], ortho)
        builder(s)
        scene.render.filepath = os.path.join(folder, "%03d.png" % i)
        bpy.ops.render.render(write_still=True)


# Sizes are 3x the largest on-screen size at 100% so the UI stays sharp at 150% on Retina.
TOGGLE_TILTS = [62, 31, 0, -31, -62]   # frame 0 = on (up), last = off (down); middle frames animate the flip

JOBS = {
    "alu":     lambda: render_rotations("alu", build_alu_knob, 300),
    "black":   lambda: render_rotations("black", build_black_knob, 180),
    "chicken": lambda: render_rotations("chicken", build_chicken_knob, 270),
    "toggle":  lambda: render_states("toggle", build_toggle, TOGGLE_TILTS, (300, 300), 2 * 2.6),
    "power":   lambda: render_states("power", lambda t: build_toggle(t, horizontal=True),
                                     TOGGLE_TILTS, (240, 240), 2 * 2.6),
}

for job_name, job in JOBS.items():
    if ONLY is None or ONLY == job_name:
        print("Rendering", job_name)
        job()
