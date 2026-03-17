// w_PetActionRound.cpp: implementation of the PetActionRound class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "w_PetActionRound.h"
#include "ZzzAI.h"
#include "ZzzEffect.h"

PetActionRoundPtr PetActionRound::Make()
{
	PetActionRoundPtr petActionRound(new PetActionRound);
	return petActionRound;
}

PetActionRound::PetActionRound()
{

}

PetActionRound::~PetActionRound()
{

}

bool PetActionRound::Release(OBJECT* obj, CHARACTER* Owner)
{
	return false;
}

bool PetActionRound::Model(OBJECT* obj, CHARACTER* Owner, int targetKey, double tick, bool bForceRender)
{
	return false;
}

bool PetActionRound::Move(OBJECT* obj, CHARACTER* Owner, int targetKey, double tick, bool bForceRender)
{
	vec3_t  TargetPosition;
	float fRadWidth = ((2 * 3.14f) / 5000.0f) * fmodf(tick , 5000);
	float fRadHeight = ((2 * 3.14f) / 1000.0f) * fmodf(tick, 1000);

	VectorCopy(obj->Owner->Position, TargetPosition);
	VectorCopy(obj->Owner->HeadAngle, obj->HeadAngle);

	obj->Position[0] = TargetPosition[0] + (sinf(fRadWidth) * 150.0f);
	obj->Position[1] = TargetPosition[1] + (cosf(fRadWidth) * 150.0f);
	obj->Position[2] = TargetPosition[2] + 100 + (sinf(fRadHeight) * 30.0f);

	float Angle = CreateAngle(obj->Position[0], obj->Position[1], TargetPosition[0], TargetPosition[1]);

	obj->Angle[2] = TurnAngle2(obj->Angle[2], Angle + 270, 20.0f);

	return TRUE;
}

bool PetActionRound::Effect(OBJECT* obj, CHARACTER* Owner, int targetKey, double tick, bool bForceRender)
{
	if (NULL == obj || NULL == Owner) return FALSE;

	BMD* b = gmClientModels->GetModel(obj->Type);
	if (NULL == b) return FALSE;

	vec3_t Position, vRelativePos, Light;
	float fRad = ((3.14f / 2500.0f) * fmodf(tick, 2500));
	float fLumi = sinf(fRad) * 0.2f + 0.8f;

	VectorCopy(obj->Position, b->BodyOrigin);
	Vector(0.f, 0.f, 0.f, vRelativePos);

	b->Animation(BoneTransform, obj->AnimationFrame, obj->PriorAnimationFrame, obj->PriorAction, obj->Angle, obj->HeadAngle);

	b->TransformPosition(BoneTransform[1], vRelativePos, Position, false);
	Vector(0.7f * fLumi, 0.4f * fLumi, 1.0f * fLumi, Light);
	CreateSprite(BITMAP_LIGHTMARKS_FOREIGN, Position, 1.0f, Light, obj);
	CreateSprite(BITMAP_FLARE, Position, 0.5f, Light, obj);

	b->TransformPosition(BoneTransform[2], vRelativePos, Position, false);
	Vector(0.5f * fLumi, 0.6f * fLumi, 1.0f * fLumi, Light);
	CreateSprite(BITMAP_LIGHT, Position, 1.2f, Light, obj);

	VectorCopy(obj->Position, Position);
	Position[2] += 20.0f;
	Vector(0.6f * fLumi, 0.6f * fLumi, 1.0f * fLumi, Light);
	CreateParticleSync(BITMAP_SMOKE, Position, obj->Angle, Light, 67, 0.8f);

	return TRUE;
}
