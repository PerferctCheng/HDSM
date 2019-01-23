#pragma once
#include "Task.h"
#include "Connection.h"
#include "ITaskNotify.h"
#include "TaskCenter.h"
#include "ConnectionPool.h"
#include "ISimpleTimerHandler.h"
#include "SimpleTimer.h"
#include "BaseSocket.h"
#include <vector>
#include "TypeDefine.h"

using namespace std;

class TaskServer 
	: public ITaskNotify, public ISimpleTimerHandler
{
public:
	TaskServer(HUINT16 port);
	~TaskServer(void);
public:
	virtual HBOOL on_task_completed(Connection *pConn);
	virtual HBOOL on_command(HUINT32 cmd);
public:
	virtual void on_timer(HUINT32 ulCurTicks);
public:
	void stop();
	void start();
private:
	void reset_all_fd_set();
	HBOOL check_accept();
	HBOOL check_conns();
	void handle_task(Connection *pConn);
private:
	HBOOL read_bytes(Connection *pConn);
	HBOOL send_bytes(Connection *pConn);
private:
	HUINT16			m_ListenningPort;
	BaseSocket		m_ListeningSocket;
	fd_set			m_fdRead;//, m_fdException;
	vector<Connection *> m_ClientConns;
	HBOOL			m_bShutdown;
	TaskCenter		*m_pTaskCenter;
	ConnectionPool	m_ConnPool;
	HUINT64 m_ullRecvedTaskCount;
	HUINT64 m_ullFinishedTaskCount;
	HINT32			m_nMaxFD;
private:
	SimpleTimer		*m_pTimer;
};

