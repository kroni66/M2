#include "stdafx.h"
#include <sstream>

#include "desc.h"
#include "party.h"
#include "char.h"
#include "questlua.h"
#include "questmanager.h"
#include "packet.h"
#include "char_manager.h"
#include "db.h"

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

namespace quest
{
	using namespace std;

	//
	// "party" Lua functions
	//
	ALUA(party_clear_ready)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
		{
			FPartyClearReady f;
			ch->GetParty()->ForEachNearMember(f);
		}
		return 0;
	}

	ALUA(party_get_max_level)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
			lua_pushnumber(L,ch->GetParty()->GetMemberMaxLevel());
		else
			lua_pushnumber(L, 1);

		return 1;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(party_get_min_level)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
			lua_pushnumber(L,ch->GetParty()->GetMemberMinLevel());
		else
			lua_pushnumber(L, 1);

		return 1;
	}

	ALUA(party_leave_party)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		// if (!ch->GetParty()&&!CPartyManager::instance().IsEnablePCParty()&&ch->GetDungeon())
			// return 0;

		LPPARTY pParty = ch->GetParty();
		if (pParty)
		{
			if (pParty->GetMemberCount() == 2)
				CPartyManager::instance().DeleteParty(pParty);
			else
				pParty->Quit(ch->GetPlayerID());
		}

		lua_pushboolean(L, ch->GetParty()==NULL);
		return 1;
	}

	ALUA(party_delete_party)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		// if (!ch->GetParty()&&!CPartyManager::instance().IsEnablePCParty()&&ch->GetDungeon()&&!ch->GetParty()->GetLeaderPID() == ch->GetPlayerID())
			// return 0;

		if (ch->GetParty() && ch->GetParty()->GetLeaderPID() == ch->GetPlayerID())
			CPartyManager::instance().DeleteParty(ch->GetParty());

		lua_pushboolean(L, ch->GetParty()==NULL);
		return 1;
	}
#endif

    struct FRunCinematicSender
    {
        std::string data;
        struct packet_script pack;

        FRunCinematicSender(const char* str)
        {
            data = "[RUN_CINEMA value;";
            data += str;
            data += "]";

            pack.header = HEADER_GC_SCRIPT;
            pack.skin = CQuestManager::QUEST_SKIN_CINEMATIC;
            //pack.skin = CQuestManager::QUEST_SKIN_NOWINDOW;
            pack.src_size = data.size();
            pack.size = pack.src_size + sizeof(struct packet_script);
        }

        void operator()(LPCHARACTER ch)
        {
            sys_log(0, "CINEMASEND_TRY %s", ch->GetName());

            if (ch->GetDesc())
            {
                sys_log(0, "CINEMASEND %s", ch->GetName());
                ch->GetDesc()->BufferedPacket(&pack, sizeof(struct packet_script));
                ch->GetDesc()->Packet(data.c_str(),data.size());
            }
        }
    };

	ALUA(party_run_cinematic)
	{
		if (!lua_isstring(L, 1))
			return 0;

		sys_log(0, "RUN_CINEMA %s", lua_tostring(L, 1));
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
		{
			FRunCinematicSender f(lua_tostring(L, 1));

			ch->GetParty()->Update();
			ch->GetParty()->ForEachNearMember(f);
		}

		return 0;
	}

	struct FCinematicSender
	{
		const char* str;
		struct ::packet_script packet_script;
		int len;

		FCinematicSender(const char* str)
			: str(str)
		{
			len = strlen(str);

			packet_script.header = HEADER_GC_SCRIPT;
			packet_script.skin = CQuestManager::QUEST_SKIN_CINEMATIC;
			packet_script.src_size = len;
			packet_script.size = packet_script.src_size + sizeof(struct packet_script);
		}

		void operator()(LPCHARACTER ch)
		{
			sys_log(0, "CINEMASEND_TRY %s", ch->GetName());

			if (ch->GetDesc())
			{
				sys_log(0, "CINEMASEND %s", ch->GetName());
				ch->GetDesc()->BufferedPacket(&packet_script, sizeof(struct packet_script));
				ch->GetDesc()->Packet(str,len);
			}
		}
	};

	ALUA(party_show_cinematic)
	{
		if (!lua_isstring(L, 1))
			return 0;

		sys_log(0, "CINEMA %s", lua_tostring(L, 1));
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
		{
			FCinematicSender f(lua_tostring(L, 1));

			ch->GetParty()->Update();
			ch->GetParty()->ForEachNearMember(f);
		}
		return 0;
	}

	ALUA(party_get_near_count)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty())
			lua_pushnumber(L, ch->GetParty()->GetNearMemberCount());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(party_syschat)
	{
		LPPARTY pParty = CQuestManager::Instance().GetCurrentCharacterPtr()->GetParty();

		if (pParty)
		{
			ostringstream s;
			combine_lua_string(L, s);

			FPartyChat f(CHAT_TYPE_INFO, s.str().c_str());

			pParty->ForEachOnlineMember(f);
		}

		return 0;
	}

	ALUA(party_is_leader)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty() && ch->GetParty()->GetLeaderPID() == ch->GetPlayerID())
			lua_pushboolean(L, 1);
		else
			lua_pushboolean(L, 0);

		return 1;
	}

	ALUA(party_is_party)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushboolean(L, ch->GetParty() ? 1 : 0);
		return 1;
	}

	ALUA(party_get_leader_pid)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (ch->GetParty())
		{
			lua_pushnumber(L, ch->GetParty()->GetLeaderPID());
		}
		else
		{
			lua_pushnumber(L, -1);
		}
		return 1;
	}


	ALUA(party_chat)
	{
		LPPARTY pParty = CQuestManager::Instance().GetCurrentCharacterPtr()->GetParty();

		if (pParty)
		{
			ostringstream s;
			combine_lua_string(L, s);

			FPartyChat f(CHAT_TYPE_TALKING, s.str().c_str());

			pParty->ForEachOnlineMember(f);
		}

		return 0;
	}


	ALUA(party_is_map_member_flag_lt)
	{

		if (!lua_isstring(L, 1) || !lua_isnumber(L, 2))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		CQuestManager& q = CQuestManager::Instance();
		LPPARTY pParty = q.GetCurrentCharacterPtr()->GetParty();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		PC* pPC = q.GetCurrentPC();

		const char* sz = lua_tostring(L,1);

		if (pParty)
		{
			FPartyCheckFlagLt f;
			f.flagname = pPC->GetCurrentQuestName() + "."+sz;
			f.value = (int) rint(lua_tonumber(L, 2));

			bool returnBool = pParty->ForEachOnMapMemberBool(f, ch->GetMapIndex());
			lua_pushboolean(L, returnBool);
		}

		return 1;
	}

	ALUA(party_set_flag)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch->GetParty() && lua_isstring(L, 1) && lua_isnumber(L, 2))
			ch->GetParty()->SetFlag(lua_tostring(L, 1), (int)lua_tonumber(L, 2));

		return 0;
	}

	ALUA(party_get_flag)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (!ch->GetParty() || !lua_isstring(L, 1))
			lua_pushnumber(L, 0);
		else
			lua_pushnumber(L, ch->GetParty()->GetFlag(lua_tostring(L, 1)));

		return 1;
	}

	ALUA(party_set_quest_flag)
	{
		CQuestManager & q = CQuestManager::instance();

		FSetQuestFlag f;

		f.flagname = q.GetCurrentPC()->GetCurrentQuestName() + "." + lua_tostring(L, 1);
		f.value = (int) rint(lua_tonumber(L, 2));

		LPCHARACTER ch = q.GetCurrentCharacterPtr();

		if (ch->GetParty())
			ch->GetParty()->ForEachOnlineMember(f);
		else
			f(ch);

		return 0;
	}

	ALUA(party_is_in_dungeon)
	{
		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		LPPARTY pParty = ch->GetParty();
		if (pParty != NULL){
			lua_pushboolean (L, pParty->GetDungeon() ? true : false);
			return 1;
		}
		lua_pushboolean (L, false);
		return 1;
	}

	struct FGiveBuff
	{
		DWORD dwType;
		BYTE bApplyOn;
		long lApplyValue;
		DWORD dwFlag;
		long lDuration;
		long lSPCost;
		bool bOverride;
		bool IsCube;

		FGiveBuff (DWORD _dwType, BYTE _bApplyOn, long _lApplyValue, DWORD _dwFlag, long _lDuration,
					long _lSPCost, bool _bOverride, bool _IsCube = false)
			: dwType (_dwType), bApplyOn (_bApplyOn), lApplyValue (_lApplyValue), dwFlag(_dwFlag), lDuration(_lDuration),
				lSPCost(_lSPCost), bOverride(_bOverride), IsCube(_IsCube)
		{}
		void operator () (LPCHARACTER ch)
		{
			ch->AddAffect(dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		}
	};

	// 파티 단위로 버프 주는 함수.
	// 같은 맵에 있는 파티원만 영향을 받는다.
	ALUA(party_give_buff)
	{
		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();

		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) ||
			!lua_isnumber(L, 5) || !lua_isnumber(L, 6) || !lua_isboolean(L, 7) || !lua_isboolean(L, 8))
		{
			lua_pushboolean (L, false);
			return 1;
		}
		DWORD dwType = lua_tonumber(L, 1);
		BYTE bApplyOn = lua_tonumber(L, 2);
		long lApplyValue = lua_tonumber(L, 3);
		DWORD dwFlag = lua_tonumber(L, 4);
		long lDuration = lua_tonumber(L, 5);
		long lSPCost = lua_tonumber(L, 6);
		bool bOverride = lua_toboolean(L, 7);
		bool IsCube = lua_toboolean(L, 8);

		FGiveBuff f (dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		if (ch->GetParty())
			ch->GetParty()->ForEachOnMapMember(f, ch->GetMapIndex());
		else
			f(ch);

		lua_pushboolean(L, true);
		return 1;
	}

	struct FPartyPIDCollector
	{
		std::vector <DWORD> vecPIDs;
		FPartyPIDCollector()
		{
		}
		void operator () (LPCHARACTER ch)
		{
			vecPIDs.push_back(ch->GetPlayerID());
		}
	};

	ALUA(party_get_member_pids)
	{
		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		LPPARTY pParty = ch->GetParty();
		if (NULL == pParty)
		{
			return 0;
		}
		FPartyPIDCollector f;
		pParty->ForEachOnMapMember(f, ch->GetMapIndex());

		for (std::vector <DWORD>::iterator it = f.vecPIDs.begin(); it != f.vecPIDs.end(); it++)
		{
			lua_pushnumber(L, *it);
		}
		return f.vecPIDs.size();
	}

	ALUA(party_get_leader_name)
	{
		std::string name = "";

		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		if (!ch)
		{
			lua_pushstring(L, name.c_str());
			return 1;
		}

		LPPARTY party = ch->GetParty();
		if (party)
		{
			int32_t pid = party->GetLeaderPID();
			LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(pid);
			name = leader != NULL ? leader->GetName() : ch->GetName();
		}
		else
		{
			name = ch->GetName();
		}

		lua_pushstring(L, name.c_str());
		return 1;
	}

	ALUA(party_give_gold)
	{
		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		if (!ch)
		{
			return 0;
		}

#ifdef ENABLE_LONG_LONG
		int64_t gold = (int64_t)lua_tonumber(L, -1);
#else
		int32_t gold = (int32_t)lua_tonumber(L, -1);
#endif

		LPPARTY party = ch->GetParty();
		if (party)
		{
			FPartyPIDCollector f;
			party->ForEachOnMapMember(f, ch->GetMapIndex());

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); it++)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && tch->IsPC())
				{
					if (gold + tch->GetGold() < 0)
					{
#ifdef ENABLE_LONG_LONG
						sys_err("QUEST wrong ChangeGold %lld (now %lld)", gold, tch->GetGold());
#else
						sys_err("QUEST wrong ChangeGold %d (now %d)", gold, tch->GetGold());
#endif
					}
					else
					{
						DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, tch->GetPlayerID(), gold);
						tch->PointChange(POINT_GOLD, gold, true);
					}
				}
			}

			if (!q.GetPC(ch->GetPlayerID()))
			{
				sys_err("cannot return to main.");
			}
		}
		else
		{
			if (gold + ch->GetGold() < 0)
			{
#ifdef ENABLE_LONG_LONG
				sys_err("QUEST wrong ChangeGold %lld (now %lld)", gold, ch->GetGold());
#else
				sys_err("QUEST wrong ChangeGold %d (now %d)", gold, ch->GetGold());
#endif
			}
			else
			{
				DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, ch->GetPlayerID(), gold);
				ch->PointChange(POINT_GOLD, gold, true);
			}
		}

		return 0;
	}

	ALUA(party_give_blacksmith)
	{
		CQuestManager & q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		if (!ch)
		{
			return 0;
		}

		LPPARTY party = ch->GetParty();
		if (party)
		{
			FPartyPIDCollector f;
			party->ForEachOnMapMember(f, ch->GetMapIndex());

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); it++)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && tch->IsPC())
				{
					tch->SetQuestFlag("deviltower_zone.can_refine", 1);
				}
			}

			if (!q.GetPC(ch->GetPlayerID()))
			{
				sys_err("cannot return to main.");
			}
		}
		else
		{
			ch->SetQuestFlag("deviltower_zone.can_refine", 1);
		}

		return 0;
	}

	void RegisterPartyFunctionTable()
	{
		luaL_reg party_functions[] =
		{
			{ "is_leader",		party_is_leader		},
			{ "is_party",		party_is_party		},
			{ "get_leader_pid",	party_get_leader_pid},
			{ "setf",			party_set_flag		},
			{ "getf",			party_get_flag		},
			{ "setqf",			party_set_quest_flag},
			{ "chat",			party_chat			},
			{ "syschat",		party_syschat		},
			{ "get_near_count",	party_get_near_count},
			{ "show_cinematic",	party_show_cinematic},
			{ "run_cinematic",	party_run_cinematic	},
			{ "get_max_level",	party_get_max_level	},
#ifdef ENABLE_NEWSTUFF
			{ "get_min_level",	party_get_min_level	},	// [return lua number]
			{ "leave_party",	party_leave_party	},	// [return lua boolean=successfulness]
			{ "delete_party",	party_delete_party	},	// [return lua boolean=successfulness]
#endif
			{ "clear_ready",	party_clear_ready	},
			{ "is_in_dungeon",	party_is_in_dungeon	},
			{ "give_buff",		party_give_buff		},
			{ "is_map_member_flag_lt",	party_is_map_member_flag_lt	},
			{ "get_member_pids",		party_get_member_pids	}, // 파티원들의 pid를 return
			{"get_leader_name", party_get_leader_name},
			{"give_gold", party_give_gold},
			{"give_blacksmith", party_give_blacksmith},
			{ NULL,				NULL				}
		};

		CQuestManager::instance().AddLuaFunctionTable("party", party_functions);
	}
}




