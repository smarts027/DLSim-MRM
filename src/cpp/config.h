/* Portions Copyright 2021 Xuesong Zhou and Peiheng Li
 *
 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

#ifndef GUARD_CONFIG_H
#define GUARD_CONFIG_H

// if you are using cmake, please #include <build_config.h>
#ifndef _WIN32
#include <build_config.h>
#endif

#ifdef BUILD_EXE
    double network_assignment(int assignment_mode, int iteration_number, int column_updating_iterations, int ODME_iterations, int sensitivity_analysis_iterations, int simulation_iterations, int number_of_memory_blocks);
    void perform_network_assignment();
    void perform_cbi();
    void perform_cbsa();
    void generate_demand();
    void generate_zones();
    void generate_default_settings();
#else
    #ifdef _WIN32
        #define DTALIBRARY_API __declspec(dllexport)
    #else
        #define DTALIBRARY_API
    #endif

    extern "C" DTALIBRARY_API double network_assignment(int assignment_mode, int iteration_number, int column_updating_iterations, int ODME_iterations, int sensitivity_analysis_iterations, int simulation_iterations, int number_of_memory_blocks);
    extern "C" DTALIBRARY_API void perform_network_assignment();
    extern "C" DTALIBRARY_API void perform_cbi();
    extern "C" DTALIBRARY_API void perform_cbsa();
    extern "C" DTALIBRARY_API void generate_demand();
    extern "C" DTALIBRARY_API void generate_zones();
    extern "C" DTALIBRARY_API void generate_default_settings();
#endif

#endif