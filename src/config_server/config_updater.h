#ifndef SRC_CONFIG_SERVER_CONFIG_UPDATER_H_
#define SRC_CONFIG_SERVER_CONFIG_UPDATER_H_

#include "caer-sdk/config/dvConfig.hpp"
#include "caer-sdk/cross/portable_threads.h"

#include <atomic>
#include <chrono>
#include <thread>

class ConfigUpdater {
private:
	std::thread updateThread;
	std::atomic_bool runThread;
	dv::Config::Tree configTree;

public:
	ConfigUpdater() : configTree(dv::Config::GLOBAL) {
	}

	ConfigUpdater(dv::Config::Tree tree) : configTree(tree) {
	}

	void threadStart() {
		runThread.store(true);

		updateThread = std::thread([this]() {
			// Set thread name.
			portable_thread_set_name("ConfigUpdater");

			while (runThread.load()) {
				configTree.attributeUpdaterRun();

				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		});
	}

	void threadStop() {
		runThread.store(false);

		updateThread.join();
	}
};

#endif /* SRC_CONFIG_SERVER_CONFIG_UPDATER_H_ */
