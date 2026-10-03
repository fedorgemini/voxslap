"""
The VoxSlap emblem, shared by tools/render_faceplate.py (name plate) and tools/render_icon.py (app icon)
so both always show exactly the same mark: a black enamel roundel in a chrome bezel with a chrome
dot (the voice) and two arcs opening to the right (its echoes).

All sizes are fractions of the roundel radius R.
"""
import bpy, math


def _mat(name, base, rough, metallic=0.0, coat=0.0, specular=0.5):
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*base, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Specular IOR Level"].default_value = specular
    return m


def _assign(o, m):
    o.data.materials.clear()
    o.data.materials.append(m)


def build_emblem(cx, cy, z, R):
    """Builds the emblem centred at (cx, cy) on a surface at height z. Returns the created objects."""
    enamel = _mat("EmblemEnamel", (0.006, 0.006, 0.006), 0.45, specular=0.12)
    chrome = _mat("EmblemChrome", (0.92, 0.92, 0.9), 0.1, 1.0)
    objs = []

    bpy.ops.mesh.primitive_cylinder_add(radius=R, depth=0.058 * R, vertices=160, location=(cx, cy, z + 0.029 * R))
    disc = bpy.context.active_object
    _assign(disc, enamel)
    objs.append(disc)

    bpy.ops.mesh.primitive_torus_add(major_radius=1.055 * R, minor_radius=0.073 * R, major_segments=160,
                                     minor_segments=24, location=(cx, cy, z + 0.05 * R))
    rim = bpy.context.active_object
    _assign(rim, chrome)
    bpy.ops.object.shade_smooth()
    objs.append(rim)

    ox = cx - 0.2 * R   # arcs and dot sit left of centre so the mark is optically balanced
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.153 * R, location=(ox, cy, z + 0.065 * R))
    dot = bpy.context.active_object
    dot.scale.z = 0.5
    _assign(dot, chrome)
    bpy.ops.object.shade_smooth()
    objs.append(dot)

    bpy.ops.mesh.primitive_cube_add(size=1, location=(ox - 0.55 * R, cy, z + 0.07 * R))
    cutter = bpy.context.active_object
    cutter.scale = (1.1 * R, 2.2 * R, 0.4 * R)
    cutter.hide_render = True
    for r in (0.455, 0.71):
        bpy.ops.mesh.primitive_torus_add(major_radius=r * R, minor_radius=0.058 * R, major_segments=128,
                                         minor_segments=20, location=(ox, cy, z + 0.07 * R))
        arc = bpy.context.active_object
        _assign(arc, chrome)
        bpy.ops.object.shade_smooth()
        m = arc.modifiers.new("Cut", "BOOLEAN")
        m.operation = "DIFFERENCE"
        m.object = cutter
        objs.append(arc)
    return objs
