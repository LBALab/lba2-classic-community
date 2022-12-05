add_library(men SORT.ASM MENULIB.CPP MENUFUNC.CPP #[[SELECTOR.CPP]])
target_compile_definitions(men PRIVATE _BUILDING_LIB LIB_LOCAL)

set_source_files_properties(SORT.ASM PROPERTIES LANGUAGE ASM_MASM)
