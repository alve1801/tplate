# TPlate

Accidentally got a friend into worldbuilding a couple years ago. He was going
 through [worldbuildingpasta's blog posts on the matter](https://worldbuildingpasta.blogspot.com/2020/01/an-apple-pie-from-scratch-part-va.html)
 and lamenting how difficult GPlate can be to use, so I decided to try making
 something similar from scratch.

At the moment, it is still barely a proof of concept, so I will not be going
 much into how to use it. Pretty much everything happens via keybindings, the
 help menu should be up to date. If you want to peruse the code, you might
 find it interesting that instead of convex decomposition or similar, the
 rasterization algorithm rasterizes entire landmasses at once (line by line).
 Likewise, rifting assumes a single rift splits a single continent in two.
 Both of these can lead to "counterintuitive" behavior (self-intersecting
 coasts and rifts etc), but instead of using significantly more complicated
 algorithms (and much more code), I opted to go with something simpler that is
 usable enough for what you'd probably want to do anyway.

## Roadmap / Todo

- Morphs
 - savefiles, rendering, editing, oh my!
 - "decorations"?
 - polylines for tracking rifts?
- UI streamlining
 - states as objects, or some similar encapsulation? (see minix)
 - GUI buttons instead of keybindings for everything
- Relations
 - 'move linked', 'join' etc. Parental stuff currently only doable via savefile
  editing
 - recenter continents of rifting
- have pts[] store points on surface instead of screen space (also makes it
 possible to rotate the planet while drawing)
 - might cause issues w/ the rifting alg?
 - rifts could use a tooltip for casting the last pt
- did we frustrum-cull the rasterizer?
- speed/distance still wrong
