// source - https://github.com/twanvl/graph-cluster
// -----------------------------------------------------------------------------
// Local search optimization clustering
// By Twan van Laarhoven
// License: BSD style, see file LICENSE
// -----------------------------------------------------------------------------

#ifndef HEADER_LSO_CLUSTER_ARGUMENT_PARSER
#define HEADER_LSO_CLUSTER_ARGUMENT_PARSER

#include "lso_cluster.hpp"
#include "loss_functions.hpp"

namespace lso_cluster {

// -----------------------------------------------------------------------------
// Argument sources, can be overloaded for different interfaces
// -----------------------------------------------------------------------------

struct ArgSource {
	virtual string         get_string_argument(vector<double>* more_out = 0) = 0;
	virtual double         get_double_argument() = 0;
	virtual int            get_int_argument() {
		return get_double_argument();
	}
	virtual bool           get_bool_argument() {
		return get_int_argument();
	}
	virtual vector<clus_t> get_1dvec_argument() = 0;
	virtual SparseMatrix   get_matrix_argument() = 0;
};
struct ParamSource : ArgSource {
	virtual bool end() = 0;
	virtual string get_parameter_name() {
		return get_string_argument();
	}
};

// -----------------------------------------------------------------------------
// Argument parsing, shared by all interfaces
// -----------------------------------------------------------------------------

void normalize_key(string& key) {
	for (string::iterator it = key.begin() ; it != key.end() ; ++it) {
		if (*it == ' ' || *it == '-') *it = '_';
	}
}

struct LsoMainFunction {
	// The configuration
	SparseMatrix graph;
	OptimizationParams params;
	shared_ptr<LossFunction> lossfun;
	bool optimize;
	vector<shared_ptr<TraceStep> > trace;
	string trace_file; // write trace to a file?
	int seed;
	// The output
	vector<clus_t> clustering;
	double loss;
	int num_clusters;
	
	// Defaults
	LsoMainFunction(OptimizationParams const& params)
		: params(params)
		, lossfun(new Modularity)
		, optimize(true)
		, seed(1234567)
		, loss(0)
	{}
	
	// Argument parsing
	void add_all_parameters(ParamSource& args) {
		// first parameter is the graph
		graph = args.get_matrix_argument();
		// then come the optional parameters
		while (!args.end()) {
			string key = args.get_parameter_name();
			normalize_key(key);
			add_parameter(key, args);
		}
	}
	
	virtual void add_parameter(string const& key, ArgSource& args) {
		if (key == "loss" || key == "lossfun" || key == "objective") {
			vector<double> more;
			string lossname = args.get_string_argument(&more);
			lossfun = loss_function_by_name(lossname, more.size(), &more[0]);
		} else if (key == "loss_extra" || key == "extra_loss" || key == "extra_loss_self") {
			double extra = args.get_double_argument();
			lossfun = shared_ptr<LossFunction>(new ExtraSelf(lossfun,extra));
		} else if (key == "extra_loss_num") {
			double extra = args.get_double_argument();
			lossfun = shared_ptr<LossFunction>(new ExtraNum(lossfun,extra));
		} else if (key == "extra_no_singleton") {
			double amount = args.get_double_argument();
			lossfun = shared_ptr<LossFunction>(new ExtraNoSingleton(lossfun,amount));
		} else if (key == "total_volume") {
			double vol = args.get_double_argument();
			lossfun = shared_ptr<LossFunction>(new WithTotalVolume(lossfun,vol));
			
		} else if (key == "init" || key == "initial") {
			clustering = args.get_1dvec_argument();
		} else if (key == "eval" || key == "evaluate") {
			clustering = args.get_1dvec_argument();
			optimize = false;
		
		} else if (key == "max_clusters" || key == "max_cluster" || key == "max_num_clusters" || key == "max_num_cluster") {
			params.max_num_clusters = args.get_int_argument();
		} else if (key == "min_clusters" || key == "min_cluster" || key == "min_num_clusters" || key == "min_num_cluster") {
			params.min_num_clusters = args.get_int_argument();
		} else if (key == "num_clusters" || key == "num_cluster") {
			params.max_num_clusters = params.min_num_clusters = args.get_int_argument();
		
		} else if (key == "verbose" || key == "verbosity") {
			params.verbosity = args.get_int_argument();
		} else if (key == "check_invariants" || key == "check" || key == "check_loss") {
			params.check_invariants = args.get_bool_argument();
		} else if (key == "num_repeats" || key == "num_repeat" || key == "repeats") {
			params.num_repeats = args.get_int_argument();
		} else if (key == "num_loss_tweak" || key == "num_loss_tweak_iterations" || key == "loss_tweak_iterations") {
			params.num_loss_tweak_iterations = args.get_int_argument();
		} else if (key == "num_partitions" || key == "num_partition" || key == "partition") {
			params.num_partitions = args.get_int_argument();
		} else if (key == "always_consider_empty" || key == "consider_empty") {
			params.always_consider_empty = args.get_bool_argument();
		} else if (key == "optimize_after_higher_level") {
			params.optimize_after_higher_level = args.get_bool_argument();
		} else if (key == "optimize_higher_level") {
			params.optimize_higher_level = args.get_bool_argument();
		} else if (key == "optimize_only_num_cluster" || key == "optimize_num_clusters_with_outer_loop") {
			params.optimize_num_clusters_with_outer_loop = args.get_bool_argument();
		} else if (key == "optimize_globally_best_moves" || key == "optimize_globally_best_move") {
			params.optimize_globally_best_moves = args.get_bool_argument();
		} else if (key == "seed") {
			seed = args.get_int_argument();
		
		} else if (key == "trace_file") {
			params.trace_out = &trace;
			trace_file = args.get_string_argument();
		
		} else {
			throw std::invalid_argument("Unrecognized key: " + key);
		}
	}
	
	// Running
	
	void run() {
		// Do some validation of the arguments
		if (graph.rows() != graph.cols()) {
			throw std::invalid_argument("Matrix must be square");
		}
		if (graph.any_element_is_inf_or_nan()) {
			throw std::invalid_argument("Matrix includes Inf or NaN values");
		}
		if (graph.any_element_is_negative()) {
			throw std::invalid_argument("Matrix includes negative values");
		}
		if (!is_symmetric(graph)) {
			throw std::invalid_argument("Matrix must be symmetric");
		}
		if (!clustering.empty() && (int)clustering.size() != graph.rows()) {
			throw std::invalid_argument("Initial value must have same size as the matrix");
		}
		
		// initialize Clustering object
		params.lossfun = lossfun.get();
		srand(seed);
		Clustering clus(graph,params);
		if (!clustering.empty()) {
			clus.set_clustering(clustering);
		}
		
		// perform clustering
		if (optimize) {
			clus.optimize();
		}
		
		// outputs
		clustering = clus.get_clustering();
		loss = clus.get_loss();
		num_clusters = clus.num_clusters();
	}
};

// -----------------------------------------------------------------------------
// Usage info (matlab / octave)
// -----------------------------------------------------------------------------

#define USAGE_INFO \
	"Find an optimal clustering for a given graph, using local search optimization.\n" \
	"\n" \
	"Usage: \n" \
	"   [clus,loss,numclus] = lso_cluster(A, [varargin])\n" \
	"\n" \
	"Inputs:\n" \
	"   A:        The adjacency matrix of a graph.\n" \
	"              All entries must be non-negative and finite.\n" \
	"              A must be symmetric, i.e. A==A', this is not checked.\n" \
	"   varargin: Extra options as 'key',value pairs\n" \
	"\n" \
	"Outputs:\n" \
	"   clus:     The found clustering. Clusters will be labeled 0,1,2,etc.\n" \
	"   loss:     Loss of the optimal clustering\n" \
	"   numclus:  The number of clusters found\n" \
	"\n" \
	"Extra options:\n" \
	"   'loss':             Loss function to use. (See below)\n" \
	"   'verbose':          Verbosity level, default 0\n" \
	"   'seed':             Random seed.\n" \
	"   'num_cluster':      Force the solution to have this many clusters.\n" \
	"   'min_num_cluster':  Force the solution to have at least this many clusters.\n" \
	"   'max_num_cluster':  Force the solution to have at most this many clusters.\n" \
	"   'check_invariants': Recheck invariants after every change.\n" \
	"   'num_repeats':      How often is the greedy search repeated from\n" \
	"                        scratch? default 10.\n" \
	"   'num_partitions':   How often is the solution re-partitioned and\n" \
	"                        optimized again? default 0.\n" \
	"   'eval':             Don't optimize, but evaluate loss on given clustering.\n" \
	"   'init':             Start from the given initial clustering.\n" \
	"\n" \
	"Loss functions:\n" \
	"   'modularity': minus modularity: -sum of (within(c)/total_deg) - (deg(c)/total_deg)^2 \n" \
	"   'ncut':       normalized cut: sum of exit(c)/size(c)\n" \
	"   'rcut':       ratio cut: sum of exit(c)/degree(c)\n" \
	"   'infomap':    loss used by infomap source code\n" \
	"   'w-log-v':    sum of  within(c)/total_deg * lof(deg(c)/total_deg)\n" \
	"   'parabola':   sum of  within(c)/total_deg * (deg(c)/total_deg - 1)\n" \
	"   ... many more experimental loss functions, see the source for details.\n" \
	"\n" \
	"Examples:\n" \
	"\n" \
	"   # Find clustering that optimizes w-log-v:\n" \
	"   clus = greedy_cluster(A,'loss','w-log-v');\n" \
	"   # Calculate modularity of that clustering:\n" \
	"   [_,loss] = greedy_cluster(A,'loss','modularity','eval',clus);\n"

// -----------------------------------------------------------------------------
}
#endif
