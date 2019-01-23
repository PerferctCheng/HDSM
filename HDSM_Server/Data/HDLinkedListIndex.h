#pragma once
#include <map>
#include <vector>
#include "HDLinkedListNode.h"
#include "IHDLinkedList.h"
#include "TypeDefine.h"
using namespace std;

typedef struct tagInterval
{
	HINT32 lStartOffset;
	HINT32 lEndOffset;
	tagInterval()
	{
		lStartOffset = 0;
		lEndOffset = 0;
	}

	tagInterval(HINT32 s, HINT32 e)
	{
		lStartOffset = s;
		lEndOffset = e;
	}
}interval;

enum HDLIST_INDEX_LEVEL
{
	Level_None = 0,
	Level_1,
	Level_2,
	Level_3,
	Level_4,
	Level_5,
};

class HDLinkedListIndex
{
public:
	HDLinkedListIndex(IHDLinkedList *pList, HBOOL bEnableHighLevelIndex);
	virtual ~HDLinkedListIndex(void);
public:
	void insert(const string &k, HINT32 offset, HBOOL init = false);
	interval get(const string &k, HINT32 offset, HUINT32 &level);
	interval query(const string &k, HINT32 level);
	void erase(HDLinkedListNode *pNode);
	void clear();
	vector<HUINT32> size();
	HDLIST_INDEX_LEVEL get_top_level();
private:
	void update(map<string, interval> &m, HUINT32 level, const string &k, HINT32 offset, HBOOL init);
	interval query(map<string, interval> &m, HUINT32 level, const string &k);
	void erase(map<string, interval> &m, HUINT32 level, HDLinkedListNode *pNode);
private:
	map<string, interval>	m_Intervals1;
	map<string, interval>	m_Intervals2;
	map<string, interval>	m_Intervals3;
	map<string, interval>	m_Intervals4;
	map<string, interval>	m_Intervals5;
private:
	IHDLinkedList *m_pList;
	HBOOL m_bEnableHighLevelIndex;
};

