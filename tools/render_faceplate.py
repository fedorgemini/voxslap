"""
Renders the static faceplates (main unit + expander) in Blender: painted steel with bevelled edges,
walnut cheeks, sunken meter/scope wells, screws and the aluminium name plate with its emblem.
Knobs, switches, meter faces, the scope picture and all printed text are drawn live by the plugin
on top of these images.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/render_faceplate.py -- [test]

Layout numbers mirror VoxSlapEditor::resized() / Faceplate in Source/PluginEditor.cpp (logical px).
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TEST = "test" in argv
HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "Resources")
SCALE = 3                     # output pixels per logical pixel
U = 0.01                      # Blender units per logical pixel

EDITOR_W, CHEEK_W = 1000, 26
TOP_H, BOTTOM_H = 540, 196
METERS = [(56, 92, 214, 196), (284, 92, 432, 196), (730, 92, 214, 196)]   # VU in, scope, VU out
PLATE = (44, 16, 912, 60)

PANEL_Z, CHEEK_Z = 0.15, 0.32
KEY_ENERGY = float(next((a.split("=")[1] for a in argv if a.startswith("key=")), 1800))


def P(x, y, z=0.0):
    """Logical px (origin top-left, y down) -> Blender coords."""
    return Vector((x * U, -y * U, z))


# ---------------------------------------------------------------- scene

def reset(height_px):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        for d in prefs.devices:
            d.use = True
        sc.cycles.device = "GPU"
    except Exception as e:
        print("GPU setup failed:", e)
    sc.cycles.samples = 32 if TEST else 160
    sc.cycles.use_denoising = True
    sc.render.resolution_x = EDITOR_W * SCALE // (2 if TEST else 1)
    sc.render.resolution_y = height_px * SCALE // (2 if TEST else 1)
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 92
    sc.view_settings.view_transform = "Standard"

    world = bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    nt = world.node_tree
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.join(os.path.dirname(bpy.app.binary_path), "..", "Resources",
                                                  "%d.%d" % bpy.app.version[:2], "datafiles", "studiolights", "world", "studio.exr"))
    nt.links.new(env.outputs["Color"], nt.nodes["Background"].inputs["Color"])
    nt.nodes["Background"].inputs["Strength"].default_value = 0.1

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = EDITOR_W * U
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = P(EDITOR_W / 2, height_px / 2, 30)
    sc.collection.objects.link(cam)
    sc.camera = cam

    # Same light direction as the knob renders (upper left), large soft box for studio falloff.
    centre = P(EDITOR_W / 2, height_px / 2)
    for name, offset, energy, size in (("Key", (-4.0, 4.5, 9.0), KEY_ENERGY, 7.0), ("Fill", (5.0, -3.0, 8.0), KEY_ENERGY * 0.2, 9.0)):
        data = bpy.data.lights.new(name, "AREA")
        data.energy = energy
        data.size = size
        obj = bpy.data.objects.new(name, data)
        obj.location = centre + Vector(offset)
        obj.rotation_euler = (centre - obj.location).to_track_quat("-Z", "Y").to_euler()
        sc.collection.objects.link(obj)
    return sc


# ---------------------------------------------------------------- materials

def image_material(name, file, rough, metallic=0.0, coat=0.0, aniso=0.0, uv_scale=(1, 1), uv_offset=(0, 0), specular=0.5):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    p = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (uv_scale[0], uv_scale[1], 1)
    mp.inputs["Location"].default_value = (uv_offset[0], uv_offset[1], 0)
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(RES, file))
    tex.extension = "EXTEND"
    nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
    nt.links.new(tex.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Coat Roughness"].default_value = 0.3
    p.inputs["Specular IOR Level"].default_value = specular
    if aniso:
        p.inputs["Anisotropic"].default_value = aniso
    return mat


def material(name, base, rough=0.4, metallic=0.0, coat=0.0, specular=0.5):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    p = mat.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Specular IOR Level"].default_value = specular
    return mat


def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def bevel(obj, width, segments=3):
    m = obj.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"


# ---------------------------------------------------------------- geometry

def slab(name, x, y, w, h, z0, z1):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = (x + (v.co.x + 0.5) * w) * U
        v.co.y = -(y + (0.5 - v.co.y) * h) * U
        v.co.z = z0 + (v.co.z + 0.5) * (z1 - z0)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def cut(target, cutter):
    m = target.modifiers.new("Cut", "BOOLEAN")
    m.operation = "DIFFERENCE"
    m.object = cutter
    m.solver = "EXACT"
    cutter.hide_render = True


def rect_loop(bm, x, y, w, h, z):
    return [bm.verts.new(P(px, py, z)) for px, py in ((x, y), (x + w, y), (x + w, y + h), (x, y + h))]


def meter_bezel(name, x, y, w, h, mat_frame, mat_floor):
    """Raised frame with a sloped well: lip at outer-5px, floor at outer-15px (matches the face rect in code)."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    l0 = rect_loop(bm, x, y, w, h, PANEL_Z - 0.02)
    l1 = rect_loop(bm, x, y, w, h, PANEL_Z + 0.05)
    l2 = rect_loop(bm, x + 5, y + 5, w - 10, h - 10, PANEL_Z + 0.05)
    l3 = rect_loop(bm, x + 15, y + 15, w - 30, h - 30, PANEL_Z - 0.09)
    for a, b in ((l0, l1), (l1, l2), (l2, l3)):
        for i in range(4):
            bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
    floor = bm.faces.new(l3)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(mat_frame)
    obj.data.materials.append(mat_floor)
    obj.data.polygons[-1].material_index = 1
    bevel(obj, 0.006, 2)
    return obj


def screw(x, y, r_px=5.5, angle=0.3):
    chrome = bpy.data.materials.get("ScrewChrome") or material("ScrewChrome", (0.85, 0.85, 0.83), 0.22, 1.0)
    dark = bpy.data.materials.get("Slot") or material("Slot", (0.03, 0.03, 0.03), 0.6)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r_px * U, segments=48, ring_count=16, location=P(x, y, PANEL_Z))
    head = bpy.context.active_object
    head.scale.z = 0.45
    assign(head, chrome)
    bpy.ops.object.shade_smooth()
    for a in (angle, angle + math.pi / 2):
        s = slab("Slot", 0, 0, r_px * 1.5, r_px * 0.28, 0, 0.05)
        s.location = P(x, y, PANEL_Z) - Vector((0.75 * r_px * U, -0.14 * r_px * U, -0.0))
        s.rotation_euler.z = a
        s.location = P(x, y, PANEL_Z)
        bm_center = Vector(((r_px * 1.5) / 2 * U, -(r_px * 0.28) / 2 * U, 0))
        for v in s.data.vertices:
            v.co -= bm_center
        assign(s, dark)


def cheek(x, unit_h, v0, v1, uoff):
    obj = slab("Cheek", x, 0, CHEEK_W, unit_h, 0.0, CHEEK_Z)
    bevel(obj, 0.05, 5)
    assign(obj, image_material("Walnut%d" % x, "wood_cheeks.jpg", rough=0.45, coat=0.4,
                               uv_scale=(0.5, v1 - v0), uv_offset=(uoff, 1.0 - v1), specular=0.3))


def emblem(cx, cy):
    enamel = material("Enamel", (0.006, 0.006, 0.006), 0.5, specular=0.15)
    chrome = material("EmblemChrome", (0.9, 0.9, 0.88), 0.12, 1.0)
    z = PANEL_Z + 0.04
    bpy.ops.mesh.primitive_cylinder_add(radius=19 * U, depth=0.02, vertices=96, location=P(cx, cy, z + 0.01))
    disc = bpy.context.active_object
    assign(disc, enamel)
    bevel(disc, 0.004, 2)
    for r in (21, 13.5, 8.5, 3.5):
        bpy.ops.mesh.primitive_torus_add(major_radius=r * U, minor_radius=(1.6 if r > 20 else 1.0) * U,
                                         major_segments=96, minor_segments=12, location=P(cx, cy, z + 0.02))
        t = bpy.context.active_object
        assign(t, chrome)
        bpy.ops.object.shade_smooth()
    # Leave the right half of the inner rings open, like echoes travelling outwards: ")))"
    hider = slab("EmblemCut", cx - 19, cy - 19, 19, 38, z - 0.01, z + 0.05)
    for o in [o for o in bpy.context.scene.objects if o.name.startswith("Torus")]:
        if o.dimensions.x < 40 * U:
            cut(o, hider)


# ---------------------------------------------------------------- units

def render_unit(kind):
    unit_h = TOP_H if kind == "top" else BOTTOM_H
    sc = reset(unit_h)

    panel = slab("Panel", CHEEK_W, 0, EDITOR_W - 2 * CHEEK_W, unit_h, 0.0, PANEL_Z)
    bevel(panel, 0.02, 3)
    tex = "panel_burgundy.jpg" if kind == "top" else "panel_black.jpg"
    assign(panel, image_material("Paint", tex, rough=0.7, coat=0.0, specular=0.1))

    total = TOP_H + BOTTOM_H
    v0, v1 = (0.0, TOP_H / total) if kind == "top" else (TOP_H / total, 1.0)
    cheek(0, unit_h, v0, v1, 0.0)
    cheek(EDITOR_W - CHEEK_W, unit_h, v0, v1, 0.5)

    for (x, y) in ((CHEEK_W + 13, 13), (EDITOR_W - CHEEK_W - 13, 13),
                   (CHEEK_W + 13, unit_h - 13), (EDITOR_W - CHEEK_W - 13, unit_h - 13)):
        screw(x, y, angle=(x * 0.37 + y * 0.11) % 3.0)

    if kind == "top":
        frame = material("BezelMetal", (0.06, 0.057, 0.054), 0.45, 0.5)
        floor = material("WellFloor", (0.0, 0.0, 0.0), 0.9)
        for i, (x, y, w, h) in enumerate(METERS):
            hole = slab("Hole%d" % i, x + 3, y + 3, w - 6, h - 6, -0.5, 0.5)
            cut(panel, hole)
            meter_bezel("Bezel%d" % i, x, y, w, h, frame, floor)

        x, y, w, h = PLATE
        plate = slab("Plate", x, y, w, h, PANEL_Z, PANEL_Z + 0.035)
        bevel(plate, 0.004, 2)
        assign(plate, image_material("Alu", "plate_aluminium.jpg", rough=0.5, metallic=0.0, specular=0.25))
        for sx in (x + 13, x + w - 13):
            screw(sx, y + h / 2, 5.0, angle=sx * 0.1)
        bpy.data.objects["Plate"]  # keep
        emblem(x + 50, y + h / 2)

    out = os.path.join(RES, f"faceplate_{kind}.jpg")
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("Saved", out)


for kind in ("top", "bottom"):
    render_unit(kind)
