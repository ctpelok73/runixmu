// ItemDrop.cpp: implementation of the CItemDrop class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ItemDrop.h"
#include "BonusManager.h"
#include "CrywolfSync.h"
#include "DSProtocol.h"
#include "ItemManager.h"
#include "ItemOptionRate.h"
#include "MemScript.h"
#include "Monster.h"
#include "RandomManager.h"
#include "Log.h"
#include "Util.h"

CItemDrop gItemDrop;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CItemDrop::CItemDrop() // OK
{
	this->m_ItemDropInfo.clear();
}

CItemDrop::~CItemDrop() // OK
{

}

void CItemDrop::Load(char* path) // OK
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

		this->m_ItemDropInfo.clear();

		pugi::xml_node root = file.child("ItemDrop");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			ITEM_DROP_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.Level = leaf.attribute("Level").as_int();
			info.Grade = leaf.attribute("Grade").as_int();
			info.Option0 = leaf.attribute("Option0").as_int();
			info.Option1 = leaf.attribute("Option1").as_int();
			info.Option2 = leaf.attribute("Option2").as_int();
			info.Option3 = leaf.attribute("Option3").as_int();
			info.Option4 = leaf.attribute("Option4").as_int();
			info.Option5 = leaf.attribute("Option5").as_int();
			info.Option6 = leaf.attribute("Option6").as_int();
			info.Duration = leaf.attribute("Duration").as_int();
			info.MapNumber = leaf.attribute("MapNumber").as_int();
			info.MonsterClass = leaf.attribute("MonsterClass").as_int();
			info.MonsterLevelMin = leaf.attribute("MonsterLevelMin").as_int();
			info.MonsterLevelMax = leaf.attribute("MonsterLevelMax").as_int();
			info.DropRate = leaf.attribute("DropRate").as_int();

			this->m_ItemDropInfo.push_back(info);
		}

		LogAdd(LOG_BLUE, "[XML] ItemDrop loaded successfully (%d records) [%s]", (int)this->m_ItemDropInfo.size(), sourcePath);

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

	this->m_ItemDropInfo.clear();

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

			ITEM_DROP_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.Level = lpMemScript->GetAsNumber();

			info.Grade = lpMemScript->GetAsNumber();

			info.Option0 = lpMemScript->GetAsNumber();

			info.Option1 = lpMemScript->GetAsNumber();

			info.Option2 = lpMemScript->GetAsNumber();

			info.Option3 = lpMemScript->GetAsNumber();

			info.Option4 = lpMemScript->GetAsNumber();

			info.Option5 = lpMemScript->GetAsNumber();

			info.Option6 = lpMemScript->GetAsNumber();

			info.Duration = lpMemScript->GetAsNumber();

			info.MapNumber = lpMemScript->GetAsNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.MonsterLevelMin = lpMemScript->GetAsNumber();

			info.MonsterLevelMax = lpMemScript->GetAsNumber();

			info.DropRate = lpMemScript->GetAsNumber();

			this->m_ItemDropInfo.push_back(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] ItemDrop loaded successfully (%d records) [%s]", (int)this->m_ItemDropInfo.size(), path);

	delete lpMemScript;
}

void CItemDrop::ExportXML(std::string filename)
{
	pugi::xml_node leaf;

	pugi::xml_document doc;

	pugi::xml_node root = doc.append_child("ItemDrop");

	for (int i = 0; i < m_ItemDropInfo.size(); i++)
	{
		ITEM_DROP_INFO* info = &m_ItemDropInfo[i];

		if (info)
		{
			leaf = root.append_child("Info");

			leaf.append_attribute("Index").set_value(info->Index);

			leaf.append_attribute("Level").set_value(info->Level);

			leaf.append_attribute("Grade").set_value(info->Grade);

			leaf.append_attribute("Option0").set_value(info->Option0);

			leaf.append_attribute("Option1").set_value(info->Option1);

			leaf.append_attribute("Option2").set_value(info->Option2);

			leaf.append_attribute("Option3").set_value(info->Option3);

			leaf.append_attribute("Option4").set_value(info->Option4);

			leaf.append_attribute("Option5").set_value(info->Option5);

			leaf.append_attribute("Option6").set_value(info->Option6);

			leaf.append_attribute("Duration").set_value(info->Duration);

			leaf.append_attribute("MapNumber").set_value(info->MapNumber);

			leaf.append_attribute("MonsterClass").set_value(info->MonsterClass);

			leaf.append_attribute("MonsterLevelMin").set_value(info->MonsterLevelMin);

			leaf.append_attribute("MonsterLevelMax").set_value(info->MonsterLevelMax);

			leaf.append_attribute("DropRate").set_value(info->DropRate);

			if (info->Index != -1)
			{
				leaf.append_attribute("Comment").set_value(gItemManager.GetItemName(info->Index));
			}
			else
			{
				leaf.append_attribute("Comment").set_value(" ");
			}
		}
	}

	doc.save_file(filename.c_str());
}

int CItemDrop::DropItem(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	CRandomManager RandomManager;

	for(std::vector<ITEM_DROP_INFO>::iterator it=this->m_ItemDropInfo.begin();it != this->m_ItemDropInfo.end();it++)
	{
		int DropRate;

		ITEM_INFO ItemInfo;

		if(gItemManager.GetInfo(it->Index,&ItemInfo) == 0)
		{
			continue;
		}

		if(it->MapNumber != -1 && it->MapNumber != lpObj->Map)
		{
			continue;
		}

		if(it->MonsterClass != -1 && it->MonsterClass != lpObj->Class)
		{
			continue;
		}

		if(it->MonsterLevelMin != -1 && it->MonsterLevelMin > lpObj->Level)
		{
			continue;
		}

		if(it->MonsterLevelMax != -1 && it->MonsterLevelMax < lpObj->Level)
		{
			continue;
		}

		if((DropRate=it->DropRate) == -1 || (GetLargeRand()%1000000) < (DropRate=this->GetItemDropRate(lpObj,lpTarget,it->Index,it->Level,it->DropRate)))
		{
			int rate = (1000000/((DropRate==-1)?1000000:DropRate));

			RandomManager.AddElement((int)(&(*it)),rate);
		}
	}

	ITEM_DROP_INFO* lpItemDropInfo;

	if(RandomManager.GetRandomElement((int*)&lpItemDropInfo) == 0)
	{
		return 0;
	}
	else
	{
		WORD ItemIndex = lpItemDropInfo->Index;
		BYTE ItemLevel = lpItemDropInfo->Level;
		BYTE ItemOption1 = 0;
		BYTE ItemOption2 = 0;
		BYTE ItemOption3 = 0;
		BYTE ItemNewOption = 0;
		BYTE ItemSetOption = 0;
		BYTE ItemSocketOption[MAX_SOCKET_OPTION] = {0xFF,0xFF,0xFF,0xFF,0xFF};

		gItemOptionRate.GetItemOption0(lpItemDropInfo->Option0,&ItemLevel);

		gItemOptionRate.GetItemOption1(lpItemDropInfo->Option1,&ItemOption1);

		gItemOptionRate.GetItemOption2(lpItemDropInfo->Option2,&ItemOption2);

		gItemOptionRate.GetItemOption3(lpItemDropInfo->Option3,&ItemOption3);

		gItemOptionRate.GetItemOption4(lpItemDropInfo->Option4,&ItemNewOption);

		gItemOptionRate.GetItemOption5(lpItemDropInfo->Option5,&ItemSetOption);

		gItemOptionRate.GetItemOption6(lpItemDropInfo->Option6,&ItemSocketOption[0]);

		gItemOptionRate.MakeNewOption(ItemIndex,ItemNewOption,&ItemNewOption);

		gItemOptionRate.MakeSetOption(ItemIndex,ItemSetOption,&ItemSetOption);

		gItemOptionRate.MakeSocketOption(ItemIndex,ItemSocketOption[0],&ItemSocketOption[0]);

		GDCreateItemSend(lpTarget->Index,lpObj->Map,(BYTE)lpObj->X,(BYTE)lpObj->Y,ItemIndex,ItemLevel,0,ItemOption1,ItemOption2,ItemOption3,lpTarget->Index,((ItemNewOption==0)?lpItemDropInfo->Grade:ItemNewOption),ItemSetOption,0,0,ItemSocketOption,0xFF,((lpItemDropInfo->Duration>0)?((DWORD)time(0)+lpItemDropInfo->Duration):0));

		return 1;
	}
}

int CItemDrop::GetItemDropRate(LPOBJ lpObj,LPOBJ lpTarget,int ItemIndex,int ItemLevel,int DropRate) // OK
{
	if(ItemIndex == GET_ITEM(12,25) || ItemIndex == GET_ITEM(14,13) || ItemIndex == GET_ITEM(14,14) || ItemIndex == GET_ITEM(14,16) || ItemIndex == GET_ITEM(14,22) || ItemIndex == GET_ITEM(14,31))
	{
		if(gCrywolfSync.CheckApplyPenalty() != 0 && gCrywolfSync.GetOccupationState() == 1)
		{
			if((GetLargeRand()%100) >= gCrywolfSync.GetGemDropPenaltiyRate())
			{
				return 0;
			}
		}
	}

	return gBonusManager.GetBonusValue(lpTarget,BONUS_INDEX_CMN_ITEM_DROP_RATE,DropRate,ItemIndex,ItemLevel,lpObj->Class,lpObj->Level);
}
