#include "service.hpp"

#include "main.hpp"

#if !defined(OS_WINDOWS)
#	include <fcntl.h>
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <unistd.h>

static void unixDaemonize();

static void unixDaemonize() {
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

#if defined(OS_WINDOWS)
#	include <windows.h>

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

void ServiceMain(int argc, char **argv);
void ControlHandler(DWORD request);

void ServiceMain(int argc, char **argv) {
	int error;

	ServiceStatus.dwServiceType             = SERVICE_WIN32;
	ServiceStatus.dwCurrentState            = SERVICE_START_PENDING;
	ServiceStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode           = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint              = 0;
	ServiceStatus.dwWaitHint                = 0;

	hStatus = RegisterServiceCtrlHandler("dv-runtime-service", (LPHANDLER_FUNCTION) ControlHandler);
	if (hStatus == (SERVICE_STATUS_HANDLE) 0) {
		// Registering Control Handler failed
		return;
	}

	// We report the running status to SCM.
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hStatus, &ServiceStatus);

	// The worker loop of a service
	(*runner)();
}

// Control handler function
void ControlHandler(DWORD request) {
	switch (request) {
		case SERVICE_CONTROL_STOP:
			ServiceStatus.dwWin32ExitCode = 0;
			ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
			SetServiceStatus(hStatus, &ServiceStatus);
			dv::MainData::getGlobal().systemRunning = false;
			return;

		case SERVICE_CONTROL_SHUTDOWN:
			ServiceStatus.dwWin32ExitCode = 0;
			ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
			SetServiceStatus(hStatus, &ServiceStatus);
			dv::MainData::getGlobal().systemRunning = false;
			return;

		default:
			// Report current status
			SetServiceStatus(hStatus, &ServiceStatus);
			return;
	}
}
#endif

void dv::ServiceInit(void (*runner)()) {
	auto systemNode = dv::Config::GLOBAL.getNode("/system/");

	systemNode.create<dv::CfgType::BOOL>(
		"backgroundService", false, {}, dv::CfgFlags::READ_ONLY, "Start program as a background service.");

	bool backgroundService = systemNode.get<dv::CfgType::BOOL>("backgroundService");

	if (backgroundService) {
#if defined(OS_WINDOWS)
		// Implement Windows service.
		SERVICE_TABLE_ENTRY ServiceTable[2];

		ServiceTable[0].lpServiceName = "dv-runtime-service";
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION) ServiceMain;

		ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;

		// Start the control dispatcher thread for our service
		StartServiceCtrlDispatcher(ServiceTable);
#else
		// Unix: double fork to background.
		unixDaemonize();

		// Run main code in new process.
		(*runner)();
#endif
	}
	else {
		// Console application. Just run main code directly.
		(*runner)();
	}
}
