#ifndef GAME_SERVER_SQL_H
#define GAME_SERVER_SQL_H
#include "../gamecontext.h"

//#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

class CSql : public IInterface
{
	private:
		CGameContext* m_pGameServer;
		IServer *m_pServer;

		sql::Driver *m_pDriver;
		sql::Connection *m_pConnection;
		sql::PreparedStatement *m_pPrepareStatement;
		sql::Statement *m_pStatement;
		sql::ResultSet *m_pResults;

		//Database data
		const char* m_pDatabase;
		const char* m_pPrefix;
		const char* m_pUser;
		const char* m_pPass;
		const char* m_pIp;
		const char* m_pPort;

		const char* m_pMod;

		CSql();
		~CSql();
		CSql(const CSql&);             // intentionally undefined
		CSql& operator=(const CSql &); // intentionally undefined

		static void getMapIDThread(void *pTmp);
		static void LoginThread(void *pData);
		static void SaveMatchThread(void *pTmp);
		static void getRankThread(void *pTmp);

		bool Connect();
		void Disconnect();
		static void startThread(void* pData, void (*pThread)(void*));
		static void saveThread(void *Tmp);
		static void ClearString(char *pString);

	public:
		static CSql& getInstance();
		static void Init(CGameContext* server);

		static void Login(int ClientID, const char* pUsername, const char* pPassword);
		static void SaveMatch();
		static void getRank(int ClientID);
};

struct CSqlThreadData
{
	void (*m_pThread)(void*);
	void* m_pData;
};

struct CSqlLoginData
{
	int m_ClientID;
	char m_aUsername[255];
	char m_aPassword[255];
};

struct CSqlMatchData
{
	int m_aScores[2];
	int m_aClientIDs[2];
	int m_aUserIDs[2];
	long m_aServerIP;
	int m_aServerPort;
};

struct CSqlRankData
{
	int m_pClientID;
};

#endif
