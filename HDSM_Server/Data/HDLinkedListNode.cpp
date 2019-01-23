#include "HDLinkedListNode.h"
#include "Utils.h"
#include "../Global.h"
#include "../Logger.h"
#include <string.h>

HDLinkedListNode::HDLinkedListNode()
{
	m_bValid = false;
}

HDLinkedListNode::HDLinkedListNode(const HDLinkedListNode &node)
{
	m_ucFlag = node.m_ucFlag;
	m_ulNextPtr = node.m_ulNextPtr;
	m_ulPrePtr = node.m_ulPrePtr;
	m_ulSelfPtr = node.m_ulSelfPtr;
	m_strKey = node.m_strKey;
	m_strValue = node.m_strValue;
	m_bValid = node.m_bValid;
	m_ullTimestamp = node.m_ullTimestamp;
	m_lExpireMinutes = node.m_lExpireMinutes;
}

HDLinkedListNode::HDLinkedListNode(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	m_ucFlag = 0x00;
	m_ulNextPtr = 0;
	m_ulPrePtr = 0;
	m_ulSelfPtr = 0;
	//m_ullTimestamp = Utils::get_current_time_stamp();
	m_ullTimestamp = ts;
	m_bValid = true;
	m_lExpireMinutes = lExpireMinutes;
	if (k.length() <= Global::MAX_KEY_LENGTH)
		m_strKey = k;
	else
		m_strKey = k.substr(0, Global::MAX_KEY_LENGTH);

	if(v.length() <= Global::MAX_VALUE_LENGTH)
		m_strValue = v;
	else
		m_strValue = k.substr(0, Global::MAX_VALUE_LENGTH);
}

HDLinkedListNode::HDLinkedListNode(const HCHAR *pszBuf, HUINT32 nBufLen)
{
	m_bValid = false;
	if (pszBuf != NULL && nBufLen > 0)
	{
		HUINT8 checksum = pszBuf[0];
		if (checksum == Utils::get_check_sum(pszBuf+sizeof(HUINT8), nBufLen- sizeof(HUINT8)))
		{
			HUINT32 offset = sizeof(HUINT8);
			memcpy(&m_ucFlag, pszBuf+offset, sizeof(m_ucFlag));
			offset += sizeof(m_ucFlag);

			memcpy(&m_ullTimestamp, pszBuf+offset, sizeof(HUINT64));
			offset += sizeof(HUINT64);

			memcpy(&m_lExpireMinutes, pszBuf+offset, sizeof(HINT32));
			offset += sizeof(HINT32);

			memcpy(&m_ulNextPtr, pszBuf+offset, sizeof(m_ulNextPtr));
			offset += sizeof(m_ulNextPtr);

			memcpy(&m_ulPrePtr, pszBuf+offset, sizeof(m_ulPrePtr));
			offset += sizeof(m_ulPrePtr);

			memcpy(&m_ulSelfPtr, pszBuf+offset, sizeof(m_ulSelfPtr));
			offset += sizeof(m_ulSelfPtr);

			HUINT32 ulKeyLen = 0;
			memcpy(&ulKeyLen, pszBuf+offset, sizeof(ulKeyLen));
			offset += sizeof(ulKeyLen);

			HUINT32 ulValueLen = 0;
			memcpy(&ulValueLen, pszBuf+offset, sizeof(ulValueLen));
			offset += sizeof(ulValueLen);

			m_strKey.append(pszBuf+offset, ulKeyLen);
			offset += ulKeyLen;

			m_strValue.append(pszBuf+offset, ulValueLen);
			offset += ulValueLen;

			m_bValid = true;
		}
		else
		{
			Logger::log_w("Checksum is Invalid!");
		}
	}
}

HDLinkedListNode::~HDLinkedListNode(void)
{
	m_bValid = false;
	m_strKey.clear();
	m_strValue.clear();
}

const string &HDLinkedListNode::key()
{
	return m_strKey;
}

const string &HDLinkedListNode::value()
{
	return m_strValue;
}

void HDLinkedListNode::set_deleted(HBOOL deleted)
{
	if (deleted) 
		m_ucFlag |= 0x01;
	else
		m_ucFlag &= 0x00;
}

HUINT64 HDLinkedListNode::get_time_stamp() const
{
	return m_ullTimestamp;
}

HBOOL HDLinkedListNode::is_deleted()
{
	return (m_ucFlag&0x01);
}

string HDLinkedListNode::buffer()
{
	string _str;
	_str.append((const HCHAR *)&m_ucFlag, sizeof(m_ucFlag));
	_str.append((const HCHAR *)&m_ullTimestamp, sizeof(m_ullTimestamp));
	_str.append((const HCHAR *)&m_lExpireMinutes, sizeof(m_lExpireMinutes));
	_str.append((const HCHAR *)&m_ulNextPtr, sizeof(m_ulNextPtr));
	_str.append((const HCHAR *)&m_ulPrePtr, sizeof(m_ulPrePtr));
	_str.append((const HCHAR *)&m_ulSelfPtr, sizeof(m_ulSelfPtr));

	HUINT32 ulKeyLen = m_strKey.length();
	_str.append((const HCHAR *)&ulKeyLen, sizeof(ulKeyLen));

	HUINT32 ulValueLen = m_strValue.length();
	_str.append((const HCHAR *)&ulValueLen, sizeof(ulValueLen));

	if (m_strKey.length() <= Global::MAX_KEY_LENGTH)
		_str += m_strKey;
	else
		_str += m_strKey.substr(0, Global::MAX_KEY_LENGTH);
	
	if (m_strValue.length() <= Global::MAX_VALUE_LENGTH)
		_str += m_strValue;
	else
		_str += m_strValue.substr(0, Global::MAX_VALUE_LENGTH);

	HUINT8 checksum = Utils::get_check_sum(_str.c_str(), _str.size());

	string str;
	str += checksum;
	str += _str;

	return str;
}

HBOOL HDLinkedListNode::is_valid()
{
	return m_bValid;
}

HINT32 HDLinkedListNode::next() const
{
	return m_ulNextPtr;
}

HINT32 HDLinkedListNode::pre() const
{
	return m_ulPrePtr;
}

HINT32 HDLinkedListNode::self() const
{
	return m_ulSelfPtr;
}

HUINT32 HDLinkedListNode::size()
{
	HUINT32 s = sizeof(HUINT8)
		+sizeof(HUINT64)
		+sizeof(HINT32)
		+sizeof(HUINT8)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HUINT32)
		+sizeof(HUINT32)
		+m_strKey.length()
		+m_strValue.length();
	return s;
}

HUINT32 HDLinkedListNode::max_size()
{
	HUINT32 s = sizeof(HUINT8)
		+sizeof(HUINT64)
		+sizeof(HINT32)
		+sizeof(HUINT8)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HUINT32)
		+sizeof(HUINT32)
		+Global::MAX_KEY_LENGTH
		+Global::MAX_VALUE_LENGTH;
	return s;
}

HUINT32 HDLinkedListNode::min_size()
{
	HUINT32 s = sizeof(HUINT8)
		+sizeof(HUINT64)
		+sizeof(HINT32)
		+sizeof(HUINT8)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HINT32)
		+sizeof(HUINT32)
		+sizeof(HUINT32);

	return s;
}

void HDLinkedListNode::set_self(HINT32 s)
{
	m_ulSelfPtr = s;
}

void HDLinkedListNode::set_next(HINT32 n)
{
	m_ulNextPtr = n;
}

void HDLinkedListNode::set_pre(HINT32 p)
{
	m_ulPrePtr = p;
}

void HDLinkedListNode::set_value(const string &v)
{
	m_strValue = v;
}

HINT32 HDLinkedListNode::get_expire_minutes() const
{
	return m_lExpireMinutes;
}