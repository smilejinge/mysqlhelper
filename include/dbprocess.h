#ifndef __DB_PROCESS_H__
#define __DB_PROCESS_H__


#include <stdint.h>
#include <string.h>

#include <string>

#include "first_order_constant.h"
#include "mysql.h"


struct DBHost{
	char host[128];				/*DB host*/
	uint32_t port;				/*DB port*/
	char user[64];				/*DB user*/
	char password[128];			/*DB password*/
	char character[64];			/*DB default-character-set*/
	char dbname[128];			/*DB name*/
	uint32_t connect_timeout;	/*DB connect timeout*/

	DBHost()
	{
		memset(host, sizeof(host), 0);
		port = 0;
		memset(user, sizeof(user), 0);
		memset(password, sizeof(password), 0);
		memset(character, sizeof(character), 0);
		memset(dbname, sizeof(dbname), 0);
		connect_timeout = 0;
	}

	void CopyData(DBHost *self, const DBHost *other)
	{
		STRCPY(self->host, other->host);
		self->port = other->port;
		STRCPY(self->user, other->user);
		STRCPY(self->password, other->password);
		STRCPY(self->character, other->character);
		STRCPY(self->dbname, other->dbname);
		self->connect_timeout = other->connect_timeout;
	}

	DBHost(const DBHost &dbhost)
	{
		CopyData(this, &dbhost);
	}

	DBHost &operator=(const DBHost &dbhost)
	{
		CopyData(this, &dbhost);
		return *this;
	}
};


class CDBProcess
{
public:
	CDBProcess();
	CDBProcess(const DBHost *dbhost);
	~CDBProcess();
	void SetDBHost(const DBHost *dbhost);
	void UseMatchedRows();
	int32_t Open();
	int32_t Open(const char *DBName);
	int32_t Ping();
	unsigned long EscapeString(char *dst, const char *src, unsigned long len);
	const char *GetDBName();
	int32_t SelectDataBase(const char *DBName);
	int32_t ExecSQL(const char *sql);
	int32_t ExecSQL(const char *DBName, const char *sql);
	int64_t AffectedRows();
	int32_t UseResult();
	int32_t FetchRow();
	int32_t FreeResult();
	int32_t BeginTransaction(const char *DBName);
	int32_t RollBackTransaction(const char *DBName);
	int32_t CommitTransaction(const char *DBName);
	uint64_t InsertID();
	int32_t GetResNum();
	unsigned long *GetLengths();
	int32_t GetErrNo();
	const char *GetErrMsg();

private:
	int32_t Connect(const char *DBName);
	int32_t CloseDB();
	void SetError(const char *prefix);

public:
	MYSQL_ROW m_myrow;
	
private:
	int32_t m_connected;
	int32_t m_needfree;
	int32_t m_usematched;
	int32_t m_dberrno;
	int32_t m_resnum;
	int32_t m_transaction;
	char m_dberrmsg[512];
	DBHost m_dbhost;
	MYSQL m_mysql;
	MYSQL_RES *m_myres;
};


#endif
