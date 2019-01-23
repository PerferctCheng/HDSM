#include "WorkThread.h"
#include "Utils.h"

WorkThread::WorkThread(ITaskCenter *pTaskCenter)
{
	m_pTaskCenter = pTaskCenter;
}

WorkThread::~WorkThread(void)
{
}

void WorkThread::run()
{
	while (!m_bStopped && m_pTaskCenter != NULL && m_pTaskCenter->wait_notify())
	{
		Task task = m_pTaskCenter->pop_task();
		if (task.is_valid())
		{
			m_pTaskCenter->exec_task(task);
		}
	}
	return;
}