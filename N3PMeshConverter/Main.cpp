/*
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
	float x, y, z, w;
} __Quaternion;

typedef struct {
	float x, y, z;
	float nx, ny, nz;
	float u, v;
} Vertex;

typedef struct {
	float x, y, z;
} Vec3;

struct __VertexSkinned {
	Vec3   vOrigin;
	int    nAffect;
	int*   pnJoints;
	float* pfWeights;
};

struct __VertexXyzNormal: public Vec3
{
public:
	Vec3 n;

public:
	const __VertexXyzNormal& operator = (const Vertex vec) {
		x   = vec.x ; y   = vec.y ;   z = vec.z ;
		n.x = vec.nx; n.y = vec.ny; n.z = vec.nz;
		return *this;
	}
};

struct VertUVs
{
public:
	float u, v;

public:
	const VertUVs& operator = (const Vertex vec) {
		u = vec.u; v = vec.v;
		return *this;
	}
};

typedef unsigned short Element;

struct _N3EdgeCollapse {
	int NumIndicesToLose;
	int NumIndicesToChange;
	int NumVerticesToLose;
	int iIndexChanges;
	int CollapseTo;
	bool bShouldCollapse;
};

struct _N3LODCtrlValue {
	float fDist;
	int iNumVertices;
};

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
void N3BuildSkin(const char* szFN);
void GenerateScene(const char* szFN);

//-----------------------------------------------------------------------------
int main(int argc, char* argv[]) {

	Assimp::Exporter* pExporter = new Assimp::Exporter();

	if(!strcmp(argv[1], "-version") && argc==2) {
		printf("\nDB: Version %.2f\n", N3VERSION);
	} else if(!strcmp(argv[1], "-formats") && argc==2) {
		size_t iNumFormats = pExporter->GetExportFormatCount();

		for(unsigned int i=0; i<iNumFormats; ++i) {
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

		char pFileBase[MAXLEN] = {};
		char pOutputFile[MAXLEN] = {};

		char c = ' '; int offset = 0;
		while(c!='.' && c!='\0') c = pFileName[offset++];
		memcpy(pFileBase, pFileName, (offset-1)*sizeof(char));

		size_t iNumFormats = pExporter->GetExportFormatCount();

		int found = 0;
		for(unsigned int i=0; i<iNumFormats; ++i) {
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
			system("pause");
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
	} else if(!strcmp(argv[1], "-import") && argc==4) {
		const char* pFileName = argv[2];
		const char* pMeshType = argv[3];
		
		char pFileBase[MAXLEN] = {};
		char pOutputFile[MAXLEN] = {};

		char c = ' '; int offset = 0;
		while(c!='.' && c!='\0') c = pFileName[offset++];
		memcpy(pFileBase, pFileName, (offset-1)*sizeof(char));

		printf("\nDB: Loading \"%s\"... ", pFileName);
		ParseScene(pFileName);

		if(!strcmp(pMeshType, "n3pmesh")) {
			sprintf(pOutputFile, "./%s_mod.%s", pFileBase, "n3pmesh");
			printf("\nDB: Generating N3PMesh...\n");
			N3BuildMesh(pOutputFile);
		} else if(!strcmp(pMeshType, "n3cskins")) {
			sprintf(pOutputFile, "./%s_mod.%s", pFileBase, "n3cskins");
			printf("\nDB: Generating N3CSkins...\n");
			N3BuildSkin(pOutputFile);
		}
	} else {
		printf("Incorrect command-line arguments.\n");
	}

	delete pExporter;
	pExporter = NULL;

	system("pause");

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
		system("pause");
		exit(-1);
	}

	aiMesh* pMesh = pScene->mMeshes[0];

	m_iMaxNumVertices = pMesh->mNumVertices;
	m_iMaxNumIndices  = 3*pMesh->mNumFaces;

	if(m_iMaxNumVertices==0 || m_iMaxNumIndices==0) {
		printf("Failed!\n");
		printf("\nER: Mesh data missing!\n");
		system("pause");
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

	for(unsigned int i=0; i<pMesh->mNumVertices; ++i) {
		m_pVertices[i].x = pMesh->mVertices[i].x;
		m_pVertices[i].y = pMesh->mVertices[i].y;
		m_pVertices[i].z = pMesh->mVertices[i].z;

		m_pVertices[i].nx = pMesh->mNormals[i].x;
		m_pVertices[i].ny = pMesh->mNormals[i].y;
		m_pVertices[i].nz = pMesh->mNormals[i].z;

		m_pVertices[i].u = pMesh->mTextureCoords[0][i].x;
		m_pVertices[i].v = pMesh->mTextureCoords[0][i].y;
	}

	for(unsigned int i=0; i<pMesh->mNumFaces; ++i) {
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
		fprintf(stderr, "\nERROR: Missing mesh %s\n", szFN);
		system("pause");
		exit(-1);
	}
	/*
	*/

	// NOTE: length of the name for the mesh
	int nL0 = 0;
	fread(&nL0, sizeof(int), 1, fpMesh);

	// NOTE: if the shape has a mesh name read it in
	char m_szName0[0xFF] = {};

	if(nL0 > 0) {
		memset(&m_szName0, 0, (nL0+1));
		fread(&m_szName0, sizeof(char), nL0, fpMesh);
	}

	// NOTE: read in the number of "collapses"
	int m_iNumCollapses0;
	fread(&m_iNumCollapses0, sizeof(int), 1, fpMesh);

	// NOTE: read in the total index changes
	int m_iTotalIndexChanges0;
	fread(&m_iTotalIndexChanges0, sizeof(int), 1, fpMesh);

	// NOTE: read in the max num of vertices
	fread(&m_iMaxNumVertices, sizeof(int), 1, fpMesh);

	// NOTE: read in the max num of indices
	fread(&m_iMaxNumIndices, sizeof(int), 1, fpMesh);

	// NOTE: read in the min num of vertices
	int m_iMinNumVertices0;
	fread(&m_iMinNumVertices0, sizeof(int), 1, fpMesh);

	// NOTE: read in the min num of indices
	int m_iMinNumIndices0;
	fread(&m_iMinNumIndices0, sizeof(int), 1, fpMesh);

	// NOTE: free the previous vertex data
	if(m_pVertices) {
		delete m_pVertices;
		m_pVertices = NULL;
	}

	// NOTE: if there is a max vertex amount allocate space for it
	if(m_iMaxNumVertices > 0) {
		m_pVertices = new Vertex[m_iMaxNumVertices];
		memset(m_pVertices, 0, sizeof(Vertex)*m_iMaxNumVertices);

		// NOTE: read in the vertex data
		fread(m_pVertices, sizeof(Vertex), m_iMaxNumVertices, fpMesh);
	}

	// NOTE: free the previous index data
	if(m_pIndices) {
		delete m_pIndices;
		m_pIndices = NULL;
	}

	// NOTE: if there is a max index amount allocate space for it
	if(m_iMaxNumIndices > 0) {
		m_pIndices = new unsigned short[m_iMaxNumIndices];
		memset(m_pIndices, 0, sizeof(unsigned short)*m_iMaxNumIndices);

		// NOTE: read in the vertex data
		fread(m_pIndices, sizeof(unsigned short), m_iMaxNumIndices, fpMesh);
	}

	// NOTE: read in the "collapses" (I think this is used to set the vertices
	// based on how close the player is to the object)
	_N3EdgeCollapse* m_pCollapses = new _N3EdgeCollapse[(m_iNumCollapses0+1)];
	memset(&m_pCollapses[m_iNumCollapses0], 0, sizeof(_N3EdgeCollapse));

	if(m_iNumCollapses0 > 0) {
		fread(m_pCollapses, sizeof(_N3EdgeCollapse), m_iNumCollapses0, fpMesh);
		memset(&m_pCollapses[m_iNumCollapses0], 0, sizeof(_N3EdgeCollapse));
	}

	// NOTE: read in the index changes
	int* m_pAllIndexChanges = new int[m_iTotalIndexChanges0];

	if(m_iTotalIndexChanges0 > 0) {
		fread(m_pAllIndexChanges, sizeof(int), m_iTotalIndexChanges0, fpMesh);
	}

	// NOTE: read in m_iLODCtrlValueCount
	int m_iLODCtrlValueCount0;
	fread(&m_iLODCtrlValueCount0, sizeof(int), 1, fpMesh);

	// NOTE: read in the LODCtrls (current size seems to be 0)
	_N3LODCtrlValue* m_pLODCtrlValues = new _N3LODCtrlValue[m_iLODCtrlValueCount0];

	if(m_iLODCtrlValueCount0) {
		fread(m_pLODCtrlValues, sizeof(_N3LODCtrlValue), m_iLODCtrlValueCount0, fpMesh);
	}

	// NOTE: the actual number of indices and vertices for the specific
	// collapse
	if(m_pAllIndexChanges) {
		int m_iNumIndices = 0;
		int m_iNumVertices = 0;

		int c = 0;
		int LOD = 0;

		int iDiff = m_pLODCtrlValues[LOD].iNumVertices - m_iNumVertices;

		while(m_pLODCtrlValues[LOD].iNumVertices > m_iNumVertices) {
			if(c >= m_iNumCollapses0) break;
			if(m_pCollapses[c].NumVerticesToLose+m_iNumVertices > m_pLODCtrlValues[LOD].iNumVertices)
				break;

			m_iNumIndices += m_pCollapses[c].NumIndicesToLose;
			m_iNumVertices += m_pCollapses[c].NumVerticesToLose;
		
			int tmp0 = m_pCollapses[c].iIndexChanges;
			int tmp1 = tmp0+m_pCollapses[c].NumIndicesToChange;

			for(int i=tmp0; i<tmp1; i++) {
				m_pIndices[m_pAllIndexChanges[i]] = m_iNumVertices-1;
			}

			c++;
		}

		// NOTE: if we break on a collapse that isn't intended to be one we
		// should collapse up to then keep collapsing until we find one
		while(m_pCollapses[c].bShouldCollapse) {
			/*
			- not sure if this is really needed
			*/
			if(c >= m_iNumCollapses0) break;

			m_iNumIndices += m_pCollapses[c].NumIndicesToLose;
			m_iNumVertices += m_pCollapses[c].NumVerticesToLose;
		
			int tmp0 = m_pCollapses[c].iIndexChanges;
			int tmp1 = tmp0+m_pCollapses[c].NumIndicesToChange;

			for(int i=tmp0; i<tmp1; i++) {
				m_pIndices[m_pAllIndexChanges[i]] = m_iNumVertices-1;
			}

			c++;
		}
	}

	free(m_pLODCtrlValues);
	free(m_pAllIndexChanges);
	free(m_pCollapses);
	
	// NOTE: display debug info
	printf("\nMeshName: %s\n", m_szName0);
	printf("m_iNumCollapses      -> %d\n", m_iNumCollapses0);
	printf("m_iTotalIndexChanges -> %d\n", m_iTotalIndexChanges0);
	printf("m_iMaxNumVertices    -> %d\n", m_iMaxNumVertices);
	printf("m_iMaxNumIndices     -> %d\n", m_iMaxNumIndices);
	printf("m_iMinNumVertices    -> %d\n", m_iMinNumVertices0);
	printf("m_iMinNumIndices     -> %d\n", m_iMinNumIndices0);
	printf("m_iLODCtrlValueCount -> %d\n", m_iLODCtrlValueCount0);

	/*
	*/
	fflush(stdout);
	fclose(fpMesh);
}

//-----------------------------------------------------------------------------
void N3BuildMesh(const char* szFN) {
	FILE* fpMesh = fopen(szFN, "wb");
	if(fpMesh == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		system("pause");
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
void N3BuildSkin(const char* szFN) {
	FILE* fpSkin = fopen(szFN, "wb");
	if(fpSkin == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		system("pause");
		exit(-1);
	}

	int iNL = strlen(szFN);
	fwrite(&iNL, sizeof(int), 1, fpSkin);
	fwrite(szFN, sizeof(char), iNL, fpSkin);

	#define MAX_CHR_LOD 4
	for(int i=0; i<MAX_CHR_LOD; ++i) {
		// CN3IMesh::Load()
		int iNL = 0;
		fwrite(&iNL, sizeof(int), 1, fpSkin);

		/*
		Element* m_pIndices;
		Vertex*  m_pVertices;
		size_t   m_iMaxNumIndices;
		size_t   m_iMaxNumVertices;
		*/

		int nFC = 0, nVC = 0, nUVC = 0;
		nFC  = m_iMaxNumIndices/3;
		fwrite(&nFC, sizeof(int), 1, fpSkin);
		nVC  = m_iMaxNumVertices;
		fwrite(&nVC, sizeof(int), 1, fpSkin);
		nUVC = m_iMaxNumVertices;
		fwrite(&nUVC, sizeof(int), 1, fpSkin);

		VertUVs* m_pfUVs = new VertUVs[nVC];
		memset(m_pfUVs, 0x00, nVC*sizeof(VertUVs));
		__VertexXyzNormal* m_pVerticesWithNorms = new __VertexXyzNormal[nVC];
		memset(m_pVerticesWithNorms, 0x00, nVC*sizeof(__VertexXyzNormal));
		for(int k=0; k<nVC; ++k) {
			m_pfUVs[k] = m_pVertices[k];

			//m_pVertices[k].x /= 50.0f;//m_pVertices[k].x = (m_pVertices[k].x/10.0f +312.931f);
			//m_pVertices[k].y /= 50.0f;
			//m_pVertices[k].z /= 50.0f;//m_pVertices[k].z = (m_pVertices[k].z/10.0f +313.378979f);
			m_pVerticesWithNorms[k] = m_pVertices[k];
		}

		if(nFC>0 && nVC>0) {
			fwrite(m_pVerticesWithNorms, sizeof(__VertexXyzNormal), nVC, fpSkin);
			fwrite(m_pIndices, sizeof(Element), 3*nFC, fpSkin);
		}

		if(nUVC>0) {
			fwrite(m_pfUVs, sizeof(VertUVs), nUVC, fpSkin);
			fwrite(m_pIndices, sizeof(Element), 3*nFC, fpSkin);
		}

		delete m_pfUVs;
		delete m_pVerticesWithNorms;

		//CN3Skin::Load()
		for(int k=0; k<nVC; ++k) {
			__VertexSkinned skin_vert = {};
			skin_vert.nAffect = 0;
			fwrite(&skin_vert, sizeof(__VertexSkinned), 1, fpSkin);
		}
	}


	fflush(stdout);
	fclose(fpSkin);

	// TEMP: write out the over things
	fpSkin = fopen("mob_worm.n3anim", "wb");
	if(fpSkin == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		system("pause");
		exit(-1);
	}

	int nCount = 0;
	fwrite(&nCount, sizeof(int), 1, fpSkin);

	fclose(fpSkin);

	// TEMP: write out the over things
	fpSkin = fopen("mob_worm.n3joint", "wb");
	if(fpSkin == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		system("pause");
		exit(-1);
	}

	iNL = strlen(szFN);
	fwrite(&iNL, sizeof(int), 1, fpSkin);
	fwrite(szFN, sizeof(char), iNL, fpSkin);

	//sizeof(__Vector3)
	unsigned char data0[12] = {};
	fwrite(data0, sizeof(unsigned char), 12, fpSkin);

	//sizeof(__Quaternion)
	//unsigned char data1[16] = {};
	//fwrite(data1, sizeof(unsigned char), 16, fpSkin);
	__Quaternion tmp = {}; tmp.w = 1.0f;
	fwrite(&tmp, sizeof(__Quaternion), 1, fpSkin);

	//sizeof(__Vector3)
	//unsigned char data2[12] = {};
	//fwrite(data2, sizeof(unsigned char), 12, fpSkin);
	Vec3 tmp0 = {1.0f, 1.0f, 1.0f};
	fwrite(&tmp0, sizeof(Vec3), 1, fpSkin);

	//CN3AnimKey::Load()
	nCount = 0;
	fwrite(&nCount, sizeof(int), 1, fpSkin);
	fwrite(&nCount, sizeof(int), 1, fpSkin);
	fwrite(&nCount, sizeof(int), 1, fpSkin);
	//m_KeyOrient
	fwrite(&nCount, sizeof(int), 1, fpSkin);
	//nCC
	fwrite(&nCount, sizeof(int), 1, fpSkin);

	fclose(fpSkin);

	// TEMP: write out the over things
	fpSkin = fopen("mob_worm_body.n3cpart", "wb");
	if(fpSkin == NULL) {
		printf("\nER: Unable to create mesh file!\n");
		system("pause");
		exit(-1);
	}

	iNL = strlen(szFN);
	fwrite(&iNL, sizeof(int), 1, fpSkin);
	fwrite(szFN, sizeof(char), iNL, fpSkin);

	int m_dwReserved = 0;
	fwrite(&m_dwReserved, sizeof(int), 1, fpSkin);

	//sizeof(__Material)
	unsigned char data3[92] = {};
	fwrite(data3, sizeof(unsigned char), 92, fpSkin);

	// DXT name
	char dxt_name[] = "item\\capture_flag.dxt";
	iNL = strlen(dxt_name);
	fwrite(&iNL, sizeof(int), 1, fpSkin);
	fwrite(dxt_name, sizeof(char), iNL, fpSkin);

	// Skin name
	char skin_name[] = "item\\capture_flag.n3cskins";
	iNL = strlen(skin_name);
	fwrite(&iNL, sizeof(int), 1, fpSkin);
	fwrite(skin_name, sizeof(char), iNL, fpSkin);

	fclose(fpSkin);
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

		for(unsigned int i=0; i<m_iMaxNumVertices; ++i) {
			Vertex v = m_pVertices[i];
			pMesh->mVertices[i] = aiVector3D(v.x, v.y, v.z);
			pMesh->mTextureCoords[0][i] = aiVector3D(v.u, (1.0f-v.v), 0);
		}

		pMesh->mFaces = new aiFace[(m_iMaxNumIndices/3)];
		pMesh->mNumFaces = (m_iMaxNumIndices/3);

		for(unsigned int i=0; i<(m_iMaxNumIndices/3); ++i) {
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
		system("pause");
		exit(-1);
	}

	printf("Success!\n");

	fflush(stdout);
}
