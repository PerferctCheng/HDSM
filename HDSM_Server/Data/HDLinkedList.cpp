#include "HDLinkedList.h"
#include "Utils.h"
#include "../Global.h"
#include "../Logger.h"
#include <string.h>

#define GET_NODE(_pNode_, _offset_)					\
	HDLinkedListNode _node_ = get_node(_offset_);	\
	if (_node_.is_valid())							\
		_pNode_ = &_node_;							\
	else											\
		_pNode_ = NULL;

HDLinkedList::HDLinkedList(const string &strDataFile) 
	:	FILE_TAG("HDLINKEDLIST"),
		OFFSET_LENGTH_FIELD(strlen(FILE_TAG)+sizeof(Global::VERSION_CODE)),
		OFFSET_STARTOFFSET_FIELD(strlen(FILE_TAG)+sizeof(Global::VERSION_CODE)+sizeof(m_ulLength)+sizeof(m_ulInvalidLength)),
		OFFSET_INVALID_LENGTH_FIELD(strlen(FILE_TAG)+sizeof(Global::VERSION_CODE)+sizeof(m_ulLength))
{
	m_bInitialized = false;
	m_lFileEndPos = 0;
	m_ulLength = 0;
	m_ulInvalidLength = 0;
	m_lStartOffset = strlen(FILE_TAG)+sizeof(Global::VERSION_CODE)+sizeof(m_ulLength)+sizeof(m_ulInvalidLength)+sizeof(m_lStartOffset);
	m_strDataFile = strDataFile;
	m_pBufPool = new BufferPool(Global::MAX_BUFFER_ROOMS_COUNT, Global::MAX_BUFFER_ROOMS_SIZE);
	m_pIndex = new HDLinkedListIndex(this, ConfigureMgr::get_enable_high_level_index());
	m_pExpireCache = new HDLinkedListExpireCache();

}

HDLinkedList::~HDLinkedList(void)
{
	if (m_pBufPool != NULL)
		delete m_pBufPool; 
	if (m_fpDataFile != NULL)
		fclose(m_fpDataFile);
	if (m_pIndex != NULL)
		delete m_pIndex;
	if (m_pExpireCache != NULL)
		delete m_pExpireCache;
}

HBOOL HDLinkedList::init()
{
	if (!m_bInitialized)
	{
		m_bInitialized = true;
		m_bValid = _init();
	}
	return m_bValid;
}

HBOOL HDLinkedList::_init()
{
	if (!Utils::file_exist(m_strDataFile))
	{
		m_fpDataFile = fopen(m_strDataFile.c_str(), "wb+");
		if (m_fpDataFile != NULL)
		{
			fwrite(FILE_TAG, strlen(FILE_TAG), 1, m_fpDataFile);
			m_lFileEndPos += strlen(FILE_TAG);
			fwrite((const HCHAR *)&Global::VERSION_CODE, sizeof(HUINT32), 1, m_fpDataFile);
			m_lFileEndPos += sizeof(HUINT32);
			fwrite((const HCHAR *)&m_ulLength, sizeof(HUINT32), 1, m_fpDataFile);
			m_lFileEndPos += sizeof(HUINT32);
			fwrite((const HCHAR *)&m_ulInvalidLength, sizeof(HUINT32), 1, m_fpDataFile);
			m_lFileEndPos += sizeof(HUINT32);
			fwrite((const HCHAR *)&m_lStartOffset, sizeof(HUINT32), 1, m_fpDataFile);
			m_lFileEndPos += sizeof(HUINT32);
			fflush(m_fpDataFile);
			return true;
		}
	}
	else
	{
		m_fpDataFile = fopen(m_strDataFile.c_str(), "rb+");
		if (m_fpDataFile != NULL)
		{
			HCHAR *file_tag = new HCHAR[strlen(FILE_TAG)+1];
			memset(file_tag, 0, strlen(FILE_TAG)+1);
			fread(file_tag, strlen(FILE_TAG), 1, m_fpDataFile);
			if (strcmp(file_tag, FILE_TAG) != 0)
			{
				delete file_tag;
				fclose(m_fpDataFile);
				m_fpDataFile = NULL;
				return false;
			}
			delete file_tag;

			fread(&m_ulVersionCode, sizeof(HUINT32), 1, m_fpDataFile);
			if (m_ulVersionCode > Global::VERSION_CODE)
			{
				fclose(m_fpDataFile);
				m_fpDataFile = NULL;
				return false;
			}

			fread(&m_ulLength, sizeof(HUINT32), 1, m_fpDataFile);
			fread(&m_ulInvalidLength, sizeof(HUINT32), 1, m_fpDataFile);

			fread(&m_lStartOffset, sizeof(HUINT32), 1, m_fpDataFile);
			if ((HUINT32)m_lStartOffset < (strlen(FILE_TAG)+sizeof(Global::VERSION_CODE)+sizeof(m_ulLength)+sizeof(m_ulInvalidLength)+sizeof(m_lStartOffset)))
			{
				fclose(m_fpDataFile);
				m_fpDataFile = NULL;
				return false;
			}

			fseek(m_fpDataFile, 0, SEEK_END);
			m_lFileEndPos = ftell(m_fpDataFile);

			init_indexs();
			return true;
		}
	}
	
	return false;
}

HBOOL HDLinkedList::is_valid()
{
	return m_bValid;
}

void HDLinkedList::init_indexs()
{
	if (m_fpDataFile != NULL)
	{
		if (m_ulLength > 0)
		{
			Logger::log_i("Loading Data Index: %s", m_strDataFile.c_str());
			HDLinkedListNode *pNode = NULL;
			HINT32 nextoffset = m_lStartOffset;	
			//HINT32 k = 0;
			while (nextoffset != 0)
			{
				GET_NODE(pNode, nextoffset);
				if (pNode != NULL)
				{
					if (pNode->get_expire_minutes() >= 0)
					{
						HINT32 ulExpiredMS = pNode->get_expire_minutes()*Global::SECONDS_IN_ONE_MINUTE;
						if (Utils::get_current_time_stamp() - pNode->get_time_stamp() > ulExpiredMS)
						{
							nextoffset = pNode->next();
							_erase(pNode);
							continue;
						}
						else
						{
							m_pExpireCache->post(pNode);
						}
					}

					m_pIndex->insert(pNode->key(), pNode->self(), true);
					nextoffset = pNode->next();
				}
				else
					break;
			}

			Logger::log_i("Loading Data Index Completed: %s", m_strDataFile.c_str());
		}
	}

	return;
}

void HDLinkedList::update_node(HDLinkedListNode &node)
{
	//大小不能变，只能更新前后指针值
	fseek(m_fpDataFile, node.self()+sizeof(HUINT32), SEEK_SET);
	fwrite(node.buffer().c_str(), node.size(), 1, m_fpDataFile);
	fflush(m_fpDataFile);
	return;
}

void HDLinkedList::add_node(HDLinkedListNode &node)
{
	fseek(m_fpDataFile, node.self(), SEEK_SET);
	HUINT32 ulNodeLen = node.size();
	fwrite(&ulNodeLen, sizeof(HUINT32), 1, m_fpDataFile);
	fwrite(node.buffer().c_str(), node.size(), 1, m_fpDataFile);

	m_ulLength++;
	fseek(m_fpDataFile, OFFSET_LENGTH_FIELD, SEEK_SET);
	fwrite(&m_ulLength, sizeof(m_ulLength), 1, m_fpDataFile);
	fflush(m_fpDataFile);

	m_lFileEndPos += sizeof(HUINT32);
	m_lFileEndPos += node.size();

	m_pExpireCache->post(&node);

	return;
}

void HDLinkedList::update_length()
{
	fseek(m_fpDataFile, OFFSET_LENGTH_FIELD, SEEK_SET);
	fwrite(&m_ulLength, sizeof(m_ulLength), 1, m_fpDataFile);
	fflush(m_fpDataFile);
	return;
}

void HDLinkedList::update_invalid_length()
{
	fseek(m_fpDataFile, OFFSET_INVALID_LENGTH_FIELD, SEEK_SET);
	fwrite(&m_ulInvalidLength, sizeof(m_ulInvalidLength), 1, m_fpDataFile);
	fflush(m_fpDataFile);
	return;
}

void HDLinkedList::update_start_offset()
{
	fseek(m_fpDataFile, OFFSET_STARTOFFSET_FIELD, SEEK_SET);
	fwrite(&m_lStartOffset, sizeof(m_lStartOffset), 1, m_fpDataFile);
	fflush(m_fpDataFile);
	return;
}

HBOOL HDLinkedList::put_when_length_eq_zero(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	m_pIndex->insert(k, m_lFileEndPos);

	m_lStartOffset = m_lFileEndPos;
	update_start_offset();	

	HDLinkedListNode node(k, v, ts, lExpireMinutes);
	node.set_self(m_lFileEndPos);
	add_node(node);

	return true;
}

HBOOL HDLinkedList::put_when_length_uneq_zero(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	HDLinkedListNode *pNode = NULL;

	HINT32 start = m_lStartOffset;
	HINT32 end = 0;
	HUINT32 level = Level_None;

	interval val = m_pIndex->get(k, m_lFileEndPos, level);
	if (level > Level_None)
	{
		start = val.lStartOffset;
		end = val.lEndOffset;
	}
	m_pIndex->insert(k, m_lFileEndPos);

	HINT32 nextoffset = start;
	while (nextoffset != 0)
	{
		GET_NODE(pNode, nextoffset);
		if (pNode != NULL)
		{
			if (level == m_pIndex->get_top_level())
			{
				HINT32 r = pNode->key().compare(k);
				if (r < 0)
				{
					nextoffset = pNode->next();
					if (nextoffset == 0)
					{
						HDLinkedListNode node(k, v, ts, lExpireMinutes);
						node.set_self(m_lFileEndPos);
						node.set_next(0);
						node.set_pre(pNode->self());
						add_node(node);

						pNode->set_next(node.self());
						update_node(*pNode);
						return true;
					}

				}
				else if (r == 0)//相等
				{
					if (pNode->value().compare(v) != 0)
						return false;
					else
						return true;
				}
				else
				{
					HDLinkedListNode node(k, v, ts, lExpireMinutes);
					node.set_self(m_lFileEndPos);
					node.set_next(pNode->self());
					node.set_pre(pNode->pre());
					add_node(node);

					if (pNode->self() == m_lStartOffset)
					{
						m_lStartOffset = node.self();
						update_start_offset();
					}

					HDLinkedListNode *pPreNode = NULL;
					GET_NODE(pPreNode, pNode->pre());
					if (pPreNode != NULL)
					{
						pPreNode->set_next(node.self());
						update_node(*pPreNode);
					}

					pNode->set_pre(node.self());
					update_node(*pNode);

					return true;
				}
			}
			else
			{
				HINT32 r = pNode->key().compare(k);
				if (r < 0)
				{
					HUINT32 lEndOffset = m_pIndex->query(pNode->key(), level+1).lEndOffset;
					if (lEndOffset != nextoffset)
					{
						HDLinkedListNode *pEndNode = NULL;
						GET_NODE(pEndNode, lEndOffset);
						if (pEndNode == NULL)
						{
							return false;
						}
						else
						{
							nextoffset = pEndNode->next();
							if (nextoffset == 0)
							{
								HDLinkedListNode node(k, v, ts, lExpireMinutes);
								node.set_self(m_lFileEndPos);
								node.set_next(0);
								node.set_pre(pEndNode->self());
								add_node(node);

								pEndNode->set_next(node.self());
								update_node(*pEndNode);
								return true;
							}
						}
					}
					else
					{
						nextoffset = pNode->next();
						if (nextoffset == 0)
						{
							HDLinkedListNode node(k, v, ts, lExpireMinutes);
							node.set_self(m_lFileEndPos);
							node.set_next(0);
							node.set_pre(pNode->self());
							add_node(node);

							pNode->set_next(node.self());
							update_node(*pNode);
							return true;
						}

					}
				}
				else if (r > 0)
				{
					HDLinkedListNode node(k, v, ts, lExpireMinutes);
					node.set_self(m_lFileEndPos);
					node.set_next(pNode->self());
					node.set_pre(pNode->pre());
					add_node(node);

					if (pNode->self() == m_lStartOffset)
					{
						m_lStartOffset = node.self();
						update_start_offset();
					}

					HDLinkedListNode *pPreNode = NULL;
					GET_NODE(pPreNode, pNode->pre());
					if (pPreNode != NULL)
					{
						pPreNode->set_next(node.self());
						update_node(*pPreNode);
					}

					pNode->set_pre(node.self());
					update_node(*pNode);

					return true;
				}
				else
				{
					Logger::log_w("Find invaild index when put a key [%s]!!!", k.c_str());
					return false;
				}
			}
		}
		else
			break;
	}

	return false;
}

HBOOL HDLinkedList::_put(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	if (m_fpDataFile == NULL) 
		return false;

	if (m_ulLength == 0)
		return put_when_length_eq_zero(k, v, ts, lExpireMinutes);
	else
		return put_when_length_uneq_zero(k, v, ts, lExpireMinutes);
}

HBOOL HDLinkedList::_erase(const string &k)
{
	if (m_fpDataFile == NULL) 
		return false;
	HDLinkedListNode node = get_node_by_key(k);
	if (!node.is_valid())
		return false;
	return _erase(&node);
}

HBOOL HDLinkedList::_erase(HDLinkedListNode *pNode)
{
	if (pNode != NULL)
	{	
		m_pIndex->erase(pNode);

		if (1)
		{
			HDLinkedListNode *pPreNode = NULL;
			GET_NODE(pPreNode, pNode->pre());
			if (pPreNode != NULL)
			{
				pPreNode->set_next(pNode->next());
				update_node(*pPreNode);
			}
		}

		if (1)
		{
			HDLinkedListNode *pNextNode = NULL;
			GET_NODE(pNextNode, pNode->next());
			if (pNextNode != NULL)
			{
				pNextNode->set_pre(pNode->pre());
				update_node(*pNextNode);
			}
		}

		if (pNode->self() == m_lStartOffset && pNode->next() != 0)
		{
			m_lStartOffset = pNode->next();
			update_start_offset();
		}

		m_ulLength--;
		update_length();
		m_ulInvalidLength++;
		update_invalid_length();

		pNode->set_deleted(true);
		update_node(*pNode);

		return true;
	}
	else
		return false;
}

HDLinkedListNode HDLinkedList::get_node_by_key(const string &k)
{
	HINT32 start = 0;
	HINT32 end = 0;

	interval val = m_pIndex->query(k, m_pIndex->get_top_level());
	if (val.lStartOffset > 0 && val.lEndOffset > 0)
	{
		start = val.lStartOffset;
		end = val.lEndOffset;
	}
	else
	{
		return HDLinkedListNode(NULL, 0);
	}

	HDLinkedListNode *pNode = NULL;
	HINT32 nextoffset = start;
	while (nextoffset != 0)
	{
		GET_NODE(pNode, nextoffset);
		if (pNode != NULL)
		{
			if (!pNode->is_deleted() && pNode->key().compare(k) == 0)//找到
			{
				return *pNode;
			}
			else if (pNode->next() == 0)
			{
				return HDLinkedListNode(NULL, 0);
			}
			else
			{
				if (pNode->self() == end)
				{
					return HDLinkedListNode(NULL, 0);
				}
				else
				{
					nextoffset = pNode->next();
				}
			}
		}
		else
			break;
	}

	return HDLinkedListNode(NULL, 0);
}

HBOOL HDLinkedList::_get(const string &k, string &v, HINT32 &lExpireMinutes, HDLinkedListNode *p)
{
	if (m_fpDataFile == NULL) 
		return false;

	HINT32 start = 0;
	HINT32 end = 0;
	interval val = m_pIndex->query(k, m_pIndex->get_top_level());
	if (val.lStartOffset > 0 && val.lEndOffset > 0)
	{
		start = val.lStartOffset;
		end = val.lEndOffset;
	}
	else
	{
		return false;
	}

	//Logger::log_i("key=%s, start=%d, end=%d",k.c_str(), start, end);

	HDLinkedListNode *pNode = NULL;
	HINT32 nextoffset = start;
	while (nextoffset != 0)
	{
		GET_NODE(pNode, nextoffset);
		if (pNode != NULL)
		{
			if (!pNode->is_deleted() && pNode->key().compare(k) == 0)//找到
			{
				v = pNode->value();
				lExpireMinutes = pNode->get_expire_minutes();
				if (p != NULL)
					*p = *pNode;
				return true;
			}
			else if (pNode->next() == 0)
			{
				return false;
			}
			else
			{
				if (pNode->self() == end)
					return false;
				else
					nextoffset = pNode->next();
			}	
		}
		else
			break;
	}

	return false;
}

HBOOL HDLinkedList::exists(const string &k)
{
	string v;
	HINT32 lExpireMinutes = -1;
	return get(k, v, lExpireMinutes);
}

HUINT32 HDLinkedList::length()
{
	return m_ulLength;
}

HUINT32 HDLinkedList::version()
{
	return m_ulVersionCode;
}

HDLinkedListNode HDLinkedList::get_node(HINT32 offset)
{
	FILE *fp = m_fpDataFile;
	if (fp == NULL || offset == 0) 
		return HDLinkedListNode(NULL, 0);

	HINT32 iBufferPoolIndex = -1;
	if (m_pBufPool != NULL)
		iBufferPoolIndex = (*m_pBufPool).lease();
	if (iBufferPoolIndex == -1)
		return HDLinkedListNode(NULL, 0);

	HUINT32 ulNodeLen = 0;
	m_SeekFileLock.lock();
	fseek(fp, offset, SEEK_SET);
	fread(&ulNodeLen, sizeof(HUINT32), 1, fp);
	if (ulNodeLen > HDLinkedListNode::min_size() && ulNodeLen <= HDLinkedListNode::max_size())
		fread((*m_pBufPool).get(iBufferPoolIndex), ulNodeLen, 1, fp);
	else
		ulNodeLen = 0;
	m_SeekFileLock.unlock();

	HDLinkedListNode node((HCHAR *)(*m_pBufPool).get(iBufferPoolIndex), ulNodeLen);
	(*m_pBufPool).ret(iBufferPoolIndex);

	if (!node.is_valid())
		Logger::log_w("A invalid key in the data file : %s!", m_strDataFile.c_str());
	return node;
}

void HDLinkedList::_output_to_map(map<string, KeyValueInfo> &mapOutput, HUINT32 limit)
{
	if (m_fpDataFile != NULL)
	{
		if (m_ulLength > 0)
		{
			HUINT32 count = 0;
			HINT32 nextoffset = m_lStartOffset;
			HDLinkedListNode *pNode = NULL;
			while (nextoffset != 0 && count < limit)
			{
				GET_NODE(pNode, nextoffset);
				if (pNode != NULL)
				{
					count++;
					KeyValueInfo kv(pNode->key(), pNode->value(), pNode->get_expire_minutes(), pNode->get_time_stamp());
					mapOutput[pNode->key()] = kv;
					nextoffset = pNode->next();
				}
				else
					break;
			}
		}
	}

	return;
}

HUINT32 HDLinkedList::invalid_length()
{
	return m_ulInvalidLength;
}

HBOOL HDLinkedList::_trim_for_erase()
{
	if (m_fpDataFile == NULL) 
		return false;

	Logger::log_i("Trimming Data File: %s", m_strDataFile.c_str());

	string strTempDataFile = m_strDataFile + ".temp";
	FILE *fp = fopen(strTempDataFile.c_str(), "wb");
	if (fp != NULL)
	{
		m_pIndex->clear();
		m_lFileEndPos = 0;
		m_ulInvalidLength = 0;

		fwrite(FILE_TAG, strlen(FILE_TAG), 1, fp);
		m_lFileEndPos += strlen(FILE_TAG);
		fwrite((const HCHAR *)&Global::VERSION_CODE, sizeof(HUINT32), 1, fp);
		m_lFileEndPos += sizeof(HUINT32);
		fwrite((const HCHAR *)&m_ulLength, sizeof(HUINT32), 1, fp);
		m_lFileEndPos += sizeof(HUINT32);
		fwrite((const HCHAR *)&m_ulInvalidLength, sizeof(HUINT32), 1, fp);
		m_lFileEndPos += sizeof(HUINT32);
		HINT32 lTempStartOffset = m_lFileEndPos+sizeof(HUINT32);
		fwrite((const HCHAR *)&lTempStartOffset, sizeof(HUINT32), 1, fp);
		m_lFileEndPos += sizeof(HUINT32);

		if (m_fpDataFile != NULL)
		{
			if (m_ulLength > 0)
			{
				HINT32 nextoffset = m_lStartOffset;
				HDLinkedListNode *pNode = NULL;
				HDLinkedListNode *pPreNode = NULL;
				while (nextoffset != 0)
				{
					GET_NODE(pNode, nextoffset);
					if (pNode != NULL)
					{
						nextoffset = pNode->next();
						pNode->set_self(m_lFileEndPos);
						if (pNode->next() != 0)
							pNode->set_next(m_lFileEndPos + pNode->size());
						if (pNode->pre() != 0 && pPreNode != NULL)
							pNode->set_pre(m_lFileEndPos - pPreNode->size());

						HUINT32 ulNodeLen = pNode->size();
						fwrite(&ulNodeLen, sizeof(HUINT32), 1, fp);
						m_lFileEndPos += sizeof(HUINT32);
						fwrite(pNode->buffer().c_str(), pNode->size(), 1, fp);
						m_lFileEndPos += pNode->size();
						fflush(fp);

						m_pIndex->insert(pNode->key(), pNode->self(), true);

						pPreNode = pNode;
					}
					else
						break;
				}
			}
		}
		fclose(fp);
		fclose(m_fpDataFile);
		m_lStartOffset = lTempStartOffset;
			
		Utils::delete_file(m_strDataFile);
		Utils::rename_file(strTempDataFile, m_strDataFile);
		m_fpDataFile = fopen(m_strDataFile.c_str(), "rb+");
	}

	Logger::log_i("Trimming Data File Completed: %s", m_strDataFile.c_str());

	return true;
}

HUINT32 HDLinkedList::trim_for_expire()
{
	HUINT32 ret = 0;
	m_RWLock.lock(RWLock::LOCK_LEVEL_WRITE);
	ret = _trim_for_expire();
	m_RWLock.unlock();
	return ret;
}

HBOOL HDLinkedList::load_expire_cache()
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_READ);
	ret = _load_expire_cache();
	m_RWLock.unlock();
	return ret;
}

HBOOL HDLinkedList::_load_expire_cache()
{
	if (m_fpDataFile == NULL) 
		return false;

	if (m_ulLength > 0)
	{	
		m_pExpireCache->clear();
		HINT32 nextoffset = m_lStartOffset;
		HDLinkedListNode *pNode = NULL;
		while (nextoffset != 0)
		{
		 	GET_NODE(pNode, nextoffset);
		 	if (pNode != NULL)
		 	{
		 		nextoffset = pNode->next();
		 		if (pNode->get_expire_minutes() < 0)//小于0代表永久不过期
		 			continue;
				m_pExpireCache->post(pNode);
		 	}
		 	else
		 		break;
		}
	}

	return true;

}

HUINT32 HDLinkedList::_trim_for_expire()
{
	if (m_fpDataFile == NULL) 
		return 0;

	HUINT32 ulCount = 0; 
	if (m_ulLength > 0)
	{
		vector<vector<HUINT32>> vec = m_pExpireCache->peek();
		for (HUINT32 i=0; i<vec.size(); i++)
		{
			vector<HUINT32> vecNode = vec[i];
			for (HUINT32 j=0; j<vecNode.size(); j++)
			{
				HDLinkedListNode *pNode = NULL;
				GET_NODE(pNode, vecNode[j]);
				if (pNode != NULL && !pNode->is_deleted())
				{
					_erase(pNode);
					ulCount++;
				}
			}
		}
	}
	
	return ulCount;
}

HBOOL HDLinkedList::update(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_WRITE);

	HDLinkedListNode node;
	string val;
	HINT32 et = 0;
	ret = _get(k, val, et, &node);
	if (ret)
	{
		if (val.compare(v) != 0 || lExpireMinutes != 0)
		{
			ret = _erase(&node);
			if (ret)
			{
				if (lExpireMinutes != 0)
					ret = _put(k, v, ts, lExpireMinutes);
				else
					ret = _put(k, v, node.get_time_stamp(), node.get_expire_minutes());
			}
		}
	}

	m_RWLock.unlock();
	return ret;
}

HBOOL HDLinkedList::put(const string &k, const string &v, HUINT64 ts, HINT32 lExpireMinutes)
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_WRITE);
	ret = _put(k, v, ts, lExpireMinutes);
	m_RWLock.unlock();
	return ret;
}

HBOOL HDLinkedList::erase(const string &k)
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_WRITE);
	ret = _erase(k);
	if (invalid_length() > Global::MAX_INVALID_KEYS)
		_trim_for_erase();
	m_RWLock.unlock();
	return ret;
}

HBOOL HDLinkedList::get(const string &k, string &v, HINT32 &lExpireMinutes)
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_READ);
	ret = _get(k, v, lExpireMinutes);
	m_RWLock.unlock();
	return ret;
}

void HDLinkedList::output_to_map(map<string, KeyValueInfo> &m, HUINT32 limit)
{
	m_RWLock.lock(RWLock::LOCK_LEVEL_READ);
	_output_to_map(m, limit);
	m_RWLock.unlock();
	return;
}

HBOOL HDLinkedList::trim_for_erase()
{
	HBOOL ret = false;
	m_RWLock.lock(RWLock::LOCK_LEVEL_WRITE);
	ret = _trim_for_erase();
	m_RWLock.unlock();
	return ret;
}

vector<HUINT32> HDLinkedList::get_indexs_size()
{
	return m_pIndex->size();
}

HUINT32 HDLinkedList::get_expire_cache_size()
{
	return m_pExpireCache->size();
}