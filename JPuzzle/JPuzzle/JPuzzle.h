
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

struct Color {
	float x,y,z,w;
	void operator=(Vector4f & vec) {
		x=vec.x();
		y=vec.y();
		z=vec.z();
		w=vec.w();
	}
};

struct Texture {
	Texture():texels(0) {}
	Texture(Texture & cpy) {
		width=cpy.width, height=cpy.height; 
		texels = new Vector4f[width*height];
		memcpy(texels, cpy.texels, width*height*sizeof(Vector4f));
	}
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
	static const int m_MaxEdgeInsets=4;

	struct EdgePoint {
		Vector2f pos;
		float k;
		float w;
	};
	struct PuzzlePiece {
		PuzzlePiece():isBorderPiece(0) {memset(edgeCovered, 0, 4); }
		Matrix4f transform;
		Vector2f endPoints[4];
		Vector2f edgeNor[4];

		EdgePoint * edges[4];
		int nEdgePoints[4];
		
		Color * edgeColors[4][m_MaxEdgeInsets];
		int nEdgeColors[4][m_MaxEdgeInsets];
		
		float * projectedPoints[4];
		int nProjectedPoints[4];

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
	void ProcessPuzzlePiece(Texture & tex, int edgeInsetLevel, ID3D10Device * pDevice);
	bool OnOutsideBoundary(int i, int j, Texture & tex);
	bool OnBoundary(int i, int j, Texture & tex);
	void AddPiece();
	float CompareEdgesByShape(PuzzlePiece & a, PuzzlePiece & b, int k, int l);
public:
	JPuzzle();
	~JPuzzle() {}

	HRESULT Init(char * dir, int nToLoad, ID3D10Device * pDevice);
	void ComparePieces();
	void Render(ID3D10Device * pDevice);

	void Destroy();
};


#endif