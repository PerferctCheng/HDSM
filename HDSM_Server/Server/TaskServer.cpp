#ifdef WIN32
#include <WinSock2.h>  
#endif
#include "TaskServer.h"
#include "Utils.h"
#include "BaseSocket.h"
#include "../ConfigureMgr.h"
#include "../Logger.h"
#include <string.h>


TaskServer::TaskServer(HUINT16 port)
{	
	m_ullRecvedTaskCount = 0;
	m_ullFinishedTaskCount = 0;
	m_ListenningPort = port;
	m_bShutdown = false;

	m_ListeningSocket.create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!m_ListeningSocket.is_valid())
	{
		Logger::log_e("Init server failed(create stream socket)...");
		exit(0);
	}

	if (m_ListeningSocket.bind(port) == SOCKET_ERROR)
	{
		Logger::log_e("Init server failed(bind server port)...");
		exit(0);
	}
	
	if (m_ListeningSocket.set_block_flag(false) == SOCKET_ERROR)
	{
		Logger::log_e("Init server failed(ioctlsocket)...");
		exit(0);
	}

	m_pTaskCenter = new TaskCenter(this, ConfigureMgr::get_work_threads_count());
	m_pTimer = new SimpleTimer(this, 1000);
	if (m_pTimer != NULL)
		m_pTimer->start();

	Logger::log_i("Init server Successed!");
}

TaskServer::~TaskServer(void)
{
	if (m_pTaskCenter != NULL)
	{
		delete m_pTaskCenter;
		m_pTaskCenter = NULL;
	}

	if (m_pTimer != NULL)
	{
		m_pTimer->stop();
		delete m_pTimer;
		m_pTimer = NULL;
	}
}

void TaskServer::reset_all_fd_set()
{
	FD_ZERO(&m_fdRead);

	FD_SET((SOCKET)m_ListeningSocket, &m_fdRead);

	for (HUINT32 i=0;i<m_ClientConns.size(); ++i)
	{
		Connection *pConn = m_ClientConns[i];

		if (pConn->m_ulReadOffset < Global::DEFAULT_TEMP_BUFFER_SIZE)
			FD_SET((SOCKET)pConn->get_sock(), &m_fdRead);
	}

	return;
}

HBOOL TaskServer::check_accept()
{
	HINT32 nLastErr = 0;


	if (FD_ISSET((SOCKET)m_ListeningSocket, &m_fdRead))
	{
		if (m_ClientConns.size() >= FD_SETSIZE-1)
		{
			Logger::log_w("Too many connections!");
			return true;
		}

		HINT32 index = m_ConnPool.lease();
		if (index == -1)
		{
			Logger::log_w("Too many connections!");
			return true;
		}

// 		sockaddr_in addrRemote;
// 		HINT32 nSize = sizeof(sockaddr_in);
// 		HUINT32 s = accept(m_ListeningSocket,(sockaddr *)&addrRemote, &nSize);

		BaseSocket s;
		m_ListeningSocket.accept(s);

		nLastErr = BaseSocket::get_error();
#ifdef WIN32
		if (!s.is_valid() && nLastErr != WSAEWOULDBLOCK)
		{
			Logger::log_e("Create new connection failed!(ERR = %d)", nLastErr);
			return false;
		}
#else
		if (!s.is_valid())
		{
			Logger::log_e("Create new connection failed!(ERR = %d)", nLastErr);
			return false;
		}
#endif

		if (s.set_block_flag(false) == SOCKET_ERROR)
		{
			s.close();
			return false;
		}

		if (s.get_raw_socket() > m_nMaxFD)
			m_nMaxFD = s.get_raw_socket();

		Connection *pConn = (Connection *)m_ConnPool.get(index);
		if (pConn != NULL)
		{
			pConn->set_sock(s);
			m_ClientConns.push_back(pConn);
			Logger::log_i("Active connections: %d", m_ClientConns.size());
		}
	}

	return true;
}

HBOOL TaskServer::check_conns()
{
 	vector<Connection *>::iterator it = m_ClientConns.begin();
 	while (it != m_ClientConns.end())
 	{
 		Connection *pConn = *it;
 		HBOOL bOK = true;

		if (pConn == NULL || !pConn->get_sock().is_valid() || pConn->is_close_wait())
		{
			bOK = false;
		}
		else
		{
				if (FD_ISSET((SOCKET)pConn->get_sock(), &m_fdRead))
					bOK = read_bytes(pConn);
		}
 
 		if (!bOK)
 		{
			if (pConn != NULL)
			{
				Logger::log_i("Close connection(sock = %d)!", pConn->get_sock().get_raw_socket());
				pConn->get_sock().close();
 				m_ConnPool.ret(pConn->get_index());
			}

 			it = m_ClientConns.erase(it);
			Logger::log_d("Active Connections: %d", m_ClientConns.size());
 		}
 		else
 		{
 			++it;
 		}
 	}
 
 	return true;
}

void TaskServer::start()
{
	if (m_ListeningSocket.listen() == SOCKET_ERROR)
	{
		Logger::log_e("The Server closed unexpectedly!(listen port failed)");
		return;
	}

	Logger::log_i("The Server is listenning Port :%d...", m_ListenningPort);

	if (m_pTaskCenter != NULL) 
		m_pTaskCenter->start();

	m_nMaxFD = m_ListeningSocket.get_raw_socket();

	timeval waittimeout; 
	waittimeout.tv_sec  = 3;
	waittimeout.tv_usec = 0; 

	while (!m_bShutdown)
	{
		reset_all_fd_set();

		HINT32 nRet = select(m_nMaxFD+1, &m_fdRead, NULL, NULL/*&m_fdException*/, &waittimeout);
		if (nRet < 0)
		{
			Logger::log_e("The Server closed unexpectedly(select failed)!");
			break;
		}

		if (!check_accept())
		{
			Logger::log_e("The Server closed unexpectedly(check accept failed)!");
			break;
		}

		check_conns();
	}

	if (m_pTaskCenter != NULL) 
		m_pTaskCenter->stop();

	vector<Connection *>::iterator it;
	for (it=m_ClientConns.begin(); it!=m_ClientConns.end(); ++it)
	{
		Connection *pConn = *it;
		if (pConn != NULL)
		{
			pConn->get_sock().close();
			m_ConnPool.ret(pConn->get_index());
		}
	}
	m_ClientConns.clear();

	m_ListeningSocket.close();

	if (m_bShutdown)
		Logger::log_i("The Server is closed, bye!");
	else
		Logger::log_e("The Server shutdown unexpectedly!!!");

	return;
}

void TaskServer::stop()
{
	Logger::log_i("The Server is closing...");
	m_bShutdown = true;
}

HBOOL TaskServer::read_bytes(Connection *pConn)
{
	if (!pConn || !pConn->get_sock().is_valid()) 
		return false;

	HINT32 nRet = pConn->get_sock().recv(pConn->m_szReadBuffer + pConn->m_ulReadOffset, 
								Global::DEFAULT_TEMP_BUFFER_SIZE-pConn->m_ulReadOffset, 
								0);

	if (nRet > 0)
	{
		pConn->m_ulReadOffset += nRet;
		handle_task(pConn);
		return true;
	}
	else if (nRet == 0)
	{
		Logger::log_i("Client closes the connection(sock = %d)!", pConn->get_sock().get_raw_socket());
		pConn->set_close_wait();
		return false;
	}
	else
	{
#ifdef WIN32
		HINT32 nLastErr = BaseSocket::get_error();//WSAGetLastError();
		if (nLastErr != WSAEWOULDBLOCK)
		{
			//Logger::log_w("数据通道接受异常(sock=%d, ERR=%d)", pConn->get_sock(), nLastErr);
			pConn->set_close_wait();
			return false;
		}
#else
		pConn->set_close_wait();
		return false;
#endif
	}

	return true;
}

HBOOL TaskServer::send_bytes(Connection *pConn)
{
	if (!pConn || !pConn->get_sock().is_valid()) 
		return false;

	HINT32 flag = 0;
#ifndef WIN32
	flag = MSG_NOSIGNAL;
#endif
	pConn->wait_for_send_respond_right();
	while (pConn->get_response_count() > 0)
	{
		Connection::Response *pr = pConn->get_front_response();
		if (pr != NULL)
		{
			const HCHAR *buf = pr->buffer.c_str();
			while (true)
			{
				HINT32 nSent = pConn->get_sock().send(&buf[pr->ulWrittenOffset], pr->buffer.size()-pr->ulWrittenOffset, flag);
				if (nSent > 0)
				{
					pr->ulWrittenOffset += nSent;
					if (pr->ulWrittenOffset >= pr->buffer.size())
					{
						pConn->pop_response();
						m_ullFinishedTaskCount++;
						if (pr->operate == OP_QUIT)
						{
							pConn->set_close_wait();
							pConn->ret_send_respond_right();
							return true;
						}
						break;
					}
				}
				else if (nSent == 0)
				{
					pConn->set_close_wait();
					pConn->ret_send_respond_right();
					return false;
				}
				else
				{
#ifdef WIN32
					HINT32 nLastErr = BaseSocket::get_error();//WSAGetLastError();
					if (nLastErr != WSAEWOULDBLOCK)
#endif
					{

						pConn->set_close_wait();
						Logger::log_w("An exception Occured while Sending Data(sock=%d, size=%d)", pConn->get_sock().get_raw_socket(), pr->buffer.size());
						pConn->ret_send_respond_right();
						return false;
					}

					break;
				}
			}
		}
	}

	pConn->ret_send_respond_right();
	return true;
}

void TaskServer::handle_task(Connection *pConn)
{
	if (pConn != NULL && pConn->get_sock().is_valid())
	{
		while (pConn->m_ulReadOffset > 4)
		{
			HUINT32 offset = 0;
			HUINT32 ulBufLen = 0;
			memcpy(&ulBufLen, pConn->m_szReadBuffer, sizeof(HUINT32));
			offset += sizeof(HUINT32);

			if ((pConn->m_ulReadOffset - sizeof(HUINT32)) >= ulBufLen)
			{
				Task task(pConn, &pConn->m_szReadBuffer[offset], ulBufLen);
				if (task.is_valid())
				{
					if (m_pTaskCenter != NULL) 
					{
						Logger::log_d("Recved New Task(sock=%d, op=%s)", pConn->get_sock().get_raw_socket(), task.get_operate_str().c_str());
						m_ullRecvedTaskCount++;
						m_pTaskCenter->push_task(task);
					}
					offset += ulBufLen;
					memmove(pConn->m_szReadBuffer, &pConn->m_szReadBuffer[offset], Global::DEFAULT_TEMP_BUFFER_SIZE-offset);
					pConn->m_ulReadOffset -= offset;	
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	return;
}

HBOOL TaskServer::on_task_completed(Connection *pConn)
{
	send_bytes(pConn);
	return true;
}

void TaskServer::on_timer(HUINT32 ulCurTicks)
{
	HUINT32 uiPrintStatusInfoPeriod = ConfigureMgr::get_print_status_info_period();
	if (uiPrintStatusInfoPeriod < 5)
		uiPrintStatusInfoPeriod = 5;
	if (uiPrintStatusInfoPeriod > Global::SECONDS_IN_ONE_HOURS)
		uiPrintStatusInfoPeriod = Global::SECONDS_IN_ONE_HOURS;

	if (ulCurTicks % uiPrintStatusInfoPeriod == 0)
	{
		if (m_pTaskCenter != NULL)
		{
			Logger::log_i("---------Server Running Status Information--------");
			Logger::log_i("Server Work Mode: %s", Global::SERVER_MODE == 0 ? "ALONE" : "MIRROR");
			Logger::log_i("Unhandled Tasks: %d, Active Connections: %d", m_pTaskCenter->get_task_count(), m_ClientConns.size());
			Logger::log_i("Total [handled|recved] Tasks: %I64d | %I64d", m_ullFinishedTaskCount,  m_ullRecvedTaskCount);
			Logger::log_i("Data Index Information: %s", m_pTaskCenter->get_indexs_size().c_str());
			Logger::log_i("Total Valid Keys: %d", m_pTaskCenter->get_total_keys());
			Logger::log_i("Total Valid oplogs: %d", m_pTaskCenter->get_total_oplogs());
			Logger::log_i("Total Expiring Keys: %d", m_pTaskCenter->get_expire_cache_size());
			Logger::log_i("Total Active Clients: %d", m_ConnPool.get_used_size());
			vector<string> vec = m_ConnPool.get_client_ips();
			for (HUINT32 i=0; i<vec.size(); i++)
			{
				Logger::log_i("Client%d: %s", i+1, vec[i].c_str());
			}
			Logger::log_i("--------------------------------------------------");
		}
	}

	if (m_pTaskCenter != NULL && m_pTaskCenter->get_task_count() > 0)
		m_pTaskCenter->renotify();

 	if (ulCurTicks % 10 == 0)
		Logger::reload_syslog_level();

	return;
}


HBOOL TaskServer::on_command(HUINT32 cmd)
{
	if (cmd == OP_SHUTDOWN)
	{
		Logger::log_i("Client sent Shutdown command！The Server will be closed in 3 Seconds...");
		Utils::sleep_for_seconds(3);
		m_bShutdown = true;
	}
	return true;
}