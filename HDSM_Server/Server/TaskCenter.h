#pragma once

#include "Task.h"
#include "ITaskCenter.h"
#include "ITaskNotify.h"
#include "WorkThread.h"
#include "OplogManager.h"
#include "OplogSyncThread.h"
#include "SimpleNotifier.h"
#include "SimpleLock.h"
#include "../Data/HDSimpleMap.h"
#include <queue>
#include <vector>
#include "TypeDefine.h"

using namespace std;

class TaskCenter 
	: public ITaskCenter
{
public:
	TaskCenter(ITaskNotify *pNotify, HUINT32 nWorkThreadCount = 0);
	virtual ~TaskCenter(void);
public:
	virtual HBOOL push_task(const Task &task);
	virtual Task pop_task();
	virtual HBOOL exec_task(Task &task);
	virtual HBOOL wait_notify();
public:
	void renotify();
public:
	HBOOL start();
	HBOOL stop();
public:
	HUINT32 get_task_count();
	string get_indexs_size();
	HUINT32 get_total_keys();
	HUINT32 get_total_oplogs();
	HUINT32 get_expire_cache_size();
private:
	queue<Task>		m_TaskQueue;
	HUINT32	m_nThreadCount;
	SimpleLock		m_lock;
	vector<WorkThread *> m_vecThreads;
private:
	HDSimpleMap		*m_pHDSM;
	SimpleNotifier	m_Notifier;
	ITaskNotify		*m_pTaskNotify;
	OplogManager	*m_pOPLogger;
	OplogSyncThread *m_pOplogSyncThd;
};

