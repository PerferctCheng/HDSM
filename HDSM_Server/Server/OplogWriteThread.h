#pragma once
#include "BaseThread.h"
#include "IOplogManager.h"
#include "TypeDefine.h"


class OplogWriteThread 
	: public BaseThread
{
public:
	OplogWriteThread(IOplogManager *pManager);
	~OplogWriteThread(void);
private:
	virtual void run();
private:
	IOplogManager *m_pManager;
};

