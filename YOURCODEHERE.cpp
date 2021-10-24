#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

#include "431project.h"

using namespace std;

/*
 * Enter your PSU IDs here to select the appropriate scanning order.
 */
#define PSU_ID_SUM (908591357+000000000)

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */
unsigned int currentlyExploringDim = 12;
bool currentDimDone = false;
bool isDSEComplete = false;

bool firstConfig = true;
bool dimsComplete[NUM_DIMS] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	string latencySettings;

	//
	//YOUR CODE BEGINS HERE
	//
	//8.3.5 & 8.3.6
	std::stringstream latency;

	int dl1size = getdl1size(halfBackedConfig);
	int il1size = getil1size(halfBackedConfig);
	int ul2size = getl2size(halfBackedConfig);

	int dl1assoc_index = extractConfigPararm(halfBackedConfig, 4);
	int il1assoc_index = extractConfigPararm(halfBackedConfig, 6);
	int ul2assoc_index = extractConfigPararm(halfBackedConfig, 9);

	int dl1_late = log2(dl1size/1024) + dl1assoc_index - 1;
	int il1_late = log2(il1size/1024) + il1assoc_index - 1;
	int ul2_late = log2(ul2size/1024) + ul2assoc_index - 5;

	latency << dl1_late << " " << il1_late << " " << ul2_late;

	//
	//YOUR CODE ENDS HERE
	//
	return latency.str();
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {

	// FIXME - YOUR CODE HERE
	// First four points in section 8.3
	
	int il1block_size = 8 << extractConfigPararm(configuration, 2);
	int il1size = getdl1size(configuration);
	int dl1size = getil1size(configuration);
	int ul2block_size = 16 << extractConfigPararm(configuration, 8);
	int ul2size = getl2size(configuration);
	int ifq = (1 << extractConfigPararm(configuration, 0)) * 8;

	if(il1block_size < ifq)
		return 0;
	else if(ul2block_size < 2 * il1block_size)
		return 0;
	else if(il1size < 2048 || il1size > 65536)
		return 0;
	else if(dl1size < 2048 || dl1size > 65536)
		return 0;
	else if(ul2size < 32768 || ul2size > 1048576)
		return 0;
	
	// The below is a necessary, but insufficient condition for validating a
	// configuration.
	return isNumDimConfiguration(configuration);
}

/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations

	std::string nextconfiguration = currentconfiguration;
	// Continue if proposed configuration is invalid or has been seen/checked before.
	while (!validateConfiguration(nextconfiguration) ||
		GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
			return currentconfiguration;
		}

		std::stringstream ss;

		string bestConfig;
		if (optimizeforEXEC == 1)
			bestConfig = bestEXECconfiguration;

		if (optimizeforEDP == 1)
			bestConfig = bestEDPconfiguration;

		// Fill in the dimensions already-scanned with the already-selected best
		// value.
		/*
		for (int dim = 0; dim < currentlyExploringDim; ++dim) {
			if(dimsComplete[dim])
				ss << extractConfigPararm(bestConfig, dim) << " ";
			else
				ss << extractConfigPararm(GLOB_baseline, dim) << " ";
		}
		*/

		// Handling for currently exploring dimension. This is a very dumb
		// implementation.

		// Change these two blocks
		int nextValue = extractConfigPararm(nextconfiguration,
				currentlyExploringDim) + 1;
		
		if(firstConfig){
			nextValue = 0;
			firstConfig = false;
		}

		if (nextValue >= GLOB_dimensioncardinality[currentlyExploringDim]) {
			nextValue = GLOB_dimensioncardinality[currentlyExploringDim] - 1;
			currentDimDone = true;
			dimsComplete[currentlyExploringDim] = true;
		}

		//ss << nextValue << " ";

		// Fill in remaining independent params with 0.
		// Change to put best fit
		for (int dim = 0; dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
			if(dimsComplete[dim])
				ss << extractConfigPararm(bestConfig, dim) << " ";
			else if(dim == currentlyExploringDim)
				ss << nextValue << " ";
			else
				ss << extractConfigPararm(GLOB_baseline, dim) << " ";
		}

		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();

		//Change next two parts for the order #5

		// Make sure we start exploring next dimension in next iteration.
		// 5. BP -> FPU -> CORE -> CACHE
		// 12 -> 13 -> 14 -> 11 -> 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10
		if (currentDimDone) {
			if(currentlyExploringDim == 10){
				isDSEComplete = true;
			}else if(currentlyExploringDim == 14){
				currentlyExploringDim = 11;
			}else if(currentlyExploringDim == 11){
				currentlyExploringDim = 0;
			}else{
				currentlyExploringDim++;
			}
			currentDimDone = false;

			firstConfig = true;
		}

		// Signal that DSE is complete after this configuration.
		//if (currentlyExploringDim == 10)
		//	isDSEComplete = true;
	}
	return nextconfiguration;
}

