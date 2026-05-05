# Walkthrough 5: Kamera, viewport i interakcja UI

## Funkcje
- `Camera::handle_event(const SDL_Event&)`
- `Camera::view_matrix()`
- `Camera::projection_matrix(float)`
- `Camera::set_viewport_size(float, float)`
- `Application::sync_camera_to_viewport()`
- `Application::build_docked_layout()`

## 1) Obsluga eventow kamery
`Camera::handle_event`:
1. MOUSE_MOTION:
- srodkowy przycisk: orbit albo pan (zalezne od SHIFT),
- prawy przycisk: zoom vertical.
2. MOUSE_BUTTON_DOWN/UP:
- steruje flagami interakcji (`middle_button_down_`, `right_button_down_`).
3. MOUSE_WHEEL:
- dolly.
4. KEY_DOWN:
- `V` przelacza projection mode,
- `F` wykonuje fit_all,
- `KP_1/2/3/0` ustawia widoki predefiniowane.

## 2) Macierze kamery
- `view_matrix()` zwraca `lookAt(position, target, up)`.
- `projection_matrix(aspect)`:
- perspective: `glm::perspective(fov, aspect, near, far)`,
- orthographic: `glm::ortho(...)` zalezne od `ortho_scale`.

## 3) Synchronizacja z rendererem
`Application::sync_camera_to_viewport()`:
1. Oblicza aspect ratio z rozmiaru okna.
2. Pobiera `view` i `projection` z `Camera`.
3. Liczy `view_projection = projection * view`.
4. Wypycha macierze do:
- `vulkan_context_.set_camera_matrices(...)`,
- `viewport_panel_.set_camera_matrices(...)`.

## 4) Integracja z petla renderowania
`Application::build_docked_layout()`:
1. Odpala ramke ImGui i dockspace.
2. Renderuje panele:
- `FeatureTreePanel::draw`,
- `ViewportPanel::draw`,
- `PropertiesPanel::draw`.
3. Dla klikniecia LPM w viewport uruchamia picking:
- `viewport_panel_.getClickRay(...)`,
- `submit_once(PickSolid)`,
- aktualizacja selekcji feature/edges.
4. Przetwarza akcje z drzewa (`process_feature_tree_actions`).
5. Rysuje overlay szkicu (`SketchView::draw_overlay`).

## 5) Granice odpowiedzialnosci
- Camera: logika transformacji i input low-level.
- Application: routing eventow i synchronizacja do GPU/UI.
- ViewportPanel: ray i prezentacja 3D.
- FeatureTreePanel: intencje edycji, ktore uruchamiaja rebuild.

## Ryzyka regresji
- Niespojnosc viewport size vs aspect ratio.
- Bledy raycastu po zmianie macierzy.
- Konflikty focusu ImGui i eventow SDL.
- Brak synchronizacji camera matrices po resize.