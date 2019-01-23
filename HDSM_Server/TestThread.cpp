#include "TestThread.h"
#include "Utils.h"
#include <iostream>
using namespace std;

TestThread::TestThread(HDSimpleMap *pMap)
{
	m_pHDSM = pMap;
}


TestThread::~TestThread(void)
{
}

void TestThread::run()
{
	//put_many_keys();
	//get_many_keys();
	erase_many_keys();
}

void TestThread::put_many_keys()
{
	HINT32 failed = 0;
	HINT32 k = 0;
	//HINT32 miss = 0;
	HBOOL r = false;
	for (HUINT64 i=m_nStartPos; i<m_nEndPos; i++)
	{
		string key = "key@" + std::to_string(i);
		string value = "value@" + std::to_string(i);

		r = m_pHDSM->put(key, value, Utils::get_current_time_stamp());
		if (!r)
			failed++;
		k++;

		string get_value;
		HINT32 lExpireMinutes = -1;
		r = m_pHDSM->get(key, get_value, lExpireMinutes);
		if (r && get_value.compare(value)!=0)
		{
			cout<< "PUT key"<<key.c_str()<<"异常!!!"<<endl;
		}

		if (k % 100 == 0)
			cout<<"===Total Length: "<< m_pHDSM->length() << "==="<< endl;
	}
	cout << "总共Put" << (m_nEndPos-m_nStartPos) << "个key， 失败" << failed << "个" << endl; 
	return;
}

void TestThread::get_many_keys()
{
	HINT32 failed = 0;
	HINT32 k = 0;
	//HINT32 miss = 0;
	HBOOL r = false;
	for (HUINT64 i=m_nStartPos; i<m_nEndPos; i++)
	{
		string key = "key@" + std::to_string(i);
		string v;
		string value = "value@" + std::to_string(i);
		HINT32 lExpireMinutes = -1;

		k++;
		r = m_pHDSM->get(key, v, lExpireMinutes);

		if (!r || v.compare(value)!=0)
			failed++;

		if (k%1000 == 0)
			cout << "已GET:"<< i << "个key" << endl;
	}

	cout << "总共Get" << (m_nEndPos-m_nStartPos) << "个key， 失败" << failed << "个" << endl;
}

void TestThread::erase_many_keys()
{
	HINT32 failed = 0;
	HINT32 k = 0;
	//HINT32 miss = 0;
	HBOOL r = false;
	for (HUINT64 i=m_nStartPos; i<m_nEndPos; i++)
	{
		string key = "key@" + std::to_string(i);
		//string value = "value@" + std::to_string(i);

		r = m_pHDSM->erase(key);
		if (!r)
			failed++;
		k++;

		if (k % 100 == 0)
			cout<<"===Total Length: "<< m_pHDSM->length() << "==="<< endl;
	}
	cout << "总共ERASE" << (m_nEndPos-m_nStartPos) << "个key， 失败" << failed << "个" << endl; 
	return;
}

void TestThread::set_interval(HINT32 start, HINT32 end)
{
	m_nStartPos = start;
	m_nEndPos = end;
}