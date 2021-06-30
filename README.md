# Lua-5.4-Event-System

An event system for Lua with events, connections, and waiting threads. Errors inside of connections and waiting threads will generate error messages.

## Functions exposed to C code:

### void LoadEventLibrary(lua_State*)

Defines the functions described later in the globals table. This function should only be called once, and there should be at least 5 available stack slots. This function may generate memory errors, and invoke metamethods in assigning to the globals table.

## Functions exposed to Lua:

### Event __ENewEvent(void)

Creates a new event, and returns it.

### Connection __EConnect(Event,Function)

Connects Function (which should be a function) to Event, and returns the Connection. The arguments to Function from calls from Connection will be the extra arguments passed to __EFire. Calls to Function from Connection shouldn't yield.

### void __EDisconnect(Connection)

Disconnects the Connection (if Connection is connected), preventing **all** further calls (until reconnection) of the function from the Connection, including when the Connection would be fired by a __EFire call that was running during the call of __EDisconnect.

### void __EReconnect(Connection)

Reconnects the Connection (if Connection isn't connected).

### boolean __EIsConnected(Connection)

Returns if the Connection is connected.

### ... __EWait(Event,boolean)

Yields the thread until Event gets fired. In case of error, the thread will be closed if the boolean argument is true. The results are the extra arguments passed to __EFire.

### void __EFire(Event,...)

Fires all connections, and then all waiting threads. The order in which connections and waiting threads fire is newest to oldest. A reconnected connection will be considered from the time of reconnection; however, if there is an active call to the connection (the connection was disconnected during the call), reconnecting will maintain the current position. Connections (and reconnected connections) created during the invocations of connected connections and waiting threads created during the invocation of waiting threads will be ignored. A call to __EWait in a connection to the same event will be fired with the waiting threads of the same __EFire call.
