#ifndef INPUT_H
#define INPUT_H

// The binding tables and the fold that builds Input from them. Included here so
// that every file which already includes INPUT.H sees them where it did.
#include "INPUT_BINDINGS.H"

// Touches clavier du jeu

// Input

#define I_UP (1 << INPUT_SLOT_UP)                       // 'UP'		Avancer
#define I_DOWN (1 << INPUT_SLOT_DOWN)                   // 'DOWN'	Reculer
#define I_LEFT (1 << INPUT_SLOT_LEFT)                   // 'LEFT'	Tourner à Gauche
#define I_RIGHT (1 << INPUT_SLOT_RIGHT)                 // 'RIGHT'	Tourner à droite
#define I_THROW (1 << INPUT_SLOT_THROW)                 // 'ALT'	Utilisation arme
#define I_COMPORTEMENT (1 << INPUT_SLOT_COMPORTEMENT)   // 'CTRL'	Comportement Menu
#define I_INVENTORY (1 << INPUT_SLOT_INVENTORY)         // 'SHIFT', 'I' Inventaire
#define I_ACTION_M (1 << INPUT_SLOT_ACTION_M)           // 'SPACE' 	Action du Comportement
#define I_ACTION_ALWAYS (1 << INPUT_SLOT_ACTION_ALWAYS) // 'W'   	Dialogue/Fouille
#define I_RETURN (1 << INPUT_SLOT_RETURN)               // 'RETURN'	Recentrage caméra
#define I_MENUS (1 << INPUT_SLOT_MENUS)                 // 'ESC'	Menus
#define I_HOLOMAP (1 << INPUT_SLOT_HOLOMAP)             // 'H' 		Holomap
#define I_PAUSE (1 << INPUT_SLOT_PAUSE)                 // 'P' 		Pause
#define I_ESQUIVE (1 << INPUT_SLOT_ESQUIVE)             // 'X' 		Esquive

#define I_NORMAL (1 << INPUT_SLOT_NORMAL)     // 'F5' 	Comportement Normal
#define I_SPORTIF (1 << INPUT_SLOT_SPORTIF)   // 'F6' 	Comportement Sportif
#define I_AGRESSIF (1 << INPUT_SLOT_AGRESSIF) // 'F7' 	Comportement Agressif
#define I_DISCRET (1 << INPUT_SLOT_DISCRET)   // 'F8' 	Comportement Discret

#define I_HELP (1 << INPUT_SLOT_HELP)       // 'F1' 	Aide en ligne
#define I_SAVE (1 << INPUT_SLOT_SAVE)       // 'F2' et 'S' 	Sauver
#define I_LOAD (1 << INPUT_SLOT_LOAD)       // 'F3' et 'L' 	Charger
#define I_OPTIONS (1 << INPUT_SLOT_OPTIONS) // 'F4' et 'O' 	Options Menu

#define I_CAMERA (1 << INPUT_SLOT_CAMERA)                         // '²'		Tourner la camera
#define I_CAMERA_LEVEL_PLUS (1 << INPUT_SLOT_CAMERA_LEVEL_PLUS)   // '+'		Niveau Caméra suivant
#define I_CAMERA_LEVEL_MOINS (1 << INPUT_SLOT_CAMERA_LEVEL_MOINS) // '-'		Niveau Caméra Precedent

#define I_WEAPON_1 (1 << INPUT_SLOT_WEAPON_1)  // '1'		Arme 1 (Balle Magique)
#define I_WEAPON_2 (1 << INPUT_SLOT_WEAPON_2)  // '2'		Arme 2 (Balle Magique)
#define I_WEAPON_3 (1 << INPUT_SLOT_WEAPON_3)  // '3'		Arme 3 (Balle Magique)
#define I_WEAPON_4 (1 << INPUT_SLOT_WEAPON_4)  // '4'		Arme 4 (Balle Magique)
#define I_WEAPON_5 (1 << INPUT_SLOT_WEAPON_5)  // '5'		Arme 5 (Balle Magique)
#define I_WEAPON_6 (1 << INPUT_SLOT_WEAPON_6)  // '6'		Arme 6 (Balle Magique)
#define I_WEAPON_7 (1U << INPUT_SLOT_WEAPON_7) // '7'		Arme 7 (Balle Magique)

// Input2

#define I_PINGOUIN (1 << (INPUT_SLOT_PINGOUIN - INPUT_SLOT_PINGOUIN))     // 'N'		Lancement Pingouin
#define I_JETPACK (1 << (INPUT_SLOT_JETPACK - INPUT_SLOT_PINGOUIN))       // 'J'		Jetpack On/Off
#define I_PROTECTION (1 << (INPUT_SLOT_PROTECTION - INPUT_SLOT_PINGOUIN)) // 'C'		Sort de Protection
#define I_FOUDRE (1 << (INPUT_SLOT_FOUDRE - INPUT_SLOT_PINGOUIN))         // 'F'		Sort de Foudre

#define I_JOY (I_UP | I_DOWN | I_LEFT | I_RIGHT)
#define I_FIRE (I_ACTION_M | I_INVENTORY | I_RETURN | I_THROW | I_COMPORTEMENT)
#define I_ACTION (I_ACTION_M | I_ACTION_ALWAYS)

// Hero discrete-action input bits that the throttled object loop (OBJECT.CPP MOVE_MANUAL)
// consumes as EDGES, not held state: a melee/weapon attack starts on the press edge and weapon
// fire stops on the release edge. Unlike movement (I_JOY) -- held state the fixed-timestep sim
// throttle is correct to skip -- a dropped action/fire edge sticks: the blowgun keeps firing,
// melee never starts (#407). So the throttle must not skip a frame whose input changed here (see
// I_ACTION_EDGE use in PERSO.CPP MainLoop). I_FIRE is included because the object loop's stop/idle
// reset also keys off an I_FIRE change (LastMyFire).
#define I_ACTION_EDGE (I_ACTION | I_THROW | I_ESQUIVE | I_FIRE)

#define ALL_INPUTS (0xFFFFFFFFL)

// Input
extern U32 LastInput;
extern U32 WaitNoKey;

#define ClearWaitNoInput() ClearNoRepeatInput()
#define InitWaitNoInput(inputs) GetInput(inputs)
#define InitWaitNoKey()   \
    {                     \
        WaitNoKey = TRUE; \
        MyKey = 0;        \
    }

#ifdef LBA_EDITOR
extern U32 WaitNoClick;

#define InitWaitNoClick(click) \
    {                          \
        WaitNoClick |= click;  \
        MyClick &= ~(click);   \
    }
#endif

/*══════════════════════════════════════════════════════════════════════════*
			██████┐ ██┐ ██┐ ██████┐ ██████┐
			└─██┌─┘ ███┐██│ └─██┌─┘ └─██┌─┘
			  ██│   ██████│   ██│     ██│
			  ██│   ██┌███│   ██│     ██│
			██████┐ ██│└██│ ██████┐   ██│
			└─────┘ └─┘ └─┘ └─────┘   └─┘
 *══════════════════════════════════════════════════════════════════════════*/

//-------------------------------------------------------------------------
extern void ReadInputConfig(void);
//-------------------------------------------------------------------------
extern void WriteInputConfig(void);
//-------------------------------------------------------------------------
extern void MyGetInput(void);
/* Main-loop sample; modal interfaces keep their own input context. */
extern void MyGetGameplayInput(void);
//-------------------------------------------------------------------------
extern void WaitNoInput(void);
/* Drain only UI directions without charging their release time to gameplay. */
extern void WaitNoMenuInput(void);
//-------------------------------------------------------------------------
extern void WaitInput(void);
//-------------------------------------------------------------------------
extern void ReadGamepadConfig(void);
//-------------------------------------------------------------------------
extern void WriteGamepadConfig(void);
//-------------------------------------------------------------------------

#endif // INPUT_H
