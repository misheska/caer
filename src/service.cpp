#include "service.hpp"

#if !defined(OS_WINDOWS)
#	include <fcntl.h>
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <unistd.h>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void unixDaemonize(void);

static void unixDaemonize(void) {
	// Double fork to background, for more details take a look at:
	// http://stackoverflow.com/questions/3095566/linux-daemonize
	pid_t result = fork();

	// Handle errors first.
	if (result == -1) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed the first fork.");
		exit(EXIT_FAILURE);
	}

	// Then exit if you are the parent.
	if (result > 0) {
		// Exit without calling atexit() functions!
		_exit(EXIT_SUCCESS);
	}

	// So we must be the child here (result == 0).
	// Become session group leader.
	setsid();

	// Fork again, so that the child can never regain a controlling terminal.
	result = fork();

	// Handle errors first.
	if (result == -1) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed the second fork.");
		exit(EXIT_FAILURE);
	}

	// Then exit if you are the parent.
	if (result > 0) {
		// Exit without calling atexit() functions!
		_exit(EXIT_SUCCESS);
	}

	// So we must be the child here (result == 0).
	// Ensure we don't keep directories busy.
	if (chdir("/") != 0) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed to change directory to '/'.");
		exit(EXIT_FAILURE);
	}

	// Redirect stdin to /dev/null, stdout and stderr to the log-file.
	close(STDIN_FILENO); // stdin
	if (open("/dev/null", O_RDONLY) != STDIN_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed to redirect stdin to log file.");
		exit(EXIT_FAILURE);
	}

	if (dup2(caerLogFileDescriptorsGetFirst(), STDOUT_FILENO) != STDOUT_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed to redirect stdout to log file.");
		exit(EXIT_FAILURE);
	}

	if (dup2(caerLogFileDescriptorsGetFirst(), STDERR_FILENO) != STDERR_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Service", "Failed to redirect stderr to log file.");
		exit(EXIT_FAILURE);
	}

	// Disable stderr logging for caerLog(), keep only the direct logging to file there.
	caerLogFileDescriptorsSet(caerLogFileDescriptorsGetFirst(), -1);

	// At this point everything should be ok and we can return!
}
#endif

void dv::ServiceInit(void (*runner)(void)) {
	auto systemNode = dv::Config::GLOBAL.getNode("/system/");

	systemNode.create<dvCfgType::BOOL>(
		"backgroundService", false, {}, dvCfgFlags::READ_ONLY, "Start program as a background service.");

	bool backgroundService = systemNode.get<dvCfgType::BOOL>("backgroundService");

	if (backgroundService) {
#if defined(OS_WINDOWS)
		// TODO: implement Windows service.
		(*runner)();
#else
		// Unix: double fork to background.
		unixDaemonize();

		// Run main processing loop.
		(*runner)();
#endif
	}
	else {
		// Console application. Just run main processing loop.
		(*runner)();
	}
}
