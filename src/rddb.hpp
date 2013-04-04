/*
 * rrdb.hpp
 *
 *  Created on: 2013-3-27
 *      Author: wqy
 */

#ifndef RRDB_HPP_
#define RRDB_HPP_
#include <stdint.h>
#include <map>
#include <list>
#include <string>
#include "common.hpp"
#include "rddb_data.hpp"
#include "slice.hpp"
#include "util/helpers.hpp"
#include "util/buffer_helper.hpp"
#include "util/sds.h"

#define RDDB_OK 0
#define ERR_INVALID_STR -5
#define ERR_DB_NOT_EXIST -6
#define ERR_KEY_EXIST -7
#define ERR_INVALID_TYPE -8
#define ERR_OUTOFRANGE -9
#define ERR_NOT_EXIST -10

namespace rddb
{

	enum ValueDataType
	{
		EMPTY = 0, INTEGER = 1, DOUBLE = 2, RAW = 3
	};

	enum OperationType
	{
		NOOP = 0, ADD = 1
	};

	struct ValueObject
	{
			uint8_t type;
			union
			{
					int64_t int_v;
					double double_v;
					Buffer* raw;
			} v;
			uint64_t expire;
			ValueObject() :
					type(RAW), expire(0)
			{
				v.int_v = 0;
			}
			void Clear()
			{
				if (type == RAW && NULL != v.raw)
				{
					DELETE(v.raw);
				}
				v.int_v = 0;
			}
			~ValueObject()
			{
				if (type == RAW && v.raw != NULL)
				{
					delete v.raw;
				}
			}
	};

	struct Iterator
	{
			virtual void Next() = 0;
			virtual void Prev() = 0;
			virtual Slice Key() const = 0;
			virtual Slice Value() const = 0;
			virtual bool Valid() = 0;
			virtual ~Iterator()
			{
			}
	};

	struct KeyValueEngine
	{
			virtual int Get(const Slice& key, std::string* value) = 0;
			virtual int Put(const Slice& key, const Slice& value) = 0;
			virtual int Del(const Slice& key) = 0;
			virtual int BeginBatchWrite() = 0;
			virtual int CommitBatchWrite() = 0;
			virtual int DiscardBatchWrite() = 0;
			virtual Iterator* Find(const Slice& findkey) = 0;
			virtual ~KeyValueEngine()
			{
			}
	};

	class BatchWriteGuard
	{
		private:
			KeyValueEngine* m_engine;
			bool m_success;
		public:
			BatchWriteGuard(KeyValueEngine* engine) :
					m_engine(engine), m_success(true)
			{
				if (NULL != m_engine)
				{
					m_engine->BeginBatchWrite();
				}
			}
			void MarkFailed()
			{
				m_success = false;
			}
			~BatchWriteGuard()
			{
				if (NULL != m_engine)
				{
					if (m_success)
					{
						m_engine->CommitBatchWrite();
					} else
					{
						m_engine->DiscardBatchWrite();
					}
				}
			}
	};

	typedef uint64_t DBID;
	struct KeyValueEngineFactory
	{
			virtual KeyValueEngine* CreateDB(DBID db) = 0;
			virtual void DestroyDB(KeyValueEngine* engine) = 0;
			virtual ~KeyValueEngineFactory()
			{
			}
	};

	class RDDB
	{
		private:
			static void EncodeValue(Buffer& buf, const ValueObject& value);
			static bool DecodeValue(Buffer& buf, ValueObject& value);
			static void FillValueObject(const Slice& value,
					ValueObject& valueobj);
			static Buffer* ValueObject2RawBuffer(ValueObject& valueobj);
			static size_t RealPosition(Buffer* buf, int pos);

			KeyValueEngineFactory* m_engine_factory;
			typedef std::map<DBID, KeyValueEngine*> KeyValueEngineTable;
			KeyValueEngineTable m_engine_table;
			KeyValueEngine* GetDB(DBID db);

			int SetExpiration(DBID db, const Slice& key, uint64_t expire);
			int GetValue(DBID db, const KeyObject& key, ValueObject& v);
			int SetValue(DBID db, KeyObject& key, ValueObject& value);
			int DelValue(DBID db, KeyObject& key);
			Iterator* FindValue(DBID db, KeyObject& key);
			int SetHashValue(DBID db, const Slice& key, const Slice& field,
					ValueObject& value);
			int ListPush(DBID db, const Slice& key, const Slice& value,
					bool athead, bool onlyexist);
			int ListPop(DBID db, const Slice& key, bool athead,
					ValueObject& value);
		public:
			RDDB(KeyValueEngineFactory* factory);
			int Set(DBID db, const Slice& key, const Slice& value);
			int SetNX(DBID db, const Slice& key, const Slice& value);
			int SetEx(DBID db, const Slice& key, const Slice& value,
					uint32_t secs);
			int PSetEx(DBID db, const Slice& key, const Slice& value,
					uint32_t ms);
			int Get(DBID db, const Slice& key, ValueObject& value);
			int Del(DBID db, const Slice& key);
			bool Exists(DBID db, const Slice& key);
			int Expire(DBID db, const Slice& key, uint32_t secs);
			int Expireat(DBID db, const Slice& key, uint32_t ts);
			int Persist(DBID db, const Slice& key);
			int Pexpire(DBID db, const Slice& key, uint32_t ms);
			int Strlen(DBID db, const Slice& key);
			int Keys(DBID db, const std::string& pattern,
					std::list<std::string>& ret)
			{
				return -1;
			}
			int Move(DBID srcdb, const Slice& key, DBID dstdb);

			int Append(DBID db, const Slice& key, const Slice& value);
			int Decr(DBID db, const Slice& key, int64_t& value);
			int Decrby(DBID db, const Slice& key, int64_t decrement,
					int64_t& value);
			int Incr(DBID db, const Slice& key, int64_t& value);
			int XIncrby(DBID db, const Slice& key, int64_t increment);
			int Incrby(DBID db, const Slice& key, int64_t increment,
					int64_t& value);
			int IncrbyFloat(DBID db, const Slice& key, double increment,
					double& value);
			int GetRange(DBID db, const Slice& key, int start, int end,
					ValueObject& valueobj);
			int SetRange(DBID db, const Slice& key, int start,
					const Slice& value);
			int GetSet(DBID db, const Slice& key, const Slice& value,
					ValueObject& valueobj);

			int HSet(DBID db, const Slice& key, const Slice& field,
					const Slice& value);
			int HSetNX(DBID db, const Slice& key, const Slice& field,
					const Slice& value);
			int HDel(DBID db, const Slice& key, const Slice& field);
			bool HExists(DBID db, const Slice& key, const Slice& field);
			int HGet(DBID db, const Slice& key, const Slice& field,
					ValueObject& value);
			int HIncrby(DBID db, const Slice& key, const Slice& field,
					int64_t increment, int64_t& value);
			int HIncrbyFloat(DBID db, const Slice& key, const Slice& field,
					double increment, double& value);

			int LPush(DBID db, const Slice& key, const Slice& value);
			int LPushx(DBID db, const Slice& key, const Slice& value);
			int RPush(DBID db, const Slice& key, const Slice& value);
			int RPushx(DBID db, const Slice& key, const Slice& value);
			int LPop(DBID db, const Slice& key, ValueObject& v);
			int RPop(DBID db, const Slice& key, ValueObject& v);
			int LLen(DBID db, const Slice& key);

			int ZAdd(DBID db, const Slice& key, double score,
					const Slice& value);
			int ZCard(DBID db, const Slice& key);
			int ZScore(DBID db, const Slice& key, const Slice& value,
					double& score);
			int ZRem(DBID db, const Slice& key, const Slice& value);
			int ZCount(DBID db, const Slice& key, const std::string& min,
					const std::string& max);
			int ZIncrby(DBID db, const Slice& key, double increment,
					const Slice& value, double& score);

			int SAdd(DBID db, const Slice& key, const Slice& value);
			int SCard(DBID db, const Slice& key);
			int SIsMember(DBID db, const Slice& key, const Slice& value);
			int SRem(DBID db, const Slice& key, const Slice& value);
	};
}

#endif /* RRDB_HPP_ */