
#ifndef JPUZZLE_H
#define JPUZZLE_H

#include <windows.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <Eigen/Dense>
#include <vector>
using namespace Eigen;

#pragma comment(lib, "d3d10")
#pragma comment(lib, "d3dx10")

const int g_TextureSize = 356;

struct SimpleVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 Tex;
};

struct Texture {
	Texture():texels(0) {}
	~Texture() {}

	int width;
	int height;
	Vector4f * texels;

	void Init(int _width, int _height) {
		width = _width;
		height = _height;
		texels = new Vector4f[width*height];
		memset(texels, 0, width*height*sizeof(Vector4f));
	}

	void ClearChannels() {
		memset(texels, 0, width*height*sizeof(Vector4f));
	}

	Vector4f & operator()(int i, int j) {
		assert(i>=0 && i<height && j>=0 && j<width);
		return texels[width*i + j];
	}

	bool Inside(int i, int j) {
		return (i>=0 && i<height && j>=0 && j<width);
	}
};

class JPuzzle {
private:
	struct BoundaryPoint {
		Vector2f pos;
		float k;
		float w;
	};
	struct PuzzlePiece {
		PuzzlePiece():isBorderPiece(0) {memset(edgeCovered, 0, 4); }
		Matrix4f transform;
		Vector2f endPoints[4];
		Vector2f edgeNor[4];
		std::vector<BoundaryPoint> edges[4];
		
		bool isBorderPiece; 
		bool edgeCovered[4];
		float totalCurvature[4];
		float totalLength[4];

		Texture tex;
		ID3D10ShaderResourceView* SRVPuzzleTexture;

		public:
			EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	};

	/* Puzzle graphics */
	ID3D10Effect*                       m_pEffect;
	ID3D10EffectTechnique*              m_pTechnique;
	ID3D10InputLayout*                  m_pVertexLayout;
	ID3D10Buffer*                       m_pVBQuad;
	ID3D10Buffer*                       m_pIBQuad;

	ID3D10EffectShaderResourceVariable* m_pSRVPuzzleTextureFx;
	PuzzlePiece * m_PuzzlePieces;
	int m_nPuzzlePieces;
	std::vector<PuzzlePiece*> m_AddedPuzzlePieces;
	std::vector<PuzzlePiece*> m_NotAddedPuzzlePieces;
	int m_nPiecesAdded;

	ID3D10EffectMatrixVariable*         m_pWorldfx;
	Matrix4f							m_World;

	HRESULT CreateGraphics(ID3D10Device * pDevice);
	HRESULT ExtractPuzzlePieces(char * file, ID3D10Device * pDevice);
	bool ExtractPiece(Texture & tex, Texture & tmpTex, std::vector<Vector2f> & piecePixels, int i, int j, ID3D10Device * pDevice, char * fileName);
	void ProcessPuzzlePiece(ID3D10Device * pDevice);
	bool OnOutsideBoundary(int i, int j, Texture & tex);
	void AddPiece();
	float CompareEdgesByShape(PuzzlePiece & a, PuzzlePiece & b, int k, int l);
public:
	JPuzzle();
	~JPuzzle() {}

	HRESULT Init(char * dir, ID3D10Device * pDevice);
	void ComparePieces();
	void Render(ID3D10Device * pDevice);

	void Destroy();
};


#endif