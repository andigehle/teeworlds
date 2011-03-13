#ifndef GAME_SERVER_GAMEMODES_ONEONONE_H
#define GAME_SERVER_GAMEMODES_ONEONONE_H
#include <game/server/gamecontroller.h>
#include <list>

class CGameControllerOneOnOne : public IGameController
{
private:
	struct match
	{
		CPlayer *player1;
		CPlayer *player2;
		int accepted;
	};
	std::list<match> nextmatches;
	std::list<match>::iterator it;

public:
	CGameControllerOneOnOne(class CGameContext *pGameServer);
	bool CanJoinTeam(int Team, int User_id);
	void StartRound();
	void EndRound();
	void EloPoints(int* Scores, int* EloPoints, int* NewEloPoints);
	
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual void Tick();

	void VoteChallenge();
	void Challenge(CPlayer *p, CPlayer *p2);
	void accept(CPlayer *p);
	void decline(CPlayer *p);
	void remove(int ClientId);
};
#endif
