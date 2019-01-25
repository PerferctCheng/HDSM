#pragma once
#include "ConfigureMgr.h"
#include <string>
#include "TypeDefine.h"

using namespace std;

class Global
{
public:
	enum enSERVER_MODE
	{
		SERVER_MODE_ALONE = 0,
		SERVER_MODE_MIRROR,
	};
public:
	static string CONFIGURE_FILE_PATH;
	static HUINT32 MAX_KEY_LENGTH;
	static HUINT32 MAX_VALUE_LENGTH;
	static const HUINT32 VERSION_CODE = 1;
	static HUINT32 MAX_SHARD_COUNT;
	static HUINT32 MAX_INVALID_KEYS;
	static const HUINT32 DEFAULT_BUFFER_SIZE_1K = 1024;
	static HUINT32 MAX_BUFFER_ROOMS_COUNT;
	static HUINT32 MAX_BUFFER_ROOMS_SIZE;
	static HUINT32 SECONDS_IN_ONE_HOURS;
	static HUINT32 SECONDS_IN_ONE_MINUTE;
	static HUINT32 SECONDS_IN_ONE_DAY;
	static HUINT32 SERVER_MODE;
};
