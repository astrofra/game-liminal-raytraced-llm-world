Legacy source: `C:\works\projects\toy-cpp-raytracer-2003`

This new renderer does not embed the 2003 engine wholesale. It selectively keeps the algorithmic lineage of a few useful pieces and rewrites them in a smaller, testable module:

- slab-style ray/AABB rejection derived from `src/trace.cpp`
- Moller-Trumbore ray/triangle intersection derived from `src/trace.cpp`
- minimal OBJ triangulation spirit derived from `src/geometry.cpp`

Everything else in the current module is new code adapted to the present repository:

- grayscale material reduction from MTL data
- Cornell-specific light reconstruction
- BVH construction
- diffuse path tracing with direct light sampling
- binary PGM output
