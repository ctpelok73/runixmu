// CustomMonster.cpp: implementation of the CCustomMonster class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CustomMonster.h"
#include "DSProtocol.h"
#include "MemScript.h"
#include "Message.h"
#include "Monster.h"
#include "Notice.h"
#include "Log.h"
#include "Util.h"

CCustomMonster gCustomMonster;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomMonster::CCustomMonster() // OK
{
	this->m_CustomMonsterInfo.clear();
}

CCustomMonster::~CCustomMonster() // OK
{

}

void CCustomMonster::Load(char* path) // OK
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

		this->m_CustomMonsterInfo.clear();

		pugi::xml_node root = file.child("CustomMonster");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			CUSTOM_MONSTER_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.MapNumber = leaf.attribute("MapNumber").as_int();
			info.MaxLife = leaf.attribute("MaxLife").as_int();
			info.DamageMin = leaf.attribute("DamageMin").as_int();
			info.DamageMax = leaf.attribute("DamageMax").as_int();
			info.Defense = leaf.attribute("Defense").as_int();
			info.AttackRate = leaf.attribute("AttackRate").as_int();
			info.DefenseRate = leaf.attribute("DefenseRate").as_int();
			info.ExperienceRate = leaf.attribute("ExperienceRate").as_int();
			info.KillMessage = leaf.attribute("KillMessage").as_int();
			info.InfoMessage = leaf.attribute("InfoMessage").as_int();
			info.RewardValue1 = leaf.attribute("RewardValue1").as_int();
			info.RewardValue2 = leaf.attribute("RewardValue2").as_int();
			info.SummonMonster = leaf.attribute("SummonMonster").as_int();
			info.SummonMonsterCount = leaf.attribute("SummonMonsterCount").as_int();
			info.SummonMonsterRate = leaf.attribute("SummonMonsterRate").as_int();

			this->m_CustomMonsterInfo.push_back(info);
		}

		LogAdd(LOG_BLUE, "[XML] CustomMonster loaded successfully (%d records) [%s]", (int)this->m_CustomMonsterInfo.size(), sourcePath);

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

	this->m_CustomMonsterInfo.clear();

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

			CUSTOM_MONSTER_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.MapNumber = lpMemScript->GetAsNumber();

			info.MaxLife = lpMemScript->GetAsNumber();

			info.DamageMin = lpMemScript->GetAsNumber();

			info.DamageMax = lpMemScript->GetAsNumber();

			info.Defense = lpMemScript->GetAsNumber();

			info.AttackRate = lpMemScript->GetAsNumber();

			info.DefenseRate = lpMemScript->GetAsNumber();

			info.ExperienceRate = lpMemScript->GetAsNumber();

			info.KillMessage = lpMemScript->GetAsNumber();

			info.InfoMessage = lpMemScript->GetAsNumber();

			info.RewardValue1 = lpMemScript->GetAsNumber();

			info.RewardValue2 = lpMemScript->GetAsNumber();

			info.SummonMonster = lpMemScript->GetAsNumber();

			info.SummonMonsterCount = lpMemScript->GetAsNumber();

			info.SummonMonsterRate = lpMemScript->GetAsNumber();

			this->m_CustomMonsterInfo.push_back(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomMonster loaded successfully (%d records) [%s]", (int)this->m_CustomMonsterInfo.size(), path);

	delete lpMemScript;
}

void CCustomMonster::SetCustomMonsterInfo(LPOBJ lpObj) // OK
{
	CUSTOM_MONSTER_INFO CustomMonsterInfo;

	if(this->GetCustomMonsterInfo(lpObj->Class,lpObj->Map,&CustomMonsterInfo) == 0)
	{
		return;
	}

	lpObj->Life = (float)((__int64)lpObj->Life*((CustomMonsterInfo.MaxLife==-1)?100:CustomMonsterInfo.MaxLife))/100;

	lpObj->MaxLife = (float)((__int64)lpObj->MaxLife*((CustomMonsterInfo.MaxLife==-1)?100:CustomMonsterInfo.MaxLife))/100;

	lpObj->ScriptMaxLife = (float)((__int64)lpObj->ScriptMaxLife*((CustomMonsterInfo.MaxLife==-1)?100:CustomMonsterInfo.MaxLife))/100;

	lpObj->PhysiDamageMin = ((__int64)lpObj->PhysiDamageMin*((CustomMonsterInfo.DamageMin==-1)?100:CustomMonsterInfo.DamageMin))/100;

	lpObj->PhysiDamageMax = ((__int64)lpObj->PhysiDamageMax*((CustomMonsterInfo.DamageMax==-1)?100:CustomMonsterInfo.DamageMax))/100;

	lpObj->Defense = ((__int64)lpObj->Defense*((CustomMonsterInfo.Defense==-1)?100:CustomMonsterInfo.Defense))/100;

	lpObj->AttackSuccessRate = ((__int64)lpObj->AttackSuccessRate*((CustomMonsterInfo.AttackRate==-1)?100:CustomMonsterInfo.AttackRate))/100;

	lpObj->DefenseSuccessRate = ((__int64)lpObj->DefenseSuccessRate*((CustomMonsterInfo.DefenseRate==-1)?100:CustomMonsterInfo.DefenseRate))/100;

	#if(GAMESERVER_UPDATE>=701)

	lpObj->ElementalDefense = ((__int64)lpObj->ElementalDefense*((CustomMonsterInfo.Defense==-1)?100:CustomMonsterInfo.Defense))/100;

	lpObj->ElementalDamageMin = ((__int64)lpObj->ElementalDamageMin*((CustomMonsterInfo.DamageMin==-1)?100:CustomMonsterInfo.DamageMin))/100;

	lpObj->ElementalDamageMax = ((__int64)lpObj->ElementalDamageMax*((CustomMonsterInfo.DamageMax==-1)?100:CustomMonsterInfo.DamageMax))/100;

	lpObj->ElementalAttackSuccessRate = ((__int64)lpObj->ElementalAttackSuccessRate*((CustomMonsterInfo.AttackRate==-1)?100:CustomMonsterInfo.AttackRate))/100;

	lpObj->ElementalDefenseSuccessRate = ((__int64)lpObj->ElementalDefenseSuccessRate*((CustomMonsterInfo.DefenseRate==-1)?100:CustomMonsterInfo.DefenseRate))/100;

	#endif
}

void CCustomMonster::MonsterDieProc(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	CUSTOM_MONSTER_INFO CustomMonsterInfo;

	if(this->GetCustomMonsterInfo(lpObj->Class,lpObj->Map,&CustomMonsterInfo) == 0)
	{
		return;
	}

	int aIndex = gObjMonsterGetTopHitDamageUser(lpObj);

	if(OBJECT_RANGE(aIndex) != 0)
	{
		lpTarget = &gObj[aIndex];
	}

	if(CustomMonsterInfo.KillMessage != -1)
	{
		gNotice.GCNoticeSendToAll(0,0,0,0,0,0,gMessage.GlobalText(CustomMonsterInfo.KillMessage),lpTarget->Name);
	}

	if(CustomMonsterInfo.InfoMessage != -1)
	{
		gNotice.GCNoticeSend(lpTarget->Index,1,0,0,0,0,0,gMessage.GlobalText(CustomMonsterInfo.InfoMessage),CustomMonsterInfo.RewardValue1,CustomMonsterInfo.RewardValue2);
	}

	if(CustomMonsterInfo.RewardValue1 != -1 && CustomMonsterInfo.RewardValue2 != -1)
	{
		GDCustomMonsterRewardSaveSend(lpTarget->Index,lpObj->Class,lpObj->Map,CustomMonsterInfo.RewardValue1,CustomMonsterInfo.RewardValue2);
	}

	if(CustomMonsterInfo.SummonMonster > 0 && (GetLargeRand()%100) < CustomMonsterInfo.SummonMonsterRate)
	{
		int qtd = (CustomMonsterInfo.SummonMonsterCount > 0) ? CustomMonsterInfo.SummonMonsterCount : 1;

		for(int n=0;n < qtd;n++)
		{
			int index = gObjAddMonster(lpObj->Map);

			if(OBJECT_RANGE(index) == 0)
			{
				return;
			}

			LPOBJ lpMonster = &gObj[index];

			int px = lpObj->X;
			int py = lpObj->Y;

			if(gObjGetRandomFreeLocation(lpObj->Map,&px,&py,3,3,50) == 0)
			{
				return;
			}

			lpMonster->PosNum = -1;
			lpMonster->X = px;
			lpMonster->Y = py;
			lpMonster->TX = px;
			lpMonster->TY = py;
			lpMonster->OldX = px;
			lpMonster->OldY = py;
			lpMonster->StartX = px;
			lpMonster->StartY = py;
			lpMonster->Dir = 1;
			lpMonster->Map = lpObj->Map;
			lpMonster->MonsterDeleteTime = GetTickCount()+1800000;

			if(gObjSetMonster(index,CustomMonsterInfo.SummonMonster) == 0)
			{
				gObjDel(index);
			}
		}
	}
}

long CCustomMonster::GetCustomMonsterExperienceRate(int index,int map) // OK
{
	CUSTOM_MONSTER_INFO CustomMonsterInfo;

	if(this->GetCustomMonsterInfo(index,map,&CustomMonsterInfo) == 0)
	{
		return 100;
	}
	else
	{
		return ((CustomMonsterInfo.ExperienceRate==-1)?100:CustomMonsterInfo.ExperienceRate);
	}
}

long CCustomMonster::GetCustomMonsterMasterExperienceRate(int index,int map) // OK
{
	CUSTOM_MONSTER_INFO CustomMonsterInfo;

	if(this->GetCustomMonsterInfo(index,map,&CustomMonsterInfo) == 0)
	{
		return 100;
	}
	else
	{
		return ((CustomMonsterInfo.ExperienceRate==-1)?100:CustomMonsterInfo.ExperienceRate);
	}
}

bool CCustomMonster::GetCustomMonsterInfo(int index,int map,CUSTOM_MONSTER_INFO* lpInfo) // OK
{
	for(std::vector<CUSTOM_MONSTER_INFO>::iterator it=this->m_CustomMonsterInfo.begin();it != this->m_CustomMonsterInfo.end();it++)
	{
		if(it->Index == index && (it->MapNumber == -1 || it->MapNumber == map))
		{
			(*lpInfo) = (*it);
			return 1;
		}
	}

	return 0;
}
