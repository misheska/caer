#ifndef DV_SDK_MODULE_HPP
#define DV_SDK_MODULE_HPP

#include "module_base.hpp"

#include <boost/tti/has_static_member_function.hpp>

/**
 * Macro that expands into the global `dvModuleGetInfo` function, exposed to the API for DV.
 * The function instantiates the `ModuleStaticDefinition` class with the given Module (A subclass)
 * of `dv::ModuleBase` and returns the static info section.
 * @param MODULE
 */
#define registerModuleClass(MODULE)                \
	dvModuleInfo dvModuleGetInfo() {               \
		return &(dv::ModuleStatics<MODULE>::info); \
	}

namespace dv {

/**
 * Trait for the existence of a static getDescription method with const char* return value
 * @tparam T The class to be tested
 */
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(getDescription)
template<typename T>
inline constexpr bool has_getDescription = has_static_member_function_getDescription<T, const char *>::value;

/**
 * Trait for the existence of a static getConfigOptions method with map argument
 * @tparam T The class to be tested
 */
BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(getConfigOptions)
template<typename T>
inline constexpr bool has_getConfigOptions
	= has_static_member_function_getConfigOptions<T, void(RuntimeConfig &)>::value;

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(advancedStaticInit)
template<typename T>
inline constexpr bool has_advancedStaticInit
	= has_static_member_function_advancedStaticInit<T, void(dv::Config::Node)>::value;

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(addTypes)
template<typename T>
inline constexpr bool has_addTypes
	= has_static_member_function_addTypes<T, void(std::vector<dv::Types::Type> &)>::value;

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(addInputs)
template<typename T>
inline constexpr bool has_addInputs
	= has_static_member_function_addInputs<T, void(std::vector<dv::InputDefinition> &)>::value;

BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION(addOutputs)
template<typename T>
inline constexpr bool has_addOutputs
	= has_static_member_function_addOutputs<T, void(std::vector<dv::OutputDefinition> &)>::value;

/**
 * Pure static template class that provides the static C interface functions
 * for the module class `T` to be exposed to DV. It essentially wraps the
 * the functions of the given C++ module class to the stateless
 * functions and external state of the DV interface. Template parameter T must be
 * a valid DV module, e.g. it has to extend `dv::ModuleBase` and provide
 * certain static functions. Upon (static) instantiation, the `ModuleStatics`
 * class performs static (compile time) validations to check if `T` is a valid
 * module.
 * @tparam T A valid DV Module class that extends `dv::ModuleBase`
 */
template<class T> class ModuleStatics {
	/* Static assertions. Checks the existence of all required static members. */
	static_assert(std::is_base_of_v<ModuleBase, T>, "Your module does not inherit from dv::ModuleBase.");
	static_assert(has_getDescription<T>,
		"Your module does not specify a `static const char* getDescription()` function."
		"This function should return a string with a description of the module.");
	static_assert(has_getConfigOptions<T>,
		"Your module does not specify a `static void getConfigOptions(dv::RuntimeConfig &config)` "
		"function. This function should insert desired config options into the map.");

public:
	/**
	 * Wrapper for the `staticInit` DV function. Performs a static call to the
	 * `configInit<T>` function of `ModuleBase`, which in turn gets the config from
	 * the user defined module `T`. The config then gets parsed and injected as DvConfig
	 * nodes.
	 * @param moduleData The DV provided moduleData.
	 */
	static void staticInit(dvModuleData moduleData) {
		if constexpr (has_addTypes<T>) {
			std::vector<dv::Types::Type> types;

			T::addTypes(types);

			for (const auto &t : types) {
				dvModuleRegisterType(moduleData, t);
			}
		}

		if constexpr (has_addInputs<T>) {
			std::vector<dv::InputDefinition> inputs;

			T::addInputs(inputs);

			for (const auto &in : inputs) {
				dvModuleRegisterInput(moduleData, in.name.c_str(), in.typeName.c_str(), in.optional);
			}
		}

		if constexpr (has_addOutputs<T>) {
			std::vector<dv::OutputDefinition> outputs;

			T::addOutputs(outputs);

			for (const auto &out : outputs) {
				dvModuleRegisterOutput(moduleData, out.name.c_str(), out.typeName.c_str());
			}
		}

		ModuleBase::__setStaticGetDefaultConfig(std::function<void(RuntimeConfig &)>(T::getConfigOptions));

		ModuleBase::staticConfigInit(moduleData->moduleNode);

		if constexpr (has_advancedStaticInit<T>) {
			T::advancedStaticInit(moduleData->moduleNode);
		}
	}

	/**
	 * Wrapper for the `init` DV function. Constructs the user defined `T` module
	 * into the module state. Configuration is updated by the ModuleBase constructor.
	 * @param moduleData The DV provided moduleData.
	 * @return true if construction succeeded, false if it failed.
	 */
	static bool init(dvModuleData moduleData) {
		try {
			// Set the static fields prior to construction.
			ModuleBase::__setStaticGetDefaultConfig(std::function<void(RuntimeConfig &)>(T::getConfigOptions));
			ModuleBase::__setStaticModuleData(moduleData);

			// Construct T, will call ModuleBase() and then T() constructors.
			new (moduleData->moduleState) T();
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::ERROR, "Could not initialize module: %s", ex.what());
			return (false);
		}

		return (true);
	}

	/**
	 * Wrapper for the stateless `run` DV function. Relays the call to the user
	 * defined `run` function of the user defined `T` module, which exists as the
	 * module state.
	 * @param moduleData The DV provided moduleData. Used to extract the state.
	 * @param in The input data to be processed by the module.
	 * @param out Pointer to the output data.
	 */
	static void run(dvModuleData moduleData) {
		static_cast<T *>(moduleData->moduleState)->run();
	}

	/**
	 * Deconstructs the user defined `T` module in the state by calling
	 * its destructor.
	 * @param moduleData The DV provided moduleData.
	 */
	static void exit(dvModuleData moduleData) {
		static_cast<T *>(moduleData->moduleState)->~T();
	}

	/**
	 * Wrapper for the DV config function. Relays the call to the stateful
	 * `configUpdate` function of the `T` module. If not overloaded by a the user,
	 * the `configUpdate` function of `ModuleBase` is called which reads out all
	 * config from the DvConfig node and updates a runtime dict of configs.
	 * @param moduleData The moduleData provided by DV.
	 */
	static void config(dvModuleData moduleData) {
		static_cast<T *>(moduleData->moduleState)->configMerge();
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
 * @tparam T The user defined module. Must inherit from `dv::ModuleBase`
 */
template<class T>
const dvModuleFunctionsS ModuleStatics<T>::functions = {&ModuleStatics<T>::staticInit, &ModuleStatics<T>::init,
	&ModuleStatics<T>::run, &ModuleStatics<T>::config, &ModuleStatics<T>::exit};

/**
 * Static definition of the info struct, which gets passed to DV.
 * DV reads this and uses the information to call into the module.
 * It gets instantiated with the template parameter T, which has to
 * be a valid module and inherit from `dv::ModuleBase`.
 * @tparam T The user defined module. Must inherit from `dv::ModuleBase`
 */
template<class T> const dvModuleInfoS ModuleStatics<T>::info = {1, T::getDescription(), sizeof(T), &functions};

} // namespace dv

#endif // DV_SDK_MODULE_HPP
