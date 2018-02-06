#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif

#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include "log/log.h"

#define LOGSIZE 16384

int __log_level__ = 6;
static int logfd = -1;
static char appname[32] = "";
static char log_dir[128] = "./log";
static int logday = 0;
static int noconsole = 0;

static int gettid(void) { return syscall(__NR_gettid); }

// clean logfd when module unloaded
__attribute__((__destructor__))
static void clean_logfd(void) {
	if (logfd >= 0) {
		close(logfd);
		logfd = -1;
	}
}

void _init_log_(const char *app, const char *dir) {
	strncpy(appname, app, sizeof(appname) - 1);

	if (dir) {
		strncpy(log_dir, dir, sizeof(log_dir) - 1);
	}
	mkdir(log_dir, 0777);
	if (access(log_dir, W_OK | X_OK) < 0) {
		log_error("logdir(%s): Not writable", log_dir);
	}

	logfd = open("/dev/null", O_WRONLY);
	if (logfd < 0)
		logfd = dup(2);
	fcntl(logfd, F_SETFD, FD_CLOEXEC);
}

void _set_log_level_(int l) {
	if (l >= 0)
		__log_level__ = l > 4 ? l : 4;
}

void _write_log_(int level, const char *filename, const char *funcname,
	int lineno, const char *format, ...) {
	// save errno
	int savedErrNo = errno;
	int off = 0;
	char buf[LOGSIZE];
	char logfile[256];
	if (appname[0] == 0)
		return;
	if (level < 0)
		level = 0;
	else if (level > 7)
		level = 7;


	struct tm tm;
	time_t now = time(NULL);
	localtime_r(&now, &tm);

	if (filename == NULL) {
		off = snprintf(buf, LOGSIZE, "<%d>[%02d:%02d:%02d] pid[%d]: ", level,
			tm.tm_hour, tm.tm_min, tm.tm_sec, getpid());
	} else {
		filename = basename((char*)filename);
		off = snprintf(buf, LOGSIZE,
			"<%d>[%02d:%02d:%02d] pid[%d]: %s(%d)[%s]: ", level, tm.tm_hour,
			tm.tm_min, tm.tm_sec, gettid(), filename, lineno, funcname);
	}
	if (off >= LOGSIZE)
		off = LOGSIZE - 1;

	int today = tm.tm_year * 1000 + tm.tm_yday;
	if (logfd >= 0 && today != logday) {
		int fd;
		logday = today;
		snprintf(logfile, sizeof(logfile), "%s/%s.error%04d%02d%02d.log",
			log_dir, appname, tm.tm_year + 1900, tm.tm_mon + 1,
			tm.tm_mday);
		fd = open(logfile, O_CREAT | O_LARGEFILE | O_APPEND | O_WRONLY,
			0644);
		if (fd >= 0) {
			dup2(fd, logfd);
			close(fd);
			fcntl(logfd, F_SETFD, FD_CLOEXEC);
		}
	}

	// formatted message
	va_list ap;
	va_start(ap, format);
	// restore errno
	errno = savedErrNo;
	off += vsnprintf(buf + off, LOGSIZE - off, format, ap);
	va_end(ap);

	if (off >= LOGSIZE)
		off = LOGSIZE - 1;
	if (buf[off - 1] != '\n') {
		buf[off++] = '\n';
	}
	int unused;

	if (logfd >= 0) {
		unused = write(logfd, buf, off);
	}

	if (level <= 6 && !noconsole) {
		// debug don't send to console/stderr
		unused = fwrite(buf + 3, 1, off - 3, stderr);
		if (unused <= 0) {
			// disable console if write error
			noconsole = 1;
		}
	}
}

void _write_stderr_(const char *fmt, ...) {
	int len, size, errno_save;
	char buf[4 * 256];
	va_list args;

	errno_save = errno;
	len = 0; /* length of output buffer */
	size = 4 * 256; /* size of output buffer */

	va_start(args, fmt);
	len += vsnprintf(buf, size, fmt, args);
	va_end(args);

	buf[len++] = '\n';

	write(STDERR_FILENO, buf, len);
	errno = errno_save;
}
