#include "stdafx.h"
#include "ZzzTexture.h"
#include "NewUISystem.h"
#include "CGMEffectHandle.h"
#include "MonkSystem.h"
#include "Protocol.h"
#include "./Utilities/Log/ErrorReport.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include <set>

#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

#ifdef EFFECT_MNG_HANDLE

SEASON3B::CGFxEffectHandle::CGFxEffectHandle()
{
	m_pNewUIMng = NULL;
	m_Pos.x = 0;
	m_Pos.y = 0;
	m_bEditEnchant = 0;
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
	m_bEditEnchant = FALSE;

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

		if (SEASON3B::IsPress(VK_UP))
		{
			m_ItemSection--;
			changed = true;
		}
		else if (SEASON3B::IsPress(VK_DOWN))
		{
			m_ItemSection++;
			changed = true;
		}
		else if (SEASON3B::IsPress(VK_LEFT))
		{
			m_ItemType--;
			changed = true;
		}
		else if (SEASON3B::IsPress(VK_RIGHT))
		{
			m_ItemType++;
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

void SEASON3B::CGFxEffectHandle::RenderFrame()
{
	if (this->IsGmUser() == false)
	{
		g_pNewUISystem->Hide(INTERFACE_EFFECT_MANAGER);
		return;
	}

	// Iniciar frame de ImGui
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplWin32_NewFrame();
	// Iniciar frame de ImGui
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(m_Pos.x * g_fScreenRate_x, m_Pos.y * g_fScreenRate_y));
	ImGui::SetNextWindowSize(ImVec2(460.f * g_fScreenRate_x, 264.f * g_fScreenRate_y));

	static bool GraphicImage = false;

	ImGui::Begin("RenderMesh Tools", &GraphicImage, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	// Secci�n 1: Arriba izquierda
	ImGui::BeginChild("Child1", ImVec2((160 * g_fScreenRate_x), 0), ImGuiChildFlags_Border);

	ImGui::BeginChild("Sub-Child1", ImVec2(0, (110.f * g_fScreenRate_y)), ImGuiChildFlags_Border);
	ImGui::EndChild();

	ImGui::BeginChild("Sub-Child2", ImVec2(0, 0), ImGuiChildFlags_Border);

	//EditEnchantList.Render("##GroupDataMesh");

	ImGui::EndChild();

	// Contenido de la primera secci�n
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("Child2", ImVec2(0, 0), ImGuiChildFlags_Border);

	this->RenderContents();

	ImGui::EndChild();

	ImGui::End();

	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	//------------------
	if (selectedItem != -1 && m_bPreviewModelOk)
	{
		Script_Item* item_info = GMItemMng->find(selectedItem);

		if (item_info->Name[0] != '\0')
		{
			int wmodel = item_info->Width * 20;
			int hmodel = item_info->Height * 20;

			int posx = m_Pos.x + 80 - (wmodel / 2);
			int posy = m_Pos.y + 55 - (hmodel / 2);

			RenderLocalItem3D(posx, posy, wmodel, hmodel, selectedItem);
		}
	}
}

void SEASON3B::CGFxEffectHandle::RenderContents()
{
	SyncItemIndexFromSectionType();

	m_bPreviewModelOk = false;
	m_szPreviewModelError[0] = '\0';

	auto fileExists = [](const char* path) -> bool
		{
			const DWORD attr = GetFileAttributesA(path);
			if (attr == INVALID_FILE_ATTRIBUTES)
				return false;
			return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
		};

	auto guessModelPath = [](int section, int type, char* outPath, size_t outPathSize)
		{
			if (outPathSize == 0)
				return;

			outPath[0] = '\0';

			const char* dir = "Data\\Item\\";
			if (section >= 7 && section <= 11)
				dir = "Data\\Player\\";

			const char* prefix = 0;
			switch (section)
			{
			case 0: prefix = "Sword"; break;
			case 1: prefix = "Axe"; break;
			case 2: prefix = "Mace"; break;
			case 3: prefix = "Spear"; break;
			case 4: prefix = "Bow"; break;
			case 5: prefix = "Staff"; break;
			case 6: prefix = "Shield"; break;
			case 7: prefix = "HelmMale"; break;
			case 8: prefix = "ArmorMale"; break;
			case 9: prefix = "PantMale"; break;
			case 10: prefix = "GloveMale"; break;
			case 11: prefix = "BootMale"; break;
			case 12: prefix = "Wing"; break;
			case 13: prefix = "Helper"; break;
			default: prefix = 0; break;
			}

			if (prefix == 0)
				return;

			const int idx = type + 1;
			if (idx >= 10)
				sprintf_s(outPath, outPathSize, "%s%s%d.bmd", dir, prefix, idx);
			else
				sprintf_s(outPath, outPathSize, "%s%s0%d.bmd", dir, prefix, idx);
		};

	Script_Item* item_info = GMItemMng->find(selectedItem);

	auto kind2Name = [](BYTE kind2) -> const char*
		{
			switch (kind2)
			{
			case 1: return "Sword";
			case 2: return "Magic Sword";
			case 3: return "Fist";
			case 4: return "Axe";
			case 5: return "Mace";
			case 6: return "Scepter";
			case 7: return "Lance";
			case 8: return "Bow";
			case 9: return "Crossbow";
			case 10: return "Arrow";
			case 11: return "Bolt";
			case 12: return "Staff";
			case 13: return "Stick";
			case 14: return "Book";
			case 15: return "Shield";
			case 16: return "Helm";
			case 17: return "Armor";
			case 18: return "Pants";
			case 19: return "Gloves";
			case 20: return "Boots";
			case 23: return "Wings Lv1";
			case 24: return "Wings Lv2";
			case 25: return "Wings Lv3";
			case 76: return "Wings Lv4";
			case 80: return "Wings Power";
			case 29: return "Pendant";
			case 30: return "Wizard Pendant";
			case 31: return "Ring";
			case 43: return "Pentagram";
			case 44: return "Errtel";
			case 51: return "Potion";
			case 56: return "Jewel";
			case 63: return "Muun";
			default: return "Unknown";
			}
		};

	ImGui::Text("Item Selection");
	{
		ImGui::PushID("section");
		if (ImGui::ArrowButton("##up", ImGuiDir_Up)) m_ItemSection--;
		ImGui::SameLine();
		if (ImGui::ArrowButton("##down", ImGuiDir_Down)) m_ItemSection++;
		ImGui::SameLine();
		ImGui::Text("Section: %d", m_ItemSection);
		ImGui::PopID();
	}
	{
		ImGui::PushID("type");
		if (ImGui::ArrowButton("##left", ImGuiDir_Left)) m_ItemType--;
		ImGui::SameLine();
		if (ImGui::ArrowButton("##right", ImGuiDir_Right)) m_ItemType++;
		ImGui::SameLine();
		ImGui::Text("Type: %d", m_ItemType);
		ImGui::PopID();
	}

	if (m_ItemSection < 0) m_ItemSection = 0;
	if (m_ItemSection > 15) m_ItemSection = 15;
	if (m_ItemType < 0) m_ItemType = 0;
	if (m_ItemType > 511) m_ItemType = 511;

	SyncItemIndexFromSectionType();
	item_info = GMItemMng->find(selectedItem);

	ImGui::Text("Index: %d", selectedItem);

	if (item_info->Name[0] != '\0')
	{
		ImGui::Text("%s", item_info->Name);
	}
	else
	{
		ImGui::Text("No name in Item.bmd");
	}

	if (selectedItem >= 0)
	{
		const int modelType = MODEL_ITEM + selectedItem;
		BMD* pModel = gmClientModels ? gmClientModels->GetModel(modelType) : 0;
		const bool ok = (pModel != 0 && pModel->NumMeshs > 0 && pModel->Meshs != 0);

		if (ok)
		{
			m_bPreviewModelOk = true;
			ImGui::Text("Model: OK");
		}
		else
		{
			char guessPath[260] = { 0 };
			guessModelPath(m_ItemSection, m_ItemType, guessPath, sizeof(guessPath));

			if (guessPath[0] != '\0')
				sprintf_s(m_szPreviewModelError, sizeof(m_szPreviewModelError), "ModelId=%d  Guess=%s  Exists=%d",
					modelType,
					guessPath,
					fileExists(guessPath) ? 1 : 0);
			else
				sprintf_s(m_szPreviewModelError, sizeof(m_szPreviewModelError), "ModelId=%d  Guess=(unknown)",
					modelType);

			ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Model: MISSING");
			ImGui::TextWrapped("%s", m_szPreviewModelError);

			static std::set<int> s_loggedMissingModels;
			if (s_loggedMissingModels.insert(modelType).second)
			{
				static bool s_errorReportInit = false;
				if (!s_errorReportInit)
				{
					g_ErrorReport.Create("GMMenu_MissingModels.log");
					s_errorReportInit = true;
				}

				g_ErrorReport.Write("[GMMenu] Missing model: item=%d section=%d type=%d modelId=%d guess=%s exists=%d\r\n",
					selectedItem,
					m_ItemSection,
					m_ItemType,
					modelType,
					guessPath[0] ? guessPath : "(unknown)",
					guessPath[0] ? (fileExists(guessPath) ? 1 : 0) : -1);

				if (g_ConsoleDebug)
				{
					g_ConsoleDebug->Write(MCD_ERROR, "[GMMenu] Missing model: item=%d section=%d type=%d modelId=%d guess=%s exists=%d",
						selectedItem,
						m_ItemSection,
						m_ItemType,
						modelType,
						guessPath[0] ? guessPath : "(unknown)",
						guessPath[0] ? (fileExists(guessPath) ? 1 : 0) : -1);
				}
			}
		}
	}

	ImGui::Separator();

	ImGui::Text("Client Data (Item.bmd)");
	ImGui::Text("Kind: %d / %s / %d", item_info->Kind1, kind2Name(item_info->Kind2), item_info->Kind3);
	ImGui::Text("Size: %dx%d  Dur: %d", (int)item_info->Width, (int)item_info->Height, (int)item_info->Durability);
	ImGui::Text("DMG: %d-%d  DEF: %d  MDEF: %d", (int)item_info->DamageMin, (int)item_info->DamageMax, (int)item_info->Defense, (int)item_info->MagicDefense);
	ImGui::Text("Req Lvl: %d  Str: %d  Dex: %d  Ene: %d  Vit: %d  Cmd: %d",
		(int)item_info->RequireLevel,
		(int)item_info->RequireStrength,
		(int)item_info->RequireDexterity,
		(int)item_info->RequireEnergy,
		(int)item_info->RequireVitality,
		(int)item_info->RequireCharisma);

	ImGui::Separator();

	ImGui::Text("Create Item");
	ImGui::SliderInt("Count", &m_SpawnCount, 1, 100);
	ImGui::SliderInt("Level (0..15)", &m_SpawnLevel, 0, 15);
	{
		bool skill = (m_SpawnSkill != 0);
		if (ImGui::Checkbox("Skill (+Skill)", &skill)) m_SpawnSkill = skill ? 1 : 0;
		ImGui::SameLine();
		bool luck = (m_SpawnLuck != 0);
		if (ImGui::Checkbox("Luck (+Luck)", &luck)) m_SpawnLuck = luck ? 1 : 0;
	}
	ImGui::SliderInt("Option (+0..+7)", &m_SpawnOption, 0, 7);

	{
		const int kind2 = item_info ? (int)item_info->Kind2 : 0;
		const bool isWeapon = (kind2 >= 1 && kind2 <= 14) || kind2 == 78 || kind2 == 81 || kind2 == 84 || kind2 == 89 || kind2 == 90;
		const bool isArmorOrShield = (kind2 >= 15 && kind2 <= 20) || kind2 == 77;

		const char* excWeapon[6] = {
			"Exc: Mana per kill",
			"Exc: Life per kill",
			"Exc: Attack speed +7",
			"Exc: Damage +2%",
			"Exc: Damage +Level/20",
			"Exc: Excellent damage +10%",
		};

		const char* excArmor[6] = {
			"Exc: Max HP +4%",
			"Exc: Max MP +4%",
			"Exc: Damage decrease +4%",
			"Exc: Damage reflect +5%",
			"Exc: Defense success rate +10%",
			"Exc: Zen per kill +40%",
		};

		const char** excNames = isWeapon ? excWeapon : (isArmorOrShield ? excArmor : 0);

		auto setExcBit = [this](int mask, bool enabled)
			{
				m_SpawnExc = enabled ? (m_SpawnExc | mask) : (m_SpawnExc & ~mask);
			};

		ImGui::Text("Excellent");
		ImGui::PushID("exc");
		for (int i = 0; i < 6; ++i)
		{
			const int mask = (1 << i);
			bool enabled = (m_SpawnExc & mask) != 0;
			const char* label = excNames ? excNames[i] : "Exc";
			if (excNames == 0)
			{
				char fallback[16] = { 0 };
				sprintf_s(fallback, "Exc %d", i + 1);
				label = fallback;
			}

			if (ImGui::Checkbox(label, &enabled))
				setExcBit(mask, enabled);
		}
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Advanced"))
	{
		ImGui::SliderInt("Ancient/Set ID (0..255)", &m_SpawnSet, 0, 255);
		ImGui::SliderInt("Sockets (0..5)", &m_SpawnSocket, 0, 5);
	}

	ImGui::NewLine();

	if (ImGui::Button("To inventory")) this->SendItemSpawn(0);
	ImGui::SameLine();
	if (ImGui::Button("To ground")) this->SendItemSpawn(1);
	ImGui::SameLine();
	if (ImGui::Button("Create set")) this->SendItemSpawn(2);

	ImGui::SameLine();
	if (ImGui::Button("Clear inventory (keep equipped)")) this->SendClearInventoryKeepEquipped();
}

void SEASON3B::CGFxEffectHandle::RenderButtons()
{
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

	/*if (m_bEditEnchant && itemindex == selectedItem)
	{
		is_rendered = true;
	}
	else
	{*/
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
	//}
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

void SEASON3B::ComboBoxGird::Render(const char* nameId, const char* error)
{
	const char* selectedLabel1 = ((unsigned int)selectedId >= comboBoxItems.size()) ? error : comboBoxItems[selectedId].second.c_str();

	int selected = selectedIndex;
	if (ImGui::BeginCombo(nameId, selectedLabel1, 0))
	{
		for (size_t i = 0; i < comboBoxItems.size(); ++i)
		{
			bool isSelected = (selectedIndex == comboBoxItems[i].first);
			if (ImGui::Selectable(comboBoxItems[i].second.c_str(), isSelected))
			{
				selectedId = i;
				selectedIndex = comboBoxItems[i].first;
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus(); // Foco en la opci�n seleccionada
			}
		}
		ImGui::EndCombo();
	}
}

void SEASON3B::ComboBoxGird::push_back(int id, std::string name)
{
	comboBoxItems.push_back({ id, name });
}

void SEASON3B::GirdListBox::Render(const char* nameId)
{
	if (listItems.size() > 0)
	{
		std::vector<const char*> items_cstr;
		items_cstr.reserve(listItems.size());

		for (const std::string& item : listItems)
		{
			items_cstr.push_back(item.c_str());
		}

		if (ImGui::ListBox(nameId, &selectedIndex, items_cstr.data(), (int)items_cstr.size(), -1))
		{
		}
	}
}

void SEASON3B::GirdListBox::push_back(std::string name)
{
	listItems.push_back(name);
}
#endif // EFFECT_MNG_HANDLE
