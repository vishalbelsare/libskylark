#ifndef SKYLARK_RLSC_PARAMS_HPP
#define SKYLARK_RLSC_PARAMS_HPP

#ifndef SKYLARK_RLSC_HPP
#error "Include top-level rlsc.hpp instead of including individuals headers"
#endif

namespace skylark { namespace ml {

struct rlsc_params_t : public base::params_t {

    // For all methods that use feature transforms
    bool use_fast;

    // For approximate methods (ApproximateRLSC)
    bool sketched_rls;
    El::Int sketch_size;
    bool fast_sketch;

    // For iterative methods (FasterRLSC, LargeScaleRLSC)
    int iter_lim;
    int res_print;
    double tolerance;
    double precond_lambda;  // -1 (default) will just input lambda
    algorithms::krylov_method_t krylov_method;

    // For memory limited methods (SketchedApproximateRLSC, LargeScaleRLSC)
    El::Int max_split;

    rlsc_params_t(bool am_i_printing = 0,
        int log_level = 0,
        std::ostream &log_stream = std::cout,
        std::string prefix = "",
        int debug_level = 0) :
        base::params_t(am_i_printing, log_level, log_stream, prefix, debug_level) {

        use_fast = false;

        sketched_rls = false;
        sketch_size = -1;
        fast_sketch = false;

        tolerance = 1e-3;
        res_print = 10;
        iter_lim = 1000;
        precond_lambda = -1;
        krylov_method = algorithms::CG_TAG;

        max_split = 0;
    }

    rlsc_params_t(const boost::property_tree::ptree& json)
        : params_t(json) {
        use_fast = json.get<bool>("use_fast", false);
        sketched_rls = json.get<bool>("sketched_rr", false);
        sketch_size = json.get<int>("sketch_size", -1);
        fast_sketch = json.get<bool>("fast_sketch", false);
        iter_lim = json.get<int>("iter_lim", 1000);
        res_print = json.get<int>("res_print", 10);
        tolerance = json.get<double>("tolerance", 1e-3);
        max_split = json.get<int>("max_split", 0);
        precond_lambda = json.get<double>("precond_lambda", -1);
        krylov_method = algorithms::CG_TAG;
        std::string krylov_method_s = json.get<std::string>("krylov_method", "cg");
        if (krylov_method_s == "cg")
            krylov_method = algorithms::CG_TAG;
        if (krylov_method_s == "fcg")
            krylov_method = algorithms::FCG_TAG;
    }

};

} }  // skylark::ml

#endif
