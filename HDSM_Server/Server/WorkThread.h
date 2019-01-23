#pragma once
#include "BaseThread.h"
#include "ITaskCenter.h"
#include "TypeDefine.h"

class WorkThread 
	: public BaseThread
{
public:
	WorkThread(ITaskCenter *pTaskCenter);
	~WorkThread(void);
private:
	virtual void run();
private:
	ITaskCenter *m_pTaskCenter;
};

