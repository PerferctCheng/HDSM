#pragma once
#include "BaseThread.h"
#include "./Data/HDSimpleMap.h"
#include "TypeDefine.h"

class TestThread 
	: public BaseThread
{
public:
	TestThread(HDSimpleMap *pMap);
	~TestThread(void);
public:
	virtual void run();
public:
	void put_many_keys();
	void get_many_keys();
	void erase_many_keys();
public:
	void set_interval(HINT32 start, HINT32 end);
private:
	HINT32 m_nStartPos, m_nEndPos;
	HDSimpleMap *m_pHDSM;
};

