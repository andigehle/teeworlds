#include "gamecontext.h"
#include <engine/shared/config.h>
#include <game/version.h>
#include <stdio.h>
#include "gamemodes/oneonone.h"
#include <game/server/sql/sql.h>


int CGameContext::checkMessage(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	const int commandscount = 10;

	// "/" and wrongcmd have to be at the end of their arrays
	char* commands[commandscount] = {"/register", "/rank", "/top5", "/login ", "/vs ", "/y", "/n", "/help", "/info", "/"};
	void (CGameContext::*functions[commandscount])(int, CPlayer*, CNetMsg_Cl_Say*) = {&CGameContext::regpage, &CGameContext::rank, &CGameContext::top5, &CGameContext::login, &CGameContext::challenge,&CGameContext::accept, &CGameContext::decline, &CGameContext::help, &CGameContext::info, &CGameContext::wrongcmd};
	//

	for(int i = 0; i < commandscount; i++)
	{
		if(!str_comp_num(pMsg->m_pMessage, commands[i], strlen(commands[i])))
		{
			(this->*functions[i])(ClientId, p, pMsg);
			return 0;
		}
	}
	return 1;
}

void CGameContext::rank(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	CGameControllerOneOnOne *controller = (CGameControllerOneOnOne*) m_pController;
	controller->VoteChallenge();//p, m_apPlayers[i]);
	//CSql::getRank(ClientId);
}

void CGameContext::top5(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	int top;
	char Buf[512];
	if(sscanf(pMsg->m_pMessage, "/top5 %i", &top) != 1)
	{
		SendChatTarget(ClientId, "top5");
		//SQL.top5(1);
	}
	else
	{
		str_format(Buf, sizeof(Buf),  "top5 %i", top);
		SendChatTarget(ClientId, Buf);
		//SQL.top5(top);
	}
}

void CGameContext::login(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	char name[512];
	char pass[512];
	if(sscanf(pMsg->m_pMessage, "/login %s %s", &name, &pass) != 2)
	{
		SendChatTarget(ClientId, "Please stick to the given structure:");
		SendChatTarget(ClientId, "/login <user> <pass>");
		return;
	}
	SendChatTarget(ClientId, "Login try.....");
	dbg_msg("Thread", "start login");
	CSql::Login(ClientId, name, pass);
}

void CGameContext::challenge(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	// Check if the user is logged in
	if(p->m_UserID == 0)
	{
		SendChatTarget(ClientId, "You are not logged in");
		return;
	}

	// Check right using of the command
	char name[512];
	if(sscanf(pMsg->m_pMessage, "/vs %s", &name) != 1)
	{
		SendChatTarget(ClientId, "use \"/vs foobar\" to challenge a user with nickname foobar");
		//SendChatTarget(ClientId, "You can also write a part of nickname. (at least 3 first letters)");
		return;
	}

	// Search opponent
	int found = 0;
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(!m_apPlayers[i])
			break;
		if(!str_comp(Server()->ClientName(i), name))
		{
			found = 1;
			if(i == ClientId)
			{
				SendChatTarget(ClientId, "You can't challenge yourself.");
				break;
			}
			if(m_apPlayers[i]->m_UserID == 0)
			{
				SendChatTarget(ClientId, "This player is not logged in.");
				break;
			}
			CGameControllerOneOnOne *controller = (CGameControllerOneOnOne*) m_pController;
			controller->Challenge(p, m_apPlayers[i]);
			break;
		}
	}
	if(!found)
		SendChatTarget(ClientId, "Player not found");
}

void CGameContext::accept(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	CGameControllerOneOnOne *controller = (CGameControllerOneOnOne*) m_pController;
	controller->accept(p);
}

void CGameContext::decline(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	CGameControllerOneOnOne *controller = (CGameControllerOneOnOne*) m_pController;
	controller->decline(p);
}

void CGameContext::help(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	SendChatTarget(ClientId, "help");
}

void CGameContext::info(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	SendChatTarget(ClientId, "1on1 Mod made by Kugel and Dimidrol");
	SendChatTarget(ClientId, "Using MySQL Connection Mod by SushiTee");
	SendChatTarget(ClientId, "Have fun playing :)");
}

void CGameContext::regpage(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	SendChatTarget(ClientId, "You can register at teedb.info");
}

void CGameContext::wrongcmd(int ClientId, CPlayer *p, CNetMsg_Cl_Say *pMsg)
{
	SendChatTarget(ClientId, "That command does not exist");
}

