#include "dbprocess.h"

#include <errmsg.h>
#include <string.h>

#include "log.h"

#include "first_order_constant.h"


CDBProcess::CDBProcess()
{
	m_connected = 0;
	m_needfree = 0;
	m_usematched = 0;
	m_dberrno = 0;
	m_resnum = 0;
	m_transaction = 0;
	
	memset(m_dberrmsg, 0, sizeof(m_dberrmsg));
	memset(&m_dbhost, 0, sizeof(m_dbhost));

	m_myres = NULL;

	/*
	if(NULL == mysql_init(&m_mysql))
	{
		m_dberrno = mysql_errno(&m_mysql);
		snprintf(m_dberrmsg, sizeof(m_dberrmsg) - 1, "mysql init errno : %d, error : %s", m_dberrno, mysql_error(&m_mysql));
	}
	*/
}

CDBProcess::CDBProcess(const DBHost *dbhost)
{
	m_connected = 0;
	m_needfree = 0;
	m_usematched = 0;
	m_dberrno = 0;
	m_resnum = 0;
	m_transaction = 0;
	
	memset(m_dberrmsg, 0, sizeof(m_dberrmsg));
	m_dbhost = (*dbhost);
	
	m_myres = NULL;

	/*
	if(NULL == mysql_init(&m_mysql))
	{
		m_dberrno = mysql_errno(&m_mysql);
		snprintf(m_dberrmsg, sizeof(m_dberrmsg) - 1, "mysql init errno : %d, error : %s", m_dberrno, mysql_error(&m_mysql));
	}
	*/
}

CDBProcess::~CDBProcess()
{
	FreeResult();
	CloseDB();
}

void CDBProcess::SetDBHost(const DBHost *dbhost)
{
	CloseDB();
	m_connected = 0;
	m_dberrno = 0;
	memset(m_dberrmsg, 0, sizeof(m_dberrmsg));
	m_dbhost = (*dbhost);
}

void CDBProcess::UseMatchedRows()
{
	m_usematched = 1;
}

int32_t CDBProcess::Open()
{
	return Connect(NULL);
}

int32_t CDBProcess::Open(const char *DBName)
{
	int32_t ret = 0;
	ret = Connect(DBName);
	if(0 != ret && (CR_SERVER_GONE_ERROR == GetErrNo() || CR_SERVER_LOST == GetErrNo()))
	{
		//选择数据库失败后，主动进行一次重连
		ret = Connect(DBName);
	}
	
	return ret;
}

int32_t CDBProcess::Ping()
{
	int32_t ret = 0;
	ret = Open();
	if(0 != ret)
	{
		return ret;
	}

	ret = mysql_ping(&m_mysql);
	if(0 != ret)
	{
		SetError("mysql_ping");
		CloseDB();
	}

	return ret;
}

unsigned long CDBProcess::EscapeString(char *dst, const char *src, unsigned long len)
{
	if(NULL == dst || NULL == src)
	{
		return -1;
	}
	
	unsigned long escape_len = mysql_real_escape_string(&m_mysql, dst, src, len);
	if(((unsigned long)-1) == escape_len)
	{
		SetError("mysql_real_escape_string");
	}
	
	return escape_len;
}

const char *CDBProcess::GetDBName()
{
	return m_dbhost.dbname;
}

int32_t CDBProcess::SelectDataBase(const char *DBName)
{
	if(NULL == DBName || DBName[0] == '\0')
	{
		log_error("DBName is invalid.");
		return -1;
	}

	int32_t ret = mysql_select_db(&m_mysql, DBName);
	if(0 != ret)
	{
		SetError("mysql_select_db");
		CloseDB();
	}

	return ret;
}

int32_t CDBProcess::ExecSQL(const char *sql)
{
	if(NULL == sql)
	{
		log_error("sql is null.");
		return -1;
	}
	
	int32_t ret = 0;
	ret = Ping();
	if(0 != ret)
	{
		log_debug("ping failed [%d].", ret);
		ret = Open();
		if(0 != ret)
		{
			log_error("exec sql [%s] failed.", sql);
		}
		return ret;
	}

	ret = mysql_real_query(&m_mysql, sql, strlen(sql));
	if(0 != ret)
	{
		SetError("mysql_real_query");
		log_error("ExecSQL error : %s", m_dberrmsg);
		CloseDB();
	}

	return ret;
}

int32_t CDBProcess::ExecSQL(const char *DBName, const char *sql)
{
	int ret = 0;
	if(NULL == DBName || '\0' == DBName[0])
	{
		return ExecSQL(sql);
	}
	
	ret = Open(DBName);
	if(0 != ret)
	{
		log_error("exec dbname [%s], sql [%s] failed.", DBName, sql);
		return ret;
	}

	ret = mysql_real_query(&m_mysql, sql, strlen(sql));
	if(0 != ret)
	{
		SetError("mysql_real_query");
		log_error("ExecSQL error : %s", m_dberrmsg);
		CloseDB();
	}

	return ret;
}

int64_t CDBProcess::AffectedRows()
{
	int64_t rownum;
	rownum = mysql_affected_rows(&m_mysql);
	if(rownum < 0)
	{
		SetError("mysql_affected_rows");
		log_error("affected rows error : %s", m_dberrmsg);
		CloseDB();
	}

	return rownum;
}

int32_t CDBProcess::UseResult()
{
	m_myres = mysql_store_result(&m_mysql);
	if(NULL == m_myres)
	{
		if(0 != mysql_errno(&m_mysql))
		{
			SetError("mysql_store_result");
			log_error("store result error : %s", m_dberrmsg);
			CloseDB();
		}
		else
		{
			m_resnum = 0;
		}

		return -1;
	}

	m_resnum = mysql_num_rows(m_myres);
	if(m_resnum < 0)
	{
		SetError("mysql_num_rows");
		log_error("num rows error : %s", m_dberrmsg);
		CloseDB();
		return -1;
	}

	m_needfree = 1;
	return 0;
}

int32_t CDBProcess::FetchRow()
{
	m_myrow = mysql_fetch_row(m_myres);
	if(NULL == m_myrow)
	{
		SetError("mysql_fetch_row");
		log_error("fetch row error : %s", m_dberrmsg);
		CloseDB();
		return -1;
	}

	return 0;
}

int32_t CDBProcess::FreeResult()
{
	if(m_needfree)
	{
		mysql_free_result(m_myres);
		m_needfree = 0;
	}

	return 0;
}

int CDBProcess::BeginTransaction(const char *DBName)
{
	if(0 != m_transaction)
	{
		log_error("already has a transaction");
		return -1;
	}		
 	int status = ExecSQL(DBName, "BEGIN;");
	if(status < 0)
	{
		log_error("begin transaction error");
	}
	else
	{
		m_transaction = 1;  //a transaction start in mysql connection
	}
	
	return status;
}

int CDBProcess::RollBackTransaction(const char *DBName)
{
	if(0 == m_transaction)
	{
		log_error("no transaction");
		return -1;
	}	
	int status = ExecSQL(DBName, "ROLLBACK;");
	if(status < 0)
	{
		log_error("rollback transaction error");
	}
	m_transaction = 0;  //a transaction rollback in mysql connection
	
	return status;
}

int CDBProcess::CommitTransaction(const char *DBName)
{
	if(0 == m_transaction)
	{
		log_error("no transaction when commit");
		return -1;
	}
	int status = ExecSQL(DBName, "COMMIT;");
	if(status < 0)
	{
		log_error("commit transaction error");
	}
	m_transaction = 0;  //a transaction rollback in mysql connection
	
	return status;
}

uint64_t CDBProcess::InsertID()
{
	return static_cast<uint64_t>(mysql_insert_id(&m_mysql));
}

int32_t CDBProcess::GetResNum()
{
	return m_resnum;
}

unsigned long *CDBProcess::GetLengths()
{
	return mysql_fetch_lengths(m_myres);
}

int32_t CDBProcess::GetErrNo()
{
	return m_dberrno;
}

const char *CDBProcess::GetErrMsg()
{
	return m_dberrmsg;
}

int32_t CDBProcess::Connect(const char *DBName)
{
	if(!m_connected)
	{
		if(NULL == mysql_init(&m_mysql))
		{
			SetError("mysql_init");
			return -1;
		}
		
		if(0 != m_dbhost.connect_timeout)
		{
			mysql_options(&m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&(m_dbhost.connect_timeout));
		}

		int isunix = m_dbhost.host[0] == '/';
		if(NULL == mysql_real_connect(&m_mysql,
							  isunix ? NULL : m_dbhost.host,
							  m_dbhost.user,
							  m_dbhost.password,
							  NULL,
							  isunix ? 0 : m_dbhost.port,
							  isunix ? m_dbhost.host : NULL,
							  m_usematched ? CLIENT_FOUND_ROWS : 0
		   ))
		{

			SetError("mysql_real_connect");
			return -1;
		}

		//设置自动重连
		char reconnect = 1;
		mysql_options(&m_mysql, MYSQL_OPT_RECONNECT, (const char *)(&reconnect));
		
		if(m_dbhost.character[0] != '\0')
		{
			log_debug("mysql set [%s] character.", m_dbhost.character);
			if(0 != mysql_set_character_set(&m_mysql, m_dbhost.character))
			{
				SetError("mysql_set_character_set");
				CloseDB();
				return -1;
			}
		}
		
		m_connected = 1;
	}

	if(DBName != NULL && DBName[0] != '\0')
	{
		if(mysql_select_db(&m_mysql, DBName) != 0)
		{

			SetError("mysql_select_db");
			CloseDB();
			return -1;
		}

		//设置自动重连
		char reconnect = 1;
		mysql_options(&m_mysql, MYSQL_OPT_RECONNECT, (const char *)(&reconnect));
	}

	log_debug("connect mysql success.");

	return 0;
}

int32_t CDBProcess::CloseDB()
{
	if(m_connected)
	{
		mysql_close(&m_mysql);
		m_connected = 0;
	}

	return 0;
}

void CDBProcess::SetError(const char *prefix)
{
	m_dberrno = mysql_errno(&m_mysql);
	snprintf(m_dberrmsg, sizeof(m_dberrmsg) - 1, "mysql %s failed, errno: %d, errmsg: %s.", prefix, m_dberrno, mysql_error(&m_mysql));
}
