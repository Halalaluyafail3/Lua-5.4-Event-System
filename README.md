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

Disconnects the Connection, preventing **all** further calls of the function from the Connection, including when the Connection would be fired by a __EFire call that was running during the call of __EDisconnect.

### void __EReconnect(Connection)

Reconnects the Connection.

### boolean __EIsConnected(Connection)

Returns if the Connection is connected.

### ... __EWait(Event,boolean)

Yields the thread until Event gets fired. In case of error, the thread will be closed if the boolean argument is true. The results are the extra arguments passed to __EFire.

### void __EFire(Event,...)

Fires all connections, and then all waiting threads. The order in which connections and waiting threads fire is newest to oldest. A reconnected connection will be considered new; however, if there is an active call to the connection (the connection was disconnected during the call), reconnecting will maintain the current position. Connections (and reconnected connections) and waiting threads created during the call to __EFire will be ignored.
