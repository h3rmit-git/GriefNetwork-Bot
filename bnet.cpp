/*

	ent-ghost
	Copyright [2011-2013] [Jack Lu]

	This file is part of the ent-ghost source code.

	ent-ghost is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	ent-ghost source code is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with ent-ghost source code. If not, see <http://www.gnu.org/licenses/>.

	ent-ghost is modified from GHost++ (http://ghostplusplus.googlecode.com/)
	GHost++ is Copyright [2008] [Trevor Hogan]

*/

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "commandpacket.h"
#include "ghostdb.h"
#include "bncsutilinterface.h"
#include "bnlsclient.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameprotocol.h"
#include "game_base.h"

#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CBNET
//

CBNET :: CBNET( CGHost *nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, uint32_t nLocaleID, string nUserName, string nUserPassword, string nKeyOwnerName, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nHoldFriends, bool nHoldClan, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, string nPVPGNRealmName, uint32_t nMaxMessageLength, uint32_t nHostCounterID )
{
	// todotodo: append path seperator to Warcraft3Path if needed

	m_GHost = nGHost;
	m_Socket = new CTCPClient( );
	m_Protocol = new CBNETProtocol( );
	m_BNLSClient = NULL;
	m_BNCSUtil = new CBNCSUtilInterface( nUserName, nUserPassword );
	m_Exiting = false;
	m_Server = nServer;
	string LowerServer = m_Server;
	transform( LowerServer.begin( ), LowerServer.end( ), LowerServer.begin( ), (int(*)(int))tolower );

	if( !nServerAlias.empty( ) )
		m_ServerAlias = nServerAlias;
	else if( LowerServer == "useast.battle.net" )
		m_ServerAlias = "USEast";
	else if( LowerServer == "uswest.battle.net" )
		m_ServerAlias = "USWest";
	else if( LowerServer == "asia.battle.net" )
		m_ServerAlias = "Asia";
	else if( LowerServer == "europe.battle.net" )
		m_ServerAlias = "Europe";
	else
		m_ServerAlias = m_Server;
	
	m_CallableAdminList = m_GHost->m_DB->ThreadedAdminList( nServer );

	if( nPasswordHashType == "pvpgn" && !nBNLSServer.empty( ) )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] pvpgn connection found with a configured BNLS server, ignoring BNLS server" );
		nBNLSServer.clear( );
		nBNLSPort = 0;
		nBNLSWardenCookie = 0;
	}

	m_BNLSServer = nBNLSServer;
	m_BNLSPort = nBNLSPort;
	m_BNLSWardenCookie = nBNLSWardenCookie;
	m_CDKeyROC = nCDKeyROC;
	m_CDKeyTFT = nCDKeyTFT;

	// remove dashes and spaces from CD keys and convert to uppercase

	m_CDKeyROC.erase( remove( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), '-' ), m_CDKeyROC.end( ) );
	m_CDKeyTFT.erase( remove( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), '-' ), m_CDKeyTFT.end( ) );
	m_CDKeyROC.erase( remove( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), ' ' ), m_CDKeyROC.end( ) );
	m_CDKeyTFT.erase( remove( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), ' ' ), m_CDKeyTFT.end( ) );
	transform( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), m_CDKeyROC.begin( ), (int(*)(int))toupper );
	transform( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), m_CDKeyTFT.begin( ), (int(*)(int))toupper );

	if( m_CDKeyROC.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your ROC CD key is not 26 characters long and is probably invalid" );

	if( m_GHost->m_TFT && m_CDKeyTFT.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your TFT CD key is not 26 characters long and is probably invalid" );

	m_CountryAbbrev = nCountryAbbrev;
	m_Country = nCountry;
	m_LocaleID = nLocaleID;
	m_UserName = nUserName;
	m_UserPassword = nUserPassword;
	m_KeyOwnerName = nKeyOwnerName;
	m_FirstChannel = nFirstChannel;
	m_RootAdmin = nRootAdmin;
	transform( m_RootAdmin.begin( ), m_RootAdmin.end( ), m_RootAdmin.begin( ), (int(*)(int))tolower );
	m_CommandTrigger = nCommandTrigger;
	m_War3Version = nWar3Version;
	m_EXEVersion = nEXEVersion;
	m_EXEVersionHash = nEXEVersionHash;
	m_PasswordHashType = nPasswordHashType;
	m_PVPGNRealmName = nPVPGNRealmName;
	m_MaxMessageLength = nMaxMessageLength;
	m_HostCounterID = nHostCounterID;
	m_LastDisconnectedTime = 0;
	m_LastConnectionAttemptTime = 0;
	m_LastNullTime = 0;
	m_LastOutPacketTicks = 0;
	m_LastOutPacketSize = 0;
	m_LastPacketReceivedTicks = GetTicks( );
	m_LastCommandTicks = GetTicks( );
	m_LastAdminRefreshTime = GetTime( );
	m_FirstConnect = true;
	m_WaitingToConnect = true;
	m_LoggedIn = false;
	m_InChat = false;
	m_HoldFriends = nHoldFriends;
	m_HoldClan = nHoldClan;
	m_PublicCommands = nPublicCommands;
	m_LastInviteCreation = false;
	m_ServerReconnectCount = 0;
	m_CDKeyUseCount = 0;
}

CBNET :: ~CBNET( )
{
	delete m_Socket;
	delete m_Protocol;
	delete m_BNLSClient;

	while( !m_Packets.empty( ) )
	{
		delete m_Packets.front( );
		m_Packets.pop( );
	}

	delete m_BNCSUtil;

        for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); ++i )
		delete *i;

        for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); ++i )
		delete *i;

	boost::mutex::scoped_lock lock( m_GHost->m_CallablesMutex );
	
        for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

        for( vector<PairedAdminAdd> :: iterator i = m_PairedAdminAdds.begin( ); i != m_PairedAdminAdds.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

        for( vector<PairedAdminRemove> :: iterator i = m_PairedAdminRemoves.begin( ); i != m_PairedAdminRemoves.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

        for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

        for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

        for( vector<PairedVPSCheck> :: iterator i = m_PairedVPSChecks.begin( ); i != m_PairedVPSChecks.end( ); ++i )
		m_GHost->m_Callables.push_back( i->second );

	    for( vector<PairedVerifyUserCheck> :: iterator i = m_PairedVerifyUserChecks.begin( ); i != m_PairedVerifyUserChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

	if( m_CallableAdminList )
		m_GHost->m_Callables.push_back( m_CallableAdminList );

	lock.unlock( );
}

BYTEARRAY CBNET :: GetUniqueName( )
{
	return m_Protocol->GetUniqueName( );
}

uint32_t CBNET :: GetReconnectTime( )
{
	if( m_GHost->m_FastReconnect )
		return 25;
	if( m_CDKeyUseCount == 0 )
		return 90;
	else
	{
		uint32_t Time = 180 + m_CDKeyUseCount * 90;
		
		if( Time < 600 )
		{
			return Time;
		}
		else
		{
			return 600;
		}
	}
}

unsigned int CBNET :: SetFD( void *fd, void *send_fd, int *nfds )
{
	unsigned int NumFDs = 0;

	if( !m_Socket->HasError( ) && m_Socket->GetConnected( ) )
	{
		m_Socket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
                ++NumFDs;

		if( m_BNLSClient )
			NumFDs += m_BNLSClient->SetFD( fd, send_fd, nfds );
	}

	return NumFDs;
}

bool CBNET :: Update( void *fd, void *send_fd )
{
	//
	// update callables
	//

	for( vector<PairedAdminCount> :: iterator i = m_PairedAdminCounts.begin( ); i != m_PairedAdminCounts.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			uint32_t Count = i->second->GetResult( );

			if( Count == 0 )
				QueueChatCommand( m_GHost->m_Language->ThereAreNoAdmins( m_Server ), i->first, !i->first.empty( ) );
			else if( Count == 1 )
				QueueChatCommand( m_GHost->m_Language->ThereIsAdmin( m_Server ), i->first, !i->first.empty( ) );
			else
				QueueChatCommand( m_GHost->m_Language->ThereAreAdmins( m_Server, UTIL_ToString( Count ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminCounts.erase( i );
		}
		else
                        ++i;
	}

	for( vector<PairedAdminAdd> :: iterator i = m_PairedAdminAdds.begin( ); i != m_PairedAdminAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				AddAdmin( i->second->GetUser( ) );
				QueueChatCommand( m_GHost->m_Language->AddedUserToAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorAddingUserToAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminAdds.erase( i );
		}
		else
                        ++i;
	}

	for( vector<PairedAdminRemove> :: iterator i = m_PairedAdminRemoves.begin( ); i != m_PairedAdminRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				RemoveAdmin( i->second->GetUser( ) );
				QueueChatCommand( m_GHost->m_Language->DeletedUserFromAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->ErrorDeletingUserFromAdminDatabase( m_Server, i->second->GetUser( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedAdminRemoves.erase( i );
		}
		else
                        ++i;
	}

	for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBGamePlayerSummary *GamePlayerSummary = i->second->GetResult( );

			if( GamePlayerSummary )
				QueueChatCommand( "[" + i->second->GetName( ) + "] has played " + UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ) + " games on this bot.", i->first, !i->first.empty( ) );
			else
				QueueChatCommand( m_GHost->m_Language->HasntPlayedGamesWithThisBot( i->second->GetName( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedGPSChecks.erase( i );
		}
		else
                        ++i;
	}

	for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBDotAPlayerSummary *DotAPlayerSummary = i->second->GetResult( );

			if( DotAPlayerSummary )
			{
				string Summary = m_GHost->m_Language->HasPlayedDotAGamesWithThisBot(	i->second->GetName( ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalGames( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalWins( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalLosses( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalDeaths( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCreepKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCreepDenies( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalAssists( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalNeutralKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalTowerKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalRaxKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetTotalCourierKills( ) ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgDeaths( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCreepKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCreepDenies( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgAssists( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgNeutralKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgTowerKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgRaxKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetAvgCourierKills( ), 2 ),
																						UTIL_ToString( DotAPlayerSummary->GetScore( ), 2 ) );

				QueueChatCommand( Summary, i->first, !i->first.empty( ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot( i->second->GetName( ) ), i->first, !i->first.empty( ) );

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedDPSChecks.erase( i );
		}
		else
                        ++i;
	}

	for( vector<PairedVPSCheck> :: iterator i = m_PairedVPSChecks.begin( ); i != m_PairedVPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBVampPlayerSummary *VampPlayerSummary = i->second->GetResult( );

			if( VampPlayerSummary )
			{
				double MinCommandCenter = VampPlayerSummary->GetMinCommandCenter( ) / 60.0;
				double AvgCommandCenter = VampPlayerSummary->GetAvgCommandCenter( ) / 60.0;
				double MinBase = VampPlayerSummary->GetMinBase( ) / 60.0;
				double AvgBase = VampPlayerSummary->GetAvgBase( ) / 60.0;

				string StrMinCC = UTIL_ToString(MinCommandCenter, 2);
				string StrAvgCC = UTIL_ToString(AvgCommandCenter, 2);
				string StrMinBase = UTIL_ToString(MinBase, 2);
				string StrAvgBase = UTIL_ToString(AvgBase, 2);
				
				if(MinCommandCenter <= 0) StrMinCC = "none";
				if(AvgCommandCenter <= 0) StrAvgCC = "none";
				if(MinBase <= 0) StrMinBase = "none";
				if(AvgBase <= 0) StrAvgBase = "none";
				QueueChatCommand( m_GHost->m_Language->HasPlayedVampGamesWithThisBot( i->second->GetName( ),
						UTIL_ToString(VampPlayerSummary->GetTotalGames( )),
						UTIL_ToString(VampPlayerSummary->GetTotalHumanGames( )),
						UTIL_ToString(VampPlayerSummary->GetTotalVampGames( )),
						UTIL_ToString(VampPlayerSummary->GetTotalHumanWins( )),
						UTIL_ToString(VampPlayerSummary->GetTotalVampWins( )),
						UTIL_ToString(VampPlayerSummary->GetTotalHumanLosses( )),
						UTIL_ToString(VampPlayerSummary->GetTotalVampLosses( )),
						UTIL_ToString(VampPlayerSummary->GetTotalVampKills( )),
						StrMinCC,
						StrAvgCC,
						StrMinBase,
						StrAvgBase),
					i->first, !i->first.empty() );
			}
			else
			{
				QueueChatCommand( m_GHost->m_Language->HasntPlayedVampGamesWithThisBot( i->second->GetName( ) ), i->first, !i->first.empty() );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedVPSChecks.erase( i );
		}
		else
			++i;
	}

        for( vector<PairedVerifyUserCheck> :: iterator i = m_PairedVerifyUserChecks.begin( ); i != m_PairedVerifyUserChecks.end( ); )
        {
                if( i->second->GetReady( ) )
                {
			uint32_t result = i->second->GetResult( );

			if(result == 1) {
				QueueChatCommand( "Your account has been successfully connected to your forum account.", i->first, !i->first.empty() );
			} else if(result == 2) {
				QueueChatCommand( "The token and name could not be matched to any pending confirmation.", i->first, !i->first.empty() );
			} else {
				QueueChatCommand( "An unexpected error occured confirming your account.", i->first, !i->first.empty() );
			}

                        m_GHost->m_DB->RecoverCallable( i->second );
                        delete i->second;
                        i = m_PairedVerifyUserChecks.erase( i );
                }
                else
                        ++i;
        }

	// refresh the admin list every hour

	if( !m_CallableAdminList && GetTime( ) - m_LastAdminRefreshTime >= 3600 )
		m_CallableAdminList = m_GHost->m_DB->ThreadedAdminList( m_Server );

	if( m_CallableAdminList && m_CallableAdminList->GetReady( ) )
	{
		// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] refreshed admin list (" + UTIL_ToString( m_Admins.size( ) ) + " -> " + UTIL_ToString( m_CallableAdminList->GetResult( ).size( ) ) + " admins)" );
		m_Admins = m_CallableAdminList->GetResult( );
		m_GHost->m_DB->RecoverCallable( m_CallableAdminList );
		delete m_CallableAdminList;
		m_CallableAdminList = NULL;
		m_LastAdminRefreshTime = GetTime( );
	}

	// we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
	// that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
	// but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

	if( m_Socket->HasError( ) )
	{
		// the socket has an error

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net due to socket error" );

		if( m_Socket->GetError( ) == ECONNRESET && GetTime( ) - m_LastConnectionAttemptTime <= 15 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - you are probably temporarily IP banned from battle.net" );

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		delete m_BNLSClient;
		m_BNLSClient = NULL;
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && !m_WaitingToConnect )
	{
		// the socket was disconnected

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net" );
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		delete m_BNLSClient;
		m_BNLSClient = NULL;
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( m_Socket->GetConnected( ) )
	{
		// the socket is connected and everything appears to be working properly

		m_Socket->DoRecv( (fd_set *)fd );
		ExtractPackets( );
		ProcessPackets( );

		// update the BNLS client

		if( m_BNLSClient )
		{
			if( m_BNLSClient->Update( fd, send_fd ) )
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] deleting BNLS client" );
				delete m_BNLSClient;
				m_BNLSClient = NULL;
			}
			else
			{
				BYTEARRAY WardenResponse = m_BNLSClient->GetWardenResponse( );

				if( !WardenResponse.empty( ) )
					m_Socket->PutBytes( m_Protocol->SEND_SID_WARDEN( WardenResponse ) );
			}
		}

		// check if at least one packet is waiting to be sent and if we've waited long enough to prevent flooding
		// this formula has changed many times but currently we wait 1 second if the last packet was "small", 3.5 seconds if it was "medium", and 4 seconds if it was "big"

		uint32_t WaitTicks = 0;

		if( m_LastOutPacketSize < 10 )
			WaitTicks = 1700;
		else if( m_LastOutPacketSize < 30 )
			WaitTicks = 4000;
		else if( m_LastOutPacketSize < 50 )
			WaitTicks = 4400;
		else if( m_LastOutPacketSize < 70 )
			WaitTicks = 5000;
		else if( m_LastOutPacketSize < 100 )
			WaitTicks = 6500;
		else if( m_LastOutPacketSize < 150 )
			WaitTicks = 7000;
		else if( m_LastOutPacketSize < 200 )
			WaitTicks = 7400;
		else
			WaitTicks = 7800;

		//disable waitticks for our own server
		if( m_Server == "hive.entgaming.net" )
			WaitTicks = 0;

		boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
		
		if( !m_OutPackets.empty( ) && GetTicks( ) - m_LastOutPacketTicks >= WaitTicks )
		{
			if( m_OutPackets.size( ) > 7 )
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] packet queue warning - there are " + UTIL_ToString( m_OutPackets.size( ) ) + " packets waiting to be sent" );

			m_Socket->PutBytes( m_OutPackets.front( ) );
			m_LastOutPacketSize = m_OutPackets.front( ).size( );
			m_OutPackets.pop( );
			m_LastOutPacketTicks = GetTicks( );
		}
		
		packetsLock.unlock( );

		// check if we haven't received a packet in a while
		// after 60 seconds, we send a /whoami chat packet
		// then, if still no response after 90 second total, we disconnect

		if( GetTicks( ) - m_LastPacketReceivedTicks > 60000 && m_Server != "hive.entgaming.net" )
		{
			if( GetTicks( ) - m_LastCommandTicks > 20000 )
				QueueChatCommand( "/whoami" );

			else if( GetTicks( ) - m_LastPacketReceivedTicks > 90000 )
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnecting due to timeout on packet receive time" );
				m_Socket->Disconnect( );
				return m_Exiting;
			}
		}

		// send a null packet every 60 seconds to detect disconnects

		if( GetTime( ) - m_LastNullTime >= 60 && GetTicks( ) - m_LastOutPacketTicks >= 60000 )
		{
			m_Socket->PutBytes( m_Protocol->SEND_SID_NULL( ) );
			m_LastNullTime = GetTime( );
		}

		m_Socket->DoSend( (fd_set *)send_fd );
		return m_Exiting;
	}

	if( m_Socket->GetConnecting( ) )
	{
		// we are currently attempting to connect to battle.net

		if( m_Socket->CheckConnect( ) )
		{
			// the connection attempt completed

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connected" );
			m_GHost->EventBNETConnected( this );
			m_Socket->PutBytes( m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR( ) );
			m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_INFO( m_War3Version, m_GHost->m_TFT, m_LocaleID, m_CountryAbbrev, m_Country ) );
			m_Socket->DoSend( (fd_set *)send_fd );
			m_LastNullTime = GetTime( );
			m_LastOutPacketTicks = GetTicks( );
			m_LastCommandTicks = GetTicks( );
			m_LastPacketReceivedTicks = GetTicks( );

			boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
			
			while( !m_OutPackets.empty( ) )
				m_OutPackets.pop( );
			
			packetsLock.unlock( );

			return m_Exiting;
		}
		else if( GetTime( ) - m_LastConnectionAttemptTime >= 15 )
		{
			// the connection attempt timed out (15 seconds)

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connect timed out" );
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
			m_GHost->EventBNETConnectTimedOut( this );
			m_Socket->Reset( );
			m_LastDisconnectedTime = GetTime( );
			m_WaitingToConnect = true;
			return m_Exiting;
		}
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && ( m_FirstConnect || GetTime( ) - m_LastDisconnectedTime >= GetReconnectTime( ) ) )
	{
		// attempt to connect to battle.net

		m_FirstConnect = false;
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connecting to server [" + m_Server + "] on port 6112" );
		m_GHost->EventBNETConnecting( this );

		if( !m_GHost->m_BindAddress.empty( ) )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]" );

		// clear the cached IP address if we've disconnected several times
		// although resolving is blocking, it's not a big deal because games are anyway in separate threads
		if( m_ServerReconnectCount > 10 )
		{
			m_ServerReconnectCount = 0;
			m_ServerIP = string( );
		}

		if( m_ServerIP.empty( ) )
		{
			m_Socket->Connect( m_GHost->m_BindAddress, m_Server, 6112 );

			if( !m_Socket->HasError( ) )
			{
				m_ServerIP = m_Socket->GetIPString( );
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] resolved and cached server IP address " + m_ServerIP );
			}
		}
		else
		{
			// use cached server IP address since resolving takes time and is blocking

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using cached server IP address " + m_ServerIP );
			m_Socket->Connect( m_GHost->m_BindAddress, m_ServerIP, 6112 );
			
			m_ServerReconnectCount++;
		}

		m_WaitingToConnect = false;
		m_LastConnectionAttemptTime = GetTime( );
		return m_Exiting;
	}

	return m_Exiting;
}

void CBNET :: ExtractPackets( )
{
	// extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

	string *RecvBuffer = m_Socket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		// byte 0 is always 255

		if( Bytes[0] == BNET_HEADER_CONSTANT )
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
					m_Packets.push( new CCommandPacket( BNET_HEADER_CONSTANT, Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );
					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
					return;
			}
			else
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad length), disconnecting" );
				m_Socket->Disconnect( );
				return;
			}
		}
		else
		{
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad header constant), disconnecting" );
			m_Socket->Disconnect( );
			return;
		}
	}
}

void CBNET :: ProcessPackets( )
{
	CIncomingGameHost *GameHost = NULL;
	CIncomingChatEvent *ChatEvent = NULL;
	BYTEARRAY WardenData;
	vector<CIncomingFriendList *> Friends;
	vector<CIncomingClanList *> Clans;

	// process all the received packets in the m_Packets queue
	// this normally means sending some kind of response

	while( !m_Packets.empty( ) )
	{
		CCommandPacket *Packet = m_Packets.front( );
		m_Packets.pop( );
		m_LastPacketReceivedTicks = GetTicks( );

		if( Packet->GetPacketType( ) == BNET_HEADER_CONSTANT )
		{
			switch( Packet->GetID( ) )
			{
			case CBNETProtocol :: SID_NULL:
				// warning: we do not respond to NULL packets with a NULL packet of our own
				// this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
				// official battle.net servers do not respond to NULL packets

				m_Protocol->RECEIVE_SID_NULL( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_GETADVLISTEX:
				GameHost = m_Protocol->RECEIVE_SID_GETADVLISTEX( Packet->GetData( ) );

				if( GameHost )
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining game [" + GameHost->GetGameName( ) + "]" );

				delete GameHost;
				GameHost = NULL;
				break;

			case CBNETProtocol :: SID_ENTERCHAT:
				if( m_Protocol->RECEIVE_SID_ENTERCHAT( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining channel [" + m_FirstChannel + "]" );
					m_InChat = true;
					m_OutPackets.push( m_Protocol->SEND_SID_JOINCHANNEL( m_FirstChannel ) );
				}

				break;

			case CBNETProtocol :: SID_CHATEVENT:
				ChatEvent = m_Protocol->RECEIVE_SID_CHATEVENT( Packet->GetData( ) );

				if( ChatEvent )
					ProcessChatEvent( ChatEvent );

				delete ChatEvent;
				ChatEvent = NULL;
				break;

			case CBNETProtocol :: SID_CHECKAD:
				m_Protocol->RECEIVE_SID_CHECKAD( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_STARTADVEX3:
				if( m_Protocol->RECEIVE_SID_STARTADVEX3( Packet->GetData( ) ) )
				{
					m_InChat = false;
					m_GHost->EventBNETGameRefreshed( this );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] startadvex3 failed" );
					m_GHost->EventBNETGameRefreshFailed( this );
				}

				break;

			case CBNETProtocol :: SID_PING:
				m_Socket->PutBytes( m_Protocol->SEND_SID_PING( m_Protocol->RECEIVE_SID_PING( Packet->GetData( ) ) ) );
				break;

			case CBNETProtocol :: SID_AUTH_INFO:
				if( m_Protocol->RECEIVE_SID_AUTH_INFO( Packet->GetData( ) ) )
				{
					if( m_BNCSUtil->HELP_SID_AUTH_CHECK( m_GHost->m_TFT, m_GHost->m_Warcraft3Path, m_CDKeyROC, m_CDKeyTFT, m_Protocol->GetValueStringFormulaString( ), m_Protocol->GetIX86VerFileNameString( ), m_Protocol->GetClientToken( ), m_Protocol->GetServerToken( ) ) )
					{
						// override the exe information generated by bncsutil if specified in the config file
						// apparently this is useful for pvpgn users

						if( m_EXEVersion.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version bnet_custom_exeversion = " + UTIL_ToString( m_EXEVersion[0] ) + " " + UTIL_ToString( m_EXEVersion[1] ) + " " + UTIL_ToString( m_EXEVersion[2] ) + " " + UTIL_ToString( m_EXEVersion[3] ) );
							m_BNCSUtil->SetEXEVersion( m_EXEVersion );
						}

						if( m_EXEVersionHash.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version hash bnet_custom_exeversionhash = " + UTIL_ToString( m_EXEVersionHash[0] ) + " " + UTIL_ToString( m_EXEVersionHash[1] ) + " " + UTIL_ToString( m_EXEVersionHash[2] ) + " " + UTIL_ToString( m_EXEVersionHash[3] ) );
							m_BNCSUtil->SetEXEVersionHash( m_EXEVersionHash );
						}

						if( m_GHost->m_TFT )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: The Frozen Throne" );
						else
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: Reign of Chaos" );							

						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_CHECK( m_GHost->m_TFT, m_Protocol->GetClientToken( ), m_BNCSUtil->GetEXEVersion( ), m_BNCSUtil->GetEXEVersionHash( ), m_BNCSUtil->GetKeyInfoROC( ), m_BNCSUtil->GetKeyInfoTFT( ), m_BNCSUtil->GetEXEInfo( ), m_KeyOwnerName ) );

						// the Warden seed is the first 4 bytes of the ROC key hash
						// initialize the Warden handler

						if( !m_BNLSServer.empty( ) )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] creating BNLS client" );
							delete m_BNLSClient;
							m_BNLSClient = new CBNLSClient( m_BNLSServer, m_BNLSPort, m_BNLSWardenCookie );
							m_BNLSClient->QueueWardenSeed( UTIL_ByteArrayToUInt32( m_BNCSUtil->GetKeyInfoROC( ), false, 16 ) );
						}
					}
					else
					{
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting" );
						m_Socket->Disconnect( );
						delete Packet;
						return;
					}
				}

				break;

			case CBNETProtocol :: SID_AUTH_CHECK:
				if( m_Protocol->RECEIVE_SID_AUTH_CHECK( Packet->GetData( ) ) )
				{
					// cd keys accepted

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] cd keys accepted" );
					m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON( );
					m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON( m_BNCSUtil->GetClientKey( ), m_UserName ) );
					
					m_ServerReconnectCount = 0;
					m_CDKeyUseCount = 0;
				}
				else
				{
					// cd keys not accepted

					switch( UTIL_ByteArrayToUInt32( m_Protocol->GetKeyState( ), false ) )
					{
					case CBNETProtocol :: KR_ROC_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						m_CDKeyUseCount++;
						
						if( m_Protocol->GetKeyStateDescription( ) == "ur5949" )
							UxReconnected( );
						
						break;
					case CBNETProtocol :: KR_TFT_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						m_CDKeyUseCount++;
						
						if( m_Protocol->GetKeyStateDescription( ) == "ur5949" )
							UxReconnected( );
						
						break;
					case CBNETProtocol :: KR_OLD_GAME_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is too old, disconnecting" );
						break;
					case CBNETProtocol :: KR_INVALID_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is invalid, disconnecting" );
						break;
					default:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - cd keys not accepted, disconnecting" );
						m_CDKeyUseCount++;
						break;
					}

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGON:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] username [" + m_UserName + "] accepted" );

					if( m_PasswordHashType == "pvpgn" )
					{
						// pvpgn logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using pvpgn logon type (for pvpgn servers only)" );
						m_BNCSUtil->HELP_PvPGNPasswordHash( m_UserPassword );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetPvPGNPasswordHash( ) ) );
					}
					else
					{
						// battle.net logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using battle.net logon type (for official battle.net servers only)" );
						m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF( m_Protocol->GetSalt( ), m_Protocol->GetServerPublicKey( ) );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetM1( ) ) );
					}
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid username, disconnecting" );
					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGONPROOF:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF( Packet->GetData( ) ) )
				{
					// logon successful

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon successful" );
					m_LoggedIn = true;
					m_GHost->EventBNETLoggedIn( this );
					m_Socket->PutBytes( m_Protocol->SEND_SID_NETGAMEPORT( m_GHost->m_HostPort ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_ENTERCHAT( ) );
					m_OutPackets.push( m_Protocol->SEND_SID_FRIENDSLIST( ) );
					m_OutPackets.push( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid password, disconnecting" );

					// try to figure out if the user might be using the wrong logon type since too many people are confused by this

					string Server = m_Server;
					transform( Server.begin( ), Server.end( ), Server.begin( ), (int(*)(int))tolower );

					if( m_PasswordHashType == "pvpgn" && ( Server == "useast.battle.net" || Server == "uswest.battle.net" || Server == "asia.battle.net" || Server == "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's \"battle.net custom data\" section" );
					else if( m_PasswordHashType != "pvpgn" && ( Server != "useast.battle.net" && Server != "uswest.battle.net" && Server != "asia.battle.net" && Server != "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's \"battle.net custom data\" section" );

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_WARDEN:
				WardenData = m_Protocol->RECEIVE_SID_WARDEN( Packet->GetData( ) );

				if( m_BNLSClient )
					m_BNLSClient->QueueWardenRaw( WardenData );
				else
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon" );

				break;

			case CBNETProtocol :: SID_FRIENDSLIST:
				Friends = m_Protocol->RECEIVE_SID_FRIENDSLIST( Packet->GetData( ) );

                                for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); ++i )
					delete *i;

				m_Friends = Friends;
				break;

			case CBNETProtocol :: SID_CLANMEMBERLIST: {
				vector<CIncomingClanList *> Clans = m_Protocol->RECEIVE_SID_CLANMEMBERLIST( Packet->GetData( ) );

                                for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); ++i )
					delete *i;

				m_Clans = Clans;
				break;
			}
			
			case CBNETProtocol :: SID_CLANCREATIONINVITATION: {
				string ClanCreateName = m_Protocol->RECEIVE_SID_CLANCREATIONINVITATION( Packet->GetData( ) );
				
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] Invited (creation) to clan " + ClanCreateName + ", !accept to accept" );
				m_LastInviteCreation = true;
				break;
			}
			
			case CBNETProtocol :: SID_CLANINVITATIONRESPONSE: {
				string ClanInviteName = m_Protocol->RECEIVE_SID_CLANINVITATIONRESPONSE( Packet->GetData( ) );
				
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] Invited to clan " + ClanInviteName + ", !accept to accept" );
				m_LastInviteCreation = false;
				break;
			}
			}
		}

		delete Packet;
	}
}

void CBNET :: ProcessChatEvent( CIncomingChatEvent *chatEvent )
{
	CBNETProtocol :: IncomingChatEvent Event = chatEvent->GetChatEvent( );
	bool Whisper = ( Event == CBNETProtocol :: EID_WHISPER );
	string User = chatEvent->GetUser( );
	string Message = chatEvent->GetMessage( );

	if( Event == CBNETProtocol :: EID_WHISPER || Event == CBNETProtocol :: EID_TALK )
	{
		if( Event == CBNETProtocol :: EID_WHISPER )
		{
			CONSOLE_Print( "[WHISPER: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETWhisper( this, User, Message );
		}
		else
		{
			CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETChat( this, User, Message );
		}

		// handle spoof checking for current game
		// this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
		// note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

		boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );
		
		if( Event == CBNETProtocol :: EID_WHISPER && m_GHost->m_CurrentGame )
		{
			bool Success = false;
			QueuedSpoofAdd SpoofAdd;
			SpoofAdd.server = m_Server;
			SpoofAdd.name = User;
			SpoofAdd.sendMessage = false;
			SpoofAdd.failMessage = string( );
			
			if( Message == "s" || Message == "sc" || Message == "spoof" || Message == "check" || Message == "spoofcheck" )
			{
				Success = true;
				SpoofAdd.sendMessage = true;
			}
			
			else if( Message.find( m_GHost->m_CurrentGame->GetGameName( ) ) != string :: npos )
			{
				// look for messages like "entered a Warcraft III The Frozen Throne game called XYZ"
				// we don't look for the English part of the text anymore because we want this to work with multiple languages
				// it's a pretty safe bet that anyone whispering the bot with a message containing the game name is a valid spoofcheck

				if( m_PasswordHashType == "pvpgn" && User == m_PVPGNRealmName )
				{
					// the equivalent pvpgn message is: [PvPGN Realm] Your friend abc has entered a Warcraft III Frozen Throne game named "xyz".

					vector<string> Tokens = UTIL_Tokenize( Message, ' ' );

					if( Tokens.size( ) >= 3 )
						SpoofAdd.name = Tokens[2];
				}
				
				Success = true;
			}
			
			if( Success )
			{
				if( !m_GHost->m_Stage )
				{
					boost::mutex::scoped_lock spoofLock( m_GHost->m_CurrentGame->m_SpoofAddMutex );
					m_GHost->m_CurrentGame->m_DoSpoofAdd.push_back( SpoofAdd );
					spoofLock.unlock( );
				}
				else
				{
					boost::mutex::scoped_lock spoofLock( m_GHost->m_StageMutex );
					m_GHost->m_DoSpoofAdd.push_back( SpoofAdd );
					spoofLock.unlock( );
				}
			}
		}
		
		lock.unlock( );

		// handle bot commands

		if( Message == "?trigger" && ( IsAdmin( User ) || IsRootAdmin( User ) || ( m_PublicCommands && m_OutPackets.size( ) <= 3 ) ) )
			QueueChatCommand( m_GHost->m_Language->CommandTrigger( string( 1, m_CommandTrigger ) ), User, Whisper );
		else if( !Message.empty( ) && Message[0] == m_CommandTrigger )
		{
            BotCommand( Message, User, Whisper, false );
		}
	}
	else if( Event == CBNETProtocol :: EID_CHANNEL )
	{
		// keep track of current channel so we can rejoin it after hosting a game

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joined channel [" + Message + "]" );
		m_CurrentChannel = Message;
	}
	else if( Event == CBNETProtocol :: EID_INFO )
	{
		CONSOLE_Print( "[INFO: " + m_ServerAlias + "] " + Message );

		// extract the first word which we hope is the username
		// this is not necessarily true though since info messages also include channel MOTD's and such

		string UserName;
		string :: size_type Split = Message.find( " " );

		if( Split != string :: npos )
			UserName = Message.substr( 0, Split );
		else
			UserName = Message.substr( 0 );

		// handle spoof checking for current game
		// this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
		// at all times you can still /w the bot with "spoofcheck" to manually spoof check

		boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );
		
		if( m_GHost->m_CurrentGame )
		{
			string FailMessage;
			
			if( Message.find( "is away" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofPossibleIsAway( UserName );
			else if( Message.find( "is unavailable" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofPossibleIsUnavailable( UserName );
			else if( Message.find( "is refusing messages" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofPossibleIsRefusingMessages( UserName );
			else if( Message.find( "is using Warcraft III The Frozen Throne in the channel" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofDetectedIsNotInGame( UserName );
			else if( Message.find( "is using Warcraft III The Frozen Throne in channel" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofDetectedIsNotInGame( UserName );
			else if( Message.find( "is using Warcraft III The Frozen Throne in a private channel" ) != string :: npos )
				FailMessage = m_GHost->m_Language->SpoofDetectedIsInPrivateChannel( UserName );
			
			if( !FailMessage.empty( ) )
			{
				boost::mutex::scoped_lock sayLock( m_GHost->m_CurrentGame->m_SayGamesMutex );
				m_GHost->m_CurrentGame->m_DoSayGames.push_back( FailMessage );
				sayLock.unlock( );
			}


			vector<string> SearchMessages;
			SearchMessages.push_back( "is using Warcraft III The Frozen Throne in game" );
			SearchMessages.push_back( "is using Warcraft III Frozen Throne and is currently in  game");
			SearchMessages.push_back( "is using Warcraft III in game" );
			SearchMessages.push_back( "is using Warcraft III  in game" );

			for( vector<string> :: iterator it = SearchMessages.begin( ); it != SearchMessages.end( ); it++ )
			{
				size_t pos = Message.find( *it );

				if( pos != string :: npos && Message.length( ) > pos + it->length() + 1 )
				{
					// extract the gamename from the message: "... game [GAMENAME]."
					size_t start = pos + it->length() + 1;
					string MessageGameName = Message.substr( start, Message.length( ) - 1 - start );
					
					// check both the current game name and the last game name against the /whois response
					// this is because when the game is rehosted, players who joined recently will be in the previous game according to battle.net
					// note: if the game is rehosted more than once it is possible (but unlikely) for a false positive because only two game names are checked
					QueuedSpoofAdd SpoofAdd;
					SpoofAdd.server = m_Server;
					SpoofAdd.name = UserName;
					SpoofAdd.sendMessage = false;
					SpoofAdd.failMessage = string( );

					if( MessageGameName != m_GHost->m_CurrentGame->GetGameName( ) && MessageGameName != m_GHost->m_CurrentGame->GetLastGameName( ) )
						SpoofAdd.failMessage = m_GHost->m_Language->SpoofDetectedIsInAnotherGame( UserName );

					if( !m_GHost->m_Stage )
					{
						boost::mutex::scoped_lock spoofLock( m_GHost->m_CurrentGame->m_SpoofAddMutex );
						m_GHost->m_CurrentGame->m_DoSpoofAdd.push_back( SpoofAdd );
						spoofLock.unlock( );
					}
					else
					{
						boost::mutex::scoped_lock spoofLock( m_GHost->m_StageMutex );
						m_GHost->m_DoSpoofAdd.push_back( SpoofAdd );
						spoofLock.unlock( );
					}
				}
			}
		}
		
		lock.unlock( );
	}
	else if( Event == CBNETProtocol :: EID_ERROR )
		CONSOLE_Print( "[ERROR: " + m_ServerAlias + "] " + Message );
	else if( Event == CBNETProtocol :: EID_EMOTE )
	{
		CONSOLE_Print( "[EMOTE: " + m_ServerAlias + "] [" + User + "] " + Message );
		m_GHost->EventBNETEmote( this, User, Message );
	}
}

void CBNET :: BotCommand( string Message, string User, bool Whisper, bool ForceRoot ) {
	// extract the command trigger, the command, and the payload
	// e.g. "!say hello world" -> command: "say", payload: "hello world"

	string Command;
	string Payload;
	string :: size_type PayloadStart = Message.find( " " );

	if( PayloadStart != string :: npos )
	{
		Command = Message.substr( 1, PayloadStart - 1 );
		Payload = Message.substr( PayloadStart + 1 );
	}
	else
		Command = Message.substr( 1 );

	transform( Command.begin( ), Command.end( ), Command.begin( ), (int(*)(int))tolower );

	// check if this is root admin bot on hive.entgaming.net
//	string LowerUser = User;
//	transform( LowerUser.begin( ), LowerUser.end( ), LowerUser.begin( ), (int(*)(int))tolower );

//	if( m_Server == "hive.entgaming.net" && User == "clan.enterprise" )
//		ForceRoot = true;

	if( IsAdmin( User ) || IsRootAdmin( User ) || ForceRoot )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] admin [" + User + "] sent command [" + Message + "]" );

		/*****************
		* ADMIN COMMANDS *
		******************/
		
        //
		// !ADDADMIN
		//

		if( Command == "addadmin" && !Payload.empty( ) )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( IsAdmin( Payload ) )
					QueueChatCommand( m_GHost->m_Language->UserIsAlreadyAnAdmin( m_Server, Payload ), User, Whisper );
				else
					m_PairedAdminAdds.push_back( PairedAdminAdd( Whisper ? User : string( ), m_GHost->m_DB->ThreadedAdminAdd( m_Server, Payload ) ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !ADDBAN
		// !BAN
		//

		else if( ( Command == "addban" || Command == "ban" || Command == "kick" || Command == "ipkick" ) && !Payload.empty( ) )
		{
			// extract the victim and the reason
			// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

			string Victim;
			string Reason;
			stringstream SS;
			SS << Payload;
			SS >> Victim;

			if( !SS.eof( ) )
			{
				getline( SS, Reason );
				string :: size_type Start = Reason.find_first_not_of( " " );

				if( Start != string :: npos )
					Reason = Reason.substr( Start );
			}
			
			boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );

			string ForwardCommand = "kick";

			if( Command == "ipkick" )
				ForwardCommand = "ipkick";
				
			if( m_GHost->m_CurrentGame )
			{
				boost::mutex::scoped_lock sayLock( m_GHost->m_CurrentGame->m_SayGamesMutex );
				m_GHost->m_CurrentGame->m_DoSayGames.push_back( "/" + ForwardCommand + " " + Victim );
				sayLock.unlock( );
			}

			for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); ++i )
			{
				boost::mutex::scoped_lock sayLock( (*i)->m_SayGamesMutex );
				(*i)->m_DoSayGames.push_back( "/" + ForwardCommand + " " + Victim );
				sayLock.unlock( );
			}
			
			lock.unlock( );
		}

		//
		// !AUTOHOST
		//

		else if( Command == "autohost" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( Payload.empty( ) || Payload == "off" )
				{
					QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
					m_GHost->m_AutoHostGameName.clear( );
					m_GHost->m_AutoHostOwner.clear( );
					m_GHost->m_AutoHostServer.clear( );
					m_GHost->m_AutoHostMaximumGames = 0;
					m_GHost->m_AutoHostAutoStartPlayers = 0;
					m_GHost->m_LastAutoHostTime = GetTime( );
					m_GHost->m_AutoHostMatchMaking = false;
					m_GHost->m_AutoHostMinimumScore = 0.0;
					m_GHost->m_AutoHostMaximumScore = 0.0;
				}
				else
				{
					// extract the maximum games, auto start players, and the game name
					// e.g. "5 10 BattleShips Pro" -> maximum games: "5", auto start players: "10", game name: "BattleShips Pro"

					uint32_t MaximumGames;
					uint32_t AutoStartPlayers;
					string GameName;
					stringstream SS;
					SS << Payload;
					SS >> MaximumGames;

					if( SS.fail( ) || MaximumGames == 0 )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohost command" );
					else
					{
						SS >> AutoStartPlayers;

						if( SS.fail( ) || AutoStartPlayers == 0 )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohost command" );
						else
						{
							if( SS.eof( ) )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #3 to autohost command" );
							else
							{
								getline( SS, GameName );
								string :: size_type Start = GameName.find_first_not_of( " " );

								if( Start != string :: npos )
									GameName = GameName.substr( Start );

								QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
								m_GHost->ClearAutoHostMap( );
								m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
								m_GHost->m_AutoHostGameName = GameName;
								m_GHost->m_AutoHostOwner = User;
								m_GHost->m_AutoHostServer = m_Server;
								m_GHost->m_AutoHostMaximumGames = MaximumGames;
								m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
								m_GHost->m_LastAutoHostTime = GetTime( );
								m_GHost->m_AutoHostMatchMaking = false;
								m_GHost->m_AutoHostMinimumScore = 0.0;
								m_GHost->m_AutoHostMaximumScore = 0.0;
							}
						}
					}
				}
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !AUTOHOSTMM
		//

		else if( Command == "autohostmm" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( Payload.empty( ) || Payload == "off" )
				{
					QueueChatCommand( m_GHost->m_Language->AutoHostDisabled( ), User, Whisper );
					m_GHost->m_AutoHostGameName.clear( );
					m_GHost->m_AutoHostOwner.clear( );
					m_GHost->m_AutoHostServer.clear( );
					m_GHost->m_AutoHostMaximumGames = 0;
					m_GHost->m_AutoHostAutoStartPlayers = 0;
					m_GHost->m_LastAutoHostTime = GetTime( );
					m_GHost->m_AutoHostMatchMaking = false;
					m_GHost->m_AutoHostMinimumScore = 0.0;
					m_GHost->m_AutoHostMaximumScore = 0.0;
				}
				else
				{
					// extract the maximum games, auto start players, minimum score, maximum score, and the game name
					// e.g. "5 10 800 1200 BattleShips Pro" -> maximum games: "5", auto start players: "10", minimum score: "800", maximum score: "1200", game name: "BattleShips Pro"

					uint32_t MaximumGames;
					uint32_t AutoStartPlayers;
					double MinimumScore;
					double MaximumScore;
					string GameName;
					stringstream SS;
					SS << Payload;
					SS >> MaximumGames;

					if( SS.fail( ) || MaximumGames == 0 )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #1 to autohostmm command" );
					else
					{
						SS >> AutoStartPlayers;

						if( SS.fail( ) || AutoStartPlayers == 0 )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #2 to autohostmm command" );
						else
						{
							SS >> MinimumScore;

							if( SS.fail( ) )
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #3 to autohostmm command" );
							else
							{
								SS >> MaximumScore;

								if( SS.fail( ) )
									CONSOLE_Print( "[BNET: " + m_ServerAlias + "] bad input #4 to autohostmm command" );
								else
								{
									if( SS.eof( ) )
										CONSOLE_Print( "[BNET: " + m_ServerAlias + "] missing input #5 to autohostmm command" );
									else
									{
										getline( SS, GameName );
										string :: size_type Start = GameName.find_first_not_of( " " );

										if( Start != string :: npos )
											GameName = GameName.substr( Start );

										QueueChatCommand( m_GHost->m_Language->AutoHostEnabled( ), User, Whisper );
										m_GHost->ClearAutoHostMap( );
										m_GHost->m_AutoHostMap.push_back( new CMap( *m_GHost->m_Map ) );
										m_GHost->m_AutoHostOwner = User;
										m_GHost->m_AutoHostServer = m_Server;
										m_GHost->m_AutoHostMaximumGames = MaximumGames;
										m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
										m_GHost->m_LastAutoHostTime = GetTime( );
										m_GHost->m_AutoHostMatchMaking = true;
										m_GHost->m_AutoHostMinimumScore = MinimumScore;
										m_GHost->m_AutoHostMaximumScore = MaximumScore;
									}
								}
							}
						}
					}
				}
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !CHANNEL (change channel)
		//

		else if( Command == "channel" && !Payload.empty( ) )
			QueueChatCommand( "/join " + Payload );

		//
		// !CHECKADMIN
		//

		else if( Command == "checkadmin" && !Payload.empty( ) )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( IsAdmin( Payload ) )
					QueueChatCommand( m_GHost->m_Language->UserIsAnAdmin( m_Server, Payload ), User, Whisper );
				else
					QueueChatCommand( m_GHost->m_Language->UserIsNotAnAdmin( m_Server, Payload ), User, Whisper );
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !COUNTADMINS
		//

		else if( Command == "countadmins" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
				m_PairedAdminCounts.push_back( PairedAdminCount( Whisper ? User : string( ), m_GHost->m_DB->ThreadedAdminCount( m_Server ) ) );
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !DBSTATUS
		//

		else if( Command == "dbstatus" )
			QueueChatCommand( m_GHost->m_DB->GetStatus( ), User, Whisper );

		//
		// !DELADMIN
		//

		else if( Command == "deladmin" && !Payload.empty( ) )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( !IsAdmin( Payload ) )
					QueueChatCommand( m_GHost->m_Language->UserIsNotAnAdmin( m_Server, Payload ), User, Whisper );
				else
					m_PairedAdminRemoves.push_back( PairedAdminRemove( Whisper ? User : string( ), m_GHost->m_DB->ThreadedAdminRemove( m_Server, Payload ) ) );
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !DISABLE
		//

		else if( Command == "disable" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				QueueChatCommand( m_GHost->m_Language->BotDisabled( ), User, Whisper );
				m_GHost->m_Enabled = false;
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !ENABLE
		//

		else if( Command == "enable" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				QueueChatCommand( m_GHost->m_Language->BotEnabled( ), User, Whisper );
				m_GHost->m_Enabled = true;
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !ENFORCESG
		//

		else if( Command == "enforcesg" && !Payload.empty( ) )
		{
			// only load files in the current directory just to be safe

			if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
				QueueChatCommand( m_GHost->m_Language->UnableToLoadReplaysOutside( ), User, Whisper );
			else
			{
				string File = m_GHost->m_ReplayPath + Payload + ".w3g";

				if( UTIL_FileExists( File ) )
				{
					QueueChatCommand( m_GHost->m_Language->LoadingReplay( File ), User, Whisper );
					CReplay *Replay = new CReplay( );
					Replay->Load( File, false );
					Replay->ParseReplay( false );
					m_GHost->m_EnforcePlayers = Replay->GetPlayers( );
					delete Replay;
				}
				else
					QueueChatCommand( m_GHost->m_Language->UnableToLoadReplayDoesntExist( File ), User, Whisper );
			}
		}

		//
		// !EXIT
		// !QUIT
		//

		else if( Command == "exit" || Command == "quit" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				if( Payload == "nice" )
					m_GHost->m_ExitingNice = true;
				else if( Payload == "force" )
					m_Exiting = true;
				else
				{
					if( m_GHost->m_CurrentGame || !m_GHost->m_Games.empty( ) )
						QueueChatCommand( m_GHost->m_Language->AtLeastOneGameActiveUseForceToShutdown( ), User, Whisper );
					else
						m_Exiting = true;
				}
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !GETCLAN
		//

		else if( Command == "getclan" )
		{
			SendGetClanList( );
			QueueChatCommand( m_GHost->m_Language->UpdatingClanList( ), User, Whisper );
		}

		//
		// !GETFRIENDS
		//

		else if( Command == "getfriends" )
		{
			SendGetFriendsList( );
			QueueChatCommand( m_GHost->m_Language->UpdatingFriendsList( ), User, Whisper );
		}

		//
		// !GETGAME
		//

		else if( Command == "getgame" && !Payload.empty( ) )
		{
			uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;
			
			boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );

			if( GameNumber < m_GHost->m_Games.size( ) )
				QueueChatCommand( m_GHost->m_Language->GameNumberIs( Payload, m_GHost->m_Games[GameNumber]->GetDescription( ) ), User, Whisper );
			else
				QueueChatCommand( m_GHost->m_Language->GameNumberDoesntExist( Payload ), User, Whisper );
			
			lock.unlock( );
		}

		//
		// !GETGAMES
		//

		else if( Command == "getgames" )
		{
			boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );
			
			if( m_GHost->m_CurrentGame )
				QueueChatCommand( m_GHost->m_Language->GameIsInTheLobby( m_GHost->m_CurrentGame->GetDescription( ), UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
			else
				QueueChatCommand( m_GHost->m_Language->ThereIsNoGameInTheLobby( UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
			
			lock.unlock( );
		}

		//
		// !HOSTSG
		//

		else if( Command == "hostsg" && !Payload.empty( ) )
			m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, true, Payload, User, User, m_Server, Whisper );

        //
		// !LOAD (load config file)
		//

		else if( Command == "load" || Command == "loadobs" )
		{
			if( Payload.empty( ) )
				QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
			else
			{
				string FoundMapConfigs;

				try
				{
					path MapCFGPath( m_GHost->m_MapCFGPath );
					string Pattern = Payload;
					transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

					if( !exists( MapCFGPath ) )
					{
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - map config path doesn't exist" );
						QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
					}
					else
					{
						string File = "";
						string PayloadFileName = UTIL_FileSafeName( Payload );

						if( UTIL_FileExists( m_GHost->m_MapCFGPath + PayloadFileName ) )
							File = PayloadFileName;
						else if( UTIL_FileExists( m_GHost->m_MapCFGPath + PayloadFileName + ".cfg" ) )
							File = PayloadFileName + ".cfg";
						else
						{
							directory_iterator EndIterator;
							path LastMatch;
							uint32_t Matches = 0;

							for( directory_iterator i( MapCFGPath ); i != EndIterator; ++i )
							{
								string FileName = i->path( ).filename( ).string( );
								string Stem = i->path( ).stem( ).string( );
								transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
								transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

								if( !is_directory( i->status( ) ) && i->path( ).extension( ) == ".cfg" && FileName.find( Pattern ) != string :: npos )
								{
									LastMatch = i->path( );
		                                                                    ++Matches;

									if( FoundMapConfigs.empty( ) )
										FoundMapConfigs = i->path( ).filename( ).string( );
									else
										FoundMapConfigs += ", " + i->path( ).filename( ).string( );

									// if the pattern matches the filename exactly, with or without extension, stop any further matching

									if( FileName == Pattern || Stem == Pattern )
									{
										Matches = 1;
										break;
									}
								}
							}

							if( Matches == 0 )
								QueueChatCommand( m_GHost->m_Language->NoMapConfigsFound( ), User, Whisper );
							else if( Matches == 1 )
								File = LastMatch.filename( ).string( );
							else
								QueueChatCommand( m_GHost->m_Language->FoundMapConfigs( FoundMapConfigs ), User, Whisper );
						}

						if( !File.empty( ) )
						{
							QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( m_GHost->m_MapCFGPath + File ), User, Whisper );
							CConfig *MapCFG = new CConfig( );
							MapCFG->Read( m_GHost->m_MapCFGPath + File );

							if( Command == "loadobs" )
								MapCFG->Set( "map_observers", "4" );
						
							m_GHost->AsynchronousMapLoad( MapCFG, m_GHost->m_MapCFGPath + File );
						}
					}
				}
				catch( const exception &ex )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - caught exception [" + ex.what( ) + "]" );
					QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
				}
			}
		}

		//
		// !LOADSG
		//

		else if( Command == "loadsg" && !Payload.empty( ) )
		{
			// only load files in the current directory just to be safe

			if( Payload.find( "/" ) != string :: npos || Payload.find( "\\" ) != string :: npos )
				QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGamesOutside( ), User, Whisper );
			else
			{
				string File = m_GHost->m_SaveGamePath + Payload + ".w3z";
				string FileNoPath = Payload + ".w3z";

				if( UTIL_FileExists( File ) )
				{
					if( m_GHost->m_CurrentGame )
						QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGameGameInLobby( ), User, Whisper );
					else
					{
						QueueChatCommand( m_GHost->m_Language->LoadingSaveGame( File ), User, Whisper );
						m_GHost->m_SaveGame->Load( File, false );
						m_GHost->m_SaveGame->ParseSaveGame( );
						m_GHost->m_SaveGame->SetFileName( File );
						m_GHost->m_SaveGame->SetFileNameNoPath( FileNoPath );
					}
				}
				else
					QueueChatCommand( m_GHost->m_Language->UnableToLoadSaveGameDoesntExist( File ), User, Whisper );
			}
		}

		//
		// !MAP (load map file)
		//

		else if( Command == "map" || Command == "mapobs" )
		{
			if( Payload.empty( ) )
				QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
			else
			{
				string FoundMaps;

				try
				{
					path MapPath( m_GHost->m_MapPath );
					string Pattern = Payload;
					transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

					if( !exists( MapPath ) )
					{
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist" );
						QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
					}
					else
					{
						directory_iterator EndIterator;
						path LastMatch;
						uint32_t Matches = 0;

                                                        for( directory_iterator i( MapPath ); i != EndIterator; ++i )
						{
							string FileName = i->path( ).filename( ).string( );
							string Stem = i->path( ).stem( ).string( );
							transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
							transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

							if( !is_directory( i->status( ) ) && FileName.find( Pattern ) != string :: npos )
							{
								LastMatch = i->path( );
                                                                        ++Matches;

								if( FoundMaps.empty( ) )
									FoundMaps = i->path( ).filename( ).string( );
								else
									FoundMaps += ", " + i->path( ).filename( ).string( );

								// if the pattern matches the filename exactly, with or without extension, stop any further matching

								if( FileName == Pattern || Stem == Pattern )
								{
									Matches = 1;
									break;
								}
							}
						}

						if( Matches == 0 )
							QueueChatCommand( m_GHost->m_Language->NoMapsFound( ), User, Whisper );
						else if( Matches == 1 )
						{
							string File = LastMatch.filename( ).string( );
							QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), User, Whisper );

							// hackhack: create a config file in memory with the required information to load the map

							CConfig *MapCFG = new CConfig( );
							MapCFG->Set( "map_path", "Maps\\Download\\" + File );
							MapCFG->Set( "map_localpath", File );

							if( Command == "mapobs" )
								MapCFG->Set( "map_observers", "4" );
							
							m_GHost->AsynchronousMapLoad( MapCFG, File );
						}
						else
							QueueChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), User, Whisper );
					}
				}
				catch( const exception &ex )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - caught exception [" + ex.what( ) + "]" );
					QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
				}
			}
		}

		//
		// !PRIV (host private game)
		//

		else if( Command == "priv" && !Payload.empty( ) )
		{
			if( !m_GHost->m_MapGameCreateRequest )
			{
				GameCreateRequest *Request = new GameCreateRequest;
				Request->gameState = GAME_PRIVATE;
				Request->saveGame = false;
				Request->gameName = Payload;
				Request->ownerName = User;
				Request->creatorName = User;
				Request->creatorServer = m_Server;
				Request->whisper = Whisper;
				m_GHost->m_MapGameCreateRequest = Request;
				m_GHost->m_MapGameCreateRequestTicks = GetTicks( );
			}
			else
				QueueChatCommand( "Unable to create game: there is already a game being created.", User, Whisper );
		}

		//
		// !PRIVBY (host private game by other player)
		//

		else if( Command == "privby" && !Payload.empty( ) )
		{
			// extract the owner and the game name
			// e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

			string Owner;
			string GameName;
			string :: size_type GameNameStart = Payload.find( " " );

			if( GameNameStart != string :: npos )
			{
				Owner = Payload.substr( 0, GameNameStart );
				GameName = Payload.substr( GameNameStart + 1 );

				if( !m_GHost->m_MapGameCreateRequest )
				{
					GameCreateRequest *Request = new GameCreateRequest;
					Request->gameState = GAME_PRIVATE;
					Request->saveGame = false;
					Request->gameName = GameName;
					Request->ownerName = Owner;
					Request->creatorName = User;
					Request->creatorServer = m_Server;
					Request->whisper = Whisper;
					m_GHost->m_MapGameCreateRequest = Request;
					m_GHost->m_MapGameCreateRequestTicks = GetTicks( );
				}
				else
					QueueChatCommand( "Unable to create game: there is already a game being created.", User, Whisper );
			}
		}

		//
		// !PUB (host public game)
		//

		else if( Command == "pub" && !Payload.empty( ) )
		{
			if( !m_GHost->m_MapGameCreateRequest )
			{
				GameCreateRequest *Request = new GameCreateRequest;
				Request->gameState = GAME_PUBLIC;
				Request->saveGame = false;
				Request->gameName = Payload;
				Request->ownerName = User;
				Request->creatorName = User;
				Request->creatorServer = m_Server;
				Request->whisper = Whisper;
				m_GHost->m_MapGameCreateRequest = Request;
				m_GHost->m_MapGameCreateRequestTicks = GetTicks( );
			}
			else
				QueueChatCommand( "Unable to create game: there is already a game being created.", User, Whisper );
		}

		//
		// !PUBBY (host public game by other player)
		//

		else if( Command == "pubby" && !Payload.empty( ) )
		{
			// extract the owner and the game name
			// e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

			string Owner;
			string GameName;
			string :: size_type GameNameStart = Payload.find( " " );

			if( GameNameStart != string :: npos )
			{
				Owner = Payload.substr( 0, GameNameStart );
				GameName = Payload.substr( GameNameStart + 1 );

				if( !m_GHost->m_MapGameCreateRequest )
				{
					GameCreateRequest *Request = new GameCreateRequest;
					Request->gameState = GAME_PUBLIC;
					Request->saveGame = false;
					Request->gameName = GameName;
					Request->ownerName = Owner;
					Request->creatorName = User;
					Request->creatorServer = m_Server;
					Request->whisper = Whisper;
					m_GHost->m_MapGameCreateRequest = Request;
					m_GHost->m_MapGameCreateRequestTicks = GetTicks( );
				}
				else
					QueueChatCommand( "Unable to create game: there is already a game being created.", User, Whisper );
			}
		}

		//
		// !MB (host public/private game with set map/config and by other player)
		//

		else if( Command == "mb" && !Payload.empty( ) )
		{
			string GameType;
			string MapType;
			string Map;
			string Owner;
			string GameName;
			
			stringstream SS;
			SS << Payload;
			SS >> GameType;
			
			if( ( GameType == "u" || GameType == "r" ) && !SS.eof( ) )
			{
				if( GameType == "u" )
					GameType = "pubby";
				else
					GameType = "privby";
				
				SS >> MapType;
				
				if( ( MapType == "m" || MapType == "l" || MapType == "o" || MapType == "p" ) && !SS.eof( ) )
				{
					string MapCommand = "load";

					if( MapType == "p" )
						MapCommand = "loadobs";
					else if( MapType == "m" )
						MapCommand = "map";
					else if( MapType == "o" )
						MapCommand = "mapobs";
					
					SS >> Map;
					
					//change pipes in map to spaces
					// this is done to avoid problems with map and then gamename
					UTIL_Replace( Map, "|", " " );
					
					if( !SS.eof( ) )
					{
						SS >> Owner;
					
						if( !SS.eof( ) )
						{
							getline( SS, GameName );
							string :: size_type Start = GameName.find_first_not_of( " " );

							if( Start != string :: npos )
								GameName = GameName.substr( Start );
						
							//load the map through secondary command
							BotCommand( m_CommandTrigger + MapCommand + " " + Map, User, Whisper, ForceRoot );

							//host the game through secondary command
							BotCommand( m_CommandTrigger + GameType + " " + Owner + " " + GameName, User, Whisper, ForceRoot );
							
						}
					}
				}
			}
		}

		//
		// !RELOAD
		//

		else if( Command == "reload" )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				QueueChatCommand( m_GHost->m_Language->ReloadingConfigurationFiles( ), User, Whisper );
				m_GHost->ReloadConfigs( );
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !SAY
		//

		else if( Command == "say" && !Payload.empty( ) )
			QueueChatCommand( Payload );

		//
		// !SAYGAMES
		//

		else if( ( Command == "saygames" || Command == "saylobby" ) && !Payload.empty( ) )
		{
			if( IsRootAdmin( User ) || ForceRoot )
			{
				boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );
				
				if( m_GHost->m_CurrentGame )
				{
					boost::mutex::scoped_lock sayLock( m_GHost->m_CurrentGame->m_SayGamesMutex );
					m_GHost->m_CurrentGame->m_DoSayGames.push_back( Payload );
					sayLock.unlock( );
				}
				
				if( Command == "saygames" )
				{
					for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); ++i )
					{
						boost::mutex::scoped_lock sayLock( (*i)->m_SayGamesMutex );
						(*i)->m_DoSayGames.push_back( Payload );
						sayLock.unlock( );
					}
				}
				
				lock.unlock( );
			}
			else
				QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
		}

		//
		// !UNHOST
		//

		else if( Command == "unhost" )
		{
			boost::mutex::scoped_lock lock( m_GHost->m_GamesMutex );
			
			if( m_GHost->m_CurrentGame )
			{
				if( m_GHost->m_CurrentGame->GetCountDownStarted( ) )
					QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameCountdownStarted( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );

				// if the game owner is still in the game only allow the root admin to unhost the game

				else if( m_GHost->m_CurrentGame->GetPlayerFromName( m_GHost->m_CurrentGame->GetOwnerName( ), false ) && !IsRootAdmin( User ) && !ForceRoot )
					QueueChatCommand( m_GHost->m_Language->CantUnhostGameOwnerIsPresent( m_GHost->m_CurrentGame->GetOwnerName( ) ), User, Whisper );
				else
				{
					QueueChatCommand( m_GHost->m_Language->UnhostingGame( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );
					m_GHost->m_CurrentGame->SetExiting( true );
				}
			}
			else
				QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameNoGameInLobby( ), User, Whisper );
			
			lock.unlock( );
		}

		//
		// !WARDENSTATUS
		//

		else if( Command == "wardenstatus" )
		{
			if( m_BNLSClient )
				QueueChatCommand( "WARDEN STATUS --- " + UTIL_ToString( m_BNLSClient->GetTotalWardenIn( ) ) + " requests received, " + UTIL_ToString( m_BNLSClient->GetTotalWardenOut( ) ) + " responses sent.", User, Whisper );
			else
				QueueChatCommand( "WARDEN STATUS --- Not connected to BNLS server.", User, Whisper );
		}
	}
	else
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] non-admin [" + User + "] sent command [" + Message + "]" );

	/*********************
	* NON ADMIN COMMANDS *
	*********************/

	// don't respond to non admins if there are more than 3 messages already in the queue
	// this prevents malicious users from filling up the bot's chat queue and crippling the bot
	// in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
	// e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

	if( IsAdmin( User ) || IsRootAdmin( User ) || ( m_PublicCommands && m_OutPackets.size( ) <= 3 ) )
	{
		//
		// !STATS
		//

		if( Command == "stats" )
		{
			string StatsUser = User;

			if( !Payload.empty( ) )
				StatsUser = Payload;

			// check for potential abuse

			if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
				m_PairedGPSChecks.push_back( PairedGPSCheck( Whisper ? User : string( ), m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser, string( ) ) ) );
		}

		//
		// !STATSDOTA
		//

		else if( Command == "statsdota" || Command == "sd" )
		{
			string StatsUser = User;

			if( !Payload.empty( ) )
				StatsUser = Payload;

			// check for potential abuse

			if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
				m_PairedDPSChecks.push_back( PairedDPSCheck( Whisper ? User : string( ), m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, string( ), "dota" ) ) );
		}
				
		//
		// !VAMPSTATS
		//

		else if( Command == "vampstats" || Command == "vs" )
		{
			string StatsUser = User;

			if( !Payload.empty( ) )
				StatsUser = Payload;

			// check for potential abuse

			if( !StatsUser.empty( ) && StatsUser.size( ) < 16 && StatsUser[0] != '/' )
				m_PairedVPSChecks.push_back( PairedVPSCheck( Whisper ? User : string( ), m_GHost->m_DB->ThreadedVampPlayerSummaryCheck( StatsUser ) ) );
		}
        
        //
        // !SLAP
        //
        
		else if( (Command == "slap" )&& !Payload.empty() && !m_GHost->m_SlapPhrases.empty() )
		{
			//pick a phrase
			uint32_t numPhrases = m_GHost->m_SlapPhrases.size();
			uint32_t randomPhrase = rand() % numPhrases;
		
			string phrase = m_GHost->m_SlapPhrases[randomPhrase];
			uint32_t nameIndex = phrase.find_first_of("[") + 1;
			if(!Whisper)
				QueueChatCommand("[" + User + "] " + phrase.insert(nameIndex, Payload), User, Whisper);
		}

		//
		// !VERSION
		//

		else if( Command == "version" )
		{
			if( IsAdmin( User ) || IsRootAdmin( User ) )
				QueueChatCommand( m_GHost->m_Language->VersionAdmin( m_GHost->m_Version ), User, Whisper );
			else
				QueueChatCommand( m_GHost->m_Language->VersionNotAdmin( m_GHost->m_Version ), User, Whisper );
		}

		//
		// !VERIFY
		//
		else if( Command == "verify" )
		{
			m_PairedVerifyUserChecks.push_back( PairedVerifyUserCheck( Whisper ? User : string( ), m_GHost->m_DB->ThreadedVerifyUser( User, Payload, m_Server )));
		}
	}
}

void CBNET :: UxReconnected( )
{
	boost::mutex::scoped_lock lock( m_GHost->m_CallablesMutex );
	m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedReconUpdate( m_HostCounterID, GetReconnectTime( ) ) );
	lock.unlock( );
}

void CBNET :: SendJoinChannel( string channel )
{
	if( m_LoggedIn && m_InChat )
		m_OutPackets.push( m_Protocol->SEND_SID_JOINCHANNEL( channel ) );
}

void CBNET :: SendGetFriendsList( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_FRIENDSLIST( ) );
}

void CBNET :: SendGetClanList( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
}

void CBNET :: QueueEnterChat( )
{
	boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
	
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_ENTERCHAT( ) );
	
	packetsLock.unlock( );
}

void CBNET :: SendClanInvitation( string accountName )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_CLANINVITATION( accountName ) );
}

void CBNET :: SendClanRemoveMember( string accountName )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_CLANREMOVEMEMBER( accountName ) );
}

void CBNET :: SendClanChangeRank( string accountName, CBNETProtocol :: RankCode rank )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_CLANCHANGERANK( accountName, rank ) );
}

void CBNET :: SendClanSetMotd( string motd )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_CLANSETMOTD( motd ) );
}

void CBNET :: SendClanAcceptInvite( bool accept )
{
	if( m_LoggedIn ) {
		if( m_LastInviteCreation )
			m_OutPackets.push( m_Protocol->SEND_SID_CLANCREATIONINVITATION( accept ) );
		else
			m_OutPackets.push( m_Protocol->SEND_SID_CLANINVITATIONRESPONSE( accept ) );
	}
}

void CBNET :: QueueChatCommand( string chatCommand )
{
	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

		if( chatCommand.size( ) > 255 )
			chatCommand = chatCommand.substr( 0, 255 );

		boost::mutex::scoped_lock packetsLock( m_PacketsMutex );

		if( chatCommand.size( ) > 4 && chatCommand.substr( 0, 4 ) == "/who" )
			m_LastCommandTicks = GetTicks( );
		
		if( m_OutPackets.size( ) > 10 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempted to queue chat command [" + chatCommand + "] but there are too many (" + UTIL_ToString( m_OutPackets.size( ) ) + ") packets queued, discarding" );
		else
		{
			CONSOLE_Print( "[QUEUED: " + m_ServerAlias + "] " + chatCommand );
			m_OutPackets.push( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
		}
		
		packetsLock.unlock( );
	}
}

void CBNET :: QueueChatCommand( string chatCommand, string user, bool whisper )
{
	if( chatCommand.empty( ) )
		return;

	// if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command

	if( whisper )
		QueueChatCommand( "/w " + user + " " + chatCommand );
	else
		QueueChatCommand( chatCommand );
}

void CBNET :: QueueGameCreate( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *savegame, uint32_t hostCounter )
{
	if( m_LoggedIn && map )
	{
		if( !m_CurrentChannel.empty( ) )
			m_FirstChannel = m_CurrentChannel;

		m_InChat = false;

		// a game creation message is just a game refresh message with upTime = 0

		QueueGameRefresh( state, gameName, hostName, map, savegame, 0, hostCounter );
	}
}

void CBNET :: QueueGameRefresh( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *saveGame, uint32_t upTime, uint32_t hostCounter )
{
	if( hostName.empty( ) )
	{
		BYTEARRAY UniqueName = m_Protocol->GetUniqueName( );
		hostName = string( UniqueName.begin( ), UniqueName.end( ) );
    }

	if( m_LoggedIn && map )
	{
		// construct a fixed host counter which will be used to identify players from this realm
		// the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
		// the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
		// since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
		// when a player joins a game we can obtain the ID from the received host counter
		// note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

		uint32_t FixedHostCounter = ( hostCounter & 0x0FFFFFFF ) | ( m_HostCounterID << 28 );

		if( saveGame )
		{
			uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;

			// the state should always be private when creating a saved game

			if( state == GAME_PRIVATE )
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			MapWidth.push_back( 192 );
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );

			boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
			
			if( m_GHost->m_Reconnect )
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, upTime, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
			else
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), UTIL_CreateByteArray( (uint16_t)0, false ), UTIL_CreateByteArray( (uint16_t)0, false ), gameName, hostName, upTime, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
			
			packetsLock.unlock( );
		}
		else
		{
			uint32_t MapGameType = map->GetMapGameType( );
			MapGameType |= MAPGAMETYPE_UNKNOWN0;

			if( state == GAME_PRIVATE )
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			MapWidth.push_back( 192 );
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );
            
            //MapGameType = 21569664;
            MapGameType = m_GHost->m_MapGameType;

			boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
			
			if( m_GHost->m_Reconnect ) {
			      m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, upTime, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
			}
			else {
			      m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), map->GetMapWidth( ), map->GetMapHeight( ), gameName, hostName, upTime, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
            }
            
            packetsLock.unlock( );
		}
	}
}

void CBNET :: QueueGameUncreate( )
{
	boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
	
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_STOPADV( ) );
	
	packetsLock.unlock( );
}

void CBNET :: UnqueuePackets( unsigned char type )
{
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	boost::mutex::scoped_lock packetsLock( m_PacketsMutex );
	
	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet.size( ) >= 2 && Packet[1] == type )
                        ++Unqueued;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;
	
	packetsLock.unlock( );

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " packets of type " + UTIL_ToString( type ) );
}

void CBNET :: UnqueueChatCommand( string chatCommand )
{
	// hackhack: this is ugly code
	// generate the packet that would be sent for this chat command
	// then search the queue for that exact packet

	BYTEARRAY PacketToUnqueue = m_Protocol->SEND_SID_CHATCOMMAND( chatCommand );
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	boost::mutex::scoped_lock packetsLock( m_PacketsMutex );

	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet == PacketToUnqueue )
                        ++Unqueued;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;
	
	packetsLock.unlock( );

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " chat command packets" );
}

void CBNET :: UnqueueGameRefreshes( )
{
	UnqueuePackets( CBNETProtocol :: SID_STARTADVEX3 );
}

bool CBNET :: IsAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

        for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); ++i )
	{
		if( *i == name )
			return true;
	}

	return false;
}

bool CBNET :: IsRootAdmin( string name )
{
	// m_RootAdmin was already transformed to lower case in the constructor

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// updated to permit multiple root admins seperated by a space, e.g. "Varlock Kilranin Instinct121"
	// note: this function gets called frequently so it would be better to parse the root admins just once and store them in a list somewhere
	// however, it's hardly worth optimizing at this point since the code's already written

	stringstream SS;
	string s;
	SS << m_RootAdmin;

	while( !SS.eof( ) )
	{
		SS >> s;

		if( name == s )
			return true;
	}

	return false;
}

void CBNET :: AddAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	m_Admins.push_back( name );
}

void CBNET :: RemoveAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); )
	{
		if( *i == name )
			i = m_Admins.erase( i );
		else
                        ++i;
	}
}

void CBNET :: HoldFriends( CBaseGame *game )
{
	if( game )
	{
                for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); ++i )
			game->AddToReserved( (*i)->GetAccount( ) );
	}
}

void CBNET :: HoldClan( CBaseGame *game )
{
	if( game )
	{
                for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); ++i )
			game->AddToReserved( (*i)->GetName( ) );
	}
}
