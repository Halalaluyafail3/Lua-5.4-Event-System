#include<stdio.h>
#include<string.h>
#include<stddef.h>
#include<stdbool.h>
#include"EventSystem.h"
#include"Includes.h"
/* Connection system for Lua 5.4 */
#define E_CONNECTION_NAME "Connection"
/*
Connection uses 3 bools to store its state, a pointer for a string, and a size_t for the string length
IsConnected - whether the Connection is connected
IsWaitingToDisconnect - whether the Connection is going to be disconnected (it disconnected during it being fired)
IsRunning - whether the Connection is running
DebugStrLen - the length of DebugStr
DebugStr - the traceback of where the connection was connected
uservalue 1 - function
uservalue 2 - next Connection
uservalue 3 - previous Connection
uservalue 4 - Event
*/
typedef struct Connection Connection;
struct Connection{
	size_t DebugStrLen;
	bool IsConnected:1;
	bool IsWaitingToDisconnect:1;
	bool IsRunning:1;
	char DebugStr[];
};
#define E_EVENT_NAME "Event"
/*
Event uses its 1 bool of data to represent whether it is firing or not
IsRunning - whether the Event is running
uservalue 1 - first connection
uservalue 2 - first waiting thread
*/
typedef struct Event Event;
struct Event{
	bool IsRunning:1;
};
#define E_WTHREAD_NAME "WThread"
/*
WThread uses its 2 bools of data to represent its state, whether it's being run, and whether it should close the thread upon erroring
ShouldCloseOnErr - detemines if the thread is closed when the thread that is resumed errors
IsRunning - whether the thread is running
uservalue 1 - next WThread
uservalue 2 - previous WThread
uservalue 3 - the thread
*/
typedef struct WThread WThread;
struct WThread{
	bool ShouldCloseOnErr:1;
	bool IsRunning:1;
};
static void PrintErrMessage(lua_State *L){
	switch(lua_type(L,-1)){
		case LUA_TSTRING:{
			size_t StrLen;
			const char *Str=lua_tolstring(L,-1,&StrLen);
			fwrite(Str,1,StrLen,stderr);
			break;
		}
		case LUA_TNUMBER:{
			if(lua_isinteger(L,-1)){
				fprintf(stderr,LUA_INTEGER_FMT,lua_tointeger(L,-1));
			}else{
				fprintf(stderr,LUA_NUMBER_FMT,lua_tonumber(L,-1));
			}
			break;
		}
		case LUA_TNIL:{
			fputs("nil",stderr);
			break;
		}
		case LUA_TBOOLEAN:{
			if(lua_toboolean(L,-1)){
				fputs("true",stderr);
			}else{
				fputs("false",stderr);
			}
			break;
		}
		default:{
			int TT=luaL_getmetafield(L,-1,"__name");
			if(TT==LUA_TSTRING){
				size_t StrLen;
				const char *Str=lua_tolstring(L,-1,&StrLen);
				fwrite(Str,1,StrLen,stderr);
				lua_pop(L,1);
				fprintf(stderr,": %p",lua_topointer(L,-1));
			}else{
				if(TT!=LUA_TNIL){
					lua_pop(L,1);
				}
				fprintf(stderr,"%s: %p",luaL_typename(L,-1),lua_topointer(L,-1));
			}
			break;
		}
	}
}
static int EIsConnected(lua_State *L){
	Connection *Con=luaL_checkudata(L,1,E_CONNECTION_NAME);
	lua_pushboolean(L,Con->IsConnected&&!Con->IsWaitingToDisconnect);
	return 1;
}
static int EConnect(lua_State *L){
	luaL_checkudata(L,1,E_EVENT_NAME);
	luaL_checktype(L,2,LUA_TFUNCTION);
	luaL_traceback(L,L,0,0);
	size_t StrLen;
	const char *Str=lua_tolstring(L,-1,&StrLen);
	Connection *Con=lua_newuserdatauv(L,offsetof(Connection,DebugStr)+StrLen+1,4);
	Con->IsConnected=1;
	Con->IsWaitingToDisconnect=0;
	Con->IsRunning=0;
	Con->DebugStrLen=StrLen;
	char *Debug=Con->DebugStr;
	memcpy(Debug,Str,StrLen+1);
	lua_remove(L,-2);
	lua_pushvalue(L,lua_upvalueindex(1));
	lua_setmetatable(L,-2);
	lua_pushvalue(L,2);
	lua_setiuservalue(L,-2,1); /* set function */
	/* current next */
	if(lua_getiuservalue(L,1,1)!=LUA_TNIL){ /* has next */
		lua_pushvalue(L,-1);
		lua_setiuservalue(L,-3,2); /* set next */
		lua_pushvalue(L,-2);
		lua_setiuservalue(L,-2,3); /* set next's previous */
	}
	lua_pop(L,1);
	lua_pushvalue(L,-1);
	lua_setiuservalue(L,1,1); /* set start */
	lua_pushvalue(L,1);
	lua_setiuservalue(L,-2,4); /* set event */
	return 1;
}
static void Disconnect(lua_State *L,int Idx){
	Idx=lua_absindex(L,Idx);
	Connection *Con=lua_touserdata(L,Idx);
	if(Con->IsConnected){
		if(Con->IsRunning){
			Con->IsWaitingToDisconnect=1;
		}else{
			Con->IsConnected=0;
			if(lua_getiuservalue(L,Idx,3)!=LUA_TNIL){ /* has previous */
				if(lua_getiuservalue(L,Idx,2)!=LUA_TNIL){ /* has previous and next */
					lua_pushvalue(L,-1);
					lua_setiuservalue(L,-3,2); /* set previous's next */
					lua_pushvalue(L,-2);
					lua_setiuservalue(L,-2,3); /* set next's previous */
					lua_pushnil(L);
					lua_pushnil(L);
					lua_setiuservalue(L,Idx,2); /* remove next */
					lua_setiuservalue(L,Idx,3); /* remove previous */
					lua_pop(L,2);
				}else{ /* has previous and no next */
					lua_setiuservalue(L,-2,2); /* remove previous's next */
					lua_pushnil(L);
					lua_setiuservalue(L,Idx,3); /* remove previous */
					lua_pop(L,1);
				}
			}else{ /* no previous */
				if(lua_getiuservalue(L,Idx,2)!=LUA_TNIL){ /* no previous but has a next */
					lua_getiuservalue(L,Idx,4);
					lua_pushvalue(L,-2);
					lua_setiuservalue(L,-2,1); /* new start */
					lua_pushnil(L);
					lua_setiuservalue(L,-3,3); /* remove next's previous */
					lua_pushnil(L);
					lua_setiuservalue(L,Idx,2); /* remove next */
					lua_pop(L,3);
				}else{ /* no previous and no next */
					lua_getiuservalue(L,Idx,4);
					lua_pushnil(L);
					lua_setiuservalue(L,-2,1); /* remove start */
					lua_pop(L,3);
				}
			}
		}
	}
}
static int EDisconnect(lua_State *L){
	luaL_checkudata(L,1,E_CONNECTION_NAME);
	Disconnect(L,1);
	return 0;
}
static int EReconnect(lua_State *L){
	Connection *Con=luaL_checkudata(L,1,E_CONNECTION_NAME);
	if(!Con->IsConnected){
		Con->IsConnected=1;
		lua_getiuservalue(L,1,4);
		if(lua_getiuservalue(L,-1,1)!=LUA_TNIL){ /* has next */
			lua_pushvalue(L,1);
			lua_setiuservalue(L,-2,3); /* set next's previous */
			lua_setiuservalue(L,1,2); /* set next */
		}else{
			lua_pop(L,1);
		}
		lua_pushvalue(L,1);
		lua_setiuservalue(L,-2,1); /* new start */
	}else if(Con->IsWaitingToDisconnect){
		Con->IsWaitingToDisconnect=0;
	}
	return 0;
}
static int ENewEvent(lua_State *L){
	Event *Event=lua_newuserdatauv(L,sizeof(Event),2);
	Event->IsRunning=0;
	lua_pushvalue(L,lua_upvalueindex(1));
	lua_setmetatable(L,-2);
	return 1;
}
static int EWait(lua_State *L){
	luaL_checkudata(L,1,E_EVENT_NAME);
	luaL_argcheck(L,lua_isboolean(L,2)||lua_isnoneornil(L,2),2,"boolean, nil, or none expected");
	bool ShouldCloseOnErr=!lua_isnone(L,2)&&lua_toboolean(L,2);
	WThread *Waiting=lua_newuserdatauv(L,sizeof(WThread),3);
	Waiting->IsRunning=0;
	Waiting->ShouldCloseOnErr=ShouldCloseOnErr;
	lua_pushvalue(L,lua_upvalueindex(1));
	lua_setmetatable(L,-2);
	if(lua_getiuservalue(L,1,2)!=LUA_TNIL){ /* has next */
		lua_pushvalue(L,-2);
		lua_setiuservalue(L,-2,2); /* set next's previous */
		lua_setiuservalue(L,-2,1); /* set next */
	}else{
		lua_pop(L,1);
	}
	lua_pushthread(L);
	lua_setiuservalue(L,-2,3); /* set thread */
	lua_setiuservalue(L,1,2); /* new start */
	lua_settop(L,0);
	return lua_yield(L,0);
}
static int ConnectionErrHandler(lua_State *L){
	lua_settop(L,1);
	fputs("| Error message (Connection):\n",stderr);
	PrintErrMessage(L);
	fputs("\n| Traceback:\n",stderr);
	luaL_traceback(L,L,0,1);
	size_t StrLen;
	const char *Str=lua_tolstring(L,2,&StrLen);
	fwrite(Str,1,StrLen,stderr);
	fputc('\n',stderr);
	return 0;
}
static int EFire(lua_State *L){
	Event *Running=luaL_checkudata(L,1,E_EVENT_NAME);
	int Top=lua_gettop(L);
	int MTop=Top-1;
	int PTop=Top+1;
	int ArgTop=Top+2;
	luaL_checkstack(L,ArgTop,"too many arguments");
	bool NotRunning=!Running->IsRunning;
	if(NotRunning){
		Running->IsRunning=1;
	}
	/* event, args, err_handler, connection, function, args */
	lua_pushcfunction(L,ConnectionErrHandler);
	lua_getiuservalue(L,1,1);
	while(lua_type(L,-1)!=LUA_TNIL){
		Connection *Con=lua_touserdata(L,-1);
		if(!Con->IsWaitingToDisconnect){
			bool NotRunningCon=!Con->IsRunning;
			if(NotRunningCon){
				Con->IsRunning=1;
			}
			lua_getiuservalue(L,-1,1);
			for(int Idx=2;Idx<=Top;++Idx){
				lua_pushvalue(L,Idx);
			}
			int Err=lua_pcall(L,MTop,0,PTop);
			lua_settop(L,ArgTop);
			if(Err==LUA_ERRRUN){
				fputs("| Connection Point:\n",stderr);
				fwrite(Con->DebugStr,1,Con->DebugStrLen,stderr);
				fputs("\n| End\n",stderr);
			}
			if(NotRunningCon){
				Con->IsRunning=0;
				if(Con->IsWaitingToDisconnect){
					Con->IsWaitingToDisconnect=0;
					lua_getiuservalue(L,-1,2);
					lua_insert(L,-2);
					Disconnect(L,-1);
					lua_settop(L,ArgTop);
				}else{
					lua_getiuservalue(L,-1,2);
					lua_remove(L,-2);
				}
			}else{
				lua_getiuservalue(L,-1,2);
				lua_remove(L,-2);
			}
		}else{
			lua_getiuservalue(L,-1,2);
			lua_remove(L,-2);
		}
	}
	lua_settop(L,Top);
	/* event, args, WThread, thread, args */
	lua_getiuservalue(L,1,2);
	while(lua_type(L,-1)!=LUA_TNIL){
		WThread *Waiting=lua_touserdata(L,-1);
		if(Waiting->IsRunning){
			break;
		}
		Waiting->IsRunning=1;
		lua_getiuservalue(L,-1,3);
		lua_State *Thread=lua_tothread(L,-1);
		if(lua_checkstack(Thread,MTop)){
			for(int Idx=2;Idx<=Top;++Idx){
				lua_pushvalue(L,Idx);
			}
			lua_xmove(L,Thread,MTop);
			int NRes;
			int Res=lua_resume(Thread,L,MTop,&NRes);
			if(Res!=LUA_OK&&Res!=LUA_YIELD){
				lua_xmove(Thread,L,1);
				fputs("| Error Message (Wait Resume):\n",stderr);
				PrintErrMessage(L);
				fputs("\n| Traceback:\n",stderr);
				luaL_traceback(L,Thread,0,0);
				size_t StrLen;
				const char *Str=lua_tolstring(L,-1,&StrLen);
				fwrite(Str,1,StrLen,stderr);
				fputs("\n| Fire Point:\n",stderr);
				luaL_traceback(L,L,0,0);
				size_t StrLen2;
				const char *Str2=lua_tolstring(L,-1,&StrLen2);
				fwrite(Str2,1,StrLen2,stderr);
				if(Waiting->ShouldCloseOnErr){
					/* pass error object ???? (so __close metemethod can see it) */
					if(lua_resetthread(Thread)!=LUA_OK){ /* should i really warn for this??? */
						lua_xmove(Thread,L,1);
						fputs("\n| Error closing thread:\n",stderr);
						PrintErrMessage(L);
					}
				}
				fputs("\n| End\n",stderr);
			}else{
				lua_pop(Thread,NRes);
			}
		}else{
			fputs("| Too many arguments to resume thread (Wait Resume)\n",stderr);
		}
		Waiting->IsRunning=0;
		lua_settop(L,PTop);
		if(lua_getiuservalue(L,-1,2)!=LUA_TNIL){ /* has previous */
			if(lua_getiuservalue(L,-2,1)!=LUA_TNIL){ /* has previous and next */
				lua_pushvalue(L,-1);
				lua_setiuservalue(L,-3,1); /* set previous's next */
				lua_pushvalue(L,-2);
				lua_setiuservalue(L,-2,2); /* set next's previous */
				lua_insert(L,-3);
				lua_pushnil(L);
				lua_setiuservalue(L,-3,1); /* remove next */
				lua_pushnil(L);
				lua_setiuservalue(L,-3,2); /* remove previous */
				lua_pop(L,2);
			}else{ /* has previous but no next */
				lua_setiuservalue(L,-2,1); /* remove previous's next */
				lua_pushnil(L);
				lua_setiuservalue(L,-3,2); /* remove previous */
				lua_pop(L,2);
				lua_pushnil(L);
			}
		}else{ /* no previous */
			if(lua_getiuservalue(L,-2,1)!=LUA_TNIL){ /* has next but no previous */
				lua_pushnil(L);
				lua_setiuservalue(L,-2,2); /* remove next's previous */
				lua_pushvalue(L,-1);
				lua_setiuservalue(L,1,2); /* new start */
				lua_insert(L,-3);
				lua_setiuservalue(L,-2,1); /* remove next */
				lua_pop(L,1);
			}else{ /* has no previous or next */
				lua_setiuservalue(L,1,2); /* remove start */
				lua_remove(L,-2);
			}
		}
	}
	if(NotRunning){
		Running->IsRunning=0;
	}
	return 0;
}
void LoadEventLib(lua_State *L){
	lua_createtable(L,0,2); /* Connections metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_CONNECTION_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_CONNECTION_NAME);
	lua_pushcclosure(L,EConnect,1);
	lua_setglobal(L,"__EConnect");
	lua_register(L,"__EDisconnect",EDisconnect);
	lua_register(L,"__EReconnect",EReconnect);
	lua_register(L,"__EIsConnected",EIsConnected);
	lua_createtable(L,0,2); /* Event metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_EVENT_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_EVENT_NAME);
	lua_pushcclosure(L,ENewEvent,1);
	lua_setglobal(L,"__ENewEvent");
	lua_createtable(L,0,2); /* WThread metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_WTHREAD_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_WTHREAD_NAME);
	lua_pushcclosure(L,EWait,1);
	lua_setglobal(L,"__EWait");
	lua_register(L,"__EFire",EFire);
}
