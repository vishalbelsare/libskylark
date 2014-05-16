#ifndef SKYLARK_HILBERT_RUN_HPP
#define SKYLARK_HILBERT_RUN_HPP

#include "BlockADMM.hpp"
#include "options.hpp"
#include "io.hpp"
#include "../base/context.hpp"

template <class InputType>
BlockADMMSolver<InputType>* GetSolver(skylark::base::context_t& context,
    const hilbert_options_t& options, int dimensions) {

    lossfunction *loss = NULL;
    switch(options.lossfunction) {
    case SQUARED:
        loss = new squaredloss();
        break;
    case HINGE:
        loss = new hingeloss();
        break;
    case LOGISTIC:
        loss = new logisticloss();
        break;
    case LAD:
    	loss = new ladloss();
    	break;
    default:
        // TODO
        break;
    }

    regularization *regularizer = NULL;
    switch(options.regularizer) {
    case L2:
        regularizer = new l2();
        break;
    case L1:
    default:
        // TODO
        break;
    }

    BlockADMMSolver<InputType> *Solver = NULL;
    int features = 0;
    switch(options.kernel) {
    case LINEAR:
        features = dimensions;
        Solver =
            new BlockADMMSolver<InputType>(loss,
                regularizer,
                options.lambda,
                dimensions,
                options.numfeaturepartitions);
        break;

    case GAUSSIAN:
        features = options.randomfeatures;
        if (options.regularmap)
            Solver =
                new BlockADMMSolver<InputType>(context,
                    loss,
                    regularizer,
                    options.lambda,
                    features,
                    skylark::ml::kernels::gaussian_t(dimensions,
                        options.kernelparam),
                    skylark::ml::regular_feature_transform_tag(),
                    options.numfeaturepartitions);
        else
            Solver =
                new BlockADMMSolver<InputType>(context,
                    loss,
                    regularizer,
                    options.lambda,
                    features,
                    skylark::ml::kernels::gaussian_t(dimensions,
                        options.kernelparam),
                    skylark::ml::fast_feature_transform_tag(),
                    options.numfeaturepartitions);
        break;

    case POLYNOMIAL:
        features = options.randomfeatures;
        Solver = 
            new BlockADMMSolver<InputType>(context,
                loss,
                regularizer,
                options.lambda,
                features,
                skylark::ml::kernels::polynomial_t(dimensions,
                    options.kernelparam, options.kernelparam2, options.kernelparam3),
                skylark::ml::regular_feature_transform_tag(),
                options.numfeaturepartitions);
        break;

    case LAPLACIAN:
        features = options.randomfeatures;
        Solver = 
            new BlockADMMSolver<InputType>(context,
                loss,
                regularizer,
                options.lambda,
                features,
                skylark::ml::kernels::laplacian_t(dimensions, options.kernelparam),
                skylark::ml::regular_feature_transform_tag(),
                options.numfeaturepartitions);
        break;

    case EXPSEMIGROUP:
        features = options.randomfeatures;
        Solver =
            new BlockADMMSolver<InputType>(context,
                loss,
                regularizer,
                options.lambda,
                features,
                skylark::ml::kernels::expsemigroup_t(dimensions, options.kernelparam),
                skylark::ml::regular_feature_transform_tag(),
                options.numfeaturepartitions);
        break;

    default:
        // TODO!
        break;

    }

    // Set parameters
    Solver->set_rho(options.rho);
    Solver->set_maxiter(options.MAXITER);
    Solver->set_tol(options.tolerance);
    Solver->set_nthreads(options.numthreads);
    Solver->set_cache_transform(options.cachetransforms);

    return Solver;
}

/* TODO move to utility */
int max(DistTargetMatrixType& Y) {
    int k =  (int) *std::max_element(Y.Buffer(), Y.Buffer() + Y.LocalHeight());
    return k;
}
int max(elem::Matrix<double> Y) {
    int k =  (int) *std::max_element(Y.Buffer(), Y.Buffer() + Y.Height());
    return k;
}

template<class LabelType>
int GetNumClasses(const boost::mpi::communicator &comm, LabelType& Y) {
    int k = 0;
    int kmax = max(Y);
    boost::mpi::all_reduce(comm, kmax, k, boost::mpi::maximum<int>());

    // we assume 0-to-N encoding of classes. Hence N = k+1.
    // For two classes, k=1.
    if (k>1)
        return k+1;
    else
        return 1;
}



template <class InputType, class LabelType>
int run(const boost::mpi::communicator& comm, skylark::base::context_t& context,
    hilbert_options_t& options) {

    int rank = comm.rank();

    InputType X, Xv, Xt;
    LabelType Y, Yv, Yt;

    read(comm, options.fileformat, options.trainfile, X, Y);

    int dimensions = skylark::base::Height(X);
    int classes = GetNumClasses<LabelType>(comm, Y);

    bool shift = false;

    // we treat binary classification with multinomial logistic loss as a two-output problem
    double y;
    if ((options.lossfunction == LOGISTIC) && (classes == 1)) {

    	for(int i=0;i<Y.Height(); i++) {
    			y = Y.Get(i, 0);
    			Y.Set(i, 0, 0.5*(y+1.0));
    		}
    	classes = 2;
    	shift = true;
    }

    BlockADMMSolver<InputType>* Solver =
        GetSolver<InputType>(context, options, dimensions);

    if(!options.valfile.empty()) {
        comm.barrier();
        if(rank == 0) std::cout << "Loading validation data." << std::endl;
        read(comm, options.fileformat, options.valfile, Xv, Yv,
            skylark::base::Height(X));

        if ((options.lossfunction == LOGISTIC) && shift) {
            	for(int i=0;i<Yv.Height(); i++) {
            			y = Yv.Get(i, 0);
            			Yv.Set(i, 0, 0.5*(y+1.0));
            		}
            }
    }

    elem::Matrix<double> Wbar(Solver->get_numfeatures(), classes);
    elem::MakeZeros(Wbar);

    Solver->train(X, Y, Wbar, Xv, Yv, comm);

    SaveModel(options, Wbar);

    if(!options.testfile.empty()) {
        comm.barrier();
        if(rank == 0) std::cout << "Starting testing phase." << std::endl;
        read(comm, options.fileformat, options.testfile, Xt, Yt,
            skylark::base::Height(X));
        	if ((options.lossfunction == LOGISTIC) && shift) {
                    	for(int i=0;i<Yt.Height(); i++) {
                    			y = Yt.Get(i, 0);
                    			Yt.Set(i, 0, 0.5*(y+1.0));
                    		}
                    }

        LabelType Yp(Yt.Height(), classes);
        Solver->predict(Xt, Yp, Wbar);
        double accuracy = Solver->evaluate(Yt, Yp, comm);
        if(rank == 0)
            std::cout << "Test Accuracy = " <<  accuracy << " %" << std::endl;
    }

    delete Solver;

    return 0;
}



#endif /* SKYLARK_HILBERT_RUN_HPP */
