# SpatialCoreDemo: the worked example of a consumer driving RenderEngine from the three control
# routes (ADM-OSC, trajectories, the spatial map), and the screenshot tool for the map (Phase 4,
# D-01 to D-03). Built only when SPATIALCORE_BUILD_EXAMPLES=ON in a top-level build; see the
# root CMakeLists.txt.
juce_add_gui_app(SpatialCoreDemo
    PRODUCT_NAME "SpatialCoreDemo"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(SpatialCoreDemo
    PRIVATE
        demo/Main.cpp
        demo/DemoComponent.cpp)

target_compile_definitions(SpatialCoreDemo
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="SpatialCoreDemo"
        JUCE_APPLICATION_VERSION_STRING="0.1.0")

target_link_libraries(SpatialCoreDemo
    PRIVATE
        SpatialCoreUI
        SpatialCore)
