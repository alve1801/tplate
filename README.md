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

Also included is `mapmaker.cpp`, which was an earlier iteration of the same
 idea. It's a small, low-feature paint program, with the gimmick that the
 canvas is a sphere. The upload is for archivial purposes only - if/when I
 revisit the idea, it'd be as part of TPlate proper.

Does require my [SDL2 framework](https://github.com/alve1801/engine) to work.

## Roadmap / Todo

- Morphs (multiple shapes per plate)
 - savefiles, rendering, editing, oh my!
 - "decorations"? will need either that, or draw order
 - polylines for tracking rifts?
 - consider keeping coast data as something else than `vector<Q>`
- UI streamlining
 - states as objects, or some similar encapsulation? see minix
 - GUI buttons instead of keybindings for everything
 - add/move/delete in timegraph ('T')?
  - adding a new one uses M, how move/delete? do we *need* separate ui for it?
- Relations
 - 'move linked', 'join', 'swap rel' etc. Parental stuff currently only doable
  via manual savefile editing
 + J for join
- Exports - equirectangular, maybe mollweide? mainly just a different cast
 while rasretizing, but how adapt?
- Mapmaker merge - shouldnt be *too* hard, but adapting the UI will be messy
- Various fixes:
 + have pts[] store points on surface instead of screen space (also makes it
  possible to rotate the planet while drawing)
 + store timestamps in the continents instead of a separate struct
 - speed/distance still wrong
 - `pts[]` has fixed size
 - gnome sort in rasterizer is hacky af, how fix?
 - centering occasionally incorrect (no signs in trigs)
 - recenter continents after rifting?
 - set parent when rifting
 - 'select/move linked'?
 - hack rasterizer to extend until horizon (how?)
 - how remove keyframe preview struct?
 - timegraph upside down
 - better text input
 + added a rebinding for SDL_SCANCODEs so my eyes hurt less now
- birth/death times of plates? separate from parental rifting
