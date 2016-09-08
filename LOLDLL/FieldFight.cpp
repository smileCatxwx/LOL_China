#include "stdafx.h"
#include "FieldFight.h"
#include <MyTools/CLLog.h>
#include "Solider.h"
#include "ObjectExtend.h"
#include "Person.h"
#include "GameRes.h"
#include "Turret.h"
#include "EqumentServices.h"
#include "CreepServices.h"
#include "FightServices.h"
#include "SkillServices.h"
#include "HeroFight.h"

#define _SELF L"FieldFight.cpp"
CFieldFight::CFieldFight()
{
}

CFieldFight::~CFieldFight()
{
}

CFieldFight& CFieldFight::GetInstance()
{
	static CFieldFight FieldFight;
	return FieldFight;
}

VOID CFieldFight::StartShowTime() CONST
{
	PartyInitialization();
	
	while (GameRun)
	{
		if (CPerson::GetInstance().IsDead())
		{
			Log(LOG_LEVEL_NORMAL, L"�������!");
			Relive();
			continue;
		}
		
		// ��·ˮ���������ڵ�ʱ��, ���·���·��
		if (!CFightServices::GetInstance().ExistMiddleCrystalTurret())
			SetHeroPath();

		if (CCreepServices::GetInstance().IsRefreshDragonBuff())
		{
			Log(LOG_LEVEL_NORMAL, L"ˢ����!");
			KillDragon();
			continue;
		}

		// �Ƿ���Ҫ�س���
		if (CFightServices::GetInstance().IsHaveToBackToBase())
		{
			BackToBase();
			continue;
		}
		
		// ������
		if (DoTurret())
			continue;

		// ����Ӣ��
		if(DoHero())
			continue;

		// ����С��
		if(DoSolider())
			continue;

		// �ӷ�· �ֲ��ߵ��з�Ȫˮ!
		MoveToEnemySpring();
	}
	Log(LOG_LEVEL_NORMAL, L"�㶨!");
}


VOID CFieldFight::PartyInitialization() CONST
{
	// �ȴ���ʼ
	CFightServices::GetInstance().WaitToShowSolider();

	// �����ӽ�
	CPerson::GetInstance().LockView();

	// ��ʼ������
	CEqumentServices::GetInstance().BuyEqument();
	CEqumentServices::GetInstance().BuyMedicine();

	// ���÷�·
	SetHeroPath();
}

BOOL CFieldFight::SetHeroPath() CONST
{
	if (CFightServices::GetInstance().ExistMiddleCrystalTurret())
	{
		// set back default path when Exist Middle crystal turret
		CPerson::GetInstance().GetHeroPath() = CGameRes::GetInstance().GetDefaultPathTypeByHero(CPerson::GetInstance().GetHeroPro());
		return TRUE;
	}

	// ���˺����͸��׿��Ը�ռһ·����, ������ȥ��·
	switch (CPerson::GetInstance().GetHeroPro())
	{
	case em_Hero_Pro_Ashe:
		CPerson::GetInstance().GetHeroPath() = em_Path_Type::Path_Type_Top;
		break;
	case em_Hero_Pro_Garen:
		CPerson::GetInstance().GetHeroPath() = em_Path_Type::Path_Type_Buttom;
		break;
	default:
		CPerson::GetInstance().GetHeroPath() = em_Path_Type::Path_Type_Middle;
		break;
	}
	return TRUE;
}

BOOL CFieldFight::Relive() CONST
{
	while (GameRun && CPerson::GetInstance().IsDead() && LOLSleep(1000));

	RestInSpring();
	if (!CFightServices::GetInstance().ExistMiddleCrystalTurret())
		return TRUE;
	else if (CCreepServices::GetInstance().IsRefreshBlueBuff())
		return KillBuffMonster(CGameRes::GetInstance().GetBlueBuffPointByCamp(CPerson::GetInstance().GetCurrentCamp()), [] { return CCreepServices::GetInstance().SetRefreshBuffTick(CCreepServices::em_Creep_Buff_Type_Blue); });
	else if (CCreepServices::GetInstance().IsRefreshRedBuff())
		return KillBuffMonster(CGameRes::GetInstance().GetRedBuffPointByCamp(CPerson::GetInstance().GetCurrentCamp()), [] { return CCreepServices::GetInstance().SetRefreshBuffTick(CCreepServices::em_Creep_Buff_Type_Red); });

	return TRUE;
}

BOOL CFieldFight::KillBuffMonster(_In_ CONST Point& TarPoint, _In_ std::function<BOOL()> Worker) CONST
{
	while (GameRun && !CPerson::GetInstance().IsDead() && CPerson::GetInstance().GetDis(TarPoint) >= 2.0f)
	{
		LOLMove(TarPoint, 500);
		// Kill Around Enemy Hero
		while (CObjectExtend::GetInstance().GetAroundHumanTypeCount<CHero>(em_Human_Type_Hero, CPerson::GetInstance().GetEnemyCamp(), MAX_ATTACK_DIS) > 0 \
			&& GameRun && !CPerson::GetInstance().IsDead() && AttackHero());
	}

	if (!GameRun || CPerson::GetInstance().IsDead())
		return FALSE;

	CSolider Solider;
	while (CObjectExtend::GetInstance().GetLatelySoliderByCamp(em_Camp_Neutral, MAX_ATTACK_DIS, &Solider) && GameRun && !CPerson::GetInstance().IsDead())
	{
		CHeroFight::GetInstance().KillSolider(Solider);
		LOLSleep(100);
	}

	return Worker();
}

BOOL CFieldFight::KillDragon() CONST
{
	if (!BackToBase())
		return FALSE;

	auto p1 = CGameRes::GetInstance().GetDroganPoint();
	Point p2(p1.X + 300.0f, p1.Y + 300.0f, p1.Z);

	auto fnGather = []
	{
		return TRUE;
	};

	ULONGLONG ulTick = ::GetTickCount64();
	while (GameRun && !CPerson::GetInstance().IsDead() && !fnGather() && ::GetTickCount64() - ulTick <= 1 * 60 * 1000)
	{
		LOLMove(p1, 500);

		// Kill Around Enemy Hero
		while (CObjectExtend::GetInstance().GetAroundHumanTypeCount<CHero>(em_Human_Type_Hero, CPerson::GetInstance().GetEnemyCamp(), MAX_ATTACK_DIS) > 0 \
			&& GameRun && !CPerson::GetInstance().IsDead() && AttackHero());

		if (CPerson::GetInstance().GetDis(p1) < 2.0f)
		{
			// Around un exist Dragon!
			CSolider Dragon;
			if (!CObjectExtend::GetInstance().ExistSoliderByName(DRAGONNAME, em_Camp_Neutral, &Dragon) || Dragon.GetPercentHp() != 100)
				break;

			// need member more then 4 when self level <= 6
			if(CPerson::GetInstance().GetLevel() <= 6 && CObjectExtend::GetInstance().GetAroundHumanTypeCount<CHero>(em_Human_Type_Hero, CPerson::GetInstance().GetCurrentCamp(), MAX_ATTACK_DIS) >= 4)
				break;
			
			// need member more then 3 when self level > 6
			if (CPerson::GetInstance().GetLevel() > 6 && CObjectExtend::GetInstance().GetAroundHumanTypeCount<CHero>(em_Human_Type_Hero, CPerson::GetInstance().GetCurrentCamp(), MAX_ATTACK_DIS) >= 3)
				break;
		}

		LOLMove(p2, 500);
	}

	if (!GameRun || CPerson::GetInstance().IsDead())
		return FALSE;

	CSolider Solider;
	while (CObjectExtend::GetInstance().GetLatelySoliderByCamp(em_Camp_Neutral, MAX_ATTACK_DIS, &Solider) && GameRun && !CPerson::GetInstance().IsDead())
	{
		CHeroFight::GetInstance().KillSolider(Solider);
		LOLSleep(100);
	}

	return CCreepServices::GetInstance().SetRefreshBuffTick(CCreepServices::em_Creep_Buff_Type_Dragon);
}

BOOL CFieldFight::BackToBase() CONST
{
	// while(!IsBackTOBaseSucc)
	// move to lately turret
	// back to base
	while (!CFightServices::GetInstance().IsAtBase() && GameRun && !CPerson::GetInstance().IsDead())
	{
		if(!MoveToLatelyTurret())
			break;

		Log(LOG_LEVEL_NORMAL, L"ʹ�ûسǼ���!");
		CSkillServices::GetInstance().UseBuffSkill(SKILLNAME_RECALL1);
		CSkillServices::GetInstance().UseBuffSkill(SKILLNAME_RECALL2);

		// check exist recall buff!
		if (!CLPublic::TimeOut_By_Condition(12 * 1000, [] {
			return GameRun && !CPerson::GetInstance().IsDead() && \
			CObjectExtend::GetInstance().GetAroundHumanTypeCount<CSolider>(em_Human_Type_Solider, CPerson::GetInstance().GetEnemyCamp(), 10.0f) == 0 && \
			CObjectExtend::GetInstance().GetAroundHumanTypeCount<CHero>(em_Human_Type_Hero, CPerson::GetInstance().GetEnemyCamp(), 10.0f) == 0;
		}))
		{
			Log(LOG_LEVEL_NORMAL, L"�س����!");
			break;
		}
	}

	// reset
	return RestInSpring() && !CPerson::GetInstance().IsDead();
}

BOOL CFieldFight::MoveToEnemySpring() CONST
{
	auto pResPoint = CGameRes::GetInstance().GetNextMovePoint(CPerson::GetInstance().GetCurrentCamp(), CPerson::GetInstance().GetHeroPath(), CPerson::GetInstance().GetPoint());
	return pResPoint == nullptr ? FALSE : LOLMove(*pResPoint, 300);
}

BOOL CFieldFight::MoveToLatelyTurret() CONST
{
	static ULONGLONG ulRunFastTick = NULL;
	static ULONGLONG ulAddHpTick = NULL;

	while (GameRun && !CPerson::GetInstance().IsDead() && \
		(CObjectExtend::GetInstance().GetEnemyHeroCountByDis(15.0f) != NULL || \
			CObjectExtend::GetInstance().GetEnemySoliderCountByDis(15.0f) != NULL))
	{
		if (CObjectExtend::GetInstance().GetEnemyHeroCountByDis(10.0f) > 0 && CPerson::GetInstance().GetPercentHp() < 30)
		{
			if (::GetTickCount64() - ulRunFastTick >= SKILLTIME_SUMMONERHASTE)
			{
				Log(LOG_LEVEL_NORMAL, L"ʹ�ü���!");
				CSkillServices::GetInstance().UseItemSkill(SKILLNAME_SUMMONERHASTE);
				ulRunFastTick = ::GetTickCount64();
			}
			else if (::GetTickCount64() - ulAddHpTick >= SKILLTIME_SUMMONERHEAL)
			{
				Log(LOG_LEVEL_NORMAL, L"ʹ�ûش���!");
				CSkillServices::GetInstance().UseItemSkill(SKILLNAME_SUMMONERHEAL);
				ulAddHpTick = ::GetTickCount64();
			}
		}
		BackOff();
	}
	LOLSleep(1000);
	return CPerson::GetInstance().StopAction();
}

BOOL CFieldFight::RestInSpring() CONST
{
	while (GameRun && (CPerson::GetInstance().GetPercentHp() < 70 || CPerson::GetInstance().GetPercentMp() < 70) && LOLSleep(500));
	
	return CEqumentServices::GetInstance().BuyEqument();
}

BOOL CFieldFight::BackOff() CONST
{
	auto pResPoint = CGameRes::GetInstance().GetPreviouMovePoint(CPerson::GetInstance().GetCurrentCamp(), CPerson::GetInstance().GetHeroPath(), CPerson::GetInstance().GetPoint());
	if (pResPoint == nullptr)
	{
		Point PrintPoint = CPerson::GetInstance().GetPoint();
		LogMsgBox(LOG_LEVEL_EXCEPTION, L"pResPoint = nullptr, Camp=%X,Path=%X,Point=[%d,%d,%d]", CPerson::GetInstance().GetCurrentCamp(), CPerson::GetInstance().GetHeroPath(), \
			PrintPoint.GetX(), PrintPoint.GetY(), PrintPoint.GetZ());
		return FALSE;
	}
	return LOLMove(*pResPoint, 300);
}

BOOL CFieldFight::DoBackToBase() CONST
{
	if (!CFightServices::GetInstance().IsHaveToBackToBase())
		return FALSE;

	Log(LOG_LEVEL_NORMAL, L"��Ҫ�س�!");
	if (!BackToBase())
		return FALSE;

	return Relive();
}

BOOL CFieldFight::DoTurret() CONST
{
	// did have to dodge turret(dis || attack)
	if (CFightServices::GetInstance().IsDodgeTurret())
		return DodgeTurret();
	else if (CFightServices::GetInstance().IsAttackTurret())
		return AttackTurret();
	return FALSE;
}

BOOL CFieldFight::DodgeTurret() CONST
{
	while (GameRun && !CPerson::GetInstance().IsDead() && CObjectExtend::GetInstance().GetLatelyTurretByCamp(CPerson::GetInstance().GetEnemyCamp(), 10.0f + 5.0f, nullptr) && BackOff());
	
	static ULONGLONG ulTick = NULL;
	if (::GetTickCount64() - ulTick >= SKILLTIME_ITEMVOIDGATE && CObjectExtend::GetInstance().ExistEqumentById(EQUMENT_ID_������, nullptr))
		CSkillServices::GetInstance().UseUnDirectionalItemSkill(SKILLNAME_ITEMVOIDGATE);

	return CPerson::GetInstance().StopAction();
}

BOOL CFieldFight::AttackTurret() CONST
{
	CTurret Turret;
	if (!CObjectExtend::GetInstance().GetLatelyTurretByCamp(CPerson::GetInstance().GetEnemyCamp(), MAX_ATTACK_DIS + 2.0f, &Turret))
		return FALSE;

	if (Turret.GetDis() > CPerson::GetInstance().GetNormalAttackDis())
		return LOLMove(Turret.GetPoint(), 300);
	
	return Turret.Attack();
}

BOOL CFieldFight::DoSolider() CONST
{
	return TRUE;
}

BOOL CFieldFight::DodgeSolider() CONST
{
	while (GameRun && !CPerson::GetInstance().IsDead() && CFightServices::GetInstance().IsDogeSolider() && BackOff());
	return CPerson::GetInstance().StopAction();
}

BOOL CFieldFight::AttackSolider() CONST
{
	CSolider Solider;
	if (!CObjectExtend::GetInstance().GetLatelySoliderByCamp(CPerson::GetInstance().GetEnemyCamp(), MAX_ATTACK_DIS, &Solider))
		return FALSE;

	if (Solider.GetDis() > CPerson::GetInstance().GetNormalAttackDis())
		return LOLMove(Solider.GetPoint(), 300);;

	return CHeroFight::GetInstance().KillSolider(Solider);
}

BOOL CFieldFight::DoHero() CONST
{
	return TRUE;
}

BOOL CFieldFight::DoDgeHero() CONST
{
	while (GameRun && !CPerson::GetInstance().IsDead() && CObjectExtend::GetInstance().GetEnemyAttackSoliderCountByDis(10.0f) > 0 && BackOff());
	return CPerson::GetInstance().StopAction();
}

BOOL CFieldFight::AttackHero() CONST
{
	CHero Hero;
	if (!CObjectExtend::GetInstance().GetLatelyHeroByCamp(CPerson::GetInstance().GetEnemyCamp(), MAX_ATTACK_DIS, &Hero))
		return FALSE;

	return CHeroFight::GetInstance().KillHero(Hero);
}