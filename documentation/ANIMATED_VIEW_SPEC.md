# Animated Raytraced View Specification

Status: proposed  
Date: 2026-08-18  
Scope: SDL viewport presentation and background raytracing only

## 1. Purpose

Each playable view must become a short animated raytraced loop instead of a single fixed image.

The animation represents a small, damped movement of the player's head inside the suit, as if the character were breathing heavily. The world itself remains fixed. Only the camera rig moves between two poses, `A` and `B`.

The system must preserve the current interaction model: the first image appears as soon as possible, keyboard input remains responsive, and the HUD remains visible whether the viewport is static, partially animated, or fully animated.

## 2. Normative language

The keywords **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** describe normative requirements.

## 3. Definitions

- **View**: one committed scene and camera configuration associated with the player's current place.
- **Scene snapshot**: the immutable geometry, materials, lights, sky, and render settings used to produce all images of one view.
- **Animation image**: one complete raytraced image generated from the scene snapshot at a specific camera pose.
- **Frame slot**: one position in the ordered image buffer, from `0` to `7`.
- **Available image count**: the number of valid, contiguous animation images currently ready for presentation.
- **Pose A**: the authored camera pose of the scene and the first pose of the breathing movement.
- **Pose B**: the displaced camera pose at the other end of the breathing movement.
- **Ping-pong playback**: forward playback from the first available image to the last available image, followed by reverse playback to the first.

## 4. Functional requirements

### 4.1 First-image priority

1. Every new view **MUST** render image `0` from pose `A` first.
2. Image `0` **MUST** be presented immediately after it is complete.
3. The system **MUST NOT** wait for the complete animation before showing the new view.
4. Until a second image is available, image `0` **MUST** remain displayed as a valid static view.

### 4.2 Progressive image generation

1. After image `0` is complete, generation of image `1` **MUST** begin without waiting for player input.
2. Images **MUST** be calculated sequentially from pose `A` toward pose `B`.
3. Generation **MUST** continue frame by frame until one of the following conditions is met:
   - eight valid images are available;
   - the active view is replaced;
   - generation is cancelled during shutdown;
   - a rendering error prevents the next contiguous image from being produced.
4. A view **MUST NOT** contain more than eight resident animation images.
5. A completed image **MUST** become available to playback without waiting for the remaining images.
6. The system **MUST NOT** raytrace a separate `B -> A` sequence. Reverse motion is produced exclusively by playing the existing `A -> B` images backward.

### 4.3 Interaction while rendering

1. Keyboard event polling, text editing, cursor movement, command history, and text display **MUST** remain active while animation images are being generated.
2. Typing **MUST NOT** pause, cancel, or restart background image generation.
3. The render worker **MUST NOT** own or block the SDL event loop.
4. Publishing a completed image to the presentation thread **MUST** use a short synchronization section; the UI thread **MUST NOT** wait for an image to finish rendering.
5. Submitting a command **MAY** start LLM or turn processing while the current view continues to animate.
6. The current animation **SHOULD** remain visible until image `0` of the next committed view is ready, preventing a blank viewport during a turn transition.

### 4.4 Image buffer

1. The active view **MUST** own an ordered buffer with a hard capacity of eight images.
2. Valid images **MUST** occupy a contiguous range beginning at slot `0`.
3. A slot **MUST** become visible to the presentation thread only after its image is complete.
4. The presentation thread **MUST** read an immutable completed image; it **MUST NOT** read a render target that is still being written.
5. Late images from an obsolete view **MUST** be discarded by comparing a view-generation identifier.
6. Replacing a view **MUST** eventually release all images belonging to the obsolete view.
7. Raw image storage is bounded by:

   ```text
   maximum image memory = width × height × bytes_per_pixel × 8
   ```

   GPU texture copies and renderer-specific staging memory must be accounted for separately.

## 5. Camera animation

### 5.1 Fixed scene rule

Across all images belonging to one view, the following data **MUST** remain identical:

- world geometry and object transforms;
- materials and material parameters;
- sky parameters;
- world-owned lights;
- spatial and narrative state;
- render resolution, sample count, bounce limits, and exposure.

Only the camera rig may change. A camera-mounted spotlight **MAY** follow the moving camera, but its range, cone, color, and intensity **MUST** remain unchanged.

No prop, alien structure, atmospheric processor, crystal, beacon, or environmental element may be animated by this feature.

### 5.2 Endpoint poses

Pose `A` **MUST** be the authored camera pose supplied by the scene.

Pose `B` **MUST** be derived deterministically from pose `A` in camera-local space. The default displacement should remain small enough to read as head movement rather than locomotion:

- vertical translation: `+0.025 m`;
- forward translation: `+0.012 m`;
- pitch: `+0.35°`;
- roll: `+0.15°`;
- yaw: `0°` by default.

These values **SHOULD** be configurable constants. They **MUST NOT** depend on LLM sampling temperature, spatial entropy, body temperature, or random generation in the first implementation.

The field of view **MUST** remain constant between `A` and `B`.

### 5.3 Ease-in/ease-out interpolation

The target animation contains eight image positions indexed from `0` to `7`.

For image index `i`:

```text
t = i / 7
eased_t = 0.5 - 0.5 × cos(π × t)
pose(i) = interpolate(pose_A, pose_B, eased_t)
```

Translation **MUST** use the eased parameter. Orientation **MUST** use normalized rotation interpolation or an equivalent stable interpolation of the camera basis.

The interpolation must satisfy:

```text
pose(0) = pose_A
pose(7) = pose_B
```

The derivative of the movement should approach zero at both endpoints, creating the requested damped ease-in/ease-out breathing effect.

The renderer **MUST NOT** accumulate the offset from one image to the next. Every pose must be derived directly from the immutable pose `A`, preventing camera drift across loops or view changes.

## 6. Ping-pong playback

### 6.1 Playback over available images

Playback **MUST** use every image currently available, even while the buffer is still filling.

For `N` available images:

- `N = 0`: keep presenting the preceding view or a loading state;
- `N = 1`: display image `0` continuously;
- `N = 2`: play `0, 1, 0, 1, ...`;
- `N > 2`: play `0, 1, ..., N-1, N-2, ..., 1, 0, ...`.

Endpoint images **MUST NOT** be duplicated at the direction change. For example, four available images produce:

```text
0, 1, 2, 3, 2, 1, 0, 1, 2, 3, ...
```

### 6.2 Buffer growth during playback

1. The player **MUST NOT** wait for all eight images before animation starts.
2. If a new image becomes available while playback is moving from `A` toward `B`, the forward traversal **SHOULD** extend naturally to the new highest slot.
3. If a new image becomes available while playback is moving back toward `A`, it **SHOULD** join the sequence on the next forward traversal.
4. Buffer growth **MUST NOT** force the displayed index to jump backward or skip directly to the newest image.
5. Once eight images are available, the stable playback order is:

   ```text
   0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, ...
   ```

### 6.3 Playback timing

1. Playback timing **MUST** be independent from image-generation timing.
2. A slow raytrace **MUST NOT** alter the display duration of already available images.
3. The initial default playback rate **SHOULD** be `6 images per second`, producing a complete eight-image breathing cycle of approximately `2.33 seconds`.
4. The playback rate **SHOULD** be configurable without regenerating the images.
5. Playback time **MUST** use a monotonic clock and **MUST** tolerate delayed UI updates without permanently accelerating the animation.

## 7. HUD and viewport composition

1. Animation images **MUST** contain only the raytraced 3D view.
2. The procedural visor mask, thermal panels, compass, title, score, status, transcript, input field, cursor, and language-specific text **MUST** be composed after selection of the current animation image.
3. The HUD **MUST** be redrawn on every UI presentation pass, whether the viewport has one image or eight.
4. HUD values **MUST** reflect the latest committed game state; they **MUST NOT** be frozen to the state that existed when an animation image was rendered.
5. The HUD, visor mask, and text **MUST NOT** move with the breathing camera.
6. Switching between animation images **MUST NOT** erase, smear, or temporally accumulate HUD pixels.
7. The animated and static paths **MUST** share the same final compositor so that a one-image view and an eight-image view expose identical interface elements.

## 8. Runtime state model

The feature should expose an explicit state equivalent to:

```text
AnimatedView
  generation_id
  scene_snapshot
  pose_a
  pose_b
  images[8]
  available_image_count
  image_being_rendered
  displayed_image_index
  playback_direction
  playback_accumulator
  generation_complete
  generation_failed
```

State ownership requirements:

- the render worker owns the image currently being generated;
- the active animated view owns completed images;
- the UI thread owns playback time and the displayed image index;
- the game session owns HUD and narrative state independently from the animated view;
- no animation state may be serialized into the narrative save game in the first implementation.

## 9. View replacement and cancellation

1. Every committed scene change **MUST** create a new `generation_id`.
2. The old view **MAY** continue ping-pong playback while the first image of the new view is rendering.
3. The presentation swap **MUST** occur only when image `0` of the new view is complete.
4. After the swap, unfinished work for the previous generation **SHOULD** be cancelled as soon as safely possible.
5. A worker that completes an obsolete image **MUST** discard it instead of publishing it into the active buffer.
6. Shutdown **MUST** request cancellation, join the render worker, and release all image and texture resources cleanly.

## 10. Error and degraded modes

1. Failure to generate image `0` is a view-rendering failure and **MUST** use the existing error-reporting path.
2. Failure after image `0` **MUST NOT** invalidate images that are already complete.
3. If generation stops with fewer than eight images, ping-pong playback **MUST** continue over the available contiguous images.
4. If only one valid image exists, the system **MUST** degrade to the current fixed-view behavior while retaining the HUD and input.
5. An animation failure **MUST NOT** block keyboard input or corrupt the committed game state.

## 11. Observability

Debug output **SHOULD** expose, without adding mandatory player-facing HUD elements:

- active `generation_id`;
- available image count, from `0` to `8`;
- image currently being rendered;
- displayed image index and playback direction;
- render duration per image;
- total time required to fill the buffer;
- cancellation and stale-image discard events;
- estimated image and texture memory use.

The existing HUD must remain visually unchanged unless a separate interface specification explicitly requests animation diagnostics.

## 12. Non-goals

This evolution does not introduce:

- animated scenery, props, characters, crystals, lights, weather, or topology;
- camera travel between rooms;
- procedural camera shake or random jitter;
- more than two camera endpoints;
- a separately rendered reverse sequence;
- optical-flow interpolation between raytraced images;
- motion blur, temporal denoising, or temporal sample accumulation;
- animation state in save files;
- a change to LLM authority or narrative state validation.

## 13. Acceptance criteria

The feature is complete when all of the following checks pass:

1. A new view displays its first raytraced image before the remaining images have completed.
2. The second image begins rendering automatically after the first image is published.
3. Text can be entered, edited, and displayed continuously while images `1` through `7` are rendering.
4. The active view never owns more than eight completed animation images.
5. With one available image, the viewport is static and the HUD remains visible.
6. With two, four, and eight available images, playback follows the exact ping-pong sequences defined above.
7. Only camera pose data differs between the eight raytracing requests for a view.
8. Image `0` exactly uses pose `A`, image `7` exactly uses pose `B`, and intermediate poses use the specified ease-in/ease-out curve.
9. After at least ten complete ping-pong cycles, the camera returns exactly to the endpoint poses without accumulated drift.
10. HUD values can change while an old animation image is displayed and appear without rerendering that image.
11. Replacing a view cannot publish a late image from the preceding generation.
12. A forced failure on image `4` leaves images `0` through `3` playable and keeps keyboard input responsive.
13. Closing the application during background image generation terminates without a worker-thread or texture-resource leak.

## 14. Recommended implementation order

1. Introduce the bounded animated-view state and generation identifier.
2. Separate raytraced viewport images from HUD composition.
3. Publish image `0` immediately and preserve the current static fallback.
4. Add sequential background generation for images `1` through `7`.
5. Add ping-pong playback over the currently available contiguous range.
6. Add deterministic `A -> B` camera interpolation with the easing curve.
7. Add view replacement, stale-generation rejection, cancellation, and shutdown handling.
8. Add diagnostics and execute the acceptance checks.
