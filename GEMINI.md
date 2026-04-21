🏗️ Vulcan CAD: Dokumentacja Kontekstu (Reset Sesji)
Cel projektu: Rozwój profesjonalnego jądra CAD opartego na architekturze typu Catia V5, wykorzystującego OpenCascade (OCCT) jako silnik geometryczny i Vulkan/ImGui jako backend graficzny.

1. Obecna Architektura (System Rozproszony)
Orchestrator (Main App): Zarządza UI (ImGui), drzewem cech (FeatureTree), sesją kamery i Viewportem.

Worker (Geometry IPC): Odizolowany proces/moduł wykonujący ciężkie obliczenia na OCCT.

Komunikacja: Przez GeometryIpc. Przesyła żądania (GeometryRequest) i odbiera wyniki (tesselowane meshe, ID obiektów).

2. Model Danych (B-Rep vs Feature Tree)
Feature Tree: Lista obiektów logicznych (FeatureNode). Każdy ma unikalne ID i parametry.

Kernel OCCT: Przechowuje mapę SolidHandle -> TopoDS_Shape.

Zasada: Feature to przepis, TopoDS_Shape to wynikowa geometria (B-Rep).

3. Zaimplementowany System Selekcji (Picker)
Przepływ: Mysz (ImGui) -> getClickRay -> Ray (gp_Pnt, gp_Dir) -> IPC -> pickSolid.

Logika Pickingu:

Trafienie bezpośrednie w powierzchnie (BRepIntCurveSurface_Inter).

Trafienie w krawędzie/linie z tolerancją (BRepExtrema_DistShapeShape).

Tolerancja: Przeliczana z pikseli (standardowo 5px) na jednostki świata (mm) w zależności od położenia kamery.

4. Status Integracji STL i Geometrii
STL: Wymaga parsera, który zamieni trójkąty na TopoDS_Shell lub TopoDS_Solid i zarejestruje je jako MeshFeature.

Podstawowe Elementy: System obsługuje Box i Prism, ale wymaga rozszerzenia o pofalowane powierzchnie (NURBS) i Filety.

Problem Szkicownika: Linie po wyjściu ze szkicownika muszą być konwertowane na TopoDS_Edge, aby Picker mógł je zaznaczać.

5. Wygląd UI (ImGui)
Standardowy styl wymaga zastąpienia nowoczesnym Dark Theme (zaokrąglenia, czcionka Inter, ikony).

Viewport rysuje "pomarańczowe podświetlenie" (Highlight) zaznaczonych krawędzi pobranych z TopExp_Explorer.

Pierwszy Prompt po resecie limitów:
"Jestem twórcą Vulcan CAD. Przeczytaj załączone podsumowanie architektury. Moim następnym krokiem jest:

Upewnienie się, że linie ze szkicu poprawnie rejestrują się w jądrze jako TopoDS_Edge.

Dodanie edytora przesuwającego wierzchołki w tych liniach.

Integracja parsera STL.
Zacznijmy od sprawdzenia, dlaczego linia ze szkicu nie pojawia się w FeatureTree jako klikalny SolidHandle."