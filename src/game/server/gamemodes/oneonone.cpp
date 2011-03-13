#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include "oneonone.h"
#include <game/server/sql/sql.h>
#include <base/vmath.h>

CGameControllerOneOnOne::CGameControllerOneOnOne(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "1on1";
	m_GameFlags = GAMEFLAG_TEAMS;

	CSql::Init(pGameServer);
	//dbg_msg("DBG Echo", "Storage: %s",Storage());
}

bool CGameControllerOneOnOne::CanJoinTeam(int Team, int User_id)
{
	if(!GameServer()->m_apPlayers[User_id]->IsLogedIn())
		return false;

	if(Team == -1 || (GameServer()->m_apPlayers[User_id] && GameServer()->m_apPlayers[User_id]->GetTeam() != -1))
		return true;

	int aNumplayers[2] = {0,0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != User_id)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= 0)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}
	
	return (aNumplayers[0] + aNumplayers[1]) < g_Config.m_SvMaxClients - g_Config.m_SvSpectatorSlots;
}

void CGameControllerOneOnOne::EndRound()
{
	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;

	CSql::SaveMatch();
	//(CServer*) Server()->DoSnapshot();
	//CServer::ConStopRecord(IConsole::IResult *pResult, Server());
}

void CGameControllerOneOnOne::StartRound()
{
	if(!nextmatches.empty())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameServer()->m_apPlayers[i])
				break;
			if(GameServer()->m_apPlayers[i]->GetTeam() >= 0)
				GameServer()->m_apPlayers[i]->SetTeam(-1);
		}
		for (it=nextmatches.begin(); it!=nextmatches.end(); it++)
		{
			if(it->accepted == 1)
			{
				it->player1->SetTeam(0);
				it->player2->SetTeam(1);
				nextmatches.erase(it);
				break;
			}
		}
	}

	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameServer()->m_World.m_Paused = false;
	m_aTeamscore[0] = 0;
	m_aTeamscore[1] = 0;
	m_ForceBalanced = false;

	//CConsole::CResult args;
	//args->AddArgument("bla");
	//CServer::ConRecord((IConsole::IResult) res, Server());

	dbg_msg("game","start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
}

int CGameControllerOneOnOne::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	
	
	if(Weapon != WEAPON_GAME)
	{
		// do team scoring
		if(pKiller == pVictim->GetPlayer() || pKiller->GetTeam() == pVictim->GetPlayer()->GetTeam())
			m_aTeamscore[pKiller->GetTeam()&1]--; // klant arschel
		else
			m_aTeamscore[pKiller->GetTeam()&1]++; // good shit
	}
		
	return 0;
}

void CGameControllerOneOnOne::Tick()
{
	DoTeamScoreWincheck();
	IGameController::Tick();
}

void CGameControllerOneOnOne::VoteChallenge()
{
	GameServer()->Test();
}
void CGameControllerOneOnOne::Challenge(CPlayer *p, CPlayer *p2)
{
	if(p->GetTeam() != -1)
	{
		GameServer()->SendChatTarget(p->GetCID(), "Only specs can challenge other players.");
		return;
	}
	if(p2->GetTeam() != -1)
	{
		GameServer()->SendChatTarget(p->GetCID(), "This player is playing atm.");
		GameServer()->SendChatTarget(p->GetCID(), "Please choose another opponent or wait for finish of current game.");
		return;
	}

	if(!nextmatches.empty())
	{
		for (it=nextmatches.begin(); it!=nextmatches.end(); it++)
		{
			if(it->player1 == p || it->player2 == p)
			{
				GameServer()->SendChatTarget(p->GetCID(), "You are already in queue.");
				return;
			}
			if(it->player1 == p2 || it->player2 == p2)
			{
				GameServer()->SendChatTarget(p->GetCID(), "Your opponent is already in queue.");
				return;
			}
		}
	}
	struct match newmatch = {p, p2, 0};
	nextmatches.push_back(newmatch);

	char Buf[512];
	str_format(Buf, sizeof(Buf),  "%s wants to play versus you", Server()->ClientName(p->GetCID()));
	GameServer()->SendChatTarget(p2->GetCID(), Buf);
	GameServer()->SendChatTarget(p2->GetCID(), "/y to accept; /n to decline.");

	GameServer()->SendChatTarget(p->GetCID(), "Your opponent is asked. Wait for his answer.");
}

void CGameControllerOneOnOne::accept(CPlayer *p)
{
	char Buf[512];
	if(!nextmatches.empty())
	{
		for (it=nextmatches.begin(); it!=nextmatches.end(); it++)
		{
			if(it->player2 == p)
			{
				if(it->accepted == 0)
				{
					it->accepted = 1;
					str_format(Buf, sizeof(Buf),  "%s accepted.", Server()->ClientName(p->GetCID()));
					GameServer()->SendChatTarget(it->player1->GetCID(), Buf);
				}
				else
					GameServer()->SendChatTarget(p->GetCID(), "You already accepted.");
				return;
			}
		}
	}
	GameServer()->SendChatTarget(p->GetCID(), "Nobody challenged you. lol.");
}

void CGameControllerOneOnOne::decline(CPlayer *p)
{
	char Buf[512];
	if(!nextmatches.empty())
	{
		for (it=nextmatches.begin(); it!=nextmatches.end(); it++)
		{
			if(it->player2 == p)
			{
				if(it->accepted == 0)
					nextmatches.erase(it);
				else
					GameServer()->SendChatTarget(p->GetCID(), "Too late, you already accepted haha");
				return;
			}
		}
	}
	GameServer()->SendChatTarget(p->GetCID(), "Nobody challenged you. lol.");
}

void CGameControllerOneOnOne::remove(int ClientId)
{
	if(!nextmatches.empty())
	{
		for (it=nextmatches.begin(); it!=nextmatches.end(); it++)
		{
			if(GameServer()->m_apPlayers[ClientId] == it->player1 || GameServer()->m_apPlayers[ClientId] == it->player2)
			{
				nextmatches.erase(it);
				break;
			}
		}
	}
}

/**
 * Calculate the new EloPoints (Only for 2 Players)
 * @param int* Scores[2]
 * @param int* EloPoints[2]
 */
void CGameControllerOneOnOne::EloPoints(int* Scores, int* EloPoints, int* NewEloPoints)
{
	int C1 = 50, C2 = 400;
	double ScorePercents[2], Probability[2];

	//Raise negative points
	while(Scores[0] < 0 || Scores[1] < 0){
		Scores[0]++;
		Scores[1]++;
	}
	dbg_msg("ELOPOINTS", "Score1: %i",Scores[0]);
	dbg_msg("ELOPOINTS", "Score2: %i",Scores[1]);

	ScorePercents[0] = (double) Scores[0] / (Scores[0] + Scores[1]);
	ScorePercents[1] = (double) Scores[1] / double(Scores[0] + Scores[1]);
	dbg_msg("ELOPOINTS", "Percent1: %lf",ScorePercents[0]);
	dbg_msg("ELOPOINTS", "Percent2: %lf",ScorePercents[1]);

	//Win probability
	Probability[0] = (double) 1 / (1 + pow(10,(double) ((-1)*(EloPoints[0]-EloPoints[1])/C2)));
	Probability[1] = (double) 1 / (1 + pow(10,(double) ((-1)*(EloPoints[1]-EloPoints[0])/C2)));
	dbg_msg("ELOPOINTS", "Probability1: %lf",Probability[0]);
	dbg_msg("ELOPOINTS", "Probability2: %lf",Probability[1]);

	//New EloPoints Rnew = Rold + C1 * ( W - E )
	NewEloPoints[0] = EloPoints[0] + (int) (round(C1 * ( ScorePercents[0] - Probability[0] )));
	NewEloPoints[1] = EloPoints[1] + (int) (round(C1 * ( ScorePercents[1] - Probability[1] )));
	dbg_msg("ELOPOINTS", "NewEloPoints1: %i",NewEloPoints[0]);
	dbg_msg("ELOPOINTS", "NewEloPoints2: %i",NewEloPoints[1]);
}
