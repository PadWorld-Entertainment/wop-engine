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
 * name:		be_aas_cluster.c
 *
 * desc:		area clustering
 *
 * $Archive: /MissionPack/code/botlib/be_aas_cluster.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_libvar.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_aas_def.h"
#include "be_aas_cluster.h"

extern botlib_import_t botimport;

#define AAS_MAX_PORTALS 65536
#define AAS_MAX_PORTALINDEXSIZE 65536
#define AAS_MAX_CLUSTERS 65536

#define MAX_PORTALAREAS 1024

// do not flood through area faces, only use reachabilities
static int nofaceflood = qtrue;

static void AAS_RemoveClusterAreas(void) {
	int i;

	for (i = 1; i < aasworld.numareas; i++) {
		aasworld.areasettings[i].cluster = 0;
	}
}

static int AAS_UpdatePortal(int areanum, int clusternum) {
	int portalnum;
	aas_portal_t *portal;
	aas_cluster_t *cluster;

	// find the portal of the area
	for (portalnum = 1; portalnum < aasworld.numportals; portalnum++) {
		if (aasworld.portals[portalnum].areanum == areanum)
			break;
	}

	if (portalnum == aasworld.numportals) {
		AAS_Error("no portal of area %d\n", areanum);
		return qtrue;
	}

	portal = &aasworld.portals[portalnum];
	// if the portal is already fully updated
	if (portal->frontcluster == clusternum)
		return qtrue;
	if (portal->backcluster == clusternum)
		return qtrue;
	// if the portal has no front cluster yet
	if (!portal->frontcluster) {
		portal->frontcluster = clusternum;
	}
	// if the portal has no back cluster yet
	else if (!portal->backcluster) {
		portal->backcluster = clusternum;
	} else {
		// remove the cluster portal flag contents
		aasworld.areasettings[areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
		Log_Write("portal area %d is separating more than two clusters\r\n", areanum);
		return qfalse;
	}
	if (aasworld.portalindexsize >= AAS_MAX_PORTALINDEXSIZE) {
		AAS_Error("AAS_MAX_PORTALINDEXSIZE\n");
		return qtrue;
	}
	// set the area cluster number to the negative portal number
	aasworld.areasettings[areanum].cluster = -portalnum;
	// add the portal to the cluster using the portal index
	cluster = &aasworld.clusters[clusternum];
	aasworld.portalindex[cluster->firstportal + cluster->numportals] = portalnum;
	aasworld.portalindexsize++;
	cluster->numportals++;
	return qtrue;
}

static int AAS_FloodClusterAreas_r(int areanum, int clusternum) {
	aas_area_t *area;
	aas_face_t *face;
	int facenum, i;

	if (areanum <= 0 || areanum >= aasworld.numareas) {
		AAS_Error("AAS_FloodClusterAreas_r: areanum out of range\n");
		return qfalse;
	}
	// if the area is already part of a cluster
	if (aasworld.areasettings[areanum].cluster > 0) {
		if (aasworld.areasettings[areanum].cluster == clusternum)
			return qtrue;

		// there's a reachability going from one cluster to another only in one direction

		AAS_Error("cluster %d touched cluster %d at area %d\n", clusternum, aasworld.areasettings[areanum].cluster,
				  areanum);
		return qfalse;
	}
	// don't add the cluster portal areas to the clusters
	if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL) {
		return AAS_UpdatePortal(areanum, clusternum);
	}
	// set the area cluster number
	aasworld.areasettings[areanum].cluster = clusternum;
	aasworld.areasettings[areanum].clusterareanum = aasworld.clusters[clusternum].numareas;
	// the cluster has an extra area
	aasworld.clusters[clusternum].numareas++;

	area = &aasworld.areas[areanum];
	// use area faces to flood into adjacent areas
	if (!nofaceflood) {
		for (i = 0; i < area->numfaces; i++) {
			facenum = abs(aasworld.faceindex[area->firstface + i]);
			face = &aasworld.faces[facenum];
			if (face->frontarea == areanum) {
				if (face->backarea)
					if (!AAS_FloodClusterAreas_r(face->backarea, clusternum))
						return qfalse;
			} else {
				if (face->frontarea)
					if (!AAS_FloodClusterAreas_r(face->frontarea, clusternum))
						return qfalse;
			}
		}
	}
	// use the reachabilities to flood into other areas
	for (i = 0; i < aasworld.areasettings[areanum].numreachableareas; i++) {
		if (!aasworld.reachability[aasworld.areasettings[areanum].firstreachablearea + i].areanum) {
			continue;
		}
		if (!AAS_FloodClusterAreas_r(
				aasworld.reachability[aasworld.areasettings[areanum].firstreachablearea + i].areanum, clusternum))
			return qfalse;
	}
	return qtrue;
}
//===========================================================================
// try to flood from all areas without cluster into areas with a cluster set

//===========================================================================
static int AAS_FloodClusterAreasUsingReachabilities(int clusternum) {
	int i, j, areanum;

	for (i = 1; i < aasworld.numareas; i++) {
		// if this area already has a cluster set
		if (aasworld.areasettings[i].cluster)
			continue;
		// if this area is a cluster portal
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
			continue;
		// loop over the reachable areas from this area
		for (j = 0; j < aasworld.areasettings[i].numreachableareas; j++) {
			// the reachable area
			areanum = aasworld.reachability[aasworld.areasettings[i].firstreachablearea + j].areanum;
			// if this area is a cluster portal
			if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL)
				continue;
			// if this area has a cluster set
			if (aasworld.areasettings[areanum].cluster) {
				if (!AAS_FloodClusterAreas_r(i, clusternum))
					return qfalse;
				i = 0;
				break;
			}
		}
	}
	return qtrue;
}

static void AAS_NumberClusterAreas(int clusternum) {
	int i, portalnum;
	aas_cluster_t *cluster;
	aas_portal_t *portal;

	aasworld.clusters[clusternum].numareas = 0;
	aasworld.clusters[clusternum].numreachabilityareas = 0;
	// number all areas in this cluster WITH reachabilities
	for (i = 1; i < aasworld.numareas; i++) {

		if (aasworld.areasettings[i].cluster != clusternum)
			continue;

		if (!AAS_AreaReachability(i))
			continue;

		aasworld.areasettings[i].clusterareanum = aasworld.clusters[clusternum].numareas;
		// the cluster has an extra area
		aasworld.clusters[clusternum].numareas++;
		aasworld.clusters[clusternum].numreachabilityareas++;
	}
	// number all portals in this cluster WITH reachabilities
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++) {
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		portal = &aasworld.portals[portalnum];
		if (!AAS_AreaReachability(portal->areanum))
			continue;
		if (portal->frontcluster == clusternum) {
			portal->clusterareanum[0] = cluster->numareas++;
			aasworld.clusters[clusternum].numreachabilityareas++;
		} else {
			portal->clusterareanum[1] = cluster->numareas++;
			aasworld.clusters[clusternum].numreachabilityareas++;
		}
	}
	// number all areas in this cluster WITHOUT reachabilities
	for (i = 1; i < aasworld.numareas; i++) {

		if (aasworld.areasettings[i].cluster != clusternum)
			continue;

		if (AAS_AreaReachability(i))
			continue;

		aasworld.areasettings[i].clusterareanum = aasworld.clusters[clusternum].numareas;
		// the cluster has an extra area
		aasworld.clusters[clusternum].numareas++;
	}
	// number all portals in this cluster WITHOUT reachabilities
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numportals; i++) {
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		portal = &aasworld.portals[portalnum];
		if (AAS_AreaReachability(portal->areanum))
			continue;
		if (portal->frontcluster == clusternum) {
			portal->clusterareanum[0] = cluster->numareas++;
		} else {
			portal->clusterareanum[1] = cluster->numareas++;
		}
	}
}

static int AAS_FindClusters(void) {
	int i;
	aas_cluster_t *cluster;

	AAS_RemoveClusterAreas();

	for (i = 1; i < aasworld.numareas; i++) {
		// if the area is already part of a cluster
		if (aasworld.areasettings[i].cluster)
			continue;
		// if not flooding through faces only use areas that have reachabilities
		if (nofaceflood) {
			if (!aasworld.areasettings[i].numreachableareas)
				continue;
		}
		// if the area is a cluster portal
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)
			continue;
		if (aasworld.numclusters >= AAS_MAX_CLUSTERS) {
			AAS_Error("AAS_MAX_CLUSTERS\n");
			return qfalse;
		}
		cluster = &aasworld.clusters[aasworld.numclusters];
		cluster->numareas = 0;
		cluster->numreachabilityareas = 0;
		cluster->firstportal = aasworld.portalindexsize;
		cluster->numportals = 0;
		// flood the areas in this cluster
		if (!AAS_FloodClusterAreas_r(i, aasworld.numclusters))
			return qfalse;
		if (!AAS_FloodClusterAreasUsingReachabilities(aasworld.numclusters))
			return qfalse;
		// number the cluster areas
		AAS_NumberClusterAreas(aasworld.numclusters);
		// Log_Write("cluster %d has %d areas\r\n", aasworld.numclusters, cluster->numareas);
		aasworld.numclusters++;
	}
	return qtrue;
}

static void AAS_CreatePortals(void) {
	int i;
	aas_portal_t *portal;

	for (i = 1; i < aasworld.numareas; i++) {
		// if the area is a cluster portal
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL) {
			if (aasworld.numportals >= AAS_MAX_PORTALS) {
				AAS_Error("AAS_MAX_PORTALS\n");
				return;
			}
			portal = &aasworld.portals[aasworld.numportals];
			portal->areanum = i;
			portal->frontcluster = 0;
			portal->backcluster = 0;
			aasworld.numportals++;
		}
	}
}

static void AAS_ConnectedAreas_r(int *areanums, int numareas, int *connectedareas, int curarea) {
	int i, j, otherareanum, facenum;
	aas_area_t *area;
	aas_face_t *face;

	connectedareas[curarea] = qtrue;
	area = &aasworld.areas[areanums[curarea]];
	for (i = 0; i < area->numfaces; i++) {
		facenum = abs(aasworld.faceindex[area->firstface + i]);
		face = &aasworld.faces[facenum];
		// if the face is solid
		if (face->faceflags & FACE_SOLID)
			continue;
		// get the area at the other side of the face
		if (face->frontarea != areanums[curarea])
			otherareanum = face->frontarea;
		else
			otherareanum = face->backarea;
		// check if the face is leading to one of the other areas
		for (j = 0; j < numareas; j++) {
			if (areanums[j] == otherareanum)
				break;
		}
		// if the face isn't leading to one of the other areas
		if (j == numareas)
			continue;
		// if the other area is already connected
		if (connectedareas[j])
			continue;
		// recursively proceed with the other area
		AAS_ConnectedAreas_r(areanums, numareas, connectedareas, j);
	}
}

static qboolean AAS_ConnectedAreas(int *areanums, int numareas) {
	int connectedareas[MAX_PORTALAREAS], i;

	Com_Memset(connectedareas, 0, sizeof(connectedareas));
	if (numareas < 1)
		return qfalse;
	if (numareas == 1)
		return qtrue;
	AAS_ConnectedAreas_r(areanums, numareas, connectedareas, 0);
	for (i = 0; i < numareas; i++) {
		if (!connectedareas[i])
			return qfalse;
	}
	return qtrue;
}
//===========================================================================
// gets adjacent areas with less presence types recursively

//===========================================================================
static int AAS_GetAdjacentAreasWithLessPresenceTypes_r(int *areanums, int numareas, int curareanum) {
	int i, j, presencetype, otherpresencetype, otherareanum, facenum;
	aas_area_t *area;
	aas_face_t *face;

	areanums[numareas++] = curareanum;
	area = &aasworld.areas[curareanum];
	presencetype = aasworld.areasettings[curareanum].presencetype;
	for (i = 0; i < area->numfaces; i++) {
		facenum = abs(aasworld.faceindex[area->firstface + i]);
		face = &aasworld.faces[facenum];
		// if the face is solid
		if (face->faceflags & FACE_SOLID)
			continue;
		// the area at the other side of the face
		if (face->frontarea != curareanum)
			otherareanum = face->frontarea;
		else
			otherareanum = face->backarea;

		otherpresencetype = aasworld.areasettings[otherareanum].presencetype;
		// if the other area has less presence types
		if ((presencetype & ~otherpresencetype) && !(otherpresencetype & ~presencetype)) {
			// check if the other area isn't already in the list
			for (j = 0; j < numareas; j++) {
				if (otherareanum == areanums[j])
					break;
			}
			// if the other area isn't already in the list
			if (j == numareas) {
				if (numareas >= MAX_PORTALAREAS) {
					AAS_Error("MAX_PORTALAREAS\n");
					return numareas;
				}
				numareas = AAS_GetAdjacentAreasWithLessPresenceTypes_r(areanums, numareas, otherareanum);
			}
		}
	}
	return numareas;
}

static int AAS_CheckAreaForPossiblePortals(int areanum) {
	int i, j, k, fen, ben, frontedgenum, backedgenum, facenum;
	int areanums[MAX_PORTALAREAS], numareas, otherareanum;
	int numareafrontfaces[MAX_PORTALAREAS], numareabackfaces[MAX_PORTALAREAS];
	int frontfacenums[MAX_PORTALAREAS], backfacenums[MAX_PORTALAREAS];
	int numfrontfaces, numbackfaces;
	int frontareanums[MAX_PORTALAREAS], backareanums[MAX_PORTALAREAS];
	int numfrontareas, numbackareas;
	int frontplanenum, backplanenum, faceplanenum;
	aas_area_t *area;
	aas_face_t *frontface, *backface, *face;

	// if it isn't already a portal
	if (aasworld.areasettings[areanum].contents & AREACONTENTS_CLUSTERPORTAL)
		return 0;
	// it must be a grounded area
	if (!(aasworld.areasettings[areanum].areaflags & AREA_GROUNDED))
		return 0;

	Com_Memset(numareafrontfaces, 0, sizeof(numareafrontfaces));
	Com_Memset(numareabackfaces, 0, sizeof(numareabackfaces));
	numfrontfaces = numbackfaces = 0;
	numfrontareas = numbackareas = 0;
	frontplanenum = backplanenum = -1;
	// add any adjacent areas with less presence types
	numareas = AAS_GetAdjacentAreasWithLessPresenceTypes_r(areanums, 0, areanum);

	for (i = 0; i < numareas; i++) {
		area = &aasworld.areas[areanums[i]];
		for (j = 0; j < area->numfaces; j++) {
			facenum = abs(aasworld.faceindex[area->firstface + j]);
			face = &aasworld.faces[facenum];
			// if the face is solid
			if (face->faceflags & FACE_SOLID)
				continue;
			// check if the face is shared with one of the other areas
			for (k = 0; k < numareas; k++) {
				if (k == i)
					continue;
				if (face->frontarea == areanums[k] || face->backarea == areanums[k])
					break;
			}
			// if the face is shared
			if (k != numareas)
				continue;
			// the number of the area at the other side of the face
			if (face->frontarea == areanums[i])
				otherareanum = face->backarea;
			else
				otherareanum = face->frontarea;
			// if the other area already is a cluter portal
			if (aasworld.areasettings[otherareanum].contents & AREACONTENTS_CLUSTERPORTAL)
				return 0;
			// number of the plane of the area
			faceplanenum = face->planenum & ~1;

			if (frontplanenum < 0 || faceplanenum == frontplanenum) {
				frontplanenum = faceplanenum;
				frontfacenums[numfrontfaces++] = facenum;
				for (k = 0; k < numfrontareas; k++) {
					if (frontareanums[k] == otherareanum)
						break;
				}
				if (k == numfrontareas)
					frontareanums[numfrontareas++] = otherareanum;
				numareafrontfaces[i]++;
			} else if (backplanenum < 0 || faceplanenum == backplanenum) {
				backplanenum = faceplanenum;
				backfacenums[numbackfaces++] = facenum;
				for (k = 0; k < numbackareas; k++) {
					if (backareanums[k] == otherareanum)
						break;
				}
				if (k == numbackareas)
					backareanums[numbackareas++] = otherareanum;
				numareabackfaces[i]++;
			} else {
				return 0;
			}
		}
	}
	// every area should have at least one front face and one back face
	for (i = 0; i < numareas; i++) {
		if (!numareafrontfaces[i] || !numareabackfaces[i])
			return 0;
	}
	// the front areas should all be connected
	if (!AAS_ConnectedAreas(frontareanums, numfrontareas))
		return 0;
	// the back areas should all be connected
	if (!AAS_ConnectedAreas(backareanums, numbackareas))
		return 0;
	// none of the front faces should have a shared edge with a back face
	for (i = 0; i < numfrontfaces; i++) {
		frontface = &aasworld.faces[frontfacenums[i]];
		for (fen = 0; fen < frontface->numedges; fen++) {
			frontedgenum = abs(aasworld.edgeindex[frontface->firstedge + fen]);
			for (j = 0; j < numbackfaces; j++) {
				backface = &aasworld.faces[backfacenums[j]];
				for (ben = 0; ben < backface->numedges; ben++) {
					backedgenum = abs(aasworld.edgeindex[backface->firstedge + ben]);
					if (frontedgenum == backedgenum)
						break;
				}
				if (ben != backface->numedges)
					break;
			}
			if (j != numbackfaces)
				break;
		}
		if (fen != frontface->numedges)
			break;
	}
	if (i != numfrontfaces)
		return 0;
	// set the cluster portal contents
	for (i = 0; i < numareas; i++) {
		aasworld.areasettings[areanums[i]].contents |= AREACONTENTS_CLUSTERPORTAL;
		// this area can be used as a route portal
		aasworld.areasettings[areanums[i]].contents |= AREACONTENTS_ROUTEPORTAL;
		Log_Write("possible portal: %d\r\n", areanums[i]);
	}

	return numareas;
}

static void AAS_FindPossiblePortals(void) {
	int i, numpossibleportals;

	numpossibleportals = 0;
	for (i = 1; i < aasworld.numareas; i++) {
		numpossibleportals += AAS_CheckAreaForPossiblePortals(i);
	}
	botimport.Print(PRT_MESSAGE, "\r%6d possible portal areas\n", numpossibleportals);
}

static int AAS_TestPortals(void) {
	int i;
	aas_portal_t *portal;

	for (i = 1; i < aasworld.numportals; i++) {
		portal = &aasworld.portals[i];
		if (!portal->frontcluster) {
			aasworld.areasettings[portal->areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
			Log_Write("portal area %d has no front cluster\r\n", portal->areanum);
			return qfalse;
		}
		if (!portal->backcluster) {
			aasworld.areasettings[portal->areanum].contents &= ~AREACONTENTS_CLUSTERPORTAL;
			Log_Write("portal area %d has no back cluster\r\n", portal->areanum);
			return qfalse;
		}
	}
	return qtrue;
} // end of the function

static void AAS_CountForcedClusterPortals(void) {
	int num, i;

	num = 0;
	for (i = 1; i < aasworld.numareas; i++) {
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL) {
			Log_Write("area %d is a forced portal area\r\n", i);
			num++;
		}
	}
	botimport.Print(PRT_MESSAGE, "%6d forced portal areas\n", num);
}

static void AAS_CreateViewPortals(void) {
	int i;

	for (i = 1; i < aasworld.numareas; i++) {
		if (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL) {
			aasworld.areasettings[i].contents |= AREACONTENTS_VIEWPORTAL;
		}
	}
}

static void AAS_SetViewPortalsAsClusterPortals(void) {
	int i;

	for (i = 1; i < aasworld.numareas; i++) {
		if (aasworld.areasettings[i].contents & AREACONTENTS_VIEWPORTAL) {
			aasworld.areasettings[i].contents |= AREACONTENTS_CLUSTERPORTAL;
		}
	}
}

void AAS_InitClustering(void) {
	int i, removedPortalAreas;
	int n, total, numreachabilityareas;

	if (!aasworld.loaded)
		return;
	// if there are clusters
	if (aasworld.numclusters >= 1) {
#ifndef BSPC
		// if clustering isn't forced
		if (!((int)LibVarGetValue("forceclustering")) && !((int)LibVarGetValue("forcereachability")))
			return;
#endif
	}
	// set all view portals as cluster portals in case we re-calculate the reachabilities and clusters (with -reach)
	AAS_SetViewPortalsAsClusterPortals();
	// count the number of forced cluster portals
	AAS_CountForcedClusterPortals();
	// remove all area cluster marks
	AAS_RemoveClusterAreas();
	// find possible cluster portals
	AAS_FindPossiblePortals();
	// craete portals to for the bot view
	AAS_CreateViewPortals();
	// remove all portals that are not closing a cluster
	// AAS_RemoveNotClusterClosingPortals();
	// initialize portal memory
	if (aasworld.portals)
		FreeMemory(aasworld.portals);
	aasworld.portals = (aas_portal_t *)GetClearedMemory(AAS_MAX_PORTALS * sizeof(aas_portal_t));
	// initialize portal index memory
	if (aasworld.portalindex)
		FreeMemory(aasworld.portalindex);
	aasworld.portalindex = (aas_portalindex_t *)GetClearedMemory(AAS_MAX_PORTALINDEXSIZE * sizeof(aas_portalindex_t));
	// initialize cluster memory
	if (aasworld.clusters)
		FreeMemory(aasworld.clusters);
	aasworld.clusters = (aas_cluster_t *)GetClearedMemory(AAS_MAX_CLUSTERS * sizeof(aas_cluster_t));

	removedPortalAreas = 0;
	botimport.Print(PRT_MESSAGE, "\r%6d removed portal areas", removedPortalAreas);
	while (1) {
		botimport.Print(PRT_MESSAGE, "\r%6d", removedPortalAreas);
		// initialize the number of portals and clusters
		aasworld.numportals = 1; // portal 0 is a dummy
		aasworld.portalindexsize = 0;
		aasworld.numclusters = 1; // cluster 0 is a dummy
		// create the portals from the portal areas
		AAS_CreatePortals();

		removedPortalAreas++;
		// find the clusters
		if (!AAS_FindClusters())
			continue;
		// test the portals
		if (!AAS_TestPortals())
			continue;

		break;
	}
	botimport.Print(PRT_MESSAGE, "\n");
	// the AAS file should be saved
	aasworld.savefile = qtrue;
	// write the portal areas to the log file
	for (i = 1; i < aasworld.numportals; i++) {
		Log_Write("portal %d: area %d\r\n", i, aasworld.portals[i].areanum);
	}
	// report cluster info
	botimport.Print(PRT_MESSAGE, "%6d portals created\n", aasworld.numportals);
	botimport.Print(PRT_MESSAGE, "%6d clusters created\n", aasworld.numclusters);
	for (i = 1; i < aasworld.numclusters; i++) {
		botimport.Print(PRT_MESSAGE, "cluster %d has %d reachability areas\n", i,
						aasworld.clusters[i].numreachabilityareas);
	}
	// report AAS file efficiency
	numreachabilityareas = 0;
	total = 0;
	for (i = 0; i < aasworld.numclusters; i++) {
		n = aasworld.clusters[i].numreachabilityareas;
		numreachabilityareas += n;
		total += n * n;
	}
	total += numreachabilityareas * aasworld.numportals;

	botimport.Print(PRT_MESSAGE, "%6i total reachability areas\n", numreachabilityareas);
	botimport.Print(PRT_MESSAGE, "%6i AAS memory/CPU usage (the lower the better)\n", total * 3);
}
