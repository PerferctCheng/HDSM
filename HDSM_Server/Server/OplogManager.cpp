#ifdef WIN32
#include <WinSock2.h>  
#endif
#include "OplogManager.h"
#include "Utils.h"
#include "../Global.h"
#include "../Logger.h"

#define INVALID_INDEX 0 
#define FRIST_INDEX 1

OplogManager::OplogManager(void)
{
	m_strOPLogPath = ConfigureMgr::get_oplog_path();
	Utils::create_dirs(m_strOPLogPath);

	m_uiMinSyncPeriod = ConfigureMgr::get_oplog_min_split_period()*1000;
	if (m_uiMinSyncPeriod > 30000) m_uiMinSyncPeriod = 30000;
	if (m_uiMinSyncPeriod < 1000) m_uiMinSyncPeriod = 1000;

	m_uiMaxSyncOps = ConfigureMgr::get_oplog_max_spilt_count();
	if (m_uiMaxSyncOps > 100000) m_uiMaxSyncOps = 100000;
	if (m_uiMaxSyncOps < 1000) m_uiMaxSyncOps = 1000;

	m_ulTotalWrittenOps = 0;
	m_ulWrittenOps = 0;
	m_pCurLogFile = NULL;

	load_write_index();
	load_read_index();

	m_pWriteThread = new OplogWriteThread(this);
	m_pWriteThread->start();
}


OplogManager::~OplogManager(void)
{
	m_Notifier.awake();

	if (m_pCurLogFile != NULL)
		fclose(m_pCurLogFile);
	
	if (m_pWriteIndexFile != NULL)
		fclose(m_pWriteIndexFile);

	if (m_pReadIndexFile != NULL)
		fclose(m_pReadIndexFile);

	if (m_pWriteThread != NULL)
	{
		m_pWriteThread->stop();
		delete m_pWriteThread;
	}
}

void OplogManager::get_log_buffer(Task *pTask, string &str)
{
	HUINT32 sn = pTask->get_sn();
	str.append((const HCHAR *)&sn, sizeof(HUINT32));

	HUINT32 op = pTask->get_operate();
	str.append((const HCHAR *)&op, sizeof(HUINT32));

	if (op == OP_PUT || op == OP_UPDATE)
	{
		HUINT32 ulKeyLen = pTask->get_key().length();
		str.append((const HCHAR *)&ulKeyLen, sizeof(HUINT32));
		str += pTask->get_key();

		HUINT32 ulValueLen = pTask->get_value().length();
		str.append((const HCHAR *)&ulValueLen, sizeof(HUINT32));
		str += pTask->get_value();

		HINT32 lExpireMinutes = pTask->get_expire_minutes();
		str.append((const HCHAR *)&lExpireMinutes, sizeof(HINT32));

		HUINT64 ullTimeStamp = pTask->get_time_stamp();
		str.append((const HCHAR *)&ullTimeStamp, sizeof(HUINT64));
	}
	else
	{
		HUINT32 ulKeyLen = pTask->get_key().length();
		str.append((const HCHAR *)&ulKeyLen, sizeof(HUINT32));
		str += pTask->get_key();
	}

	return;
}

HBOOL OplogManager::add_log(Task *pTask)
{
	//len(4)+checksum(1)+sn(4)+op(4)+[keylen(4)+key]+[valuelen(4)+value]+expireminutes（4）+ts（8）
	if (pTask == NULL)
		return false;

	HUINT32 op = pTask->get_operate();
	if (op != OP_PUT && op != OP_ERASE)
		return true;

	string _str;
	get_log_buffer(pTask, _str);

	HUINT8 checksum = Utils::get_check_sum(_str.c_str(), _str.size());
	HUINT32 ulDataLen = _str.length()+sizeof(HUINT8);

	string str;
	str.append((const HCHAR *)&ulDataLen, sizeof(HUINT32));
	str += checksum;
	str += _str;

	m_lock_for_write_buffer.lock();
	m_WriteBufferQueue.push(str);
	m_lock_for_write_buffer.unlock();
	m_Notifier.awake();

	return true;
}

HBOOL OplogManager::enter_write_loop()
{
	//等待信号
	m_Notifier.wait();

	string str;
	m_lock_for_write_buffer.lock();
	if (m_WriteBufferQueue.size() > 0)
	{
		str = m_WriteBufferQueue.front();
		m_WriteBufferQueue.pop();
		if (m_WriteBufferQueue.size() == 0)
			m_Notifier.rest();
	}
	m_lock_for_write_buffer.unlock();

	if (str.length() > 0)
	{
		m_lock_for_log_file.lock();
		if (m_pCurLogFile == NULL || m_ulWrittenOps >= m_uiMaxSyncOps)
		{
			if (!open_new_file())
			{
				Logger::log_e("Create new opl file failed: %s!", m_strCurOPLogFile.c_str());
			}
		}

		if (m_pCurLogFile != NULL)
		{
			fwrite(str.c_str(), str.size(), 1, m_pCurLogFile);
			fflush(m_pCurLogFile);
			m_ulWrittenOps++;
			m_ulTotalWrittenOps++;
		}
		m_lock_for_log_file.unlock();
	}

	return true;
}

HBOOL OplogManager::open_new_file()
{
	HBOOL ret = false;
	if (m_pCurLogFile != NULL)
	{
		Logger::log_d("[%d] oplogs in No.%I64d opl file!", m_ulWrittenOps, m_ullCurWriteIndex);
		fclose(m_pCurLogFile);
		m_ullCurWriteIndex++;
	}
	
	m_strCurOPLogFile = m_strOPLogPath + "/" + std::to_string(m_ullCurWriteIndex) + ".opl";
	m_pCurLogFile = fopen(m_strCurOPLogFile.c_str(), "wb");
	if (m_pCurLogFile != NULL)
	{
		m_ulWrittenOps = 0;
		update_write_index();
		m_ulLastOpenFileMoment = Utils::get_tick_count();
		ret = true;
	}
	return ret;
}

string OplogManager::load_file(HUINT64 index)
{
	string str;
	string strFile = m_strOPLogPath + "/" + std::to_string(index) + ".opl";
	if (Utils::file_exist(strFile))
	{
		FILE *fp = fopen(strFile.c_str(), "rb");
		if (fp != NULL)
		{
			HCHAR szBuf[Global::DEFAULT_BUFFER_SIZE_1K] = {0};
			HUINT32 read = 0;
			while ((read = fread(szBuf, 1, Global::DEFAULT_BUFFER_SIZE_1K, fp)) > 0)
				str.append(szBuf, read);
			fclose(fp);
		}
	}
	return str;
}

HBOOL OplogManager::ack(HUINT64 ack_index)
{
	HBOOL ret = false;
	if (ack_index != INVALID_INDEX && ack_index >= m_ullCurReadIndex)
	{
		Logger::log_i("The No.[%I64d] opl file sync Completed!", ack_index);
		m_ullCurReadIndex = ack_index+1;
		if (!remove_file(ack_index))
			Logger::log_w("The No.[%I64d] opl file is Deleted!", ack_index);
		ret = update_read_index(ack_index);
	}
	else
	{
		Logger::log_w("An exception occured while The Mirror Peer ACK [%I64d]!", ack_index);
	}
	return ret;
}

HUINT64 OplogManager::sync(string &str)
{	
	HUINT64 ret = INVALID_INDEX;
	if (m_ullCurReadIndex < m_ullCurWriteIndex)
	{		
		while (m_ullCurReadIndex < m_ullCurWriteIndex)//避免因为某些脏的空文件导致同步异常
		{
			str = load_file(m_ullCurReadIndex);
			if (str.length() > 0)
			{
				Logger::log_i("The Mirror Peer is syncing opl file: [%I64d][%d bytes，status %I64d | %I64d]",
					m_ullCurReadIndex, str.length(), m_ullCurReadIndex, m_ullCurWriteIndex);
				ret = m_ullCurReadIndex;
				break;
			}
			else
			{
				m_ullCurReadIndex++;
			}
		}
	}
	else if (m_ullCurReadIndex == m_ullCurWriteIndex)
	{
		if (m_ulWrittenOps > 0 && Utils::get_tick_count()-m_ulLastOpenFileMoment > m_uiMinSyncPeriod)
		{
			m_lock_for_log_file.lock();
			if (m_pCurLogFile != NULL)
			{
				Logger::log_d("[%d] oplogs in No.%I64d opl file!", m_ulWrittenOps, m_ullCurWriteIndex);
				fclose(m_pCurLogFile);
				m_pCurLogFile = NULL;
				m_ullCurWriteIndex++;
				m_ulWrittenOps = 0;
			}
			m_lock_for_log_file.unlock();
			str = load_file(m_ullCurReadIndex);
			if (str.length() > 0)
			{
				Logger::log_i("The Mirror Peer is syncing opl file: [%I64d][%d bytes，status %I64d | %I64d]",
					m_ullCurReadIndex, str.length(), m_ullCurReadIndex, m_ullCurWriteIndex);
				ret = m_ullCurReadIndex;
			}
		}
		else
		{
			str = "NODATA";
		}
	}
	else
	{
		str = "NODATA";
	}

	return ret;
}

HBOOL OplogManager::remove_file(HUINT64 index)
{
	HBOOL ret = false;
	if (index < m_ullCurWriteIndex)
	{
		string strFile = m_strOPLogPath + "/" + std::to_string(index) + ".opl";
		ret = Utils::delete_file(strFile);
	}
	return ret;
}

HBOOL OplogManager::load_write_index()
{
	string strFile = m_strOPLogPath + "/write.idx";

	m_ullCurWriteIndex = FRIST_INDEX;
	if (Utils::file_exist(strFile))
	{
		m_pWriteIndexFile = fopen(strFile.c_str(), "rb+");
		if (m_pWriteIndexFile != NULL)
		{
			fread(&m_ullCurWriteIndex, sizeof(HUINT64), 1, m_pWriteIndexFile);
			m_ullCurWriteIndex++;
			return true;
		}
	}
	else
	{
		m_pWriteIndexFile = fopen(strFile.c_str(), "wb+");
		if (m_pWriteIndexFile != NULL)
			return true;
	}

	return false;
}

HBOOL OplogManager::load_read_index()
{
	string strFile = m_strOPLogPath + "/read.idx";

	m_ullCurReadIndex = FRIST_INDEX;
	if (Utils::file_exist(strFile))
	{
		m_pReadIndexFile = fopen(strFile.c_str(), "rb+");
		if (m_pReadIndexFile != NULL)
		{
			fread(&m_ullCurReadIndex, sizeof(HUINT64), 1, m_pReadIndexFile);
			m_ullCurReadIndex++;
			return true;
		}
	}
	else
	{
		m_pReadIndexFile = fopen(strFile.c_str(), "wb+");
		if (m_pReadIndexFile != NULL)
			return true;
	}

	return false;
}

HBOOL OplogManager::update_write_index()
{
	HBOOL ret = false;
	if (m_pWriteIndexFile != NULL)
	{
		fseek(m_pWriteIndexFile, 0, SEEK_SET);
		fwrite(&m_ullCurWriteIndex, sizeof(HUINT64), 1, m_pWriteIndexFile);
		fflush(m_pWriteIndexFile);
		ret = true;
	}
	return ret;
}

HBOOL OplogManager::update_read_index(HUINT64 index)
{
	HBOOL ret = false;
	if (m_pReadIndexFile != NULL)
	{
		fseek(m_pReadIndexFile, 0, SEEK_SET);
		fwrite(&index, sizeof(HUINT64), 1, m_pReadIndexFile);
		fflush(m_pReadIndexFile);
		ret = true;
	}
	return ret;
}

HUINT32 OplogManager::get_total_ops()
{
	return m_ulTotalWrittenOps;
}