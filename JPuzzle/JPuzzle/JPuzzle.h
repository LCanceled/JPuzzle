
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

struct SimpleVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 Tex;
};

struct Texture {
	int width;
	int height;
	Vector4f * texels;

	void Init(int _width, int _height) {
		width = _width;
		height = _height;
		texels = new Vector4f[width*height];
	}

	Vector4f & operator()(int i, int j) {
		assert(i>=0 && i<height && j>=0 && j<width);
		return texels[width*i + j];
	}
};

class JPuzzle {
private:
	struct PuzzlePiece {
		Vector2f endPoints[4];
		Vector2f maxPoint[4];
		Texture tex;
	};

	/* Puzzle graphics */
	ID3D10Effect*                       m_pEffect;
	ID3D10EffectTechnique*              m_pTechnique;
	ID3D10InputLayout*                  m_pVertexLayout;
	ID3D10Buffer*                       m_pVBQuad;
	ID3D10Buffer*                       m_pIBQuad;

	std::vector<ID3D10ShaderResourceView*> m_rgSRVPuzzleTexture;
	std::vector<PuzzlePiece> m_PuzzlePieces;
	ID3D10EffectShaderResourceVariable* m_pSRVPuzzleTextureFx;

	ID3D10EffectMatrixVariable*         m_pWorldfx;
	Matrix4f							m_World;

	HRESULT CreateGraphics(ID3D10Device * pDevice);
	void ProcessPuzzlePiece(ID3D10Device * pDevice);
	bool OnOutsideBoundary(int i, int j, Texture & tex);
public:
	JPuzzle();
	~JPuzzle() {}

	HRESULT Init(char * dir, ID3D10Device * pDevice);
	void ComparePieces();
	void Render(ID3D10Device * pDevice);

	void Destroy();
};


#endif