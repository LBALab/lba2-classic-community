#!/usr/bin/env python3
"""Disassemble the Life (LM_*) and Track (TM_*) scripts in SCENE.HQR.

Operand layouts are the runtime's own reads: DoLife, DoFuncLife and DoTest in
SOURCES/GERELIFE.CPP for Life, DoTrack and SearchOffsetTrack in
SOURCES/GERETRAK.CPP for Track. Opcode numbers are parsed from SOURCES/COMMON.H.
The format is written up in docs/SCRIPTS.md.

--game lba1 reads LBA1's SCENE.HQR instead. Its tables come from the released
LBA1 source at the commit that matches the GOG build (lba1-classic 7404567:
GERELIFE.C, GERETRAK.C, DISKFUNC.C, COMMON.H); that source is not in this
repository, so its opcode numbers are written out below.

Usage:
  scene_disasm.py <SCENE.HQR> --cube N      text listing of one cube's scripts
  scene_disasm.py <SCENE.HQR> --check       decode every script, verify, print totals
  scene_disasm.py <SCENE.HQR> --find OP...  list every use of the named opcodes (LM_/TM_ names)
  scene_disasm.py <SCENE.HQR> --sweep       give each used opcode a wrong size, count what --check catches
  scene_disasm.py --table                   print the operand tables as Markdown (docs/SCRIPTS.md)
  add --game lba1 to any of these for LBA1

--check fails (exit 1) unless every script decodes exactly to its stored length,
ends with its only END, every jump lands on an instruction start, and every
SET_TRACK, SET_TRACK_OBJ and SET_COMPORTEMENT_OBJ offset lands on an instruction
start in the script it names. In LBA2, cube N is HQR entry N + 1 and entry 0
holds the size of the largest scene; in LBA1, scene N is entry N.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hqr_inspect  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# Operand types: B u8, b s8, h s16, H u16, J Life jump (s16, absolute in this object's
# Life), G Track jump (s16, absolute in this object's Track), Z zero-terminated string,
# C condition (DoFuncLife + DoTest), F switch function (DoFuncLife only), T switch test
# (DoTest at the width of the last LM_SWITCH), M move (u8; DoLife's AdjustDirObject reads
# one more byte for some moves).
LIFE = {
    "END": [], "END_LIFE": [], "RETURN": [], "END_COMPORTEMENT": [],
    "COMPORTEMENT": [("num", "B")],
    "FALLABLE": [("on", "B")],
    "COMPORTEMENT_HERO": [("comportement", "B")],
    "SET_MAGIC_LEVEL": [("level", "B")],
    "SUB_MAGIC_POINT": [("points", "B")],
    "CAM_FOLLOW": [("obj", "B")],
    "KILL_OBJ": [("obj", "B")],
    "SUICIDE": [],
    "SET_DIR": [("move", "M")],
    "SET_DIR_OBJ": [("obj", "B"), ("move", "M")],
    "SET_COMPORTEMENT": [("life", "J")],
    "SET_COMPORTEMENT_OBJ": [("obj", "B"), ("life", "h")],
    "SET_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "SUB_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "ADD_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "HIT_OBJ": [("obj", "B"), ("force", "B")],
    "SET_TRACK": [("track", "h")],
    "SET_TRACK_OBJ": [("obj", "B"), ("track", "h")],
    "STOP_L_TRACK": [],
    "STOP_L_TRACK_OBJ": [("obj", "B")],
    "RESTORE_L_TRACK": [],
    "RESTORE_L_TRACK_OBJ": [("obj", "B")],
    "IF": [("cond", "C"), ("else", "J")],
    "AND_IF": [("cond", "C"), ("else", "J")],
    "OR_IF": [("cond", "C"), ("then", "J")],
    "SWIF": [("cond", "C"), ("else", "J")],
    "SNIF": [("cond", "C"), ("else", "J")],
    "ONEIF": [("cond", "C"), ("else", "J")],
    "NEVERIF": [("cond", "C"), ("to", "J")],
    "OFFSET": [("to", "J")],
    "ELSE": [("to", "J")],
    "SWITCH": [("func", "F")],
    "CASE": [("next", "J"), ("test", "T")],
    "OR_CASE": [("then", "J"), ("test", "T")],
    "BREAK": [("to", "J")],
    "DEFAULT": [], "END_SWITCH": [],
    "BODY": [("body", "B")],
    "BODY_OBJ": [("obj", "B"), ("body", "B")],
    "ANIM_OBJ": [("obj", "B"), ("anim", "H")],
    "ANIM": [("anim", "H")],
    "ANIM_SET": [("anim", "H")],
    "MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "ADD_MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "MESSAGE": [("text", "h")],
    "ADD_MESSAGE": [("text", "h")],
    "MESSAGE_ZOE": [("text", "h")],
    "SET_VAR_CUBE": [("var", "B"), ("value", "B")],
    "ADD_VAR_CUBE": [("var", "B"), ("value", "B")],
    "SUB_VAR_CUBE": [("var", "B"), ("value", "B")],
    "SET_VAR_GAME": [("var", "B"), ("value", "h")],
    "ADD_VAR_GAME": [("var", "B"), ("value", "h")],
    "SUB_VAR_GAME": [("var", "B"), ("value", "h")],
    "SET_USED_INVENTORY": [("item", "B")],
    "GIVE_GOLD_PIECES": [("count", "h")],
    "ADD_GOLD_PIECES": [("count", "h")],
    "USE_ONE_LITTLE_KEY": [], "INC_CHAPTER": [],
    "STATE_INVENTORY": [("item", "B"), ("body", "B")],
    "FOUND_OBJECT": [("item", "B")],
    "SET_DOOR_LEFT": [("dist", "h")],
    "SET_DOOR_RIGHT": [("dist", "h")],
    "SET_DOOR_UP": [("dist", "h")],
    "SET_DOOR_DOWN": [("dist", "h")],
    "GIVE_BONUS": [("keep", "B")],
    "CHANGE_CUBE": [("cube", "B")],
    "ADD_FUEL": [("fuel", "B")],
    "SUB_FUEL": [("fuel", "B")],
    "SET_HOLO_POS": [("pos", "B")],
    "CLR_HOLO_POS": [("pos", "B")],
    "SET_GRM": [("zone", "B"), ("on", "B")],
    "SET_CHANGE_CUBE": [("zone", "B"), ("on", "B")],
    "INC_CLOVER_BOX": [],
    "OBJ_COL": [("on", "B")],
    "INVISIBLE": [("on", "B")],
    "BRICK_COL": [("mode", "B")],
    "POS_POINT": [("point", "B")],
    "PLAY_ACF": [("name", "Z")],
    "ADD_CHOICE": [("text", "h")],
    "ASK_CHOICE": [("text", "h")],
    "ASK_CHOICE_OBJ": [("obj", "B"), ("text", "h")],
    "FULL_POINT": [],
    "BETA": [("beta", "h")],
    "BULLE": [("on", "B")],
    "GAME_OVER": [], "THE_END": [], "BRUTAL_EXIT": [],
    "PLAY_MUSIC": [("music", "B")],
    "ECHELLE": [("zone", "B"), ("on", "B")],
    "SET_HIT_ZONE": [("zone", "B"), ("force", "B")],
    "SET_ARMURE": [("armure", "b")],
    "SET_ARMURE_OBJ": [("obj", "B"), ("armure", "b")],
    "SAVE_COMPORTEMENT": [],
    "SAVE_COMPORTEMENT_OBJ": [("obj", "B")],
    "RESTORE_COMPORTEMENT": [],
    "RESTORE_COMPORTEMENT_OBJ": [("obj", "B")],
    "SAMPLE": [("sample", "h")],
    "SAMPLE_RND": [("sample", "h")],
    "SAMPLE_ALWAYS": [("sample", "h")],
    "SAMPLE_STOP": [("sample", "h")],
    "REPEAT_SAMPLE": [("sample", "h"), ("repeat", "B")],
    "BACKGROUND": [("on", "B")],
    "SET_RAIL": [("zone", "B"), ("on", "B")],
    "INVERSE_BETA": [], "NO_BODY": [], "POPCORN": [],
    "FLOW_POINT": [("point", "B"), ("flow", "B")],
    "FLOW_OBJ": [("obj", "B"), ("flow", "B")],
    "SET_ANIM_DIAL": [("anim", "H")],
    "PCX": [("pcx", "B"), ("effect", "B")],
    "PCX_MESS_OBJ": [("pcx", "B"), ("effect", "B"), ("obj", "B"), ("text", "h")],
    "PALETTE": [("palette", "B")],
    "FADE_TO_PAL": [("palette", "B")],
    "SET_CAMERA": [("zone", "B"), ("on", "B")],
    "CAMERA_CENTER": [("quarter", "B")],
    "MEMO_ARDOISE": [("num", "B")],
    "SAVE_HERO": [], "RESTORE_HERO": [],
    "SHADOW_OBJ": [("obj", "B"), ("on", "B")],
    "TRACK_TO_VAR_GAME": [("var", "B")],
    "VAR_GAME_TO_TRACK": [("var", "B")],
    "ACTION": [],
    "SET_FRAME": [("frame", "B")],
    "SET_SPRITE": [("sprite", "h")],
    "SET_FRAME_3DS": [("frame", "B")],
    "IMPACT_OBJ": [("obj", "B"), ("impact", "H"), ("dy", "h")],
    "IMPACT_POINT": [("point", "B"), ("impact", "H")],
    "NO_CHOC": [("on", "B")],
    "CINEMA_MODE": [("on", "B")],
    "ANIM_TEXTURE": [("on", "B")],
    "END_MESSAGE": [],
    "END_MESSAGE_OBJ": [("obj", "B")],
    "INIT_BUGGY": [("mode", "B")],
    "NEW_SAMPLE": [("sample", "h"), ("decalage", "h"), ("volume", "B"), ("frequence", "h")],
    "PARM_SAMPLE": [("decalage", "h"), ("volume", "B"), ("frequence", "h")],
    "POS_OBJ_AROUND": [("obj", "B"), ("around", "B")],
    "ESCALATOR": [("zone", "B"), ("on", "B")],
    "ECLAIR": [("duration", "B")],
    "PLUIE": [("duration", "B")],
}

# Conditions (LF_*): (parameter, compare width). DoFuncLife leaves TypeAnswer at
# RET_S8 unless the case sets RET_U8 or RET_S16; DoTest then reads one test byte
# (LT_*) and a value of that width. No condition returns RET_STRING.
FUNC = {
    "COL": (None, 1), "CHAPTER": (None, 1), "LIFE_POINT": (None, 2), "HIT_BY": (None, 1),
    "HIT_OBJ_BY": ("obj", 1), "ACTION": (None, 1), "LIFE_POINT_OBJ": ("obj", 2),
    "COL_OBJ": ("obj", 1), "DISTANCE": ("obj", 2), "DISTANCE_3D": ("obj", 2),
    "CONE_VIEW": ("obj", 2), "ZONE": (None, 1), "NB_GOLD_PIECES": (None, 2),
    "NB_LITTLE_KEYS": (None, 1), "COMPORTEMENT_HERO": (None, 1), "MAGIC_LEVEL": (None, 1),
    "MAGIC_POINT": (None, 1), "CHOICE": (None, 2), "FUEL": (None, 2), "L_TRACK": (None, 1),
    "ZONE_OBJ": ("obj", 1), "VAR_CUBE": ("var", 1), "VAR_GAME": ("var", 2),
    "USE_INVENTORY": ("item", 1), "L_TRACK_OBJ": ("obj", 1), "BODY": (None, 1),
    "BODY_OBJ": ("obj", 1), "ANIM": (None, 2), "ANIM_OBJ": ("obj", 2), "CARRY_BY": (None, 1),
    "CARRY_OBJ_BY": ("obj", 1), "CDROM": (None, 1), "ECHELLE": ("zone", 1), "RND": ("max", 1),
    "RAIL": ("zone", 1), "BETA": (None, 2), "BETA_OBJ": ("obj", 2), "ANGLE": ("obj", 2),
    "ANGLE_OBJ": ("obj", 2), "REAL_ANGLE": ("obj", 2), "DISTANCE_MESSAGE": ("obj", 2),
    "DEMO": (None, 1), "COL_DECORS": (None, 1), "COL_DECORS_OBJ": ("obj", 1),
    "PROCESSOR": (None, 1), "OBJECT_DISPLAYED": ("obj", 1),
}
# The compare value is signed for RET_S8 and RET_S16, unsigned for RET_U8.
FUNC_U8 = {"L_TRACK", "VAR_CUBE", "L_TRACK_OBJ", "RND", "COL_DECORS"}

# Track (TM_*): sizes agree between DoTrack and SearchOffsetTrack. The WAIT_NB_*
# timers carry 4 bytes of runtime state after their count; LOOP carries its
# initial count, the live counter, then the target.
TRACK = {
    "END": [], "NOP": [], "BODY": [("body", "B")], "ANIM": [("anim", "H")],
    "GOTO_POINT": [("point", "B")], "WAIT_ANIM": [],
    "LOOP": [("count", "B"), ("counter", "B"), ("to", "G")],
    "ANGLE": [("beta", "h")], "POS_POINT": [("point", "B")], "LABEL": [("label", "B")],
    "GOTO": [("to", "G")], "STOP": [], "GOTO_SYM_POINT": [("point", "B")],
    "WAIT_NB_ANIM": [("count", "B"), ("counter", "B")], "SAMPLE": [("sample", "h")],
    "GOTO_POINT_3D": [("point", "B")], "SPEED": [("speed", "h")],
    "BACKGROUND": [("on", "B")],
    "WAIT_NB_SECOND": [("seconds", "B"), ("timer", "I")], "NO_BODY": [],
    "BETA": [("beta", "h")], "OPEN_LEFT": [("dist", "h")], "OPEN_RIGHT": [("dist", "h")],
    "OPEN_UP": [("dist", "h")], "OPEN_DOWN": [("dist", "h")], "CLOSE": [], "WAIT_DOOR": [],
    "SAMPLE_RND": [("sample", "h")], "SAMPLE_ALWAYS": [("sample", "h")],
    "SAMPLE_STOP": [("sample", "h")], "PLAY_ACF": [("name", "Z")],
    "REPEAT_SAMPLE": [("repeat", "h")], "SIMPLE_SAMPLE": [("sample", "h")],
    "FACE_TWINSEN": [("state", "h")], "ANGLE_RND": [("beta", "h"), ("state", "h")],
    "REM": [], "WAIT_NB_DIZIEME": [("tenths", "B"), ("timer", "I")], "DO": [],
    "SPRITE": [("sprite", "h")],
    "WAIT_NB_SECOND_RND": [("seconds", "B"), ("timer", "I")], "AFF_TIMER": [],
    "SET_FRAME": [("frame", "B")], "SET_FRAME_3DS": [("frame", "B")],
    "SET_START_3DS": [("frame", "B")], "SET_END_3DS": [("frame", "B")],
    "START_ANIM_3DS": [("fps", "B")], "STOP_ANIM_3DS": [], "WAIT_ANIM_3DS": [],
    "WAIT_FRAME_3DS": [("frame", "B")],
    "WAIT_NB_DIZIEME_RND": [("tenths", "B"), ("timer", "I")],
    "DECALAGE": [("decalage", "h")], "FREQUENCE": [("frequence", "h")],
    "VOLUME": [("volume", "B")],
}

# LBA1 (lba1-classic 7404567). DoLife, DoFuncLife, DoTest and DoTrack read no
# operand for an opcode they have no case for (NOP, ENDIF).
LIFE1 = {
    "END": [], "NOP": [], "END_LIFE": [], "RETURN": [], "END_COMPORTEMENT": [], "ENDIF": [],
    "SNIF": [("cond", "C"), ("else", "J")],
    "OFFSET": [("to", "J")],
    "NEVERIF": [("cond", "C"), ("to", "J")],
    "LABEL": [("num", "B")],
    "IF": [("cond", "C"), ("else", "J")],
    "SWIF": [("cond", "C"), ("else", "J")],
    "ONEIF": [("cond", "C"), ("else", "J")],
    "OR_IF": [("cond", "C"), ("then", "J")],
    "ELSE": [("to", "J")],
    "BODY": [("body", "B")], "BODY_OBJ": [("obj", "B"), ("body", "B")],
    "ANIM": [("anim", "B")], "ANIM_OBJ": [("obj", "B"), ("anim", "B")],
    "ANIM_SET": [("anim", "B")],
    "SET_LIFE": [("life", "J")], "SET_LIFE_OBJ": [("obj", "B"), ("life", "h")],
    "SET_COMPORTEMENT": [("life", "J")], "SET_COMPORTEMENT_OBJ": [("obj", "B"), ("life", "h")],
    "SET_TRACK": [("track", "h")], "SET_TRACK_OBJ": [("obj", "B"), ("track", "h")],
    "MESSAGE": [("text", "h")], "MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "BIG_MESSAGE": [("text", "h")],
    "SAY_MESSAGE": [("text", "h")], "SAY_MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "FALLABLE": [("on", "B")],
    "SET_DIR": [("move", "M")], "SET_DIR_OBJ": [("obj", "B"), ("move", "M")],
    "CAM_FOLLOW": [("obj", "B")],
    "COMPORTEMENT_HERO": [("comportement", "B")],
    "SET_FLAG_CUBE": [("flag", "B"), ("value", "B")],
    "SET_FLAG_GAME": [("flag", "B"), ("value", "B")],
    "COMPORTEMENT": [("num", "B")],
    "KILL_OBJ": [("obj", "B")], "SUICIDE": [],
    "USE_ONE_LITTLE_KEY": [], "GIVE_GOLD_PIECES": [("count", "h")],
    "STOP_L_TRACK": [], "RESTORE_L_TRACK": [],
    "INC_CHAPTER": [], "FOUND_OBJECT": [("item", "B")],
    "SET_DOOR_LEFT": [("dist", "h")], "SET_DOOR_RIGHT": [("dist", "h")],
    "SET_DOOR_UP": [("dist", "h")], "SET_DOOR_DOWN": [("dist", "h")],
    "GIVE_BONUS": [("keep", "B")], "CHANGE_CUBE": [("cube", "B")],
    "OBJ_COL": [("on", "B")], "BRICK_COL": [("mode", "B")], "INVISIBLE": [("on", "B")],
    "ZOOM": [("on", "B")], "POS_POINT": [("point", "B")],
    "SET_MAGIC_LEVEL": [("level", "B")], "SUB_MAGIC_POINT": [("points", "B")],
    "SET_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "SUB_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "HIT_OBJ": [("obj", "B"), ("force", "B")],
    "PLAY_FLA": [("name", "Z")], "PLAY_MIDI": [("music", "B")],
    "INC_CLOVER_BOX": [], "SET_USED_INVENTORY": [("item", "B")],
    "ADD_CHOICE": [("text", "h")], "ASK_CHOICE": [("text", "h")],
    "ASK_CHOICE_OBJ": [("obj", "B"), ("text", "h")],
    "INIT_PINGOUIN": [("obj", "B")],
    "SET_HOLO_POS": [("pos", "B")], "CLR_HOLO_POS": [("pos", "B")],
    "ADD_FUEL": [("fuel", "B")], "SUB_FUEL": [("fuel", "B")],
    "SET_GRM": [("grm", "B")], "GRM_OFF": [],
    "FULL_POINT": [], "BETA": [("beta", "h")],
    "FADE_PAL_RED": [], "FADE_ALARM_RED": [], "FADE_ALARM_PAL": [], "FADE_RED_PAL": [],
    "FADE_RED_ALARM": [], "FADE_PAL_ALARM": [],
    "EXPLODE_OBJ": [("obj", "B")], "BULLE_ON": [], "BULLE_OFF": [],
    "SET_DARK_PAL": [], "SET_NORMAL_PAL": [], "MESSAGE_SENDELL": [],
    "HOLOMAP_TRAJ": [("traj", "B")],
    "GAME_OVER": [], "THE_END": [], "BRUTAL_EXIT": [], "MIDI_OFF": [],
    "PLAY_CD_TRACK": [("track", "B")],
    "PROJ_ISO": [], "PROJ_3D": [], "TEXT": [("text", "h")], "CLEAR_TEXT": [],
}

# LBA1 DoTest compares a byte as BYTE (signed) and a word as WORD.
FUNC1 = {
    "COL": (None, 1), "COL_OBJ": ("obj", 1), "DISTANCE": ("obj", 2), "ZONE": (None, 1),
    "ZONE_OBJ": ("obj", 1), "BODY": (None, 1), "BODY_OBJ": ("obj", 1), "ANIM": (None, 1),
    "ANIM_OBJ": ("obj", 1), "L_TRACK": (None, 1), "L_TRACK_OBJ": ("obj", 1),
    "FLAG_CUBE": ("flag", 1), "CONE_VIEW": ("obj", 2), "HIT_BY": (None, 1),
    "ACTION": (None, 1), "FLAG_GAME": ("flag", 1), "LIFE_POINT": (None, 1),
    "LIFE_POINT_OBJ": ("obj", 1), "NB_LITTLE_KEYS": (None, 1), "NB_GOLD_PIECES": (None, 2),
    "COMPORTEMENT_HERO": (None, 1), "CHAPTER": (None, 1), "DISTANCE_3D": ("obj", 2),
    "MAGIC_LEVEL": (None, 1), "MAGIC_POINT": (None, 1), "USE_INVENTORY": ("item", 1),
    "CHOICE": (None, 2), "FUEL": (None, 1), "CARRY_BY": (None, 1), "CDROM": (None, 1),
}

# LBA1 TM_LOOP restarts the Track at 0 and has no operands.
TRACK1 = {
    "END": [], "NOP": [], "BODY": [("body", "B")], "ANIM": [("anim", "B")],
    "GOTO_POINT": [("point", "B")], "WAIT_ANIM": [], "LOOP": [],
    "ANGLE": [("beta", "h")], "POS_POINT": [("point", "B")], "LABEL": [("label", "B")],
    "GOTO": [("to", "G")], "STOP": [], "GOTO_SYM_POINT": [("point", "B")],
    "WAIT_NB_ANIM": [("count", "B"), ("counter", "B")], "SAMPLE": [("sample", "h")],
    "GOTO_POINT_3D": [("point", "B")], "SPEED": [("speed", "h")],
    "BACKGROUND": [("on", "B")], "WAIT_NB_SECOND": [("seconds", "B"), ("timer", "I")],
    "NO_BODY": [], "BETA": [("beta", "h")],
    "OPEN_LEFT": [("dist", "h")], "OPEN_RIGHT": [("dist", "h")],
    "OPEN_UP": [("dist", "h")], "OPEN_DOWN": [("dist", "h")], "CLOSE": [], "WAIT_DOOR": [],
    "SAMPLE_RND": [("sample", "h")], "SAMPLE_ALWAYS": [("sample", "h")],
    "SAMPLE_STOP": [("sample", "h")], "PLAY_FLA": [("name", "Z")],
    "REPEAT_SAMPLE": [("repeat", "h")], "SIMPLE_SAMPLE": [("sample", "h")],
    "FACE_TWINKEL": [("state", "h")], "ANGLE_RND": [("beta", "h"), ("state", "h")],
}

# LBA1 COMMON.H numbers, in order from 0 (gaps marked "-").
LBA1_NUMBERS = {
    "LM_": """END NOP SNIF OFFSET NEVERIF - - - - - LABEL RETURN IF SWIF ONEIF ELSE ENDIF
        BODY BODY_OBJ ANIM ANIM_OBJ SET_LIFE SET_LIFE_OBJ SET_TRACK SET_TRACK_OBJ MESSAGE
        FALLABLE SET_DIR SET_DIR_OBJ CAM_FOLLOW COMPORTEMENT_HERO SET_FLAG_CUBE COMPORTEMENT
        SET_COMPORTEMENT SET_COMPORTEMENT_OBJ END_COMPORTEMENT SET_FLAG_GAME KILL_OBJ SUICIDE
        USE_ONE_LITTLE_KEY GIVE_GOLD_PIECES END_LIFE STOP_L_TRACK RESTORE_L_TRACK MESSAGE_OBJ
        INC_CHAPTER FOUND_OBJECT SET_DOOR_LEFT SET_DOOR_RIGHT SET_DOOR_UP SET_DOOR_DOWN
        GIVE_BONUS CHANGE_CUBE OBJ_COL BRICK_COL OR_IF INVISIBLE ZOOM POS_POINT SET_MAGIC_LEVEL
        SUB_MAGIC_POINT SET_LIFE_POINT_OBJ SUB_LIFE_POINT_OBJ HIT_OBJ PLAY_FLA PLAY_MIDI
        INC_CLOVER_BOX SET_USED_INVENTORY ADD_CHOICE ASK_CHOICE BIG_MESSAGE INIT_PINGOUIN
        SET_HOLO_POS CLR_HOLO_POS ADD_FUEL SUB_FUEL SET_GRM SAY_MESSAGE SAY_MESSAGE_OBJ
        FULL_POINT BETA GRM_OFF FADE_PAL_RED FADE_ALARM_RED FADE_ALARM_PAL FADE_RED_PAL
        FADE_RED_ALARM FADE_PAL_ALARM EXPLODE_OBJ BULLE_ON BULLE_OFF ASK_CHOICE_OBJ
        SET_DARK_PAL SET_NORMAL_PAL MESSAGE_SENDELL ANIM_SET HOLOMAP_TRAJ GAME_OVER THE_END
        MIDI_OFF PLAY_CD_TRACK PROJ_ISO PROJ_3D TEXT CLEAR_TEXT BRUTAL_EXIT""",
    "LF_": """COL COL_OBJ DISTANCE ZONE ZONE_OBJ BODY BODY_OBJ ANIM ANIM_OBJ L_TRACK L_TRACK_OBJ
        FLAG_CUBE CONE_VIEW HIT_BY ACTION FLAG_GAME LIFE_POINT LIFE_POINT_OBJ NB_LITTLE_KEYS
        NB_GOLD_PIECES COMPORTEMENT_HERO CHAPTER DISTANCE_3D MAGIC_LEVEL MAGIC_POINT
        USE_INVENTORY CHOICE FUEL CARRY_BY CDROM""",
    "LT_": "EQUAL SUP LESS SUP_EQUAL LESS_EQUAL DIFFERENT",
    "TM_": """END NOP BODY ANIM GOTO_POINT WAIT_ANIM LOOP ANGLE POS_POINT LABEL GOTO STOP
        GOTO_SYM_POINT WAIT_NB_ANIM SAMPLE GOTO_POINT_3D SPEED BACKGROUND WAIT_NB_SECOND
        NO_BODY BETA OPEN_LEFT OPEN_RIGHT OPEN_UP OPEN_DOWN CLOSE WAIT_DOOR SAMPLE_RND
        SAMPLE_ALWAYS SAMPLE_STOP PLAY_FLA REPEAT_SAMPLE SIMPLE_SAMPLE FACE_TWINKEL ANGLE_RND""",
    "MOVE_": "- MANUAL FOLLOW TRACK FOLLOW_2 TRACK_ATTACK SAME_XZ RANDOM",
}

SIZE = {"B": 1, "b": 1, "h": 2, "H": 2, "I": 4, "J": 2, "G": 2}
FMT = {"B": "<B", "b": "<b", "h": "<h", "H": "<H", "I": "<I", "J": "<h", "G": "<h"}


def load_defines():
    path = os.path.join(REPO, "SOURCES", "COMMON.H")
    defs = {}
    with open(path, encoding="latin-1") as f:
        for line in f:
            m = re.search(r"#define\s+((?:LM|LF|LT|TM|MOVE)_\w+)\s+(\d+)\b", line)
            if m:
                defs[m.group(1)] = int(m.group(2))
    return defs


def lba1_defines():
    defs = {}
    for prefix, names in LBA1_NUMBERS.items():
        for n, name in enumerate(names.split()):
            if name != "-":
                defs[prefix + name] = n
    return defs


def table(prefix, names):
    out = {}
    for name in names:
        key = prefix + name
        if key not in DEFS:
            sys.exit(f"{key} is not defined for {GAME}")
        out[DEFS[key]] = name
    return out


TEST_SYM = {"EQUAL": "==", "SUP": ">", "LESS": "<", "SUP_EQUAL": ">=", "LESS_EQUAL": "<=",
            "DIFFERENT": "!="}


def select_game(game):
    """Point the decoder at one game's tables."""
    global GAME, DEFS, LIFE, FUNC, TRACK, FUNC_U8, LIFE_BY_NUM, FUNC_BY_NUM, TRACK_BY_NUM
    global TEST_BY_NUM, MOVE_WITH_ARG, MOVE_BY_NUM
    GAME = game
    if game == "lba1":
        DEFS, LIFE, FUNC, TRACK, FUNC_U8 = lba1_defines(), LIFE1, FUNC1, TRACK1, set()
        moves = ("MOVE_FOLLOW",)  # DoLife reads Info3 for FOLLOW only
    else:
        DEFS, LIFE, FUNC, TRACK, FUNC_U8 = load_defines(), LIFE2, FUNC2, TRACK2, FUNC2_U8
        # AdjustDirObject (GERELIFE.CPP) reads Info3 for these moves.
        moves = ("MOVE_FOLLOW", "MOVE_SAME_XZ", "MOVE_SAME_XZ_BETA", "MOVE_CIRCLE",
                 "MOVE_CIRCLE2")
    LIFE_BY_NUM = table("LM_", LIFE)
    FUNC_BY_NUM = table("LF_", FUNC)
    TRACK_BY_NUM = table("TM_", TRACK)
    TEST_BY_NUM = {v: k[3:] for k, v in DEFS.items() if k.startswith("LT_")}
    MOVE_WITH_ARG = {DEFS[m] for m in moves}
    MOVE_BY_NUM = {v: k[5:] for k, v in DEFS.items() if k.startswith("MOVE_")}


LIFE2, FUNC2, TRACK2, FUNC2_U8 = LIFE, FUNC, TRACK, FUNC_U8
select_game("lba2")


class DecodeError(Exception):
    pass


def need(b, pc, n):
    if pc + n > len(b):
        raise DecodeError(f"operand runs past the end at {pc}")


def read_cond(b, pc, with_test):
    """DoFuncLife, then DoTest if with_test. Returns (text, width, pc)."""
    need(b, pc, 1)
    f = b[pc]
    pc += 1
    if f not in FUNC_BY_NUM:
        raise DecodeError(f"unknown condition {f} at {pc - 1}")
    name = FUNC_BY_NUM[f]
    param, width = FUNC[name]
    text = name
    if param:
        need(b, pc, 1)
        text += f"({b[pc]})"
        pc += 1
    if not with_test:
        return text, width, pc
    test, pc = read_test(b, pc, width, name in FUNC_U8)
    return f"{text} {test}", width, pc


def read_test(b, pc, width, unsigned):
    need(b, pc, 1 + width)
    t = b[pc]
    fmt = "<B" if width == 1 and unsigned else ("<b" if width == 1 else "<h")
    value = struct.unpack_from(fmt, b, pc + 1)[0]
    sym = TEST_SYM.get(TEST_BY_NUM.get(t, ""), f"?test{t}")
    return f"{sym} {value}", pc + 1 + width


def decode(b, kind):
    """Decode one script linearly. Returns [(offset, name, [(field, value, type)])]."""
    by_num, spec = (LIFE_BY_NUM, LIFE) if kind == "life" else (TRACK_BY_NUM, TRACK)
    pc, out, switch_width = 0, [], None
    while pc < len(b):
        start = pc
        op = b[pc]
        pc += 1
        if op not in by_num:
            raise DecodeError(f"unknown {kind} opcode {op} at {start}")
        name = by_num[op]
        args = []
        for field, t in spec[name]:
            if t in SIZE:
                need(b, pc, SIZE[t])
                args.append((field, struct.unpack_from(FMT[t], b, pc)[0], t))
                pc += SIZE[t]
            elif t == "Z":
                end = b.find(0, pc)
                if end < 0:
                    raise DecodeError(f"unterminated string at {pc}")
                args.append((field, b[pc:end].decode("latin-1"), t))
                pc = end + 1
            elif t == "C":
                text, _, pc = read_cond(b, pc, True)
                args.append((field, text, t))
            elif t == "F":
                text, switch_width, pc = read_cond(b, pc, False)
                args.append((field, text, t))
            elif t == "T":
                # ExeSwitch is one slot per object, not a stack: after a nested switch
                # ends, the outer CASEs compare at the inner switch's width.
                if switch_width is None:
                    raise DecodeError(f"{name} before any SWITCH at {start}")
                text, pc = read_test(b, pc, switch_width, False)
                args.append((field, text, t))
            elif t == "M":
                need(b, pc, 1)
                move = b[pc]
                pc += 1
                args.append((field, move, t))
                if move in MOVE_WITH_ARG:
                    need(b, pc, 1)
                    args.append(("info3", b[pc], "B"))
                    pc += 1
        out.append((start, name, args))
    return out


def check_jumps(ins, size):
    # Every retail script ends with END and has no other: a field read one byte
    # short leaves its zero high byte to decode as END, so this catches it.
    # LBA1 data also jumps to -1, which stops the script (GOTO -1, SET_COMPORTEMENT
    # -1); LBA2 data never does, so there it stays an error.
    nowhere = -1 if GAME == "lba1" else None
    for s, name, _ in ins[:-1]:
        if name == "END":
            raise DecodeError(f"END before the end of the script at {s}")
    if not ins or ins[-1][1] != "END":
        raise DecodeError("script does not end with END")
    starts = {s for s, _, _ in ins} | {size}
    for s, name, args in ins:
        for field, value, t in args:
            if t in "JG" and value not in starts and value != nowhere:
                raise DecodeError(f"{name} at {s} jumps to {value}, not an instruction start")


def scene_scripts(sc):
    """Yield (object, track bytes, life bytes) in LoadScene order (DISKFUNC.CPP)."""
    if GAME == "lba1":
        yield from scene_scripts_lba1(sc)
        return
    p = 6 + 1 + 4 + 5 * 2 * 4 + 4 + 1  # info world, cube count, light, ambiance, jingle
    p += 6  # hero start
    t, = struct.unpack_from("<H", sc, p)
    track = sc[p + 2:p + 2 + t]
    p += 2 + t
    n, = struct.unpack_from("<H", sc, p)
    yield 0, track, sc[p + 2:p + 2 + n]
    p += 2 + n
    nobj, = struct.unpack_from("<H", sc, p)
    p += 2
    anim_3ds = 1 << 18  # ANIM_3DS, COMMON.H
    for obj in range(1, nobj):
        flags, = struct.unpack_from("<I", sc, p)
        # Flags, file3d, body, anim, sprite, x y z, hitforce, optionflags, beta, srot,
        # move, info..info3, nbbonus, coulobj
        p += 4 + 2 + 1 + 2 + 2 + 6 + 1 + 2 + 2 + 2 + 1 + 8 + 2 + 1
        if flags & anim_3ds:
            p += 4 + 2
        p += 1 + 1  # armure, life points
        t, = struct.unpack_from("<H", sc, p)
        track = sc[p + 2:p + 2 + t]
        p += 2 + t
        n, = struct.unpack_from("<H", sc, p)
        yield obj, track, sc[p + 2:p + 2 + n]
        p += 2 + n


def scene_scripts_lba1(sc):
    """The same for LBA1: LoadScene, DISKFUNC.C."""
    p = 1 + 1 + 2 + 2  # island, game-over cube, two unused words
    p += 2 + 2 + 4 * 3 * 2 + 2 + 2 + 1  # light, 4 ambiances x (sample, repeat, rnd), seconds, jingle
    p += 6  # hero start
    t, = struct.unpack_from("<H", sc, p)
    track = sc[p + 2:p + 2 + t]
    p += 2 + t
    n, = struct.unpack_from("<H", sc, p)
    yield 0, track, sc[p + 2:p + 2 + n]
    p += 2 + n
    nobj, = struct.unpack_from("<H", sc, p)
    p += 2
    for obj in range(1, nobj):
        # Flags u16, file3d, body u8, anim u8, sprite, x y z, hitforce, optionflags, beta,
        # srot, move u16, info..info3, nbbonus u8, coulobj, armure, life points
        p += 2 + 2 + 1 + 1 + 2 + 6 + 1 + 2 + 2 + 2 + 2 + 8 + 1 + 1 + 1 + 1
        t, = struct.unpack_from("<H", sc, p)
        track = sc[p + 2:p + 2 + t]
        p += 2 + t
        n, = struct.unpack_from("<H", sc, p)
        yield obj, track, sc[p + 2:p + 2 + n]
        p += 2 + n


def scripts_of(sc):
    """scene_scripts as a list, or DecodeError if the layout does not fit the entry."""
    try:
        return list(scene_scripts(sc))
    except struct.error:
        raise DecodeError(f"the scene runs past the end of its entry; not an {GAME} scene?")


def decode_cube(sc):
    """Decode every script of one cube. Returns ({obj: (track_ins, life_ins)}, errors)."""
    objs, errors = {}, []
    try:
        scripts = scripts_of(sc)
    except DecodeError as e:
        return objs, [("-", "scene", len(sc), str(e))]
    for obj, track, life in scripts:
        pair = []
        for kind, b in (("track", track), ("life", life)):
            try:
                ins = decode(b, kind)
                check_jumps(ins, len(b))
                pair.append(ins)
            except DecodeError as e:
                pair.append(None)
                errors.append((obj, kind, len(b), str(e)))
        objs[obj] = tuple(pair)
    errors += [(o, "life", 0, e) for o, e in cross_check(objs)]
    return objs, errors


def cross_check(objs):
    """Offsets one script hands to another must land on an instruction start there."""
    starts = {o: tuple({s for s, _, _ in ins} if ins else None for ins in pair)
              for o, pair in objs.items()}
    errors = []
    for obj, (_, life) in objs.items():
        for s, name, args in life or []:
            if name == "SET_TRACK":
                target, kind, value = obj, 0, args[0][1]
            elif name in ("SET_TRACK_OBJ", "SET_COMPORTEMENT_OBJ", "SET_LIFE_OBJ"):
                target, value = args[0][1], args[1][1]
                kind = 0 if name == "SET_TRACK_OBJ" else 1
            else:
                continue
            if value == -1:
                continue
            if target not in starts:
                errors.append((obj, f"{name} at {s} names object {target}, which does not exist"))
            elif starts[target][kind] is not None and value not in starts[target][kind]:
                errors.append((obj, f"{name} at {s} points at {value}, not an instruction "
                                    f"start in object {target}"))
    return errors


def track_label(objs, obj, offset):
    track = objs.get(obj, (None, None))[0] or []
    for s, name, args in track:
        if s == offset and name == "LABEL":
            return f"  ; label {args[0][1]}"
    return ""


def entry_of(cube):
    return cube if GAME == "lba1" else cube + 1


def cubes(path):
    ents, data = hqr_inspect.entries(path)
    first = 0 if GAME == "lba1" else 1
    for i, off, size, csize, method in ents:
        if i < first or size is None:
            continue
        yield i - first, hqr_inspect.decompress_entry(data, off, size, csize, method)


def fmt_arg(field, value, t, labels):
    if t in "JG" and value in labels:
        return f"{field} {labels[value]}"
    if t == "M":
        return f"{field}={MOVE_BY_NUM.get(value, value)}"
    if t in "CFT":
        return str(value)
    if t == "Z":
        return f'{field}="{value}"'
    return f"{field}={value}"


def listing(ins, b, out, note=None):
    targets = sorted({v for _, _, a in ins for _, v, t in a if t in "JG"})
    labels = {v: f"L_{v:04d}" for v in targets}
    for s, name, args in ins:
        if s in labels:
            out.append(f"{labels[s]}:")
        text = " ".join(fmt_arg(f, v, t, labels) for f, v, t in args)
        extra = note(name, args) if note else ""
        out.append(f"  {s:5d}  {name} {text}".rstrip() + extra)
    if len(b) in labels:
        out.append(f"{labels[len(b)]}:  ; end")


def cmd_cube(path, num):
    for cube, sc in cubes(path):
        if cube != num:
            continue
        objs, errors = decode_cube(sc)
        out = [f"; cube {cube} (SCENE.HQR entry {entry_of(cube)})"]
        try:
            scripts = scripts_of(sc)
        except DecodeError as e:
            sys.exit(f"cube {cube}: {e}")
        for obj, track, life in scripts:

            def note(name, args, obj=obj):
                if name == "SET_TRACK":
                    return track_label(objs, obj, args[0][1])
                if name == "SET_TRACK_OBJ":
                    return track_label(objs, args[0][1], args[1][1])
                return ""

            out.append("")
            out.append(f"== object {obj}  track {len(track)} bytes  life {len(life)} bytes")
            for kind, b, ins in (("track", track, objs[obj][0]), ("life", life, objs[obj][1])):
                out.append(f"-- {kind}")
                if ins is None:
                    out.append("  ; did not decode, see --check")
                else:
                    listing(ins, b, out, note if kind == "life" else None)
        for obj, kind, _, err in errors:
            out.append(f"; object {obj} {kind}: {err}")
        print("\n".join(out))
        return 1 if errors else 0
    sys.exit(f"no cube {num} in {path}")


def walk_all(path):
    """Yield (cube, obj, kind, instructions) for every script that decodes."""
    for cube, sc in cubes(path):
        objs, _ = decode_cube(sc)
        for obj, (track, life) in objs.items():
            for kind, ins in (("track", track), ("life", life)):
                if ins is not None:
                    yield cube, obj, kind, ins


def cmd_check(path):
    counts = {"track": [0, 0], "life": [0, 0]}
    nscripts, bad_cubes, ncubes = 0, 0, 0
    for cube, sc in cubes(path):
        ncubes += 1
        objs, errors = decode_cube(sc)
        nscripts += len(objs)
        for obj, kind, size, err in errors:
            print(f"FAIL cube {cube} object {obj} {kind}: {err}")
        bad_cubes += bool(errors)
        for track, life in objs.values():
            for kind, ins in (("track", track), ("life", life)):
                if ins is not None:
                    counts[kind][0] += 1
                    counts[kind][1] += len(ins)
    for kind, (ok, n) in counts.items():
        print(f"{kind}: {ok}/{nscripts} scripts decoded, {n} instructions")
    print(f"cubes: {ncubes - bad_cubes}/{ncubes} clean")
    return 1 if bad_cubes else 0


def cmd_find(path, names):
    want = set()
    for n in names:
        n = n.upper()
        if not n.startswith(("LM_", "TM_")):
            sys.exit(f"--find takes LM_ or TM_ names, got {n}")
        if n[3:] not in (LIFE if n.startswith("LM_") else TRACK):
            sys.exit(f"{n} is not a known opcode")
        want.add(("life" if n.startswith("LM_") else "track", n[3:]))
    for cube, obj, kind, ins in walk_all(path):
        for s, name, args in ins:
            if (kind, name) in want:
                text = " ".join(fmt_arg(f, v, t, {}) for f, v, t in args)
                print(f"cube {cube} object {obj} {kind} @{s}: {name} {text}".rstrip())
    return 0


def cmd_sweep(path):
    """Change one operand size at a time and count how many changes --check catches.

    Each opcode the data uses gets an extra byte, its first byte field read as two,
    its first two-byte field read as one, and its last field dropped; each condition
    gets its compare width flipped and a parameter added or dropped.
    """
    scenes = [sc for _, sc in cubes(path)]

    def clean():
        try:
            return not any(decode_cube(sc)[1] for sc in scenes)
        except (DecodeError, IndexError, struct.error):
            return False

    if not clean():
        sys.exit("the data does not pass --check, so a sweep would mean nothing")
    used, funcs = set(), set()
    for sc in scenes:
        for track, life in decode_cube(sc)[0].values():
            used |= {("track", n) for _, n, _ in track} | {("life", n) for _, n, _ in life}
            funcs |= {v.split("(")[0].split(" ")[0] for _, _, a in life
                      for _, v, t in a if t in "CF"}
    caught, missed, total = 0, [], 0
    trials = []
    for kind, name in sorted(used):
        spec = LIFE if kind == "life" else TRACK
        orig = spec[name]
        muts = [("extra byte", orig + [("x", "B")])]
        for i, (f, t) in enumerate(orig):
            if t in "Bb":
                muts.append(("byte read as two", orig[:i] + [(f, "h")] + orig[i + 1:]))
                break
        for i, (f, t) in enumerate(orig):
            if t in "hHJG":
                muts.append(("two bytes read as one", orig[:i] + [(f, "B")] + orig[i + 1:]))
                break
        if orig:
            muts.append(("last field dropped", orig[:-1]))
        trials += [(spec, name, m, f"{kind} {name}: {tag}") for tag, m in muts]
    for name in sorted(funcs):
        param, width = FUNC[name]
        trials += [(FUNC, name, (param, 3 - width), f"condition {name}: width"),
                   (FUNC, name, (None if param else "x", width), f"condition {name}: parameter")]
    for spec, name, changed, label in trials:
        orig, spec[name] = spec[name], changed
        total += 1
        if clean():
            missed.append(label)
        else:
            caught += 1
        spec[name] = orig
    print(f"{len(used)} opcodes and {len(funcs)} conditions used; "
          f"{caught} of {total} changes caught")
    for label in missed:
        print(f"  not caught: {label}")
    for title, names in (("Life", set(LIFE) - {n for k, n in used if k == "life"}),
                         ("Track", set(TRACK) - {n for k, n in used if k == "track"}),
                         ("conditions", set(FUNC) - funcs)):
        print(f"unused {title}: {', '.join(sorted(names)) or 'none'}")
    return 0


TYPE_TEXT = {"B": "u8", "b": "s8", "h": "s16", "H": "u16", "I": "u32", "J": "s16 Life offset",
             "G": "s16 Track offset", "Z": "string", "C": "condition", "F": "condition, no test",
             "T": "test", "M": "move"}


def cmd_table():
    rows = []
    for title, prefix, spec in (("Life", "LM_", LIFE), ("Track", "TM_", TRACK)):
        rows += [f"### {title} opcodes", "", "| Code | Opcode | Operands |", "|---|---|---|"]
        for name, fields in sorted(spec.items(), key=lambda kv: DEFS[prefix + kv[0]]):
            ops = ", ".join(f"`{f}` {TYPE_TEXT[t]}" for f, t in fields) or "none"
            rows.append(f"| {DEFS[prefix + name]} | `{prefix}{name}` | {ops} |")
        rows.append("")
    rows += ["### Conditions", "", "| Code | Condition | Parameter | Compared as |", "|---|---|---|---|"]
    for name, (param, width) in sorted(FUNC.items(), key=lambda kv: DEFS["LF_" + kv[0]]):
        kind = "s16" if width == 2 else ("u8" if name in FUNC_U8 else "s8")
        rows.append(f"| {DEFS['LF_' + name]} | `LF_{name}` | {f'`{param}` u8' if param else 'none'} | {kind} |")
    print("\n".join(rows))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("hqr", nargs="?", help="path to SCENE.HQR")
    ap.add_argument("--game", choices=("lba2", "lba1"), default="lba2",
                    help="which game the SCENE.HQR is from (default lba2)")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--table", action="store_true", help="print the operand tables as Markdown")
    g.add_argument("--cube", type=int, help="list one cube's scripts")
    g.add_argument("--check", action="store_true", help="decode and verify every script")
    g.add_argument("--find", nargs="+", metavar="OP", help="list uses of LM_/TM_ opcodes")
    g.add_argument("--sweep", action="store_true",
                   help="give each used opcode a wrong size and count what --check catches")
    a = ap.parse_args()
    select_game(a.game)
    if a.table:
        return cmd_table()
    if not a.hqr:
        ap.error("SCENE.HQR is required")
    if a.check:
        return cmd_check(a.hqr)
    if a.find:
        return cmd_find(a.hqr, a.find)
    if a.sweep:
        return cmd_sweep(a.hqr)
    return cmd_cube(a.hqr, a.cube)


if __name__ == "__main__":
    sys.exit(main())
