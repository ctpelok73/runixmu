# Shader pipeline: what is still missing for full migration

## Current state
- The project already has a GLSL program wrapper (`CShaderGL`) with uniforms for MVP, alpha test, fog and texture usage.
- This shader path is used in `BMD::RenderVertexBuffer` for mesh rendering via VAO/VBO.

## What is missing for **full** shader operation

1. **Most rendering is still fixed-function / immediate mode**
   - A large portion of rendering still uses `glBegin/glEnd` and fixed pipeline state.
   - Examples include terrain, shadow volume, effects, water and utility quad/fan rendering.
   - Until these paths are migrated to VBO/VAO + shaders, shader support is only partial.

2. **No central shader-state bridge for all legacy GL states**
   - Current shader sync handles only a subset (`alpha test`, `fog enable`, color multiplier, texture on/off).
   - Blend mode equations/functions, depth func/write, cull mode, fog parameters and other material states are not fully mirrored into shader uniforms/UBOs.
   - As a result, visual parity with fixed pipeline is incomplete.

3. **Legacy matrix flow is still used in the shader path**
   - `RenderVertexBuffer` pulls matrices from OpenGL state (`glGetFloatv(GL_PROJECTION_MATRIX/GL_MODELVIEW_MATRIX)`), instead of using a single engine-side camera/model matrix source.
   - `CShaderGL::SetViewFromCamera` exists but is not used.
   - For full migration, matrix data should come from engine transforms and not from compatibility matrix stack.

4. **Compatibility-profile dependency remains**
   - ✅ Base mesh shader path (`CShaderGL`) no longer relies on `gl_ModelViewMatrix` and `gl_Fog`: model-view and fog params are passed explicitly as uniforms.
   - Remaining compatibility dependency now mainly sits in non-migrated render paths still using fixed-function/immediate mode.

5. **No robust capability/version fallback gates**
   - Shader/VAO path initializes after `glewInit()`, but there is no explicit runtime gate for required OpenGL/extension support with graceful fallback decisions and diagnostics per feature.

6. **UI/backend split is not yet modernized**
   - ImGui is initialized with `ImGui_ImplOpenGL2`, which is fixed-function oriented.
   - Full shader pipeline modernization usually requires consistent backend strategy (or strict state isolation wrappers).

## Minimal completion roadmap
1. Inventory all `glBegin/glEnd` render paths and prioritize by frame cost/visibility.
2. Introduce unified render state object (blend/depth/cull/fog/material), consumed by shaders.
3. Replace fixed-pipeline matrices with engine-owned matrices passed as uniforms.
4. Remove compatibility built-ins from GLSL and pass fog/light data explicitly.
5. Add runtime capability checks and explicit per-feature fallback paths.
6. Migrate remaining draw paths (terrain/effects/water/UI overlays) to shader-compatible vertex formats.
