/*
g++ -static -static-libgcc -static-libstdc++ main.cpp -o N3PMeshConvert.exe -I./include -L./lib -lassimp.dll
N3PMeshConvert -export obj 1_6011_00_0.n3pmesh item_co_bow.bmp
N3PMeshConvert -import 1_6011_00_0.obj
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define N3VERSION 0.1

#include "assimp/scene.h"
#include "assimp/Exporter.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

//-----------------------------------------------------------------------------
typedef struct {
	float x, y, z;
	float nx, ny, nz;
	float u, v;
} Vertex;

typedef unsigned short Element;

//-----------------------------------------------------------------------------
aiScene  m_Scene;
Element* m_pIndices;
Vertex*  m_pVertices;
size_t   m_iMaxNumIndices;
size_t   m_iMaxNumVertices;

//-----------------------------------------------------------------------------
void ParseScene(const char* szFN);
void N3LoadMesh(const char* szFN);
void N3BuildMesh(const char* szFN);
void GenerateScene(const char* szFN);

//-----------------------------------------------------------------------------
int main(int argc, char* argv[]) {

	Assimp::Exporter* pExporter = new Assimp::Exporter();

	if(!strcmp(argv[1], "-version") && argc==2) {
		printf("\nDB: Version %.2f\n", N3VERSION);
	} else if(!strcmp(argv[1], "-formats") && argc==2) {
		size_t iNumFormats = pExporter->GetExportFormatCount();

		for(int i=0; i<iNumFormats; ++i) {
			const aiExportFormatDesc* pFormatDesc;
			pFormatDesc = pExporter->GetExportFormatDescription(i);

			printf("\nDB: ID-> %s\nDB: %s\n",
				pFormatDesc->id,
				pFormatDesc->description
			);
		}
	} else if(!strcmp(argv[1], "-export") && argc==5) {
		const char* pFormatID = argv[2];
		const char* pFileName = argv[3];
		const char* pTextName = argv[4];

		char pFileBase[MAX_PATH] = {};
		char pOutputFile[MAX_PATH] = {};

		char c = ' '; int offset = 0;
		while(c!='.' && c!='\0') c = pFileName[offset++];
		memcpy(pFileBase, pFileName, (offset-1)*sizeof(char));

		size_t iNumFormats = pExporter->GetExportFormatCount();

		int found = 0;
		for(int i=0; i<iNumFormats; ++i) {
			const aiExportFormatDesc* pFormatDesc;
			pFormatDesc = pExporter->GetExportFormatDescription(i);

			if(!strcmp(pFormatID, pFormatDesc->id)) {
				found = 1;
				sprintf(pOutputFile, "./%s.%s",
					pFileBase,
					pFormatDesc->fileExtension
				);
			}
		}

		if(!found) {
			printf("\nER: That format is not supported! Check the following list\n");
			system("N3PMeshConvert -formats\n");
			exit(-1);
		}

		printf("\nDB: Loading \"%s\"...\n", pFileName);
		N3LoadMesh(pFileName);
		printf("\nDB: Generating scene... ");
		GenerateScene(pTextName);
		printf("\nDB: Exporting to %s... ", pFormatID);

		aiReturn ret = pExporter->Export(&m_Scene, pFormatID, pOutputFile);
		if(ret == aiReturn_SUCCESS) {
			printf("Done!\n");
		} else {
			printf("Failed!\n");
		}
	} else if(!strcmp(argv[1], "-import") && argc==3) {
		const char* pFileName = argv[2];
		
		char pFileBase[MAX_PATH] = {};
		char pOutputFile[MAX_PATH] = {};

		char c = ' '; int offset = 0;
		while(c!='.' && c!='\0') c = pFileName[offset++];
		memcpy(pFileBase, pFileName, (offset-1)*sizeof(char));

		sprintf(pOutputFile, "./%s_mod.%s",
			pFileBase,
			"n3pmesh"
		);

		printf("\nDB: Loading \"%s\"... ", pFileName);
		ParseScene(pFileName);
		printf("\nDB: Generating N3PMesh...\n");
		N3BuildMesh(pOutputFile);
	}

	delete pExporter;
	pExporter = NULL;

	return 0;
}

//-----------------------------------------------------------------------------
void ParseScene(const char* szFN) {
	Assimp::Importer Importer;

	const aiScene* pScene = Importer.ReadFile(
		szFN,
		aiProcess_Triangulate      |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipUVs
	);

	if(pScene == NULL) {
		printf("\nER: %s\n", Importer.GetErrorString());
		exit(-1);
	}

	aiMesh* pMesh = pScene->mMeshes[0];

	m_iMaxNumVertices = pMesh->mNumVertices;
	m_iMaxNumIndices  = 3*pMesh->mNumFaces;

	if(m_iMaxNumVertices==0 || m_iMaxNumIndices==0) {
		printf("Failed!\n");
		printf("\nER: Mesh data missing!\n");
		exit(-1);
	}

	if(m_pVertices) {
		delete m_pVertices;
		m_pVertices = NULL;
	}

	m_pVertices = new Vertex[m_iMaxNumVertices];
	memset(m_pVertices, 0, sizeof(Vertex)*m_iMaxNumVertices);

	if(m_pIndices) {
		delete m_pIndices;
		m_pIndices = NULL;
	}

	m_pIndices = new Element[m_iMaxNumIndices];
	memset(m_pIndices, 0, sizeof(Element)*m_iMaxNumIndices);

	for(int i=0; i<pMesh->mNumVertices; ++i) {
		m_pVertices[i].x = pMesh->mVertices[i].x;
		m_pVertices[i].y = pMesh->mVertices[i].y;
		m_pVertices[i].z = pMesh->mVertices[i].z;

		m_pVertices[i].nx = pMesh->mNormals[i].x;
		m_pVertices[i].ny = pMesh->mNormals[i].y;
		m_pVertices[i].nz = pMesh->mNormals[i].z;

		m_pVertices[i].u = pMesh->mTextureCoords[0][i].x;
		m_pVertices[i].v = pMesh->mTextureCoords[0][i].y;
	}

	for(int i=0; i<pMesh->mNumFaces; ++i) {
		aiFace& face = pMesh->mFaces[i];

		m_pIndices[3*i+0] = (Element) face.mIndices[0];
		m_pIndices[3*i+1] = (Element) face.mIndices[1];
		m_pIndices[3*i+2] = (Element) face.mIndices[2];
	}

	printf("Success!\n");

	fflush(stdout);
}

//-----------------------------------------------------------------------------
void N3LoadMesh(const char* szFN) {
	FILE* fpMesh = fopen(szFN, "rb");
	if(fpMesh == NULL) {
		printf("\nER: Unable to find mesh file!\n");
		exit(-1);
	}

	int nL = 0;
	fread(&nL, sizeof(uint32_t), 1, fpMesh);

	char m_szName[(nL+1)];

	if(nL > 0) {
		memset(&m_szName, 0, (nL+1));
		fread(&m_szName, sizeof(char), nL, fpMesh);
	}

	int m_iNumCollapses;
	int m_iMinNumIndices;
	int m_iMinNumVertices;
	int m_iTotalIndexChanges;

	fread(&m_iNumCollapses,      sizeof(uint32_t), 1, fpMesh);
	fread(&m_iTotalIndexChanges, sizeof(uint32_t), 1, fpMesh);
	fread(&m_iMaxNumVertices,    sizeof(uint32_t), 1, fpMesh);
	fread(&m_iMaxNumIndices,     sizeof(uint32_t), 1, fpMesh);
	fread(&m_iMinNumVertices,    sizeof(uint32_t), 1, fpMesh);
	fread(&m_iMinNumIndices,     sizeof(uint32_t), 1, fpMesh);

	if(m_pVertices) {
		delete m_pVertices;
		m_pVertices = NULL;
	}

	if(m_iMaxNumVertices > 0) {
		m_pVertices = new Vertex[m_iMaxNumVertices];
		memset(m_pVertices, 0, sizeof(Vertex)*m_iMaxNumVertices);
		fread(m_pVertices, sizeof(Vertex), m_iMaxNumVertices, fpMesh);
	}

	if(m_pIndices) {
		delete m_pIndices;
		m_pIndices = NULL;
	}

	if(m_iMaxNumIndices > 0) {
		m_pIndices = new Element[m_iMaxNumIndices];
		memset(m_pIndices, 0, sizeof(Element)*m_iMaxNumIndices);
		fread(m_pIndices, sizeof(Element), m_iMaxNumIndices, fpMesh);
	}

	printf("\nDB: MeshName: \"%s\"\n", m_szName);
	printf("DB: m_iNumCollapses      -> %d\n", m_iNumCollapses);
	printf("DB: m_iTotalIndexChanges -> %d\n", m_iTotalIndexChanges);
	printf("DB: m_iMaxNumVertices    -> %d\n", m_iMaxNumVertices);
	printf("DB: m_iMaxNumIndices     -> %d\n", m_iMaxNumIndices);
	printf("DB: m_iMinNumVertices    -> %d\n", m_iMinNumVertices);
	printf("DB: m_iMinNumIndices     -> %d\n", m_iMinNumIndices);

	if(
		m_iNumCollapses!=0                   ||
		m_iTotalIndexChanges!=0              ||
		m_iMaxNumVertices!=m_iMinNumVertices ||
		m_iMaxNumIndices!=m_iMinNumIndices
	) {
		printf("\nER: This release does not support mesh LODs!");
		exit(-1);
	}

	fflush(stdout);
	fclose(fpMesh);
}

//-----------------------------------------------------------------------------
void N3BuildMesh(const char* szFN) {
	FILE* fpMesh = fopen(szFN, "wb");
	if(fpMesh == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		exit(-1);
	}

	int nL = 0;
	fwrite(&nL, sizeof(uint32_t), 1, fpMesh);

	int m_iNumCollapses = 0;
	int m_iTotalIndexChanges = 0;
	int m_iMinNumIndices = m_iMaxNumIndices;
	int m_iMinNumVertices = m_iMaxNumVertices;

	fwrite(&m_iNumCollapses,      sizeof(uint32_t), 1, fpMesh);
	fwrite(&m_iTotalIndexChanges, sizeof(uint32_t), 1, fpMesh);
	fwrite(&m_iMaxNumVertices,    sizeof(uint32_t), 1, fpMesh);
	fwrite(&m_iMaxNumIndices,     sizeof(uint32_t), 1, fpMesh);
	fwrite(&m_iMinNumVertices,    sizeof(uint32_t), 1, fpMesh);
	fwrite(&m_iMinNumIndices,     sizeof(uint32_t), 1, fpMesh);

	if(m_iMaxNumVertices > 0) {
		fwrite(m_pVertices, sizeof(Vertex), m_iMaxNumVertices, fpMesh);
	}

	if(m_iMaxNumIndices > 0) {
		fwrite(m_pIndices, sizeof(Element), m_iMaxNumIndices, fpMesh);
	}

	int m_iLODCtrlValueCount = 0;
	fwrite(&m_iLODCtrlValueCount, sizeof(uint32_t), 1, fpMesh);
	
	printf("\nDB: MeshName: \"\"\n");
	printf("DB: m_iNumCollapses      -> %d\n", m_iNumCollapses);
	printf("DB: m_iTotalIndexChanges -> %d\n", m_iTotalIndexChanges);
	printf("DB: m_iMaxNumVertices    -> %d\n", m_iMaxNumVertices);
	printf("DB: m_iMaxNumIndices     -> %d\n", m_iMaxNumIndices);
	printf("DB: m_iMinNumVertices    -> %d\n", m_iMinNumVertices);
	printf("DB: m_iMinNumIndices     -> %d\n", m_iMinNumIndices);

	fflush(stdout);
	fclose(fpMesh);
}

//-----------------------------------------------------------------------------
void GenerateScene(const char* szFN) {
	m_Scene.mRootNode = new aiNode();

	m_Scene.mMaterials = new aiMaterial*[1];
	m_Scene.mMaterials[0] = NULL;
	m_Scene.mNumMaterials = 1;

	m_Scene.mMaterials[0] = new aiMaterial();

	aiString strTex(szFN);
	m_Scene.mMaterials[0]->AddProperty(
		&strTex, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0)
	);

	if(m_pVertices!=NULL && m_pIndices!=NULL) {

		m_Scene.mMeshes = new aiMesh*[1];
		m_Scene.mMeshes[0] = NULL;
		m_Scene.mNumMeshes = 1;

		m_Scene.mMeshes[0] = new aiMesh();
		m_Scene.mMeshes[0]->mMaterialIndex = 0;

		m_Scene.mRootNode->mMeshes = new unsigned int[1];
		m_Scene.mRootNode->mMeshes[0] = 0;
		m_Scene.mRootNode->mNumMeshes = 1;

		aiMesh* pMesh = m_Scene.mMeshes[0];

		pMesh->mVertices = new aiVector3D[m_iMaxNumVertices];
		pMesh->mNumVertices = m_iMaxNumVertices;

		pMesh->mTextureCoords[0] = new aiVector3D[m_iMaxNumVertices];
		pMesh->mNumUVComponents[0] = m_iMaxNumVertices;

		for(int i=0; i<m_iMaxNumVertices; ++i) {
			Vertex v = m_pVertices[i];
			pMesh->mVertices[i] = aiVector3D(v.x, v.y, v.z);
			pMesh->mTextureCoords[0][i] = aiVector3D(v.u, (1.0f-v.v), 0);
		}

		pMesh->mFaces = new aiFace[(m_iMaxNumIndices/3)];
		pMesh->mNumFaces = (m_iMaxNumIndices/3);

		for(int i=0; i<(m_iMaxNumIndices/3); ++i) {
			aiFace& face = pMesh->mFaces[i];

			face.mIndices = new unsigned int[3];
			face.mNumIndices = 3;

			face.mIndices[0] = m_pIndices[3*i+0];
			face.mIndices[1] = m_pIndices[3*i+1];
			face.mIndices[2] = m_pIndices[3*i+2];
		}
	} else {
		printf("Failed!\n");
		printf("\nER: Mesh data missing!\n");
		exit(-1);
	}

	printf("Success!\n");

	fflush(stdout);
}
