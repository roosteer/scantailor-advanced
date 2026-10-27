set(CMAKE_AUTOUIC ON)

set(ui_files
    OptionsWidget.ui)

set(sources
    ImageView.cpp ImageView.h
    Thumbnail.cpp Thumbnail.h
    Filter.cpp Filter.h
    OptionsWidget.cpp OptionsWidget.h
    Settings.cpp Settings.h
    Task.cpp Task.h
    CacheDrivenTask.cpp CacheDrivenTask.h
    Dependencies.cpp Dependencies.h
    ObliqueFinder.cpp ObliqueFinder.h
    ObliquePolicy.cpp ObliquePolicy.h
    Params.cpp Params.h
    Utils.cpp Utils.h)

add_library(deskew STATIC ${sources} ${ui_files})
target_link_libraries(deskew PUBLIC core)

translation_sources(scantailor ${sources} ${ui_files})