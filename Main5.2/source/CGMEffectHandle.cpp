#include "stdafx.h"
#include "ZzzTexture.h"
#include "NewUISystem.h"
#include "CGMEffectHandle.h"
#include "MonkSystem.h"
#include "Protocol.h"
#include "ZzzInventory.h"
#include "ZzzOpenglUtil.h"

#include <string>
#include <vector>

#ifdef EFFECT_MNG_HANDLE

SEASON3B::CGFxEffectHandle::CGFxEffectHandle()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;
	m_bEditEnchant = FALSE;
	selectedItem = -1;
	m_ItemSection = 0;
	m_ItemType = 0;
	m_SpawnCount = 1;
	m_SpawnLevel = 0;
	m_SpawnSkill = 0;
	m_SpawnLuck = 0;
	m_SpawnOption = 0;
	m_SpawnExc = 0;
	m_SpawnSet = 0;
	m_SpawnSocket = 0;
	m_bPreviewModelOk = false;
	memset(m_szPreviewModelError, 0, sizeof(m_szPreviewModelError));

	memset(&m_bSettings, 0, sizeof(m_bSettings));
}

SEASON3B::CGFxEffectHandle::~CGFxEffectHandle()
{
	this->Release();
}

bool SEASON3B::CGFxEffectHandle::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if (pNewUIMng)
	{
		m_pNewUIMng = pNewUIMng;

		m_pNewUIMng->AddUIObj(INTERFACE_EFFECT_MANAGER, this);

		this->LoadImages();

		this->runtime_colector_Enchant("Data\\Local\\RenderMesh.bmd");

		this->SetButtonInfo();

		this->SetPos(x, y);

		Show(false);

		return true;
	}
	return false;

}

void SEASON3B::CGFxEffectHandle::Release()
{
	this->UnloadImages();

	if (m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void SEASON3B::CGFxEffectHandle::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool SEASON3B::CGFxEffectHandle::UpdateMouseEvent()
{
	return !SEASON3B::CheckMouseIn(0, 0, GetWindowsX, GetWindowsY - 51);
}

bool SEASON3B::CGFxEffectHandle::UpdateKeyEvent()
{
	if (IsVisible())
	{
		if (SEASON3B::IsPress(VK_ESCAPE))
		{
			g_pNewUISystem->Hide(INTERFACE_EFFECT_MANAGER);
			return false;
		}

		bool changed = false;
		const DWORD nowTick = GetTickCount();
		static int s_holdKey = 0;
		static DWORD s_nextRepeatTick = 0;

		auto applyKey = [&](int vk, int sectionDelta, int typeDelta) -> bool
		{
			const bool pressed = SEASON3B::IsPress(vk);
			const bool held = SEASON3B::IsRepeat(vk);

			if (!held && s_holdKey == vk)
			{
				s_holdKey = 0;
				s_nextRepeatTick = 0;
			}

			if (pressed)
			{
				m_ItemSection += sectionDelta;
				m_ItemType += typeDelta;
				s_holdKey = vk;
				s_nextRepeatTick = nowTick + 300;
				return true;
			}

			if (held && s_holdKey == vk && nowTick >= s_nextRepeatTick)
			{
				m_ItemSection += sectionDelta;
				m_ItemType += typeDelta;
				s_nextRepeatTick = nowTick + 90;
				return true;
			}

			return false;
		};

		if (applyKey(VK_UP, -1, 0))
		{
			changed = true;
		}
		else if (applyKey(VK_DOWN, 1, 0))
		{
			changed = true;
		}
		else if (applyKey(VK_LEFT, 0, -1))
		{
			changed = true;
		}
		else if (applyKey(VK_RIGHT, 0, 1))
		{
			changed = true;
		}

		if (changed != false)
		{
			if (m_ItemSection < 0) m_ItemSection = 0;
			if (m_ItemSection > 15) m_ItemSection = 15;
			if (m_ItemType < 0) m_ItemType = 0;
			if (m_ItemType > 511) m_ItemType = 511;

			SyncItemIndexFromSectionType();
			return false;
		}
	}
	return true;
}

bool SEASON3B::CGFxEffectHandle::Update()
{
	if (IsVisible())
	{
	}
	return true;
}

bool SEASON3B::CGFxEffectHandle::Render()
{
	if (IsVisible() == false)
	{
		return true;
	}

	EnableAlphaTest();

	glColor4f(1.f, 1.f, 1.f, 1.f);

	//draw_list.clear();

	this->RenderFrame();

	//render_draw_list();

	DisableAlphaBlend();

	return true;
}

void SEASON3B::CGFxEffectHandle::LoadImages()
{
	LoadBitmap("Interface\\HUD\\EDIT\\effect_back_mng.tga", IMAGE_EFFECT_BACK_MNG, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\EDIT\\effect_btn_edit.tga", IMAGE_EFFECT_BTN_EDIT, GL_LINEAR);
	LoadBitmap("Interface\\HUD\\EDIT\\effect_btn_save.tga", IMAGE_EFFECT_BTN_SAVE, GL_LINEAR);
}

void SEASON3B::CGFxEffectHandle::UnloadImages()
{
	DeleteBitmap(IMAGE_EFFECT_BACK_MNG);
	DeleteBitmap(IMAGE_EFFECT_BTN_EDIT);
	DeleteBitmap(IMAGE_EFFECT_BTN_SAVE);
}

void SEASON3B::CGFxEffectHandle::SetButtonInfo()
{

}

void SEASON3B::CGFxEffectHandle::SyncItemIndexFromSectionType()
{
	selectedItem = (m_ItemSection * 512) + m_ItemType;
}

bool SEASON3B::CGFxEffectHandle::IsGmUser() const
{
	if (Hero == nullptr)
	{
		return false;
	}

	if ((Hero->CtlCode & CTLCODE_20OPERATOR) != 0 || (Hero->CtlCode & CTLCODE_08OPERATOR) != 0)
	{
		return true;
	}

	return g_isCharacterBuff((&Hero->Object), eBuff_GMEffect) != 0;
}

void SEASON3B::CGFxEffectHandle::SendItemSpawn(BYTE action) const
{
	if (selectedItem < 0)
	{
		return;
	}

	PMSG_GM_ITEM_SPAWN_RECV pMsg;
	pMsg.header.set(0xF3, 0xF2, sizeof(pMsg));
	pMsg.action = action;
	pMsg.itemIndex[0] = SET_NUMBERHB((WORD)selectedItem);
	pMsg.itemIndex[1] = SET_NUMBERLB((WORD)selectedItem);
	pMsg.level = (BYTE)((m_SpawnLevel < 0) ? 0 : (m_SpawnLevel > 15) ? 15 : m_SpawnLevel);
	pMsg.skill = (BYTE)((m_SpawnSkill != 0) ? 1 : 0);
	pMsg.luck = (BYTE)((m_SpawnLuck != 0) ? 1 : 0);
	pMsg.option = (BYTE)((m_SpawnOption < 0) ? 0 : (m_SpawnOption > 7) ? 7 : m_SpawnOption);
	pMsg.exc = (BYTE)((m_SpawnExc < 0) ? 0 : (m_SpawnExc > 63) ? 63 : m_SpawnExc);
	pMsg.set = (BYTE)((m_SpawnSet < 0) ? 0 : (m_SpawnSet > 255) ? 255 : m_SpawnSet);
	pMsg.socket = (BYTE)((m_SpawnSocket < 0) ? 0 : (m_SpawnSocket > 5) ? 5 : m_SpawnSocket);
	pMsg.count = (WORD)((m_SpawnCount < 1) ? 1 : (m_SpawnCount > 100) ? 100 : m_SpawnCount);
	DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void SEASON3B::CGFxEffectHandle::SendClearInventoryKeepEquipped() const
{
	PMSG_GM_CLEAR_INVENTORY_RECV pMsg;
	pMsg.header.set(0xF3, 0xF4, sizeof(pMsg));
	DataSend((BYTE*)&pMsg, pMsg.header.size);
}

void SEASON3B::CGFxEffectHandle::reload_collection_item(int currentIndex)
{
	if (comboBoxItems.empty())
	{
		for (int i = 0; i < MAX_ITEM_LINE; i++)
		{
			Script_Item* item_info = GMItemMng->find(i);

			if (item_info->Name[0] != '\0')
			{
				comboBoxItems.push_back(i, item_info->Name);
			}
		}
	}

	if (currentIndex == 1 && selectedItem != -1)
	{
		comboBoxGroup.clear();

		BMD* pModel = gmClientModels->GetModel(selectedItem + MODEL_ITEM);
		if (pModel)
		{
			char nameId[50];
			for (int i = 0; i < pModel->NumMeshs; i++)
			{
					sprintf_s(nameId, "GroupId %d", i + 1);
				comboBoxGroup.push_back(i, nameId);
			}

			VectorCopy(pModel->BodyLight, m_bSettings.Color);
		}
	}
}

float SEASON3B::CGFxEffectHandle::GetLayerDepth()
{
	return 10.5f;
}

float SEASON3B::CGFxEffectHandle::GetKeyEventOrder()
{
	return 10.f;
}

void SEASON3B::CGFxEffectHandle::OpenningProcess()
{
}

void SEASON3B::CGFxEffectHandle::ClosingProcess()
{
}

void SEASON3B::CGFxEffectHandle::AddEnchantEffect(int itemindex, const EnchantEffect& effect)
{
	mapeEnchant[itemindex].buffer.push_back(effect);
}


bool SEASON3B::CGFxEffectHandle::runtime_colector_Enchant(std::string filename)
{
	EnchantEffect* item_info;
	int MaxLine = PackFileDecrypt(filename, item_info, 0, sizeof(EnchantEffect), 0xE2F1);

	if (MaxLine != 0 && item_info != NULL)
	{
		for (int i = 0; i < MaxLine; i++)
		{
			this->AddEnchantEffect(item_info[i].itemindex, item_info[i]);
		}
	}
	SAFE_DELETE_ARRAY(item_info);

	return false;
}

void SEASON3B::CGFxEffectHandle::runtime_export_settings(std::string filename)
{
	FILE* fp = fopen(filename.c_str(), "wb");

	std::string buffer;
	buffer += std::to_string((int)(selectedItem >> 9));
	buffer += "    " + std::to_string((int)(selectedItem % 512));
	buffer += "    " + std::to_string(m_bSettings.Color[0]);
	buffer += "    " + std::to_string(m_bSettings.Color[1]);
	buffer += "    " + std::to_string(m_bSettings.Color[2]);
	buffer += "    " + std::to_string(m_bSettings.GroupId);

	buffer += "    " + std::to_string(m_bSettings.RenderFlag);
	buffer += "    " + std::to_string(m_bSettings.RenderType);
	buffer += "    " + std::to_string(m_bSettings.TimeEffectType);
	buffer += "    " + std::to_string(m_bSettings.TextureID);
	buffer += "    " + std::to_string(m_bSettings.TimeTextureID);
	buffer += "    1";
	if (buffer.size() > 0)
	{
		buffer += "\n";
		fwrite(buffer.data(), 1, buffer.size(), fp);
		buffer.clear();
	}
	fclose(fp);
}

bool SEASON3B::CGFxEffectHandle::runtime_render_Enchant(int modelType, BMD* pModel, OBJECT* pObject)
{
	bool is_rendered = false;
	int itemindex = g_CMonkSystem.EqualItemModelType(modelType);

	if (m_bEditEnchant && itemindex == selectedItem)
	{
		is_rendered = true;
		const EnchantEffect* Enchant = &m_bSettings;

		for (int i = 0; i < pModel->NumMeshs; i++)
		{
			if (i != Enchant->GroupId)
			{
				pModel->RenderMesh(i, 2, pObject->Alpha, pObject->BlendMesh, pObject->BlendMeshLight, pObject->BlendMeshTexCoordU, pObject->BlendMeshTexCoordV, -1);
			}
			else
			{
				runtime_interpret_setting(Enchant, pModel, pObject);
			}
		}
	}
	else
	{
		const std::vector<EnchantEffect>& effects = GetEnchantEffects(itemindex);

		if (!effects.empty())
		{
			for (size_t i = 0; i < effects.size(); i++)
			{
				const EnchantEffect* Enchant = &effects[i];

				runtime_interpret_setting(Enchant, pModel, pObject);
			}
			is_rendered = true;
		}
	}

	return is_rendered;
}

bool SEASON3B::CGFxEffectHandle::runtime_render_NoGlow(int modelType, BMD* pModel, OBJECT* pObject, int RenderType, float Alpha)
{
	bool is_rendered = false;
	int itemindex = g_CMonkSystem.EqualItemModelType(modelType);

	const std::vector<EnchantEffect>& effects = GetEnchantEffects(itemindex);

	if (!effects.empty())
	{
		for (size_t i = 0; i < effects.size(); i++)
		{
			const EnchantEffect* Enchant = &effects[i];

			if (Enchant->NoGlow)
			{
				pModel->RenderMesh(Enchant->GroupId, RenderType, Alpha, pObject->BlendMesh, pObject->BlendMeshLight, pObject->BlendMeshTexCoordU, pObject->BlendMeshTexCoordV);
				is_rendered = true;
			}
		}
	}
	return is_rendered;
}


void SEASON3B::CGFxEffectHandle::runtime_interpret_setting(const EnchantEffect* Enchant, BMD* pModel, OBJECT* pObject)
{
	int blend_mesh = 0;
	float blend_mesh_light = 0;
	float blend_mesh_tex_coord_u = 0;
	float blend_mesh_tex_coord_v = 0;

	int TextureID = Enchant->TextureID;

	vec3_t BodyLight = { 0.0, 0.0, 0.0 };
	VectorCopy(pModel->BodyLight, BodyLight);

	if (Enchant->RenderFlag == RENDER_TEXTURE)
	{
		if (Enchant->Color[0] == 1.f && Enchant->Color[1] == 1.f && Enchant->Color[2] == 1.f)//PK Fixed
			glColor3fv(pModel->BodyLight);
		else
			glColor3f(Enchant->Color[0], Enchant->Color[1], Enchant->Color[2]);

		blend_mesh = pObject->BlendMesh;
	}
	else
	{
		VectorCopy(Enchant->Color, pModel->BodyLight);
		blend_mesh = Enchant->GroupId;
	}

	float timeeeee = Render22(-(Enchant->TimeTextureID), 0.0) * 0.7 + 0.2;

	if (Enchant->TimeTextureID == -1.f)
	{
		blend_mesh_light = pObject->BlendMeshLight;
	}
	else
	{
		blend_mesh_light = timeeeee;
	}

	int Effect01 = 1000;
	float Effect02 = ((int)(Effect01 - timeGetTime() * Enchant->TimeEffectType) % (Effect01 + 1)) / (double)Effect01;

	int Frame = (((int)((timeGetTime()) * Enchant->TimeEffectType)) % 600 / 40);
	double FrameX = (double)(Frame % 4) * 0.25;
	double FrameY = (double)(Frame / 4) * 0.25;
	float Frame2 = ((int)(timeGetTime() * Enchant->TimeEffectType) % 16 / 4) * 0.25;

	switch (Enchant->RenderType)
	{
	case 0://Movimiento en X
		blend_mesh_tex_coord_u = Effect02;
		blend_mesh_tex_coord_v = pObject->BlendMeshTexCoordV;
		break;
	case 1://Movimiento en Y
		blend_mesh_tex_coord_u = pObject->BlendMeshTexCoordU;
		blend_mesh_tex_coord_v = Effect02;
		break;
	case 2://Movimiento inverso en X
		blend_mesh_tex_coord_u = -Effect02;
		blend_mesh_tex_coord_v = pObject->BlendMeshTexCoordV;
		break;
	case 3://Movimiento inverso en Y
		blend_mesh_tex_coord_u = pObject->BlendMeshTexCoordU;
		blend_mesh_tex_coord_v = -Effect02;
		break;
	case 4://Frame 4x4
		blend_mesh_tex_coord_u = FrameX;
		blend_mesh_tex_coord_v = FrameY;
		break;
	case 5://Frame 1x4
		blend_mesh_tex_coord_u = pObject->BlendMeshTexCoordU;
		blend_mesh_tex_coord_v = Frame2;
		break;
	default://Sin Efecto
		blend_mesh_tex_coord_u = pObject->BlendMeshTexCoordU;
		blend_mesh_tex_coord_v = pObject->BlendMeshTexCoordV;
		break;
	}

	pModel->RenderMesh(Enchant->GroupId, Enchant->RenderFlag, pObject->Alpha, blend_mesh, blend_mesh_light, blend_mesh_tex_coord_u, blend_mesh_tex_coord_v, TextureID);
	VectorCopy(BodyLight, pModel->BodyLight);
}
#endif // EFFECT_MNG_HANDLE
