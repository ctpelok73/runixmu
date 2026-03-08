// MoveSummon.cpp: implementation of the CMoveSummon class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MoveSummon.h"
#include "CastleSiege.h"
#include "DefaultClassInfo.h"
#include "Gate.h"
#include "ItemManager.h"
#include "Map.h"
#include "MemScript.h"
#include "Log.h"
#include "Util.h"

CMoveSummon gMoveSummon;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMoveSummon::CMoveSummon() // OK
{
	this->m_MoveSummonInfo.clear();
}

CMoveSummon::~CMoveSummon() // OK
{

}

void CMoveSummon::Load(char* path) // OK
{
	char xmlPath[MAX_PATH] = { 0 };
	const char* sourcePath = path;
	bool loadXml = false;
	const char* ext = strrchr(path, '.');

	if (ext != 0 && _stricmp(ext, ".xml") == 0)
	{
		loadXml = true;
	}
	else if (ext != 0 && _stricmp(ext, ".txt") == 0)
	{
		strcpy_s(xmlPath, path);
		char* xmlExt = strrchr(xmlPath, '.');

		if (xmlExt != 0)
		{
			strcpy_s(xmlExt, 5, ".xml");
			FILE* file = 0;

			if (fopen_s(&file, xmlPath, "r") == 0 && file != 0)
			{
				fclose(file);
				sourcePath = xmlPath;
				loadXml = true;
			}
		}
	}

	if (loadXml != 0)
	{
		pugi::xml_document file;
		pugi::xml_parse_result res = file.load_file(sourcePath);

		if (res.status != pugi::status_ok)
		{
			ErrorMessageBox("Error load fail: %s", sourcePath);
			return;
		}

		this->m_MoveSummonInfo.clear();

		pugi::xml_node root = file.child("MoveSummon");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			MOVE_SUMMON_INFO info;

			info.Map = leaf.attribute("Map").as_int();
			info.X = leaf.attribute("X").as_int();
			info.Y = leaf.attribute("Y").as_int();
			info.TX = leaf.attribute("TX").as_int();
			info.TY = leaf.attribute("TY").as_int();
			info.MinLevel = leaf.attribute("MinLevel").as_int();
			info.MaxLevel = leaf.attribute("MaxLevel").as_int();
			info.MinReset = leaf.attribute("MinReset").as_int();
			info.MaxReset = leaf.attribute("MaxReset").as_int();
			info.AccountLevel = leaf.attribute("AccountLevel").as_int();
			info.PkMove = leaf.attribute("PkMove").as_int();

			this->m_MoveSummonInfo.push_back(info);
		}

		LogAdd(LOG_BLUE, "[XML] MoveSummon loaded successfully (%d records) [%s]", (int)this->m_MoveSummonInfo.size(), sourcePath);

		return;
	}

	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->m_MoveSummonInfo.clear();

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			MOVE_SUMMON_INFO info;

			info.Map = lpMemScript->GetNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			info.TX = lpMemScript->GetAsNumber();

			info.TY = lpMemScript->GetAsNumber();

			info.MinLevel = lpMemScript->GetAsNumber();

			info.MaxLevel = lpMemScript->GetAsNumber();

			info.MinReset = lpMemScript->GetAsNumber();

			info.MaxReset = lpMemScript->GetAsNumber();

			info.AccountLevel = lpMemScript->GetAsNumber();

			info.PkMove = lpMemScript->GetAsNumber();

			this->m_MoveSummonInfo.push_back(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] MoveSummon loaded successfully (%d records) [%s]", (int)this->m_MoveSummonInfo.size(), path);

	delete lpMemScript;
}

bool CMoveSummon::CheckMoveSummon(LPOBJ lpObj,int map,int x,int y) // OK
{
	if(lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
	{
		return 0;
	}

	#if(GAMESERVER_TYPE==1)

	if(map == MAP_CASTLE_SIEGE && gCastleSiege.GetCastleState() != CASTLESIEGE_STATE_STARTSIEGE && (x > 160 && x < 192 && y > 187 && y < 217))
	{
		return 0;
	}

	#endif

	if(map == MAP_ATLANS && (lpObj->Inventory[8].IsItem() != 0 && (lpObj->Inventory[8].m_Index == GET_ITEM(13,2) || lpObj->Inventory[8].m_Index == GET_ITEM(13,3)))) // Uniria,Dinorant
	{
		return 0;
	}

	if((map == MAP_ICARUS || map == MAP_KANTURU3) && (lpObj->Inventory[7].IsItem() == 0 && lpObj->Inventory[8].m_Index != GET_ITEM(13,3) && lpObj->Inventory[8].m_Index != GET_ITEM(13,37))) // Dinorant,Fenrir
	{
		return 0;
	}

	for(std::vector<MOVE_SUMMON_INFO>::iterator it=this->m_MoveSummonInfo.begin();it != this->m_MoveSummonInfo.end();it++)
	{
		if(it->Map != map)
		{
			continue;
		}

		if((it->X > x || it->TX < x) || (it->Y > y || it->TY < y))
		{
			continue;
		}

		if(it->MinLevel != -1 && lpObj->Level < gGate.GetMoveLevel(lpObj,it->Map,it->MinLevel))
		{
			return 0;
		}

		if(it->MaxLevel != -1 && lpObj->Level > it->MaxLevel)
		{
			return 0;
		}

		if(it->MinReset != -1 && lpObj->Reset < it->MinReset)
		{
			return 0;
		}

		if(it->MaxReset != -1 && lpObj->Reset > it->MaxReset)
		{
			return 0;
		}

		if(lpObj->AccountLevel < it->AccountLevel)
		{
			return 0;
		}		
		
		if(it->PkMove == 0 && lpObj->PKLevel >= 5)
		{
			return 0;
		}

		else
		{
			return 1;
		}
	}

	return 0;
}
