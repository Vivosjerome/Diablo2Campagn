#ifndef MYTURN_OFFSETS_H
#define MYTURN_OFFSETS_H

/* D2R memory offsets for current game build */

#define OFF_UNIT_TABLE   0x1EAD470ULL
#define OFF_ROSTER       0x1EC3780ULL

#define OFF_UNIT_TYPE    0x00
#define OFF_UNIT_TXT     0x04
#define OFF_UNIT_ID      0x08
#define OFF_UNIT_MODE    0x0C
#define OFF_UNIT_PDATA   0x10
#define OFF_UNIT_PACT    0x20
#define OFF_UNIT_PPATH   0x38
#define OFF_UNIT_PSTATS  0x88
#define OFF_UNIT_PNEXT   0x158
#define OFF_UNIT_CORPSE  0x1A6

#define OFF_UI_STATES    0x1EBD158ULL

#define OFF_PATH_OFF_X   0x00
#define OFF_PATH_DYN_X   0x02
#define OFF_PATH_OFF_Y   0x04
#define OFF_PATH_DYN_Y   0x06
#define OFF_PATH_PROOM   0x20

#define OFF_ROOM_PROOM2  0x18
#define OFF_ROOM2_PLEVEL 0x90
#define OFF_LEVEL_ID     0x1F8

#define OFF_ACT_PMISC    0x70
#define OFF_ACTMISC_DIFF 0x830
#define OFF_ACTMISC_INIT 0x840
#define OFF_ACTMISC_END  0x860

/* Roster */
#define OFF_ROSTER_UNITID 0x48
#define OFF_ROSTER_AREA   0x5C
#define OFF_ROSTER_POS_X  0x60
#define OFF_ROSTER_POS_Y  0x64
#define OFF_ROSTER_NEXT   0x148

#endif
