/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		be_aas_bspq3.c
 *
 * desc:		BSP, Environment Sampling
 *
 * $Archive: /MissionPack/code/botlib/be_aas_bspq3.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_aas_def.h"

extern botlib_import_t botimport;

//#define TRACE_DEBUG

#define ON_EPSILON 0.005
//#define DEG2RAD( a ) (( a * M_PI ) / 180.0F)

#define MAX_BSPENTITIES 2048

typedef struct rgb_s {
	int red;
	int green;
	int blue;
} rgb_t;

// bsp entity epair
typedef struct bsp_epair_s {
	char *key;
	char *value;
	struct bsp_epair_s *next;
} bsp_epair_t;

// bsp data entity
typedef struct bsp_entity_s {
	bsp_epair_t *epairs;
} bsp_entity_t;

// id Software BSP data
typedef struct bsp_s {
	// true when bsp file is loaded
	int loaded;
	// entity data
	int entdatasize;
	char *dentdata;
	// bsp entities
	int numentities;
	bsp_entity_t entities[MAX_BSPENTITIES];
} bsp_t;

// global bsp
static bsp_t bspworld;

#ifdef BSP_DEBUG
typedef struct cname_s {
	int value;
	const char *name;
} cname_t;

static const cname_t contentnames[] = {{CONTENTS_SOLID, "CONTENTS_SOLID"},
									   {CONTENTS_WINDOW, "CONTENTS_WINDOW"},
									   {CONTENTS_AUX, "CONTENTS_AUX"},
									   {CONTENTS_LAVA, "CONTENTS_LAVA"},
									   {CONTENTS_SLIME, "CONTENTS_SLIME"},
									   {CONTENTS_WATER, "CONTENTS_WATER"},
									   {CONTENTS_MIST, "CONTENTS_MIST"},
									   {LAST_VISIBLE_CONTENTS, "LAST_VISIBLE_CONTENTS"},

									   {CONTENTS_AREAPORTAL, "CONTENTS_AREAPORTAL"},
									   {CONTENTS_PLAYERCLIP, "CONTENTS_PLAYERCLIP"},
									   {CONTENTS_MONSTERCLIP, "CONTENTS_MONSTERCLIP"},
									   {CONTENTS_CURRENT_0, "CONTENTS_CURRENT_0"},
									   {CONTENTS_CURRENT_90, "CONTENTS_CURRENT_90"},
									   {CONTENTS_CURRENT_180, "CONTENTS_CURRENT_180"},
									   {CONTENTS_CURRENT_270, "CONTENTS_CURRENT_270"},
									   {CONTENTS_CURRENT_UP, "CONTENTS_CURRENT_UP"},
									   {CONTENTS_CURRENT_DOWN, "CONTENTS_CURRENT_DOWN"},
									   {CONTENTS_ORIGIN, "CONTENTS_ORIGIN"},
									   {CONTENTS_MONSTER, "CONTENTS_MONSTER"},
									   {CONTENTS_DEADMONSTER, "CONTENTS_DEADMONSTER"},
									   {CONTENTS_DETAIL, "CONTENTS_DETAIL"},
									   {CONTENTS_TRANSLUCENT, "CONTENTS_TRANSLUCENT"},
									   {CONTENTS_LADDER, "CONTENTS_LADDER"},
									   {0, NULL}};

static void PrintContents(int contents) {
	int i;

	for (i = 0; contentnames[i].value; i++) {
		if (contents & contentnames[i].value) {
			botimport.Print(PRT_MESSAGE, "%s\n", contentnames[i].name);
		}
	}
}

#endif // BSP_DEBUG

//===========================================================================
// traces axial boxes of any size through the world
//===========================================================================
bsp_trace_t AAS_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
	bsp_trace_t bsptrace;
	botimport.Trace(&bsptrace, start, mins, maxs, end, passent, contentmask);
	return bsptrace;
}

//===========================================================================
// returns the contents at the given point
//===========================================================================
int AAS_PointContents(vec3_t point) {
	return botimport.PointContents(point);
}

qboolean AAS_EntityCollision(int entnum, vec3_t start, vec3_t boxmins, vec3_t boxmaxs, vec3_t end, int contentmask,
							 bsp_trace_t *trace) {
	bsp_trace_t enttrace;

	botimport.EntityTrace(&enttrace, start, boxmins, boxmaxs, end, entnum, contentmask);
	if (enttrace.fraction < trace->fraction) {
		Com_Memcpy(trace, &enttrace, sizeof(bsp_trace_t));
		return qtrue;
	}
	return qfalse;
}

//===========================================================================
// returns true if in Potentially Visible Set
//===========================================================================
qboolean AAS_inPVS(vec3_t p1, vec3_t p2) {
	return botimport.inPVS(p1, p2);
}

void AAS_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin) {
	botimport.BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);
}

//===========================================================================
// unlinks the entity from all leaves
//===========================================================================
void AAS_UnlinkFromBSPLeaves(bsp_link_t *leaves) {
}

bsp_link_t *AAS_BSPLinkEntity(vec3_t absmins, vec3_t absmaxs, int entnum, int modelnum) {
	return NULL;
}

int AAS_NextBSPEntity(int ent) {
	ent++;
	if (ent >= 1 && ent < bspworld.numentities)
		return ent;
	return 0;
}

static int AAS_BSPEntityInRange(int ent) {
	if (ent <= 0 || ent >= bspworld.numentities) {
		botimport.Print(PRT_MESSAGE, "bsp entity out of range\n");
		return qfalse;
	}
	return qtrue;
}

int AAS_ValueForBSPEpairKey(int ent, const char *key, char *value, int size) {
	bsp_epair_t *epair;

	value[0] = '\0';
	if (!AAS_BSPEntityInRange(ent))
		return qfalse;
	for (epair = bspworld.entities[ent].epairs; epair; epair = epair->next) {
		if (!strcmp(epair->key, key)) {
			Q_strncpyz(value, epair->value, size);
			return qtrue;
		}
	}
	return qfalse;
}

int AAS_VectorForBSPEpairKey(int ent, const char *key, vec3_t v) {
	char buf[MAX_EPAIRKEY];
	double v1, v2, v3;

	VectorClear(v);
	if (!AAS_ValueForBSPEpairKey(ent, key, buf, MAX_EPAIRKEY))
		return qfalse;
	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf(buf, "%lf %lf %lf", &v1, &v2, &v3);
	v[0] = v1;
	v[1] = v2;
	v[2] = v3;
	return qtrue;
}

int AAS_FloatForBSPEpairKey(int ent, const char *key, float *value) {
	char buf[MAX_EPAIRKEY];

	*value = 0;
	if (!AAS_ValueForBSPEpairKey(ent, key, buf, MAX_EPAIRKEY))
		return qfalse;
	*value = (float)atof(buf);
	return qtrue;
}

int AAS_IntForBSPEpairKey(int ent, const char *key, int *value) {
	char buf[MAX_EPAIRKEY];

	*value = 0;
	if (!AAS_ValueForBSPEpairKey(ent, key, buf, MAX_EPAIRKEY))
		return qfalse;
	*value = atoi(buf);
	return qtrue;
}

static void AAS_FreeBSPEntities(void) {
	int i;
	bsp_entity_t *ent;
	bsp_epair_t *epair, *nextepair;

	for (i = 1; i < bspworld.numentities; i++) {
		ent = &bspworld.entities[i];
		for (epair = ent->epairs; epair; epair = nextepair) {
			nextepair = epair->next;

			if (epair->key)
				FreeMemory(epair->key);
			if (epair->value)
				FreeMemory(epair->value);
			FreeMemory(epair);
		}
	}
	bspworld.numentities = 0;
}

static void AAS_ParseBSPEntities(void) {
	script_t *script;
	token_t token;
	bsp_entity_t *ent;
	bsp_epair_t *epair;

	script = LoadScriptMemory(bspworld.dentdata, bspworld.entdatasize, "entdata");
	SetScriptFlags(script, SCFL_NOSTRINGWHITESPACES | SCFL_NOSTRINGESCAPECHARS); // SCFL_PRIMITIVE);

	bspworld.numentities = 1;

	while (PS_ReadToken(script, &token)) {
		if (strcmp(token.string, "{") != 0) {
			ScriptError(script, "invalid %s", token.string);
			AAS_FreeBSPEntities();
			FreeScript(script);
			return;
		}
		if (bspworld.numentities >= MAX_BSPENTITIES) {
			botimport.Print(PRT_MESSAGE, "too many entities in BSP file\n");
			break;
		}
		ent = &bspworld.entities[bspworld.numentities];
		bspworld.numentities++;
		ent->epairs = NULL;
		while (PS_ReadToken(script, &token)) {
			if (!strcmp(token.string, "}"))
				break;
			epair = (bsp_epair_t *)GetClearedHunkMemory(sizeof(bsp_epair_t));
			epair->next = ent->epairs;
			ent->epairs = epair;
			if (token.type != TT_STRING) {
				ScriptError(script, "invalid %s", token.string);
				AAS_FreeBSPEntities();
				FreeScript(script);
				return;
			}
			StripDoubleQuotes(token.string);
			epair->key = (char *)GetHunkMemory(strlen(token.string) + 1);
			strcpy(epair->key, token.string);
			if (!PS_ExpectTokenType(script, TT_STRING, 0, &token)) {
				AAS_FreeBSPEntities();
				FreeScript(script);
				return;
			}
			StripDoubleQuotes(token.string);
			epair->value = (char *)GetHunkMemory(strlen(token.string) + 1);
			strcpy(epair->value, token.string);
		}
		if (strcmp(token.string, "}") != 0) {
			ScriptError(script, "missing }");
			AAS_FreeBSPEntities();
			FreeScript(script);
			return;
		}
	}
	FreeScript(script);
}

void AAS_DumpBSPData(void) {
	AAS_FreeBSPEntities();

	if (bspworld.dentdata)
		FreeMemory(bspworld.dentdata);
	bspworld.dentdata = NULL;
	bspworld.entdatasize = 0;

	bspworld.loaded = qfalse;
	Com_Memset(&bspworld, 0, sizeof(bspworld));
}

//===========================================================================
// load a .bsp file
//===========================================================================
int AAS_LoadBSPFile(void) {
	AAS_DumpBSPData();
	bspworld.entdatasize = (int)strlen(botimport.BSPEntityData()) + 1;
	bspworld.dentdata = (char *)GetClearedHunkMemory(bspworld.entdatasize);
	Com_Memcpy(bspworld.dentdata, botimport.BSPEntityData(), bspworld.entdatasize);
	AAS_ParseBSPEntities();
	bspworld.loaded = qtrue;
	return BLERR_NOERROR;
}
