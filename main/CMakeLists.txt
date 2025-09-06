file(GLOB_RECURSE SOURCES_C *.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)

idf_component_register(SRC_DIRS
                    "." 
                    "ui/components"
                    "ui/fonts"
                    "ui/images"
                    "ui/screens"
                    "ui"
                    "ui/fonts"
                    INCLUDE_DIRS
                    ".")

