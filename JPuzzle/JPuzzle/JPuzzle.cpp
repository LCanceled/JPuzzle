
#include "JPuzzle.h"
#include <string>
#include <fstream>
#include <stack>
#include <Eigen/Eigenvalues> 
#include <queue>
#include <list>
#include <Eigen/LU>

JPuzzle::JPuzzle():m_pEffect(0), m_pTechnique(0), m_pVertexLayout(0), m_pVBQuad(0), m_pIBQuad(0), m_pSRVPuzzleTextureFx(0), m_pWorldfx(0), m_nPiecesAdded(0)
{
	int size = 1024;
	m_LeftColors[0] = new Color[size];
	m_LeftColors[1] = new Color[size];
	m_RightColors[0] = new Color[size];
	m_RightColors[1] = new Color[size];
}

HRESULT JPuzzle::CreateGraphics(ID3D10Device * pDevice)
{
	HRESULT hr = S_OK;

	// Create the effect
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    hr = D3DX10CreateEffectFromFile( L"PT.fx", NULL, NULL, "fx_4_0", dwShaderFlags, 0, pDevice, NULL,
                                         NULL, &m_pEffect, NULL, NULL );
    if( FAILED( hr ) )
    {
        MessageBox( NULL,
                    L"The FX file cannot be located.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

    // Obtain the technique
    m_pTechnique = m_pEffect->GetTechniqueByName( "Render" );

    // Obtain the variables
    m_pWorldfx = m_pEffect->GetVariableByName( "World" )->AsMatrix();
	m_pSRVPuzzleTextureFx = m_pEffect->GetVariableByName( "txDiffuse" )->AsShaderResource();

    // Define the input layout
    D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof( layout ) / sizeof( layout[0] );

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    m_pTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                          PassDesc.IAInputSignatureSize, &m_pVertexLayout );
    if( FAILED( hr ) )
        return hr;

    // Set the input layout
    pDevice->IASetInputLayout( m_pVertexLayout );

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
        { D3DXVECTOR3( -1.0f, -1.0f, 1.0f ), D3DXVECTOR2( 0.0f, 1.0f ) },
        { D3DXVECTOR3( 1.0f, 1.0f, 1.0f ), D3DXVECTOR2( 1.0f, 0.0f ) },
        { D3DXVECTOR3( 1.0f, -1.0f, 1.0f ), D3DXVECTOR2( 1.0f, 1.0f ) },
        { D3DXVECTOR3( -1.0f, 1.0f, 1.0f ), D3DXVECTOR2( 0.0f, 0.0f ) },
    };

    D3D10_BUFFER_DESC bd;
    bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;
    bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    hr = pDevice->CreateBuffer( &bd, &InitData, &m_pVBQuad );
    if( FAILED( hr ) )
        return hr;

    // Set vertex buffer
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    pDevice->IASetVertexBuffers( 0, 1, &m_pVBQuad, &stride, &offset );

    // Create index buffer
    // Create vertex buffer
    DWORD indices[] =
    {
        0,1,2,
        0,3,1
    };
    bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( DWORD ) * 6;
    bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    InitData.pSysMem = indices;
    hr = pDevice->CreateBuffer( &bd, &InitData, &m_pIBQuad );
    if( FAILED( hr ) )
        return hr;

    // Set index buffer
    pDevice->IASetIndexBuffer( m_pIBQuad, DXGI_FORMAT_R32_UINT, 0 );

    // Set primitive topology
    pDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    if( FAILED( hr ) )
        return hr;

    // Initialize the world matrices
	m_World = m_World.Identity();

	return S_OK;
}

HRESULT JPuzzle::ExtractPuzzlePieces(char * file, ID3D10Device * pDevice)
{
	ID3D10Texture2D * pTexture;
	D3DX10_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
	loadInfo.MipLevels = 1;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.Usage = D3D10_USAGE_STAGING;
	loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_READ;
	loadInfo.BindFlags = 0;
	std::string fileName(file+std::string(".png"));
	if (FAILED(D3DX10CreateTextureFromFileA(pDevice, fileName.c_str(), &loadInfo, NULL, (ID3D10Resource**)&pTexture, NULL)))
		DebugBreak();

	D3D10_TEXTURE2D_DESC desc;
	pTexture->GetDesc(&desc);
	Texture tex;
	tex.Init(desc.Width, desc.Height);

	D3D10_MAPPED_TEXTURE2D mappedTex;
	pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_READ, 0, &mappedTex);
	UCHAR* pTexels = (UCHAR*)mappedTex.pData;
	for( UINT row = 0; row < desc.Height; row++ )
	{
		UINT rowStart = row * mappedTex.RowPitch;
		for( UINT col = 0; col < desc.Width; col++ )
		{
			UINT colStart = col * 4;
			tex(row, col) = Vector4f(
				pTexels[rowStart + colStart + 0], 
				pTexels[rowStart + colStart + 1], 
				pTexels[rowStart + colStart + 2], 
				pTexels[rowStart + colStart + 3]);
		}
	}
	pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
	pTexture->Release();

	int count=0;
	Texture tmpTex; 
	tmpTex.Init(tex.width, tex.height);
	std::vector<Vector2f> piecePixels;
	piecePixels.resize(tex.width*tex.height);
	for (int i=0; i<tex.height; i++) {
		for (int j=0; j<tex.width; j++) {
			if (tex(i,j).w() > 0) {
				char fileName[256];
				sprintf(fileName, "%s/%s_%.3i.png", file, file, count+1);
				if (ExtractPiece(tex, tmpTex, piecePixels, i, j, pDevice, fileName)) count++;
			}
		}
	}
	exit(0);

	return S_OK;
}

bool JPuzzle::ExtractPiece(Texture & tex, Texture & tmpTex, std::vector<Vector2f> & piecePixels, int i, int j, ID3D10Device * pDevice, char * fileName)
{
	tmpTex.ClearChannels();

	int nPiecePixels = 0;
	std::queue<std::pair<int,int> > q;
	q.push(std::pair<int,int>(i,j));
	while (!q.empty()) {
		std::pair<int,int> pair = q.front();
		q.pop();
		int ii=pair.first, jj=pair.second;
		if (tex(ii, jj).w() == 0) continue;

		if (tex(ii, jj).w() > 200) {
			tmpTex(ii,jj) = tex(ii,jj);
			piecePixels[nPiecePixels++] = Vector2f(jj,ii);
		}
		tex(ii,jj).w() = 0;

		if (tex(ii+1, jj).w() > 0) {q.push(std::pair<int,int>(ii+1,jj)); }
		if (tex(ii-1, jj).w() > 0) {q.push(std::pair<int,int>(ii-1,jj)); }
		if (tex(ii, jj+1).w() > 0) {q.push(std::pair<int,int>(ii,jj+1)); }
		if (tex(ii, jj-1).w() > 0) {q.push(std::pair<int,int>(ii,jj-1)); }
		if (tex(ii+1, jj+1).w() > 0) {q.push(std::pair<int,int>(ii+1,jj+1)); }
		if (tex(ii-1, jj+1).w() > 0) {q.push(std::pair<int,int>(ii-1,jj+1)); }
		if (tex(ii+1, jj-1).w() > 0) {q.push(std::pair<int,int>(ii+1,jj-1)); }
		if (tex(ii-1, jj-1).w() > 0) {q.push(std::pair<int,int>(ii-1,jj-1)); }
	}
	if (nPiecePixels < 200) return 0;
	
	float xMin=FLT_MAX, xMax=-FLT_MIN, yMin=FLT_MAX, yMax=-FLT_MAX; 
	for (int i=0; i<nPiecePixels; i++) {
		if (piecePixels[i].x() < xMin) xMin = piecePixels[i].x();
		if (piecePixels[i].x() > xMax) xMax = piecePixels[i].x();
		if (piecePixels[i].y() < yMin) yMin = piecePixels[i].y();
		if (piecePixels[i].y() > yMin) yMax = piecePixels[i].y();
	}

	int bbWidth = xMax-xMin;
	int bbHeight = yMax-yMin;
	int offsetX = xMin-(g_TextureSize-bbWidth)/2;
	int offsetY = yMin-(g_TextureSize-bbHeight)/2;

	ID3D10Texture2D * pTexture;
	D3D10_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D10_TEXTURE2D_DESC));
	texDesc.MipLevels = 1;
	texDesc.Width = g_TextureSize;
	texDesc.Height = g_TextureSize;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D10_USAGE_DYNAMIC;
	texDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	texDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	DXGI_SAMPLE_DESC sampleDesc = {1, 0};
	texDesc.SampleDesc = sampleDesc;
	if (FAILED(pDevice->CreateTexture2D(&texDesc, 0, &pTexture)))
		DebugBreak();

	D3D10_MAPPED_TEXTURE2D mappedTex;
	pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_WRITE_DISCARD, 0, &mappedTex);
	UCHAR* pTexels = (UCHAR*)mappedTex.pData;
	float scale = 2;
	for(UINT row = 0; row < g_TextureSize; row++ ) {
		UINT rowStart = row * mappedTex.RowPitch;
		for( UINT col = 0; col < g_TextureSize; col++ ) {
			UINT colStart = col * 4;

			Vector4f color(0,0,0,0);
			if (tmpTex.Inside(offsetY+row, offsetX+col)) color = tmpTex(offsetY+row, offsetX+col);
			pTexels[rowStart + colStart + 0] = color[0];
			pTexels[rowStart + colStart + 1] = color[1];
			pTexels[rowStart + colStart + 2] = color[2];
			pTexels[rowStart + colStart + 3] = color[3];
		}
	}
	pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));

	if (FAILED(D3DX10SaveTextureToFileA(pTexture, D3DX10_IFF_PNG, fileName)))
		DebugBreak();

	pTexture->Release();

	return 1;
}

HRESULT JPuzzle::Init(char * file, int nToLoad, ID3D10Device * pDevice)
{
	HRESULT hr = CreateGraphics(pDevice);
	if (hr != S_OK) return hr;

	/* Load puzzle pieces and textures */
	std::string sFile(file);
	sFile += "/";
	std::vector<std::string> fileNames;
	auto GetPuzzleFiles = [&]() {
		WIN32_FIND_DATAA findFileData;
		HANDLE hFind = FindFirstFileA((sFile+"*").c_str(), &findFileData);
		while (FindNextFileA(hFind, &findFileData) != 0) {
			if (strstr(findFileData.cFileName, "png")) {
				fileNames.push_back(findFileData.cFileName);
			}
		}
	};
	GetPuzzleFiles();
	if (fileNames.size() < 1) {
		ExtractPuzzlePieces(file, pDevice);
		GetPuzzleFiles();
	}
	
	m_nPuzzlePieces = 0;
	m_PuzzlePieces = new PuzzlePiece[fileNames.size()];
	for (int i=0; i<fileNames.size(); i++) {
		ID3D10ShaderResourceView * pRSV = NULL;
		D3DX10_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory( &loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
		loadInfo.MipLevels = 1;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D10_USAGE_DYNAMIC;
		loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE;
		loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		if (FAILED(D3DX10CreateShaderResourceViewFromFileA(pDevice, (sFile+fileNames[i]).c_str(), &loadInfo, NULL, &pRSV, NULL))) 
			DebugBreak();

		/* Create the puzzle piece */
		PuzzlePiece& piece = m_PuzzlePieces[m_nPuzzlePieces];
		piece.SRVPuzzleTexture = pRSV;
		ID3D10Texture2D * pTexture;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D10_USAGE_STAGING;
		loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_READ;
		loadInfo.BindFlags = 0;
		if (FAILED(D3DX10CreateTextureFromFileA(pDevice, (sFile+fileNames[i]).c_str(), &loadInfo, NULL, (ID3D10Resource**)&pTexture, NULL)))
			DebugBreak();

		D3D10_TEXTURE2D_DESC desc;
		pTexture->GetDesc(&desc);
		D3D10_MAPPED_TEXTURE2D mappedTex;
		pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_READ, 0, &mappedTex);
		piece.tex.Init(desc.Width, desc.Height);
		UCHAR* pTexels = (UCHAR*)mappedTex.pData;
		for( UINT row = 0; row < desc.Height; row++ )
		{
			UINT rowStart = row * mappedTex.RowPitch;
			for( UINT col = 0; col < desc.Width; col++ )
			{
				UINT colStart = col * 4;
				piece.tex(row, col) = Vector4f(
					pTexels[rowStart + colStart + 0], 
					pTexels[rowStart + colStart + 1], 
					pTexels[rowStart + colStart + 2], 
					pTexels[rowStart + colStart + 3]);
			}
		}
		pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
		pTexture->Release();

		piece.transform = Matrix4f::Identity();
		//m_PuzzlePieces[m_nPuzzlePieces++] = piece;
		m_nPuzzlePieces++;

		Texture tmpTex(piece.tex);
		for (int k=0; k<m_MaxColorLayers; k++)
			ProcessPuzzlePiece(tmpTex, k, pDevice);

		if (i>=nToLoad-1) break;
	}
	m_nPiecesAdded = 1;
	m_AddedPuzzlePieces.push_back(&m_PuzzlePieces[0]);
	for (int i=1; i<m_nPuzzlePieces; i++) {
		m_NotAddedPuzzlePieces.push_back(&m_PuzzlePieces[i]);
	}

	return S_OK;
}

bool JPuzzle::OnOutsideBoundary(int i, int j, Texture & tex)
{
	if (tex(i, j).w() > 0)
		return 0;

	int k=-1+i;
	for (; k<=i+1; k++) {
		int l=-1+j;
		for (; l<=j+1; l++) {
			if (tex(k, l).w() > 0) 
				return 1;
		}
	}
	return 0;
	//return tex(i-1,j).w() > 0 || tex(i+1,j).w() > 0 || tex(i,j-1).w() > 0 || tex(i,j+1).w() > 0;
}

bool JPuzzle::OnBoundary(int i, int j, Texture & tex)
{
	if (tex(i, j).w() == 0)
		return 0;

	int k=-1+i;
	for (; k<=i+1; k++) {
		int l=-1+j;
		for (; l<=j+1; l++) {
			if (tex(k, l).w() == 0) 
				return 1;
		}
	}
	return 0;
	//return tex(i-1,j).w() > 0 || tex(i+1,j).w() > 0 || tex(i,j-1).w() > 0 || tex(i,j+1).w() > 0;
}

int CompareCurvature(const void * a, const void * b) 
{
	if ( *(float*)a <  *(float*)b ) return (int)-1;
	if ( *(float*)a == *(float*)b ) return (int)0;
	if ( *(float*)a >  *(float*)b ) return (int)1;
}

void JPuzzle::ProcessPuzzlePiece(Texture & tex, int edgeInsetLevel, ID3D10Device * pDevice)
{
	PuzzlePiece & piece = m_PuzzlePieces[m_nPuzzlePieces-1];
	piece.index = m_nPuzzlePieces-1;
	char buf[256];
	sprintf(buf, "\n%i\n", m_nPuzzlePieces);
	OutputDebugStringA(buf);

	/* Find pixels on boundary */
	int i=tex.height/2, j=0;
	for (; j<tex.width; j++) {
		if (tex(i, j).w() > 0) break;
	}
	//j -= 1;
	int startX = j;
	int startY = i;
	if (startX >= tex.width-1)
		DebugBreak();

	int * used = new int[tex.width*tex.height];
	memset(used, 0, sizeof(int)*tex.width*tex.height);

	/*D3D10_MAPPED_TEXTURE2D mappedTex;
	ID3D10Texture2D * pTexture;
	piece.SRVPuzzleTexture->GetResource((ID3D10Resource**)&pTexture);
	pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_WRITE_DISCARD, 0, &mappedTex);
	UCHAR* pTexels = (UCHAR*)mappedTex.pData;*/
	std::vector<Vector2f> pixelBoundaryPos;
	//std::vector<Vector2f> boundaryPos;
	std::vector<Vector2f> tmpBoundaryPos;
	bool popped = 0;

	auto IsAtStart = [] (int i, int j, int startX, int startY) {
		return (i+1==startY && j==startX || i-1==startY && j==startX || i==startY && j+1==startX || i==startY && j-1==startX ||
			i+1==startY && j+1==startX || i-1==startY && j+1==startX || i+1==startY && j-1==startX || i-1==startY && j-1==startX);
	};

	for (; ;) {
		if (!popped) {
			pixelBoundaryPos.push_back(Vector2f(j, i));
			tmpBoundaryPos.push_back(Vector2f(j, i));
		}
		popped = 0;
		used[tex.width*i + j] = pixelBoundaryPos.size()+1;

		/*if (edgeInsetLevel == 2) {
			UINT rowStart = (int)pixelBoundaryPos.back().y() * mappedTex.RowPitch;
			UINT colStart =  (int)pixelBoundaryPos.back().x() * 4;
			pTexels[rowStart + colStart + 0] = 255*edgeInsetLevel;
			pTexels[rowStart + colStart + 1] = 255;
			pTexels[rowStart + colStart + 2] = 0; 
			pTexels[rowStart + colStart + 3] = 255;
		}//*/
		
		if (pixelBoundaryPos.size() > 10 && IsAtStart(i, j, startX, startY))
			break;

		int x=-1,y=-1;
		if (OnBoundary(i+1, j, tex) && !used[tex.width*(i+1) + j]) y=i+1,x=j;
		else if (OnBoundary(i-1, j, tex) && !used[tex.width*(i-1) + j]) y=i-1,x=j;
		else if (OnBoundary(i, j+1, tex) && !used[tex.width*(i) + j+1]) y=i,x=j+1;
		else if (OnBoundary(i, j-1, tex) && !used[tex.width*(i) + j-1]) y=i,x=j-1;
		else if (OnBoundary(i+1, j+1, tex) && !used[tex.width*(i+1) + j+1]) y=i+1,x=j+1;
		else if (OnBoundary(i-1, j+1, tex) && !used[tex.width*(i-1) + j+1]) y=i-1,x=j+1;
		else if (OnBoundary(i+1, j-1, tex) && !used[tex.width*(i+1) + j-1]) y=i+1,x=j-1;
		else if (OnBoundary(i-1, j-1, tex) && !used[tex.width*(i-1) + j-1]) y=i-1,x=j-1;
		/*if (OnOutsideBoundary(i+1, j, tex) && !used[tex.width*(i+1) + j]) y=i+1,x=j;
		else if (OnOutsideBoundary(i-1, j, tex) && !used[tex.width*(i-1) + j]) y=i-1,x=j;
		else if (OnOutsideBoundary(i, j+1, tex) && !used[tex.width*(i) + j+1]) y=i,x=j+1;
		else if (OnOutsideBoundary(i, j-1, tex) && !used[tex.width*(i) + j-1]) y=i,x=j-1;
		else if (OnOutsideBoundary(i+1, j+1, tex) && !used[tex.width*(i+1) + j+1]) y=i+1,x=j+1;
		else if (OnOutsideBoundary(i-1, j+1, tex) && !used[tex.width*(i-1) + j+1]) y=i-1,x=j+1;
		else if (OnOutsideBoundary(i+1, j-1, tex) && !used[tex.width*(i+1) + j-1]) y=i+1,x=j-1;
		else if (OnOutsideBoundary(i-1, j-1, tex) && !used[tex.width*(i-1) + j-1]) y=i-1,x=j-1;*/
		else { // back track
			if (pixelBoundaryPos.size() > 10 && (
				IsAtStart(i, j, pixelBoundaryPos[1].x(), pixelBoundaryPos[1].y()) || 
				IsAtStart(i, j, pixelBoundaryPos[2].x(), pixelBoundaryPos[2].y()) ))
				break;

			pixelBoundaryPos.pop_back();
			if (pixelBoundaryPos.size() == 0)  {
				
				for (int i=0; i<4; i++) {
					int s = piece.edgeColors[i][edgeInsetLevel-1].size();
					piece.edgeColors[i][edgeInsetLevel].resize(s);
					memcpy(piece.edgeColors[i][edgeInsetLevel].data(), piece.edgeColors[i][edgeInsetLevel-1].data(), sizeof(Color)*s);
				}
				
				delete[] used;
				//pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
				return;
				DebugBreak();
			}
			y = (int)pixelBoundaryPos.back().y();
			x = (int)pixelBoundaryPos.back().x();
			popped=1;
			
		}

		i = y;
		j = x;
	}
	delete[] used;
	int nPoints = pixelBoundaryPos.size();

	/* Correct orientation */
	{
		Vector2f & v1 = pixelBoundaryPos[0];
		Vector2f & v2 = pixelBoundaryPos[nPoints/3];
		Vector2f & v3 = pixelBoundaryPos[2*nPoints/3];
		Vector2f e1(v2-v1);
		Vector2f e2(v3-v1);
		
		if (e2.x()*e1.y() - e2.y()*e1.x() < 0) {
			for (int i=0; i<nPoints/2; i++) {
				Vector2f t(pixelBoundaryPos[i]);
				pixelBoundaryPos[i] = pixelBoundaryPos[nPoints-i-1];
				pixelBoundaryPos[nPoints-i-1] = t;

				/*t = (boundaryPos[i]);
				boundaryPos[i] = boundaryPos[nPoints-i-1];
				boundaryPos[nPoints-i-1] = t;*/
			}
		}
	}

	/* Compute curvatures */
	std::vector<float> curvatures;
	std::vector<float> angles;
	if (edgeInsetLevel == 0) {
		curvatures.resize(nPoints);
		angles.resize(nPoints);
		const int curvatureSize=7;
		Matrix2f M;
		Vector2f avgPt;
		MatrixXf curvaturePtsX(2*(curvatureSize+1)-1, 3);
		VectorXf curvaturePtsY(2*(curvatureSize+1)-1);
		std::ofstream out("out.txt");
		int special = 580007;//nPoints-232-1;
		auto ComputeNormalDirection = [&] (Vector2f oPt, float flip) {
			avgPt /= curvatureSize+1;
			M = (M - (curvatureSize+1)*avgPt*avgPt.transpose())/(curvatureSize+1);
		
			EigenSolver<Matrix2f> eig(M);
			Vector2f normalDir;
			Matrix2f vectors = eig.pseudoEigenvectors();
			if (eig.eigenvalues()[0].real() > eig.eigenvalues()[1].real())
				 normalDir = vectors.col(1);
			else normalDir = vectors.col(0);

			Vector2f v = avgPt - oPt;
			if (flip*(v.y()*normalDir.x() - v.x()*normalDir.y()) > 0)
				normalDir = -normalDir;

			return normalDir;
		};
		for (int i=0; i<nPoints; i++) {
			M = Matrix2Xf::Zero(2,2);
			avgPt = Vector2f::Zero(2);
		
			// Compute normal direction
			for (int j=i-curvatureSize; j<=i; j++) {
				int index = j < 0 ? nPoints+j : j;
				M += pixelBoundaryPos[index] * pixelBoundaryPos[index].transpose();
				avgPt += pixelBoundaryPos[index];
				//if (i == special)
				//	out << pixelBoundaryPos[index].x() << ' ' << pixelBoundaryPos[index].y()  << std::endl;
			}
			Vector2f v1 = ComputeNormalDirection(pixelBoundaryPos[i < 0 ? nPoints+i : i], -1);

			M = Matrix2Xf::Zero(2,2);
			avgPt = Vector2f::Zero(2);
			for (int j=i; j<=(i+curvatureSize); j++) {
				int index = j%nPoints;
				M += pixelBoundaryPos[index] * pixelBoundaryPos[index].transpose();
				avgPt += pixelBoundaryPos[index];
				//if (i == special)
				//	out << pixelBoundaryPos[index].x() << ' ' << pixelBoundaryPos[index].y()  << std::endl;
			}
			Vector2f v2 = ComputeNormalDirection(pixelBoundaryPos[i%nPoints], 1);
			Vector2f normalDir((v1+v2).normalized());
		
			// Compute angle
			float val = v1.dot(v2);
			if (val < -1) val = -1;
			if (val > 1) val = 1;
			val = acos(val);
			angles[i] = val;

			// Compute curvature
			int count = 0;
			Vector2f center(pixelBoundaryPos[i]);
			for (int j=i-curvatureSize; j<=i; j++) {
				int index = j < 0 ? nPoints+j : j;
				Vector2f l(pixelBoundaryPos[index]-center);
				float y = normalDir.dot(l);
				float x = sqrt(abs(l.squaredNorm() - y*y));
				if (normalDir.x()*l.y() - normalDir.y()*l.x() < 0) x = -x;
				curvaturePtsX(count, 0) = x*x;
				curvaturePtsX(count, 1) = x;
				curvaturePtsX(count, 2) = 1;
				curvaturePtsY(count++) = y;
			}
	
			for (int j=i+1; j<=(i+curvatureSize); j++) {
				int index = j%nPoints;
				Vector2f l(pixelBoundaryPos[index]-center);
				float y = normalDir.dot(l);
				float x = sqrt(abs(l.squaredNorm() - y*y));
				if (normalDir.x()*l.y() - normalDir.y()*l.x() < 0) x = -x;
				curvaturePtsX(count, 0) = x*x;
				curvaturePtsX(count, 1) = x;
				curvaturePtsX(count, 2) = 1;
				curvaturePtsY(count++) = y;
			}
		
			Vector3f curveInfo = curvaturePtsX.jacobiSvd(ComputeThinU | ComputeThinV).solve(curvaturePtsY);		
			float curvatureVal = 2*curveInfo.x()/pow(1+curveInfo.y()*curveInfo.y(), 1.5);

			if (i == special) {
				//out << v2 << std::endl << std::endl;
				//out << v1.dot(v2) << std::endl;
				float flt = v1.dot(v2); 
				//out << val << std::endl;
				//out << abs(.5f*D3DX_PI - val) << std::endl << std::endl;
			}

			curvatures[i] = curvatureVal;
			//out << curvatures[i] << std::endl;
		} //*/ 
	
		/*
		for (int i=0; i<nPoints; i++) {
			if (curvatures[i] < 0) {
				UINT rowStart = (int)pixelBoundaryPos[i].y() * mappedTex.RowPitch;
				UINT colStart =  (int)pixelBoundaryPos[i].x() * 4;
				pTexels[rowStart + colStart + 0] = 0;
				pTexels[rowStart + colStart + 1] = 0;
				pTexels[rowStart + colStart + 2] = 0; 
				pTexels[rowStart + colStart + 3] = 255;
			}

		}
		pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
		return;*/
	}

	/* Find the corner points */
	auto findClosestPt = [edgeInsetLevel, nPoints,&pixelBoundaryPos,&angles] (Vector2f & pt, Vector2f & offset) {
		float bestDistSq = FLT_MAX;
		int index = 0;
		for (int i=0; i<nPoints; i++) {
			float dist = (pixelBoundaryPos[i] - pt).squaredNorm();
			bool b = 1;
			if (edgeInsetLevel == 0) b = (.5f*D3DX_PI-abs(angles[i])) < .5f;
			if (dist < bestDistSq && b) {
				bestDistSq = dist;
				index = i;
			}
		}

	//	if (offset == Vector2f(-1,-1)) {
		/*UINT rowStart = (int)(pixelBoundaryPos[index].y()) * mappedTex.RowPitch;
		UINT colStart =  (int)(pixelBoundaryPos[index].x()) * 4;
		//UINT rowStart = (int)(pixelBoundaryPos[index].y()) * mappedTex.RowPitch;
		//UINT colStart =  (int)(pixelBoundaryPos[index].x()) * 4;
		pTexels[rowStart + colStart + 0] = 0;
		pTexels[rowStart + colStart + 1] = 255;
		pTexels[rowStart + colStart + 2] = 0; 
		pTexels[rowStart + colStart + 3] = 255;//*/
		//}
		return index;
	};
    int endPoints[4];
	//if (edgeInsetLevel == 0) {
		endPoints[0] = findClosestPt(Vector2f(0,0), Vector2f(1,1));
		endPoints[1] = findClosestPt(Vector2f(0,tex.height-1), Vector2f(1,-1));
		endPoints[2] = findClosestPt(Vector2f(tex.width-1,tex.height-1), Vector2f(-1,-1));
		endPoints[3] = findClosestPt(Vector2f(tex.width-1,0), Vector2f(-1,1));
/*	} else {
		endPoints[0] = findClosestPt(piece.endPoints[0], Vector2f(1,1));
		endPoints[1] = findClosestPt(piece.endPoints[1], Vector2f(1,-1));
		endPoints[2] = findClosestPt(piece.endPoints[2], Vector2f(-1,-1));
		endPoints[3] = findClosestPt(piece.endPoints[3], Vector2f(-1,1));
	}*/
	/* Segmented the boundary */
	if (edgeInsetLevel > 0) {
		for (int i=0; i<4; i++) {
			int edgeSize = 0;
			for (int j=endPoints[i]; j!=endPoints[(i+1)%4]; j=(j+1)%nPoints) edgeSize++;

			//piece.nEdgeColors[i][edgeInsetLevel] = edgeSize;
			//piece.edgeColors[i][edgeInsetLevel] = new Color[edgeSize];
			piece.edgeColors[i][edgeInsetLevel].resize(edgeSize);
			for (int j=endPoints[i], count=0; j!=endPoints[(i+1)%4]; j=(j+1)%nPoints, count++) {
				
				//piece.edgeColors[i][edgeInsetLevel][count] = tex(pixelBoundaryPos[j].y(), pixelBoundaryPos[j].x());
				piece.edgeColors[i][edgeInsetLevel][count] = tex(pixelBoundaryPos[j].y(), pixelBoundaryPos[j].x());
				tex(pixelBoundaryPos[j].y(), pixelBoundaryPos[j].x()).w() = 0;

				/*if (edgeInsetLevel==3) {
				UINT rowStart = (int)(pixelBoundaryPos[j].y()) * mappedTex.RowPitch;
				UINT colStart =  (int)(pixelBoundaryPos[j].x()) * 4;
				pTexels[rowStart + colStart + 0] = i == 0 ? 255 : 0;
				pTexels[rowStart + colStart + 1] = i == 1 ? 255 : 0;
				pTexels[rowStart + colStart + 2] = i == 2 ? 255 : 0; 
				pTexels[rowStart + colStart + 3] = 255;
				}
				/*pTexels[rowStart + colStart + 0] = edgeInsetLevel == 0 ? 255 : 0;
				pTexels[rowStart + colStart + 1] = edgeInsetLevel == 1 ? 255 : 0;
				pTexels[rowStart + colStart + 2] = edgeInsetLevel == 2 ? 255 : 0; 
				pTexels[rowStart + colStart + 3] = 255;*/
			}
		}

		//pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
		return;
	}

	std::vector<EdgePoint> * edges = piece.edges;
	for (int i=0; i<4; i++) {
		edges[i].reserve(nPoints);
		for (int j=endPoints[i]; j!=endPoints[(i+1)%4]; j=(j+1)%nPoints) {
			EdgePoint bd;
			bd.pos = pixelBoundaryPos[j];
			bd.k = curvatures[j];
			edges[i].push_back(bd);

			/*UINT rowStart = (int)(pixelBoundaryPos[j].y()) * mappedTex.RowPitch;
			UINT colStart =  (int)(pixelBoundaryPos[j].x()) * 4;
			pTexels[rowStart + colStart + 0] = i == 0 ? 255 : 0;
			pTexels[rowStart + colStart + 1] = i == 1 ? 255 : 0;
			pTexels[rowStart + colStart + 2] = i == 2 ? 255 : 0; 
			pTexels[rowStart + colStart + 3] = 255;*/
				
			/*pTexels[rowStart + colStart + 0] = edgeInsetLevel == 0 ? 255 : 0;
			pTexels[rowStart + colStart + 1] = edgeInsetLevel == 1 ? 255 : 0;
			pTexels[rowStart + colStart + 2] = edgeInsetLevel == 2 ? 255 : 0; 
			pTexels[rowStart + colStart + 3] = 255;*/
		}
		piece.endPoints[i] = pixelBoundaryPos[endPoints[i]];
	}

	/* Compute stats */
	for (int i=0; i<4; i++) {
		//std::ofstream out1("out3.txt", std::ofstream::trunc | std::ofstream::out);
		//std::ofstream out2("out4.txt", std::ofstream::trunc | std::ofstream::out);

		Vector3f edgeVec(piece.endPoints[(i+1)%4].x() - piece.endPoints[i].x(), piece.endPoints[(i+1)%4].y() - piece.endPoints[i].y(), 0);
		float edgeLen = edgeVec.norm();
		Vector3f up(0, 0, 1);
		Vector3f edgeNor(up.cross(edgeVec).normalized());
		piece.edgeNor[i] = Vector2f(edgeNor.x(), edgeNor.y());

		int nZeros = 0;
		Vector2f previousPt(edges[i][0].pos);
		piece.totalCurvature[i] = 0;
		piece.totalLength[i] = 0;
		int nProjectedPoints = ceil(edgeLen);
		piece.projectedPoints[i].resize(nProjectedPoints);
		memset(piece.projectedPoints[i].data(), 0, nProjectedPoints*sizeof(float));
		//piece.edgeColors[i][edgeInsetLevel] = new Color[edges[i].size()];
		//piece.nEdgeColors[i][edgeInsetLevel] = edges[i].size();
		piece.edgeColors[i][edgeInsetLevel].resize(edges[i].size());
		for (int j=0; j<edges[i].size(); j++) {
			// Assign color
			piece.edgeColors[i][edgeInsetLevel][j] = tex(edges[i][j].pos.y(), edges[i][j].pos.x());
			//piece.edgeColors[i][edgeInsetLevel][j] = tex(edges[i][j].pos.y(), edges[i][j].pos.x());
			tex(edges[i][j].pos.y(), edges[i][j].pos.x()).w() = 0;

			// Compute total curvature and len 
			piece.totalCurvature[i] += abs(edges[i][j].k);
			if (abs(edges[i][j].k) < .01) nZeros++;
			piece.totalLength[i] += (previousPt - edges[i][j].pos).norm();

			// Z-Buffer
			Vector2f & pt1 = previousPt;
			Vector2f & pt2 = edges[i][j].pos;
			auto Project = [&] (Vector2f & point) {
				Vector2f l(point-piece.endPoints[i]);
				float y = Vector2f(edgeNor.x(), edgeNor.y()).dot(l);
				float x = sqrt(abs(l.squaredNorm() - y*y));
				//out1 << x << ' ' << y << std::endl;
				return Vector2f(x,y);
			};
			Vector2f xy1(Project(pt1));
			Vector2f xy2(Project(pt2));
			int xLow = ceilf((xy1.x()/edgeLen)*nProjectedPoints);
			xLow = xLow < nProjectedPoints ? xLow : nProjectedPoints-1;
			int xHigh = floorf((xy2.x()/edgeLen)*nProjectedPoints);
			xHigh = xHigh < nProjectedPoints ? xHigh : nProjectedPoints-1;
			if (j>0){
				for (int k=xLow; k<=xHigh; k++) {
					float t = (xy2.x()-xy1.x());
					if (t != 0) t = ((float)xLow-(xy1.x()/edgeLen)*nProjectedPoints)/t;
					float y = (1-t)*xy1.y()+t*xy2.y();
					if (y >= 0 && piece.projectedPoints[i][k] < y) piece.projectedPoints[i][k] = y;
					else if (y <= 0 && piece.projectedPoints[i][k] > y) piece.projectedPoints[i][k] = y;
				}
			}

			//UINT rowStart = (int)(edges[i][j].pos.y()+4*piece.edgeNor[i].y()) * mappedTex.RowPitch;
			//UINT colStart =  (int)(edges[i][j].pos.x()+4*piece.edgeNor[i].x()) * 4;
			/*UINT rowStart = (int)(edges[i][j].pos.y()) * mappedTex.RowPitch;
			UINT colStart =  (int)(edges[i][j].pos.x()) * 4;
			pTexels[rowStart + colStart + 0] = edgeInsetLevel == 0 ? 255 : 0;
			pTexels[rowStart + colStart + 1] = edgeInsetLevel == 1 ? 255 : 0;
			pTexels[rowStart + colStart + 2] = edgeInsetLevel == 2 ? 255 : 0; 
			pTexels[rowStart + colStart + 3] = 255;*/

			previousPt = edges[i][j].pos;
		}
		//for (int ii=0; ii<nProjectedPoints; ii++) {
		//	out2 << ii << ' ' << piece.projectedPoints[i][ii] << std::endl;
		//}
		if ((float)nZeros/edges[i].size() > .75) {
			piece.edgeCovered[i] = 1;
			piece.edgeIsBorder[i] = 1;
			piece.isBorderPiece = 1;
		}

		//out1.close();
		//out2.close();
	}

	/* Sort the curvatures 
	struct SortData {
		float k;
		int index;
	};
	std::vector<SortData> data(nPoints);
	for (int i=0; i<nPoints; i++) {data[i].k = abs(curvatures[i]); data[i].index = i; }
	qsort(data.data(), data.size(), sizeof(SortData), CompareCurvature);
	for (int i=0; i<10; i++)
		out << data[i].k << std::endl; */

	//if (piece.isBorderPiece) OutputDebugStringA("here\n");
	//pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));

	/*std::ofstream stream("C:\\Users\\Aric\\Desktop\\cs231a\\FinalProject\\code\\curve_lab\\New folder\\test2.mat", std::ios::out | std::ios::binary);
	for (int i=0; i<curves[1].size(); i++) {
		stream << curves[1][i].x();
		stream << ' ';
		stream << curves[1][i].y();
		stream << '\n';
	}*/
}

void JPuzzle::AddPiece()
{
	//border pieces
	if (m_nPiecesAdded == 1) {
		//AssemblyBorder();
		AssemblyBorderMST();
	}
	//inner pieces
	else if (m_nPiecesAdded+1 <= m_nPuzzlePieces) {
		m_nPiecesAdded++;
		ComparePieces();
		//MatchPocket(FindPockets());
		
	}
	Sleep(150);
}

void JPuzzle::AssemblyBorder()
{
	std::vector<EdgeLinkInfo> links; links.resize(1);
		std::vector<std::vector<float> > assignMatrix;
		std::vector<PuzzlePiece*> borderPieces;
		//MatrixXf mat;

		int startidx = 0;
		borderPieces.push_back(m_AddedPuzzlePieces[0]);

		for (std::vector<PuzzlePiece*>::iterator it = m_NotAddedPuzzlePieces.begin(); it != m_NotAddedPuzzlePieces.end(); ++it) {
			if ((*it)->isBorderPiece){
				borderPieces.push_back(*it);
			}
		}
		//std::cout << borderPieces.size();

		for (std::vector<PuzzlePiece*>::iterator it_r = borderPieces.begin(); it_r != borderPieces.end(); ++it_r) {
			std::vector<float> row;
			for (std::vector<PuzzlePiece*>::iterator it_c = borderPieces.begin(); it_c != borderPieces.end(); ++it_c) {
				int leftIdx = (*it_r)->left();
				int rightIdx = (*it_c)->right();
				links[0].a = *it_c;
				links[0].b = *it_r;
				links[0].k = rightIdx;
				links[0].l = leftIdx;
				if(CompareEdgesByShape(links) < 3.5)
					row.push_back(CompareEdgesByColor(links));
				else
					row.push_back(100000);
				//cout << Sim((*it_)->right(), (*it)->left()) << ' ' << (*it_)->orientation << ' ' << (*it)->orientation << endl;
			}
			assignMatrix.push_back(row);
		}
		std::list<int> assignment;
	
		assignment.push_back(startidx);
		while (assignment.size() <borderPieces.size()-1){
			
			//extend to the left
			int idxL = assignment.back();
			std::vector<float> row = assignMatrix[idxL];
			float min = FLT_MAX;
			float second_min = FLT_MAX;
			int minidx = -1;
			for (int i = 0; i<row.size(); ++i) {
				if (idxL == i)
					continue;
				bool found = false;
				for (std::list<int>::iterator j = assignment.begin(); j != assignment.end(); ++j){
					if (i == *j){
						found = true;
						break;
					}
				}
				if (found)
					continue;

				if (row[i] < min){
					second_min = min;
					min = row[i];
					minidx = i;
				}
				else if (row[i] < second_min){
					second_min = row[i];
				}
			}
			float confidence = min / second_min;

			//extend to the right
			int idxR = assignment.front();
			min = FLT_MAX;
			second_min = FLT_MAX;
			int minidxR = -1;
			for (int i = 0; i<assignMatrix.size(); ++i) {
				if (idxR == i)
					continue;
				bool found = false;
				for (std::list<int>::iterator j = assignment.begin(); j != assignment.end(); ++j){
					if (i == *j){
						found = true;
						break;
					}

				}
				if (found)
					continue;

				if (assignMatrix[i][idxR] < min){
					second_min = min;
					min = assignMatrix[i][idxR];
					minidxR = i;
				}
				else if (assignMatrix[i][idxR] < second_min){
					second_min = assignMatrix[i][idxR];
				}
			}

			if ((min / second_min) > confidence){
				//idx = minidx;
				assignment.push_back(minidx);
			}
			else{
				//idx = minidxR;
				assignment.push_front(minidxR);
				startidx++;
			}

		}
		for (int i = 0; i<assignMatrix.size(); ++i) {
			bool found = false;
			for (std::list<int>::iterator j = assignment.begin(); j != assignment.end(); ++j){
					if (i == *j){
						found = true;
						break;
					}
			}
			if(!found){
				int idxR = assignment.front();
				int idxL = assignment.back();
				if(assignMatrix[idxL][i] < assignMatrix[i][idxR])
					assignment.push_back(i);
				else{
					assignment.push_front(i);
					startidx++;
				}
				break;
			}
		}

		std::list<int>::iterator it = assignment.begin();
		std::advance(it, startidx);
		
		std::list<int>::iterator it_left = it;
		std::list<int>::iterator it_right = it;
		while (m_nPiecesAdded < borderPieces.size()){
			EdgeLinkInfo measure;
			if(++it_left != assignment.end()){
				measure.a = borderPieces[*(--it_left)];
				measure.b = borderPieces[*(++it_left)];
				measure.k = (measure.a)->left();
				measure.l = (measure.b)->right();			
				//float measure;
				for (int i = 0; i < m_NotAddedPuzzlePieces.size(); ++i){
					if (measure.b == m_NotAddedPuzzlePieces[i]){
						//measure.j = i;
						break;
					}
				}
				MovePiece(measure);
				measure.a->adjPieces[measure.k] = measure.b;
				measure.a->edgeCovered[measure.k] = 1;
				measure.b->adjPieces[measure.l] = measure.a;
				measure.b->edgeCovered[measure.l] = 1;
				measure.b->isAdded = 1;
				m_nPiecesAdded++;
			}
			if(it_left == assignment.end())
				--it_left;
			if(it_right != assignment.begin()){
				measure.a = borderPieces[*(it_right)];
				measure.b = borderPieces[*(--it_right)];
				measure.k = (measure.a)->right();
				measure.l = (measure.b)->left();			
				//float measure;
				for (int i = 0; i < m_NotAddedPuzzlePieces.size(); ++i){
					if (measure.b == m_NotAddedPuzzlePieces[i]){
						//measure.j = i;
						break;
					}
				}
				MovePiece(measure);
				measure.a->adjPieces[measure.k] = measure.b;
				measure.a->edgeCovered[measure.k] = 1;
				measure.b->adjPieces[measure.l] = measure.a;
				measure.b->edgeCovered[measure.l] = 1;
				measure.b->isAdded = 1;
				m_nPiecesAdded++;
			}
			  
		}

		PuzzlePiece * toLink[2] = {0,0};
		for (int i=0, count=0; i<m_nPiecesAdded; i++) {
			int sum=0;
			for (int j=0; j<4; j++)
				sum += m_AddedPuzzlePieces[i]->adjPieces[j] ? 1 : 0;
			if (sum == 1)
				toLink[count++] = m_AddedPuzzlePieces[i];
		}
		
		int edgeIndices[2];
		for (int i=0; i<2; i++) {
			int sum = (int)toLink[i]->edgeIsBorder[0]+(int)toLink[i]->edgeIsBorder[1]+
				(int)toLink[i]->edgeIsBorder[2]+(int)toLink[i]->edgeIsBorder[3];
			if (sum == 1) {
				int index=0;
				for (; toLink[i]->adjPieces[index] == NULL; index++) {}
				edgeIndices[i] = (index+2)%4;
			} else {
				edgeIndices[i] = 0;
				for (; toLink[i]->adjPieces[edgeIndices[i]] != NULL || toLink[i]->edgeIsBorder[edgeIndices[i]]; edgeIndices[i]++) {}
			}
		}
		for (int i=0; i<2; i++) {
			toLink[i]->adjPieces[edgeIndices[i]] = toLink[(i+1)%2];
			toLink[i]->edgeCovered[edgeIndices[i]] = 1;
		}

		/*
		PuzzlePiece * previous = m_AddedPuzzlePieces[2];
		PuzzlePiece * next = m_AddedPuzzlePieces[0];
		while (next != NULL) {
			char buf[256];
			sprintf(buf, "\n%i\n", next->index);
			OutputDebugStringA(buf);
			bool notFound=1;
			for (int i=3; i>=0; i--)  {
				if (next->adjPieces[i] != NULL && next->adjPieces[i] != previous) {
					previous = next;
					next = next->adjPieces[i];
					notFound=0;
					break;
				}
			}
			if(notFound) break;

		}*/

		// Link first piece with last piece
		/*int firstEdgeIndex=0, lastEdgeIndex=0;
		while (m_AddedPuzzlePieces[0]->edgeCovered[firstEdgeIndex]) firstEdgeIndex++;
		while (m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeIsBorder[lastEdgeIndex]) lastEdgeIndex++;
		if (m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeCovered[(lastEdgeIndex+1)%4]) 
			lastEdgeIndex = lastEdgeIndex-1 < 0 ? 3 : lastEdgeIndex-1;
		m_AddedPuzzlePieces[0]->adjPieces[firstEdgeIndex] = m_AddedPuzzlePieces[m_nPiecesAdded-1];
		m_AddedPuzzlePieces[0]->edgeCovered[firstEdgeIndex] = 1;
		m_AddedPuzzlePieces[m_nPiecesAdded-1]->adjPieces[lastEdgeIndex] = m_AddedPuzzlePieces[0];
		m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeCovered[lastEdgeIndex] = 1;*/
}

void JPuzzle::AssemblyBorderMST()
{
	struct BorderStrip{
		std::list<PuzzlePiece*> pieces;
		BorderStrip(PuzzlePiece* p) {pieces.push_back(p);}
		void addToLeft(PuzzlePiece* p) {pieces.push_front(p);}
		void addToRight(BorderStrip& bs) {pieces.insert(pieces.end(), bs.pieces.begin(), bs.pieces.end());}
	};
	
	std::vector<EdgeLinkInfo> links; links.resize(1);
	std::vector<std::vector<float> > assignMatrix;
	std::vector<BorderStrip> borderStrips;
	//MatrixXf mat;

	int startidx = 0;
	borderStrips.push_back(BorderStrip(m_AddedPuzzlePieces[0]));

	for (std::vector<PuzzlePiece*>::iterator it = m_NotAddedPuzzlePieces.begin(); it != m_NotAddedPuzzlePieces.end(); ++it) {
		if ((*it)->isBorderPiece){
			borderStrips.push_back(BorderStrip(*it));
		}
	}

	for (std::vector<BorderStrip>::iterator it_r = borderStrips.begin(); it_r != borderStrips.end(); ++it_r) {
		std::vector<float> row;//row i col j: i lies to the right of j
		for (std::vector<BorderStrip>::iterator it_c = borderStrips.begin(); it_c != borderStrips.end(); ++it_c) {
			int leftIdx = it_r->pieces.front()->left();
			int rightIdx = it_c->pieces.front()->right();
			links[0].a = it_c->pieces.front();
			links[0].b = it_r->pieces.front();
			links[0].k = rightIdx;
			links[0].l = leftIdx;
			if(CompareEdgesByShape(links) < 3.5)
				row.push_back(CompareEdgesByColor(links));
			else
				row.push_back(100000);
			//cout << Sim((*it_)->right(), (*it)->left()) << ' ' << (*it_)->orientation << ' ' << (*it)->orientation << endl;
		}
		assignMatrix.push_back(row);
	}
	
	while(borderStrips.size()>1) {
		float min = 1000000;
		int min_i = -1;
		int min_j = -1;
		for(int i=0; i<assignMatrix.size() ; ++i)
			for(int j=0; j<assignMatrix[i].size(); ++j){
				if (i != j && assignMatrix[i][j] < min) {
					min = assignMatrix[i][j];
					min_i = i;
					min_j = j;
				}
			}
		//minimum found, combine strips
		borderStrips[min_i].addToRight(borderStrips[min_j]);
		std::vector<BorderStrip>::iterator b_it = borderStrips.begin();
		std::advance(b_it, min_j);
		borderStrips.erase(b_it);
		//update assignMatrix
		assignMatrix[min_i] = assignMatrix[min_j];
		std::vector<std::vector<float>>::iterator it = assignMatrix.begin();
		std::advance(it, min_j);
		assignMatrix.erase(it);
		for(int i=0; i<assignMatrix.size() ; ++i){
			std::vector<float>::iterator row_it = assignMatrix[i].begin();
			std::advance(row_it, min_j);
			assignMatrix[i].erase(row_it);
		}
	}

	std::list<PuzzlePiece*>::iterator it_left;
	std::list<PuzzlePiece*>::iterator it_right;

	std::list<PuzzlePiece*> border = borderStrips[0].pieces;
	for(std::list<PuzzlePiece*>::iterator it = border.begin(); it != border.end(); ++it) {
		if(*it == m_AddedPuzzlePieces[0]){
			it_left = it;
			it_right = it;
		}
	}
	
	while (m_nPiecesAdded < border.size()){
			EdgeLinkInfo measure;
			if(++it_left != border.end()){
				measure.a = *(--it_left);
				measure.b = *(++it_left);
				measure.k = (measure.a)->left();
				measure.l = (measure.b)->right();			
				//float measure;
				for (int i = 0; i < m_NotAddedPuzzlePieces.size(); ++i){
					if (measure.b == m_NotAddedPuzzlePieces[i]){
						//measure.j = i;
						break;
					}
				}
				MovePiece(measure);
				measure.a->adjPieces[measure.k] = measure.b;
				measure.a->edgeCovered[measure.k] = 1;
				measure.b->adjPieces[measure.l] = measure.a;
				measure.b->edgeCovered[measure.l] = 1;
				measure.b->isAdded = 1;
				m_nPiecesAdded++;
			}
			if(it_left == border.end())
				--it_left;
			if(it_right != border.begin()){
				measure.a = *(it_right);
				measure.b = *(--it_right);
				measure.k = (measure.a)->right();
				measure.l = (measure.b)->left();			
				//float measure;
				for (int i = 0; i < m_NotAddedPuzzlePieces.size(); ++i){
					if (measure.b == m_NotAddedPuzzlePieces[i]){
						//measure.j = i;
						break;
					}
				}
				MovePiece(measure);
				measure.a->adjPieces[measure.k] = measure.b;
				measure.a->edgeCovered[measure.k] = 1;
				measure.b->adjPieces[measure.l] = measure.a;
				measure.b->edgeCovered[measure.l] = 1;
				measure.b->isAdded = 1;
				m_nPiecesAdded++;
			}
			  
		}

		PuzzlePiece * toLink[2] = {0,0};
		for (int i=0, count=0; i<m_nPiecesAdded; i++) {
			int sum=0;
			for (int j=0; j<4; j++)
				sum += m_AddedPuzzlePieces[i]->adjPieces[j] ? 1 : 0;
			if (sum == 1)
				toLink[count++] = m_AddedPuzzlePieces[i];
		}
		
		int edgeIndices[2];
		for (int i=0; i<2; i++) {
			int sum = (int)toLink[i]->edgeIsBorder[0]+(int)toLink[i]->edgeIsBorder[1]+
				(int)toLink[i]->edgeIsBorder[2]+(int)toLink[i]->edgeIsBorder[3];
			if (sum == 1) {
				int index=0;
				for (; toLink[i]->adjPieces[index] == NULL; index++) {}
				edgeIndices[i] = (index+2)%4;
			} else {
				edgeIndices[i] = 0;
				for (; toLink[i]->adjPieces[edgeIndices[i]] != NULL || toLink[i]->edgeIsBorder[edgeIndices[i]]; edgeIndices[i]++) {}
			}
		}
		for (int i=0; i<2; i++) {
			toLink[i]->adjPieces[edgeIndices[i]] = toLink[(i+1)%2];
			toLink[i]->edgeCovered[edgeIndices[i]] = 1;
		}

		/*
		PuzzlePiece * previous = m_AddedPuzzlePieces[2];
		PuzzlePiece * next = m_AddedPuzzlePieces[0];
		while (next != NULL) {
			char buf[256];
			sprintf(buf, "\n%i\n", next->index);
			OutputDebugStringA(buf);
			bool notFound=1;
			for (int i=3; i>=0; i--)  {
				if (next->adjPieces[i] != NULL && next->adjPieces[i] != previous) {
					previous = next;
					next = next->adjPieces[i];
					notFound=0;
					break;
				}
			}
			if(notFound) break;

		}*/

		// Link first piece with last piece
		/*int firstEdgeIndex=0, lastEdgeIndex=0;
		while (m_AddedPuzzlePieces[0]->edgeCovered[firstEdgeIndex]) firstEdgeIndex++;
		while (m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeIsBorder[lastEdgeIndex]) lastEdgeIndex++;
		if (m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeCovered[(lastEdgeIndex+1)%4]) 
			lastEdgeIndex = lastEdgeIndex-1 < 0 ? 3 : lastEdgeIndex-1;
		m_AddedPuzzlePieces[0]->adjPieces[firstEdgeIndex] = m_AddedPuzzlePieces[m_nPiecesAdded-1];
		m_AddedPuzzlePieces[0]->edgeCovered[firstEdgeIndex] = 1;
		m_AddedPuzzlePieces[m_nPiecesAdded-1]->adjPieces[lastEdgeIndex] = m_AddedPuzzlePieces[0];
		m_AddedPuzzlePieces[m_nPiecesAdded-1]->edgeCovered[lastEdgeIndex] = 1;*/
}
int CompareEdgeMeasures(const void * a, const void * b) 
{
	if (*(float*)a <  *(float*)b) return (int)-1;
	if (*(float*)a == *(float*)b) return (int)0;
	if (*(float*)a >  *(float*)b) return (int)1;
}

void JPuzzle::FindNeighbors(PuzzlePiece & a, PuzzlePiece & b, int k, int l, std::vector<EdgeLinkInfo> & links)
{
	auto FindAdjEdgePiece = [] (PuzzlePiece * center, int adjEdgeIndex, PuzzlePiece *& next, int & nextEdgeIndex, int dir) {
		next = center->adjPieces[adjEdgeIndex];
		if (next == 0) return false;
		nextEdgeIndex = 0;
		while (next->adjPieces[nextEdgeIndex] != center) nextEdgeIndex++;
		if (dir>0) nextEdgeIndex = nextEdgeIndex-1 < 0 ? 3 : nextEdgeIndex-1;
		else nextEdgeIndex = (nextEdgeIndex+1)%4;
		return next->edgeCovered[nextEdgeIndex];
	};

	auto FindAdjLink = [&FindAdjEdgePiece, &b, &links] (PuzzlePiece * center, int centerEdgeIndex, int & notAddedEdgeIndex, PuzzlePiece *& next, int & nextEdgeIndex, int dir) {
		PuzzlePiece * right = 0;
		int rightEdgeIndex = 0;
		int index = 0;
		if (dir>0) index = centerEdgeIndex-1 < 0 ? 3 : centerEdgeIndex-1;
		else index = (centerEdgeIndex+1)%4;
		if (!FindAdjEdgePiece(center, index, right, rightEdgeIndex, dir)) return 0;
		else {
			if (right->edgeIsBorder[rightEdgeIndex]) return 0;
			//int asdf=0;
		//	FindAdjEdgePiece(center, centerEdgeIndex-1 < 0 ? 3 : centerEdgeIndex-1, right, rightEdgeIndex);
			if (FindAdjEdgePiece(right, rightEdgeIndex, next, nextEdgeIndex, dir)) {
				if (next->edgeIsBorder[nextEdgeIndex]) return 0;
				DebugBreak();
			} else {
				EdgeLinkInfo eInfo;
				eInfo.a = next;
				eInfo.b = &b;
				eInfo.k = nextEdgeIndex;
				if (dir<0) notAddedEdgeIndex = notAddedEdgeIndex-1 < 0 ? 3 : notAddedEdgeIndex-1;
				else notAddedEdgeIndex = (notAddedEdgeIndex+1)%4;
				eInfo.l = notAddedEdgeIndex;
				links.push_back(eInfo);
				return 1;
			}
		}
		return 0;
	};

	PuzzlePiece * currentAdded = &a;
	int currentAddedEdgeIndex = k;
	int currentNonAddedEdgeIndex = l;
	PuzzlePiece * next = 0;
	int nextEdgeIndex = 0;
	while (FindAdjLink(currentAdded, currentAddedEdgeIndex, currentNonAddedEdgeIndex, next, nextEdgeIndex, 1) && next != &a) {
		currentAdded = next;
		currentAddedEdgeIndex = nextEdgeIndex;
	}

	if (links.size() < 4) {
		currentAdded = &a;
		currentAddedEdgeIndex = k;
		currentNonAddedEdgeIndex = l;

		while (FindAdjLink(currentAdded, currentAddedEdgeIndex, currentNonAddedEdgeIndex, next, nextEdgeIndex, -1) && next != &a) {
			currentAdded = next;
			currentAddedEdgeIndex = nextEdgeIndex;
		}
	}

	if (links.size() < 4) {
		// Center edge
		EdgeLinkInfo eInfo;
		eInfo.a = &a;
		eInfo.b = &b;
		eInfo.k = k;
		eInfo.l = l;
		links.push_back(eInfo);
	}
}

void JPuzzle::ComparePieces()
{
	/* Compute the measures */
	int largestLink=0;
	std::vector<EdgeLinkInfo> links;
	int nMeasures = 0;
	EdgeLinkInfo * measures = new EdgeLinkInfo[m_AddedPuzzlePieces.size()*m_NotAddedPuzzlePieces.size()*4*4]; 
	for (int i=0; i<m_AddedPuzzlePieces.size(); i++) {
		for (int j=0; j<m_NotAddedPuzzlePieces.size(); j++) {
			if (m_NotAddedPuzzlePieces[j]->isAdded) continue;
			for (int k=0; k<4; k++) {
				for (int l=0; l<4; l++) {
					if (k==3 && l==1)
						int aa=0;
					if (!m_AddedPuzzlePieces[i]->edgeCovered[k] && !m_NotAddedPuzzlePieces[j]->edgeCovered[l]) {
						if (m_AddedPuzzlePieces[i]->index == 3 && m_NotAddedPuzzlePieces[j]->index == 4) 
							int asdf=0;
						FindNeighbors(*m_AddedPuzzlePieces[i], *m_NotAddedPuzzlePieces[j], k, l, links);
						
						/*if (links.size() == 4) {
							int asdf=0;
							links.resize(0);
							FindNeighbors(*m_AddedPuzzlePieces[i], *m_NotAddedPuzzlePieces[j], k, l, links);
						}*/
						float val = CompareEdgesByShape(links);
						if (val < 10000 && links.size() >= largestLink) {
							if (links.size() > largestLink) { 
								largestLink = links.size();
								nMeasures = 0; //reset
							}
							
							measures[nMeasures].measure = val/links.size();
							measures[nMeasures].a = m_AddedPuzzlePieces[i];
							measures[nMeasures].b = m_NotAddedPuzzlePieces[j];
							//measures[nMeasures].j = j;
							measures[nMeasures].k = k;
							measures[nMeasures++].l = l;
						}
						links.resize(0);
					}
				}
			}
		}
	}
	if (nMeasures == 0) 
		DebugBreak();
	qsort(measures, nMeasures, sizeof(EdgeLinkInfo), CompareEdgeMeasures);
	float maxShapeMeasure = measures[nMeasures-1].measure;

	// Compare by Color
	std::ofstream out7("out7.txt", std::ofstream::out | std::ofstream::trunc);
	//char buf[256];
	//sprintf(buf, "\n%i\n", nMeasures);
	//OutputDebugStringA(buf);
	int count=0;
	for (int i=0; i<nMeasures; i++) {
		if (measures[i].measure < 3) count++;
		out7 << measures[i].measure << std::endl;
	}

	int nNewMeasures = min(count, nMeasures);
	for (int i=0; i<nNewMeasures; i++) {
		FindNeighbors(*measures[i].a, *measures[i].b, measures[i].k, measures[i].l, links);
		measures[i].measure = CompareEdgesByColor(links); links.resize(0);
	}
	EdgeLinkInfo * oldMeasures = new EdgeLinkInfo[nMeasures];
	memcpy(oldMeasures, measures, sizeof(EdgeLinkInfo)*nMeasures);
	qsort(measures, nNewMeasures, sizeof(EdgeLinkInfo), CompareEdgeMeasures);
	float maxColorMeasure = measures[nNewMeasures-1].measure;
	
	for (int i=0; i<nMeasures; i++) {
		if (i < nNewMeasures)
			out7 << oldMeasures[i].measure*(maxShapeMeasure/maxColorMeasure) << std::endl;
		else out7 << 0 << std::endl;
	}
	out7.close();

	MovePiece(measures[0]);

	FindNeighbors(*measures[0].a, *measures[0].b, measures[0].k, measures[0].l, links); 
	for (int i=0; i<links.size(); i++) {
		links[i].a->adjPieces[links[i].k] = links[i].b;
		links[i].a->edgeCovered[links[i].k] = 1;
		links[i].b->adjPieces[links[i].l] = links[i].a;
		links[i].b->edgeCovered[links[i].l] = 1;
		links[i].b->isAdded = 1;
	}

	delete[] measures;
	delete[] oldMeasures;
}

float JPuzzle::CompareEdgesByShape(std::vector<EdgeLinkInfo> & links) 
{					
	float measure = 0;
	for (int i=0; i<links.size(); i++) {
		PuzzlePiece & a=*links[i].a;
		PuzzlePiece & b=*links[i].b;
		int k=links[i].k, l=links[i].l;

		float dist = abs((a.endPoints[k]-a.endPoints[(k+1)%4]).norm() - (b.endPoints[l]-b.endPoints[(l+1)%4]).norm());
		if (dist > 12) return FLT_MAX;

		//return abs(a.totalLength[k] - b.totalLength[l]);
		//return abs((a.endPoints[k]-a.endPoints[(k+1)%4]).norm() - (b.endPoints[l]-b.endPoints[(l+1)%4]).norm());

		//std::ofstream out1("out1.txt");
		//std::ofstream out2("out2.txt");

		std::vector<float> * longProjectedPoints;
		std::vector<float> * shortProjectedPoints;
	
		if (a.projectedPoints[k].size() > b.projectedPoints[l].size()) {
			longProjectedPoints = &a.projectedPoints[k], shortProjectedPoints = &b.projectedPoints[l];
		} else {
			shortProjectedPoints = &a.projectedPoints[k], longProjectedPoints = &b.projectedPoints[l];
		}

		auto Compare = [&] (int offset) {
			float sum=0;
			int longEdgeSize = longProjectedPoints->size();
			for (int i=0, end=shortProjectedPoints->size(); i<end; i++) {
				float hs = (*shortProjectedPoints)[end-i-1];
				//out1 << i << ' ' << hs << std::endl;
				float hl = (*longProjectedPoints)[i+offset];
				//out2 << i << ' ' << -hl << std::endl;
				sum += abs(hs+hl);
			}
			return sum;
		};

		int offset = (longProjectedPoints->size()-shortProjectedPoints->size())/2;
		measure += dist + Compare(offset)/shortProjectedPoints->size();
	}
	return measure;
}

float JPuzzle::CompareEdgesByColor(std::vector<EdgeLinkInfo> & links) 
{
	float measure = 0;
	for (int i=0; i<links.size(); i++) {
		PuzzlePiece & a=*links[i].a;
		PuzzlePiece & b=*links[i].b;
		int k=links[i].k, l=links[i].l;

		//std::ofstream out1("out1.txt");
		//std::ofstream out2("out2.txt");

		int layerIndex=m_MaxColorLayers-2;
		int minSize = a.edgeColors[k][layerIndex+1].size();
		if (b.edgeColors[l][layerIndex+1].size() < minSize) {
			minSize = b.edgeColors[l][layerIndex+1].size();
		}
	
		auto ExtractLeftRightColors = [minSize,k,l,&a,&b] (int layerIndex, Color * leftColors, Color * rightColors) {
			int offsetA=a.edgeColors[k][layerIndex].size()-minSize;
			int offsetB=b.edgeColors[l][layerIndex].size()-minSize;
			for (int i=0; i<minSize; i++) {
				leftColors[i] = a.edgeColors[k][layerIndex][i+offsetA];
				rightColors[i] = b.edgeColors[l][layerIndex][offsetB+(minSize-i-1)];
				//leftColors[i] = a.edgeColors[k][layerIndex][i];
				//rightColors[i] = b.edgeColors[l][layerIndex][b.edgeColors[l][layerIndex].size()-i-1];
			}
		};

		if (minSize >= 1024)
			DebugBreak();

		ExtractLeftRightColors(layerIndex, m_LeftColors[1], m_RightColors[0]);
		ExtractLeftRightColors(layerIndex+1, m_LeftColors[0], m_RightColors[1]);
		measure += MGC(m_LeftColors, m_RightColors, minSize, minSize);//*/

		//if (k==2 && l==0)
		//	int a=0;

		/*std::vector<Color> left[2];
		std::vector<Color> right[2];
		int minSize = a.edgeColors[k][m_MaxColorLayers-1].size();
		if (b.edgeColors[l][m_MaxColorLayers-1].size() < minSize) {
			minSize = b.edgeColors[l][m_MaxColorLayers-1].size();
		}

		left[0] = std::vector<Color>(a.edgeColors[k][m_MaxColorLayers-1].begin(), a.edgeColors[k][m_MaxColorLayers-1].begin()+minSize);
		left[1] = std::vector<Color>(a.edgeColors[k][(m_MaxColorLayers-2)].begin(), a.edgeColors[k][(m_MaxColorLayers-2)].begin() + minSize);

		right[0] = std::vector<Color>();
		right[1] = std::vector<Color>();

		std::vector<Color>::iterator it_r0 = b.edgeColors[l][(m_MaxColorLayers-2)].end();
		std::vector<Color>::iterator it_r1 = b.edgeColors[l][m_MaxColorLayers-1].end();

		for(int i=0; i<minSize; ++i){
			right[0].push_back(*(--it_r0));
			right[1].push_back(*(--it_r1));
		}

		Color * leftColors[2];
		Color * rightColors[2];
		leftColors[0] = left[0].data();
		leftColors[1] = left[1].data();
		rightColors[0] = right[0].data();
		rightColors[1] = right[1].data();
		measure += MGC(leftColors, rightColors, minSize, minSize);//*/
	}
	return measure;
}

void JPuzzle::Render(ID3D10Device * pDevice)
{
	if (GetAsyncKeyState(VK_RETURN))
		AddPiece();

	m_World = Matrix4f::Identity();
	static float scale = 1.;
	static Vector2f trans;
	const float scaleAmount = .01;
	const float transAmount = .01/scale;
	if (GetAsyncKeyState(VK_LBUTTON)) {
		scale += scaleAmount;
	} else if (GetAsyncKeyState(VK_RBUTTON)) {
		scale -= scaleAmount;
	} else if (GetAsyncKeyState(VK_UP)) {
		trans.y() -= transAmount;
	} else if (GetAsyncKeyState(VK_DOWN)) {
		trans.y() += transAmount;
	} else if (GetAsyncKeyState(VK_LEFT)) {
		trans.x() += transAmount;
	} else if (GetAsyncKeyState(VK_RIGHT)) {
		trans.x() -= transAmount;
	}
	if (scale < .1) scale = .1;
	m_World(0,0) = scale;
	m_World(1,1) = scale;
	m_World(0, 3) = scale*trans.x();
	m_World(1, 3) = scale*trans.y();
 
	for (int i=0; i<m_nPiecesAdded; i++) {
		m_pSRVPuzzleTextureFx->SetResource(m_AddedPuzzlePieces[i]->SRVPuzzleTexture);
		Matrix4f T = m_World*m_AddedPuzzlePieces[i]->transform;
		m_pWorldfx->SetMatrix(T.data());

		D3D10_TECHNIQUE_DESC techDesc;
		m_pTechnique->GetDesc( &techDesc );
		for( UINT p = 0; p < techDesc.Passes; ++p ) {
			m_pTechnique->GetPassByIndex(p)->Apply( 0 );
			pDevice->DrawIndexed(6, 0, 0);
		}
	}
}

void JPuzzle::Destroy()
{
	delete[] m_PuzzlePieces;
	m_PuzzlePieces = NULL;
	m_nPuzzlePieces = 0;

	if(m_pVBQuad) m_pVBQuad->Release();
    if(m_pIBQuad) m_pIBQuad->Release();
    if(m_pVertexLayout) m_pVertexLayout->Release();
	//for (int i=0; i<m_nPuzzlePieces; i++)
	//	if (m_rgSRVPuzzleTexture[i]) m_rgSRVPuzzleTexture[i]->Release();
    if(m_pEffect) m_pEffect->Release();
}

void JPuzzle::MovePiece(EdgeLinkInfo & measure)
{
        /* Align the pieces */
        EdgeLinkInfo & best = measure;
        PuzzlePiece & a = *best.a;
        PuzzlePiece & b = *best.b;
        Vector4f eA0(2.f*(a.endPoints[best.k].x() / g_TextureSize) - 1, -2.f*(a.endPoints[best.k].y() / g_TextureSize) + 1, 1, 1); eA0 = a.transform*eA0;
        Vector4f eA1(2.f*(a.endPoints[(best.k + 1) % 4].x() / g_TextureSize) - 1, -2.f*(a.endPoints[(best.k + 1) % 4].y() / g_TextureSize) + 1, 1, 1); eA1 = a.transform*eA1;
        Vector4f eB0(2.f*(b.endPoints[best.l].x() / g_TextureSize) - 1, -2.f*(b.endPoints[best.l].y() / g_TextureSize) + 1, 1, 1); eB0 = b.transform*eB0;
        Vector4f eB1(2.f*(b.endPoints[(best.l + 1) % 4].x() / g_TextureSize) - 1, -2.f*(b.endPoints[(best.l + 1) % 4].y() / g_TextureSize) + 1, 1, 1); eB1 = b.transform*eB1;

        Vector2f vA(eA1.x() - eA0.x(), eA1.y() - eA0.y()); vA.normalize();
        Vector2f vB(eB0.x() - eB1.x(), eB0.y() - eB1.y()); vB.normalize();

        // Compute b's transform
        float cr = vB.x()*vA.y() - vB.y()*vA.x();
        float dot = vB.dot(vA);
        if (dot < -1) dot = -1;
        if (dot > 1) dot = 1;
        float theta = acos(dot);
        if (cr < 0) theta = -theta;
		float unitTheta = round(theta/(D3DX_PI/2.f));
		theta = unitTheta*D3DX_PI/2.f;
        Matrix3f rot; rot = AngleAxisf(theta, Vector3f::UnitZ());
        Vector3f rotatedaasdf = rot*Vector3f(vB[0], vB[1], 0);

        Matrix4f R = Matrix4f::Identity();
        for (int i = 0; i<3; i++)
        for (int j = 0; j<3; j++)
                R(i, j) = rot(i, j);
        Matrix4f T1 = Matrix4f::Identity();
        T1(0, 3) = -.5f*(eB0.x() + eB1.x());
        T1(1, 3) = -.5f*(eB0.y() + eB1.y());
        Matrix4f T2 = Matrix4f::Identity();
        T2(0, 3) = .5f*(eA0.x() + eA1.x());
        T2(1, 3) = .5f*(eA0.y() + eA1.y());

        b.transform = T2*R*T1;
		b.rotation = R;

        Vector4f asdf = T2*R*T1*Vector4f(eB0.x(), eB0.y(), 0, 1);
        Vector4f asdf2 = T2*R*T1*Vector4f(eB1.x(), eB1.y(), 0, 1);
        Vector4f dif = asdf2 - asdf;

        // Update puzzle info
		m_AddedPuzzlePieces.push_back(&b);
}

MatrixXd dummyCov(MatrixXd& mat, Vector3d& mu) 
{
	int rows = mat.rows();
	MatrixXd M(rows + 9, 3);
	M << mat, 0, 0, 0, 1, 1, 1, -1, -1, -1, 0, 0, 1, 0, 1, 0, 1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1;
	VectorXd ones;
	ones.setOnes(rows + 9);

	Matrix3d S = 1.0 / (rows - 1)*(M - ones*mu.transpose()).transpose()*(M - ones*mu.transpose());

	return S.inverse();
}

float JPuzzle::MGC(Color ** left, Color ** right, int leftSize, int rightSize) {
	
	int rows = leftSize;

	MatrixXd GL(rows, 3), GR(rows, 3), GijLR(rows, 3), GjiRL(rows, 3);
	Vector3d uiL(0.0, 0.0, 0.0);
	Vector3d ujR(0.0, 0.0, 0.0);

	for (int r = 0; r < rows; ++r) {
		Color rgb[4];

		rgb[0] = left[0][r];
		rgb[1] = left[1][r];

		rgb[2] = right[0][r];
		rgb[3] = right[1][r];
		
		for (int ch = 0; ch<3; ch++){
			GL(r, ch) = rgb[1][ch] - rgb[0][ch];
			GR(r, ch) = rgb[2][ch] - rgb[3][ch];
			GijLR(r, ch) = rgb[2][ch] - rgb[1][ch];
			GjiRL(r, ch) = rgb[1][ch] - rgb[2][ch];
			uiL(ch) += GL(r, ch);
			ujR(ch) += GR(r, ch);
		}
	}
	uiL /= rows;
	ujR /= rows;

	VectorXd ones;
	ones.setOnes(rows);

	MatrixXd SiLpinv = dummyCov(GL, uiL);//pinv(SiL);
	MatrixXd SjRpinv = dummyCov(GR, ujR);//pinv(SjR);

	double DLR = 0.0;//ones.dot(lr);
	double DRL = 0.0;//ones.dot(rl);

	MatrixXd XLR = (GijLR - ones*uiL.transpose());
	MatrixXd XRL = (GjiRL - ones*ujR.transpose());

	for (int r = 0; r<rows; r++) {
		DLR += XLR.row(r)*SiLpinv*XLR.row(r).transpose();
		DRL += XRL.row(r)*SjRpinv*XRL.row(r).transpose();
	}
	return sqrt(DLR) + sqrt(DRL);
}

std::vector<JPuzzle::Pocket> JPuzzle::FindPockets(){
	//find uncovered edges from added pieces
	//find two edges sharing one endpoint and almost perpendicular
	//return Pockets
	std::vector<Pocket> pockets;
	return pockets;
}
/*
void JPuzzle::MatchPocket(std::vector<Pocket> pockets) {
	//iterate through all the pockets and find the one with the best "confidence"
	EdgeLinkInfo bestPocket;
	bestPocket.measure = FLT_MAX;
	for(std::vector<Pocket>::iterator p_it = pockets.begin(); p_it != pockets.end(); ++p_it) {
		 PuzzlePiece* bestMatch;
		 std::vector<float> sim1;
		 std::vector<float> sim2;
		 for (std::vector<PuzzlePiece*>::iterator it = m_NotAddedPuzzlePieces.begin(); it != m_NotAddedPuzzlePieces.end(); ++it) {
			 //TODO:Shape????
			 //orientation 1
				sim1.push_back(CompareEdgesByColor(*(p_it->a), **it, p_it->k, 0));
				sim2.push_back(CompareEdgesByColor(*(p_it->b), **it, p_it->l, 1));
				//orientation 2
				sim1.push_back(CompareEdgesByColor(*(p_it->a), **it, p_it->k, 1));
				sim2.push_back(CompareEdgesByColor(*(p_it->b), **it, p_it->l, 2));
				//orientation 3
				sim1.push_back(CompareEdgesByColor(*(p_it->a), **it, p_it->k, 2));
				sim2.push_back(CompareEdgesByColor(*(p_it->b), **it, p_it->l, 3));
				//orientation 4
				sim1.push_back(CompareEdgesByColor(*(p_it->a), **it, p_it->k, 3));
				sim2.push_back(CompareEdgesByColor(*(p_it->b), **it, p_it->l, 0));

                //sim = simL + simT - alpha*sqrt(simL*simL+simT*simT-(simL+simT)*(simL+simT)/4.0);
                //cout << sim << endl;
		 }
        float max[2]={FLT_MAX,FLT_MAX}, second_max[2]={FLT_MAX,FLT_MAX};
        for(int k=0; k<sim1.size(); k++){
            if(sim1[k]<max[0]){
                second_max[0] = max[0];
                max[0] = sim1[k];
            }
            else if(sim1[k]<second_max[0]){
                second_max[0] = sim1[k];
            }

            if(sim2[k]<max[1]){
                second_max[1] = max[1];
                max[1] = sim2[k];
            }
            else if(sim2[k]<second_max[1]){
                second_max[1] = sim2[k];
            }
        }
        float sim = FLT_MAX;
        /*if(sim1.size()==1){
            Pocket p;
            p.best_fit = *(pool.begin());
            p.i = i;
            p.j = j;
            //p.best_fit_it = pool.begin();
            p.sim = 2.0;
            p.p = type;
            
            return p;
        }//
        int max_idx = -1;
        for(int k=0; k<sim1.size(); k++){
            float s1=0;
            if(sim1[k])
                s1 = sim1[k]/second_max[0];
            float s2 = 0;
            if(sim2[k])
                s2 = sim2[k]/second_max[1];
			float alpha = 1.f;
            float s = s1 + s2 + alpha*sqrt(s1*s1+s2*s2-(s1+s2)*(s1+s2)/4.f);
     
            //waitKey(0);
            if(s < sim){
                sim = s;
                max_idx = k;
            }
        }
		if(sim < bestPocket.measure) {
			int pidx = max_idx/4;
			int orientation = max_idx%4;
			std::vector<PuzzlePiece*>::iterator it = m_NotAddedPuzzlePieces.begin();
			std::advance(it, max_idx);
			bestPocket.a = p_it->a;
			bestPocket.b = *it;
			bestPocket.k = p_it->k;
			bestPocket.l = orientation;
			//bestPocket.j = max_idx;
			bestPocket.measure = sim;
		}
        //result[i*m+j] = *it;
        //cout << max_idx << endl;
	}
	MovePiece(bestPocket);
	//move the piece
	//add the piece
	//book keeping
}
*/