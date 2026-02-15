#include "stdafx.h"
#include "NewUISystem.h"
#include "CGMEffectHandle.h"
#include "Protocol.h"
#include "CSItemOption.h"
#include "CSParts.h"
#include "ZzzInventory.h"
#include "ZzzCharacter.h"
#include "ZzzOpenData.h"
#include "ZzzOpenglUtil.h"
#include "ZzzInterface.h"
#include "SocketSystem.h"
#include "CGMMonsterMng.h"
#include "supportingfeature.h"
#include "./Utilities/Log/ErrorReport.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdarg>

#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

#ifdef EFFECT_MNG_HANDLE

static int gmGetClassNameKey(int baseClass, int requireClass)
{
	int NameClass = 2305;

	switch (baseClass)
	{
	case Dark_Wizard:
		if (requireClass == 1) NameClass = 20;
		else if (requireClass == 2) NameClass = 25;
		else if (requireClass == 3) NameClass = 1669;
		else if (requireClass == 4) NameClass = 3186;
		break;
	case Dark_Knight:
		if (requireClass == 1) NameClass = 21;
		else if (requireClass == 2) NameClass = 26;
		else if (requireClass == 3) NameClass = 1668;
		else if (requireClass == 4) NameClass = 3187;
		break;
	case Fairy_Elf:
		if (requireClass == 1) NameClass = 22;
		else if (requireClass == 2) NameClass = 27;
		else if (requireClass == 3) NameClass = 1670;
		else if (requireClass == 4) NameClass = 3188;
		break;
	case Magic_Gladiator:
		if (requireClass == 1) NameClass = 23;
		else if (requireClass == 2) NameClass = 2305;
		else if (requireClass == 3) NameClass = 1671;
		else if (requireClass == 4) NameClass = 3189;
		break;
	case Dark_Lord:
		if (requireClass == 1) NameClass = 24;
		else if (requireClass == 2) NameClass = 2305;
		else if (requireClass == 3) NameClass = 1672;
		else if (requireClass == 4) NameClass = 3190;
		break;
	case Summoner:
		if (requireClass == 1) NameClass = 1687;
		else if (requireClass == 2) NameClass = 1688;
		else if (requireClass == 3) NameClass = 1689;
		else if (requireClass == 4) NameClass = 3191;
		break;
	case Rage_Fighter:
		if (requireClass == 1) NameClass = 3150;
		else if (requireClass == 2) NameClass = 2305;
		else if (requireClass == 3) NameClass = 3151;
		else if (requireClass == 4) NameClass = 3192;
		break;
	case Grow_Lancer:
		if (requireClass == 1) NameClass = 3175;
		else if (requireClass == 2) NameClass = 2305;
		else if (requireClass == 3) NameClass = 3176;
		else if (requireClass == 4) NameClass = 3177;
		break;
	case Runer_Wizzard:
		if (requireClass == 1) NameClass = 3179;
		else if (requireClass == 2) NameClass = 3181;
		else if (requireClass == 3) NameClass = 3182;
		else if (requireClass == 4) NameClass = 3183;
		break;
	case Slayer:
		if (requireClass == 1) NameClass = 3193;
		else if (requireClass == 2) NameClass = 3194;
		else if (requireClass == 3) NameClass = 3195;
		else if (requireClass == 4) NameClass = 3196;
		break;
	case Gun_Crusher:
		if (requireClass == 1) NameClass = 3200;
		else if (requireClass == 2) NameClass = 3201;
		else if (requireClass == 3) NameClass = 3202;
		else if (requireClass == 4) NameClass = 3203;
		break;
	case Mage_Kundun:
		if (requireClass == 1) NameClass = 3218;
		else if (requireClass == 2) NameClass = 3219;
		else if (requireClass == 3) NameClass = 3220;
		else if (requireClass == 4) NameClass = 3221;
		break;
	case Mage_Lemuria:
		if (requireClass == 1) NameClass = 3213;
		else if (requireClass == 2) NameClass = 3214;
		else if (requireClass == 3) NameClass = 3215;
		else if (requireClass == 4) NameClass = 3216;
		break;
	case Illusion_Knight:
		if (requireClass == 1) NameClass = 3209;
		else if (requireClass == 2) NameClass = 3210;
		else if (requireClass == 3) NameClass = 3211;
		else if (requireClass == 4) NameClass = 3212;
		break;
	default:
		NameClass = 2305;
		break;
	}

	if (NameClass == 2305 && requireClass != 1)
	{
		return gmGetClassNameKey(baseClass, 1);
	}

	return NameClass;
}

static std::string gmBuildItemClassInfoText(const Script_Item* item_info)
{
	if (!item_info)
		return std::string();

	bool any = false;
	bool all = true;

	for (int i = 0; i < MAX_CLASS_PLAYER; ++i)
	{
		if (item_info->RequireClass[i] != 0)
			any = true;
		else
			all = false;
	}

	if (!any)
		return std::string();
	if (all)
		return std::string("All");

	std::string out;
	for (int i = 0; i < MAX_CLASS_PLAYER; ++i)
	{
		const int req = (int)item_info->RequireClass[i];
		if (req == 0)
			continue;

		const int nameKey = gmGetClassNameKey(i, req);
		const char* name = GlobalText[nameKey];
		char fallback[64] = { 0 };
		if (!name || !name[0])
		{
			sprintf_s(fallback, "Class%d(req=%d)", i, req);
			name = fallback;
		}

		if (!out.empty())
			out += ", ";
		out += name;
	}

	return out;
}

extern bool g_GMMenuPreviewActive;
extern bool g_GMMenuPreviewAutoRotate;
extern float g_GMMenuPreviewRotateY;

static int s_gmMenuActiveTab = 0;
static int s_gmSelectedMonsterIndex = -1;
static int s_gmSelectedMonsterRow = -1;
static bool s_gmMonsterDbRequested = false;
static bool s_gmMonsterPreviewOk = false;
static int s_gmMonsterPreviewRenderIndex = -1;
static float s_gmMonsterPreviewScale = 1.0f;
static float s_gmMonsterPreviewFill = 0.80f;
static int s_gmMonsterPreviewKind = KIND_MONSTER;
static int s_gmMonsterPreviewAction = -1;
static float s_gmMonsterLastSize = 0.0f;
static float s_gmMonsterLastDist = 0.0f;
static float s_gmMonsterLastDistNeeded = 0.0f;
static float s_gmMonsterLastFillUsed = 0.0f;
static int s_gmMonsterLastModelType = -1;

static bool s_gmMonsterScanActive = false;
static int s_gmMonsterScanIndex = 0;
static int s_gmMonsterScanTotal = 0;
static int s_gmMonsterScanBatch = 128;
static int s_gmMonsterIssuesMissingModel = 0;
static int s_gmMonsterIssuesCreateFail = 0;
static int s_gmMonsterIssuesModelNotReady = 0;
static char s_gmMonsterLastIssue[256] = { 0 };
static bool s_gmMonsterReportInit = false;

static bool s_gmAuditAutoLog = true;
static bool s_gmAuditLogMissingModels = true;
static bool s_gmAuditLogMissingTooltips = true;
static bool s_gmAuditLogMissingName = false;
static bool s_gmAuditLogMissingClientData = true;
static bool s_gmAuditScanActive = false;
static int s_gmAuditScanIndex = 0;
static int s_gmAuditScanTotal = 0;
static int s_gmAuditScanBatch = 64;
static int s_gmAuditIssuesMissingModel = 0;
static int s_gmAuditIssuesMissingTooltip = 0;
static int s_gmAuditIssuesMissingName = 0;
static int s_gmAuditIssuesMissingClientData = 0;
static int s_gmAuditLastIssueItem = -1;
static char s_gmAuditLastIssue[256] = { 0 };
static bool s_gmAuditReportInit = false;
static std::set<unsigned long long> s_gmAuditLoggedIssues;

static void gmSendChatRaw(const char* text)
{
	if (text == 0 || text[0] == '\0')
		return;
	if (Hero == 0)
		return;

	PCHATING pMsg;
	pMsg.Header.set(0x00, sizeof(pMsg));
	memcpy(pMsg.ID, Hero->ID, MAX_ID_SIZE);
	memset(pMsg.ChatText, 0, sizeof(pMsg.ChatText));
	strncpy_s((char*)pMsg.ChatText, MAX_CHAT_SIZE, text, _TRUNCATE);
	DataSend((BYTE*)&pMsg, pMsg.Header.Size);
}

static bool gmIsModelReady(int modelType)
{
	if (gmClientModels == 0)
		return false;

	BMD* pModel = gmClientModels->GetModel(modelType);
	if (pModel == 0)
		return false;

	if (!pModel->m_bCompletedAlloc)
		return false;

	if (pModel->NumMeshs <= 0 || pModel->Meshs == 0)
		return false;

	if (pModel->NumBones <= 0 || pModel->Bones == 0)
		return false;

	if (pModel->NumActions <= 0 || pModel->Actions == 0)
		return false;

	bool hasKeys = false;
	for (int i = 0; i < pModel->NumActions; ++i)
	{
		if (pModel->Actions[i].NumAnimationKeys > 0)
		{
			hasKeys = true;
			break;
		}
	}
	if (!hasKeys)
		return false;

	return true;
}

static int gmFindSafeActionIndex(int modelType)
{
	if (gmClientModels == 0)
		return 0;

	BMD* pModel = gmClientModels->GetModel(modelType);
	if (pModel == 0 || pModel->Actions == 0 || pModel->NumActions <= 0)
		return 0;

	for (int i = 0; i < pModel->NumActions; ++i)
	{
		if (pModel->Actions[i].NumAnimationKeys > 0)
			return i;
	}

	return 0;
}

static bool gmCalcObjectSafe(OBJECT* o, bool translate, int select, int extraMon)
{
	__try
	{
		return Calc_RenderObject(o, translate, select, extraMon) ? true : false;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static int gmInferMonsterKind(int Type);

static bool gmFileExistsA(const char* path)
{
	if (path == 0 || path[0] == '\0')
		return false;
	const DWORD attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return false;
	return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool gmMonsterBmdExists(int monsterIndex)
{
	if (monsterIndex < 0 || monsterIndex >= MAX_MODEL_MONSTER)
		return false;

	char path[MAX_PATH] = { 0 };
	sprintf_s(path, "Data\\Monster\\Monster%02d.bmd", monsterIndex + 1);
	return gmFileExistsA(path);
}

static void gmRenderMonsterPreview3D(int monsterIndex, float scaleMul, float rotY, float previewX, float previewY, float previewW, float previewH, int actionOverride)
{
	const float centerX = previewX + (previewW * 0.5f);
	const float centerY = previewY + (previewH * 0.5f);

	const int clipX = (int)(previewX * g_fScreenRate_x);
	const int clipYTop = (int)(previewY * g_fScreenRate_y);
	const int clipW = (int)(previewW * g_fScreenRate_x);
	const int clipH = (int)(previewH * g_fScreenRate_y);

	if (clipW <= 0 || clipH <= 0)
		return;

	s_gmMonsterLastSize = 0.0f;
	s_gmMonsterLastDist = 0.0f;
	s_gmMonsterLastDistNeeded = 0.0f;
	s_gmMonsterLastFillUsed = 0.0f;
	s_gmMonsterLastModelType = -1;

	const int inferredKind = gmInferMonsterKind(monsterIndex);
	CUSTOM_MONSTER_INFO* customInfo = GMMonsterMng ? GMMonsterMng->FindMonsterByIndex(monsterIndex) : 0;
	if (customInfo && customInfo->RenderIndex < 0 && customInfo->isUsable())
	{
		customInfo->OpenLoad();
	}
	const bool isCustom = (customInfo != 0);
	if (!isCustom && inferredKind != KIND_NPC && !gmMonsterBmdExists(monsterIndex))
		return;

	const GLboolean oldScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLint oldScissorBox[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_SCISSOR_BOX, oldScissorBox);

	const int clipY = WindowHeight - (clipYTop + clipH);
	glEnable(GL_SCISSOR_TEST);
	glScissor(clipX, clipY, clipW, clipH);

	const float fovDeg = 12.0f;
	const float previewNear = 1.0f;
	const float previewFar = 20000.0f;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(clipX, clipYTop, clipW, clipH);
	gluPerspective2(fovDeg, (float)(clipW) / (float)(clipH), previewNear, previewFar);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	GetOpenGLMatrix(CameraMatrix);

	vec3_t target;
	CreateScreenVector((int)centerX, (int)centerY, target, false, false);

	vec3_t direction;
	vec3_t position;

	static const int s_gmPreviewCharacterKey = 0x6F01;

	DeleteCharacter(s_gmPreviewCharacterKey);

	CHARACTER* c = CreateMonster(monsterIndex, 0, 0, s_gmPreviewCharacterKey);
	if (c == 0)
	{
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		glPushMatrix();
		glLoadIdentity();
		glViewport2(0, 0, WindowWidth, WindowHeight);
		gluPerspective2(1.f, (float)(WindowWidth) / (float)(WindowHeight), RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		if (!oldScissorEnabled)
			glDisable(GL_SCISSOR_TEST);
		else
			glEnable(GL_SCISSOR_TEST);
		glScissor(oldScissorBox[0], oldScissorBox[1], oldScissorBox[2], oldScissorBox[3]);
		return;
	}

	OBJECT* o = &c->Object;
	const int modelType = o->Type;
	const int kind = o->Kind;
	s_gmMonsterLastModelType = modelType;

	VectorSubtract(target, MousePosition, direction);
	const float distFactor = (modelType >= MODEL_MONSTER_DUMY_BENGI) ? 0.30f : 0.18f;
	VectorMA(MousePosition, (float)ScaleMA(distFactor), direction, position);

	if (!gmIsModelReady(modelType))
	{
		if (monsterIndex >= 0 && monsterIndex < MAX_MODEL_MONSTER && gmMonsterBmdExists(monsterIndex))
		{
			OpenMonsterModel(monsterIndex);
		}
		if (kind == KIND_NPC)
		{
			OpenNpc(modelType);
		}
	}

	if (!gmIsModelReady(modelType))
	{
		DeleteCharacter(s_gmPreviewCharacterKey);

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		glPushMatrix();
		glLoadIdentity();
		glViewport2(0, 0, WindowWidth, WindowHeight);
		gluPerspective2(1.f, (float)(WindowWidth) / (float)(WindowHeight), RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		if (!oldScissorEnabled)
			glDisable(GL_SCISSOR_TEST);
		else
			glEnable(GL_SCISSOR_TEST);
		glScissor(oldScissorBox[0], oldScissorBox[1], oldScissorBox[2], oldScissorBox[3]);
		return;
	}

	o->Live = true;
	o->Visible = true;
	float baseScale = (o->Scale > 0.0f) ? o->Scale : 1.0f;
	float mul = (scaleMul > 0.0f) ? scaleMul : 1.0f;
	float renderScale = baseScale * mul;
	if (modelType >= MODEL_MONSTER_DUMY_BENGI)
		renderScale *= 0.01f;
	o->Scale = renderScale;
	o->Alpha = 1.0f;
	o->AlphaTarget = 1.0f;
	Vector(1.0f, 1.0f, 1.0f, o->Light);
	VectorCopy(position, o->Position);
	VectorCopy(position, o->StartPosition);
	Vector(270.0f, rotY, 0.0f, o->Angle);
	Vector(0.0f, 0.0f, 0.0f, o->HeadAngle);

	BMD* pModel = gmClientModels->GetModel(modelType);
	if (actionOverride >= 0 && pModel && pModel->NumActions > 0)
	{
		int act = actionOverride;
		if (act >= pModel->NumActions)
			act = pModel->NumActions - 1;
		if (act < 0)
			act = 0;
		o->CurrentAction = act;
		o->PriorAction = act;
	}
	else if (actionOverride < 0 && pModel && pModel->NumActions > 0)
	{
		const int act = gmFindSafeActionIndex(modelType);
		o->CurrentAction = act;
		o->PriorAction = act;
	}
	if (pModel)
	{
		const int oldEditFlag = EditFlag;
		EditFlag = 2;
		gmCalcObjectSafe(o, true, 0, 0);
		EditFlag = oldEditFlag;

		vec3_t centerDelta;
		Vector(0.0f, 0.0f, 0.0f, centerDelta);
		{
			const float dx = o->OBB.XAxis[0];
			const float dy = o->OBB.YAxis[1];
			const float dz = o->OBB.ZAxis[2];
			if (dx > 0.0f && dy > 0.0f && dz > 0.0f)
			{
				vec3_t bbCenter;
				bbCenter[0] = o->OBB.StartPos[0] + dx * 0.5f;
				bbCenter[1] = o->OBB.StartPos[1] + dy * 0.5f;
				bbCenter[2] = o->OBB.StartPos[2] + dz * 0.5f;
				VectorSubtract(bbCenter, o->Position, centerDelta);
			}
		}

		vec3_t centerPos;
		VectorAdd(o->Position, centerDelta, centerPos);
		vec3_t camToCenter;
		VectorSubtract(centerPos, MousePosition, camToCenter);
		const float dist = VectorLength(camToCenter);

		float fill = s_gmMonsterPreviewFill;
		if (fill < 0.45f) fill = 0.45f;
		if (fill > 0.95f) fill = 0.95f;

		s_gmMonsterLastFillUsed = fill;
		s_gmMonsterLastSize = pModel->fTransformedSize;
		s_gmMonsterLastDist = dist;

		if (pModel->fTransformedSize > 0.0f && dist > 0.0f)
		{
			const float fovRad = fovDeg * (Q_PI / 180.f);
			const float focal = ((float)clipH * 0.5f) / tanf(fovRad * 0.5f);
			const float targetPixels = (float)((clipW < clipH) ? clipW : clipH) * fill;
			float sizeForFit = pModel->fTransformedSize * 1.73f;
			{
				const float dx = o->OBB.XAxis[0];
				const float dy = o->OBB.YAxis[1];
				const float dz = o->OBB.ZAxis[2];
				if (dx > 0.0f && dy > 0.0f && dz > 0.0f)
				{
					const float diag = sqrtf(dx * dx + dy * dy + dz * dz);
					if (diag > 0.0f)
						sizeForFit = diag;
				}
			}
			const float currentPixels = focal * sizeForFit / dist;

			if (currentPixels > 0.0f && targetPixels > 0.0f)
			{
				float distNeeded = focal * sizeForFit / targetPixels;
				const float radius = sizeForFit * 0.5f;
				const float minDist = radius + previewNear;
				const float maxDist = previewFar - radius - previewNear;
				if (distNeeded < minDist)
					distNeeded = minDist;
				if (maxDist > minDist && distNeeded > maxDist)
					distNeeded = maxDist;

				s_gmMonsterLastDistNeeded = distNeeded;
				const float dirScale = distNeeded / dist;
				float distScale = dirScale;
				if (distScale < 0.05f) distScale = 0.05f;
				if (distScale > 2000.00f) distScale = 2000.00f;

				vec3_t newCenter;
				VectorMA(MousePosition, distScale, camToCenter, newCenter);
				vec3_t newOrigin;
				VectorSubtract(newCenter, centerDelta, newOrigin);
				VectorCopy(newOrigin, o->Position);
				VectorCopy(newOrigin, o->StartPosition);
			}
		}
	}

	RenderCharacter(c, o, 0);
	DeleteCharacter(s_gmPreviewCharacterKey);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glPushMatrix();
	glLoadIdentity();
	glViewport2(0, 0, WindowWidth, WindowHeight);
	gluPerspective2(1.f, (float)(WindowWidth) / (float)(WindowHeight), RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	if (!oldScissorEnabled)
		glDisable(GL_SCISSOR_TEST);
	else
		glEnable(GL_SCISSOR_TEST);
	glScissor(oldScissorBox[0], oldScissorBox[1], oldScissorBox[2], oldScissorBox[3]);
}

static bool gmStrCaseContains(const char* haystack, const char* needle)
{
	if (needle == 0 || needle[0] == '\0')
		return true;
	if (haystack == 0 || haystack[0] == '\0')
		return false;

	for (const char* h = haystack; *h; ++h)
	{
		const char* h2 = h;
		const char* n2 = needle;
		while (*h2 && *n2)
		{
			const unsigned char ch = (unsigned char)*h2;
			const unsigned char cn = (unsigned char)*n2;
			const int lh = (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
			const int ln = (cn >= 'A' && cn <= 'Z') ? (cn - 'A' + 'a') : cn;
			if (lh != ln)
				break;
			++h2;
			++n2;
		}
		if (*n2 == '\0')
			return true;
	}

	return false;
}

static int gmInferMonsterKind(int Type)
{
	int kind = KIND_MONSTER;

	if (Type == 200)
		kind = KIND_MONSTER;
	else if (Type >= 260)
		kind = KIND_MONSTER;
	else if (Type > 200)
		kind = KIND_NPC;
	else if (Type >= 150)
		kind = KIND_MONSTER;
	else if (Type > 110)
		kind = KIND_MONSTER;
	else if (Type >= 100)
		kind = KIND_TRAP;
	else
		kind = KIND_MONSTER;

	if (Type == 368 || Type == 369 || Type == 370)
		kind = KIND_NPC;

	if (Type == 367
		|| Type == 371
		|| Type == 375
		|| Type == 376 || Type == 377
		|| Type == 379
		|| Type == 380 || Type == 381 || Type == 382
		|| Type == 383 || Type == 384 || Type == 385
		|| Type == 406
		|| Type == 407
		|| Type == 408
		|| Type == 414
		|| Type == 415 || Type == 416 || Type == 417
		|| Type == 450
		|| Type == 452 || Type == 453
		|| Type == 464
		|| Type == 465
		|| Type == 467
		|| Type == 468 || Type == 469 || Type == 470
		|| Type == 471 || Type == 472 || Type == 473
		|| Type == 474 || Type == 475
		|| Type == 478
		|| Type == 479
		|| Type == 492
		|| Type == 540
		|| Type == 541
		|| Type == 542
		|| Type == 522
		|| Type == 543 || Type == 544
		|| Type == 545
		|| Type == 546
		|| Type == 547
		|| Type == 577 || Type == 578
		|| Type == 579)
	{
		kind = KIND_NPC;
	}

	if (Type >= 480 && Type <= 491)
	{
		kind = KIND_MONSTER;
	}

	if (Type == 451)
	{
		kind = KIND_TMP;
	}

	return kind;
}

static const char* gmKindLabel(int kind)
{
	switch (kind)
	{
	case KIND_NPC: return "NPC";
	case KIND_TRAP: return "TRAP";
	case KIND_TMP: return "TMP";
	default: return "MONSTER";
	}
}

static void gmRequestMonsterDb(BYTE flags)
{
	PMSG_GM_MONSTER_DB_REQ pMsg;
	pMsg.header.set(0xFA, 0x00, sizeof(pMsg));
	pMsg.flags = flags;
	DataSend((BYTE*)&pMsg, pMsg.header.size);
}

static void gmUpdateMonsterPreviewState(int monsterIndex)
{
	s_gmMonsterPreviewOk = false;
	s_gmMonsterPreviewRenderIndex = -1;
	s_gmMonsterPreviewKind = gmInferMonsterKind(monsterIndex);

	CUSTOM_MONSTER_INFO* customInfo = GMMonsterMng ? GMMonsterMng->FindMonsterByIndex(monsterIndex) : 0;
	if (customInfo && customInfo->RenderIndex < 0 && customInfo->isUsable())
	{
		customInfo->OpenLoad();
	}
	const bool isCustom = (customInfo != 0);

	if (!isCustom && s_gmMonsterPreviewKind != KIND_NPC && !gmMonsterBmdExists(monsterIndex))
	{
		return;
	}

	static const int s_gmPreviewCharacterKey = 0x6F01;
	DeleteCharacter(s_gmPreviewCharacterKey);

	CHARACTER* c = CreateMonster(monsterIndex, 0, 0, s_gmPreviewCharacterKey);
	if (c == 0)
	{
		return;
	}

	OBJECT* o = &c->Object;
	const int modelType = o->Type;
	const int kind = o->Kind;

	if (!gmIsModelReady(modelType))
	{
		if (monsterIndex >= 0 && monsterIndex < MAX_MODEL_MONSTER && gmMonsterBmdExists(monsterIndex))
		{
			OpenMonsterModel(monsterIndex);
		}
		if (kind == KIND_NPC)
		{
			OpenNpc(modelType);
		}
	}

	s_gmMonsterPreviewOk = gmIsModelReady(modelType);
	s_gmMonsterPreviewRenderIndex = modelType;
	s_gmMonsterPreviewKind = kind;

	DeleteCharacter(s_gmPreviewCharacterKey);
}

void SEASON3B::CGFxEffectHandle::RenderFrame()
{
	if (this->IsGmUser() == false)
	{
		g_pNewUISystem->Hide(INTERFACE_EFFECT_MANAGER);
		return;
	}

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(m_Pos.x * g_fScreenRate_x, m_Pos.y * g_fScreenRate_y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560.f * g_fScreenRate_x, 340.f * g_fScreenRate_y), ImGuiCond_FirstUseEver);

	static ImVec2 s_previewRectMin = ImVec2(0, 0);
	static ImVec2 s_previewRectMax = ImVec2(0, 0);
	static bool s_previewAutoRotate = false;
	static float s_previewRotY = 0.0f;
	bool previewRectValid = false;

	{
		ImGuiIO& io = ImGui::GetIO();
		if (s_previewAutoRotate)
		{
			const float dt = (io.DeltaTime > 0.0f) ? io.DeltaTime : 0.0f;
			s_previewRotY += dt * 120.0f;
			if (s_previewRotY > 360.0f)
				s_previewRotY -= 360.0f;
		}
	}

	bool windowOpen = true;
	const bool windowVisible = ImGui::Begin("RenderMesh Tools", &windowOpen, ImGuiWindowFlags_NoCollapse);

	if (windowVisible)
	{
		if (ImGui::BeginTabBar("##gm_menu_tabs"))
		{
			if (ImGui::BeginTabItem("Items"))
			{
				s_gmMenuActiveTab = 0;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Monsters/NPCs"))
			{
				s_gmMenuActiveTab = 1;
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::BeginChild("Child1", ImVec2((220 * g_fScreenRate_x), 0), ImGuiChildFlags_Border);

		ImGui::BeginChild("Sub-Child1", ImVec2(0, (200.f * g_fScreenRate_y)), ImGuiChildFlags_Border);
		{
			ImGui::SetCursorPos(ImVec2(6.0f * g_fScreenRate_x, 6.0f * g_fScreenRate_y));
			ImGui::Text("Preview");
			ImGui::SetCursorPos(ImVec2(6.0f * g_fScreenRate_x, 24.0f * g_fScreenRate_y));
			ImGui::Checkbox("Auto rotate", &s_previewAutoRotate);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("Automatically rotates the preview model.");

			ImGui::SetCursorPos(ImVec2(0.0f, 44.0f * g_fScreenRate_y));
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			ImGui::InvisibleButton("##preview_capture", avail);
			s_previewRectMin = ImGui::GetItemRectMin();
			s_previewRectMax = ImGui::GetItemRectMax();
			previewRectValid = true;
		}
		ImGui::EndChild();

		ImGui::BeginChild("Sub-Child2", ImVec2(0, 0), ImGuiChildFlags_Border);
		{
			ImGui::Text("Diagnostics");

			if (s_gmMenuActiveTab == 0)
			{
				ImGui::Checkbox("Auto log", &s_gmAuditAutoLog);
				ImGui::Checkbox("Log missing model", &s_gmAuditLogMissingModels);
				ImGui::Checkbox("Log missing tooltip", &s_gmAuditLogMissingTooltips);
				ImGui::Checkbox("Log missing name", &s_gmAuditLogMissingName);
				ImGui::Checkbox("Log missing client data", &s_gmAuditLogMissingClientData);

				if (ImGui::Button("Scan all"))
				{
					s_gmAuditScanActive = true;
					s_gmAuditScanIndex = 0;
					s_gmAuditScanTotal = MAX_ITEM_LINE;
					s_gmAuditLoggedIssues.clear();
					s_gmAuditIssuesMissingModel = 0;
					s_gmAuditIssuesMissingTooltip = 0;
					s_gmAuditIssuesMissingName = 0;
					s_gmAuditIssuesMissingClientData = 0;
					s_gmAuditLastIssueItem = -1;
					s_gmAuditLastIssue[0] = '\0';
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop"))
				{
					s_gmAuditScanActive = false;
				}

				if (s_gmAuditScanTotal > 0)
				{
					const float p = (s_gmAuditScanIndex <= 0) ? 0.0f : (float)s_gmAuditScanIndex / (float)s_gmAuditScanTotal;
					ImGui::ProgressBar(p, ImVec2(-1.0f, 0.0f));
				}

				ImGui::Text("Missing model: %d", s_gmAuditIssuesMissingModel);
				ImGui::Text("Missing tooltip: %d", s_gmAuditIssuesMissingTooltip);
				ImGui::Text("Missing name: %d", s_gmAuditIssuesMissingName);
				ImGui::Text("Missing client data: %d", s_gmAuditIssuesMissingClientData);

				if (s_gmAuditLastIssueItem >= 0 && s_gmAuditLastIssue[0] != '\0')
				{
					ImGui::Text("Last: %d", s_gmAuditLastIssueItem);
					ImGui::TextWrapped("%s", s_gmAuditLastIssue);
				}
			}
			else
			{
				const std::vector<GM_MONSTER_INFO_NET>& db = GMMonsterDb_Get();
				ImGui::Text("MonsterDB: %d / %d", GMMonsterDb_Received(), GMMonsterDb_Total());

				if (ImGui::Button("Scan models"))
				{
					s_gmMonsterScanActive = (db.empty() ? false : true);
					s_gmMonsterScanIndex = 0;
					s_gmMonsterScanTotal = (int)db.size();
					s_gmMonsterIssuesMissingModel = 0;
					s_gmMonsterIssuesCreateFail = 0;
					s_gmMonsterIssuesModelNotReady = 0;
					s_gmMonsterLastIssue[0] = '\0';
					s_gmMonsterReportInit = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop"))
				{
					s_gmMonsterScanActive = false;
				}

				if (s_gmMonsterScanTotal > 0)
				{
					const float p = (s_gmMonsterScanIndex <= 0) ? 0.0f : (float)s_gmMonsterScanIndex / (float)s_gmMonsterScanTotal;
					ImGui::ProgressBar(p, ImVec2(-1.0f, 0.0f));
				}

				ImGui::Text("Missing model: %d", s_gmMonsterIssuesMissingModel);
				ImGui::Text("Create fail: %d", s_gmMonsterIssuesCreateFail);
				ImGui::Text("Not ready: %d", s_gmMonsterIssuesModelNotReady);

				if (s_gmMonsterLastIssue[0] != '\0')
					ImGui::TextWrapped("%s", s_gmMonsterLastIssue);

				if (s_gmMonsterScanActive && s_gmMonsterScanTotal > 0)
				{
					const int end = (s_gmMonsterScanIndex + s_gmMonsterScanBatch > s_gmMonsterScanTotal) ? s_gmMonsterScanTotal : (s_gmMonsterScanIndex + s_gmMonsterScanBatch);
					for (int i = s_gmMonsterScanIndex; i < end; ++i)
					{
						const int monsterIndex = db[i].Index;
						const char* name = (db[i].Name[0] != '\0') ? db[i].Name : "Unnamed";
						const int inferredKind = gmInferMonsterKind(monsterIndex);
						CUSTOM_MONSTER_INFO* customInfo = GMMonsterMng ? GMMonsterMng->FindMonsterByIndex(monsterIndex) : 0;
						if (customInfo && customInfo->RenderIndex < 0 && customInfo->isUsable())
						{
							customInfo->OpenLoad();
						}
						const bool isCustom = (customInfo != 0);

						if (!isCustom && inferredKind != KIND_NPC && !gmMonsterBmdExists(monsterIndex))
						{
							s_gmMonsterIssuesMissingModel++;
							sprintf_s(s_gmMonsterLastIssue, "BMD missing: idx=%d kind=%s name=%s", monsterIndex, gmKindLabel(inferredKind), name);
							if (!s_gmMonsterReportInit)
							{
								g_ErrorReport.Create("GMMenu_MonsterAudit.log");
								s_gmMonsterReportInit = true;
							}
							g_ErrorReport.Write("[GMMenu] BMD missing: idx=%d kind=%d name=%s\r\n", monsterIndex, inferredKind, name);
							continue;
						}

						static const int s_gmScanCharacterKey = 0x6F02;
						DeleteCharacter(s_gmScanCharacterKey);

						CHARACTER* c = CreateMonster(monsterIndex, 0, 0, s_gmScanCharacterKey);
						if (c == 0)
						{
							s_gmMonsterIssuesMissingModel++;
							s_gmMonsterIssuesCreateFail++;
							sprintf_s(s_gmMonsterLastIssue, "CreateMonster failed: %d %s", monsterIndex, name);
							if (!s_gmMonsterReportInit)
							{
								g_ErrorReport.Create("GMMenu_MonsterAudit.log");
								s_gmMonsterReportInit = true;
							}
							g_ErrorReport.Write("[GMMenu] CreateMonster failed: idx=%d name=%s\r\n", monsterIndex, name);
							continue;
						}

						const int modelType = c->Object.Type;
						const int kind = c->Object.Kind;

						if (!gmIsModelReady(modelType))
						{
							if (monsterIndex >= 0 && monsterIndex < MAX_MODEL_MONSTER && gmMonsterBmdExists(monsterIndex))
								OpenMonsterModel(monsterIndex);
							if (kind == KIND_NPC)
								OpenNpc(modelType);
						}

						if (!gmIsModelReady(modelType))
						{
							s_gmMonsterIssuesMissingModel++;
							s_gmMonsterIssuesModelNotReady++;
							sprintf_s(s_gmMonsterLastIssue, "Model missing: idx=%d model=%d kind=%s name=%s", monsterIndex, modelType, gmKindLabel(kind), name);
							if (!s_gmMonsterReportInit)
							{
								g_ErrorReport.Create("GMMenu_MonsterAudit.log");
								s_gmMonsterReportInit = true;
							}
							g_ErrorReport.Write("[GMMenu] Model missing: idx=%d model=%d kind=%d name=%s\r\n", monsterIndex, modelType, kind, name);
						}

						DeleteCharacter(s_gmScanCharacterKey);
					}
					s_gmMonsterScanIndex = end;
					if (s_gmMonsterScanIndex >= s_gmMonsterScanTotal)
					{
						s_gmMonsterScanActive = false;
					}
				}
			}
		}
		ImGui::EndChild();

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("Child2", ImVec2(0, 0), ImGuiChildFlags_Border);

		this->RenderContents();

		ImGui::EndChild();
	}
	ImGui::End();

	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	if (windowOpen == false)
	{
		g_pNewUISystem->Hide(INTERFACE_EFFECT_MANAGER);
		return;
	}

	if (previewRectValid)
	{
		g_GMMenuPreviewAutoRotate = s_previewAutoRotate;
		g_GMMenuPreviewRotateY = s_previewRotY;

		const float previewX = s_previewRectMin.x / g_fScreenRate_x;
		const float previewY = s_previewRectMin.y / g_fScreenRate_y;
		const float previewW = (s_previewRectMax.x - s_previewRectMin.x) / g_fScreenRate_x;
		const float previewH = (s_previewRectMax.y - s_previewRectMin.y) / g_fScreenRate_y;

		if (s_gmMenuActiveTab == 0)
		{
			if (selectedItem != -1 && m_bPreviewModelOk)
			{
				Script_Item* item_info = GMItemMng->find(selectedItem);
				if (item_info && item_info->Name[0] != '\0')
				{
					const float boxW = previewW * 0.85f;
					const float boxH = previewH * 0.85f;
					const float sx = previewX + ((previewW - boxW) * 0.5f);
					const float sy = previewY + ((previewH - boxH) * 0.5f);

					SEASON3B::begin3D();
					g_GMMenuPreviewActive = true;
					RenderItem3D(
						sx,
						sy,
						boxW,
						boxH,
						selectedItem,
						(m_SpawnLevel << 3),
						0,
						0,
						false,
						previewX,
						previewY,
						previewW,
						previewH,
						0.0f);
					g_GMMenuPreviewActive = false;
					SEASON3B::endrender3D();
				}
			}
		}
		else
		{
			if (s_gmSelectedMonsterIndex != -1)
			{
				const float rotY = s_previewAutoRotate ? s_previewRotY : 0.0f;
				SEASON3B::begin3D();
				g_GMMenuPreviewActive = true;
				gmRenderMonsterPreview3D(
					s_gmSelectedMonsterIndex,
					s_gmMonsterPreviewScale,
					rotY,
					previewX,
					previewY,
					previewW,
					previewH,
					s_gmMonsterPreviewAction);
				g_GMMenuPreviewActive = false;
				SEASON3B::endrender3D();
			}
		}
	}
}

void SEASON3B::CGFxEffectHandle::RenderContents()
{
	if (s_gmMenuActiveTab != 0)
	{
		static char s_monFilter[64] = { 0 };
		static int s_monSpawnCount = 1;
		static char s_monSpawnCommand[32] = "/summon";
		static int s_monKindFilter = 0;
		static int s_monSourceFilter = 0;

		const bool loading = GMMonsterDb_IsLoading();
		const bool ready = GMMonsterDb_IsReady();
		const int total = GMMonsterDb_Total();
		const int received = GMMonsterDb_Received();

		if (!ready && !loading && !s_gmMonsterDbRequested)
		{
			gmRequestMonsterDb(0);
			s_gmMonsterDbRequested = true;
		}

		ImGui::BeginChild("MonList", ImVec2((320 * g_fScreenRate_x), 0), ImGuiChildFlags_Border);

		if (ImGui::Button("Reload from server"))
		{
			GMMonsterDb_Reset();
			s_gmMonsterDbRequested = false;
			s_gmSelectedMonsterIndex = -1;
			s_gmSelectedMonsterRow = -1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Request"))
		{
			GMMonsterDb_Reset();
			gmRequestMonsterDb(0);
			s_gmMonsterDbRequested = true;
		}

		if (loading)
		{
			ImGui::Text("Loading: %d / %d", received, total);
			if (total > 0)
			{
				const float p = (received <= 0) ? 0.0f : (float)received / (float)total;
				ImGui::ProgressBar(p, ImVec2(-1.0f, 0.0f));
			}
		}
		else if (ready)
		{
			ImGui::Text("Loaded: %d", received);
		}
		else
		{
			ImGui::Text("Not loaded");
		}

		ImGui::Separator();

		ImGui::InputText("Filter", s_monFilter, sizeof(s_monFilter));

		const char* kindItems[] = { "All", "MONSTER", "NPC", "TRAP", "TMP" };
		ImGui::Combo("Kind", &s_monKindFilter, kindItems, IM_ARRAYSIZE(kindItems));

		const char* srcItems[] = { "All", "Standard", "Custom" };
		ImGui::Combo("Source", &s_monSourceFilter, srcItems, IM_ARRAYSIZE(srcItems));

		const std::vector<GM_MONSTER_INFO_NET>& db = GMMonsterDb_Get();
		std::vector<int> viewRows;
		viewRows.reserve(db.size());

		for (int i = 0; i < (int)db.size(); ++i)
		{
			const GM_MONSTER_INFO_NET& m = db[i];

			const int kind = gmInferMonsterKind(m.Index);
			const bool isCustom = (GMMonsterMng->FindMonsterByIndex(m.Index) != 0);

			if (s_monKindFilter == 1 && kind != KIND_MONSTER) continue;
			if (s_monKindFilter == 2 && kind != KIND_NPC) continue;
			if (s_monKindFilter == 3 && kind != KIND_TRAP) continue;
			if (s_monKindFilter == 4 && kind != KIND_TMP) continue;

			if (s_monSourceFilter == 1 && isCustom) continue;
			if (s_monSourceFilter == 2 && !isCustom) continue;

			char label[256] = { 0 };
			sprintf_s(label, "%d %s %s", m.Index, gmKindLabel(kind), m.Name);
			if (!gmStrCaseContains(label, s_monFilter))
				continue;

			viewRows.push_back(i);
		}

		const ImGuiTableFlags flags =
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_Reorderable |
			ImGuiTableFlags_Sortable |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		const float tableHeight = ImGui::GetContentRegionAvail().y;
		if (ImGui::BeginTable("##gm_monster_table", 6, flags, ImVec2(0, tableHeight)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort);
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Kind");
			ImGui::TableSetupColumn("Source");
			ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("Life", ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableHeadersRow();

			if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
			{
				if (sortSpecs->SpecsCount > 0)
				{
					const ImGuiTableColumnSortSpecs* spec = &sortSpecs->Specs[0];
					const int col = spec->ColumnIndex;
					const bool desc = (spec->SortDirection == ImGuiSortDirection_Descending);

					std::sort(viewRows.begin(), viewRows.end(), [&](int a, int b)
						{
							const GM_MONSTER_INFO_NET& A = db[a];
							const GM_MONSTER_INFO_NET& B = db[b];

							int cmp = 0;
							switch (col)
							{
							case 0: cmp = (A.Index < B.Index) ? -1 : (A.Index > B.Index ? 1 : 0); break;
							case 1: cmp = _stricmp(A.Name, B.Name); break;
							case 2:
							{
								const int ak = gmInferMonsterKind(A.Index);
								const int bk = gmInferMonsterKind(B.Index);
								cmp = (ak < bk) ? -1 : (ak > bk ? 1 : 0);
							}
							break;
							case 3:
							{
								const bool ac = (GMMonsterMng->FindMonsterByIndex(A.Index) != 0);
								const bool bc = (GMMonsterMng->FindMonsterByIndex(B.Index) != 0);
								cmp = (ac == bc) ? 0 : (ac ? 1 : -1);
							}
							break;
							case 4: cmp = (A.Level < B.Level) ? -1 : (A.Level > B.Level ? 1 : 0); break;
							case 5: cmp = (A.Life < B.Life) ? -1 : (A.Life > B.Life ? 1 : 0); break;
							default: cmp = 0; break;
							}

							return desc ? (cmp > 0) : (cmp < 0);
						});
				}
			}

			for (int n = 0; n < (int)viewRows.size(); ++n)
			{
				const int rowIndex = viewRows[n];
				const GM_MONSTER_INFO_NET& m = db[rowIndex];
				const int kind = gmInferMonsterKind(m.Index);
				const bool isCustom = (GMMonsterMng->FindMonsterByIndex(m.Index) != 0);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const bool selected = (s_gmSelectedMonsterRow == rowIndex);
				if (ImGui::Selectable(std::to_string(m.Index).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
				{
					s_gmSelectedMonsterRow = rowIndex;
					s_gmSelectedMonsterIndex = m.Index;
					s_gmMonsterPreviewScale = 1.0f;
					s_gmMonsterPreviewFill = 0.80f;
					s_gmMonsterPreviewAction = -1;
					gmUpdateMonsterPreviewState(m.Index);
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted((m.Name[0] != '\0') ? m.Name : "Unnamed");

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(gmKindLabel(kind));

				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(isCustom ? "Custom" : "Std");

				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%d", m.Level);

				ImGui::TableSetColumnIndex(5);
				ImGui::Text("%d", m.Life);
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("MonDetails", ImVec2(0, 0), ImGuiChildFlags_Border);

		if (s_gmSelectedMonsterRow < 0 || s_gmSelectedMonsterRow >= (int)db.size())
		{
			ImGui::Text("Select a monster");
		}
		else
		{
			const GM_MONSTER_INFO_NET& m = db[s_gmSelectedMonsterRow];
			const int kind = gmInferMonsterKind(m.Index);
			const bool isCustom = (GMMonsterMng->FindMonsterByIndex(m.Index) != 0);

			ImGui::Text("Index: %d", m.Index);
			ImGui::Text("Name: %s", (m.Name[0] != '\0') ? m.Name : "Unnamed");
			ImGui::Text("Kind: %s", gmKindLabel(kind));
			ImGui::Text("Source: %s", isCustom ? "Custom" : "Standard");

			ImGui::Separator();

			ImGui::Text("Preview: %s", s_gmMonsterPreviewOk ? "OK" : "Missing");
			ImGui::Text("ModelId: %d", s_gmMonsterPreviewRenderIndex);
			if (s_gmMonsterLastModelType >= 0)
			{
				ImGui::Text("PreviewModel: %d", s_gmMonsterLastModelType);
				ImGui::Text("Fit: size=%.2f dist=%.2f need=%.2f fill=%.2f", s_gmMonsterLastSize, s_gmMonsterLastDist, s_gmMonsterLastDistNeeded, s_gmMonsterLastFillUsed);
			}
			if (ImGui::Button("Refresh preview state"))
			{
				gmUpdateMonsterPreviewState(m.Index);
			}

			ImGui::SliderFloat("Scale", &s_gmMonsterPreviewScale, 0.05f, 3.0f, "%.2f");
			ImGui::SliderFloat("Fill", &s_gmMonsterPreviewFill, 0.45f, 0.95f, "%.2f");

			BMD* pModel = (gmClientModels && s_gmMonsterPreviewRenderIndex >= 0) ? gmClientModels->GetModel(s_gmMonsterPreviewRenderIndex) : 0;
			const int actionCount = (pModel != 0) ? pModel->NumActions : 0;

			bool useDefaultAction = (s_gmMonsterPreviewAction < 0);
			if (ImGui::Checkbox("Default action", &useDefaultAction))
			{
				s_gmMonsterPreviewAction = useDefaultAction ? -1 : 0;
			}

			if (!useDefaultAction)
			{
				if (actionCount > 0)
				{
					if (s_gmMonsterPreviewAction < 0) s_gmMonsterPreviewAction = 0;
					if (s_gmMonsterPreviewAction >= actionCount) s_gmMonsterPreviewAction = actionCount - 1;
					ImGui::SliderInt("Action", &s_gmMonsterPreviewAction, 0, actionCount - 1);
				}
				else
				{
					s_gmMonsterPreviewAction = -1;
					ImGui::Text("Actions: 0");
				}
			}
			else
			{
				ImGui::Text("Actions: %d", actionCount);
			}

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Level: %d", m.Level);
				ImGui::Text("AINumber: %d", m.AINumber);
				ImGui::Text("Attribute: %d", m.Attribute);
				ImGui::Text("MonsterSkill: %d", m.MonsterSkill);
				ImGui::Text("Rate: %d", m.Rate);
				ImGui::Text("RegenTime: %d", m.RegenTime);
				ImGui::Text("Life: %d  Mana: %d  ScriptLife: %d", m.Life, m.Mana, m.ScriptLife);
				ImGui::Text("Damage: %d - %d", m.DamageMin, m.DamageMax);
				ImGui::Text("Defense: %d  MagicDefense: %d", m.Defense, m.MagicDefense);
				ImGui::Text("AttackRate: %d  DefenseRate: %d", m.AttackRate, m.DefenseRate);
				ImGui::Text("MoveSpeed: %d  AttackSpeed: %d", m.MoveSpeed, m.AttackSpeed);
				ImGui::Text("MoveRange: %d  AttackRange: %d  ViewRange: %d", m.MoveRange, m.AttackRange, m.ViewRange);
				ImGui::Text("AttackType: %d", m.AttackType);
				ImGui::Text("ItemRate: %d  MoneyRate: %d  MaxItemLevel: %d", m.ItemRate, m.MoneyRate, m.MaxItemLevel);
			}

			if (ImGui::CollapsingHeader("Resist"))
			{
				ImGui::Text("0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d",
					m.Resistance[0],
					m.Resistance[1],
					m.Resistance[2],
					m.Resistance[3],
					m.Resistance[4],
					m.Resistance[5],
					m.Resistance[6]);
			}

			if (ImGui::CollapsingHeader("Elemental"))
			{
				ImGui::Text("Attr: %d  Pattern: %d  Def: %d", m.ElementalAttribute, m.ElementalPattern, m.ElementalDefense);
				ImGui::Text("Dmg: %d - %d", m.ElementalDamageMin, m.ElementalDamageMax);
				ImGui::Text("AtkRate: %d  DefRate: %d", m.ElementalAttackRate, m.ElementalDefenseRate);
			}

			ImGui::Separator();

			ImGui::Text("Spawn");
			ImGui::InputText("Command", s_monSpawnCommand, sizeof(s_monSpawnCommand));
			ImGui::InputInt("Count", &s_monSpawnCount);
			if (s_monSpawnCount < 1) s_monSpawnCount = 1;
			if (s_monSpawnCount > 100) s_monSpawnCount = 100;

			if (ImGui::Button("Spawn"))
			{
				char cmd[128] = { 0 };
				const char* prefix = (s_monSpawnCommand[0] != '\0') ? s_monSpawnCommand : "/summon";
				if (prefix[0] == '/')
					sprintf_s(cmd, "%s %d %d", prefix, m.Index, s_monSpawnCount);
				else
					sprintf_s(cmd, "/%s %d %d", prefix, m.Index, s_monSpawnCount);
				gmSendChatRaw(cmd);
			}
		}

		ImGui::EndChild();
		return;
	}

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

	auto gmAuditEnsureReport = []()
		{
			if (s_gmAuditReportInit)
				return;
			g_ErrorReport.Create("GMMenu_ItemAudit.log");
			s_gmAuditReportInit = true;
		};

	auto gmAuditRecord = [&](int issueType, int itemIndex, bool shouldLog, const char* fmt, ...)
		{
			char msg[512] = { 0 };
			va_list args;
			va_start(args, fmt);
			vsprintf_s(msg, fmt, args);
			va_end(args);

			s_gmAuditLastIssueItem = itemIndex;
			strcpy_s(s_gmAuditLastIssue, msg);

			const unsigned long long key = ((unsigned long long)(unsigned int)issueType << 32) | (unsigned long long)(unsigned int)itemIndex;
			const bool firstTime = s_gmAuditLoggedIssues.insert(key).second;
			if (firstTime)
			{
				switch (issueType)
				{
				case 1: s_gmAuditIssuesMissingModel++; break;
				case 2: s_gmAuditIssuesMissingTooltip++; break;
				case 3: s_gmAuditIssuesMissingName++; break;
				case 4: s_gmAuditIssuesMissingClientData++; break;
				default: break;
				}
			}

			if (!firstTime)
				return;

			if (!shouldLog)
				return;
			if (!s_gmAuditAutoLog)
				return;

			gmAuditEnsureReport();
			g_ErrorReport.Write("[GMMenu] %s\r\n", msg);
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
	ImGuiIO& io = ImGui::GetIO();
	const float oldRepeatDelay = io.KeyRepeatDelay;
	const float oldRepeatRate = io.KeyRepeatRate;
	io.KeyRepeatDelay = 0.35f;
	io.KeyRepeatRate = 0.10f;
	ImGui::PushButtonRepeat(true);
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
	ImGui::PopButtonRepeat();
	io.KeyRepeatDelay = oldRepeatDelay;
	io.KeyRepeatRate = oldRepeatRate;

	if (m_ItemSection < 0) m_ItemSection = 0;
	if (m_ItemSection > 15) m_ItemSection = 15;
	if (m_ItemType < 0) m_ItemType = 0;
	if (m_ItemType > 511) m_ItemType = 511;

	SyncItemIndexFromSectionType();
	item_info = GMItemMng->find(selectedItem);

	ImGui::Text("Index: %d", selectedItem);

	if (item_info == 0)
	{
		ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Client Data: MISSING (Item.bmd)");
		gmAuditRecord(4, selectedItem, s_gmAuditLogMissingClientData, "Missing client data: item=%d", selectedItem);
		return;
	}

	if (item_info->Name[0] != '\0')
	{
		ImGui::Text("%s", item_info->Name);
	}
	else
	{
		ImGui::Text("No name in Item.bmd");
		gmAuditRecord(3, selectedItem, s_gmAuditLogMissingName, "Missing name: item=%d", selectedItem);
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

			gmAuditRecord(1, selectedItem, s_gmAuditLogMissingModels,
				"Missing model: item=%d section=%d type=%d modelId=%d guess=%s exists=%d",
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

	{
		const bool isEquipmentKind =
			(item_info->Kind1 == ItemKind1::WEAPON) ||
			(item_info->Kind1 == ItemKind1::ARMOR) ||
			(item_info->Kind1 == ItemKind1::MASTERY_WEAPON) ||
			(item_info->Kind1 == ItemKind1::MASTERY_ARMOR_1) ||
			(item_info->Kind1 == ItemKind1::MASTERY_ARMOR_2);

		if (isEquipmentKind)
		{
			std::string classInfo = gmBuildItemClassInfoText(item_info);
			if (classInfo.empty())
				ImGui::Text("Classes: (not specified)");
			else if (classInfo.size() > 80)
				ImGui::TextWrapped("Classes: %s", classInfo.c_str());
			else
				ImGui::Text("Classes: %s", classInfo.c_str());
		}
	}

#ifdef PACK_FILE_DECRYPT_H
	{
		bool tooltipOk = true;
		char reason[256] = { 0 };

		if (g_pNewItemTooltip == 0)
		{
			tooltipOk = false;
			sprintf_s(reason, "Tooltip system not initialized");
		}
		else
		{
			_ITEM_TOOLTIP_DATA* tooltip = g_pNewItemTooltip->FindTooltip(selectedItem);
			if (tooltip == 0)
			{
				tooltipOk = false;
				sprintf_s(reason, "No entry in itemtooltip.bmd (Data\\\\Local\\\\%s\\\\itemtooltip.bmd)", g_strSelectedML.c_str());
			}
			else
			{
				if (tooltip->ItemLevel >= 0)
				{
					const int findLevel = tooltip->ItemLevel + (m_SpawnLevel & 0x0F);
					if (g_pNewItemTooltip->FindLevelTooltip(findLevel) == 0)
					{
						tooltipOk = false;
						sprintf_s(reason, "Missing ItemLevelTooltip index=%d (Data\\\\Local\\\\%s\\\\ItemLevelTooltip.bmd)", findLevel, g_strSelectedML.c_str());
					}
				}

				int totalText = 0;
				int missingText = 0;
				for (int i = 0; i < 12; ++i)
				{
					const int textIndex = tooltip->TextInfo[i].Text;
					if (textIndex <= 0)
						continue;
					totalText++;
					if (g_pNewItemTooltip->FindTooltipText(textIndex) == 0)
						missingText++;
				}

				if (tooltipOk && totalText > 0 && missingText == totalText)
				{
					tooltipOk = false;
					sprintf_s(reason, "All TooltipText indices are missing (Data\\\\Local\\\\%s\\\\ItemTooltipText.bmd)", g_strSelectedML.c_str());
				}
			}
		}

		if (tooltipOk)
		{
			ImGui::Text("Tooltip: OK");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "Tooltip: MISSING");
			ImGui::TextWrapped("%s", reason);
			gmAuditRecord(2, selectedItem, s_gmAuditLogMissingTooltips, "Missing tooltip: item=%d reason=%s", selectedItem, reason);
		}
	}
#endif // PACK_FILE_DECRYPT_H

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
	{
		const int kind2 = item_info ? (int)item_info->Kind2 : 0;
		const bool isWeapon = (kind2 >= 1 && kind2 <= 14) || kind2 == 78 || kind2 == 81 || kind2 == 84 || kind2 == 89 || kind2 == 90;
		const bool isArmorOrShield = (kind2 >= 15 && kind2 <= 20) || kind2 == 77;

		const int addValue = (m_SpawnOption < 0 ? 0 : (m_SpawnOption > 7 ? 7 : m_SpawnOption)) * 4;

		char optionText[128] = { 0 };
		if (isWeapon)
		{
			const int baseMin = item_info ? (int)item_info->DamageMin : 0;
			const int baseMax = item_info ? (int)item_info->DamageMax : 0;
			sprintf_s(optionText, "+Damage +%d  (DMG ~%d-%d)", addValue, baseMin + addValue, baseMax + addValue);
		}
		else if (isArmorOrShield)
		{
			const int baseDef = item_info ? (int)item_info->Defense : 0;
			sprintf_s(optionText, "+Defense +%d  (DEF ~%d)", addValue, baseDef + addValue);
		}
		else
		{
			sprintf_s(optionText, "+Option +%d", addValue);
		}

		ImGui::Text("Option (+0..+7): %s", optionText);
		ImGui::SliderInt("##gm_spawn_option", &m_SpawnOption, 0, 7);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
			ImGui::SetTooltip("Basic option adds +Damage for weapons, +Defense for armor/shields (approx: +4 per step).");
	}

	{
		const int kind2 = item_info ? (int)item_info->Kind2 : 0;
		const bool isWeapon = (kind2 >= 1 && kind2 <= 14) || kind2 == 78 || kind2 == 81 || kind2 == 84 || kind2 == 89 || kind2 == 90;
		const bool isArmorOrShield = (kind2 >= 15 && kind2 <= 20) || kind2 == 77;
		const bool isWing = (kind2 == 23 || kind2 == 24 || kind2 == 25 || kind2 == 76 || kind2 == 80);
		const bool isAccessory = (kind2 == 29 || kind2 == 30 || kind2 == 31);

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

		const char* excWing[6] = {
			"Exc: Ignore enemy defense",
			"Exc: Return damage",
			"Exc: Complete life recovery",
			"Exc: Complete mana recovery",
			"Exc: Increase attack speed",
			"Exc: Increase damage",
		};

		const char** excNames =
			isWeapon ? excWeapon :
			(isArmorOrShield ? excArmor :
				(isWing ? excWing :
					(isAccessory ? excArmor : 0)));

		auto setExcBit = [this](int mask, bool enabled)
			{
				m_SpawnExc = enabled ? (m_SpawnExc | mask) : (m_SpawnExc & ~mask);
			};

		ImGui::Text("Excellent");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
			ImGui::SetTooltip("Excellent is a bitmask of 6 options; meaning depends on item type.");
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
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("Sends the selected excellent option bit to server (OptionEx).");
		}
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Advanced"))
	{
		char nameA[64] = { 0 };
		char nameB[64] = { 0 };
		const bool hasA = g_csItemOption.GetSetItemName(nameA, selectedItem, EXT_A_SET_OPTION);
		const bool hasB = g_csItemOption.GetSetItemName(nameB, selectedItem, EXT_B_SET_OPTION);
		const bool canAncient = (hasA || hasB);

		if (canAncient)
		{
			bool ancientEnabled = (m_SpawnSet != 0);
			if (ImGui::Checkbox("Ancient", &ancientEnabled))
			{
				if (ancientEnabled == false)
					m_SpawnSet = 0;
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("Enables Ancient (set) option for this item.");

			if (ancientEnabled)
			{
				static int s_variant = 0;
				static bool s_bonus10 = true;

				if (s_variant == 0 && hasA == false) s_variant = 1;
				if (s_variant == 1 && hasB == false) s_variant = 0;

				ImGui::Text("Ancient set");
				if (hasA) ImGui::RadioButton(nameA, &s_variant, 0);
				if (hasB) ImGui::RadioButton(nameB, &s_variant, 1);

				ImGui::Checkbox("Ancient bonus +10", &s_bonus10);
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
					ImGui::SetTooltip("Ancient bonus flag: +10 (bit 8) or +5 (bit 4).");

				const int setIndexBits = (s_variant == 0) ? 1 : 2;
				const int setValueBits = s_bonus10 ? 8 : 4;
				m_SpawnSet = (setIndexBits | setValueBits);
			}
		}
		else
		{
			m_SpawnSet = 0;
		}

		ITEM tmpItem;
		memset(&tmpItem, 0, sizeof(tmpItem));
		tmpItem.Type = selectedItem;
		const bool canSocket = (g_SocketItemMgr.IsSocketItem(&tmpItem) != FALSE);
		if (canSocket)
		{
			ImGui::SliderInt("Sockets (0..5)", &m_SpawnSocket, 0, 5);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
				ImGui::SetTooltip("Creates N empty sockets (server uses 0xFE as empty socket marker).");
		}
		else
		{
			m_SpawnSocket = 0;
		}
	}

	ImGui::NewLine();

	if (ImGui::Button("To inventory")) this->SendItemSpawn(0);
	ImGui::SameLine();
	if (ImGui::Button("To ground")) this->SendItemSpawn(1);
	ImGui::SameLine();
	if (ImGui::Button("Create set")) this->SendItemSpawn(2);

	ImGui::SameLine();
	if (ImGui::Button("Clear inventory (keep equipped)")) this->SendClearInventoryKeepEquipped();

	if (s_gmAuditScanActive)
	{
		const int end = (s_gmAuditScanIndex + s_gmAuditScanBatch > s_gmAuditScanTotal) ? s_gmAuditScanTotal : (s_gmAuditScanIndex + s_gmAuditScanBatch);
		for (int idx = s_gmAuditScanIndex; idx < end; ++idx)
		{
			Script_Item* scanItem = GMItemMng->find(idx);

			if (scanItem == 0)
			{
				gmAuditRecord(4, idx, s_gmAuditLogMissingClientData, "Missing client data: item=%d", idx);
				continue;
			}

			if (scanItem->Name[0] == '\0')
			{
				gmAuditRecord(3, idx, s_gmAuditLogMissingName, "Missing name: item=%d", idx);
			}

			const int modelId = MODEL_ITEM + idx;
			BMD* scanModel = gmClientModels ? gmClientModels->GetModel(modelId) : 0;
			const bool modelOk = (scanModel != 0 && scanModel->NumMeshs > 0 && scanModel->NumBones > 0 && scanModel->NumActions > 0);
			if (!modelOk)
			{
				const int section = (idx >> 9);
				const int type = (idx & 511);
				char guessPath[260] = { 0 };
				guessModelPath(section, type, guessPath, sizeof(guessPath));
				gmAuditRecord(1, idx, s_gmAuditLogMissingModels,
					"Missing model: item=%d section=%d type=%d modelId=%d guess=%s exists=%d",
					idx,
					section,
					type,
					modelId,
					guessPath[0] ? guessPath : "(unknown)",
					guessPath[0] ? (fileExists(guessPath) ? 1 : 0) : -1);
			}

#ifdef PACK_FILE_DECRYPT_H
			{
				bool tooltipOk = true;
				char reason[256] = { 0 };

				if (g_pNewItemTooltip == 0)
				{
					tooltipOk = false;
					sprintf_s(reason, "Tooltip system not initialized");
				}
				else
				{
					_ITEM_TOOLTIP_DATA* tooltip = g_pNewItemTooltip->FindTooltip(idx);
					if (tooltip == 0)
					{
						tooltipOk = false;
						sprintf_s(reason, "No entry in itemtooltip.bmd (Data\\\\Local\\\\%s\\\\itemtooltip.bmd)", g_strSelectedML.c_str());
					}
					else
					{
						if (tooltip->ItemLevel >= 0)
						{
							const int findLevel = tooltip->ItemLevel;
							if (g_pNewItemTooltip->FindLevelTooltip(findLevel) == 0)
							{
								tooltipOk = false;
								sprintf_s(reason, "Missing ItemLevelTooltip index=%d (Data\\\\Local\\\\%s\\\\ItemLevelTooltip.bmd)", findLevel, g_strSelectedML.c_str());
							}
						}

						int totalText = 0;
						int missingText = 0;
						for (int i = 0; i < 12; ++i)
						{
							const int textIndex = tooltip->TextInfo[i].Text;
							if (textIndex <= 0)
								continue;
							totalText++;
							if (g_pNewItemTooltip->FindTooltipText(textIndex) == 0)
								missingText++;
						}

						if (tooltipOk && totalText > 0 && missingText == totalText)
						{
							tooltipOk = false;
							sprintf_s(reason, "All TooltipText indices are missing (Data\\\\Local\\\\%s\\\\ItemTooltipText.bmd)", g_strSelectedML.c_str());
						}
					}
				}

				if (!tooltipOk)
				{
					gmAuditRecord(2, idx, s_gmAuditLogMissingTooltips, "Missing tooltip: item=%d reason=%s", idx, reason);
				}
			}
#endif // PACK_FILE_DECRYPT_H
		}

		s_gmAuditScanIndex = end;
		if (s_gmAuditScanIndex >= s_gmAuditScanTotal)
			s_gmAuditScanActive = false;
	}
}

void SEASON3B::CGFxEffectHandle::RenderButtons()
{
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
				selectedId = (int)i;
				selectedIndex = comboBoxItems[i].first;
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
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
