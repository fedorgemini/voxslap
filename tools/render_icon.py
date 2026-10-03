"""
Renders the app icon (1024x1024, transparent) in Blender: a burgundy painted-steel tile with the
VoxSlap emblem (black enamel roundel, chrome rim and three echo arcs) and four corner screws.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/render_icon.py
    python3 tools/make_icons.py      # derives icon.ico for Windows

Proportions follow the macOS icon grid: an 824 px body centred in the 1024 px canvas.
"""
import bpy, bmesh, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "Resources")

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
sc.cycles.samples = 256
sc.cycles.use_denoising = True
sc.render.film_transparent = True
sc.render.resolution_x = sc.render.resolution_y = 1024
sc.render.image_settings.file_format = "PNG"
sc.render.image_settings.color_mode = "RGBA"
sc.view_settings.view_transform = "Standard"

world = bpy.data.worlds.new("World")
sc.world = world
world.use_nodes = True
env = world.node_tree.nodes.new("ShaderNodeTexEnvironment")
env.image = bpy.data.images.load(os.path.join(os.path.dirname(bpy.app.binary_path), "..", "Resources",
                                              "%d.%d" % bpy.app.version[:2], "datafiles", "studiolights", "world", "studio.exr"))
world.node_tree.links.new(env.outputs["Color"], world.node_tree.nodes["Background"].inputs["Color"])
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.35

cam_data = bpy.data.cameras.new("Cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 10.24          # 1 unit = 100 px
cam = bpy.data.objects.new("Cam", cam_data)
cam.location = (0, 0, 30)
sc.collection.objects.link(cam)
sc.camera = cam

for name, loc, energy, size in (("Key", (-5, 6, 10), 2200, 6.0), ("Fill", (6, -4, 8), 400, 8.0)):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.size = size
    obj = bpy.data.objects.new(name, data)
    obj.location = loc
    from mathutils import Vector
    obj.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    sc.collection.objects.link(obj)

bpy.ops.mesh.primitive_plane_add(size=40)
bpy.context.active_object.is_shadow_catcher = True


def mat(name, base, rough, metallic=0.0, coat=0.0, specular=0.5):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Specular IOR Level"].default_value = specular
    return m


def textured(name, file, rough):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(RES, file))
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(tc.outputs["Generated"], tex.inputs["Vector"])
    nt.links.new(tex.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough
    p.inputs["Specular IOR Level"].default_value = 0.25
    p.inputs["Coat Weight"].default_value = 0.0
    return m


def assign(o, m):
    o.data.materials.clear()
    o.data.materials.append(m)


# Body tile: rounded square (macOS-like corner radius) with a soft top edge
def rounded_rect_prism(half, radius, height, segs=24):
    mesh = bpy.data.meshes.new("Tile")
    bm = bmesh.new()
    pts = []
    for cx, cy, a0 in ((half - radius, half - radius, 0), (-half + radius, half - radius, 90),
                       (-half + radius, -half + radius, 180), (half - radius, -half + radius, 270)):
        for i in range(segs + 1):
            a = math.radians(a0 + 90 * i / segs)
            pts.append((cx + radius * math.cos(a), cy + radius * math.sin(a)))
    face = bm.faces.new([bm.verts.new((x, y, 0.0)) for x, y in pts])
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    for v in [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]:
        v.co.z = height
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("Tile", mesh)
    sc.collection.objects.link(obj)
    return obj

tile = rounded_rect_prism(4.12, 1.85, 0.6)
bev = tile.modifiers.new("Edge", "BEVEL")
bev.width = 0.14
bev.segments = 8
bev.limit_method = "ANGLE"
bev.angle_limit = math.radians(50)
assign(tile, textured("Paint", "panel_burgundy.jpg", 0.62))
bpy.context.view_layer.objects.active = tile
tile.select_set(True)
bpy.ops.object.shade_auto_smooth(angle=math.radians(50))

top = 0.6
chrome = mat("Chrome", (0.92, 0.92, 0.9), 0.1, 1.0)
enamel = mat("Enamel", (0.006, 0.006, 0.006), 0.35, specular=0.2, coat=0.3)

# Emblem: shared with the plug-in's name plate (tools/emblem.py)
import sys
sys.path.insert(0, HERE)
from emblem import build_emblem
build_emblem(0.0, 0.0, top, 2.75)

# Corner screws
for x, y in ((-3.3, 3.3), (3.3, 3.3), (-3.3, -3.3), (3.3, -3.3)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.3, location=(x, y, top))
    s = bpy.context.active_object
    s.scale.z = 0.45
    assign(s, mat("Screw", (0.85, 0.85, 0.83), 0.2, 1.0))
    bpy.ops.object.shade_smooth()
    for a in (0.6, 0.6 + math.pi / 2):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, top + 0.13))
        slot = bpy.context.active_object
        slot.scale = (0.42, 0.07, 0.06)
        slot.rotation_euler.z = a
        assign(slot, mat("Slot", (0.03, 0.03, 0.03), 0.6))

sc.render.filepath = os.path.join(RES, "icon.png")
bpy.ops.render.render(write_still=True)
print("Saved icon")
