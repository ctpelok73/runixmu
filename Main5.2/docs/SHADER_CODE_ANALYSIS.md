# Анализ текущего shader-пути (после последних правок)

## Что уже сделано корректно

1. **Убрана прямая зависимость от `gl_ModelViewMatrix` и `gl_Fog` в основном шейдере `CShaderGL`**
   - Вершинный шейдер использует `uModelView` для расчёта `vFogCoord`.
   - Фрагментный шейдер использует `uFogColor/uFogEnd/uFogScale`.

2. **Синхронизация fog-параметров с текущим GL state добавлена**
   - Из OpenGL подтягиваются `GL_FOG_COLOR`, `GL_FOG_START`, `GL_FOG_END`.
   - На их основе вычисляется `fogScale = 1/(end-start)`.

3. **Рендер через VAO/VBO в `BMD::RenderVertexBuffer` остаётся рабочим**
   - Перед draw вызываются `SetFogFromOpenGL()`, `SetMVPFromOpenGL()`, `ApplyMVP()`.

## Что остаётся проблемой / почему «до ума» ещё не доведено полностью

1. **Matrix source всё ещё legacy**
   - `SetMVPFromOpenGL()` продолжает читать `GL_PROJECTION_MATRIX` и `GL_MODELVIEW_MATRIX` через `glGetFloatv`.
   - Это снижает переносимость (core profile) и держит зависимость от matrix stack.

2. **Fog bridge неполный относительно fixed-function семантики**
   - Сейчас шейдерная формула покрывает linear fog.
   - Параметр `GL_FOG_DENSITY` не используется (для `GL_EXP/GL_EXP2` было бы нужно), хотя в проекте плотность задаётся.

3. **Нет единого state-object для shader-пути**
   - На текущем этапе синхронизируются только texture enable, alpha test, color mul, fog.
   - Blend/depth/cull/material state не унифицированы как единый input для шейдера.

4. **Проект в целом всё ещё mixed pipeline**
   - В кодовой базе остаётся много `glBegin/glEnd` путей.
   - ImGui backend — `ImGui_ImplOpenGL2`, т.е. fixed-function ориентированный путь.

## Краткий приоритетный план

1. Вынести матрицы в engine-owned camera/model transform и убрать чтение матриц через `glGetFloatv`.
2. Зафиксировать fog contract: либо явно поддерживаем только linear fog, либо добавить поддержку EXP/EXP2 через density.
3. Ввести централизованный render-state snapshot (blend/depth/cull/fog/material) и кормить им shader path.
4. Поэтапно мигрировать самые «видимые» `glBegin/glEnd` пути на VBO/VAO + shaders.

## Вывод

Последний шаг по `CShaderGL` — правильный и полезный, но это **частичное улучшение**, а не завершённая миграция.
