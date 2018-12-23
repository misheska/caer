#include "service.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void caerLinuxDaemonize(void);

void caerServiceInit(void (*runner)(void)) {
	sshsNode systemNode = sshsGetNode(sshsGetGlobal(), "/caer/");

	sshsNodeCreate(
		systemNode, "backgroundService", false, SSHS_FLAGS_READ_ONLY, "Start program as a background service.");

	bool backgroundService = sshsNodeGetBool(systemNode, "backgroundService");

	if (backgroundService) {
		caerLinuxDaemonize();
		(*runner)();
	}
	else {
		// Console application. Just run.
		(*runner)();
	}
}

static void caerLinuxDaemonize(void) {
	// Double fork to background, for more details take a look at:
	// http://stackoverflow.com/questions/3095566/linux-daemonize
	pid_t result = fork();

	// Handle errors first.
	if (result == -1) {
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed the first fork.");
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
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed the second fork.");
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
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed to change directory to '/'.");
		exit(EXIT_FAILURE);
	}

	// Redirect stdin to /dev/null, stdout and stderr to the log-file.
	close(STDIN_FILENO); // stdin
	if (open("/dev/null", O_RDONLY) != STDIN_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed to redirect stdin to log file.");
		exit(EXIT_FAILURE);
	}

	if (dup2(caerLogFileDescriptorsGetFirst(), STDOUT_FILENO) != STDOUT_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed to redirect stdout to log file.");
		exit(EXIT_FAILURE);
	}

	if (dup2(caerLogFileDescriptorsGetFirst(), STDERR_FILENO) != STDERR_FILENO) {
		caerLog(CAER_LOG_EMERGENCY, "Daemonize", "Failed to redirect stderr to log file.");
		exit(EXIT_FAILURE);
	}

	// Disable stderr logging for caerLog(), keep only the direct logging to file there.
	caerLogFileDescriptorsSet(caerLogFileDescriptorsGetFirst(), -1);

	// At this point everything should be ok and we can return!
}
