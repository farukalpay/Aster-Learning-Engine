#pragma once
#include "profile_runtime.config.h"

#if USE_PROFILE_RUNTIME
#include "profile_runtime_message.h"

#include <mutex>
#include <thread>

namespace ProfileRuntime
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Socket;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Server
{
	InputDataStream networkStream;

	static const int BIFFER_SIZE = 1024;
	char buffer[BIFFER_SIZE];

	Socket* socket;

	std::recursive_mutex socketLock;

	CaptureSaveChunkCb saveCb;

	Server( short port );
	~Server();

	bool InitConnection();

	void Send(const char* data, size_t size);

public:
	void SetSaveCallback(CaptureSaveChunkCb cb);

	void SendStart();
	void Send(DataResponse::Type type, OutputDataStream& stream);
	void SendFinish();

	void Update();

	string GetHostName() const;

	static Server &Get();
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif //USE_PROFILE_RUNTIME