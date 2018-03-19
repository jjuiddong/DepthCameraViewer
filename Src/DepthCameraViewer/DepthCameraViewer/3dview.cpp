
#include "stdafx.h"
#include "3dview.h"
#include "plyreader.h"

using namespace graphic;
using namespace framework;

c3DView::c3DView(const string &name)
	: framework::cDockWindow(name)
	, m_groundPlane1(Vector3(0, 1, 0), 0)
	, m_groundPlane2(Vector3(0, -1, 0), 0)
	, m_showGround(true)
	, m_incT(0)
{
}

c3DView::~c3DView()
{
}


bool c3DView::Init(cRenderer &renderer)
{
	m_camera.SetCamera(Vector3(-100, 60, -100), Vector3(0, 0, 0), Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, m_rect.Width() / m_rect.Height(), 1, 10000.f);
	m_camera.SetViewPort(m_rect.Width(), m_rect.Height());

	sf::Vector2u size((u_int)m_rect.Width() - 15, (u_int)m_rect.Height() - 50);
	cViewport vp = renderer.m_viewPort;
	vp.m_vp.Width = (float)size.x;
	vp.m_vp.Height = (float)size.y;
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true
		, DXGI_FORMAT_D24_UNORM_S8_UINT);

	m_ground.Create(renderer, 100, 100, 10, 10);
	m_lineList.Create(renderer, 100);

	cPlyReader reader;
	if (reader.Read("../media/blank1.ply"))
	{
		m_vtxBuff.Create(renderer
			, reader.m_vertices.size()
			, sizeof(sVertex)
			, &reader.m_vertices[0]);
	}

	//using namespace std;
	//const string fileName = "../media/-_20180312-111908_0.dat";
	//const int fsize = (int)common::FileSize(fileName);
	//
	//ifstream ifs(fileName, ios::binary);
	//if (ifs.is_open())
	//{
	//	BYTE *pbuff = new BYTE[fsize];
	//	ZeroMemory(pbuff, fsize);
	//	ifs.read((char*)pbuff, fsize);

	//	m_vtxBuff.Create(renderer, 640 * 480, sizeof(sVertex), pbuff);

	//	delete[] pbuff;
	//}

	return true;
}


void c3DView::OnUpdate(const float deltaSeconds)
{
}


void c3DView::OnPreRender(const float deltaSeconds)
{
	cRenderer &renderer = GetRenderer();
	cAutoCam cam(&m_camera);

	renderer.UnbindTextureAll();

	GetMainCamera().Bind(renderer);
	GetMainLight().Bind(renderer);

	if (m_renderTarget.Begin(renderer))
	{
		CommonStates states(renderer.GetDevice());
		renderer.GetDevContext()->RSSetState(states.Wireframe());

		cShader11 *shader = renderer.m_shaderMgr.FindShader(eVertexType::POSITION);
		assert(shader);
		shader->SetTechnique("Unlit");
		shader->Begin();
		shader->BeginPass(renderer, 0);

		GetMainCamera().Bind(renderer);

		Transform tfm;
		tfm.pos = Vector3(0, 400, 0);
		tfm.scale = Vector3(1, 1, 1)*0.1f;
		tfm.rot.SetRotationX(MATH_PI*0.5f);

		renderer.m_cbPerFrame.m_v->mWorld = XMMatrixTranspose(tfm.GetMatrixXM());
		renderer.m_cbPerFrame.Update(renderer);
		renderer.m_cbLight.Update(renderer, 1);
		renderer.m_cbMaterial.Update(renderer, 2);

		m_vtxBuff.Bind(renderer);
		renderer.GetDevContext()->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
		renderer.GetDevContext()->DrawInstanced(640*480, 1, 0, 0);

		if (m_showGround)
			m_ground.Render(renderer);
	}
	m_renderTarget.End(renderer);
}


void c3DView::OnRender(const float deltaSeconds)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	m_viewPos = { (int)(pos.x), (int)(pos.y) };
	m_viewRect = { pos.x + 5, pos.y, pos.x + m_rect.Width() - 30, pos.y + m_rect.Height() - 50 };
	ImGui::Image(m_renderTarget.m_resolvedSRV, ImVec2(m_rect.Width() - 15, m_rect.Height() - 50));

	// HUD
	const float windowAlpha = 0.0f;
	bool isOpen = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(ImVec2(min(m_viewRect.Width(), 700), 500));
	if (ImGui::Begin("Information", &isOpen, ImVec2(700.f, 500.f), windowAlpha, flags))
	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Checkbox("Show Ground", &m_showGround);
		ImGui::End();
	}
}


void c3DView::OnResizeEnd(const framework::eDockResize::Enum type, const sRectf &rect)
{
	if (type == eDockResize::DOCK_WINDOW)
	{
		m_owner->RequestResetDeviceNextFrame();
	}
}


void c3DView::UpdateLookAt()
{
	GetMainCamera().MoveCancel();

	const float centerX = GetMainCamera().m_width / 2;
	const float centerY = GetMainCamera().m_height / 2;
	const Ray ray = GetMainCamera().GetRay((int)centerX, (int)centerY);
	const float distance = m_groundPlane1.Collision(ray.dir);
	if (distance < -0.2f)
	{
		GetMainCamera().m_lookAt = m_groundPlane1.Pick(ray.orig, ray.dir);
	}
	else
	{ // horizontal viewing
		const Vector3 lookAt = GetMainCamera().m_eyePos + GetMainCamera().GetDirection() * 50.f;
		GetMainCamera().m_lookAt = lookAt;
	}

	GetMainCamera().UpdateViewMatrix();
}


// 휠을 움직였을 때,
// 카메라 앞에 박스가 있다면, 박스 정면에서 멈춘다.
void c3DView::OnWheelMove(const float delta, const POINT mousePt)
{
	UpdateLookAt();

	float len = 0;
	const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
	Vector3 lookAt = m_groundPlane1.Pick(ray.orig, ray.dir);
	len = (ray.orig - lookAt).Length();

	// zoom in/out
	float zoomLen = 0;
	if (len > 100)
		zoomLen = 50;
	else if (len > 50)
		zoomLen = max(1.f, (len / 2.f));
	else
		zoomLen = (len / 3.f);

	//Vector3 eyePos = GetMainCamera().m_eyePos + ray.dir * ((delta <= 0) ? -zoomLen : zoomLen);
	GetMainCamera().Zoom(ray.dir, (delta < 0) ? -zoomLen : zoomLen);
}


// Handling Mouse Move Event
void c3DView::OnMouseMove(const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;


	if (m_mouseDown[0])
	{
		Vector3 dir = GetMainCamera().GetDirection();
		Vector3 right = GetMainCamera().GetRight();
		dir.y = 0;
		dir.Normalize();
		right.y = 0;
		right.Normalize();

		GetMainCamera().MoveRight(-delta.x * m_rotateLen * 0.001f);
		GetMainCamera().MoveFrontHorizontal(delta.y * m_rotateLen * 0.001f);
	}
	else if (m_mouseDown[1])
	{
		m_camera.Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		m_camera.Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));

		//GetMainCamera().Yaw2(delta.x * 0.005f, Vector3(0, 1, 0));
		//GetMainCamera().Pitch2(delta.y * 0.005f, Vector3(0, 1, 0));
	}
	else if (m_mouseDown[2])
	{
		const float len = GetMainCamera().GetDistance();
		GetMainCamera().MoveRight(-delta.x * len * 0.001f);
		GetMainCamera().MoveUp(delta.y * len * 0.001f);
	}
}


// Handling Mouse Button Down Event
void c3DView::OnMouseDown(const sf::Mouse::Button &button, const POINT mousePt)
{
	m_mousePos = mousePt;
	UpdateLookAt();
	SetCapture();

	switch (button)
	{
	case sf::Mouse::Left:
	{
		m_mouseDown[0] = true;
		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		Vector3 p1 = m_groundPlane1.Pick(ray.orig, ray.dir);
		m_rotateLen = min(500.f, (p1 - ray.orig).Length());
	}
	break;

	case sf::Mouse::Right:
	{
		m_mouseDown[1] = true;

		const Ray ray = GetMainCamera().GetRay(mousePt.x, mousePt.y);
		Vector3 target = m_groundPlane1.Pick(ray.orig, ray.dir);
		const float len = (GetMainCamera().GetEyePos() - target).Length();
	}
	break;

	case sf::Mouse::Middle:
		m_mouseDown[2] = true;
		break;
	}
}


void c3DView::OnMouseUp(const sf::Mouse::Button &button, const POINT mousePt)
{
	const POINT delta = { mousePt.x - m_mousePos.x, mousePt.y - m_mousePos.y };
	m_mousePos = mousePt;
	ReleaseCapture();

	switch (button)
	{
	case sf::Mouse::Left:
		m_mouseDown[0] = false;
		break;
	case sf::Mouse::Right:
		m_mouseDown[1] = false;
		break;
	case sf::Mouse::Middle:
		m_mouseDown[2] = false;
		break;
	}
}


void c3DView::OnEventProc(const sf::Event &evt)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (evt.type)
	{
	case sf::Event::KeyPressed:
		switch (evt.key.code)
		{
		case sf::Keyboard::Return:
			break;

		case sf::Keyboard::Space:
			break;

		//case sf::Keyboard::Left: m_camera.MoveRight(-0.5f); break;
		//case sf::Keyboard::Right: m_camera.MoveRight(0.5f); break;
		//case sf::Keyboard::Up: m_camera.MoveUp(0.5f); break;
		//case sf::Keyboard::Down: m_camera.MoveUp(-0.5f); break;
		}
		break;

	case sf::Event::MouseMoved:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnMouseMove(pos);
	}
	break;

	case sf::Event::MouseButtonPressed:
	case sf::Event::MouseButtonReleased:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		if (sf::Event::MouseButtonPressed == evt.type)
		{
			if (m_viewRect.IsIn((float)curPos.x, (float)curPos.y))
				OnMouseDown(evt.mouseButton.button, pos);
		}
		else
		{
			OnMouseUp(evt.mouseButton.button, pos);
		}
	}
	break;

	case sf::Event::MouseWheelScrolled:
	{
		cAutoCam cam(&m_camera);

		POINT curPos;
		GetCursorPos(&curPos); // sf::event mouse position has noise so we use GetCursorPos() function
		ScreenToClient(m_owner->getSystemHandle(), &curPos);
		const POINT pos = { curPos.x - m_viewPos.x, curPos.y - m_viewPos.y };
		OnWheelMove(evt.mouseWheelScroll.delta, pos);
	}
	break;
	}
}


void c3DView::OnResetDevice()
{
	cRenderer &renderer = GetRenderer();

	// update viewport
	sRectf viewRect = { 0, 0, m_rect.Width() - 15, m_rect.Height() - 50 };
	m_camera.SetViewPort(viewRect.Width(), viewRect.Height());

	cViewport vp = GetRenderer().m_viewPort;
	vp.m_vp.Width = viewRect.Width();
	vp.m_vp.Height = viewRect.Height();
	m_renderTarget.Create(renderer, vp, DXGI_FORMAT_R8G8B8A8_UNORM, true, true, DXGI_FORMAT_D24_UNORM_S8_UINT);
}

