#ifndef SKYLARK_DENSE_TRANSFORM_ELEMENTAL_MC_MR_LOCAL_HPP
#define SKYLARK_DENSE_TRANSFORM_ELEMENTAL_MC_MR_LOCAL_HPP

#include "../base/base.hpp"

#include "transforms.hpp"
#include "dense_transform_data.hpp"
#include "../utility/comm.hpp"
#include "../utility/get_communicator.hpp"

#include "sketch_params.hpp"
#include "dense_transform_Elemental_mc_mr.hpp"

namespace skylark { namespace sketch {
/**
 * Specialization distributed input [MC, MR], local output
 */
template <typename ValueType,
          template <typename> class ValueDistribution>
struct dense_transform_t <
    elem::DistMatrix<ValueType>,
    elem::Matrix<ValueType>,
    ValueDistribution> :
        public dense_transform_data_t<ValueType,
                                      ValueDistribution> {

    // Typedef matrix and distribution types so that we can use them regularly
    typedef ValueType value_type;
    typedef elem::DistMatrix<value_type> matrix_type;
    typedef elem::Matrix<value_type> output_matrix_type;
    typedef ValueDistribution<value_type> value_distribution_type;
    typedef dense_transform_data_t<ValueType,
                                   ValueDistribution> data_type;

    /**
     * Regular constructor
     */
    dense_transform_t (int N, int S, base::context_t& context)
        : data_type (N, S, context) {

    }

    /**
     * Copy constructor
     */
    dense_transform_t (dense_transform_t<matrix_type,
                                         output_matrix_type,
                                         ValueDistribution>& other)
        : data_type(other) {}

    /**
     * Constructor from data
     */
    dense_transform_t(const dense_transform_data_t<value_type,
                                            ValueDistribution>& other_data)
        : data_type(other_data) {}

    /**
     * Apply the sketching transform that is described in by the sketch_of_A.
     */
    template <typename Dimension>
    void apply (const matrix_type& A,
                output_matrix_type& sketch_of_A,
                Dimension dimension) const {
        try {
            apply_impl_dist(A, sketch_of_A, dimension);
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                base::elemental_exception()
                    << base::error_msg(e.what()) );
        } catch(boost::mpi::exception e) {
                SKYLARK_THROW_EXCEPTION (
                    base::mpi_exception()
                        << base::error_msg(e.what()) );
        }
    }

private:

    /**
     * High-performance implementations
     */

    void apply_impl_dist(const matrix_type& A,
                         output_matrix_type& sketch_of_A,
                         skylark::sketch::rowwise_tag tag) const {

        typedef elem::DistMatrix<value_type, elem::CIRC, elem::CIRC>
            intermediate_matrix_type;

        matrix_type sketch_of_A_MC_MR(A.Height(),
                                  data_type::_S);
        intermediate_matrix_type sketch_of_A_CIRC_CIRC(A.Height(),
                                  data_type::_S);

        dense_transform_t<matrix_type, matrix_type, ValueDistribution>
            transform(*this);

        transform.apply(A, sketch_of_A_MC_MR, tag);

        sketch_of_A_CIRC_CIRC = sketch_of_A_MC_MR;

        boost::mpi::communicator world;
        MPI_Comm mpi_world(world);
        elem::Grid grid(mpi_world);
        int rank = world.rank();
        if (rank == 0) {
            sketch_of_A = sketch_of_A_CIRC_CIRC.Matrix();
        }
    }


    void apply_impl_dist(const matrix_type& A,
                         output_matrix_type& sketch_of_A,
                         skylark::sketch::columnwise_tag tag) const {

        typedef elem::DistMatrix<value_type, elem::CIRC, elem::CIRC>
            intermediate_matrix_type;

        matrix_type sketch_of_A_MC_MR(data_type::_S,
                                      A.Width());
        intermediate_matrix_type sketch_of_A_CIRC_CIRC(data_type::_S,
                                      A.Width());

        dense_transform_t<matrix_type, matrix_type, ValueDistribution>
            transform(*this);

        transform.apply(A, sketch_of_A_MC_MR, tag);

        sketch_of_A_CIRC_CIRC = sketch_of_A_MC_MR;

        boost::mpi::communicator world;
        MPI_Comm mpi_world(world);
        elem::Grid grid(mpi_world);
        int rank = world.rank();
        if (rank == 0) {
            sketch_of_A = sketch_of_A_CIRC_CIRC.Matrix();
        }
    }

};

} } /** namespace skylark::sketch */

#endif // SKYLARK_DENSE_TRANSFORM_ELEMENTAL_MC_MR_LOCAL_HPP
