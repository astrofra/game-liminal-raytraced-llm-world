Cornell Box assets vendored on 2026-07-29 from:

- `https://www.rose-hulman.edu/class/cs/csse451/examples/scenes/cornell_box.obj`
- `https://www.rose-hulman.edu/class/cs/csse451/examples/scenes/cornell_box.mtl`

Notes:

- The OBJ contains the room geometry plus non-standard camera and light metadata.
- The `# light` block gives the center of the ceiling light. The renderer rebuilds the canonical Cornell area light from that center so diffuse bounce lighting works correctly.
