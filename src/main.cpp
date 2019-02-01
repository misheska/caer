#include "config.h"
#include "config_server/config_server_main.h"
#include "log.h"
#include "mainloop.h"
#include "service.h"

static void mainRunner(void) {
	// Start the configuration server thread for run-time config changes.
	caerConfigServerStart();

	// Finally run the main event processing loop.
	caerMainloopRun();

	// After shutting down the mainloops, also shutdown the config server
	// thread if needed.
	caerConfigServerStop();
}

int main(int argc, char **argv) {
	// Initialize config storage from file, support command-line overrides.
	caerConfigInit(argc, argv);

	// Initialize logging sub-system.
	caerLogInit();

	// Start cAER. Can be as a background service or console application.
	caerServiceInit(&mainRunner);

	return (EXIT_SUCCESS);
}
