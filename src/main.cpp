#include "config.h"
#include "config_server/config_server_main.h"
#include "log.hpp"
#include "mainloop.h"
#include "service.h"

static void mainRunner(void) {
	// Start the configuration server thread for run-time config changes.
	dvConfigServerStart();

	// Finally run the main event processing loop.
	dv::MainRun();

	// After shutting down the mainloops, also shutdown the config server
	// thread if needed.
	dvConfigServerStop();
}

int main(int argc, char **argv) {
	// Initialize config storage from file, support command-line overrides.
	dvConfigInit(argc, argv);

	// Initialize logging sub-system.
	dv::LoggerInit();

	// Start the DV runtime. Can be as a background service or console application.
	dvServiceInit(&mainRunner);

	return (EXIT_SUCCESS);
}
