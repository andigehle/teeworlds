/**
 * Sql Class
 * Connected to a MySql database
 * 
 * @author kugel
 * @version 1.0
 * Based on CSqlScore class by Sushi 
 **/

#include <string.h>

#include <engine/shared/config.h>
#include "sql.h"
#include "../gamemodes/oneonone.h"
#include "../gamecontext.h"
//#include <exception>

static LOCK gs_SqlLock = 0;
static int s_pMapID = 0;

CSql::CSql()
{
	m_pDatabase = g_Config.m_SvSqlDatabase;
	//m_pPrefix = g_Config.m_SvSqlPrefix;
	m_pUser = g_Config.m_SvSqlUser;
	m_pPass = g_Config.m_SvSqlPw;
	m_pIp = g_Config.m_SvSqlIp;
	m_pPort = (char*) g_Config.m_SvSqlPort;

	m_pDriver = get_driver_instance();
	gs_SqlLock = lock_create();
}

CSql::~CSql()
{
	lock_wait(gs_SqlLock);
	lock_release(gs_SqlLock);
}

CSql& CSql::getInstance()
{
	static CSql s_instance;
	return s_instance;
}

void CSql::Init(CGameContext* server)
{
	getInstance().m_pGameServer = server;
	//m_pServer = Kernel()->RequestInterface<IServer>();
	//IGameServer *pserver = Kernel()->RequestInterface<IGameServer>();
	//getInstance().m_pGameServer = (CGameContext*) pserver;
	getInstance().m_pServer = server->Server();
	//Get MapID
	CSql::startThread(g_Config.m_SvMap, &getMapIDThread);
}

bool CSql::Connect()
{
	char aBuf[256];

	str_format(aBuf, sizeof(aBuf), "tcp://%s:%d", m_pIp, m_pPort);
	m_pConnection = m_pDriver->connect(aBuf, m_pUser, m_pPass);
	m_pConnection->setSchema(m_pDatabase);

	dbg_msg("SQL", "SQL connection established");
	return true;
}

void CSql::Disconnect()
{
	delete m_pConnection;
	dbg_msg("SQL", "SQL connection disconnected");
}

void CSql::startThread(void* pData, void (*pThread)(void*)){
	CSqlThreadData *ThreadTmp = new CSqlThreadData();
	ThreadTmp->m_pData = pData;
	ThreadTmp->m_pThread = pThread;

	void *pSaveThread = thread_create(saveThread, ThreadTmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)pSaveThread);
#endif
}

/**
 * Starts Threads with Exception Handling
 * and FunctionPointers
 */
void CSql::saveThread(void *Tmp)
{
	lock_wait(gs_SqlLock);

	try
	{
		CSqlThreadData *pTmp = (CSqlThreadData *)Tmp;

		if(getInstance().Connect()){
			pTmp->m_pThread(pTmp->m_pData);
			getInstance().Disconnect();
		}
		else
		{
			dbg_msg("SQ", "Connection failed");
			getInstance().m_pGameServer->SendChatTarget(-1, "***Database offline, please try it again later***");
		}

		delete pTmp;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("Exception", "%s", e.what());
		getInstance().m_pGameServer->SendChatTarget(-1, "***WARNING: SQL Exception thrown***");
	}
	catch (...)
	{
		dbg_msg("Exception", "Thrown in Sql class");
	}

	lock_release(gs_SqlLock);
}

void CSql::getMapIDThread(void *pTmp)
{
	char *pMap = (char *)pTmp;
	char aQuery[512];

	str_format(aQuery, sizeof(aQuery),"SELECT `id` FROM map WHERE name = ?;");
	getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
	getInstance().m_pPrepareStatement->setString(1, pMap);
	getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
	if(getInstance().m_pResults->rowsCount())
	{
		getInstance().m_pResults->first();
		s_pMapID = (int) getInstance().m_pResults->getInt("id");
	}
	else
	{
		getInstance().m_pGameServer->SendChatTarget(-1, "Map not linked in DB.");
	}

	delete getInstance().m_pResults;
	delete getInstance().m_pPrepareStatement;

	delete pMap;
}

void CSql::Login(int ClientID, const char* pUsername, const char* pPassword)
{
	if(getInstance().m_pGameServer->m_apPlayers[ClientID]->IsLogedIn()){
		getInstance().m_pGameServer->SendChatTarget(ClientID, "Already loged in!");
		return;
	}

	CSqlLoginData *Tmp = new CSqlLoginData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aUsername, pUsername, sizeof(Tmp->m_aUsername));
	str_copy(Tmp->m_aPassword, pPassword, sizeof(Tmp->m_aPassword));
	//Server()->GetClientIP(ClientID, Tmp->m_aIP, sizeof(Tmp->m_aIP));

	CSql::startThread(Tmp, &LoginThread);
}

void CSql::LoginThread(void *pTmp)
{
	CSqlLoginData *pData = (CSqlLoginData *)pTmp;
	int m_UserID = 0;
	char aQuery[512];

	//Get UserID
	str_format(aQuery, sizeof(aQuery),"SELECT `id` FROM `user` WHERE `name`= ? AND `password`=SHA1(?) LIMIT 1;");
	getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
	getInstance().m_pPrepareStatement->setString(1, pData->m_aUsername);
	getInstance().m_pPrepareStatement->setString(2, pData->m_aPassword);
	getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
	if(getInstance().m_pResults->rowsCount())
	{
		getInstance().m_pResults->first();
		m_UserID = (int) getInstance().m_pResults->getInt("id");
	}
	else
	{
		getInstance().m_pGameServer->SendChatTarget(pData->m_ClientID, "Login failed!");
	}
	delete getInstance().m_pResults;
	delete getInstance().m_pPrepareStatement;

	//Check for use Account only ones
	if(m_UserID > 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(getInstance().m_pGameServer->m_apPlayers[i] && i != pData->m_ClientID)
			{
				if(getInstance().m_pGameServer->m_apPlayers[i]->m_UserID == m_UserID){
					getInstance().m_pGameServer->SendChatTarget(pData->m_ClientID, "Account already in use!");
					m_UserID = 0;
				}
			}
		}
	}

	//Set UserID, Name and lets join
	if(m_UserID > 0)
	{
		getInstance().m_pGameServer->m_apPlayers[pData->m_ClientID]->m_UserID = m_UserID;
		getInstance().m_pServer->SetClientName(pData->m_ClientID, pData->m_aUsername);
		getInstance().m_pGameServer->SendChatTarget(pData->m_ClientID, "You are now loged in!");
		dbg_msg("SQL", "Player ID set!");
		if(getInstance().m_pGameServer->m_pController->CanJoinTeam(0,pData->m_ClientID)){
			if(getInstance().m_pGameServer->m_pController->CanChangeTeam(getInstance().m_pGameServer->m_apPlayers[pData->m_ClientID], 0)){
				getInstance().m_pGameServer->m_apPlayers[pData->m_ClientID]->SetTeam(0);
			}else{
				getInstance().m_pGameServer->m_apPlayers[pData->m_ClientID]->SetTeam(1);
			}
		}
	}

	delete pData;
}

void CSql::SaveMatch()
{
	//Save Data before Round restart while thread is running!
	CSqlMatchData *Tmp = new CSqlMatchData();

	int j = 0;
	//Tmp->m_aPlayers = new CPlayer [g_Config.m_SvMaxClients - g_Config.m_SvSpectatorSlots];
	for(int i = 0; i < MAX_CLIENTS && j < 2; i++)
	{
		if(getInstance().m_pGameServer->m_apPlayers[i])
		{
			if(getInstance().m_pGameServer->m_apPlayers[i]->GetTeam() >= 0){
				Tmp->m_aClientIDs[j] = getInstance().m_pGameServer->m_apPlayers[i]->GetCID();
				Tmp->m_aUserIDs[j] = getInstance().m_pGameServer->m_apPlayers[i]->m_UserID;
				if(getInstance().m_pGameServer->m_apPlayers[i]->m_Score == NULL)
					Tmp->m_aScores[j] = 0;
				else
					Tmp->m_aScores[j] = getInstance().m_pGameServer->m_apPlayers[i]->m_Score;
				j++;
			}
		}
	}
	if(Tmp->m_aScores[0] == 0 && Tmp->m_aScores[1] == 0){
		getInstance().m_pGameServer->SendChatTarget(Tmp->m_aClientIDs[0], "0 : 0 Matches won't saved.");
		getInstance().m_pGameServer->SendChatTarget(Tmp->m_aClientIDs[1], "0 : 0 Matches won't saved.");
		return;
	}

	//TODO: Get Server IP and transform it to long
	Tmp->m_aServerIP = 2130706433; //127.0.0.1
	Tmp->m_aServerPort = 8303;

	CSql::startThread(Tmp, &SaveMatchThread);
}

void CSql::SaveMatchThread(void *pTmp)
{
	CSqlMatchData *pData = (CSqlMatchData *)pTmp;
	char aQuery[512], aStr[512];
	char m_pTime[2][32];
	int m_InserID;

	//Save Match
	str_format(aQuery, sizeof(aQuery),"INSERT INTO matches (`map_id`, `mod`, `server`, `port`, `update`, `create`) VALUES ( ?, ?, ?, ?, NOW(), NOW());");
	getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
	getInstance().m_pPrepareStatement->setInt(1, s_pMapID);
	getInstance().m_pPrepareStatement->setString(2, getInstance().m_pGameServer->m_pController->m_pGameType);
	getInstance().m_pPrepareStatement->setInt(3, pData->m_aServerIP);
	getInstance().m_pPrepareStatement->setInt(4, pData->m_aServerPort);
	getInstance().m_pPrepareStatement->execute();
	delete getInstance().m_pPrepareStatement;

	//Get Last Insert ID (Match)
	str_format(aQuery, sizeof(aQuery),"SELECT LAST_INSERT_ID() AS match_id FROM `matches`;");
	getInstance().m_pStatement = getInstance().m_pConnection->createStatement();
	getInstance().m_pResults = getInstance().m_pStatement->executeQuery(aQuery);
	getInstance().m_pResults->first();
	m_InserID = (int) getInstance().m_pResults->getInt("match_id");
	delete getInstance().m_pResults;
	delete getInstance().m_pStatement;

	//TODO: erweitern mit statistik shoots/treffer/...
	//Save Player from Match
	str_format(aQuery, sizeof(aQuery),"INSERT INTO players (`match_id`, `user_id`, `score`, `update`, `create`) VALUES ( ?, ?, ?, NOW(), NOW());");
	for(int i=0; i < 2; i++){
		dbg_msg("SQL", "Set Player Data");
		getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
		getInstance().m_pPrepareStatement->setInt(1, m_InserID);
		getInstance().m_pPrepareStatement->setInt(2, pData->m_aUserIDs[i]);
		getInstance().m_pPrepareStatement->setInt(3, pData->m_aScores[i]);
		getInstance().m_pPrepareStatement->execute();

		delete getInstance().m_pPrepareStatement;
	}

	//Get Rank
	dbg_msg("SQL", "Get Rank");
	CGameControllerOneOnOne *m_pController = (CGameControllerOneOnOne*) getInstance().m_pGameServer->m_pController;
	int m_aNewEloPoints[2], m_aOldPlace[2], m_aNewPlace[2];
	int m_aEloPoints[2] = {1000, 1000};

	//Get OldEloPoints
	dbg_msg("SQL", "Get Old Elo");
	str_format(aQuery, sizeof(aQuery),"SELECT `score`, SUBSTR( `update` , 1, 11 ) AS m_pDate, SUBSTR( `update` , 12, 8 ) AS m_pTime FROM `rank_1on1` WHERE `user_id`= ? LIMIT 1;");
	for(int i=0; i < 2; i++){
		getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
		getInstance().m_pPrepareStatement->setInt(1, pData->m_aUserIDs[i]);
		getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
		if(getInstance().m_pResults->rowsCount())
		{
			getInstance().m_pResults->first();
			m_aEloPoints[i] = (int) getInstance().m_pResults->getInt("score");
			str_format(m_pTime[i], sizeof(m_pTime[i]), "%s%s",getInstance().m_pResults->getString("m_pDate").c_str(), getInstance().m_pResults->getString("m_pTime").c_str());

		}
		delete getInstance().m_pResults;
		delete getInstance().m_pPrepareStatement;

		//Get Old Place
		if(m_aEloPoints[0] == m_aEloPoints[1] && m_pTime[i][0] != '\0')
		{
			str_format(aStr, sizeof(aStr),"SELECT `oldPlace` FROM `rank_1on1` WHERE `user_id` = ? LIMIT 1;");
			getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aStr);
			getInstance().m_pPrepareStatement->setInt(1, pData->m_aUserIDs[i]);
			getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
			if(getInstance().m_pResults->rowsCount())
			{
				getInstance().m_pResults->first();
				//If OldEloPoint equal get old places
				m_aOldPlace[i] = (int) getInstance().m_pResults->getInt("oldPlace");
			}
		}
		else
		{
			if(m_pTime[i][0] == '\0')
			{
				str_format(aStr, sizeof(aStr),"SELECT Count(*) AS OldPlaces FROM `rank_1on1` WHERE `score` > ? OR `score` = ? AND `update` < NOW() LIMIT 1;");
				getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aStr);
				getInstance().m_pPrepareStatement->setInt(1, m_aEloPoints[i]);
				getInstance().m_pPrepareStatement->setInt(2, m_aEloPoints[i]);
			}
			else
			{
				str_format(aStr, sizeof(aStr),"SELECT Count(*) AS OldPlaces FROM `rank_1on1` WHERE `score` > ? OR `score` = ? AND `update` < ? LIMIT 1;");
				getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aStr);
				getInstance().m_pPrepareStatement->setInt(1, m_aEloPoints[i]);
				getInstance().m_pPrepareStatement->setInt(2, m_aEloPoints[i]);
				getInstance().m_pPrepareStatement->setString(3, m_pTime[i]);
			}
			getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
			if(getInstance().m_pResults->rowsCount())
			{
				getInstance().m_pResults->first();
				//Player before you + you
				m_aOldPlace[i] = 1 + (int) getInstance().m_pResults->getInt("OldPlaces");
			}
		}

		delete getInstance().m_pResults;
		delete getInstance().m_pPrepareStatement;
	}

	//Get NewEloPoints
	dbg_msg("SQL", "Get New Elo");
	m_pController->EloPoints(pData->m_aScores, m_aEloPoints, m_aNewEloPoints);

	//Save New Points
	dbg_msg("SQL", "Save New Rank");

	str_format(aStr, sizeof(aStr),"%s %i : %i %s - Match saved!", getInstance().m_pServer->ClientName(pData->m_aClientIDs[0]), pData->m_aScores[0],	pData->m_aScores[1], getInstance().m_pServer->ClientName(pData->m_aClientIDs[1]));
	for(int i=0; i < 2; i++){
		if(m_pTime[i][0] == '\0'){
			str_format(aQuery, sizeof(aQuery),"INSERT INTO rank_1on1 (`user_id`, `score`, `oldPlace`, `update`, `create`) VALUES ( ?, ?, ?, NOW(), NOW());");
		}else{
			str_format(aQuery, sizeof(aQuery),"UPDATE rank_1on1 SET `score` = ?, `oldPlace` = ?, `update` = NOW() WHERE `user_id` = ?;");
		}
		getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
		if(m_pTime[i][0] == '\0'){
			getInstance().m_pPrepareStatement->setInt(1, pData->m_aUserIDs[i]);
			getInstance().m_pPrepareStatement->setInt(2, m_aNewEloPoints[i]);
			getInstance().m_pPrepareStatement->setInt(3, m_aOldPlace[i]);
		}else{
			getInstance().m_pPrepareStatement->setInt(1, m_aNewEloPoints[i]);
			getInstance().m_pPrepareStatement->setInt(2, m_aOldPlace[i]);
			getInstance().m_pPrepareStatement->setInt(3, pData->m_aUserIDs[i]);
		}
		getInstance().m_pPrepareStatement->execute();
		delete getInstance().m_pPrepareStatement;

		getInstance().m_pGameServer->SendChatTarget(pData->m_aClientIDs[i], aStr);
	}

	delete pData;
}


void CSql::getRank(int ClientID)
{
	if(!getInstance().m_pGameServer->m_apPlayers[ClientID]->IsLogedIn()){
		getInstance().m_pGameServer->SendChatTarget(ClientID, "You need to login!");
		return;
	}

	CSqlRankData *Tmp = new CSqlRankData();
	Tmp->m_pClientID = ClientID;

	CSql::startThread(Tmp, &getRankThread);
}

void CSql::getRankThread(void *pTmp)
{
	CSqlRankData *m_pData = (CSqlRankData *)pTmp;
	char aQuery[512], m_pTime[32];
	int m_pScore = 1000, m_pPlace = 0;
	int ClientID = m_pData->m_pClientID;

	//Get Score
	str_format(aQuery, sizeof(aQuery),"SELECT `score`, SUBSTR( `update` , 1, 11 ) AS m_pDate, SUBSTR( `update` , 12, 8 ) AS m_pTime FROM `rank_1on1` WHERE `user_id`= ? LIMIT 1;");
	getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
	getInstance().m_pPrepareStatement->setInt(1, getInstance().m_pGameServer->m_apPlayers[ClientID]->m_UserID);
	getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
	if(getInstance().m_pResults->rowsCount())
	{
		getInstance().m_pResults->first();
		m_pScore = (int) getInstance().m_pResults->getInt("score");
		str_format(m_pTime, sizeof(m_pTime), "%s%s",getInstance().m_pResults->getString("m_pDate").c_str(), getInstance().m_pResults->getString("m_pTime").c_str());
	}
	delete getInstance().m_pResults;
	delete getInstance().m_pPrepareStatement;

	//Get Place
	if(m_pTime[0] == '/0'){
		//When not ranked
		str_format(aQuery, sizeof(aQuery),"SELECT Count(*) AS Place FROM `rank_1on1` WHERE `score` > ? OR `score` = ? AND `update` < NOW() LIMIT 1;");
		getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
		getInstance().m_pPrepareStatement->setInt(1, m_pScore);
		getInstance().m_pPrepareStatement->setInt(2, m_pScore);
	}
	else
	{
		//When ranked
		str_format(aQuery, sizeof(aQuery),"SELECT Count(*) AS Place FROM `rank_1on1` WHERE `score` > ? OR `score` = ? AND `update` < ? LIMIT 1;");
		getInstance().m_pPrepareStatement = getInstance().m_pConnection->prepareStatement(aQuery);
		getInstance().m_pPrepareStatement->setInt(1, m_pScore);
		getInstance().m_pPrepareStatement->setInt(2, m_pScore);
		getInstance().m_pPrepareStatement->setString(3, m_pTime);
	}
	getInstance().m_pResults = getInstance().m_pPrepareStatement->executeQuery();
	if(getInstance().m_pResults->rowsCount())
	{
		getInstance().m_pResults->first();
		m_pPlace = 1 + (int) getInstance().m_pResults->getInt("Place");
	}
	delete getInstance().m_pResults;
	delete getInstance().m_pPrepareStatement;

	//Output Rank
	str_format(aQuery, sizeof(aQuery),"%i. %s (%i P.)", m_pPlace, getInstance().m_pServer->ClientName(ClientID), m_pScore);
	getInstance().m_pGameServer->SendChatTarget(ClientID, aQuery);

	delete m_pData;
}

// anti SQL injection ~ useless when prepare statements
/*
void CSql::ClearString(char *pString)
{
	// replace ' ' ' with ' \' ' and remove '\'
	for(int i = 0; i < str_length(pString); i++)
	{
		// replace '-' with '_'
		if(pString[i] == '-')
			pString[i] = '_';

		if(pString[i] == '\'')
		{
			// count \ before the '
			int SlashCount = 0;
			for(int j = i-1; j >= 0; j--)
			{
				if(pString[i] != '\\')
					break;

				SlashCount++;
			}

			if(SlashCount % 2 == 0)
			{
				for(int j = str_length(pString)-1; j > i; j--)
				{
					pString[j] = pString[j-1];
				}
				pString[i] = '\\';
				i++;
			}
		}
	}

	// and remove spaces and \ at the end xD
	for(int i = str_length(pString)-1; i >= 0; i--)
	{
		if(pString[i] == ' ' || pString[i] == '\\')
			pString[i] = '\0';
		else
			break;
	}
}
*/
