# Images

Figures and renders for the README, the docs and the posts, one folder per
topic, in the order the work happened. A file keeps its step prefix
(`step9-`, `nw5-`, ...) inside its folder, so names still sort and a name
alone says where it came from. `renders/` is gitignored; a render meant to
last is copied here, into its topic's folder.

| Folder | What | Prefixes |
|---|---|---|
| [`furnace/`](furnace/) | The white furnace test, part 1 | `furnace-`, `diffuse-` |
| [`refraction/`](refraction/) | Glass, caustics, total internal reflection (step 4) | `step4-`, `rung-tir-` |
| [`depth-of-field/`](depth-of-field/) | Thin lens, focal plane, rack focus (step 5) | `step5-` |
| [`bokeh/`](bokeh/) | Aperture shapes, and Malley's method they nearly broke | `bokeh-`, `malley-` |
| [`bvh/`](bvh/) | Baselines measured before and after the BVH (step 8) | `step8-` |
| [`textures/`](textures/) | Checkers, image maps, mesh UVs, the Cycles check (step 9); Perlin noise (*The Next Week* 5) | `step9-`, `nw5-` |
| [`motion-blur/`](motion-blur/) | Moving spheres (*The Next Week* 2, step 12) | `nw2-`, `step12-` |
| [`next-week/`](next-week/) | Quads, lights, instances, volumes (*The Next Week* 6--9) | `nw6-` to `nw9-` |
| [`reference/`](reference/) | Other people's figures, kept for comparison; not my renders | -- |

A new topic gets a new folder and a row here. Paths in the posts, docs and
`scripts/` name the folder, so a moved image breaks a link -- move it with
`git mv` and update the references in the same change.
