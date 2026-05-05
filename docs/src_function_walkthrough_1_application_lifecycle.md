# Walkthrough 1: Application lifecycle

## Funkcje
- `main()`
- `Application::init()`
- `Application::run()`
- `Application::shutdown()`

## 1) main()
Przebieg:
1. Tworzy `app::Application app`.
2. Wywoluje `app.init()`.
3. Przy bledzie inicjalizacji pokazuje `SDL_ShowSimpleMessageBox`, wykonuje `app.shutdown()` i konczy proces kodem 1.
4. Przy sukcesie uruchamia `app.run()`.
5. Na koniec zawsze wykonuje `app.shutdown()`.

## 2) Application::init()
Wejscie/wyjscie:
- Wejscie: brak parametrow.
- Wyjscie: `bool` (status startu).

Kroki:
1. Wczytuje konfiguracje z `settings.json` (`load_settings`).
2. Inicjalizuje SDL (`SDL_Init`).
3. Tworzy okno (`window_.create`).
4. Inicjalizuje Vulkan (`vulkan_context_.init`) i frame sync (`render_frame_.init`).
5. Inicjalizuje ImGui (`init_imgui`).
6. Startuje worker geometrii (`g_orchestrator.init`).
7. Odpala backend ImGui (`init_imgui_backend`).
8. Podpina panele UI do danych (`feature_tree_panel_.set_feature_tree`, `set_sketch_document`).
9. Probuje zaladowac model (`load_model_session`), a gdy brak pliku tworzy zestaw feature'ow startowych.
10. Uruchamia solver szkicu (`sketch_document_.solve`).
11. Ustawia viewport kamery i synchronizuje macierze (`camera_.set_viewport_size`, `sync_camera_to_viewport`).
12. Ustawia `running_ = true`.

Efekty uboczne:
- Tworzy zasoby SDL/Vulkan/ImGui.
- Uruchamia worker geometrii.
- Modyfikuje drzewo feature'ow i stan kamery.

## 3) Application::run()
Wejscie/wyjscie:
- Wejscie: brak.
- Wyjscie: brak (petla do `running_ == false`).

Kroki petli:
1. Przetwarza eventy SDL.
2. Przekazuje eventy do ImGui i okna.
3. Filtruje eventy kamery i wywoluje `camera_.handle_event`.
4. Obsluguje wyjscie ze sketch mode (`ESC`): buduje segmenty i wysyla `RebuildFromSketch` do workera.
5. Reaguje na resize: `vulkan_context_.recreate_swapchain`, potem sync kamery.
6. Wysyla heartbeat geometrii: `submit_once(FrameUpdate)`.
7. Renderuje klatke: `render_vulkan_frame()`.

Warunek stopu:
- `SDL_EVENT_QUIT`, `window_.should_close()`, blad renderowania, blad recreate swapchain.

## 4) Application::shutdown()
Kroki:
1. Zapisuje ustawienia (`persist_settings`).
2. Zatrzymuje worker (`g_orchestrator.shutdown`).
3. Zamyka ImGui (`shutdown_imgui`).
4. Zwalnia zasoby renderingu (`render_frame_.shutdown`, `vulkan_context_.shutdown`).
5. Niszczy okno (`window_.destroy`).
6. Konczy SDL (`SDL_Quit`).

## Najwazniejsze zaleznosci
- Lifecycle jest liniowy: init -> run -> shutdown.
- `run()` zaklada poprawna inicjalizacje Vulkan/ImGui/worker.
- `shutdown()` musi byc bezpieczny nawet po czesciowej inicjalizacji.