#ifndef UPLAY_CONFIG_H
#define UPLAY_CONFIG_H

#include <cstdint>

namespace Uplay_Configuration
{
	extern char UserName[0x200];
	extern char UserEmail[0x200];
	extern char password[0x200];
	extern char GameLanguage[0x200];
	extern char UserId[1024];
	extern char CdKey[1024];
	extern char TickedId[1024];
	extern bool Offline;
	extern bool appowned;
	extern bool logging;
	extern bool friends;
	extern bool party;
	extern bool enableSteam;
	extern uint32_t steamId;

	extern int cdkey1;
	extern int cdkey2;
	extern uint32_t gameAppId;
}

#endif