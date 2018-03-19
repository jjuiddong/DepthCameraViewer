
#include "stdafx.h"
#include "plyreader.h"

using namespace graphic;

cPlyReader::cPlyReader()
{
}

cPlyReader::~cPlyReader()
{
	Clear();
}


// *.ply file read
bool cPlyReader::Read(const string &fileName)
{
	using namespace std;

	Clear();

	const int fileSize = (int)common::FileSize(fileName);

	// find data pointer
	ifstream ifs(fileName, ios::binary);
	if (!ifs.is_open())
		return false;

	char fmt[3];
	ifs.read(fmt, 3);
	if (('p' != fmt[0]) || ('l' != fmt[1]) || ('y' != fmt[2]))
		return false;

	bool isHeaderRead = false;
	while (!ifs.eof() && !isHeaderRead)
	{
		int idx = 0;
		char line[256];
		ZeroMemory(line, ARRAYSIZE(line));
		while (ifs.read(line + idx, 1))
		{
			if ('\n' == line[idx])
			{
				line[idx-1] = NULL; // '\r'
				line[idx] = NULL;  // '\n'
				break;
			}
			++idx;
		}

		if (string(line) == "end_header")
		{
			isHeaderRead = true;
			break;
		}
	}

	if (!isHeaderRead)
		return false;


#pragma pack(push, 1)
	struct sVtx 
	{
		float x, y, z;
		unsigned char r, g, b;
	};
#pragma pop();

	m_vertices.reserve(219104);
	m_colors.reserve(219104);

	for (int i = 0; i < 219104; ++i)
	{
		sVtx vtx;
		ifs.read((char*)&vtx, sizeof(vtx));
		m_vertices.push_back(Vector3(vtx.x, vtx.y, vtx.z));

		const cColor color(vtx.r, vtx.g, vtx.b);
		m_colors.push_back(color);
	}

	return true;
}


void cPlyReader::Clear()
{
}
