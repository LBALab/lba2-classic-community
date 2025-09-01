# add_library(obj AFF_OBJ.ASM)
add_library(obj AFF_OBJ.CPP)
target_link_libraries(obj PRIVATE 3D)
# set_source_files_properties(AFF_OBJ.ASM PROPERTIES LANGUAGE ASM_MASM)

# Source files in folder but not used on CMakeScript:
# - 3D_OBJ.ASM
