
#ifndef JPUZZLE_H
#define JPUZZLE_H

#include <windows.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <Eigen/Dense>
#include <vector>
#include <list>
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
	Color():x(0),y(0),z(0),w(0){}
	float x,y,z,w;
	void operator=(Vector4f & vec) {
		x=vec.x();
		y=vec.y();
		z=vec.z();
		w=vec.w();
	}
	float operator[](int idx) {
		switch(idx){
		case 0:
			return x;
		case 1: 
			return y;
		case 2:
			return z;
		default:
			return w;
		}
	}
	operator Vector3f() {
		return Vector3f(x,y,z);
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
	static const int m_MaxColorLayers=6;
	 
	struct EdgePoint {
		Vector2f pos;
		float k;
		float w;
	};
	struct PuzzlePiece {
		PuzzlePiece():isAdded(0), isBorderPiece(0) {memset(edgeCovered, 0, 4); memset(edgeIsBorder, 0, 4); memset(adjPieces, 0, 4*sizeof(PuzzlePiece*)); }
		Matrix4f transform;
		Matrix4f rotation;
		Vector2f endPoints[4];
		Vector2f edgeNor[4];

		std::vector<EdgePoint> edges[4];	
		std::vector<Color> edgeColors[4][m_MaxColorLayers];
		std::vector<float> projectedPoints[4];

		int index;
		bool isAdded;
		bool isBorderPiece; 
		bool edgeCovered[4];
		bool edgeIsBorder[4];
		float totalCurvature[4];
		float totalLength[4];
		PuzzlePiece * adjPieces[4];

		std::vector<int> borders(){
			std::vector<int> borders;
			for (int i = 0; i < 4; ++i) {
				if (edgeIsBorder[i]){
					borders.push_back(i);
				}
			}
			return borders;
		}
		int left(){
			std::vector<int> border = borders();
			if (border.size() == 1){
				return (border[0] + 3) % 4;
			}
			else if (border.size() == 2){
				int border1 = min(border[0], border[1]);
				int border2 = max(border[0], border[1]);
				if (border2 == border1 + 1){
					return (border1 + 3) % 4;
				}
				else{
					return (border2 + 3) % 4;
				}
			}
			else
				assert(0);
		}
		int right(){
			std::vector<int> border = borders();
			if (border.size() == 1){
				return (border[0] + 1) % 4;
			}
			else if (border.size() == 2){
				int border1 = min(border[0], border[1]);
				int border2 = max(border[0], border[1]);
				if (border2 == border1 + 1){
					return (border2 + 1) % 4;
				}
				else{
					return (border1 + 1) % 4;
				}
			}
			else
				assert(0);
		}

		Texture tex;
		ID3D10ShaderResourceView* SRVPuzzleTexture;

		public:
			EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	};

	struct EdgeLinkInfo {
		float measure;
		PuzzlePiece * a;
		PuzzlePiece * b;
		int k;
		int l;
	};

	/* Temporary Memory */
	Color * m_LeftColors[2];
	Color * m_RightColors[2];

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
	//float CompareEdgesByShape(PuzzlePiece & a, PuzzlePiece & b, int k, int l);
	//float CompareEdgesByColor(PuzzlePiece & a, PuzzlePiece & b, int k, int l);
	float CompareEdgesByShape(std::vector<EdgeLinkInfo> & links);
	float CompareEdgesByColor(std::vector<EdgeLinkInfo> & links);
	float MGC(Color ** left, Color ** right, int leftSize, int rightSize);
	
	struct Pocket{
		PuzzlePiece* a;
		PuzzlePiece* b;
		int k;
		int l;
		PuzzlePiece* bestMatch;
		int mk;
		int j;
		float measure;
    };
	std::vector<Pocket> FindPockets();
	//void MatchPocket(std::vector<Pocket> pockets);
	void FindNeighbors(PuzzlePiece & a, PuzzlePiece & b, int k, int l, std::vector<EdgeLinkInfo> & links);
	void AssemblyBorder();
	struct BorderStrip{
		std::list<PuzzlePiece*> pieces;
		bool isAdded;
		BorderStrip(PuzzlePiece* p):isAdded(false){pieces.push_back(p);}
		void addToRight(PuzzlePiece* p) {pieces.push_front(p);}
		void addToLeft(BorderStrip& bs) {pieces.insert(pieces.end(), bs.pieces.begin(), bs.pieces.end());}
	};
	void AssemblyBorderMST();
	void AssemblyBorderWithDimension(int w, int h);
	void borderSearch(float& globalMin, float recursiveMin, std::vector<PuzzlePiece*>& pool, std::list<int>& border, std::list<int>& optBorder, const std::vector<std::vector<float> >& assignMatrix, int length);
	void borderStripSearch(float& globalMin, float recursiveMin, std::vector<BorderStrip>& pool, std::list<int>& recursiveBorder, std::vector<std::list<int>>& optBorder, int length);
public:
	JPuzzle();
	~JPuzzle() {}

	HRESULT Init(char * dir, int nToLoad, ID3D10Device * pDevice);
	void ComparePieces();
	void Render(ID3D10Device * pDevice);
	void MovePiece(EdgeLinkInfo & measure);

	void Destroy();
};

#endif