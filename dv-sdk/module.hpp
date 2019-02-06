#ifndef DV_SDK_MODULE_HPP
#define DV_SDK_MODULE_HPP

#include <dv-sdk/config/dvConfig.hpp>
#include <dv-sdk/mainloop.h>
#include <dv-sdk/module.h>

#include "BaseModule.hpp"
#include "log.hpp"
#include "utils.h"

#include <iostream>

/**
 * Macro that expands into the global `dvModuleGetInfo` function, exposed to the API for DV.
 * The function instantiates the `ModuleStaticDefinition` class with the given Module (A subclass)
 * of `dv::BaseModule` and returns the static info section.
 * @param MODULE
 */
#define registerModuleClass(MODULE)                \
	dvModuleInfo dvModuleGetInfo() {               \
		return &(dv::ModuleStatics<MODULE>::info); \
	}

namespace dv {

/**
 * Trait for the existence of a static getName method with const char* return value
 * @tparam T The class to be tested
 */
template<typename T, typename = void> struct has_getName : std::false_type {};
template<typename T>
struct has_getName<T,
	void_t<decltype(T::getName()), enable_if_t<std::is_same<decltype(T::getName()), const char *>::value>>>
	: std::true_type {};

/**
 * Trait for the existence of a static getName method with const char* return value
 * @tparam T The class to be tested
 */
template<typename T, typename = void> struct has_getDescription : std::false_type {};
template<typename T>
struct has_getDescription<T, void_t<decltype(T::getDescription()),
								 enable_if_t<std::is_same<decltype(T::getDescription()), const char *>::value>>>
	: std::true_type {};

/**
 * Trait for the existence of a static getConfigOptions method with map argument
 * @tparam T The class to be tested
 */
template<typename T, typename = void> struct has_getConfigOptions : std::false_type {};
template<typename T>
struct has_getConfigOptions<T,
	void_t<decltype(T::getConfigOptions(std::declval<std::map<std::string, dv::ConfigOption> &>()))>> : std::true_type {
};

/**
 * Trait for input stream definition. Tests if the supplied class has a static
 * member for the input stream definition.
 * @tparam T The class to be tested.
 */
template<typename T, typename = void> struct has_inputStreamDefinition : std::false_type {};
template<typename T>
struct has_inputStreamDefinition<T,
	void_t<decltype(T::inputStreams),
		enable_if_t<std::is_convertible<decltype(T::inputStreams), const caer_event_stream_in *>::value>>>
	: std::true_type {};

/**
 * Trait for output stream definition. Tests if the supplied class has a static
 * member for the output stream definition.
 * @tparam T The class to be tested.
 */
template<typename T, typename = void> struct has_outputStreamDefinition : std::false_type {};
template<typename T>
struct has_outputStreamDefinition<T,
	void_t<decltype(T::outputStreams),
		enable_if_t<std::is_convertible<decltype(T::outputStreams), const caer_event_stream_out *>::value>>>
	: std::true_type {};

/**
 * Const that compiles to the number of input streams the provided module T has
 * @tparam T The module T to be analyzed
 */
template<typename T>
const size_t numberOfInputStreams = T::inputStreams == nullptr ? 0 : CAER_EVENT_STREAM_IN_SIZE(T::inputStreams);

/**
 * Const that compiles to the number of output streams the provided module T has
 * @tparam T The module T to be analyzed
 */
template<typename T>
const size_t numberOfOutputStreams = T::outputStreams == nullptr ? 0 : CAER_EVENT_STREAM_OUT_SIZE(T::outputStreams);

/**
 * Const that compiles to the DV module type of the provided module T, based
 * on the number of input and output streams.
 * @tparam T The module T to be analyzed
 */
template<typename T>
const dvModuleType moduleType
	= (numberOfInputStreams<T> == 0) ? DV_MODULE_INPUT
									 : ((numberOfOutputStreams<T> == 0) ? DV_MODULE_OUTPUT : DV_MODULE_PROCESSOR);

/**
 * Pure static template class that provides the static C interface functions
 * for the module class `T` to be exposed to DV. It essentially wraps the
 * the functions of the given C++ module class to the stateless
 * functions and external state of the DV interface. Template parameter T must be
 * a valid DV module, e.g. it has to extend `dv::BaseModule` and provide
 * certain static functions. Upon (static) instantiation, the `ModuleStatics`
 * class performs static (compile time) validations to check if `T` is a valid
 * module.
 * @tparam T A valid DV Module class that extends `dv::BaseModule`
 */
template<class T> class ModuleStatics {
	/* Static assertions. Checks the existence of all required static members. */
	static_assert(std::is_base_of<BaseModule, T>::value, "Your module does not inherit from dv::BaseModule.");
	static_assert(has_getName<T>::value, "Your module does not specify a `static const char* getName()` function."
										 "This function should return a string with the name of the module.");
	static_assert(has_getDescription<T>::value,
		"Your module does not specify a `static const char* getDescription()` function."
		"This function should return a string with a description of the module.");
	static_assert(has_getConfigOptions<T>::value,
		"Your module does not specify a `static void getConfigOptions(std::map<std::string, dv::ConfigOption> "
		"&config)` function."
		"This function should insert desired config options into the map.");
	static_assert(has_inputStreamDefinition<T>::value,
		"Your module does not have a `static const caer_event_stream_in inputStreams[];` property."
		"This property should list the modules input streams.");
	static_assert(has_outputStreamDefinition<T>::value,
		"Your module does not have a `static const caer_event_stream_out outputStreams[];` property."
		"This property should list the modules output streams.");

public:
	/**
	 * Wrapper for the `configInit` DV function. Performs a static call to the
	 * `configInit<T>` function of `BaseModule`, which in turn gets the config from
	 * the user defined module `T`. The config then gets parsed and injected as DvConfig
	 * nodes.
	 * @param node The DV provided DvConfig node.
	 */
	static void configInit(dvConfigNode node) {
		BaseModule::configInit<T>(node);
	}

	/**
	 * Wrapper for the `init` DV function. Constructs the user defined `T` module
	 * into the module state, calls the config update function after construction
	 * and appends the DvConfig listener.
	 * @param moduleData The DV provided moduleData.
	 * @return true if construction succeeded, false if it failed.
	 */
	static bool init(dvModuleData moduleData) {
		try {
			// set the moduleData pointer thread local static prior to construction.
			BaseModule::__setStaticModuleData(moduleData);
			new (moduleData->moduleState) T();
			dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &dvModuleDefaultConfigListener);
		}
		catch (const std::exception &e) {
			dvModuleLog(moduleData, CAER_LOG_ERROR, "%s", e.what());
			dvModuleLog(moduleData, CAER_LOG_ERROR, "%s", "Could not initialize Module");
			return false;
		}
		return true;
	}

	/**
	 * Wrapper for the stateless `run` DV function. Relays the call to the user
	 * defined `run` function of the user defined `T` module, which exists as the
	 * module state.
	 * @param moduleData The DV provided moduleData. Used to extract the state.
	 * @param in The input data to be processed by the module.
	 * @param out Pointer to the output data.
	 */
	static void run(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
		((T *) moduleData->moduleState)->runBase(in, out);
	}

	/**
	 * Deconstructs the user defined `T` module in the state by calling
	 * its deconstructor. Removes the DvConfig attribute listener.
	 * @param moduleData The DV provided moduleData.
	 */
	static void exit(dvModuleData moduleData) {
		((T *) moduleData->moduleState)->~T();
		dvConfigNodeRemoveAllAttributeListeners(moduleData->moduleNode);
	}

	/**
	 * Wrapper for the DV config function. Relays the call to the stateful
	 * `configUpdate` function of the `T` module. If not overloaded by a the user,
	 * the `configUpdate` function of `BaseModule` is called which reads out all
	 * config from the DvConfig node and updates a runtime dict of configs.
	 * @param moduleData The moduleData provided by DV.
	 */
	static void config(dvModuleData moduleData) {
		((T *) moduleData->moduleState)->configUpdate(moduleData->moduleNode);
	}

	/**
	 * Static definition of the dvModuleFunctionsS struct. This struct
	 * gets filles with the static wrapper functions provided in this class
	 * at compile time.
	 */
	static const struct dvModuleFunctionsS functions;

	/**
	 * Static definition of the dvModuleInfoS struct. This struct
	 * gets filled with the static information from the user provided `T` module.
	 */
	static const struct dvModuleInfoS info;
};

/**
 * Static definition of the `ModuleStatics::functions` struct. This struct
 * contains the addresses to all the wrapper functions instantiated to the template
 * module `T`. This struct is then passed to DV to allow it to access the
 * functionalities inside the module.
 * @tparam T The user defined module. Must inherit from `dv::BaseModule`
 */
template<class T>
const dvModuleFunctionsS ModuleStatics<T>::functions = {&ModuleStatics<T>::configInit, &ModuleStatics<T>::init,
	&ModuleStatics<T>::run, &ModuleStatics<T>::config, &ModuleStatics<T>::exit, nullptr};

/**
 * Static definition of the info struct, which gets passed to DV.
 * DV reads this and uses the information to call into the module.
 * It gets instantiated with the template parameter T, which has to
 * be a valid module and inherit from `dv::BaseModule`.
 * @tparam T The user defined module. Must inherit from `dv::BaseModule`
 */
template<class T>
const dvModuleInfoS ModuleStatics<T>::info = {1, T::getName(), T::getDescription(), moduleType<T>, sizeof(T),
	&functions, numberOfInputStreams<T>, T::inputStreams, numberOfOutputStreams<T>, T::outputStreams};

} // namespace dv

#endif // DV_SDK_MODULE_HPP
