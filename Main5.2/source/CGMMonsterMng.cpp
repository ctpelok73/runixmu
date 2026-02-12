#include "stdafx.h"
#include "CGMProtect.h"
#include "ZzzCharacter.h"
#include "supportingfeature.h"
#include "CGMMonsterMng.h"

CGMMonsterMng::CGMMonsterMng()
{
}

CGMMonsterMng::~CGMMonsterMng()
{
	UniqueMonsterData.clear();
}

CGMMonsterMng* CGMMonsterMng::Instance()
{
	static CGMMonsterMng sInstance;
	return &sInstance;
}

void CGMMonsterMng::LoadDataMonster(CUSTOM_MONSTER_INFO* info, int Size)
{
	for (int i = 0; i < Size; i++)
	{
		CUSTOM_MONSTER_INFO m = info[i];
		m.Name[sizeof(m.Name) - 1] = '\0';
		m.directory[sizeof(m.directory) - 1] = '\0';
		m.filename[sizeof(m.filename) - 1] = '\0';

		if (m.monsterIndex == -1)
			continue;

		if (m.fSize <= 0.0f)
			m.fSize = 1.0f;

		if (m.Kind != KIND_MONSTER && m.Kind != KIND_NPC)
			m.Kind = KIND_MONSTER;

		if (m.RenderIndex < 0 || m.RenderIndex >= MAX_MODELS)
			m.RenderIndex = -1;

		bool replaced = false;
		for (size_t j = 0; j < UniqueMonsterData.size(); ++j)
		{
			if (UniqueMonsterData[j].monsterIndex == m.monsterIndex)
			{
				UniqueMonsterData[j] = m;
				replaced = true;
				break;
			}
		}

		if (!replaced)
			UniqueMonsterData.push_back(m);
	}
}

void CGMMonsterMng::LoadModelMonster()
{
	for (size_t i = 0; i < UniqueMonsterData.size(); i++)
	{
		UniqueMonsterData[i].OpenLoad();
	}
}

const type_monster& CGMMonsterMng::GetAll() const
{
	return UniqueMonsterData;
}

bool CGMMonsterMng::IsMonsterByIndex(int monsterIndex)
{
	for (size_t i = 0; i < UniqueMonsterData.size(); i++)
	{
		if (UniqueMonsterData[i].monsterIndex == monsterIndex && UniqueMonsterData[i].Kind == KIND_MONSTER)
		{
			return true;
		}
	}
	return false;
}

CUSTOM_MONSTER_INFO* CGMMonsterMng::FindMonsterByIndex(int monsterIndex)
{
	for (size_t i = 0; i < UniqueMonsterData.size(); i++)
	{
		if (UniqueMonsterData[i].monsterIndex == monsterIndex)
		{
			return &UniqueMonsterData[i];
		}
	}
	return NULL;
}

extern int HeroIndex;


CHARACTER* CGMMonsterMng::CreateMonster(int Type, int PositionX, int PositionY, int Key)
{
	CHARACTER* pCharacter = NULL;

	CUSTOM_MONSTER_INFO*  DataMonster = FindMonsterByIndex(Type);

	if (DataMonster != NULL)
	{
		pCharacter = CreateCharacter(Key, DataMonster->RenderIndex, PositionX, PositionY, 0.f);

		if (pCharacter != NULL)
		{
			OBJECT* o = &pCharacter->Object;
			o->Live = true;
			o->ExtState = 0;
			o->Kind = DataMonster->Kind;
			o->Scale = DataMonster->fSize;

			pCharacter->MonsterIndex = Type;
			pCharacter->TargetCharacter = HeroIndex;

			if (DataMonster->Name[0] != '\0')
			{
				strncpy(pCharacter->ID, DataMonster->Name, sizeof(DataMonster->Name));
			}

			if (o->Kind == KIND_NPC)
			{
				GMProtect->GetNpcName(Type, pCharacter->ID, PositionX, PositionY);
			}
		}
	}

	return pCharacter;
}
