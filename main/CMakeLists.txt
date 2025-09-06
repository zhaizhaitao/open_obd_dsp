file(GLOB_RECURSE SOURCES_C *.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)

idf_component_register(SRC_DIRS
                    "." 
                    "export_path/components"
                    "export_path/fonts"
                    "export_path/images"
                    "export_path/screens"
                    "export_path"
                    INCLUDE_DIRS
                    ".")

