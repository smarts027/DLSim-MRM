/* Portions Copyright 2019-2021 Xuesong Zhou and Peiheng Li, Cafer Avci

 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

 // Peiheng, 02/03/21, remove them later after adopting better casting
#pragma warning(disable : 4305 4267 4018)
// stop warning: "conversion from 'int' to 'float', possible loss of data"
#pragma warning(disable: 4244)

#ifdef _WIN32
#include "pch.h"
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <functional>
#include <stack>
#include <list>
#include <vector>
#include <map>
#include <omp.h>
#include "config.h"
#include "utils.h"


using std::max;
using std::min;
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::ifstream;
using std::ofstream;
using std::istringstream;


struct CNodeForwardStar {
	CNodeForwardStar() : OutgoingLinkNoArray{ nullptr }, OutgoingNodeNoArray{ nullptr }, OutgoingLinkSize{ 0 }
	{
	}

	// Peiheng, 03/22/21, release memory
	~CNodeForwardStar()
	{
		delete[] OutgoingLinkNoArray;
		delete[] OutgoingNodeNoArray;
	}

	int* OutgoingLinkNoArray;
	int* OutgoingNodeNoArray;
	int OutgoingLinkSize;
};

class NetworkForSP  // mainly for shortest path calculation
{
public:
	NetworkForSP() : temp_path_node_vector_size{ MAX_LINK_SIZE_IN_A_PATH }, m_value_of_time{ 10 }, bBuildNetwork{ false }, m_memory_block_no{ 0 }, m_agent_type_no{ 0 }, m_tau{ 0 }, b_real_time_information{ false }, PCE_ratio{ 1 }, OCC_ratio{ 1 }, m_RT_dest_node{ -1 },
		m_RT_dest_zone{ -1 }, updating_time_in_min{ 0 }
	{
	}

	~NetworkForSP()
	{
		if (m_SENodeList)  //1
			delete[] m_SENodeList;

		if (m_node_status_array)  //2
			delete[] m_node_status_array;

		if (m_label_time_array)  //3
			delete[] m_label_time_array;

		if (m_node_predecessor)  //5
			delete[] m_node_predecessor;

		if (m_link_predecessor)  //6
			delete[] m_link_predecessor;

		if (m_node_label_cost)  //7
			delete[] m_node_label_cost;

		if (m_link_PCE_volume_array)  //8
			delete[] m_link_PCE_volume_array;

		if (m_link_person_volume_array)  //8
			delete[] m_link_person_volume_array;

		if (m_link_genalized_cost_array) //9
			delete[] m_link_genalized_cost_array;

		if (m_link_outgoing_connector_zone_seq_no_array) //10
			delete[] m_link_outgoing_connector_zone_seq_no_array;

		// Peiheng, 03/22/21, memory release on OutgoingLinkNoArray and OutgoingNodeNoArray
		// is taken care by the destructor of CNodeForwardStar
		if (NodeForwardStarArray)
			delete[] NodeForwardStarArray;
	}

	float updating_time_in_min;
	int temp_path_node_vector_size;
	float m_value_of_time;
	bool bBuildNetwork;
	int m_memory_block_no;

	//node seq vector for each ODK
	int temp_path_node_vector[MAX_LINK_SIZE_IN_A_PATH];
	//node seq vector for each ODK
	int temp_path_link_vector[MAX_LINK_SIZE_IN_A_PATH];

	bool m_bSingleSP_Flag;

	// assigned nodes for computing
	std::vector<int> m_origin_node_vector;

	// rt backward shortest path
	int m_RT_dest_node;
	int m_RT_dest_zone;

	std::vector<int> m_origin_zone_seq_no_vector;

	bool b_real_time_information;
	int m_tau; // assigned nodes for computing
	int m_agent_type_no; // assigned nodes for computing
	double PCE_ratio;
	double OCC_ratio;

	CNodeForwardStar* NodeForwardStarArray;
	CNodeForwardStar* NodeBackwardStarArray;

	int m_threadNo;  // internal thread number

	int m_ListFront; // used in coding SEL
	int m_ListTail;  // used in coding SEL
	int* m_SENodeList; // used in coding SEL

	// label cost for shortest path calcuating
	double* m_node_label_cost;
	// time-based cost
	double* m_label_time_array;
	// distance-based cost
	// predecessor for nodes
	int* m_node_predecessor;
	// update status
	int* m_node_status_array;
	// predecessor for this node points to the previous link that updates its label cost (as part of optimality condition) (for easy referencing)
	int* m_link_predecessor;

	double* m_link_PCE_volume_array;
	double* m_link_person_volume_array;

	double* m_link_genalized_cost_array;
	int* m_link_outgoing_connector_zone_seq_no_array;

	// major function 1:  allocate memory and initialize the data
	void AllocateMemory(int number_of_nodes, int number_of_links)
	{
		NodeForwardStarArray = new CNodeForwardStar[number_of_nodes];
		NodeBackwardStarArray = new CNodeForwardStar[number_of_nodes];

		m_SENodeList = new int[number_of_nodes];  //1

		m_LinkBasedSEList = new int[number_of_links];  //1;  // dimension: number of links

		m_node_status_array = new int[number_of_nodes];  //2
		m_label_time_array = new double[number_of_nodes];  //3
		m_node_predecessor = new int[number_of_nodes];  //5
		m_link_predecessor = new int[number_of_nodes];  //6
		m_node_label_cost = new double[number_of_nodes];  //7

		m_link_PCE_volume_array = new double[number_of_links];  //8
		m_link_person_volume_array = new double[number_of_links];  //8
		m_link_genalized_cost_array = new double[number_of_links];  //9
		m_link_outgoing_connector_zone_seq_no_array = new int[number_of_links]; //10
	}

	bool UpdateGeneralizedLinkCost(int agent_type_no, Assignment* p_assignment, int origin_zone, int iteration_k)
	{
		int link_cost_debug_flag = 1;
		bool negative_cost_flag = false;
		for (int i = 0; i < g_link_vector.size(); ++i)
		{
			CLink* p_link = &(g_link_vector[i]);

			double LR_price = p_link->VDF_period[m_tau].LR_price[agent_type_no];

			if (p_assignment->g_AgentTypeVector[agent_type_no].real_time_information >= 1
				&& p_assignment->zone_seq_no_2_info_mapping.find(origin_zone) != p_assignment->zone_seq_no_2_info_mapping.end()
				) // for agent type with information and the origin zone is information zone
			{
				LR_price = p_link->VDF_period[m_tau].LR_RT_price[agent_type_no];

				//if (LR_price < -0.01)
			  //      negative_cost_flag = true;

			  //  if (fabs(LR_price) > 0.001)
			  //  {
			  //      dtalog.output() << "link " << p_link->link_id.c_str() << " " << g_node_vector[g_link_vector[l].from_node_seq_no].node_id << "->"
					//g_node_vector[g_link_vector[l].to_node_seq_no].node_id << "," 
			  //          << "  has a travel time of " << LR_price << " for agent type "
			  //          << assignment.g_AgentTypeVector[agent_type_no].agent_type.c_str() << " at demand period =" << m_tau << endl;
			  //  }

			}

			m_link_genalized_cost_array[i] = p_link->travel_time_per_period[m_tau] + p_link->VDF_period[m_tau].penalty +
				LR_price +
				p_link->VDF_period[m_tau].toll[agent_type_no] / m_value_of_time * 60;

			if (p_link->link_id == "7742")
			{
				int idebug = 1;
			}
			if (p_link->travel_time_per_period[m_tau] < 0)
			{

				dtalog.output() << "link " << p_link->link_id.c_str() << " " << g_node_vector[p_link->from_node_seq_no].node_id << "->" <<
					g_node_vector[p_link->to_node_seq_no].node_id << ","
					<< "  has a travel time of " << m_link_genalized_cost_array[i] << " for agent type "
					<< assignment.g_AgentTypeVector[agent_type_no].agent_type.c_str() << " at demand period =" << m_tau <<
					",p_link->travel_time_per_period[m_tau]=" << p_link->travel_time_per_period[m_tau] <<
					",p_link->VDF_period[m_tau].penalty=" << p_link->VDF_period[m_tau].penalty <<
					",LR_price=" << LR_price <<
					", p_link->VDF_period[m_tau].toll[agent_type_no] / m_value_of_time * 60=" << p_link->VDF_period[m_tau].toll[agent_type_no] / m_value_of_time * 60 <<
					endl;
			}

			//route_choice_cost 's unit is min
		}

		return negative_cost_flag;
	}

	void BuildNetwork(Assignment* p_assignment)  // build multimodal network
	{
		if (bBuildNetwork)
			return;

		int m_outgoing_link_seq_no_vector[MAX_LINK_SIZE_FOR_A_NODE];
		int m_to_node_seq_no_vector[MAX_LINK_SIZE_FOR_A_NODE];

		for (int i = 0; i < g_link_vector.size(); ++i)
		{
			CLink* p_link = &(g_link_vector[i]);
			m_link_outgoing_connector_zone_seq_no_array[i] = p_link->zone_seq_no_for_outgoing_connector;
		}

		for (int i = 0; i < p_assignment->g_number_of_nodes; ++i) // include all nodes (with physical nodes and zone centriods)
		{
			int outgoing_link_size = 0;

			for (int j = 0; j < g_node_vector[i].m_outgoing_link_seq_no_vector.size(); ++j)
			{

				int link_seq_no = g_node_vector[i].m_outgoing_link_seq_no_vector[j];
				// only predefined allowed agent type can be considered
				if (g_link_vector[link_seq_no].AllowAgentType(p_assignment->g_AgentTypeVector[m_agent_type_no].agent_type, m_tau))
				{
					m_outgoing_link_seq_no_vector[outgoing_link_size] = link_seq_no;
					m_to_node_seq_no_vector[outgoing_link_size] = g_node_vector[i].m_to_node_seq_no_vector[j];

					outgoing_link_size++;

					if (outgoing_link_size >= MAX_LINK_SIZE_FOR_A_NODE)
					{
						dtalog.output() << " Error: outgoing_link_size >= MAX_LINK_SIZE_FOR_A_NODE" << endl;
						// output the log

						g_OutputModelFiles(1);

						g_program_stop();
					}
				}
			}

			int node_seq_no = g_node_vector[i].node_seq_no;
			NodeForwardStarArray[node_seq_no].OutgoingLinkSize = outgoing_link_size;

			if (outgoing_link_size >= 1)
			{
				NodeForwardStarArray[node_seq_no].OutgoingLinkNoArray = new int[outgoing_link_size];
				NodeForwardStarArray[node_seq_no].OutgoingNodeNoArray = new int[outgoing_link_size];
			}

			for (int j = 0; j < outgoing_link_size; ++j)
			{
				NodeForwardStarArray[node_seq_no].OutgoingLinkNoArray[j] = m_outgoing_link_seq_no_vector[j];
				NodeForwardStarArray[node_seq_no].OutgoingNodeNoArray[j] = m_to_node_seq_no_vector[j];
			}
		}

		// after dynamic arrays are created for forward star
		if (dtalog.debug_level() == 2)
		{
			dtalog.output() << "add outgoing link data into dynamic array" << endl;

			for (int i = 0; i < g_node_vector.size(); ++i)
			{
				if (g_node_vector[i].zone_org_id > 0) // for each physical node
				{ // we need to make sure we only create two way connectors between nodes and zones
					dtalog.output() << "node id= " << g_node_vector[i].node_id << " with zone id " << g_node_vector[i].zone_org_id << "and "
						<< NodeForwardStarArray[i].OutgoingLinkSize << " outgoing links." << endl;

					for (int j = 0; j < NodeForwardStarArray[i].OutgoingLinkSize; j++)
					{
						int link_seq_no = NodeForwardStarArray[i].OutgoingLinkNoArray[j];
						dtalog.output() << "  outgoing node = " << g_node_vector[g_link_vector[link_seq_no].to_node_seq_no].node_id << endl;
					}
				}
				else
				{
					if (dtalog.debug_level() == 3)
					{
						dtalog.output() << "node id= " << g_node_vector[i].node_id << " with "
							<< NodeForwardStarArray[i].OutgoingLinkSize << " outgoing links." << endl;

						for (int j = 0; j < NodeForwardStarArray[i].OutgoingLinkSize; ++j)
						{
							int link_seq_no = NodeForwardStarArray[i].OutgoingLinkNoArray[j];
							dtalog.output() << "  outgoing node = " << g_node_vector[g_link_vector[link_seq_no].to_node_seq_no].node_id << endl;
						}
					}
				}
			}
		}

		m_value_of_time = p_assignment->g_AgentTypeVector[m_agent_type_no].value_of_time;
		bBuildNetwork = true;
	}

	// SEList: scan eligible List implementation: the reason for not using STL-like template is to avoid overhead associated pointer allocation/deallocation
	inline void SEList_clear()
	{
		m_ListFront = -1;
		m_ListTail = -1;
	}

	inline void SEList_push_front(int node)
	{
		if (m_ListFront == -1)  // start from empty
		{
			m_SENodeList[node] = -1;
			m_ListFront = node;
			m_ListTail = node;
		}
		else
		{
			m_SENodeList[node] = m_ListFront;
			m_ListFront = node;
		}
	}

	inline void SEList_push_back(int node)
	{
		if (m_ListFront == -1)  // start from empty
		{
			m_ListFront = node;
			m_ListTail = node;
			m_SENodeList[node] = -1;
		}
		else
		{
			m_SENodeList[m_ListTail] = node;
			m_SENodeList[node] = -1;
			m_ListTail = node;
		}
	}

	inline bool SEList_empty()
	{
		return(m_ListFront == -1);
	}

	//	inline int SEList_front()
	//	{
	//		return m_ListFront;
	//	}

	//	inline void SEList_pop_front()
	//	{
	//		int tempFront = m_ListFront;
	//		m_ListFront = m_SENodeList[m_ListFront];
	//		m_SENodeList[tempFront] = -1;
	//	}

	//major function: update the cost for each node at each SP tree, using a stack from the origin structure

	//double backtrace_shortest_path_tree(Assignment& assignment, int iteration_number, int o_node_index);
	//major function: update the cost for each node at each SP tree, using a stack from the origin structure
	double backtrace_shortest_path_tree(Assignment& assignment, int iteration_number_outterloop, int o_node_index)
	{
		double total_origin_least_cost = 0;
		int origin_node = m_origin_node_vector[o_node_index]; // assigned no
		int m_origin_zone_seq_no = m_origin_zone_seq_no_vector[o_node_index]; // assigned no

		//if (assignment.g_number_of_nodes >= 100000 && m_origin_zone_seq_no % 100 == 0)
		//{
		//	g_fout << "backtracing for zone " << m_origin_zone_seq_no << endl;
		//}


		int departure_time = m_tau;
		int agent_type = m_agent_type_no;

		if (g_node_vector[origin_node].m_outgoing_link_seq_no_vector.size() == 0)
			return 0;

		// given,  m_node_label_cost[i]; is the gradient cost , for this tree's, from its origin to the destination node i'.

		//	fprintf(g_pFileDebugLog, "------------START: origin:  %d  ; Departure time: %d  ; demand type:  %d  --------------\n", origin_node + 1, departure_time, agent_type);
		float k_path_prob = 1;
		k_path_prob = float(1) / float(iteration_number_outterloop + 1);  //XZ: use default value as MSA, this is equivalent to 1/(n+1)
		// MSA to distribute the continuous flow
		// to do, this is for each nth tree.

		//change of path flow is a function of cost gap (the updated node cost for the path generated at the previous iteration -m_node_label_cost[i] at the current iteration)
		// current path flow - change of path flow,
		// new propability for flow staying at this path
		// for current shortest path, collect all the switched path from the other shortest paths for this ODK pair.
		// check demand flow balance constraints

		int num = 0;
		int number_of_nodes = assignment.g_number_of_nodes;
		int number_of_links = assignment.g_number_of_links;
		int l_node_size = 0;  // initialize local node size index
		int l_link_size = 0;
		int node_sum = 0;

		float path_travel_time = 0;
		float path_distance = 0;

		int current_node_seq_no = -1;  // destination node
		int current_link_seq_no = -1;
		int destination_zone_seq_no;
		double ODvolume, volume;
		CColumnVector* pColumnVector;

		int local_debugging_flag = 0;
		//if (iteration_number_outterloop == 0)
		//{
		//    if (g_node_vector[origin_node].zone_id == assignment.shortest_path_log_zone_id && number_of_nodes< 10000)
		//        local_debugging_flag = 1;
		//}

		for (int i = 0; i < number_of_nodes; ++i)
		{
			if (g_node_vector[i].zone_id >= 1)
			{
				if (i == origin_node) // no within zone demand
					continue;

				if (g_node_vector[origin_node].zone_id == assignment.shortest_path_log_zone_id && g_node_vector[i].zone_id == 3)
				{
					int idebug = 1;
				}

				//fprintf(g_pFileDebugLog, "--------origin  %d ; destination node: %d ; (zone: %d) -------\n", origin_node + 1, i+1, g_node_vector[i].zone_id);
				//fprintf(g_pFileDebugLog, "--------iteration number outterloop  %d ;  -------\n", iteration_number_outterloop);
				destination_zone_seq_no = assignment.g_zoneid_to_zone_seq_no_mapping[g_node_vector[i].zone_id];

				pColumnVector = &(assignment.g_column_pool[m_origin_zone_seq_no][destination_zone_seq_no][agent_type][m_tau]);

				if (pColumnVector->bfixed_route) // with routing policy, no need to run MSA for adding new columns
					continue;

				ODvolume = pColumnVector->od_volume;
				volume = ODvolume * k_path_prob;
				// this is contributed path volume from OD flow (O, D, k, per time period

				if (ODvolume > 0.000001)
				{
					l_node_size = 0;  // initialize local node size index
					l_link_size = 0;
					node_sum = 0;

					path_travel_time = 0;
					path_distance = 0;

					current_node_seq_no = i;  // destination node
					current_link_seq_no = -1;

					if (m_link_predecessor[current_node_seq_no] >= 0)
					{
						total_origin_least_cost += ODvolume * m_node_label_cost[current_node_seq_no];
					}
					// backtrace the sp tree from the destination to the root (at origin)
					while (current_node_seq_no >= 0 && current_node_seq_no < number_of_nodes)
					{
						temp_path_node_vector[l_node_size++] = current_node_seq_no;

						if (l_node_size >= temp_path_node_vector_size)
						{
							dtalog.output() << "Error: l_node_size >= temp_path_node_vector_size" << endl;
							g_program_stop();
						}

						// this is valid node
						if (current_node_seq_no >= 0 && current_node_seq_no < number_of_nodes)
						{
							current_link_seq_no = m_link_predecessor[current_node_seq_no];
							node_sum += current_node_seq_no + current_link_seq_no;

							// fetch m_link_predecessor to mark the link volume
							if (current_link_seq_no >= 0 && current_link_seq_no < number_of_links)
							{
								temp_path_link_vector[l_link_size++] = current_link_seq_no;
								//fprintf(g_pFileDebugLog, "--------origin  %d ; destination node: %d ; (zone: %d) -------\n", origin_node + 1, i+1, g_node_vector[i].zone_id);

								// pure link based computing mode
								if (assignment.assignment_mode == 0)
								{
									// this is critical for parallel computing as we can write the volume to data
									m_link_PCE_volume_array[current_link_seq_no] += volume * PCE_ratio;  // for this network object volume from OD demand table
									m_link_person_volume_array[current_link_seq_no] += volume * OCC_ratio;
									//cout << "node = " << g_node_vector[i].node_id 
									//    << "zone id= " << g_node_vector[i].zone_id << ","
									//    << "l_link_size= " << l_link_size << ","
									//    << "link " << g_node_vector[g_link_vector[current_link_seq_no].from_node_seq_no].node_id
									//    << "->" << g_node_vector[g_link_vector[current_link_seq_no].to_node_seq_no].node_id
									//    << ": add volume " << volume << endl;

									//if (m_link_PCE_volume_array[current_link_seq_no] > 7001)
									//{
									//    int idebug = 1;
									//}
								}

								//path_travel_time += g_link_vector[current_link_seq_no].travel_time_per_period[tau];
								//path_distance += g_link_vector[current_link_seq_no].link_distance_VDF;
							}
						}
						current_node_seq_no = m_node_predecessor[current_node_seq_no];  // update node seq no
					}
					//fprintf(g_pFileDebugLog, "\n");

					// we obtain the cost, time, distance from the last tree-k
					if (assignment.assignment_mode >= 1) // column based mode
					{
						if (node_sum < 100)
						{
							int i_debug = 1;
						}

						// we cannot find a path with the same node sum, so we need to add this path into the map,
						if (l_node_size >= 2)
						{

							if (pColumnVector->path_node_sequence_map.find(node_sum) == assignment.g_column_pool[m_origin_zone_seq_no][destination_zone_seq_no][agent_type][m_tau].path_node_sequence_map.end())
							{
								// add this unique path
								int path_count = pColumnVector->path_node_sequence_map.size();
								pColumnVector->path_node_sequence_map[node_sum].path_seq_no = path_count;
								pColumnVector->path_node_sequence_map[node_sum].path_volume = 0;
								//assignment.g_column_pool[m_origin_zone_seq_no][destination_zone_seq_no][agent_type][tau].time = m_label_time_array[i];
								//assignment.g_column_pool[m_origin_zone_seq_no][destination_zone_seq_no][agent_type][tau].path_node_sequence_map[node_sum].path_distance = m_label_distance_array[i];
								pColumnVector->path_node_sequence_map[node_sum].path_toll = m_node_label_cost[i];

								if (local_debugging_flag)
								{
									for (int li = 0; li < l_node_size; ++li)
									{
										assignment.sp_log_file << "backtrace from zone_id =" << g_node_vector[i].zone_id << ", index: " << li << " node_id = " << g_node_vector[temp_path_node_vector[li]].node_id << endl;
									}

								}

								pColumnVector->path_node_sequence_map[node_sum].AllocateVector(
									l_node_size,
									temp_path_node_vector,
									l_link_size,
									temp_path_link_vector, true);
							}

							pColumnVector->path_node_sequence_map[node_sum].path_volume += volume;
						}
					}
				}
			}
		}
		return total_origin_least_cost;
	}


	//major function 2: // time-dependent label correcting algorithm with double queue implementation
	float optimal_label_correcting(int processor_id, Assignment* p_assignment, int iteration_k, int o_node_index, int d_node_no = -1, bool pure_travel_time_cost = false)
	{
		int local_debugging_flag = 0;

		int SE_loop_count = 0;


		if (iteration_k == 0)
			BuildNetwork(p_assignment);  // based on agent type and link type

		int agent_type = m_agent_type_no; // assigned nodes for computing
		int origin_node = m_origin_node_vector[o_node_index]; // assigned nodes for computing
		int origin_zone = m_origin_zone_seq_no_vector[o_node_index]; // assigned nodes for computing

		bool negative_cost_flag = UpdateGeneralizedLinkCost(agent_type, p_assignment, origin_zone, iteration_k);

		//if (iteration_k == 1) 
		//{
		//    if (g_zone_vector[origin_zone].zone_id == p_assignment->shortest_path_log_zone_id)
		//        local_debugging_flag = 1;
		//}


		if (negative_cost_flag == true)
		{
			dtalog.output() << "Negative Cost: SP iteration k =  " << iteration_k << ": origin node: " << g_node_vector[origin_node].node_id << endl;
			local_debugging_flag = 0;
			negative_cost_label_correcting(processor_id, p_assignment, iteration_k, o_node_index, d_node_no);
			return true;
		}

		if (p_assignment->g_number_of_nodes >= 1000 && origin_zone % 97 == 0)
			dtalog.output() << "label correcting for zone " << origin_zone << " in processor " << processor_id << endl;

		if (dtalog.debug_level() >= 2)
			dtalog.output() << "SP iteration k =  " << iteration_k << ": origin node: " << g_node_vector[origin_node].node_id << endl;

		if (local_debugging_flag == 1)
		{
			p_assignment->sp_log_file << " beginning of SP iteration k =  " << iteration_k << ": origin node: " << g_node_vector[origin_node].node_id << endl;
		}

		int number_of_nodes = p_assignment->g_number_of_nodes;
		//Initialization for all non-origin nodes
		for (int i = 0; i < number_of_nodes; ++i)
		{
			// not scanned
			m_node_status_array[i] = 0;
			m_node_label_cost[i] = MAX_LABEL_COST;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_link_predecessor[i] = -9999;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_node_predecessor[i] = -9999;
			// comment out to speed up comuting
			////m_label_time_array[i] = 0;
			////m_label_distance_array[i] = 0;
		}

		// int internal_debug_flag = 0;
		if (NodeForwardStarArray[origin_node].OutgoingLinkSize == 0)
			return 0;

		//Initialization for origin node at the preferred departure time, at departure time, cost = 0, otherwise, the delay at origin node
		m_label_time_array[origin_node] = 0;
		m_node_label_cost[origin_node] = 0.0;
		//Mark:	m_label_distance_array[origin_node] = 0.0;

		// Peiheng, 02/05/21, duplicate initialization, remove them later
		// pointer to previous NODE INDEX from the current label at current node and time
		m_link_predecessor[origin_node] = -9999;
		// pointer to previous NODE INDEX from the current label at current node and time
		m_node_predecessor[origin_node] = -9999;

		SEList_clear();
		SEList_push_back(origin_node);

		int from_node = -1;
		int to_node = -1;
		int link_sqe_no = -1;
		double new_time = 0;
		double new_distance = 0;
		double new_to_node_cost = 0;
		int tempFront;
		while (!(m_ListFront == -1))   //SEList_empty()
		{
			// from_node = SEList_front();
			// SEList_pop_front();  // remove current node FromID from the SE list

			from_node = m_ListFront;//pop a node FromID for scanning
			tempFront = m_ListFront;
			m_ListFront = m_SENodeList[m_ListFront];
			m_SENodeList[tempFront] = -1;

			m_node_status_array[from_node] = 2;

			if (local_debugging_flag)
			{
				p_assignment->sp_log_file << "SP:scan SE node: " << g_node_vector[from_node].node_id << " with "
					<< NodeForwardStarArray[from_node].OutgoingLinkSize << " outgoing link(s). " << endl;
			}
			//scan all outbound nodes of the current node

			int pred_link_seq_no = m_link_predecessor[from_node];

			// for each link (i,j) belong A(i)
			for (int i = 0; i < NodeForwardStarArray[from_node].OutgoingLinkSize; ++i)
			{
				to_node = NodeForwardStarArray[from_node].OutgoingNodeNoArray[i];
				link_sqe_no = NodeForwardStarArray[from_node].OutgoingLinkNoArray[i];

				if (local_debugging_flag)
					p_assignment->sp_log_file << "SP:  checking outgoing node " << g_node_vector[to_node].node_id << endl;

				// if(map (pred_link_seq_no, link_sqe_no) is prohibitted )
				//     then continue; //skip this is not an exact solution algorithm for movement

				//if (g_node_vector[from_node].node_id == 13621 && g_node_vector[to_node].node_id == 14997)
				//{
				//	float cost = m_link_genalized_cost_array[link_sqe_no];
				//	int debug = 1;
				//	p_assignment->sp_log_file << "SP:  checking from node " << g_node_vector[from_node].node_id
				//		<< "  to node " << g_node_vector[to_node].node_id << " cost = " << new_to_node_cost <<
				//		" , m_node_label_cost[from_node] " << m_node_label_cost[from_node] << ",m_link_genalized_cost_array[link_sqe_no] = " << m_link_genalized_cost_array[link_sqe_no]
				//		<< endl;
				//}

				if (g_node_vector[from_node].prohibited_movement_size >= 1)
				{
					if (pred_link_seq_no >= 0)
					{
						string	movement_string;
						string ib_link_id = g_link_vector[pred_link_seq_no].link_id;
						string ob_link_id = g_link_vector[link_sqe_no].link_id;
						movement_string = ib_link_id + "->" + ob_link_id;

						if (g_node_vector[from_node].m_prohibited_movement_string_map.find(movement_string) != g_node_vector[from_node].m_prohibited_movement_string_map.end())
						{
							dtalog.output() << "prohibited movement " << movement_string << " will not be used " << endl;
							continue;
						}
					}
				}

				//remark: the more complicated implementation can be found in paper Shortest Path Algorithms In Transportation Models: Classical and Innovative Aspects
				//	A note on least time path computation considering delays and prohibitions for intersection movements

				if (m_link_outgoing_connector_zone_seq_no_array[link_sqe_no] >= 0)
				{
					if (m_link_outgoing_connector_zone_seq_no_array[link_sqe_no] != origin_zone)
					{
						// filter out for an outgoing connector with a centriod zone id different from the origin zone seq no
						continue;
					}
				}

				//very important: only origin zone can access the outbound connectors,
				//the other zones do not have access to the outbound connectors

				// Mark				new_time = m_label_time_array[from_node] + p_link->travel_time_per_period[tau];
				// Mark				new_distance = m_label_distance_array[from_node] + p_link->link_distance_VDF;
				//float additional_cost = 0;

				//if (g_link_vector[link_sqe_no].RT_travel_time > 1)  // used in real time routing only
				//{
				//    additional_cost = g_link_vector[link_sqe_no].RT_travel_time;

				//    //if (g_link_vector[link_sqe_no].RT_travel_time > 999)
				//    //    continue; //skip this link due to closure
				//}


				new_to_node_cost = m_node_label_cost[from_node] + m_link_genalized_cost_array[link_sqe_no];


				if (local_debugging_flag)
				{
					p_assignment->sp_log_file << "SP:  checking from node " << g_node_vector[from_node].node_id
						<< "  to node " << g_node_vector[to_node].node_id << " cost = " << new_to_node_cost <<
						" , m_node_label_cost[from_node] " << m_node_label_cost[from_node] << ",m_link_genalized_cost_array[link_sqe_no] = " << m_link_genalized_cost_array[link_sqe_no]
						<< endl;

				}

				if (new_to_node_cost < m_node_label_cost[to_node]) // we only compare cost at the downstream node ToID at the new arrival time t
				{
					if (local_debugging_flag)
					{
						p_assignment->sp_log_file << "SP:  updating node: " << g_node_vector[to_node].node_id << " current cost:" << m_node_label_cost[to_node]
							<< " new cost " << new_to_node_cost << endl;
					}

					// update cost label and node/time predecessor
					// m_label_time_array[to_node] = new_time;
					// m_label_distance_array[to_node] = new_distance;
					m_node_label_cost[to_node] = new_to_node_cost;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_node_predecessor[to_node] = from_node;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_link_predecessor[to_node] = link_sqe_no;

					if (local_debugging_flag)
					{
						p_assignment->sp_log_file << "SP: add node " << g_node_vector[to_node].node_id << " new cost:" << new_to_node_cost
							<< " into SE List " << g_node_vector[to_node].node_id << endl;
					}

					// deque updating rule for m_node_status_array
					if (m_node_status_array[to_node] == 0)
					{
						///// SEList_push_back(to_node);
						///// begin of inline block
						if (m_ListFront == -1)  // start from empty
						{
							m_ListFront = to_node;
							m_ListTail = to_node;
							m_SENodeList[to_node] = -1;
						}
						else
						{
							m_SENodeList[m_ListTail] = to_node;
							m_SENodeList[to_node] = -1;
							m_ListTail = to_node;
						}
						///// end of inline block

						m_node_status_array[to_node] = 1;
					}

					if (m_node_status_array[to_node] == 2)
					{
						/////SEList_push_front(to_node);
						///// begin of inline block
						if (m_ListFront == -1)  // start from empty
						{
							m_SENodeList[to_node] = -1;
							m_ListFront = to_node;
							m_ListTail = to_node;
						}
						else
						{
							m_SENodeList[to_node] = m_ListFront;
							m_ListFront = to_node;
						}
						///// end of inline block

						m_node_status_array[to_node] = 1;
					}
				}
			}
		}

		if (local_debugging_flag)
		{
			p_assignment->sp_log_file << "SPtree at iteration k = " << iteration_k << " origin node: "
				<< g_node_vector[origin_node].node_id << endl;

			//Initialization for all non-origin nodes
			for (int i = 0; i < p_assignment->g_number_of_nodes; ++i)
			{
				int node_pred_id = -1;
				int node_pred_no = m_node_predecessor[i];

				if (node_pred_no >= 0)
					node_pred_id = g_node_vector[node_pred_no].node_id;

				if (m_node_label_cost[i] < 9999)
				{
					p_assignment->sp_log_file << "SP node: " << g_node_vector[i].node_id << " label cost " << m_node_label_cost[i] << "time "
						<< m_label_time_array[i] << "node_pred_id " << node_pred_id << endl;
				}
			}
		}

		//agent_type = m_agent_type_no; // assigned nodes for computing
		//origin_node = m_origin_node_vector[o_node_index]; // assigned nodes for computing
		//origin_zone = m_origin_zone_seq_no_vector[o_node_index]; // assigned nodes for computing

		int sp_log = 1;
	  if (sp_log == 1 && iteration_k <= 1)  // only one processor
	  {
	       if(g_zone_vector[origin_zone].zone_id == p_assignment->shortest_path_log_zone_id && p_assignment->g_origin_demand_array.find(origin_zone) != p_assignment->g_origin_demand_array.end())
	       {

	       std::string s_iteration = std::to_string(iteration_k);
	       std::string s_agent_type = p_assignment->g_AgentTypeVector [agent_type].agent_type;
	       std::string s_origin_zone = std::to_string(g_zone_vector[origin_zone].zone_id);

	       for (int i = 0; i < p_assignment->g_number_of_nodes; ++i)
	       {
	           std::string s_node = std::to_string(g_node_vector[i].node_id);
	           std::string map_key;

	           map_key = s_iteration+ "," + s_agent_type + "," + s_origin_zone + "," + s_node;

	           g_node_vector[i].pred_per_iteration_map[map_key] = m_node_predecessor[i];
	           g_node_vector[i].label_cost_per_iteration_map[map_key] = m_node_label_cost[i];

	       }
	       }
	   }


		if (d_node_no >= 1)
			return m_node_label_cost[d_node_no];
		else
			return 0;  // one to all shortest pat
	}

	double forwardtrace_shortest_path_tree(Assignment* p_assignment, int origin_node, std::vector<int>& path_link_vector)
	{

		int local_debugging_flag = 0;
		int departure_time = m_tau;
		int agent_type = m_agent_type_no;

		int number_of_nodes = assignment.g_number_of_nodes;
		int number_of_links = assignment.g_number_of_links;

		if (g_node_vector[origin_node].m_outgoing_link_seq_no_vector.size() == 0)
			return 999999;

		int l_node_size = 0;  // initialize local node size index
		int l_link_size = 0;

		float path_travel_time = 0;
		float path_distance = 0;

		int current_node_seq_no = -1;  // destination node
		int current_link_seq_no = -1;

		l_node_size = 0;  // initialize local node size index
		l_link_size = 0;

		path_travel_time = 0;
		path_distance = 0;

		current_node_seq_no = origin_node;  // current node
		current_link_seq_no = m_link_predecessor[current_node_seq_no];;
		path_link_vector.push_back(current_link_seq_no);

		// backtrace the sp tree from the destination to the root (at origin)
		while (current_node_seq_no > 0 && current_node_seq_no < number_of_nodes)
		{
			temp_path_node_vector[l_node_size++] = current_node_seq_no;

			//p_assignment->simu_log_file << g_node_vector[current_node_seq_no].node_id << ",";

			// this is valid node
			if (current_node_seq_no >= 0 && current_node_seq_no < number_of_nodes)
			{
				current_node_seq_no = m_node_predecessor[current_node_seq_no];  // update node seq no

				if (current_node_seq_no >= 0 && current_node_seq_no < number_of_nodes)
				{
					current_link_seq_no = m_link_predecessor[current_node_seq_no];
					if (current_link_seq_no >= 0 && current_link_seq_no < number_of_links)
					{
						path_link_vector.push_back(current_link_seq_no);
						//fprintf(g_pFileDebugLog, "--------origin  %d ; destination node: %d ; (zone: %d) -------\n", origin_node + 1, i+1, g_node_vector[i].zone_id);
						if (current_link_seq_no == 45106 || current_link_seq_no == 45107)
						{
							int warnning = 1;
						}
					}
				}

			}

		}
		if(path_link_vector.size() <= 2)
		{
		p_assignment->simu_log_file << "forward tracing " << endl;
		}

		return  m_node_label_cost[origin_node];
	}
	float optimal_backward_label_correcting_from_destination(int processor_id, Assignment* p_assignment, float current_updating_in_min, int dest_zone, int destination_node, int impacted_link_no = -1, int recording_flag = 0)
	{
		if (current_updating_in_min > updating_time_in_min + p_assignment->g_info_updating_freq_in_min)
		{
			updating_time_in_min = current_updating_in_min;
		}
		else
		{
			return 0;  // no update
		}

		int local_debugging_flag = 0;

		int SE_loop_count = 0;

		int agent_type = m_agent_type_no; // assigned nodes for computing

  //      if (g_zone_vector[dest_zone].zone_id == p_assignment->shortest_path_log_zone_id)
	 //   local_debugging_flag = 1;


		if (dtalog.debug_level() >= 2)
			dtalog.output() << "Dest SP =  " << ": dest node: " << g_node_vector[destination_node].node_id << endl;

		if (local_debugging_flag == 0)
		{
			p_assignment->sp_log_file << " beginning of SP   " << ": dest node: " << g_node_vector[destination_node].node_id << endl;
		}

		int number_of_nodes = p_assignment->g_number_of_nodes;
		//Initialization for all non-origin nodes
		for (int i = 0; i < number_of_nodes; ++i)
		{
			// not scanned
			m_node_status_array[i] = 0;
			m_node_label_cost[i] = MAX_LABEL_COST;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_link_predecessor[i] = -9999;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_node_predecessor[i] = -9999;
		}

		//Initialization for origin node at the given current time, at departure time, cost = 0, otherwise, the delay at origin node
		m_label_time_array[destination_node] = 0;
		m_node_label_cost[destination_node] = 0.0;
		//Mark:	m_label_distance_array[origin_node] = 0.0;

		// Peiheng, 02/05/21, duplicate initialization, remove them later
		// pointer to previous NODE INDEX from the current label at current node and time
		m_link_predecessor[destination_node] = -9999;
		// pointer to previous NODE INDEX from the current label at current node and time
		m_node_predecessor[destination_node] = -9999;

		SEList_clear();
		SEList_push_back(destination_node);

		int from_node = -1;
		int to_node = -1;
		int link_seq_no = -1;
		double new_time = 0;

		double new_from_node_cost = 0;
		int tempFront;
		while (!(m_ListFront == -1))   //SEList_empty()
		{

			to_node = m_ListFront;//pop a node FromID for scanning
			tempFront = m_ListFront;
			m_ListFront = m_SENodeList[m_ListFront];
			m_SENodeList[tempFront] = -1;

			m_node_status_array[to_node] = 2;

			if (local_debugging_flag)
			{
				p_assignment->sp_log_file << "SP:scan SE node: " << g_node_vector[to_node].node_id << " with "
					<< g_node_vector[to_node].m_incoming_link_seq_no_vector.size() << " outgoing link(s). " << endl;
			}
			//scan all inbound nodes of the current node

			int pred_link_seq_no = m_link_predecessor[to_node];

			// for each link (i,j) belong A(j)
			for (int i = 0; i < g_node_vector[to_node].m_incoming_link_seq_no_vector.size(); ++i)
			{
				link_seq_no = g_node_vector[to_node].m_incoming_link_seq_no_vector[i];
				from_node = g_link_vector[link_seq_no].from_node_seq_no;

				if (local_debugging_flag)
					p_assignment->sp_log_file << "SP:  checking incoming node " << g_node_vector[from_node].node_id << endl;

				// if(map (pred_link_seq_no, link_sqe_no) is prohibitted )
				//     then continue; //skip this is not an exact solution algorithm for movement

				//remark: the more complicated implementation can be found in paper Shortest Path Algorithms In Transportation Models: Classical and Innovative Aspects
				//	A note on least time path computation considering delays and prohibitions for intersection movements

				if (g_link_vector[link_seq_no].zone_seq_no_for_outgoing_connector >= 0)
				{
					// filter out for an outgoing connector with a centriod zone id, as real time physcial paths cannot use the connectors to walk/drive
					continue;
				}

				if (link_seq_no == impacted_link_no && current_updating_in_min < -99)
				{
					int idebug = 1;
				}

				if (g_link_vector[link_seq_no].VDF_period[this->m_tau].RT_allowed_use[this->m_agent_type_no] == false)
					continue;

				//very important: only origin zone can access the outbound connectors,
				//the other zones do not have access to the outbound connectors

				// Mark				new_time = m_label_time_array[from_node] + p_link->travel_time_per_period[tau];
				// Mark				new_distance = m_label_distance_array[from_node] + p_link->link_distance_VDF;
				//if (current_updating_in_min < -99)
				//{
				//    if (g_node_vector[from_node].node_id == 21497 &&
				//        g_node_vector[to_node].node_id == 21498)
				//    {
				//        dtalog.output() << "g_link_vector[link_seq_no].RT_waiting_time = " << g_link_vector[link_seq_no].RT_waiting_time << endl;
				//        dtalog.output() << "g_link_vector[link_seq_no].RT_waiting_time = " << g_link_vector[link_seq_no].RT_waiting_time << endl;

				//        if (g_link_vector[link_seq_no].RT_waiting_time >= 10 && g_node_vector[to_node].node_id == 21498)
				//        {
				//            local_debugging_flag = 1;
				//        }

				//    }
				//}

				if (link_seq_no == 45106 || link_seq_no == 45107)
				{
					int warnning = 1;
				}

				new_from_node_cost = m_node_label_cost[to_node] + g_link_vector[link_seq_no].VDF_period[m_tau].avg_travel_time + g_link_vector[link_seq_no].RT_waiting_time;


				if (local_debugging_flag)
				{
					////p_assignment->sp_log_file << "SP:  checking based on the to_node " << g_node_vector[to_node].node_id
					////    << "  backward to from_node " << g_node_vector[from_node].node_id << " cost = " << new_from_node_cost <<
					////    " , m_node_label_cost[from_node] " << m_node_label_cost[from_node] <<
					////     "avg travel time = " << g_link_vector[link_seq_no].VDF_period[m_tau].avg_travel_time <<
					////    ",RT_waiting_time = " << g_link_vector[link_seq_no].RT_waiting_time <<
					////    "allow_uses = " << g_link_vector[link_seq_no].VDF_period[this->m_tau].RT_allowed_use[this->m_tau]
					//    << endl;

				}

				if (new_from_node_cost < m_node_label_cost[from_node]) // we only compare cost at the downstream node ToID at the new arrival time t
				{
					if (local_debugging_flag)
					{
						p_assignment->sp_log_file << "SP:  updating node: " << g_node_vector[to_node].node_id << " current cost:" << m_node_label_cost[to_node]
							<< " new cost " << new_from_node_cost << endl;
					}

					m_node_label_cost[from_node] = new_from_node_cost;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_node_predecessor[from_node] = to_node;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_link_predecessor[from_node] = link_seq_no;

					if (local_debugging_flag)
					{
						p_assignment->sp_log_file << "SP: add node " << g_node_vector[from_node].node_id << " new cost:" << new_from_node_cost
							<< " into SE List " << g_node_vector[from_node].node_id << endl;
					}

					// deque updating rule for m_node_status_array
					if (m_node_status_array[from_node] == 0)
					{
						///// SEList_push_back(to_node);
						///// begin of inline block
						if (m_ListFront == -1)  // start from empty
						{
							m_ListFront = from_node;
							m_ListTail = from_node;
							m_SENodeList[from_node] = -1;
						}
						else
						{
							m_SENodeList[m_ListTail] = from_node;
							m_SENodeList[from_node] = -1;
							m_ListTail = from_node;
						}
						///// end of inline block

						m_node_status_array[from_node] = 1;
					}

					if (m_node_status_array[from_node] == 2)
					{
						if (m_ListFront == -1)  // start from empty
						{
							m_SENodeList[from_node] = -1;
							m_ListFront = from_node;
							m_ListTail = from_node;
						}
						else
						{
							m_SENodeList[from_node] = m_ListFront;
							m_ListFront = from_node;
						}
						///// end of inline block

						m_node_status_array[from_node] = 1;
					}
				}
			}
		}

		if (local_debugging_flag)
		{
			p_assignment->sp_log_file << "SPtree at dest node: "
				<< g_node_vector[destination_node].node_id << endl;

			//Initialization for all non-origin nodes
			for (int i = 0; i < p_assignment->g_number_of_nodes; ++i)
			{
				int node_pred_id = -1;
				int node_pred_no = m_node_predecessor[i];

				if (node_pred_no >= 0)
					node_pred_id = g_node_vector[node_pred_no].node_id;

				if (m_node_label_cost[i] < 9999)
				{
					p_assignment->sp_log_file << "SP node: " << g_node_vector[i].node_id << " label cost " << m_node_label_cost[i] << "node_pred_id " << node_pred_id << endl;
				}
			}
		}

		//agent_type = m_agent_type_no; // assigned nodes for computing
		//origin_node = m_origin_node_vector[o_node_index]; // assigned nodes for computing
		//origin_zone = m_origin_zone_seq_no_vector[o_node_index]; // assigned nodes for computing



		if (g_zone_vector[dest_zone].zone_id == p_assignment->shortest_path_log_zone_id && p_assignment->g_origin_demand_array.find(dest_zone) != p_assignment->g_origin_demand_array.end())
		{
			std::string s_demand_period = p_assignment->g_DemandPeriodVector[m_tau].demand_period.c_str();
			std::string s_agent_type = p_assignment->g_AgentTypeVector[agent_type].agent_type;
			std::string s_dest_zone = std::to_string(g_zone_vector[dest_zone].zone_id);

			for (int i = 0; i < p_assignment->g_number_of_nodes; ++i)
			{
				std::string s_node = std::to_string(g_node_vector[i].node_id);
				std::string map_key;

				map_key = s_demand_period + "," + s_agent_type + "," + s_dest_zone + "," + s_node;

				g_node_vector[i].pred_RT_map[map_key] = m_node_predecessor[i];
				g_node_vector[i].label_cost_RT_map[map_key] = m_node_label_cost[i];

			}
		}

		return 0;  // one to all shortest pat
	}

	float negative_cost_label_correcting(int processor_id, Assignment* p_assignment, int iteration_k, int o_node_index, int d_node_no = -1, bool pure_travel_time_cost = false)
	{
		int local_debugging_flag = 0;
		int SE_loop_count = 0;

		int agent_type = m_agent_type_no; // assigned nodes for computing
		int origin_node = m_origin_node_vector[o_node_index]; // assigned nodes for computing
		int origin_zone = m_origin_zone_seq_no_vector[o_node_index]; // assigned nodes for computing

		if (dtalog.debug_level() >= 2)
			dtalog.output() << "SP iteration k =  " << iteration_k << ": origin node: " << g_node_vector[origin_node].node_id << endl;

		int number_of_nodes = p_assignment->g_number_of_nodes;
		//Initialization for all non-origin nodes
		for (int i = 0; i < number_of_nodes; ++i)
		{
			// not scanned
			m_node_status_array[i] = 0;
			m_node_label_cost[i] = MAX_LABEL_COST;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_link_predecessor[i] = -9999;
			// pointer to previous NODE INDEX from the current label at current node and time
			m_node_predecessor[i] = -9999;
			// comment out to speed up comuting
			////m_label_time_array[i] = 0;
			////m_label_distance_array[i] = 0;
		}

		std::map<int, int> node_id_visit_mapping;   // used to prevent negative cost loop in the path

		// int internal_debug_flag = 0;
		if (NodeForwardStarArray[origin_node].OutgoingLinkSize == 0)
			return 0;

		//Initialization for origin node at the preferred departure time, at departure time, cost = 0, otherwise, the delay at origin node
		m_label_time_array[origin_node] = 0;
		m_node_label_cost[origin_node] = 0.0;
		//Mark:	m_label_distance_array[origin_node] = 0.0;

		// Peiheng, 02/05/21, duplicate initialization, remove them later
		// pointer to previous NODE INDEX from the current label at current node and time
		m_link_predecessor[origin_node] = -9999;
		// pointer to previous NODE INDEX from the current label at current node and time
		m_node_predecessor[origin_node] = -9999;

		SEList_clear();
		SEList_push_back(origin_node);

		std::map<int, int>::iterator it, it_begin, it_end;
		int from_node, to_node;
		int link_sqe_no;
		double new_time = 0;
		double new_distance = 0;
		double new_to_node_cost = 0;
		int tempFront;
		while (!(m_ListFront == -1))   //SEList_empty()
		{
			// from_node = SEList_front();
			// SEList_pop_front();  // remove current node FromID from the SE list

			from_node = m_ListFront;//pop a node FromID for scanning

			node_id_visit_mapping[from_node] = 1;  // map visit

			tempFront = m_ListFront;
			m_ListFront = m_SENodeList[m_ListFront];
			m_SENodeList[tempFront] = -1;

			m_node_status_array[from_node] = 2;

			if (dtalog.log_path() >= 2 || local_debugging_flag)
			{
				dtalog.output() << "SP:scan SE node: " << g_node_vector[from_node].node_id << " with "
					<< NodeForwardStarArray[from_node].OutgoingLinkSize << " outgoing link(s). " << endl;
			}
			//scan all outbound nodes of the current node

			int pred_link_seq_no = m_link_predecessor[from_node];

			// for each link (i,j) belong A(i)
			for (int i = 0; i < NodeForwardStarArray[from_node].OutgoingLinkSize; ++i)
			{
				to_node = NodeForwardStarArray[from_node].OutgoingNodeNoArray[i];
				link_sqe_no = NodeForwardStarArray[from_node].OutgoingLinkNoArray[i];

				if (dtalog.log_path() >= 2 || local_debugging_flag)
					dtalog.output() << "SP:  checking outgoing node " << g_node_vector[to_node].node_id << endl;

				// if(map (pred_link_seq_no, link_sqe_no) is prohibitted )
				//     then continue; //skip this is not an exact solution algorithm for movement

				if (g_node_vector[from_node].prohibited_movement_size >= 1)
				{
					if (pred_link_seq_no >= 0)
					{
						string	movement_string;
						string ib_link_id = g_link_vector[pred_link_seq_no].link_id;
						string ob_link_id = g_link_vector[link_sqe_no].link_id;
						movement_string = ib_link_id + "->" + ob_link_id;

						if (g_node_vector[from_node].m_prohibited_movement_string_map.find(movement_string) != g_node_vector[from_node].m_prohibited_movement_string_map.end())
						{
							dtalog.output() << "prohibited movement " << movement_string << " will not be used " << endl;
							continue;
						}
					}
				}

				//remark: the more complicated implementation can be found in paper Shortest Path Algorithms In Transportation Models: Classical and Innovative Aspects
				//	A note on least time path computation considering delays and prohibitions for intersection movements

				if (m_link_outgoing_connector_zone_seq_no_array[link_sqe_no] >= 0)
				{
					if (m_link_outgoing_connector_zone_seq_no_array[link_sqe_no] != origin_zone)
					{
						// filter out for an outgoing connector with a centriod zone id different from the origin zone seq no
						continue;
					}
				}

				//very important: only origin zone can access the outbound connectors,
				//the other zones do not have access to the outbound connectors

				// Mark				new_time = m_label_time_array[from_node] + p_link->travel_time_per_period[tau];
				// Mark				new_distance = m_label_distance_array[from_node] + p_link->link_distance_VDF;
				float additional_cost = 0;


				new_to_node_cost = m_node_label_cost[from_node] + m_link_genalized_cost_array[link_sqe_no] + additional_cost;

				if (dtalog.log_path() || local_debugging_flag)
				{
					dtalog.output() << "SP:  checking from node " << g_node_vector[from_node].node_id
						<< "  to node " << g_node_vector[to_node].node_id << " cost = " << new_to_node_cost << endl;
				}

				bool b_visit_flag = false;

				if (node_id_visit_mapping.find(to_node) != node_id_visit_mapping.end())
				{
					b_visit_flag = true;
				}


				if (b_visit_flag == false && new_to_node_cost < m_node_label_cost[to_node]) // we only compare cost at the downstream node ToID at the new arrival time t
				{

					if (dtalog.log_path() || local_debugging_flag)
					{
						dtalog.output() << "SP:  updating node: " << g_node_vector[to_node].node_id << " current cost:" << m_node_label_cost[to_node]
							<< " new cost " << new_to_node_cost << endl;
					}

					// update cost label and node/time predecessor
					// m_label_time_array[to_node] = new_time;
					// m_label_distance_array[to_node] = new_distance;
					m_node_label_cost[to_node] = new_to_node_cost;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_node_predecessor[to_node] = from_node;
					// pointer to previous physical NODE INDEX from the current label at current node and time
					m_link_predecessor[to_node] = link_sqe_no;

					if (dtalog.log_path() || local_debugging_flag)
					{
						dtalog.output() << "SP: add node " << g_node_vector[to_node].node_id << " new cost:" << new_to_node_cost
							<< " into SE List " << g_node_vector[to_node].node_id << endl;
					}

					// deque updating rule for m_node_status_array
					if (m_node_status_array[to_node] == 0)
					{
						///// SEList_push_back(to_node);
						///// begin of inline block
						if (m_ListFront == -1)  // start from empty
						{
							m_ListFront = to_node;
							m_ListTail = to_node;
							m_SENodeList[to_node] = -1;
						}
						else
						{
							m_SENodeList[m_ListTail] = to_node;
							m_SENodeList[to_node] = -1;
							m_ListTail = to_node;
						}
						///// end of inline block

						m_node_status_array[to_node] = 1;
					}

					if (m_node_status_array[to_node] == 2)
					{
						/////SEList_push_front(to_node);
						///// begin of inline block
						if (m_ListFront == -1)  // start from empty
						{
							m_SENodeList[to_node] = -1;
							m_ListFront = to_node;
							m_ListTail = to_node;
						}
						else
						{
							m_SENodeList[to_node] = m_ListFront;
							m_ListFront = to_node;
						}
						///// end of inline block

						m_node_status_array[to_node] = 1;
					}
				}
			}
		}

		if (dtalog.log_path() || local_debugging_flag)
		{
			dtalog.output() << "SPtree at iteration k = " << iteration_k << " origin node: "
				<< g_node_vector[origin_node].node_id << endl;

			//Initialization for all non-origin nodes
			for (int i = 0; i < p_assignment->g_number_of_nodes; ++i)
			{
				int node_pred_id = -1;
				int node_pred_no = m_node_predecessor[i];

				if (node_pred_no >= 0)
					node_pred_id = g_node_vector[node_pred_no].node_id;

				if (m_node_label_cost[i] < 9999)
				{
					dtalog.output() << "SP node: " << g_node_vector[i].node_id << " label cost " << m_node_label_cost[i] << "time "
						<< m_label_time_array[i] << "node_pred_id " << node_pred_id << endl;
				}
			}
		}

		if (d_node_no >= 1)
			return m_node_label_cost[d_node_no];
		else
			return 0;  // one to all shortest pat
	}

	////////// link based SE List

	int m_LinkBasedSEListFront;
	int m_LinkBasedSEListTail;
	int* m_LinkBasedSEList;  // dimension: number of links

	// SEList: Scan List implementation: the reason for not using STL-like template is to avoid overhead associated pointer allocation/deallocation
	void LinkBasedSEList_clear()
	{
		m_LinkBasedSEListFront = -1;
		m_LinkBasedSEListTail = -1;
	}

	void LinkBasedSEList_push_front(int link)
	{
		if (m_LinkBasedSEListFront == -1)  // start from empty
		{
			m_LinkBasedSEList[link] = -1;
			m_LinkBasedSEListFront = link;
			m_LinkBasedSEListTail = link;
		}
		else
		{
			m_LinkBasedSEList[link] = m_LinkBasedSEListFront;
			m_LinkBasedSEListFront = link;
		}
	}

	void LinkBasedSEList_push_back(int link)
	{
		if (m_LinkBasedSEListFront == -1)  // start from empty
		{
			m_LinkBasedSEListFront = link;
			m_LinkBasedSEListTail = link;
			m_LinkBasedSEList[link] = -1;
		}
		else
		{
			m_LinkBasedSEList[m_LinkBasedSEListTail] = link;
			m_LinkBasedSEList[link] = -1;
			m_LinkBasedSEListTail = link;
		}
	}

	bool LinkBasedSEList_empty()
	{
		return(m_LinkBasedSEListFront == -1);
	}

	int LinkBasedSEList_front()
	{
		return m_LinkBasedSEListFront;
	}

	void LinkBasedSEList_pop_front()
	{
		int tempFront = m_LinkBasedSEListFront;
		m_LinkBasedSEListFront = m_LinkBasedSEList[m_LinkBasedSEListFront];
		m_LinkBasedSEList[tempFront] = -1;
	}
};

