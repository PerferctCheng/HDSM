#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#include <stdio.h>
#endif

#include "SimpleLogger.h"
#include "Utils.h"
#include <string.h>

SimpleLogger::SimpleLogger(const string &strLogFilePath, HUINT32 nlevel, HUINT32 ulMaxFileSize)
{
	m_ulMaxFileSize = (ulMaxFileSize < 1*1024*1024) ? (1*1024*1024) : ulMaxFileSize;
	m_nLevel = nlevel;

	if (strLogFilePath.length() == 0)
		Utils::get_current_path(m_strLogFilePath);
	else
		m_strLogFilePath = strLogFilePath;

	Utils::create_dirs(m_strLogFilePath);
	
	m_strLogFilePath += "/hdsm.log";

	if (Utils::file_exist(m_strLogFilePath))
		m_fpLogFile = fopen(m_strLogFilePath.c_str(), "a+");
	else
		m_fpLogFile = fopen(m_strLogFilePath.c_str(), "w+");

}

SimpleLogger::~SimpleLogger(void)
{
	if (m_fpLogFile != NULL) 
		fclose(m_fpLogFile);
}

void SimpleLogger::check_file_size()
{
	if (m_fpLogFile != NULL)
	{
		HINT32 len = Utils::get_file_size(m_fpLogFile);
		if (len > 0 && ((HUINT32)len) > m_ulMaxFileSize)
		{
			fclose(m_fpLogFile);
			Utils::delete_file(m_strLogFilePath +".1");
			Utils::rename_file(m_strLogFilePath, m_strLogFilePath +".1");
			m_fpLogFile = fopen(m_strLogFilePath.c_str(), "w+");
		}
	}
	else
	{
		m_fpLogFile = fopen(m_strLogFilePath.c_str(), "a+");
	}

	return;
}

HBOOL SimpleLogger::log(HUINT32 nlevel, const HCHAR *pszlevelstr,  const HCHAR *pInfo)
{
	string str = Utils::get_fmt_local_time() + " [" + pszlevelstr + "] " + pInfo + "\n";
	if (nlevel >= m_nLevel)
		printf("%s", str.c_str());

	if (!m_fpLogFile)
		return false;

	m_lock.lock();

	check_file_size();
	fprintf(m_fpLogFile, "%s", str.c_str());
	fflush(m_fpLogFile);

	m_lock.unlock();
	return true;
}

