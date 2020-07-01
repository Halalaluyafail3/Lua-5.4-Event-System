#include<stdio.h>
#include<stddef.h>
/* Connection system for Lua 5.4 */
#define E_CONNECTION_NAME "Connection"
/*
Connection uses 3 bytes to store its state, a pointer for a string, and a size_t for the string length
Connected - whether the Connection is connected
ToDiscconnect - whether the Connection is going to be disconnect (it disconnected during it being fired)
Running - whether the Connection is running
uservalue 1 - function
uservalue 2 - next Connection
uservalue 3 - previous Connection
uservalue 4 - Event
*/
typedef struct Connection{
	size_t DebugStringLength;
	unsigned char Connected,ToDisconnect,Running;
	char DebugString[];
}Connection;
#define E_EVENT_NAME "Event"
/*
Event uses its 1 byte of data to represent whether it is firing or not
uservalue 1 - first connection
uservalue 2 - first waiting thread
*/
#define E_WTHREAD_NAME "WThread"
/*
WThread uses its 2 bytes of data to represent its state, whether it's being run, and whether it should close the thread upon erroring
uservalue 1 - next WThread
uservalue 2 - previous WThread
uservalue 3 - the thread
*/
typedef struct WThread{
	unsigned char CloseOnError,Running;
}WThread;
#define printLiteral(s) fwrite(s,sizeof(char),sizeof(s)-1,stdout)
void errMessage(lua_State*L){
	switch(lua_type(L,-1)){
		case LUA_TSTRING:{
			size_t size;
			const char*str = lua_tolstring(L,-1,&size);
			fwrite(str,sizeof(char),size,stdout);
			break;
		}
		case LUA_TNUMBER:{
			if(lua_isinteger(L,-1))
				printf(LUA_INTEGER_FMT,lua_tointeger(L,-1));
			else
				printf(LUA_NUMBER_FMT,lua_tonumber(L,-1));
			break;
		}
		case LUA_TNIL:{
			printLiteral("nil");
			break;
		}
		case LUA_TBOOLEAN:{
			int b = lua_toboolean(L,-1);
			if(b)
				printLiteral("true");
			else
				printLiteral("false");
			break;
		}
		default:{
			int tt = luaL_getmetafield(L,-1,"__name");
			if(tt==LUA_TSTRING){
				size_t size;
				const char*str = lua_tolstring(L,-1,&size);
				fwrite(str,sizeof(char),size,stdout);
				lua_pop(L,1);
				printf(": %p",lua_topointer(L,-1));
			}else{
				if(tt!=LUA_TNIL)
					lua_pop(L,1);
				printf("%s: %p",luaL_typename(L,-1),lua_topointer(L,-1));
			}
		}
	}
}
int eIsConnected(lua_State*L){
	Connection*con = (Connection*)luaL_checkudata(L,1,E_CONNECTION_NAME);
	lua_pushboolean(L,con->Connected&&!con->ToDisconnect);
	return 1;
}
int eErrorHandler(lua_State*L){
	lua_settop(L,1);
	printLiteral("| Error message (Connection):\n");
	errMessage(L);
	printLiteral("\n| Traceback:\n");
	luaL_traceback(L,L,NULL,0);
	size_t size;
	const char*str = lua_tolstring(L,2,&size);
	fwrite(str,sizeof(char),size,stdout);
	return 0;
}
int eConnect(lua_State*L){
	luaL_checkudata(L,1,E_EVENT_NAME);
	luaL_checktype(L,2,LUA_TFUNCTION);
	luaL_traceback(L,L,NULL,0);
	size_t size;
	const char*str = lua_tolstring(L,-1,&size);
	Connection*con = (Connection*)lua_newuserdatauv(L,offsetof(Connection,DebugString)+(size+1)*sizeof(char),4);
	con->Connected = 0xFF;
	con->ToDisconnect = 0;
	con->Running = 0;
	con->DebugStringLength = size;
	char*Debug = con->DebugString;
	for(size_t i=0;i<=size;i++)
		Debug[i] = str[i];
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
int eDisconnect(lua_State*L){
	Connection*con = (Connection*)luaL_checkudata(L,1,E_CONNECTION_NAME);
	if(con->Connected)
		if(con->Running)
			con->ToDisconnect = 0xFF;
		else{
			con->Connected = 0;
			if(lua_getiuservalue(L,1,3)!=LUA_TNIL) /* has previous */
				if(lua_getiuservalue(L,1,2)!=LUA_TNIL){ /* has previous and next */
					lua_pushvalue(L,-1);
					lua_setiuservalue(L,-3,2); /* set previous's next */
					lua_pushvalue(L,-2);
					lua_setiuservalue(L,-2,3); /* set next's previous */
					lua_pushnil(L);
					lua_pushnil(L);
					lua_setiuservalue(L,1,2); /* remove next */
					lua_setiuservalue(L,1,3); /* remove previous */
				}else{ /* has previous and no next */
					lua_setiuservalue(L,-2,2); /* remove previous's next */
					lua_pushnil(L);
					lua_setiuservalue(L,1,3); /* remove previous */
				}
			else /* no previous */
				if(lua_getiuservalue(L,1,2)!=LUA_TNIL){ /* no previous but has a next */
					lua_getiuservalue(L,1,4);
					lua_pushvalue(L,-2);
					lua_setiuservalue(L,-2,1); /* new start */
					lua_pushnil(L);
					lua_setiuservalue(L,-3,3); /* remove next's previous */
					lua_pushnil(L);
					lua_setiuservalue(L,1,2); /* remove next */
				}else{ /* no previous and no next */
					lua_getiuservalue(L,1,4);
					lua_pushnil(L);
					lua_setiuservalue(L,-2,1); /* remove start */
				}
		}
	return 0;
}
int eReconnect(lua_State*L){
	Connection*con = (Connection*)luaL_checkudata(L,1,E_CONNECTION_NAME);
	if(!con->Connected){
		con->Connected = 0xFF;
		lua_getiuservalue(L,1,4);
		if(lua_getiuservalue(L,-1,1)!=LUA_TNIL){ /* has next */
			lua_pushvalue(L,1);
			lua_setiuservalue(L,-2,3); /* set next's previous */
			lua_setiuservalue(L,1,2); /* set next */
		}else
			lua_pop(L,1);
		lua_pushvalue(L,1);
		lua_setiuservalue(L,-2,1); /* new start */
	}else if(con->ToDisconnect)
		con->ToDisconnect = 0;
	return 0;
}
int eNewEvent(lua_State*L){
	unsigned char*Event = (unsigned char*)lua_newuserdatauv(L,sizeof(unsigned char),2);
	*Event = 0;
	lua_pushvalue(L,lua_upvalueindex(1));
	lua_setmetatable(L,-2);
	return 1;
}
int eWait(lua_State*L){
	luaL_checkudata(L,1,E_EVENT_NAME);
	luaL_argcheck(L,lua_isboolean(L,2)||lua_isnoneornil(L,2),2,"boolean, nil, or none expected");
	unsigned char WValue = !lua_isnone(L,2)&&lua_toboolean(L,2)?0:0xFF;
	WThread*W = (WThread*)lua_newuserdatauv(L,sizeof(WThread),3);
	W->Running = 0;
	W->CloseOnError = WValue;
	lua_pushvalue(L,lua_upvalueindex(1));
	lua_setmetatable(L,-2);
	if(lua_getiuservalue(L,1,2)!=LUA_TNIL){ /* has next */
		lua_pushvalue(L,-2);
		lua_setiuservalue(L,-2,2); /* set next's previous */
		lua_setiuservalue(L,-2,1); /* set next */
	}else
		lua_pop(L,1);
	lua_pushthread(L);
	lua_setiuservalue(L,-2,3); /* set thread */
	lua_setiuservalue(L,1,2); /* new start */
	lua_settop(L,0);
	return lua_yield(L,0);
}
int eFire(lua_State*L){
	unsigned char*Event = (unsigned char*)luaL_checkudata(L,1,E_EVENT_NAME);
	int top = lua_gettop(L),mtop = top-1,ptop = top+1,argtop = top+2;
	luaL_checkstack(L,argtop,"too many arguments");
	unsigned char NotRunning = !*Event;
	if(NotRunning)
		*Event = 0xFF;
	/* event, args, err_handler, connection, function, args */
	lua_pushcfunction(L,eErrorHandler);
	lua_getiuservalue(L,1,1);
	while(lua_type(L,-1)!=LUA_TNIL){
		Connection*con = (Connection*)lua_touserdata(L,-1);
		if(con->Connected&&!con->ToDisconnect){
			unsigned char NotRunningCon = !con->Running;
			if(NotRunningCon)
				con->Running = 0xFF;
			lua_getiuservalue(L,-1,1);
			for(int i=2;i<=top;i++)
				lua_pushvalue(L,i);
			int err = lua_pcall(L,mtop,0,ptop);
			lua_settop(L,argtop);
			if(err==LUA_ERRRUN){
				printLiteral("\n| Connection Point:\n");
				fwrite(con->DebugString,sizeof(char),con->DebugStringLength,stdout);
				printLiteral("\n| End");
			}
			if(NotRunningCon){
				con->Running = 0;
				if(con->ToDisconnect){
					con->ToDisconnect = 0;
					lua_getiuservalue(L,-1,2);
					lua_pushcfunction(L,eDisconnect);
					lua_pushvalue(L,-3);
					lua_call(L,1,0);
					lua_remove(L,-2);
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
	lua_settop(L,top);
	/* event, args, WThread, thread, args */
	lua_getiuservalue(L,1,2);
	while(lua_type(L,-1)!=LUA_TNIL){
		WThread*W = (WThread*)lua_touserdata(L,-1);
		if(W->Running)
			break;
		W->Running = 0xFF;
		lua_getiuservalue(L,-1,3);
		lua_State*thread = lua_tothread(L,-1);
		if(lua_checkstack(thread,mtop)){
			for(int i=2;i<=top;i++)
				lua_pushvalue(L,i);
			lua_xmove(L,thread,mtop);
			int nresults;
			int result = lua_resume(thread,L,mtop,&nresults);
			if(result!=LUA_OK&&result!=LUA_YIELD){
				lua_xmove(thread,L,1);
				printLiteral("| Error Message (Wait Resume):\n");
				errMessage(L);
				printLiteral("\n| Traceback:\n");
				luaL_traceback(L,thread,NULL,0);
				size_t size;
				const char*str = lua_tolstring(L,-1,&size);
				fwrite(str,sizeof(char),size,stdout);
				printLiteral("\n| Fire Point:\n");
				luaL_traceback(L,L,NULL,0);
				const char*str2 = lua_tolstring(L,-1,&size);
				fwrite(str2,sizeof(char),size,stdout);
				printLiteral("\n| End\n");
				if(W->CloseOnError)
					/* pass error object ???? (so __close metemethod can see it) */
					if(lua_resetthread(thread)!=LUA_OK){ /* should i really warn for this??? */
						lua_xmove(thread,L,1);
						printLiteral("| Error closing thread: ");
						errMessage(L);
						putchar('\n');
					}
			}else
				lua_pop(thread,nresults);
		}else
			printLiteral("| too many arguments\n");
		W->Running = 0;
		lua_settop(L,ptop);
		if(lua_getiuservalue(L,-1,2)!=LUA_TNIL) /* has previous */
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
		else /* no previous */
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
	if(NotRunning)
		*Event = 0;
	return 0;
}
void loadEventLibrary(lua_State*L){
	lua_createtable(L,0,2); /* Connections metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_CONNECTION_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_CONNECTION_NAME);
	lua_pushcclosure(L,eConnect,1);
	lua_setglobal(L,"eConnect");
	lua_register(L,"eDisconnect",eDisconnect);
	lua_register(L,"eReconnect",eReconnect);
	lua_register(L,"eIsConnected",eIsConnected);
	lua_createtable(L,0,2); /* Event metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_EVENT_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_EVENT_NAME);
	lua_pushcclosure(L,eNewEvent,1);
	lua_setglobal(L,"eNewEvent");
	lua_createtable(L,0,2); /* WThread metatable */
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushstring(L,E_WTHREAD_NAME);
	lua_setfield(L,-2,"__name");
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,E_WTHREAD_NAME);
	lua_pushcclosure(L,eWait,1);
	lua_setglobal(L,"eWait");
	lua_register(L,"eFire",eFire);
}
