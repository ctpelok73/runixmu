// Gate.cpp: implementation of the CGate class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Gate.h"
#include "DefaultClassInfo.h"
#include "Map.h"
#include "MemScript.h"
#include "Log.h"
#include "Util.h"

CGate gGate;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGate::CGate() // OK
{
	this->m_GateInfo.clear();
}

CGate::~CGate() // OK
{
	this->m_GateInfo.clear();
}

void CGate::Load(char* path) // OK
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

		this->m_GateInfo.clear();

		pugi::xml_node root = file.child("Gate");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			GATE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.Flag = leaf.attribute("Flag").as_int();
			info.Map = leaf.attribute("Map").as_int();
			info.X = leaf.attribute("X").as_int();
			info.Y = leaf.attribute("Y").as_int();
			info.TX = leaf.attribute("TX").as_int();
			info.TY = leaf.attribute("TY").as_int();
			info.TargetGate = leaf.attribute("TargetGate").as_int();
			info.Dir = leaf.attribute("Dir").as_int();
			info.MinLevel = leaf.attribute("MinLevel").as_int();
			info.MaxLevel = leaf.attribute("MaxLevel").as_int();
			info.MinReset = leaf.attribute("MinReset").as_int();
			info.MaxReset = leaf.attribute("MaxReset").as_int();
			info.AccountLevel = leaf.attribute("AccountLevel").as_int();

			this->m_GateInfo.insert(type_map_gate::value_type(info.Index, info));
		}

		LogAdd(LOG_BLUE, "[XML] Gate loaded successfully (%d records) [%s]", (int)this->m_GateInfo.size(), sourcePath);

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

	this->m_GateInfo.clear();

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

			GATE_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.Flag = lpMemScript->GetAsNumber();

			info.Map = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			info.TX = lpMemScript->GetAsNumber();

			info.TY = lpMemScript->GetAsNumber();

			info.TargetGate = lpMemScript->GetAsNumber();

			info.Dir = lpMemScript->GetAsNumber();

			info.MinLevel = lpMemScript->GetAsNumber();

			info.MaxLevel = lpMemScript->GetAsNumber();

			info.MinReset = lpMemScript->GetAsNumber();

			info.MaxReset = lpMemScript->GetAsNumber();

			info.AccountLevel = lpMemScript->GetAsNumber();

			this->m_GateInfo.insert(type_map_gate::value_type(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] Gate loaded successfully (%d records) [%s]", (int)this->m_GateInfo.size(), path);

	delete lpMemScript;
}

void CGate::ExportXML(std::string filename)
{
	pugi::xml_document doc;

	pugi::xml_node root = doc.append_child("Gate");

	for (type_map_gate::iterator it = m_GateInfo.begin(); it != m_GateInfo.end(); it++)
	{
		GATE_INFO* s = &it->second;

		pugi::xml_node leaf = root.append_child("Info");

		leaf.append_attribute("Index").set_value(s->Index);

		leaf.append_attribute("Flag").set_value(s->Flag);

		leaf.append_attribute("Map").set_value(s->Map);

		leaf.append_attribute("X").set_value(s->X);

		leaf.append_attribute("Y").set_value(s->Y);

		leaf.append_attribute("TX").set_value(s->TX);

		leaf.append_attribute("TY").set_value(s->TY);

		leaf.append_attribute("TargetGate").set_value(s->TargetGate);

		leaf.append_attribute("Dir").set_value(s->Dir);

		leaf.append_attribute("MinLevel").set_value(s->MinLevel);

		leaf.append_attribute("MaxLevel").set_value(s->MaxLevel);

		leaf.append_attribute("MinReset").set_value(s->MinReset);

		leaf.append_attribute("MaxReset").set_value(s->MaxReset);

		leaf.append_attribute("AccountLevel").set_value(s->AccountLevel);
	}

	doc.save_file(filename.c_str());
}

void CGate::ExportBMD(std::string filename)
{
	int Size = sizeof(GATE_ATTRIBUTE);
	std::vector<GATE_ATTRIBUTE> _ReqInfo(MAX_GATES);

	for (type_map_gate::iterator it = m_GateInfo.begin(); it != m_GateInfo.end(); it++)
	{
		GATE_INFO* s = &it->second;

		if (s->Index < 0 || s->Index >= MAX_GATES)
			continue;

		GATE_ATTRIBUTE info;

		info.Flag = s->Flag;

		info.iMap = s->Map;

		info.X = s->X;

		info.Y = s->Y;

		info.TX = s->TX;

		info.TY = s->TY;

		info.Target = s->TargetGate;

		info.Angle = s->Dir;

		info.MINLevel = s->MinLevel;

		info.MAXLevel = s->MaxLevel;

		_ReqInfo[s->Index] = (info);
	}

	PackFileEncrypt(filename.c_str(), (BYTE*)_ReqInfo.data(), MAX_GATES, Size, 0, false, false);
}

bool CGate::GetInfo(int index,GATE_INFO* lpInfo) // OK
{
	type_map_gate::iterator it = this->m_GateInfo.find(index);

	if(it == this->m_GateInfo.end())
	{
		return 0;
	}
	else
	{
		(*lpInfo) = it->second;
		return 1;
	}
}

int CGate::GetGateMap(int index) // OK
{
	GATE_INFO info;

	if(this->GetInfo(index,&info) == 0)
	{
		return -1;
	}

	return info.Map;
}

int CGate::GetMoveLevel(LPOBJ lpObj,int map,int level) // OK
{
	if(map != MAP_SWAMP_OF_CALMNESS && (lpObj->Class == CLASS_MG || lpObj->Class == CLASS_DL || lpObj->Class == CLASS_RF))
	{
		return ((level*2)/3);
	}

	return level;
}

bool CGate::IsGate(int index) // OK
{
	GATE_INFO info;

	if(this->GetInfo(index,&info) == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

bool CGate::IsInGate(LPOBJ lpObj,int index) // OK
{
	GATE_INFO info;

	if(this->GetInfo(index,&info) == 0)
	{
		return 0;
	}

	if(lpObj->Map != info.Map)
	{
		return 0;
	}

	if(lpObj->X < (info.TX-5) || lpObj->X > (info.TX+5) || lpObj->Y < (info.TY-5) || lpObj->Y > (info.TY+5))
	{
		return 0;
	}

	if(info.MinLevel != -1 && lpObj->GetLevel() < this->GetMoveLevel(lpObj,info.Map,info.MinLevel))
	{
		return 0;
	}

	if(info.MaxLevel != -1 && lpObj->GetLevel() > info.MaxLevel)
	{
		return 0;
	}

	if(info.MinReset != -1 && lpObj->Reset < info.MinReset)
	{
		return 0;
	}

	if(info.MaxReset != -1 && lpObj->Reset > info.MaxReset)
	{
		return 0;
	}

	if(lpObj->AccountLevel < info.AccountLevel)
	{
		return 0;
	}

	return 1;
}

bool CGate::GetGate(int index,int* gate,int* map,int* x,int* y,int* dir,int* level) // OK
{
	GATE_INFO info;

	if(this->GetInfo(index,&info) == 0)
	{
		return 0;
	}

	if(info.TargetGate != 0 && this->GetInfo(info.TargetGate,&info) == 0)
	{
		return 0;
	}

	int px,py;

	for(int n=0;n < 50;n++)
	{
		if((info.TX-info.X) > 0)
		{
			px = info.X+(GetLargeRand()%(info.TX-info.X));
		}
		else
		{
			px = info.X;
		}

		if((info.TY-info.Y) > 0)
		{
			py = info.Y+(GetLargeRand()%(info.TY-info.Y));
		}
		else
		{
			py = info.Y;
		}

		if(gMap[info.Map].CheckAttr(px,py,4) == 0 && gMap[info.Map].CheckAttr(px,py,8) == 0)
		{
			(*gate) = info.Index;
			(*map) = info.Map;
			(*x) = px;
			(*y) = py;
			(*dir) = info.Dir;
			(*level) = info.MinLevel;
			return 1;
		}
	}

	return 0;
}
