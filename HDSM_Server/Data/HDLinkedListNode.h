#pragma once
#include <string>
#include "TypeDefine.h"
using namespace std;

class HDLinkedListNode
{
public:
	HDLinkedListNode();
	HDLinkedListNode(const HDLinkedListNode &node);
	HDLinkedListNode(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes);
	HDLinkedListNode(const HCHAR *pszBuf, HUINT32 nBufLen);
	~HDLinkedListNode(void);
public:
	string buffer();
	void set_deleted(HBOOL deleted);
	HBOOL is_deleted();
	HBOOL is_valid();
	HUINT32 size();
	static HUINT32 max_size();
	static HUINT32 min_size();
	void set_self(HINT32 s);
	void set_next(HINT32 n);
	void set_pre(HINT32 p);
	void set_value(const string &v);
public:
	const string &key();
	const string &value();
	HINT32 next() const;
	HINT32 pre() const;
	HINT32 self() const;
	HUINT64 get_time_stamp() const;
	HINT32 get_expire_minutes() const;
private:
	HUINT8	m_ucFlag;
	HINT32			m_ulNextPtr;
	HINT32			m_ulPrePtr;
	HINT32			m_ulSelfPtr;
	string			m_strKey;
	string			m_strValue;
	HBOOL			m_bValid;
	HUINT64 m_ullTimestamp;
	HINT32			m_lExpireMinutes;
};

