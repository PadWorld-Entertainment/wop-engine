//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#include "tr_surfaceanim.h"
#include "tr_backend.h"
#include "tr_globals.h"

void RB_MDRSurfaceAnim(mdrSurface_t *surface) {
	int j, k;
	float backlerp;
	mdrHeader_t *header;
	mdrBone_t bones[MDR_MAX_BONES], *bonePtr;
	int frameSize;
	int *triangles;
	int indexes;
	int baseIndex;
	int baseVertex;
	mdrFrame_t *frame;
	mdrFrame_t *oldFrame;
	int numVerts;
	mdrVertex_t *v;

	// don't lerp if lerping off, or this is the only frame, or the last frame...
	if (backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame) {
		backlerp = 0; // if backlerp is 0, lerping is off and frontlerp is never used
	} else {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	header = (mdrHeader_t *)((unsigned char *)surface + surface->ofsHeader);
	frameSize = (size_t)(&((mdrFrame_t *)0)->bones[header->numBones]);
	frame = (mdrFrame_t *)((byte *)header + header->ofsFrames + backEnd.currentEntity->e.frame * frameSize);
	oldFrame = (mdrFrame_t *)((byte *)header + header->ofsFrames + backEnd.currentEntity->e.oldframe * frameSize);

	RB_CHECKOVERFLOW(surface->numVerts, surface->numTriangles * 3);

	triangles = (int *)((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	baseIndex = tess.numIndexes;
	baseVertex = tess.numVertexes;

	// Set up all triangles.
	for (j = 0; j < indexes; j++) {
		tess.indexes[baseIndex + j] = baseVertex + triangles[j];
	}
	tess.numIndexes += indexes;

	//
	// lerp all the needed bones
	//
	if (!backlerp) {
		// no lerping needed
		bonePtr = frame->bones;
	} else {
		int nBones = header->numBones;
		int n;
		float tmp;
		bonePtr = bones;
		for (n = 0; n < nBones; n++) {
			tmp = frame->bones[n].matrix[0][0];
			bones[n].matrix[0][0] = tmp + backlerp * (oldFrame->bones[n].matrix[0][0] - tmp);
			tmp = frame->bones[n].matrix[0][1];
			bones[n].matrix[0][1] = tmp + backlerp * (oldFrame->bones[n].matrix[0][1] - tmp);
			tmp = frame->bones[n].matrix[0][2];
			bones[n].matrix[0][2] = tmp + backlerp * (oldFrame->bones[n].matrix[0][2] - tmp);
			tmp = frame->bones[n].matrix[0][3];
			bones[n].matrix[0][3] = tmp + backlerp * (oldFrame->bones[n].matrix[0][3] - tmp);

			tmp = frame->bones[n].matrix[1][0];
			bones[n].matrix[1][0] = tmp + backlerp * (oldFrame->bones[n].matrix[1][0] - tmp);
			tmp = frame->bones[n].matrix[1][1];
			bones[n].matrix[1][1] = tmp + backlerp * (oldFrame->bones[n].matrix[1][1] - tmp);
			tmp = frame->bones[n].matrix[1][2];
			bones[n].matrix[1][2] = tmp + backlerp * (oldFrame->bones[n].matrix[1][2] - tmp);
			tmp = frame->bones[n].matrix[1][3];
			bones[n].matrix[1][3] = tmp + backlerp * (oldFrame->bones[n].matrix[1][3] - tmp);

			tmp = frame->bones[n].matrix[2][0];
			bones[n].matrix[2][0] = tmp + backlerp * (oldFrame->bones[n].matrix[2][0] - tmp);
			tmp = frame->bones[n].matrix[2][1];
			bones[n].matrix[2][1] = tmp + backlerp * (oldFrame->bones[n].matrix[2][1] - tmp);
			tmp = frame->bones[n].matrix[2][2];
			bones[n].matrix[2][2] = tmp + backlerp * (oldFrame->bones[n].matrix[2][2] - tmp);
			tmp = frame->bones[n].matrix[2][3];
			bones[n].matrix[2][3] = tmp + backlerp * (oldFrame->bones[n].matrix[2][3] - tmp);
		}
	}

	//
	// deform the vertexes by the lerped bones
	//
	v = (mdrVertex_t *)((unsigned char *)surface + surface->ofsVerts);
	numVerts = surface->numVerts;
	for (j = 0; j < numVerts; j++) {
		float tempVert[3] = {0, 0, 0};
		float tempNormal[3] = {0, 0, 0};

		mdrWeight_t *w = v->weights;

		for (k = 0; k < v->numWeights; k++, w++) {
			mdrBone_t *bone = bonePtr + w->boneIndex;

			tempVert[0] += w->boneWeight * (DotProduct(bone->matrix[0], w->offset) + bone->matrix[0][3]);
			tempVert[1] += w->boneWeight * (DotProduct(bone->matrix[1], w->offset) + bone->matrix[1][3]);
			tempVert[2] += w->boneWeight * (DotProduct(bone->matrix[2], w->offset) + bone->matrix[2][3]);

			tempNormal[0] += w->boneWeight * DotProduct(bone->matrix[0], v->normal);
			tempNormal[1] += w->boneWeight * DotProduct(bone->matrix[1], v->normal);
			tempNormal[2] += w->boneWeight * DotProduct(bone->matrix[2], v->normal);
		}

		tess.xyz[baseVertex + j][0] = tempVert[0];
		tess.xyz[baseVertex + j][1] = tempVert[1];
		tess.xyz[baseVertex + j][2] = tempVert[2];

		tess.normal[baseVertex + j][0] = tempNormal[0];
		tess.normal[baseVertex + j][1] = tempNormal[1];
		tess.normal[baseVertex + j][2] = tempNormal[2];

		tess.texCoords[baseVertex + j][0][0] = v->texCoords[0];
		tess.texCoords[baseVertex + j][0][1] = v->texCoords[1];

		// supress GGC strict-alisaing warnning
		{
			mdrWeight_t *pTmp = &v->weights[v->numWeights];
			v = (mdrVertex_t *)pTmp;
		}
	}

	tess.numVertexes += surface->numVerts;
}
