//
// 2018-03-19, jjuiddong
// *.ply file reader
// *.ply file Generated Basler 3D Camera  
// 
// - reference
// https://github.com/vgvishesh/PLY_Loader
//
#pragma once


class cPlyReader
{
public:
	cPlyReader();
	virtual ~cPlyReader();

	bool Read(const string &fileName);
	void Clear();


public:
	vector<common::Vector3> m_vertices;
	vector<graphic::cColor> m_colors;
};
