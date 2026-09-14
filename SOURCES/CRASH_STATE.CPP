#include "CRASH_STATE.H"

#include "C_EXTERN.H"

#include <SYSTEM/CRASH.H>
#include <SYSTEM/TIMER.H>

/* The cast lets a volatile global through; the handler only copies its bytes. */
#define ADD_SIGNED(name, variable) Crash_AddState(name, (const void *)&(variable), sizeof(variable), 1)
#define ADD_UNSIGNED(name, variable) Crash_AddState(name, (const void *)&(variable), sizeof(variable), 0)

void CrashState_Register(void) {
    T_OBJET *hero = &ListObjet[NUM_PERSO];

    ADD_SIGNED("island", Island);
    ADD_SIGNED("cube", NumCube);
    ADD_SIGNED("new_cube", NewCube);
    ADD_SIGNED("cube_mode", CubeMode);
    ADD_SIGNED("chapter", ListVarGame[FLAG_CHAPTER]);
    ADD_UNSIGNED("behaviour", Comportement);
    ADD_UNSIGNED("cinema", CinemaMode);
    ADD_SIGNED("fade", FlagFade);
    /* 0 while a modal loop owns the frame: the nearest thing to a flag for an open
       menu or holomap, which the engine does not keep. */
    ADD_SIGNED("fps", NbFramePerSecond);
    ADD_UNSIGNED("timer", TimerRefHR);

    ADD_SIGNED("hero", hero->Obj.X);
    ADD_SIGNED("hero", hero->Obj.Y);
    ADD_SIGNED("hero", hero->Obj.Z);
    ADD_SIGNED("hero_angle", hero->Obj.Alpha);
    ADD_SIGNED("hero_angle", hero->Obj.Beta);
    ADD_SIGNED("hero_angle", hero->Obj.Gamma);
    ADD_SIGNED("life", hero->LifePoint);
    ADD_SIGNED("body", hero->Obj.Body.Num);
    ADD_SIGNED("anim", hero->Obj.Anim.Num);
    ADD_UNSIGNED("gen_anim", hero->GenAnim);
}
