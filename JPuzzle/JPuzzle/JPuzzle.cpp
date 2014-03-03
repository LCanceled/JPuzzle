
#include "JPuzzle.h"
#include <string>
#include <fstream>

JPuzzle::JPuzzle():m_pEffect(0), m_pTechnique(0), m_pVertexLayout(0), m_pVBQuad(0), m_pIBQuad(0), m_pSRVPuzzleTextureFx(0), m_pWorldfx(0)
{
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

HRESULT JPuzzle::Init(char * file, ID3D10Device * pDevice)
{
	HRESULT hr = CreateGraphics(pDevice);
	if (hr != S_OK) return hr;

	/* Load puzzle pieces and textures */
	std::string sFile(file);
	sFile += "/";
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind = FindFirstFileA((sFile+"*").c_str(), &findFileData);
	while (FindNextFileA(hFind, &findFileData) != 0) {
		if (strstr(findFileData.cFileName, "png")) {
			ID3D10ShaderResourceView * pRSV = NULL;
			D3DX10_IMAGE_LOAD_INFO loadInfo;
			ZeroMemory( &loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
			loadInfo.MipLevels = 1;
			loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			loadInfo.Usage = D3D10_USAGE_DYNAMIC;
			loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE;
			loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			if (FAILED(D3DX10CreateShaderResourceViewFromFileA(pDevice, (sFile+findFileData.cFileName).c_str(), &loadInfo, NULL, &pRSV, NULL))) 
				DebugBreak();
			m_rgSRVPuzzleTexture.push_back(pRSV);

			/* Create the puzzle piece */
			PuzzlePiece piece;
			ID3D10Texture2D * pTexture;
			loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			loadInfo.Usage = D3D10_USAGE_STAGING;
			loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_READ;
			loadInfo.BindFlags = 0;
			if (FAILED(D3DX10CreateTextureFromFileA(pDevice, (sFile+findFileData.cFileName).c_str(), &loadInfo, NULL, (ID3D10Resource**)&pTexture, NULL)))
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
			m_PuzzlePieces.push_back(piece);
			pTexture->Release();

			ProcessPuzzlePiece(pDevice);
		}
	}

	ComparePieces();
}

bool JPuzzle::OnOutsideBoundary(int i, int j, Texture & tex)
{
	if (tex(i, j).w() > 0)
		return 0;

	int k=-1+i;
	float ar[3][3];
	for (; k<=i+1; k++) {
		int l=-1+j;
		for (; l<=j+1; l++) {
			if (tex(k, l).w() > 0) 
				return 1;
			//ar[k-i+1][l-j+1] = tex(k,l).w();
		}
	}
	return 0;
}

void JPuzzle::ProcessPuzzlePiece(ID3D10Device * pDevice)
{
	PuzzlePiece & piece = m_PuzzlePieces.back();

	/* Find pixel on boundary */
	int i=piece.tex.height/2, j=0;
	for (; j<piece.tex.width; j++) {
		if (piece.tex(i, j).w() > 0) break;
	}
	j -= 1;
	int startX = j;
	int startY = i;

	bool * used = new bool[piece.tex.width*piece.tex.height];
	memset(used, 0, sizeof(bool)*piece.tex.width*piece.tex.height);

	D3D10_MAPPED_TEXTURE2D mappedTex;
	ID3D10Texture2D * pTexture;
	m_rgSRVPuzzleTexture.back()->GetResource((ID3D10Resource**)&pTexture);
	pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_WRITE_DISCARD, 0, &mappedTex);
	UCHAR* pTexels = (UCHAR*)mappedTex.pData;
	std::vector<Vector2f> linePos;
	for (; ;) {
		linePos.push_back(Vector2f(j, i));

		used[piece.tex.width*i + j] = 1;
		UINT rowStart = i * mappedTex.RowPitch;
		UINT colStart = j * 4;
		/*pTexels[rowStart + colStart + 0] = 255;
		pTexels[rowStart + colStart + 1] = 0;
		pTexels[rowStart + colStart + 2] = 0; 
		pTexels[rowStart + colStart + 3] = 255;*/

		int x=-1,y=-1;
		if (OnOutsideBoundary(i+1, j, piece.tex) && !used[piece.tex.width*(i+1) + j]) y=i+1,x=j;
		else if (OnOutsideBoundary(i-1, j, piece.tex) && !used[piece.tex.width*(i-1) + j]) y=i-1,x=j;
		else if (OnOutsideBoundary(i, j+1, piece.tex) && !used[piece.tex.width*(i) + j+1]) y=i,x=j+1;
		else if (OnOutsideBoundary(i, j-1, piece.tex) && !used[piece.tex.width*(i) + j-1]) y=i,x=j-1;
		else if (OnOutsideBoundary(i+1, j+1, piece.tex) && !used[piece.tex.width*(i+1) + j+1]) y=i+1,x=j+1;
		else if (OnOutsideBoundary(i-1, j+1, piece.tex) && !used[piece.tex.width*(i-1) + j+1]) y=i-1,x=j+1;
		else if (OnOutsideBoundary(i+1, j-1, piece.tex) && !used[piece.tex.width*(i+1) + j-1]) y=i+1,x=j-1;
		else if (OnOutsideBoundary(i-1, j-1, piece.tex) && !used[piece.tex.width*(i-1) + j-1]) y=i-1,x=j-1;
		else if (y == startY && x == startX)
			break;
		else
			break;

		i = y;
		j = x;
	}
	int nPoints = linePos.size();

	/* Compute curvatures */
	/*std::vector<float> curvatures;
	int nPoints = linePos.size();
	curvatures.resize(nPoints);
	const int curvatureSize=4;
	MatrixXf A(curvatureSize+1, 2);
	VectorXf b(curvatureSize+1);
	VectorXf s;
	for (int i=0, count=0; i<nPoints; i++) {
		count=0;
		for (int j=i-curvatureSize; j<=i; j++) {
			int index = j < 0 ? nPoints+j : j;
			A(count, 0) = linePos[index].x();
			A(count, 1) = 1;
			b(count) = linePos[index].y();
			count++;
		}
		s = ((A.transpose() * A).ldlt().solve(A.transpose() * b));
		float m1 = s(0);
		Vector2f v1(Vector2f(1, m1).normalized());
		v1 = (linePos[(i+4)%nPoints] - linePos[i]).normalized();
		//if (v1.dot(linePos[(i+1)%nPoints] - linePos[i]) < 0) v1 = -v1;

		count=0;
		for (int j=i; j<=(i+curvatureSize) % nPoints; j++) {
			A(count, 0) = linePos[j].x();
			A(count, 1) = 1;
			b(count) = linePos[j].y();
			count++;
		}
		s = ((A.transpose() * A).ldlt().solve(A.transpose() * b));
		float m2 = s(0);
		Vector2f v2(Vector2f(1, m2).normalized());
		v2 = (linePos[(i-4) < 0 ? nPoints-4 : i-4] - linePos[i]).normalized();
		//if (v2.dot(linePos[(i-1) < 0 ? nPoints-1 : (i-1)] - linePos[i]) < 0) v2 = -v2;

		curvatures[i] = (v1+v2).norm();
		UINT rowStart = (int)linePos[i].y() * mappedTex.RowPitch;
		UINT colStart =  (int)linePos[i].x() * 4;
		pTexels[rowStart + colStart + 0] = 0;
		pTexels[rowStart + colStart + 1] = 2*curvatures[i]*126;
		pTexels[rowStart + colStart + 2] = 0; 
		pTexels[rowStart + colStart + 3] = 255;

	}*/

	auto findClosestPt = [nPoints,&linePos,&mappedTex,&pTexels] (Vector2f pt) {
		float bestDistSq = FLT_MAX;
		int index = 0;
		for (int i=0; i<nPoints; i++) {
			float dist = (linePos[i] - pt).squaredNorm();
			if (dist < bestDistSq) {
				bestDistSq = dist;
				index = i;
			}
		}

		UINT rowStart = (int)linePos[index].y() * mappedTex.RowPitch;
		UINT colStart =  (int)linePos[index].x() * 4;
		pTexels[rowStart + colStart + 0] = 0;
		pTexels[rowStart + colStart + 1] = 255;
		pTexels[rowStart + colStart + 2] = 0; 
		pTexels[rowStart + colStart + 3] = 255;

		return index;
	};
    int endPoints[4];
	endPoints[0] = findClosestPt(Vector2f(0,0));
    endPoints[1] = findClosestPt(Vector2f(piece.tex.width-1,0));
    if (endPoints[1] - endPoints[0] < 0) {
		for (int i=0; i<nPoints/2; i++) {
			Vector2f t(linePos[i]);
			linePos[i] = linePos[nPoints-i-1];
			linePos[nPoints-i-1] = t;
		}
	}
	endPoints[0] = findClosestPt(Vector2f(0,0));
	endPoints[1] = findClosestPt(Vector2f(piece.tex.width-1,0));
	endPoints[2] = findClosestPt(Vector2f(piece.tex.width-1,piece.tex.height-1));
	endPoints[3] = findClosestPt(Vector2f(0,piece.tex.height-1));

	std::vector<Vector2f> curves[4];
	for (int i=0; i<4; i++) {
		curves[i].reserve(nPoints);
		for (int j=endPoints[i]; j!=endPoints[(i+1)%4]; j=(j+1)%nPoints) {
			curves[i].push_back(linePos[j]);
		}
	}
	for (int i=0; i<4; i++) {
		/*for (int j=0; j<curves[i].size(); j++) {
			UINT rowStart = (int)curves[i][j].y() * mappedTex.RowPitch;
			UINT colStart =  (int)curves[i][j].x() * 4;
			pTexels[rowStart + colStart + 0] = i == 0 ? 255 : 0;
			pTexels[rowStart + colStart + 1] = i == 1 ? 255 : 0;
			pTexels[rowStart + colStart + 2] = i == 2 ? 255 : 0; 
			pTexels[rowStart + colStart + 3] = 255;
		}*/
		piece.endPoints[i] = linePos[endPoints[i]];

		float bestDist = 0;
		Vector2f dir = (linePos[endPoints[(i+1)%4]] - linePos[endPoints[i]]).normalized();
		for (int j=0; j<curves[i].size(); j++) {
			Vector2f dir2(curves[i][j] - linePos[endPoints[i]]);
			float a = dir.dot(dir2);
			float c2 = dir2.squaredNorm();
			float b2 = c2-a*a;
			if (b2 > bestDist) {
				bestDist = b2;
				piece.maxPoint[i] = curves[i][j];
			}
		}
	}

	/*for (int i=0; i<4; i++) {
			UINT rowStart = (int)piece.maxPoint[i].y() * mappedTex.RowPitch;
			UINT colStart =  (int)piece.maxPoint[i].x() * 4;
			pTexels[rowStart + colStart + 0] = 255;
			pTexels[rowStart + colStart + 1] = 255;
			pTexels[rowStart + colStart + 2] = 255; 
			pTexels[rowStart + colStart + 3] = 255;
	}*/

	pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));

	/*std::ofstream stream("C:\\Users\\Aric\\Desktop\\cs231a\\FinalProject\\code\\curve_lab\\New folder\\test2.mat", std::ios::out | std::ios::binary);
	for (int i=0; i<curves[1].size(); i++) {
		stream << curves[1][i].x();
		stream << ' ';
		stream << curves[1][i].y();
		stream << '\n';
	}*/

}

void JPuzzle::ComparePieces()
{
	for (int i=0; i<1; i++) {
		PuzzlePiece & p1 = m_PuzzlePieces[i];
		PuzzlePiece & p2 = m_PuzzlePieces[i+1];
		for (int j=0; j<4; j++) {
			for (int k=0; k<4; k++) {
				p1.endPoints[0];
			}
		}
	}
}

void JPuzzle::Render(ID3D10Device * pDevice)
{
	m_World(0,0) = .5;
	m_World(1,1) = .5;
    m_pWorldfx->SetMatrix((float*)&m_World.transpose());
 
	static bool last = 0;
	if (GetAsyncKeyState(VK_SHIFT)) {
		last = !last;
		Sleep(300);
	}
	m_pSRVPuzzleTextureFx->SetResource(m_rgSRVPuzzleTexture[(int)last]);

	D3D10_TECHNIQUE_DESC techDesc;
    m_pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p ) {
        m_pTechnique->GetPassByIndex(p)->Apply( 0 );
        pDevice->DrawIndexed(6, 0, 0);
    }
}

void JPuzzle::Destroy()
{
	if(m_pVBQuad) m_pVBQuad->Release();
    if(m_pIBQuad) m_pIBQuad->Release();
    if(m_pVertexLayout) m_pVertexLayout->Release();
	for (int i=0, end=m_rgSRVPuzzleTexture.size(); i<end; i++)
		if (m_rgSRVPuzzleTexture[i]) m_rgSRVPuzzleTexture[i]->Release();
    if(m_pEffect) m_pEffect->Release();
}