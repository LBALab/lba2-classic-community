add_library(fio PCX.CPP LOADPCX.CPP SAVEPCX.CPP)
target_compile_definitions(fio PRIVATE _BUILDING_LIB LIB_LOCAL)
