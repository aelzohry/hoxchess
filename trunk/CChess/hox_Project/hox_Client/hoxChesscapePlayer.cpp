/***************************************************************************
 *  Copyright 2007, 2008 Huy Phan  <huyphan@playxiangqi.com>               *
 *                                                                         * 
 *  This file is part of HOXChess.                                         *
 *                                                                         *
 *  HOXChess is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  HOXChess is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with HOXChess.  If not, see <http://www.gnu.org/licenses/>.      *
 ***************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// Name:            hoxChesscapePlayer.cpp
// Created:         12/12/2007
//
// Description:     The Chesscape LOCAL Player.
/////////////////////////////////////////////////////////////////////////////

#include "hoxChesscapePlayer.h"
#include "hoxUtil.h"
#include "hoxSite.h"
#include <wx/tokenzr.h>

IMPLEMENT_DYNAMIC_CLASS(hoxChesscapePlayer, hoxLocalPlayer)

BEGIN_EVENT_TABLE(hoxChesscapePlayer, hoxLocalPlayer)
	// *** VIP-NOTES: According to http://www.wxwidgets.org//manuals/stable/wx_eventhandlingoverview.html#eventhandlingoverview
	//     we must declare an entry in each derived-class's table-event (using virtual WILL NOT WORK)....
	//     However, it seems here that I do not have to do it.

    EVT_COMMAND(hoxREQUEST_PLAYER_DATA, hoxEVT_CONNECTION_RESPONSE, hoxChesscapePlayer::OnConnectionResponse_PlayerData)
    EVT_COMMAND(wxID_ANY, hoxEVT_CONNECTION_RESPONSE, hoxChesscapePlayer::OnConnectionResponse)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
// hoxChesscapePlayer
//-----------------------------------------------------------------------------

hoxChesscapePlayer::hoxChesscapePlayer()
{
    wxFAIL_MSG( "This default constructor is never meant to be used." );
}

hoxChesscapePlayer::hoxChesscapePlayer( const wxString& name,
                          hoxPlayerType   type,
                          int             score )
            : hoxLocalPlayer( name, type, score )
            , m_bRequestingLogin( false )
			, m_bRequestingNewTable( false )
			, m_bSentMyFirstMove( false )
{ 
    const char* FNAME = "hoxChesscapePlayer::hoxChesscapePlayer";
    wxLogDebug("%s: ENTER.", FNAME);
}

hoxChesscapePlayer::~hoxChesscapePlayer() 
{
    const char* FNAME = "hoxChesscapePlayer::~hoxChesscapePlayer";
    wxLogDebug("%s: ENTER.", FNAME);
}

hoxResult
hoxChesscapePlayer::ConnectToNetworkServer()
{
	const char* FNAME = "hoxChesscapePlayer::ConnectToNetworkServer";
	wxLogDebug("%s: ENTER.", FNAME);

    m_bRequestingLogin = true;

    return this->hoxLocalPlayer::ConnectToNetworkServer();
}

hoxResult 
hoxChesscapePlayer::QueryForNetworkTables()
{
	const char* FNAME = "hoxChesscapePlayer::QueryForNetworkTables";
	wxLogDebug("%s: ENTER.", FNAME);

	/* Clone the "cache" list and return the cloned */
    std::auto_ptr<hoxNetworkTableInfoList> pTableList( new hoxNetworkTableInfoList );

	for ( hoxNetworkTableInfoList::const_iterator it = m_networkTables.begin();
		                                          it != m_networkTables.end(); 
												++it )
	{
		pTableList->push_back( (*it) );
	}
	
    hoxSite* site = this->GetSite();
    site->DisplayListOfTables( *pTableList );

	return hoxRC_OK;
}

hoxResult 
hoxChesscapePlayer::JoinNetworkTable( const wxString& tableId )
{
	/* Make sure that the table is still there. */
	if ( ! _DoesTableExist( tableId ) ) // not found?
	{
		wxLogWarning("Table [%s] not longer exist.", tableId.c_str());
		return hoxRC_OK;  // *** Fine (due to time-delay).
	}

	m_pendingJoinTableId = tableId;

	return this->hoxLocalPlayer::JoinNetworkTable( tableId );
}

hoxResult 
hoxChesscapePlayer::OpenNewNetworkTable()
{
	if ( m_bRequestingNewTable )
	{
		wxLogWarning("A new Table is already being requested."); 
		return hoxRC_ERR;
	}

	m_bRequestingNewTable = true;

	return this->hoxLocalPlayer::OpenNewNetworkTable();
}

void 
hoxChesscapePlayer::OnRequest_FromTable( hoxRequest_APtr apRequest )
{
    const char* FNAME = "hoxChesscapePlayer::OnRequest_FromTable";

    switch ( apRequest->type )
    {
        case hoxREQUEST_MOVE:
        {
	        /* If this Play is playing and this is his first Move, then
	         * update his Status to "Playing".
	         */
	        if ( ! m_bSentMyFirstMove )
	        {
		        m_bSentMyFirstMove = true;

		        wxLogDebug("%s: Sending Player-Status on the 1st Move...", FNAME);
		        hoxRequest_APtr apStatusRequest( new hoxRequest( hoxREQUEST_PLAYER_STATUS, this ) );
		        apStatusRequest->parameters["status"] = "P";
		        this->AddRequestToConnection( apStatusRequest );
	        }
            break;
        }
        case hoxREQUEST_RESIGN:
        {
            const wxString tableId = apRequest->parameters["tid"];
	        hoxTable_SPtr pTable = this->GetSite()->FindTable( tableId );
            wxCHECK_RET(pTable.get() != NULL, "Table not found");

            hoxColor myRole = pTable->GetPlayerRole( this->GetName() );
            hoxGameStatus gameStatus = ( myRole == hoxCOLOR_RED
                                        ? hoxGAME_STATUS_BLACK_WIN 
                                        : hoxGAME_STATUS_RED_WIN );
            pTable->OnGameOver_FromNetwork( this, gameStatus );
            break;
        }
        case hoxREQUEST_UPDATE:
        {
	        /* Send the current color (role) along with the new timers.
	         */
            const wxString tableId = apRequest->parameters["tid"];
	        hoxTable_SPtr pTable = this->GetSite()->FindTable( tableId );
            wxCHECK_RET(pTable.get() != NULL, "Table not found");

            hoxColor myRole = pTable->GetPlayerRole( this->GetName() );
            apRequest->parameters["color"] = hoxUtil::ColorToString( myRole );
            break;
        }
        default:
            break; // Do nothing for other requests.
    }

    this->hoxPlayer::OnRequest_FromTable( apRequest );
}

void 
hoxChesscapePlayer::OnConnectionResponse_PlayerData( wxCommandEvent& event )
{
    const char* FNAME = "hoxChesscapePlayer::OnConnectionResponse_PlayerData";
    hoxResult result = hoxRC_OK;

    const hoxResponse_APtr response( wxDynamicCast(event.GetEventObject(), hoxResponse) );
	wxString command;
	wxString paramsStr;

    hoxSite* site = this->GetSite();

    /* Handle error-code. */

    if ( response->code != hoxRC_OK )
    {
        wxLogDebug("%s: *** WARN *** Received error-code [%s].", 
            FNAME, hoxUtil::ResultToStr(response->code));

        /* Close the connection and logout.
         */
        this->LeaveAllTables();
        this->DisconnectFromNetworkServer();
        site->Handle_ShutdownReadyFromPlayer();
        wxLogDebug("%s: END (exception).", FNAME);
        return;  // *** Exit immediately.
    }

	/* Parse for the command */

	if ( ! _ParseIncomingCommand( response->content, command, paramsStr ) ) // failed?
	{
		wxLogDebug("%s: *** WARN *** Failed to parse incoming command.", FNAME);
		return;  // *** Exit immediately.
	}

	/* Processing the command... */

    if      ( command == "login" )         _HandleCmd_Login( response, paramsStr );
    else if ( command == "code" )          _HandleCmd_Code( response, paramsStr );
    else if ( command == "logout" )        _HandleCmd_Logout( paramsStr );
    else if ( command == "clients" )       _HandleCmd_Clients( paramsStr );
	else if ( command == "show" )          _HandleCmd_Show( paramsStr );
	else if ( command == "unshow" )        _HandleCmd_Unshow( paramsStr );
	else if ( command == "update" )        _HandleCmd_Update( paramsStr );
	else if ( command == "updateRating" )  _HandleCmd_UpdateRating( paramsStr );
    else if ( command == "playerInfo" )    _HandleCmd_PlayerInfo( paramsStr );
	else if ( command == "tCmd" )	       _HandleTableCmd( paramsStr );
	else if ( command == "tMsg" )	       _HandleTableMsg( paramsStr );
	else
	{
		wxLogDebug("%s: * INFO * Ignore other command = [%s].", FNAME, command.c_str());
	}
}

bool 
hoxChesscapePlayer::_ParseTableInfoString( const wxString&      tableStr,
	                                       hoxNetworkTableInfo& tableInfo ) const
{
	const char* FNAME = "hoxChesscapePlayer::_ParseTableInfoString";
	wxString delims;
	delims += 0x10;
	// ... Do not return empty tokens
	wxStringTokenizer tkz( tableStr, delims, wxTOKEN_STRTOK );
	int tokenPosition = 0;
	wxString token;
	wxString debugStr;  // For Debug purpose only!

	tableInfo.Clear();

	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		debugStr << '[' << token << "]";
		switch (tokenPosition)
		{
			case 0: /* Table-Id */
				tableInfo.id = token; 
				break;

			case 1: /* Group: Public / Private */
				tableInfo.group = (  token == "0" 
					               ? hoxGAME_GROUP_PUBLIC 
								   : hoxGAME_GROUP_PRIVATE ); 
				break;

			case 2: /* Timer symbol 'T' */
				if ( token != "T" )
					wxLogDebug("%s: *** WARN *** This token [%s] should be 'T].", FNAME, token.c_str());
				break;

			case 3: /* Timer: Game-time (in seconds) */
				tableInfo.initialTime.nGame = ::atoi(token.c_str()) / 1000;
				break;

			case 4: /* Timer: Increment-time (in seconds) */
				tableInfo.initialTime.nFree = ::atoi(token.c_str()) / 1000;
				break;

			case 5: /* Table-type: Rated / Nonrated / Solo-Black / Solo-Red */
                tableInfo.gameType = _stringToGameType( token );
				break;

			case 6: /* Players-info */
			{
				_ParseTablePlayersString( token, tableInfo );
				break;
			}

			default: /* Ignore the rest. */ break;
		}
		++tokenPosition;
	}		
	wxLogDebug("%s: ... %s", FNAME, debugStr.c_str());

	/* Do special adjustment for Solo-typed games.
	 */
	if ( tableInfo.gameType == hoxGAME_TYPE_SOLO )
	{
		if ( ! tableInfo.redId.empty() && tableInfo.blackId.empty() )
		{
			tableInfo.blackId = "COMPUTER";
			tableInfo.blackScore = "0";
		}
		else if ( ! tableInfo.blackId.empty() && tableInfo.redId.empty() )
		{
			tableInfo.redId = "COMPUTER";
			tableInfo.redScore = "0";
		}
	}

	return true;
}

bool 
hoxChesscapePlayer::_ParseTablePlayersString( 
								const wxString&      playersInfoStr,
	                            hoxNetworkTableInfo& tableInfo ) const
{
	wxString delims( (wxChar) 0x20 );
	wxStringTokenizer tkz( playersInfoStr, delims, wxTOKEN_RET_EMPTY );
	
	wxString token;
	int      position = 0;
	long     score;

	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		switch ( position )
		{
			case 0:	
				tableInfo.redId = token; 
				if ( tableInfo.redId.empty() )
				{
					tableInfo.redScore = "0";
					position = 1;  // Skip RED 's Score.
				}
				break;

			case 1:
				if ( !token.empty() && ! token.ToLong( &score ) ) // not a number?
				{
					// The current token must be a part of the player's Id.
					tableInfo.redId += " " + token;
					--position;
					break;
				}
				tableInfo.redScore   = token; 
				break;

			case 2:
				if (   token.empty()
					&& !tableInfo.redId.empty() && tableInfo.redScore.empty() ) // RED is guest?
				{
					// Skip this empty token since it is a part of the RED Guest info.
					--position;
					break;
				}
				tableInfo.blackId = token; 
				break;

			case 3:	
				if ( !token.empty() && ! token.ToLong( &score ) ) // not a number?
				{
					// The current token must be a part of the player's Id.
					tableInfo.blackId += " " + token;
					--position;
					break;
				}
				tableInfo.blackScore = token; 
				break;

			default:
				break;
		}
		++position;
	}

	return true;
}

void
hoxChesscapePlayer::_ParsePlayerStatsString( const wxString&  sStatsStr,
	                                         hoxPlayerStats&  playerStats ) const
{
	wxString delims;
	delims += 0x10;
	// ... Do not return empty tokens
	wxStringTokenizer tkz( sStatsStr, delims, wxTOKEN_STRTOK );
	int tokenPosition = 0;
	wxString token;

	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		switch ( tokenPosition++ )
		{
			case 0: /* Id */
				playerStats.id = token; 
				break;

			case 1: /* Score */
                playerStats.score = ::atoi( token.c_str() );
				break;

			case 2: /* Completed Games: Ignored!!! */ break;
            case 3: /* Resigned Games: Ignored!!! */ break;

			case 4: /* Wins */
                playerStats.wins = ::atoi( token.c_str() );
				break;

			case 5: /* Losses */
                playerStats.losses = ::atoi( token.c_str() );
				break;

			case 6: /* Draws */
                playerStats.draws = ::atoi( token.c_str() );
				break;

			default: /* Ignore the rest. */ break;
		}
	}		
}

bool 
hoxChesscapePlayer::_DoesTableExist( const wxString& tableId ) const
{
	hoxNetworkTableInfo* pTableInfo = NULL;
	return _FindTableById( tableId, pTableInfo );
}

bool
hoxChesscapePlayer::_FindTableById( const wxString&       tableId,
		                            hoxNetworkTableInfo*& pTableInfo ) const
{
	for ( hoxNetworkTableInfoList::iterator it = m_networkTables.begin();
		                                    it != m_networkTables.end(); 
								          ++it )
	{
		if ( it->id == tableId )
		{
			pTableInfo = &(*it);
			return true;
		}
	}

	return false; // not found.
}

bool 
hoxChesscapePlayer::_AddTableToList( const wxString& tableStr ) const
{
	const char* FNAME = "hoxChesscapePlayer::_AddTableToList";
	hoxNetworkTableInfo tableInfo;

	if ( ! _ParseTableInfoString( tableStr,
	                              tableInfo ) )  // failed to parse?
	{
		wxLogDebug("%s: Failed to parse table-string [%s].", FNAME, tableStr.c_str());
		return false;
	}

	/* Insert into our list. */
	m_networkTables.push_back( tableInfo );

	return true;
}

bool 
hoxChesscapePlayer::_RemoveTableFromList( const wxString& tableId ) const
{
	const char* FNAME = "hoxChesscapePlayer::_RemoveTableFromList";

	for ( hoxNetworkTableInfoList::iterator it = m_networkTables.begin();
		                                    it != m_networkTables.end(); 
										  ++it )
	{
		if ( it->id == tableId )
		{
			m_networkTables.erase( it );
			return true;
		}
	}

	return false; // not found.
}

bool 
hoxChesscapePlayer::_UpdateTableInList( const wxString&      tableStr,
									    hoxNetworkTableInfo* pTableInfo /* = NULL */ )
{
	const char* FNAME = "hoxChesscapePlayer::_UpdateTableInList";
	hoxNetworkTableInfo tableInfo;

	if ( ! _ParseTableInfoString( tableStr,
	                              tableInfo ) )  // failed to parse?
	{
		wxLogDebug("%s: Failed to parse table-string [%s].", FNAME, tableStr.c_str());
		return false;
	}

	if ( pTableInfo != NULL )
	{
		*pTableInfo = tableInfo;  // Return a copy if requested.
	}

	/* Find the table from our list. */

	hoxNetworkTableInfoList::iterator found_it = m_networkTables.end();

	for ( hoxNetworkTableInfoList::iterator it = m_networkTables.begin();
		                                    it != m_networkTables.end(); 
								          ++it )
	{
		if ( it->id == tableInfo.id )
		{
			found_it = it;
			break;
		}
	}

	/* If the table is not found, insert into our list. */
	if ( found_it == m_networkTables.end() ) // not found?
	{
		wxLogDebug("%s: Insert a new table [%s].", FNAME, tableInfo.id.c_str());
		m_networkTables.push_back( tableInfo );
        
        hoxTable_SPtr pTable = _getMyTable();
        if ( pTable.get() != NULL )
        {
            wxString message;
            if ( ! tableInfo.redId.empty() ) 
            {
                message.Printf("open %s %s %s", tableInfo.id.c_str(), 
                    tableInfo.redId.c_str(), tableInfo.redScore.c_str());
            }
            else if ( ! tableInfo.blackId.empty() ) 
            {
                message.Printf("open %s %s %s", tableInfo.id.c_str(), 
                    tableInfo.blackId.c_str(), tableInfo.blackScore.c_str());
            }
            if ( !message.empty() )
                pTable->OnSystemMsg_FromNetwork( message );
        }
	}
	else // found?
	{
		wxLogDebug("%s: Update existing table [%s] with new info.", FNAME, tableInfo.id.c_str());
		*found_it = tableInfo;
	}

	/* Trigger our own event-handler */ 
	_OnTableUpdated( tableInfo );

	return true; // everything is fine.
}

void
hoxChesscapePlayer::_AddPlayerToList( const wxString& sPlayerId,
                                      const int       nPlayerScore ) const
{
    const char* FNAME = "hoxChesscapePlayer::_AddPlayerToList";

    hoxPlayerInfo_SPtr pPlayerInfo( new hoxPlayerInfo() );
    pPlayerInfo->id = sPlayerId;
    pPlayerInfo->score = nPlayerScore;

    wxLogDebug("%s: ... Added: [%s %d]", FNAME, sPlayerId.c_str(), nPlayerScore);
    m_networkPlayers.push_back( pPlayerInfo );

    /* Inform the Site. */
    hoxSite* site = this->GetSite();
    site->OnPlayerLoggedIn( sPlayerId, nPlayerScore );
}

void
hoxChesscapePlayer::_UpdatePlayerInList( const wxString& sPlayerId,
	                                     const int       nPlayerScore )
{
	/* Find the player from our list. */

	hoxPlayerInfoList::iterator found_it = m_networkPlayers.end();

	for ( hoxPlayerInfoList::iterator it = m_networkPlayers.begin();
		                              it != m_networkPlayers.end(); ++it )
	{
		if ( (*it)->id == sPlayerId )
		{
			found_it = it;
			break;
		}
	}

	/* If the player was found, update the info.
     * Otherwise, add as NEW.
     */
	if ( found_it != m_networkPlayers.end() ) // found?
	{
        (*found_it)->score = nPlayerScore;
    }
    else
    {
        _AddPlayerToList( sPlayerId, nPlayerScore );
    }
}

hoxPlayerInfo_SPtr
hoxChesscapePlayer::_FindPlayerById( const wxString& sPlayerId ) const
{
    hoxPlayerInfo_SPtr pPlayerInfo;  // Default "empty" pointer.

	for ( hoxPlayerInfoList::const_iterator it = m_networkPlayers.begin();
		                                    it != m_networkPlayers.end(); ++it )
	{
		if ( (*it)->id == sPlayerId )
		{
			pPlayerInfo = (*it);
			break;
		}
	}

    return pPlayerInfo;
}

void
hoxChesscapePlayer::_RemovePlayerFromList( const wxString& sPlayerId ) const
{
    hoxPlayerInfoList::iterator found_it = m_networkPlayers.end();

	for ( hoxPlayerInfoList::iterator it = m_networkPlayers.begin();
		                              it != m_networkPlayers.end(); ++it )
	{
		if ( (*it)->id == sPlayerId )
		{
			found_it = it;
			break;
		}
	}

    if ( found_it != m_networkPlayers.end() )  // found?
    {
        m_networkPlayers.erase( found_it );
    }
}

bool 
hoxChesscapePlayer::_ParseIncomingCommand( const wxString& contentStr,
										   wxString&       command,
										   wxString&       paramsStr ) const
{
	const char* FNAME = "hoxChesscapePlayer::_ParseIncomingCommand";

	/* CHECK: The first character must be 0x10 */
	if ( contentStr.empty() || contentStr[0] != 0x10 )
	{
		wxLogDebug("%s: *** WARN *** Invalid command = [%s].", FNAME, contentStr.c_str());
		return false;
	}

	/* Chop off the 1st character */
	const wxString actualContent = contentStr.Mid(1);

	/* Extract the command and its parameters-string */
	command = actualContent.BeforeFirst( '=' );
	paramsStr = actualContent.AfterFirst('=');

	return true;  // success
}

void 
hoxChesscapePlayer::_ParseLoginInfoString( const wxString& cmdStr,
                                           wxString&       name,
	                                       wxString&       score,
	                                       wxString&       role ) const
{
    const char* FNAME = "hoxChesscapePlayer::_ParseLoginInfoString";
    wxLogDebug("%s: ENTER.", FNAME);

	wxString delims;
	delims += 0x10;
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	int tokenPosition = 0;
	wxString token;
	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		switch (tokenPosition++)
		{
			case 0: name = token; break;
			case 1: score = token; break;
			case 2: role = token; break;
			default: /* Ignore the rest. */ break;
		}
	}		

	wxLogDebug("%s: .... name=[%s], score=[%s], role=[%s].", 
		FNAME, name.c_str(), score.c_str(), role.c_str());
}

bool 
hoxChesscapePlayer::_HandleCmd_Login( const hoxResponse_APtr& response,
                                      const wxString&         cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Login";
	wxLogDebug("%s: ENTER.", FNAME);

    hoxSite* site = this->GetSite();

    wxString  name;
    wxString  score;
    wxString  role;

    _ParseLoginInfoString( cmdStr, name, score, role );

    const int nScore = ::atoi( score.c_str() );

    if ( m_bRequestingLogin )  // LOGIN pending?
    {
        m_bRequestingLogin = false;
        wxASSERT( name == this->GetName() );

        response->code = hoxRC_OK;
        response->content.Printf("LOGIN is OK. name=[%s], score=[%d], role=[%s]", 
	        name.c_str(), nScore, role.c_str());
        this->SetScore( nScore );
        site->OnResponse_LOGIN( response );
    }
    else
    {
        /* Update our internal player-list. */
        _UpdatePlayerInList( name, nScore );
    }

    return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_Code( const hoxResponse_APtr& response,
                                     const wxString&         cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Code";
	wxLogDebug("%s: ENTER.", FNAME);

    hoxSite* site = this->GetSite();

    if ( m_bRequestingLogin )  // LOGIN pending?
    {
        m_bRequestingLogin = false;

	    wxLogDebug("%s: *** WARN *** LOGIN return code = [%s].", FNAME, cmdStr.c_str());
	    response->code = hoxRC_ERR;
	    response->content.Printf("LOGIN returns code = [%s].", cmdStr.c_str());
        site->OnResponse_LOGIN( response );
    }

    return true;
}

bool
hoxChesscapePlayer::_HandleCmd_Logout( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Logout";
	wxLogDebug("%s: ENTER.", FNAME);

    const wxString who = cmdStr.BeforeFirst( 0x10 );

    _RemovePlayerFromList( who );

    /* Inform the Site. */
    hoxSite* site = this->GetSite();
    site->OnPlayerLoggedOut( who );

    return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_Clients( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Clients";
	wxLogDebug("%s: ENTER.", FNAME);

	/* Clear out the existing table-list. */
	
	m_networkPlayers.clear();

	/* Create a new player-list. */

	wxString delims;
	delims += 0x10;   // delimiter
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	wxString token;
    int tokenPosition = 0;
    wxString sPlayerId;
    int      nPlayerScore = 0;
	
    while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
   		switch ( tokenPosition++ )
		{
        case 0:  /* Player-Id */
            sPlayerId = token;
            break;
        
        case 1:  /* Player-Score */
            nPlayerScore = ::atoi( token.c_str() );
            break;
        
        case 2:  /* Player-Status */
            tokenPosition = 0;  // Reset for the next player
            
            // TODO: Ignore the status for now!!!!

            _AddPlayerToList( sPlayerId, nPlayerScore );
            break;
        };
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_Show(const wxString& cmdStr)
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Show";
	wxLogDebug("%s: ENTER.", FNAME);

	/* Clear out the existing table-list. */
	
	m_networkTables.clear();

	/* Create a new table-list. */

	wxString delims;
	delims += 0x11;   // table-delimiter
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	wxString token;
	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		_AddTableToList( token );
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_Unshow(const wxString& cmdStr)
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Unshow";

	const wxString tableId = cmdStr.BeforeFirst(0x10);
	wxLogDebug("%s: Processing UNSHOW [%s] command...", FNAME, tableId.c_str());

	if ( ! _RemoveTableFromList( tableId ) ) // not found?
	{
		wxLogDebug("%s: *** WARN *** Table [%s] to be deleted NOT FOUND.", 
			FNAME, tableId.c_str());
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_Update( const wxString&      cmdStr,
									   hoxNetworkTableInfo* pTableInfo /* = NULL */)
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_Update";

	wxString updateCmd = cmdStr.BeforeFirst(0x10);

	// It is a table update if the sub-command starts with a number.
	long nTableId = 0;
	if ( updateCmd.ToLong( &nTableId ) && nTableId > 0 )  // a table's update?
	{
		wxString tableStr = cmdStr;
		wxLogDebug("%s: Processing UPDATE-(table) [%ld] command...", FNAME, nTableId);
		_UpdateTableInList( tableStr, pTableInfo );
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd";

	const wxString tCmd = cmdStr.BeforeFirst(0x10);
	wxString subCmdStr = cmdStr.AfterFirst(0x10);
    if (!subCmdStr.empty() && subCmdStr[subCmdStr.size()-1] == 0x10)
        subCmdStr = subCmdStr.substr(0, subCmdStr.size()-1);
	wxLogDebug("%s: Processing tCmd = [%s]...", FNAME, tCmd.c_str());
	
	if      ( tCmd == "Settings" ) return _HandleTableCmd_Settings( subCmdStr );
	else if ( tCmd == "Invite" )   return _HandleTableCmd_Invite( subCmdStr );

	/* NOTE: The Chesscape server only support 1 table for now. */

    hoxTable_SPtr pTable = _getMyTable();
	if ( pTable.get() == NULL )
	{
		wxLogDebug("%s: *** WARN *** This player [%s] not yet joined any table.", 
			FNAME, this->GetName().c_str());
		return false;
	}

	if      ( tCmd == "MvPts" )     _HandleTableCmd_PastMoves( pTable, subCmdStr );
	else if ( tCmd == "Move" )      _HandleTableCmd_Move( pTable, subCmdStr );
	else if ( tCmd == "GameOver" )  _HandleTableCmd_GameOver( pTable, subCmdStr );
	else if ( tCmd == "OfferDraw" ) _HandleTableCmd_OfferDraw( pTable );
    else if ( tCmd == "Clients" )   _HandleTableCmd_Clients( pTable, subCmdStr );
    else if ( tCmd == "Unjoin" )    _HandleTableCmd_Unjoin( pTable, subCmdStr );
	else
	{
		wxLogDebug("%s: *** Ignore this Table-command = [%s].", FNAME, tCmd.c_str());
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_Settings( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_Settings";
	wxString delims;
	delims += 0x10;
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	wxString token;
	int tokenPosition = 0;
	wxString redId;
	wxString blackId;
    hoxGameType gameType = hoxGAME_TYPE_UNKNOWN;
	long nInitialGameTime = 0;
	long nInitialFreeTime = 0;
	long nRedGameTime = 0;
	long nBlackGameTime = 0;

	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();

		switch ( tokenPosition++ )
		{
			case 0: /* Ignore for now */ break;
			case 1: /* Table-type: Rated / Nonrated / Solo-Black / Solo-Red */
            {
                gameType = _stringToGameType( token );
				break;
            }
			case 2:
			{
				if ( token != "T" )
					wxLogDebug("%s: *** WARN *** This token [%s] should be 'T].", FNAME, token.c_str());
				break;
			}

			case 3: /* Total game-time */
			{
				if ( ! token.ToLong( &nInitialGameTime ) || nInitialGameTime <= 0 )
				{
					wxLogDebug("%s: *** WARN *** Failed to parse TOTAL game-time [%s].", FNAME, token.c_str());
				}
				break;
			}

			case 4: /* Increment game-time */
			{
				// NOTE: Free (or Increment) time can be 0.
				if ( ! token.ToLong( &nInitialFreeTime ) )
				{
					wxLogDebug("%s: *** WARN *** Failed to parse Free game-time [%s].", FNAME, token.c_str());
				}
				break;
			}
			case 5: /* Ignore Red player's name */ 
				redId = token;
				if ( redId == " " ) redId = "";
				break;
			case 6:
			{
				if ( ! token.ToLong( &nRedGameTime ) || nRedGameTime <= 0 )
				{
					wxLogDebug("%s: *** WARN *** Failed to parse RED game-time [%s].", FNAME, token.c_str());
				}
				break;
			}
			case 7: /* Ignore Black player's name */ 
				blackId = token;
				if ( blackId == " " ) blackId = "";
				break;
			case 8:
			{
				if ( ! token.ToLong( &nBlackGameTime ) || nBlackGameTime <= 0 )
				{
					wxLogDebug("%s: *** WARN *** Failed to parse BLACK game-time [%s].", FNAME, token.c_str());
				}
				break;
			}

			default:
				/* Ignore the rest */ break;
		};
	}

	wxLogDebug("%s: Game-times for table [%s] = [%ld][%ld][%ld][%ld].", 
		FNAME, m_pendingJoinTableId.c_str(), nInitialGameTime, nInitialFreeTime,
		nRedGameTime, nBlackGameTime);

	/* Set the game times. 
	 * TODO: Ignore INCREMENT game-time for now.
	 */
	if (  ! m_pendingJoinTableId.empty() )
	{
		hoxNetworkTableInfo* pTableInfo = NULL;
		if ( ! _FindTableById( m_pendingJoinTableId, pTableInfo ) ) // not found?
		{
			wxLogDebug("%s: *** WARN *** Table [%s] not found.", 
				FNAME, m_pendingJoinTableId.c_str());
			return false;
		}

		m_pendingJoinTableId = "";

		pTableInfo->redId = redId;
		pTableInfo->blackId = blackId;
		if ( pTableInfo->gameType == hoxGAME_TYPE_SOLO )
		{
			if ( pTableInfo->blackId.empty() ) pTableInfo->blackId = "COMPUTER";
			if ( pTableInfo->redId.empty() )   pTableInfo->redId = "COMPUTER";
		}

        pTableInfo->gameType = gameType;

		pTableInfo->initialTime.nGame = (int) (nInitialGameTime / 1000 );
		pTableInfo->initialTime.nFree = (int) (nInitialFreeTime / 1000);

		pTableInfo->blackTime.nGame = (int) (nBlackGameTime / 1000); // convert to seconds
		pTableInfo->redTime.nGame   = (int) (nRedGameTime / 1000);

		pTableInfo->blackTime.nFree = pTableInfo->initialTime.nFree;
		pTableInfo->redTime.nFree   = pTableInfo->initialTime.nFree;

		// Inform the site.
		hoxSite* site = this->GetSite();
		site->JoinExistingTable( *pTableInfo );
	}

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_Invite( const wxString& cmdStr )
{
	const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER. cmdStr = [%s].", FNAME, cmdStr.c_str());

	const wxString sInvitorId = cmdStr.BeforeFirst(0x10);
	const wxString sTableId = cmdStr.AfterFirst(0x10);

    const wxString sMessage =
        wxString::Format("*INVITE from [%s] to join Table [%s].",
        sInvitorId.c_str(),
        sTableId.c_str());

    hoxTable_SPtr pTable = _getMyTable();

    if ( pTable.get() != NULL )
    {
        pTable->PostBoardMessage( sMessage );
    }
    else
    {
        ::wxMessageBox( sMessage,
                        _("Invitation from Player"),
                        wxOK | wxICON_INFORMATION,
                        NULL /* No parent */ );
    }

    return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_PastMoves( hoxTable_SPtr   pTable,
	                                           const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_PastMoves";
    hoxStringList moves;

    /* Get the list of Past Moves. */

	wxString delims;
	delims += 0x10;   // move-delimiter
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	wxString moveStr;
	while ( tkz.HasMoreTokens() )
	{
		moveStr = tkz.GetNextToken();
		wxLogDebug("%s: .... move-str=[%s].", FNAME, moveStr.c_str());
        moves.push_back( moveStr );
	}

	/* Inform our table... */
    pTable->OnPastMoves_FromNetwork( this, moves );

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_Move( hoxTable_SPtr   pTable,
	                                      const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_Move";
	wxString moveStr;
	wxString moveParam;

	/* Parse the command-string for the new Move */

	wxString delims;
	delims += 0x10;   // move-delimiter
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK ); // No empty tokens
	int tokenPosition = 0;
	wxString token;

	while ( tkz.HasMoreTokens() )
	{
		token = tkz.GetNextToken();
		switch ( tokenPosition++ )
		{
			case 0: /* Move-str */
				moveStr = token; break;

			case 1: /* Move-parameter */
				moveParam = token;	break;

			default: /* Ignore the rest. */ break;
		}
	}

	/* Inform the table of the new Move 
	 * NOTE: Regarding the Move- parameter, I do not know what it is yet.
	 */

	wxLogDebug("%s: Inform table of Move = [%s][%s].", FNAME, moveStr.c_str(), moveParam.c_str());
	pTable->OnMove_FromNetwork( this, moveStr );

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_GameOver( hoxTable_SPtr   pTable,
	                                          const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_GameOver";
	hoxGameStatus gameStatus;

	const wxString statusStr = cmdStr.BeforeFirst(0x10);

	if ( statusStr == "RED_WINS" )
	{
		gameStatus = hoxGAME_STATUS_RED_WIN;
	}
	else if ( statusStr == "BLK_WINS" )
	{
		gameStatus = hoxGAME_STATUS_BLACK_WIN;
	}
	else if ( statusStr == "DRAW" )
	{
		gameStatus = hoxGAME_STATUS_DRAWN;
	}
	else
	{
		wxLogDebug("%s: *** WARN *** Unknown Game-Over parameter [%s].", FNAME, statusStr);
		return false;
	}

	wxLogDebug("%s: Inform table of Game-Status [%d].", FNAME, (int) gameStatus);
	pTable->OnGameOver_FromNetwork( this, gameStatus );

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_OfferDraw( hoxTable_SPtr pTable )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_OfferDraw";

	/* Make sure that this Player is playing... */

	hoxPlayer* whoOffered = NULL;  // Who offered draw?
	hoxPlayer* blackPlayer = pTable->GetBlackPlayer();
	hoxPlayer* redPlayer = pTable->GetRedPlayer();

	if      ( blackPlayer == this )  whoOffered = redPlayer;
	else if ( redPlayer   == this )  whoOffered = blackPlayer;

	if ( whoOffered == NULL )
	{
		wxLogDebug("%s: *** WARN *** No real Player is offering this Draw request.", FNAME);
		return false;
	}

	wxLogDebug("%s: Inform table of player [%s] is offering Draw-Request.", 
		FNAME, whoOffered->GetName().c_str());
	pTable->OnDrawRequest_FromNetwork( whoOffered );

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_Clients( hoxTable_SPtr   pTable,
	                                         const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_Clients";

	wxString delims;
	delims += 0x10;
	// ... Do not return empty tokens
	wxStringTokenizer tkz( cmdStr, delims, wxTOKEN_STRTOK );
	int tokenPosition = 0;
	wxString sPlayerId;
    hoxPlayerInfo_SPtr pPlayerInfo;

	while ( tkz.HasMoreTokens() )
	{
		sPlayerId = tkz.GetNextToken();

        const hoxColor currentRole = pTable->GetPlayerRole( sPlayerId );
        if ( currentRole != hoxCOLOR_UNKNOWN )
            continue;

        pPlayerInfo = _FindPlayerById( sPlayerId );
        if ( pPlayerInfo.get() == NULL )
            continue;

        const wxString tableId = pTable->GetId();
        const hoxColor joinColor = hoxCOLOR_NONE;
        hoxSite* site = this->GetSite();

        wxLogDebug("%s: Player [%s] joined Table [%s] as [%d].", FNAME, 
            sPlayerId.c_str(), tableId.c_str(), joinColor);

        hoxResult result = site->OnPlayerJoined( tableId, 
                                                 sPlayerId, 
                                                 pPlayerInfo->score,
                                                 joinColor );
        if ( result != hoxRC_OK )
        {
            wxLogDebug("%s: *** ERROR *** Failed to ask table to join as color [%d].", 
                FNAME, joinColor);
            // *** break;
        }
	}		

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableCmd_Unjoin( hoxTable_SPtr   pTable,
	                                        const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableCmd_Unjoin";

	const wxString sPlayerId = cmdStr;
    const int      nPlayerScore = 1500;  // *** FIXME

    hoxSite* site = this->GetSite();
    hoxPlayer* player = site->GetPlayerById( sPlayerId, 
                                             nPlayerScore );
    wxCHECK_MSG(player != NULL, false, "Unexpected NULL player");

	wxLogDebug("%s: Inform table that [%s] just left.", FNAME, sPlayerId.c_str());
    pTable->OnLeave_FromNetwork( player );

	return true;
}

bool 
hoxChesscapePlayer::_HandleTableMsg( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleTableMsg";

	/* NOTE: The Chesscape server only support 1 table for now. */

    hoxTable_SPtr pTable = _getMyTable();
	if ( pTable.get() == NULL )
	{
		wxLogDebug("%s: *** WARN *** This player [%s] not yet joined any table.", 
			FNAME, this->GetName().c_str());
		return false;
	}

	/* Inform the Table of the new message. */

	const wxString whoSent = cmdStr.AfterFirst('<').BeforeFirst('>');
	const wxString message = cmdStr.AfterFirst(' ').BeforeFirst(0x10);

	pTable->OnMessage_FromNetwork( whoSent, message );

	return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_UpdateRating( const wxString& cmdStr )
{
	const char* FNAME = "hoxChesscapePlayer::_HandleCmd_UpdateRating";

	/* TODO: Update THIS Player's rating only. */

	const wxString who = cmdStr.BeforeFirst( 0x10 );
	const wxString rating = cmdStr.AfterFirst( 0x10 ).BeforeLast( 0x10 );
    const int nScore = ::atoi( rating.c_str() );

	if ( who == this->GetName() )
	{
		this->SetScore( nScore );
	}

    /* Update our internal player-list. */

    _UpdatePlayerInList( who, nScore );

	return true;
}

bool 
hoxChesscapePlayer::_HandleCmd_PlayerInfo( const wxString& cmdStr )
{
	const char* FNAME = __FUNCTION__;
	wxLogDebug("%s: ENTER. [%s].", FNAME, cmdStr.c_str());

    hoxPlayerStats playerStats;

    _ParsePlayerStatsString( cmdStr, playerStats );

    /* Display the player's statictics on the Board. */

    hoxTable_SPtr pTable = _getMyTable();
	if ( pTable.get() != NULL )
	{
        const wxString sMessage = wxString::Format("*INFO: %s %d W%d D%d L%d",
            playerStats.id.c_str(),
            playerStats.score,
            playerStats.wins, playerStats.draws, playerStats.losses);
            
        pTable->PostBoardMessage( sMessage );
	}

    return true;
}

void 
hoxChesscapePlayer::_OnTableUpdated( const hoxNetworkTableInfo& tableInfo )
{
	const char* FNAME = "hoxChesscapePlayer::_OnTableUpdated";
	hoxResult result;

    hoxSite* site = this->GetSite();

	/* If this Player is requesting for a NEW table, then check if the input
	 * table is a "NEW-EMPTY" table.
	 */
	if (   m_bRequestingNewTable
		&& tableInfo.redId.empty() && tableInfo.blackId.empty() )
	{
		wxLogDebug("%s: Received table [%s] as this Player's NEW table.", 
			FNAME, tableInfo.id.c_str());
		m_bRequestingNewTable = false;

		// Inform the Site of the response.
        hoxNetworkTableInfo newInfo( tableInfo );
        if ( newInfo.redTime.IsEmpty() ) newInfo.redTime = newInfo.initialTime;
        if ( newInfo.blackTime.IsEmpty() ) newInfo.blackTime = newInfo.initialTime;
		site->JoinNewTable( newInfo );
		return;  // *** Done.
	}

	/* Check if this is MY table. */

    hoxTable_SPtr pTable = this->FindTable( tableInfo.id );

    if ( pTable.get() == NULL ) // not found?
	{
		return;  // Not my table. Fine. Do nothing.
	}

    /* Update the Table with the new Rated/Non-Rated Game and Timers. */

    const bool bRatedGame = (tableInfo.gameType  == hoxGAME_TYPE_RATED);
    pTable->OnUpdate_FromPlayer( this,
                                 bRatedGame, tableInfo.initialTime );

	/* Find out if any new Player just "sit" at the Table. */

	hoxPlayer* currentRedPlayer = pTable->GetRedPlayer();
	hoxPlayer* currentBlackPlayer = pTable->GetBlackPlayer();
	const wxString currentRedId = currentRedPlayer ? currentRedPlayer->GetName() : "";
	const wxString currentBlackId = currentBlackPlayer ? currentBlackPlayer->GetName() : "";
	bool bSitRed   = false;  // A new Player just 'sit' as RED.
	bool bSitBlack = false;  // A new Player just 'sit' as BLACK.
    bool bUnsitRed   = false;  // The RED Player just 'unsit'.
    bool bUnsitBlack = false;  // The BLACK Player just 'unsit'.

	bSitRed   = ( ! tableInfo.redId.empty()   && tableInfo.redId   != currentRedId );
	bSitBlack = ( ! tableInfo.blackId.empty() && tableInfo.blackId != currentBlackId );

    bUnsitRed   = ( currentRedPlayer != NULL   && tableInfo.redId.empty() );
    bUnsitBlack = ( currentBlackPlayer != NULL && tableInfo.blackId.empty() );

	/* New 'SIT' event. */

	hoxPlayer* newRedPlayer = NULL;
	hoxPlayer* newBlackPlayer = NULL;

	if ( bSitRed )
	{
        newRedPlayer = site->GetPlayerById( tableInfo.redId,
			                                ::atoi( tableInfo.redScore.c_str() ) ); 
		result = newRedPlayer->JoinTableAs( pTable, hoxCOLOR_RED );
		wxASSERT( result == hoxRC_OK  );
	}
	if ( bSitBlack )
	{
        newBlackPlayer = site->GetPlayerById( tableInfo.blackId,
		                                      ::atoi( tableInfo.blackScore.c_str() ) );
		result = newBlackPlayer->JoinTableAs( pTable, hoxCOLOR_BLACK );
		wxASSERT( result == hoxRC_OK  );
	}

	/* New 'UNSIT' event. */

	if ( bUnsitRed )
	{
		result = currentRedPlayer->JoinTableAs( pTable, hoxCOLOR_NONE );
		wxASSERT( result == hoxRC_OK  );
	}
	if ( bUnsitBlack )
	{
		result = currentBlackPlayer->JoinTableAs( pTable, hoxCOLOR_NONE );
		wxASSERT( result == hoxRC_OK  );
	}
}

hoxGameType
hoxChesscapePlayer::_stringToGameType( const wxString& sInput ) const
{
    if      ( sInput == "0" ) return hoxGAME_TYPE_RATED;
    else if ( sInput == "1" ) return hoxGAME_TYPE_NONRATED;
    else if ( sInput == "4"  /* This is the Depth (level of difficulty) */
	       || sInput == "5"
           || sInput == "6"
	       || sInput == "7"
	       || sInput == "8" ) return hoxGAME_TYPE_SOLO;

    return /* unknown */ hoxGAME_TYPE_UNKNOWN;
}

hoxTable_SPtr
hoxChesscapePlayer::_getMyTable() const
{
	/* NOTE: The Chesscape server only support 1 table for now. */

    hoxTable_SPtr pTable = this->GetFrontTable();

    return pTable;
}

void 
hoxChesscapePlayer::OnConnectionResponse( wxCommandEvent& event )
{
    const char* FNAME = "hoxChesscapePlayer::OnConnectionResponse";
    wxLogDebug("%s: ENTER.", FNAME);

    hoxResponse* response_raw = wx_reinterpret_cast(hoxResponse*, event.GetEventObject());
    std::auto_ptr<hoxResponse> response( response_raw ); // take care memory leak!

    hoxSite* site = this->GetSite();

    const wxString sType = hoxUtil::RequestTypeToString(response->type);

    switch ( response->type )
	{
		case hoxREQUEST_JOIN:      /* fall through */
		case hoxREQUEST_NEW:       /* fall through */
		case hoxREQUEST_LEAVE:     /* fall through */
		case hoxREQUEST_OUT_DATA:
		{
			wxLogDebug("%s: [%s] 's response received. END.", FNAME, sType.c_str());
			break;
		}
		default:
			wxLogDebug("%s: *** WARN *** Unsupported Request [%s].", FNAME, sType.c_str());
			break;
	} // switch


	/* Post event to the sender if it is THIS player */

    if ( response->sender && response->sender != this )
    {
        wxEvtHandler* sender = response->sender;
        response.release();
        wxPostEvent( sender, event );
    }

    wxLogDebug("%s: END.", FNAME);
}

/************************* END OF FILE ***************************************/